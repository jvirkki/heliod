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

#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "nspr.h"
#include "base/unix_utils.h"           // setFDNonInheritable
#include "wdsmf.h"
#include "wdutil.h"

#define WDSMF_PIPE_IN_NAME "wdsmf_pip_in"    // input named pipe
#define WDSMF_PIPE_OUT_NAME "wdsmf_pip_out"  // output named pipe
#define WDSMF_START_METHOD_S10 "startserv"       // SMF_METHOD value
#define WDSMF_START_METHOD_S11 "start"       // SMF_METHOD value
#define GET_PWD_STR "GET_PWD"

#define POLL_RETRIES 36                      // retry for ~3 min 
#define MAX_BUF_SIZE 5000
#define PATH_MAX 256
#define NUM_WD_PIPES 3                       // number of watchdog pipes
#define PIPE_READ_END 0
#define PIPE_WRITE_END 1
#define INPUT_PIPE 0
#define OUTPUT_PIPE 1
#define FLAG_PIPE 2

int watchdog_pwd_prompt (const char *prompt, int serial, char **pwdvalue, bool smfwatchdog);
int smf_get_pwd(char *prompt, char **pwdvalue);

wdSMF::wdSMF(const char *instanceConfigDir, const char *wsTempDir, const char *smfStartCmd)
: wdtype(WD_NONE),
  wd_smf_pipe_in_path(NULL),
  wd_smf_pipe_out_path(NULL),
  pwdFlag(0),
  service_started(0),
  wd_smf_pipe_in_create(0),
  wd_smf_pipe_out_create(0),
  smf_pa_table_size(PA_TABLE_SIZE),
  smf_pa_count(0),
  smfPipeInFd(-1),
  smfPipeOutFd(-1)
{
  readSmfHeader = PR_TRUE;

  if (instanceConfigDir)
      ws_inst_conf_dir = strdup(instanceConfigDir);
  else
      ws_inst_conf_dir = NULL;

  if (wsTempDir)
      ws_temp_dir = strdup(wsTempDir);
  else
      ws_temp_dir = NULL;

  if (smfStartCmd)
      smf_start_cmd = strdup(smfStartCmd);
  else
      smf_start_cmd = NULL;

  smf_pa_table = (struct pollfd *) malloc(smf_pa_table_size * sizeof(struct pollfd));

  // Initialize Poll table
  int i;
  for (i = 0; i<PA_TABLE_SIZE; i++) {
      smf_fd_read_ready_table[i] = 0;
      smf_pa_table[i].fd = -1;
      smf_pa_table[i].revents = 0;
      smf_pa_table[i].events = 0;
  }

  int j;
  for (i=0; i<NUM_WD_PIPES; i++) {
      for (j=0; j<2; j++)
          wdPipes[i][j] = -1;
  }
}

wdSMF::~wdSMF() {
    if (wdtype != WD_NONE) {
        if (wdtype != WD_CHILD)
            closeSmfWatchdogChannel();

        if (wdtype != WD_SMF) {
            // close the pipes
            closeWatchdogUsedChannel();
        }
    }

    if (smf_pa_table) {
        int i;
        for(i = 0; i < smf_pa_count; i++) {
            removePollEntry(i);
        }
        free(smf_pa_table);
    }

    if (ws_inst_conf_dir) {
        free(ws_inst_conf_dir);
        ws_inst_conf_dir = NULL;
	}

    if (ws_temp_dir) {
        free(ws_temp_dir);
        ws_temp_dir = NULL;
    }
    if (smf_start_cmd) {
        free(smf_start_cmd);
        smf_start_cmd = NULL;
    }
    if (wd_smf_pipe_out_path) {
        free(wd_smf_pipe_out_path);
        wd_smf_pipe_out_path = NULL;
    }
    if (wd_smf_pipe_in_path) {
        free(wd_smf_pipe_in_path);
        wd_smf_pipe_in_path = NULL;
    }
}

PRBool
wdSMF::isSmfMode() {
    PRBool smfMode = PR_FALSE;
    if (smf_start_cmd && (strlen(smf_start_cmd) > 0))
       smfMode = PR_TRUE;

    return smfMode;
}

