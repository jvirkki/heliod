/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 
 *
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * UNIX watchdog process
 *
 * The UNIX watchdog knows how to do the following
 *       - start the server
 *       - stop the server
 *       - restart the server (just a stop followed by start)
 *       
 *       - listen for messages from servers
 *       - create and destroy listen sockets
 *       - detach the process from the process group
 *       - log its pid in the pidfile
 *
 *       - detect a server crash and restart the server
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef AIX
#include <strings.h>
#include <sys/select.h>
#include <sys/types.h>
#endif
#include <grp.h>

#include "nspr.h"
#include "definesEnterprise.h"
#include "base/unix_utils.h"           // maxfd_set
#include "wdconf.h"
#include "wdsignals.h"
#include "wdlog.h"
#include "wdlsmgr.h"

#ifdef FEAT_SMF
#include "wdsmf.h"
#endif // FEAT_SMF

int _watchdog_death;
int _watchdog_killed_server;
int _watchdog_server_init_done;
int _watchdog_server_death;
int _watchdog_server_rotate;
int _watchdog_server_restart;
int _watchdog_server_start_error = 1;
int _watchdog_stop_waiting_for_messages         = 0;
int _watchdog_admin_is_waiting_for_reply        = 0;
int _watchdog_admin_waiting_for_reconfig_status = 0;
int _watchdog_created_tempdir = 0;
int _watchdog_server_pid = -1;
int _watchdog_parent_pid = -1;
int _watchdog_detach = 1;
int _watchdog_use_stderr = 1;
const char *_watchdog_server_user;
const char *_watchdog_tempdir;
char _watchdog_pidfile[PATH_MAX];
int _watchdog_created_pidfile;
char _watchdog_socket[PATH_MAX];
int _watchdog_created_socket;

int n_reconfig  = 0;
int n_reconfigDone  = 0;

#ifdef FEAT_SMF
const char *_smf_start_cmd;
wdSMF *wdsmf = NULL;
#endif // FEAT_SMF

void watchdog_exit(int status);

wdLSmanager     LS;

char errmsgstr[1024];

//
// The unix domain socket connection (to the primordial process) that 
// is used for sending Admin commands (e.g reconfigure) initiated by 
// the watchdog. Using the channel avoids having to use signals between
// the processes and also allows for returning status .
//
wdServerMessage* adminChannel = NULL;

void
watchdog_info(const char *msgstring)
{
    if (_watchdog_use_stderr) {
        fprintf(stderr, "info: %s\n", msgstring);
    }
    watchdog_log(LOG_INFO, "%s", msgstring);
}

void
watchdog_error(const char *msgstring)
{
    watchdog_log(LOG_ERR, "%s", msgstring);
    if (_watchdog_use_stderr) {
        fprintf(stderr, "failure: %s\n", msgstring);
    }
}

void
watchdog_errno(const char *msgstring)
{
    int err = errno;
    watchdog_log(LOG_ERR, "%s (%m)", msgstring);
    if (_watchdog_use_stderr) {
        fprintf(stderr, "failure: %s (%s)\n", msgstring, strerror(err));
    }
}

void
watchdog_check_status(int server_stat)
{
    /* See if the server exited */
    if (WIFEXITED(server_stat)) {
        int exit_code = WEXITSTATUS(server_stat);
        /*
         * If the server exited with non-zero status, terminate the
         * watchdog.
         */
        if (exit_code) {
            sprintf(errmsgstr, "server exit: status %d", exit_code);
            watchdog_error(errmsgstr);

            /* Maybe someone may need this option */
            if (getenv("UXWDOG_RESTART_ON_EXIT") || exit_code==RESTART_ON_EXIT_CODE) {
                /* Return to restart the server */
                return;
            }
            watchdog_exit(exit_code);
        }
    }

    /* See if the server died for reasons other than a restart */
    if (WIFSIGNALED(server_stat)) {
        int exit_sig = WTERMSIG(server_stat);

        /*
         * If the signal is not SIGTERM or the server is not being
         * restarted, report the signal, and if UXWDOG_NO_AUTOSTART
         * is set, terminate the watchdog.
         */

        if (!_watchdog_server_restart ||
            ( ( exit_sig != SIGTERM ) 
#ifdef IRIX
              && ( exit_sig != SIGKILL )
#endif
                )) {
            char *no_autostart = getenv("UXWDOG_NO_AUTOSTART");
            if (no_autostart) {
                sprintf(errmsgstr,
                        "server terminated (signal %d): watchdog exiting",
                        exit_sig);
                watchdog_error(errmsgstr);
                watchdog_exit(1);
            }
            sprintf(errmsgstr, "server terminated (signal %d)", exit_sig);
            if (_watchdog_server_init_done) {
                strcat(errmsgstr, ": watchdog is restarting it");
            }
            watchdog_error(errmsgstr);
            putenv("WD_RESTARTED=1");
        }
    }
}