PRBool
wdSMF::calledBySmf() {
    PRBool bySmf = PR_FALSE;
    char *smfMethod = getenv("SMF_METHOD");
    char *smfFmri = getenv("SMF_FMRI");
    char startMethod[PATH_MAX];

    if (smfFmri && *smfFmri && smfMethod && *smfMethod && strstr(smf_start_cmd, smfFmri) != NULL) {
        if (ws_inst_conf_dir && strstr(smfMethod, WDSMF_START_METHOD_S10) != NULL) {
            char *tempConfDir = strdup(ws_inst_conf_dir);
            if (tempConfDir) {
                char *c = strrchr(tempConfDir, '/');
                if (c) {
                    *c = '\0';
                    snprintf(startMethod, sizeof(startMethod), "\"%s/bin/%s\"", tempConfDir, WDSMF_START_METHOD_S10);
                    if (strcmp(smfMethod, startMethod) == 0)
                        bySmf = PR_TRUE;
                }
                free(tempConfDir);
            }  
        } else if (strcmp(smfMethod, WDSMF_START_METHOD_S11) == 0) {
            bySmf = PR_TRUE;
        }
    }

    return bySmf;
}

void
wdSMF::setWatchdogType(wdType type) {
    wdtype = type;
}

int
wdSMF::createSmfWatchdogChannel() {
    if (ws_temp_dir && *ws_temp_dir) {
        // input pipe
        if (!wd_smf_pipe_in_path) {
            wd_smf_pipe_in_path = (char *)malloc(PATH_MAX * sizeof(char));
            if (wd_smf_pipe_in_path) {
                snprintf(wd_smf_pipe_in_path, PATH_MAX, "%s/%s", ws_temp_dir, WDSMF_PIPE_IN_NAME);
            } else
                return 1;
        }

        // output pipe
        if (!wd_smf_pipe_out_path) {
            wd_smf_pipe_out_path = (char *)malloc(PATH_MAX * sizeof(char));
            if (wd_smf_pipe_out_path) {
                snprintf(wd_smf_pipe_out_path, PATH_MAX, "%s/%s", ws_temp_dir, WDSMF_PIPE_OUT_NAME);
            } else
                return 1;
        }

        int rv = -1;
        // when watchod child expects the password from the user,
        // watchdog parent should create this smf input named pipe
        if (wdtype == WD_PARENT && pwdFlag) {
            rv = mkfifo(wd_smf_pipe_in_path, 0600);
            if ((rv == -1) && (errno != EEXIST)) {
                return 1; // failure
            }

            wd_smf_pipe_in_create = 1;

        } else if (wdtype == WD_SMF) { // only smf watchdog should create this name pipe
            rv = mkfifo(wd_smf_pipe_out_path, 0600);
            if ((rv == -1) && (errno != EEXIST)) {
                return 1; // failure
            }
            wd_smf_pipe_out_create = 1;

            struct stat finfo;
            if (stat(wd_smf_pipe_in_path, &finfo) < 0)
                ; //ignore
            else {
                wd_smf_pipe_in_create = 1;
            }

        } else {
            struct stat finfo;
            if (stat(wd_smf_pipe_out_path, &finfo) < 0)
                return 1; //failure
            else {
                wd_smf_pipe_out_create = 1;
            }
        }
    }

    return 0; // success
}

int
wdSMF::startSmfService() {
    if (smf_start_cmd && calledBySmf()) {
        service_started = 1;
        return 1; // service already started
    } else {
        char **smfArgs = NULL;
        // Parse the command line
        smfArgs = util_argv_parse(smf_start_cmd);
        service_started = 1;

        // execute the smf service
        if (smfArgs) {
            execv(smfArgs[0], smfArgs);
            // if exec failed, then free smfArgs
            util_env_free(smfArgs);
        }
    }

    // if we are here, then exec failed !
    service_started = 0;
    return 2; // failure
}

int
wdSMF::openSmfInputChannel() {
    smfPipeInFd = -1;

    if (!wd_smf_pipe_in_create) {
        // create the named pipe
        int rv = createSmfWatchdogChannel();
        if (rv != 0)
            return rv; // error
    }

    if (wd_smf_pipe_in_create) {
        if (wdtype == WD_SMF) { //smf watchdog is calling this
            // open the input pipe for writing
            smfPipeInFd = open(wd_smf_pipe_in_path, O_WRONLY | O_NONBLOCK);
            if (smfPipeInFd >= 0) {
                pwdFlag = 1;
            }

        } else { // regular watchdog parent is calling this
            // open the input pipe for reading
            smfPipeInFd = open(wd_smf_pipe_in_path, O_RDONLY | O_NONBLOCK);
            if (smfPipeInFd >=0)
                addPollEntry(smfPipeInFd);
        }
    }

    return PR_SUCCESS;
}

int
wdSMF::openSmfOutputChannel() {
    smfPipeOutFd = -1;

    if (!wd_smf_pipe_out_create) {
        // create the named pipe
        int rv = createSmfWatchdogChannel();
        if (rv != 0)
            return rv; //error
    }

    if (wd_smf_pipe_out_create) {
        if (wdtype == WD_SMF) { //smf watchdog is calling this
            // open the output pipe for reading
            smfPipeOutFd = open(wd_smf_pipe_out_path, O_RDONLY | O_NONBLOCK);
            if (smfPipeOutFd >= 0) {
                addPollEntry(smfPipeOutFd);
                return 0; // success
            }

        } else { // regular watchdog parent is calling this
            // open the output pipe for writing
            smfPipeOutFd = open(wd_smf_pipe_out_path, O_WRONLY | O_NONBLOCK);
            if (smfPipeOutFd >= 0) {
                return 0; // success
            } else
                return 2; // try again.. smf watchdog may not be reading this pipe yet.
        }
    }

    return 1;
}

int
wdSMF::createWatchdogChannel() {
    int rv = 0;
    int i;
    for (i=0; i<NUM_WD_PIPES; i++) {
        // wdPipes[i][0] is the read end of the pipe
        // wdPipes[i][1] is the write end of the pipe
        if (pipe(wdPipes[i]) == -1)
            break;
    }
    if (i < 3)
        rv = 1;
    return rv; // success
}

void
wdSMF::setNonInheritable() {
    if (wdtype == WD_CHILD) {
        int i, j;
        for(i=0; i<NUM_WD_PIPES; i++) {
            for(j=0; j<2; j++)
                setFDNonInheritable(wdPipes[i][j]);
        }
    }
}

void
wdSMF::closeWatchdogUsedChannel(PRBool closeFlagPipe) {
    int i;
    for(i=0; i<NUM_WD_PIPES; i++) {
        switch(i) {
            case 0:
                if (wdtype == WD_PARENT) {
                    // close the write end of the input pipe
                    if (wdPipes[i][PIPE_WRITE_END] >= 0) {
                        close(wdPipes[i][PIPE_WRITE_END]);
                        wdPipes[i][PIPE_WRITE_END] = -1;
                    }
                } else if (wdtype == WD_CHILD) {
                    // close the read end of the input pipe
                    if (wdPipes[i][PIPE_READ_END] >= 0) {
                        close(wdPipes[i][PIPE_READ_END]);
                        wdPipes[i][PIPE_READ_END] = -1;
                    }
                }
                break;
            case 1:
            case 2:
                if (wdtype == WD_PARENT) {
                    // close the read end of the output/flag pipe
                    if (wdPipes[i][PIPE_READ_END] >= 0) {
                        close(wdPipes[i][PIPE_READ_END]);
                        wdPipes[i][PIPE_READ_END] = -1;
                    }
                } else if (wdtype == WD_CHILD) {
                    // close the write end of the output/flag pipe
                    if ((i != 2) || (i == 2 && closeFlagPipe)) {
                        if (wdPipes[i][PIPE_WRITE_END] >= 0) {
                            close(wdPipes[i][PIPE_WRITE_END]);
                            wdPipes[i][PIPE_WRITE_END] = -1;
						}
                    }
                }
                break;
        }
    }
}