void
watchdog_kill_server(void)
{
    if (_watchdog_killed_server)
        return;

    _watchdog_killed_server = 1;

    if (adminChannel != NULL) {
        // Send a message to the primordial
        if (adminChannel->SendToServer(wdmsgTerminate, NULL) == 0) {
            if (!_watchdog_death)
                watchdog_errno("error communicating with server");
        }
    } else {
        // Kill the primordial
        kill(_watchdog_server_pid, SIGTERM);
    }
}

void
watchdog_rotate_server_logs(void)
{
    if (adminChannel != NULL) {
        // Send a message to the primordial
        if (adminChannel->SendToServer(wdmsgRotate, NULL) == 0) {
            watchdog_errno("error communicating with server");
        }
    }
}

int
watchdog_running(void)
{
    struct stat finfo;
    int rv = 0;

    // First check if pidfile already exists:
    if(!stat(_watchdog_pidfile, &finfo)) {
        FILE *p = fopen(_watchdog_pidfile, "r");
        if(p) {
            // Is there already a pid in the pidfile?
            int z;
            if ((fscanf(p, "%d\n", &z)) != -1) {
                // Is the pid valid?
                pid_t foundpid = (pid_t) z;
                if(kill(foundpid, 0) != -1) {
                    // Does the socket exist, too?
                    if (!access(_watchdog_socket, F_OK)) {
                        // Watchdog is already running
                        rv = 1;
                    }
                }
            }
            fclose(p);
        }
    }

    return rv;
}

int
watchdog_logpid(void) 
{
    FILE *pidfile;
    int pid = getpid();
    char buff[24];
    int bytesWritten = -1;

    sprintf(buff, "%d\n", pid);
    pidfile = fopen(_watchdog_pidfile, "w");
    if (pidfile == NULL) {
        return -1;
    }

    _watchdog_created_pidfile = 1;

    setbuf(pidfile, NULL); 
    bytesWritten = fprintf(pidfile, "%s", buff);
    fclose(pidfile);
    if (bytesWritten != strlen(buff)) {
        return -1;
    }

    return 0;
}

void
watchdog_exit(int status)
{
    /* If a parent watchdog is waiting for us... */
    if (_watchdog_parent_pid != -1) {
        if (status) {
            kill(_watchdog_parent_pid, SIGUSR2); // error
        } else {
            kill(_watchdog_parent_pid, SIGUSR1); // success
        }
    }

    if (_watchdog_admin_is_waiting_for_reply) {
        int i = _watchdog_admin_is_waiting_for_reply;
        _watchdog_admin_is_waiting_for_reply = 0;
        /* Send error reply if admin fd is still there */
        assert(LS._heard_restart[i] == i);
        assert(LS.msg_table[i].wdSM != NULL);
        char msgstring[100];
        sprintf(msgstring,"%d",status);
        if (LS.msg_table[i].wdSM->SendToServer( wdmsgRestartreply, msgstring) ==0) {
                fprintf(stderr, "Restartreply failed\n");
        }
    }

    if (_watchdog_server_pid != -1) {
        /* Take the server down with us */
        watchdog_kill_server();
        if (_watchdog_created_socket)
            unlink(_watchdog_socket);
        if (_watchdog_created_pidfile)
            unlink(_watchdog_pidfile);
    }

    watchdog_closelog();

    if (_watchdog_created_tempdir) {
        PR_RmDir(_watchdog_tempdir);
    }

#ifdef FEAT_SMF
    if (wdsmf)
        delete wdsmf;
#endif // FEAT_SMF

    exit(status);
}