void
wdSMF::closeWatchdogUnusedChannel () {
    int i;
    for(i=0; i<NUM_WD_PIPES; i++) {
        switch(i) {
            case 0:
                if (wdtype == WD_PARENT) {
                    // close the read end of the input pipe
                    if (wdPipes[i][PIPE_READ_END] >= 0) {
                        close(wdPipes[i][PIPE_READ_END]);
                        wdPipes[i][PIPE_READ_END] = -1;
                    }
                } else if (wdtype == WD_CHILD) {
                    // close the write end of the input pipe
                    if (wdPipes[i][PIPE_WRITE_END] >= 0) {
                        close(wdPipes[i][PIPE_WRITE_END]);
                        wdPipes[i][PIPE_WRITE_END] = -1;
                    }
                }
                break;
            case 1:
            case 2:
                if (wdtype == WD_PARENT) {
                    // close the write end of the output/flag pipe
                    if (wdPipes[i][PIPE_WRITE_END] >= 0) {
                        close(wdPipes[i][PIPE_WRITE_END]);
                        wdPipes[i][PIPE_WRITE_END] = -1;
                    }
                    if (wdPipes[i][PIPE_READ_END] >= 0)
                        addPollEntry(wdPipes[i][PIPE_READ_END]);

                } else if (wdtype == WD_CHILD) {
                    // close the read end of the output/flag pipe
                    if (wdPipes[i][PIPE_READ_END] >= 0) {
                        close(wdPipes[i][PIPE_READ_END]);
                        wdPipes[i][PIPE_READ_END] = -1;
                    }
                }
                break;
        }
    }
}

void
wdSMF::setupSmfEnvironment() {
    if (wdtype == WD_PARENT) {
        closeWatchdogUnusedChannel();

    } else if (wdtype == WD_CHILD) {
        setNonInheritable();
        closeWatchdogUnusedChannel();
        // dup the write end of the pipe to stdout and stderr
        // so that all the msgs are passed to the parent watchdog
        // via this pipe
        redirectWatchdogInputChannel(fileno(stdin)); // stdin
        redirectWatchdogOutputChannel(fileno(stdout)); // stderr
        redirectWatchdogOutputChannel(fileno(stderr)); // stdout
        closeWatchdogUsedChannel(PR_FALSE);
    }
}

int
wdSMF::captureData(int childPid) {
    int childStatus = 0;
    int childExited = 0;
    int moreData = 0;
    int fdsReady = 0;
    int numContinuousTimeouts = 0;
    int dataToBeRead = 0;
    SmfMsgHdr hdr;

    if (smfPipeOutFd < 0) {
       int tries = 2;
       int rv = 0;
       do {
           rv = openSmfOutputChannel();
           if (rv == 2)
               sleep(1);
           tries--;
       } while(rv == 2 && tries > 0);
    }

    do {
        childExited = 0;
        if (childPid > 0) {
            int cPid = waitpid(childPid, &childStatus,WNOHANG);
            if (cPid == childPid) 
                childExited = 1;
        }

        fdsReady = waitForData();

        if (fdsReady > 0) {
            numContinuousTimeouts = 0;
            moreData = processData(fdsReady, &dataToBeRead, &hdr);
            if (moreData < 0) {
                return 3;
            }

        } else if (fdsReady == -1) {
            if (errno != EINTR) {
                return 2; // error waiting
            }
            numContinuousTimeouts = 0;

        } else if (fdsReady == -2) {
                return 3; // POLLHUP

        } else if (fdsReady == 0) { // timedout
            numContinuousTimeouts++;

            // check if child terminated
            // if yes, then return
            if (childExited) 
                return 4;
        }

    } while((numContinuousTimeouts < POLL_RETRIES) || (moreData > 0 && fdsReady > 0) || (fdsReady < 0 && errno == EAGAIN));

    if (numContinuousTimeouts >= POLL_RETRIES)
        return 5; // timeout

    return 0;
}

char *
wdSMF::readFromChannel(int fd, int readLen, int *len) {
    // keep reading from the pipe
    int lenToRead = (readLen > 0) ? readLen : MAX_BUF_SIZE-1;
    *len = 0;
    char *buf = (char *)malloc(sizeof(char) * lenToRead+1);
    /* Read from the pipe */
    do {
        *len = read(fd, buf, lenToRead);
    } while((*len < 1) && (errno == EINTR || errno == EAGAIN));
    if (*len > 0) {
        buf[*len] = 0;
    }
    return buf; //success
}

void
wdSMF::closeSmfWatchdogChannel() {
    // close the pipe
    if (smfPipeInFd >= 0) {
        close(smfPipeInFd);
        smfPipeInFd = -1;
    }
    if (smfPipeOutFd >= 0) {
        close(smfPipeOutFd);
        smfPipeOutFd = -1;
    }

    if (wd_smf_pipe_out_create) {
        // remove the output named pipe file
        // this should be removed by smf watchdog
        if (wdtype == WD_SMF) {
            unlink(wd_smf_pipe_out_path);
        }
        wd_smf_pipe_out_create = 0;
    }

    if (wd_smf_pipe_in_create) {
        // remove the input named pipe file
        // this should be removed by watchdog parent
        if (wdtype == WD_PARENT) {
            unlink(wd_smf_pipe_in_path);
        }
        wd_smf_pipe_in_create = 0;
    }
}

SmfMsgHdr
wdSMF::readHeader() {
    SmfMsgHdr hdr;
    int len;
    // Read from the pipe
    len = read(smfPipeOutFd, (char *)&hdr, sizeof(hdr));
    assert(len==sizeof(hdr));
    return hdr;
}

int
wdSMF::sendHeader(SmfMsgType type, int len) {
    SmfMsgHdr hdr;
    hdr.msgType = type;
    hdr.len = len;
    int rv = writeToChannel(smfPipeOutFd, (char *)&hdr, sizeof(hdr));
    if (rv < 0) {
        // other end of the smfPipeOutFd pipe does not exist
        close(smfPipeOutFd);
        smfPipeOutFd = -1;
    } else
        assert(rv == sizeof(hdr));

    return rv;
}

int
wdSMF::writeToChannel(int fd, char *msg, int len) {
    int totalBytesSent = 0;
    int bytesSent = -1;
    // writing to smf channel
    if (len > 0) {
        PRBool tryAgain = PR_FALSE;
        int bytesLeft = len;

        do {
            bytesSent = write(fd, msg, bytesLeft);
            if (bytesSent > 0) {
                totalBytesSent += bytesSent;
                bytesLeft -= bytesSent;
            }
        } while(((bytesSent > 0) && (bytesLeft > 0)) || ((bytesSent < 0) && (errno == EAGAIN || errno == EINTR)));
    }
    if (bytesSent < 0 && (errno == EPIPE || errno == EBADF))
        totalBytesSent = -1;

    return totalBytesSent;
}

void
wdSMF::redirectWatchdogOutputChannel(int fromFD) {
    int fd = wdPipes[OUTPUT_PIPE][PIPE_WRITE_END];
    if (fd >= 0 && fd != fromFD) {
        dup2(fd, fromFD);
    }
}

void
wdSMF::redirectWatchdogInputChannel(int fromFD) {
    int fd = wdPipes[INPUT_PIPE][PIPE_READ_END];
    if (fd >= 0 && fd != fromFD) {
        dup2(fd, fromFD);
    }
}

PRStatus
wdSMF::addPollEntry(int fd) {
    int i;
    for (i = 0; i < smf_pa_table_size; i++) {
        if (smf_pa_table[i].fd == -1) {
            // Found empty element: set it there
            smf_pa_table[i].fd = fd;
            smf_pa_table[i].events =
                    (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI);
            smf_pa_table[i].revents = 0;
            if ((i + 1) > smf_pa_count)
                smf_pa_count = i + 1;
            return PR_SUCCESS; // success
        }
    }

    return PR_FAILURE;
}