int
_watchdog_exec(int server_starts, const char *server_exe,
               char * argv[], char * envp[],
               int *spid)
{
    int rv = 0;
    int child;
    if (spid) *spid = -1;

    child = fork();
    if (child == 0) {
        static char envbuf[64];
        /* Indicate the server was started by the watchdog */
        putenv("WD_STARTED=1");

        /* Indicate whether server should close its stdout/stderr */
        if (_watchdog_detach)
            putenv("WD_DETACH=1");

        if (server_starts > 0) {
            int fd;

            fd = open("/dev/null", O_RDWR, 0);
            if (fd >= 0) {
                if (fd != 0) {
                    dup2(fd, 0);
                }
                if (fd != 1) {
                    dup2(fd, 1);
                }
                if (fd != 2){
                  dup2(fd, 2);
                }
                if (fd > 2) {
                    close(fd);
                }
            }
        }

        argv[0] = PRODUCT_DAEMON_BIN;

        rv = execv(server_exe, argv);
        if (rv < 0) {
            watchdog_errno("could not execute server binary");
            watchdog_exit(1);
        }
    }
    else if (child > 0) {
        if (spid) *spid = child;
    }
    else {
        rv = child;
        if (server_starts == 0) {
            watchdog_errno("could not fork server process");
        }
    }

    return rv;
}

int watchdog_pwd_prompt (const char *prompt, int serial, char **pwdvalue, bool smfwatchdog);
int watchdog_pwd_save   (char *pwdname, int serial, char *pwdvalue);
int watchdog_pwd_lookup (char *pwdname, int serial, char **pwdvalue);

void parse_LS_message_string(char * message, char ** ls_name, char ** ip,
                             int * port, int * family, int * ls_Qsize, 
                             int * SendBSize, int * RecvBSize)
{
        char * new_port;
        char * new_ip;
#if 0
        fprintf (stderr, " msg=%s\n", message);
#endif
        new_port = message+strlen(message);
        while (*new_port!=',') new_port--;
        *RecvBSize = atoi(new_port+1);
        *new_port=0;
        while (*new_port!=',') new_port--;
        *SendBSize = atoi(new_port+1);
        *new_port=0;
        while (*new_port!=',') new_port--;
        *ls_Qsize = atoi(new_port+1);
        *new_port=0;
        while (*new_port!=',') new_port--;
        *family = atoi(new_port+1);
        *new_port=0;
        while (*new_port!=',') new_port--;
        assert(new_port>message);
        *port = atoi(new_port+1);
        *new_port=0;
        new_ip = message+strlen(message);
        while (*new_ip!=',') new_ip--;
        *new_ip=0;
        if (*(new_ip+1)==0)     new_ip=NULL;    /* Empty string */
                else            new_ip=new_ip+1;
        *ls_name = message;
        *ip      = new_ip;
}

void parse_PWD_message_string(char *message, char **prompt, int *serial)
{
    char *p;

    p = message;

    // look for the first comma
    while (*p != ',' && *p != '\0')
        p++;
    if (*p == '\0') {
        // oops, not found
        *serial = 0;
        *prompt = NULL;
        return;
    }
    // null the comma out
    *p = '\0';
    // serial is the number in front of the comma
    *serial = atoi(message);
    // prompt is the string after the comma
    *prompt = p + 1;
}

int lastmessage = 0; 