PRStatus
wdSMF::removePollEntry(int index) {
    if (index >= 0 && index < smf_pa_table_size) {
        smf_pa_table[index].fd = -1;
        smf_pa_table[index].events = 0;
        smf_pa_table[index].revents = 0;
        smf_fd_read_ready_table[index] = 0;
        return PR_SUCCESS;

    } else
        return PR_FAILURE;
}

int
wdSMF::waitForData() {
    int someDataToRead = 0;
    int ready = poll(smf_pa_table, smf_pa_count, 5000 /* five seconds */ );

    if (ready == 0)          // Timeout
        return ready;
    else if (ready == -1) {  // Error ??
        return ready;
    } else {                 // > 0 means found events
        int i, count;
        count = ready;
        for (i = 0; i < smf_pa_table_size; i++) {
            if (smf_pa_table[i].fd >= 0) {  // only look at valid entries
                if (smf_pa_table[i].revents != 0) { // Found events
                    if ((smf_pa_table[i].revents & POLLRDBAND) ||
                               (smf_pa_table[i].revents & POLLIN) ||
                               (smf_pa_table[i].revents & POLLRDNORM) ||
                               (smf_pa_table[i].revents & POLLPRI)) {
                        // ready for Read
                        smf_fd_read_ready_table[i] = 1;

                    } else if ((smf_pa_table[i].revents & POLLHUP) ||
                        (smf_pa_table[i].revents & POLLNVAL)) { // Disconnect
                        smf_fd_read_ready_table[i] = 2;
                    }

                    smf_pa_table[i].revents = 0;
                    count--;
                    if (count == 0)
                        break;
                }
            }
        }
        return ready;
    }
}

int
wdSMF::processData(int numFds, int *dataToBeRead, SmfMsgHdr *hdr) {
    int fdsRemoved = 0;
    int closedFds = 0;
    int len = 0;
    if (numFds == 0) return len;

    int count = 0;
    int i;

    for (i=0; i < smf_pa_count && count < numFds; i++) {
        int status = smf_fd_read_ready_table[i];
        if (status != 0) { // Only look at these
            count++;
            smf_fd_read_ready_table[i] = 0; // Clear it
            if (status > 1) {
                closedFds++;
                continue;
            }

            // read from the pipe
            int fd = smf_pa_table[i].fd;
            int dataLen, toBeRead = 0;
            char *data;
            if (wdtype == WD_SMF) {
                if (readSmfHeader) {
                    *hdr = readHeader();
                    readSmfHeader = PR_FALSE;
                    *dataToBeRead = hdr->len;
                    continue;
                }
            }

            data = readFromChannel(fd, *dataToBeRead, &dataLen);
            if (dataLen > 0 && data) {
                if (len <= 0)
                    len = dataLen;

                if (wdtype == WD_SMF) { // data has been read from smf output channel
                    *dataToBeRead -= dataLen;
                    if (*dataToBeRead <= 0) {
                        readSmfHeader = PR_TRUE;
                        *dataToBeRead = 0;
                    }
                    if (hdr) {
                        if (hdr->msgType == SMF_OUTPUT) {
                            fprintf(stderr, "%s", data);
                        } else if(hdr->msgType == SMF_PWD_PROMPT) {
                            if (smfPipeInFd < 0) {
                                openSmfInputChannel();
                            }
                            if (smfPipeInFd >= 0)
                                getPassword(data, 0, NULL);
                        }
                    }

                } else { // data might have been read from smf channel or watchdog parent channel
                    // see if fd is same as the smfPipeInFd
                    // if yes, then data has been read from smf input channel
                    if ((smfPipeInFd >= 0) && (fd == smfPipeInFd)) {
                        // write it to watchdog parent's input channel
                        //int bytesSent = writeToChannel(pipIn[1], data, dataLen);
                        int bytesSent = writeToChannel(wdPipes[INPUT_PIPE][PIPE_WRITE_END], data, dataLen);
                        if (bytesSent <= 0) {
                            return 0; // write error
                        }
                        // close smf watchdog's input channel
                        pwdFlag = 0;
                        removePollEntry(i);
                        fdsRemoved++;
                        close(smfPipeInFd);
                        smfPipeInFd = -1;

                    } else {
                        int wdOutFd = wdPipes[OUTPUT_PIPE][PIPE_READ_END];
                        int wdFlagFd = wdPipes[FLAG_PIPE][PIPE_READ_END];
                        if ((wdOutFd >= 0) && (fd == wdOutFd)) { // data is from watchdog parent's output channel
                            // write it to watchdog parent's stdout
                            fprintf(stderr, "%s", data);
                            // also write it to smf output channel
                            if (smfPipeOutFd >= 0 && (sendHeader(SMF_OUTPUT, dataLen) > 0)) {
                                int bytesSent = writeToChannel(smfPipeOutFd, data, dataLen);
                                if (bytesSent <= 0) {
                                    return 0; // write error
                                }
                            }
                        } else if ((wdFlagFd >= 0) && (fd == wdFlagFd)) {
                            if (strlen(data) > 0) {
                                if (smfPipeOutFd >= 0) {
                                    pwdFlag = 1;
                                    openSmfInputChannel();
                                    sendPwdPrompt(data);
                                } else {
                                    // no smf watchdog, so don't wait for input from smf watchdog
                                    // close the parent's input channel
                                    close(wdPipes[INPUT_PIPE][PIPE_WRITE_END]);
                                    wdPipes[INPUT_PIPE][PIPE_WRITE_END] = -1;
                                }
                            }
                        }
                    }
                }

                free (data);
            }
        }
    }

    if (fdsRemoved > 0)
        smf_pa_count -= fdsRemoved;

    if (closedFds == smf_pa_count)
        return -2;

    return len;
}