void process_server_messages(int nmessages, int server_starts)
{
    int                 i, rv, count, newfd, efd, port,
                        family, lsQsize, sendBsize, recvBsize, pollTsize;
    char *              new_ip;
    char *              ls_name;
    char *              prompt;
    int                 serial;
    wdServerMessage *   wdSM;

    if (nmessages==0) return;
    count = 0;
    pollTsize = LS.get_table_size();
    for (i=0; i < pollTsize && count < nmessages; i++) {
        if (LS.msg_table[i]._waiting!=0) {      // Only look at these
            count++;
            if (LS.msg_table[i]._waiting==-1) {
                /* HUP seen on this socket */
                LS.msg_table[i]._waiting = 0;   // Clear it
                LS.pa_table[i].fd        = -1;  // no more on this socket
                if(LS.msg_table[i].wdSM!=NULL)  // might be null from EmptyRead
                    delete LS.msg_table[i].wdSM;
                LS.msg_table[i].wdSM = NULL;    // Clear it
                if (_watchdog_admin_is_waiting_for_reply == i)
                    _watchdog_admin_is_waiting_for_reply = 0;
                if (_watchdog_admin_waiting_for_reconfig_status == i)
                    _watchdog_admin_waiting_for_reconfig_status = 0;
            } else {
                LS.msg_table[i]._waiting = 0;   // Clear it
                wdSM = LS.msg_table[i].wdSM;
                if(wdSM==NULL) {
                        fprintf(stderr,
                                " NULL wdSM in watchdog:process_server_messages: index=%d, nmessages=%d, count=%d\n",
                                i, nmessages, count);
                        watchdog_exit(46);
                }
                char * msgstring = wdSM->RecvFromServer();
                lastmessage = wdSM->getLastMsgType();
                switch (lastmessage) {
                    case wdmsgGetLS:
                        /* Parse message string and create a Listen Socket */
                        parse_LS_message_string(msgstring, &ls_name, &new_ip,
                                                &port, &family, &lsQsize, 
                                                &sendBsize, &recvBsize);
                        newfd = LS.getNewLS(ls_name, new_ip, port, family,
                                            lsQsize, sendBsize, recvBsize);
                        if (newfd<0) {
                            if (_watchdog_server_init_done==0)
                                _watchdog_death = 1;    /* bad error - stop it all */
                        }
                        if (wdSM->SendToServer( wdmsgGetLSreply, (char *)&newfd)==0) {
                                watchdog_errno("error communicating with server");
                        }
                        break;
                    case wdmsgCloseLS:
                        /* Parse message string and close the Listen Socket */
                        parse_LS_message_string(msgstring, &ls_name, &new_ip,
                                                &port, &family, &lsQsize, 
                                                &sendBsize, &recvBsize);
                        newfd = LS.removeLS(ls_name, new_ip, port, family,
                                            lsQsize, sendBsize, recvBsize);
                        if (wdSM->SendToServer( wdmsgCloseLSreply, NULL) ==0) {
                                watchdog_errno("error communicating with server");
                        }
                        break;
                    case wdmsgEmptyRead:
                        // Treat this as if an end of file happened on the socket
                        // so it is like the HUP case
                        LS.pa_table[i].fd        = -1;  // no more on this socket
                        assert(LS.msg_table[i].wdSM!=NULL);
                        if (adminChannel == LS.msg_table[i].wdSM)
                            adminChannel = NULL;
                        delete LS.msg_table[i].wdSM;
                        LS.msg_table[i].wdSM = NULL;    // Clear it
                        if (_watchdog_admin_is_waiting_for_reply == i)
                            _watchdog_admin_is_waiting_for_reply = 0;
                        if (_watchdog_admin_waiting_for_reconfig_status == i)
                            _watchdog_admin_waiting_for_reconfig_status = 0;
                        break;
                    case wdmsgGetPWD:
                        char * pwd_result;
                        parse_PWD_message_string(msgstring, &prompt, &serial);
                        rv = watchdog_pwd_lookup(prompt, serial, &pwd_result);
                        if (rv == 0) {  /* did not find it */
                            if ((server_starts==0) &&
                                (_watchdog_server_init_done==0)) {
#ifdef FEAT_SMF
                                if (wdsmf && wdsmf->isSmfMode()) { // running in smf mode
                                    // then smf watchdog should fetch  the password and
                                    // pass it to this watchdog
                                    rv = wdsmf->getPassword(prompt, serial, &pwd_result);
                                } else {
#endif // FEAT_SMF
                                    rv = watchdog_pwd_prompt(prompt, serial, &pwd_result, false);
#ifdef FEAT_SMF
                                }
#endif // FEAT_SMF
                                if (rv<0) {     /* errors */
                                    char * errstr;
                                    switch (rv) {
                                        case -1:
                                        errstr = "end-of-file while reading password";
                                        break;
                                        case -2:
                                        errstr = "invalid password";
                                        break;
                                        default:
                                        errstr = "error while reading password";
                                        break;
                                    }
                                    watchdog_error(errstr);
                                    // _watchdog_death = 1; ???
                                }

                                rv = watchdog_pwd_save(prompt, serial, pwd_result);
                                // check error code??
                            }   // otherwise can fall through without prompting
                        }
//                      fprintf(stderr, " GETpwd in wd: rv=%d, pwd_result=%s\n",
//                              rv, pwd_result);
                        if (pwd_result==NULL)   pwd_result="send-non-empty-message";
                        else if (strlen(pwd_result)==0)
                                                pwd_result="send-non-empty-message";
                        if (wdSM->SendToServer( wdmsgGetPWDreply, pwd_result) ==0) {
                                watchdog_errno("error communicating with server");
                        }
                        break;
                    case wdmsgEndInit:
                        rv = watchdog_logpid();
                        if (rv) {
                            sprintf(errmsgstr,
                                    "could not log PID to %s",
                                    _watchdog_pidfile);
                            watchdog_errno(errmsgstr);
                            watchdog_exit(1);
                        }
                        _watchdog_server_init_done = 1;
                        n_reconfig = atoi(msgstring);   // Maxprocs sent back
                        if (wdSM->SendToServer( wdmsgEndInitreply, NULL) ==0) {
                                watchdog_errno("error communicating with server");
                        }
                        // Only the primordial process send the EndInit message
                        // to the watchdog. Therefore, this channel is deemed
                        // to be the channel used by the watchdog to Administer
                        // the server process(es)
                        adminChannel = wdSM;
                        break;
                    case wdmsgTerminate:
                        /* message indicates server has finished and will terminate */
                        if (adminChannel != NULL) {
                            // acknowledge the terminate command
                            wdSM->SendToServer( wdmsgTerminatereply, NULL);
                            if (adminChannel->SendToServer(wdmsgTerminate, NULL) == 0) {
                                if (!_watchdog_death)
                                    watchdog_errno("error communicating with server");
                            }
                            _watchdog_death = 1;
                        }
                        break;
                    case wdmsgRestart:
                        /* message is the status file */
                        /*
                         * The Admin Server CGI needs to be able to delete
                         * this file when it's done with it.
                         */
                        efd = open(msgstring, (O_CREAT|O_TRUNC|O_WRONLY), 0666);
                        if ((efd >= 0) && (efd != 2)) {
                                /* Replace stderr with this file */
                                dup2(efd, 2);
                                close(efd);
                                _watchdog_use_stderr = 1;
                        }
                        LS._heard_restart[i] = i; /* mark fd => came from Admin */
                        _watchdog_admin_is_waiting_for_reply = i;
                        _watchdog_server_restart = 1;
                        /* Reply is delayed until restart finishes */
                        break;
                    case wdmsgReconfigure:
                        _watchdog_admin_waiting_for_reconfig_status = i;
                        // Set the number of done messages to expect
                        n_reconfigDone = n_reconfig;
                        if (adminChannel != NULL)
                        {
                            if (adminChannel->SendToServer(wdmsgReconfigure, NULL) == 0)
                            {
                                watchdog_errno("error communicating with server");
                            }

                            // acknowledge the reconfigure command (from the admin)
                            if (wdSM->SendToServer( wdmsgReconfigurereply, NULL) ==0) {
                                watchdog_errno("error communicating with server");
                            }
                        }
                        break;
                    case wdmsgGetReconfigStatus:
                        _watchdog_admin_waiting_for_reconfig_status = i;
                        break;
                    case wdmsgReconfigStatus:
                        // Send message to admin that did last GetReconfigStatus
                        if (_watchdog_admin_waiting_for_reconfig_status>0) {
                            wdServerMessage *   wdSM;
                            wdSM = LS.msg_table[_watchdog_admin_waiting_for_reconfig_status]
                                        .wdSM;
                            assert(msgstring!=NULL);
                            if (wdSM->SendToServer( wdmsgGetReconfigStatusreply,
                                                msgstring ) ==0) {
                                watchdog_errno("error communicating with server");
                            }
                        } else {
                                // Error, or Admin no longer listening??
                        }
                        // and reply to server
                        if (wdSM->SendToServer( wdmsgReconfigStatusreply, NULL ) ==0) {
                                watchdog_errno("error communicating with server");
                        }
                        break;
                    case wdmsgReconfigStatusDone:
                        // Send admin done indication
                        n_reconfigDone--;
                        if (n_reconfigDone ==0) {
                            if (_watchdog_admin_waiting_for_reconfig_status>0) {
                                wdServerMessage *       wdSM;
                                wdSM = LS.msg_table[_watchdog_admin_waiting_for_reconfig_status]
                                        .wdSM;
                                // Send a null status message to indicate done
                                if (wdSM->SendToServer( wdmsgGetReconfigStatusreply, NULL) ==0) {
                                        watchdog_errno("error communicating with server");
                                }
                            } else {
                                // error, or admin no longer listening?
                            }
                            _watchdog_admin_waiting_for_reconfig_status = 0;
                        } else  {
                            // Ignore this potential error
                            //  assert(n_reconfigDone>0);
                        }
                        if (wdSM->SendToServer( wdmsgReconfigStatusDonereply, NULL ) ==0) {
                                watchdog_errno("error communicating with server");
                        }
                        break;
                    default:
                        fprintf(stderr,
                                "Unknown message in process_server_messages: %d\n",
                                wdSM->getLastMsgType());
                }
            }
        }
        if (count==nmessages) break;
    }   // end of for
    assert(count==nmessages);
}

void wait_for_message(int server_starts)
{
    int nmsgs = LS.Wait_for_Message();
    if (nmsgs == 0) return;
    else if (nmsgs > 0) {
        process_server_messages(nmsgs,server_starts);
    } else {
        if (nmsgs==-1) {
            if (errno!=EINTR) {
                watchdog_errno("error waiting for messages");
            }
        } else {
            sprintf(errmsgstr, "Poll failed: nmsgs=%d, errno=%d",
                    nmsgs, errno);
            watchdog_errno(errmsgstr);
        }

    }
}

int main(int argc, char **argv, char **envp)
{
    int rv;
    int c;
    int smfEnv = 0;
    int transient_child = 0;
    char server_exe[PATH_MAX];
    char *server_config_dir = NULL;
    char *server_install_dir = NULL;
    char *server_user = NULL;
    struct stat statbuf;

    /*
     * Initialize logging through the syslog API
     */

    watchdog_openlog();

    maxfd_set(maxfd_getmax());

    while ((c = getopt(argc, argv, "d:r:t:u:s:cvi")) != -1) {
        switch(c) {
          case 'd':
            server_config_dir = strdup(optarg);
            break;
          case 'r':
            server_install_dir = strdup(optarg);
            break;
          case 't':
            _watchdog_tempdir = strdup(optarg);
            break;
          case 'u':
            if (*optarg)
                server_user = strdup(optarg);
            break;
#ifdef FEAT_SMF
          case 's':
            _smf_start_cmd = strdup(optarg);
            break;
#endif // FEAT_SMF
          case 'c':
            transient_child = 1;
            break;
          case 'v':
            transient_child = 1;
            break;
          case 'i':
            _watchdog_detach = 0;
            break;
        }
    }

    if (!server_config_dir || !server_install_dir || !_watchdog_tempdir) {
#ifdef FEAT_SMF
        fprintf(stderr, "Usage1: "PRODUCT_WATCHDOG_BIN" -d configdir -r installdir -t tempdir [-u user] [-s smfservicecmd] [-c] [-v] [-i]\n");
#else
        fprintf(stderr, "Usage1: "PRODUCT_WATCHDOG_BIN" -d configdir -r installdir -t tempdir [-u user] [-c] [-v] [-i]\n");
#endif // FEAT_SMF
        watchdog_exit(1);
    }

    /* Construct name of server executable */
    snprintf(server_exe,
             sizeof(server_exe),
             "%s/"PRODUCT_PRIVATE_BIN_SUBDIR"/"PRODUCT_DAEMON_BIN,
             server_install_dir);

    rv = stat(server_exe, &statbuf);
    if (rv < 0) {
        sprintf(errmsgstr, "could not find %s", server_exe);
        watchdog_errno(errmsgstr);
        watchdog_exit(1);
    }

    /* Change to config directory */
    rv = chdir(server_config_dir);
    if (rv < 0) {
        sprintf(errmsgstr, "could not change directory to %s", server_config_dir);
        watchdog_errno(errmsgstr);
        watchdog_exit(1);
    }

    /* Construct pid file path */
    snprintf(_watchdog_pidfile,
             sizeof(_watchdog_pidfile),
             "%s/pid",
             _watchdog_tempdir);

    /* Construct socket path */
    snprintf(_watchdog_socket,
             sizeof(_watchdog_socket),
             "%s/"WDSOCKETNAME,
             _watchdog_tempdir);

    /* Check for running server */
    if (!transient_child) {
        if (watchdog_running()) {
            watchdog_info("server already running");
            watchdog_exit(0);
        }
    }

    /* Create tempdir if it doesn't exist */
    if (PR_MkDir(_watchdog_tempdir, 0700) == PR_SUCCESS) {
        if (server_user) {
            struct passwd *pw;
            pw = getpwnam(server_user);
            if (pw && geteuid() == 0) {
                chown(_watchdog_tempdir, pw->pw_uid, pw->pw_gid);
            }
        }
    }

    if (access(_watchdog_tempdir, W_OK)) {
        int len = sprintf(errmsgstr, "temporary directory %s is not writable",
                          _watchdog_tempdir);

        struct passwd *pw = getpwuid(geteuid());
        if (pw && pw->pw_name) {
            sprintf(errmsgstr + len, " by user %s", pw->pw_name);
        }

        watchdog_error(errmsgstr);
        watchdog_exit(1);
    }

#ifdef FEAT_SMF
    wdsmf = new wdSMF(server_config_dir, _watchdog_tempdir, _smf_start_cmd);

    if (wdsmf && wdsmf->isSmfMode()) {
        smfEnv = 1;
        if (!wdsmf->calledBySmf()) {
            wdsmf->setWatchdogType(wdsmf->WD_SMF);
            /* Create FIFO */
            int rv = wdsmf->createSmfWatchdogChannel();
            if (rv != 0) {
                watchdog_errno("error creating smf channel");
                watchdog_exit(1);
            }

            int p = fork();
            if(p > 0) { // smf watchdog parent
                int rv = wdsmf->captureData(p);
                switch(rv) {
                    case 1:
                        watchdog_errno("error opening smf output channel");
                        break;
                    case 2:
                        watchdog_errno("error polling channel");
                        break;
                    case 3:
                        // other end of the channel is closed.
                        // end of reading
                        break;
                    case 4:
                        // child terminated
                        break;
                    case 5:
                        watchdog_error("SMF channel timed out");
                        break;
                }

                watchdog_exit(0);

            } else if (p == 0) { // smf watchdog child
                if(wdsmf->startSmfService() > 0) {
                    watchdog_errno("could not start the service");
                    watchdog_exit(1);
                }
            }

        } else {
            wdsmf->setWatchdogType(wdsmf->WD_PARENT);
            // create pipes to capture watchdog's stdin, stdout and stderr
            if (wdsmf->createWatchdogChannel() != 0) {
                watchdog_errno("could not create watchdog parent's channel");
                watchdog_exit(1);
            }
        }
    }
#endif // FEAT_SMF

    _watchdog_created_tempdir = 1;

    if (!transient_child) {
        /* Remove any stale watchdog socket so we can bind to the name later */
        unlink(_watchdog_socket);
    }

    if (_watchdog_detach) {
        parent_watchdog_create_signal_handlers();

        /*
         * Set the watchdog up as session leader, but don't close
         * stdin, stdout, and stderr until after the server completes
         * its initialization for the first time.
         */

        rv = fork();
        if (rv != 0) {
            if (rv > 0) {
                int exitStatus = 0;
#ifdef FEAT_SMF
                if (smfEnv) {
                    exitStatus = 3;
                    wdsmf->setupSmfEnvironment();
                    int rv = wdsmf->captureData();

                    switch(rv) {
                        case 1:
                            watchdog_errno("error opening smf output channel");
                            break;
                        case 2:
                            watchdog_errno("error polling channel");
                            break;
                        case 3:
                            // other end of the channel is closed
                            // end of reading
                            exitStatus = 0;
                            break;
                        case 4:
                            // child terminated
                            break;
                        case 5:
                            watchdog_error("SMF channel timed out");
                            break;
                        default:
                            //success
                            exitStatus = 0;
                    }

                    if (wdsmf) {
                        delete wdsmf;
                        wdsmf = NULL;
                    }
                }
#endif // FEAT_SMF

                /* Parent exits normally when child signals it */
                watchdog_wait_signal();
                if(_watchdog_server_start_error) {
#ifdef FEAT_SMF
                    if (smfEnv) {
                        exitStatus = 2;
                    } else 
#endif // FEAT_SMF
                    exitStatus = 1;
                }
                exit(exitStatus);
            }
            watchdog_errno("could not detach watchdog process");
            watchdog_exit(1);
        }

        /*
         * We're the child watchdog.  We'll report back to the parent watchdog
         * via SIGUSR1 (success, _watchdog_server_start_error clear) or SIGUSR2
         * (error, _watchdog_server_start_error set).
         */
        _watchdog_parent_pid = getppid();

#ifdef FEAT_SMF
        // if running in smf mode
        if (smfEnv) {
            // set the watchdog type
            wdsmf->setWatchdogType(wdsmf->WD_CHILD);
            wdsmf->setupSmfEnvironment();
        }
#endif // FEAT_SMF

        /* Child leads a new session */
        rv = setsid();
        if (rv < 0) {
            watchdog_errno("could not setsid() for watchdog process");
        }
    }

    if (!transient_child) {
        char *resultstr = LS.InitializeLSmanager(_watchdog_socket);
        if (resultstr) {
            sprintf(errmsgstr,
                    "error %d initializing listen socket manager (%s)",
                    errno, resultstr);
            watchdog_error(errmsgstr);
            watchdog_exit(1);
        }
        _watchdog_created_socket = 1;
    }

    for (int server_starts = 0;; ++server_starts) {
        int server_stat;

        _watchdog_death                                 = 0;
        _watchdog_killed_server                         = 0;
        _watchdog_server_init_done                      = 0;
        _watchdog_server_death                          = 0;
        _watchdog_server_rotate                         = 0;
        _watchdog_server_restart                        = 0;
        _watchdog_stop_waiting_for_messages             = 0;
        _watchdog_admin_waiting_for_reconfig_status     = 0;

        watchdog_create_signal_handlers();

        rv = _watchdog_exec(server_starts, server_exe, argv, envp,
                            &_watchdog_server_pid);
        if (_watchdog_server_pid < 0) {
            if (_watchdog_detach && (server_starts == 0)) {
                kill(getppid(), SIGUSR2);
            }
            break;
        }

        /* Initialization loop:                         */
        /* Keep receiving requests from server until    */
        /* it signals an error event or done with init  */
        /* (PidLog and Password requests must happen    */
        /* during this loop; some others are allowed    */
        /* but NOT restart)                             */
        while (!_watchdog_server_init_done) {

            if (_watchdog_death)
                watchdog_kill_server();

            if (_watchdog_server_death) {
                do {
                    rv = wait(&server_stat);
                } while ((rv < 0) && (errno == EINTR));

                if (transient_child) {
                    if (WIFEXITED(server_stat) && !WEXITSTATUS(server_stat)) {
                        // version/config test success
                        watchdog_exit(0);
                    } else {
                        // config test failure
                        watchdog_exit(1);
                    }
                } else {
                    if (_watchdog_detach && (server_starts == 0)) 
                        kill(getppid(), SIGUSR2);
                }

                if ((rv < 0) && (errno == ECHILD)) {
                    watchdog_error(
                       "wait() returned ECHILD during server initialization");
                } else {
                    watchdog_error("server initialization failed");
                }
                watchdog_exit(1);
            }

            /* Wait for a request from the server */
            wait_for_message(server_starts);
        }       /* while (!_watchdog_server_init_done) */

#ifdef FEAT_SMF
        // delete wdsmf
        if (wdsmf) {
            delete wdsmf;
            wdsmf = NULL;
        }
        smfEnv = 0;
#endif // FEAT_SMF

        if (_watchdog_detach) {
            if (server_starts == 0) {
                int fd = open("/dev/null", O_RDWR, 0);
                if (fd >= 0) {
                    if (fd != 0) {
                        dup2(fd, 0);
                    }
                    if (fd != 1) {
                        dup2(fd, 1);
                    }
                    /*
                     * Send stderr to /dev/null too.
                     */
                    if (fd != 2) {
                      dup2(fd, 2);
                    }
                    if (fd > 2) {
                        close(fd);
                    }
                }

                /* The parent watchdog can exit now */
                kill(getppid(), SIGUSR1);
            }
            else {
            /*
             * stderr may have been redirected to a temporary file.
             * If we're running detached, redirect it to /dev/null.
             */
                fflush(stderr);
                int fd = open("/dev/null", O_WRONLY, 0);
                if ((fd >= 0) && (fd != 2)) {
                    dup2(fd, 2);
                    close(fd);
                }
            }
            _watchdog_use_stderr = 0;  /* reset to 0 */
        }

        if (_watchdog_admin_is_waiting_for_reply) {
            int i = _watchdog_admin_is_waiting_for_reply;
            _watchdog_admin_is_waiting_for_reply = 0;
            assert(LS._heard_restart[i] == i);
            assert(LS.msg_table[i].wdSM != NULL);
                /* Send reply if admin fd is still there */
            if (LS.msg_table[i].wdSM->SendToServer( wdmsgRestartreply, NULL)==0) {
                fprintf(stderr, "Restartreply failed\n");
            }
        }

        /* Main Loop:                                   */
        /* Just wait for requests from the server until */
        /*      a SIGCHLD or other action is signalled  */
        while(!_watchdog_server_death) {
            if (_watchdog_server_rotate) {
                _watchdog_server_rotate = 0;
                watchdog_rotate_server_logs();
            }

            if (_watchdog_death | _watchdog_server_restart) {
                watchdog_kill_server();
                break;
            }

            wait_for_message(server_starts);
        }

        if (_watchdog_server_death && !_watchdog_killed_server) {
            // server died but watchdog did not terminate it
            if (n_reconfigDone>0) {
                // possibly in the middle of a reconfigure - shut down
                // all Listen Sockets since might be all wrong now.
                LS.unbind_all();
            }
        }

        /* Shutdown loop: ends when server terminates   */
        while (!_watchdog_server_death && !_watchdog_stop_waiting_for_messages) {
            wait_for_message(server_starts);
        }

        do {
            rv = wait(&server_stat);
        } while ((rv < 0) && (errno == EINTR));

        if ((rv < 0) && (errno == ECHILD)) {
            watchdog_error("wait() returned ECHILD unexpectedly");
            if (_watchdog_death) {
                watchdog_exit(1);
            }
        }

        if (_watchdog_death) {
            watchdog_exit(0);
        }

        watchdog_check_status(server_stat);

        watchdog_delete_signal_handlers();
    }   /* for (int server_starts = ...     */

    watchdog_exit(1);
}