int
wdSMF::sendPwd(char *pwdStr) {
    // send the password through smf input channel
    // and close the channel
    int rv = 0;
    int len = (pwdStr)? strlen(pwdStr):0;
    if (len > 0) {
        int bytesSent = writeToChannel(smfPipeInFd, pwdStr, len);
        if (bytesSent <= 0) {
            rv = 2; // write error
        }
    }
    return rv;
}

int
wdSMF::sendPwdPrompt(char *prompt) {
    int rv = -1;
    int fd = -1;
    if (prompt) {
        if (wdtype == WD_PARENT) {
            if (smfPipeOutFd >= 0) {
                fd = smfPipeOutFd;
                rv = sendHeader(SMF_PWD_PROMPT, strlen(prompt));
            } else {
                // let it go to the svc log
                fd = fileno(stderr);
            }
        } else {
            fd = wdPipes[FLAG_PIPE][PIPE_WRITE_END]; // watchdog child is sending this prompt to the parent
        }

        if (fd >= 0) {
            rv = write(fd, prompt, strlen(prompt));
	}
    }

    return rv;
}

int
wdSMF::getPassword(char *prompt, int serial, char **pwdValue) {
    if (wdtype == WD_CHILD) {
        if (sendPwdPrompt(prompt) <= 0) {
            return 1;
        }
        int rv = smf_get_pwd((char *)prompt, pwdValue);
        return rv;

    } else if (wdtype == WD_SMF) {
        if (smfPipeInFd >= 0) {
            char *pwd1 = NULL;
            int rv = 0;
            if (watchdog_pwd_prompt(prompt, 0, &pwd1, true) != 0) {
                rv = 1; // error reading password
                pwd1 = strdup("\n");
                sendPwd(pwd1);
            } else {
                rv = sendPwd(pwd1);
            }

            // close smf watchdog's input channel, if required open it again.
            close(smfPipeInFd);
            smfPipeInFd = -1;
            pwdFlag = 0;
            if (pwd1)
                free(pwd1);
            if (rv != 0)
                return rv;
        }
    }

    return 0; // success
}

void 
wdSMF::logMsg(char *msg) {
    fprintf(stderr, "loggin msg\n");
    FILE *fdLog = fopen("/tmp/wdLogMsg.out", "a");
    if (msg && fdLog)
        fprintf(fdLog, "wdMsg: %s\n", msg);
    if (fdLog) {
        fclose(fdLog);
    }
}
