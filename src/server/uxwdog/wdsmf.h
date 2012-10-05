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

#ifndef _WDSMF_H_
#define _WDSMF_H_

#define PA_TABLE_SIZE 10                     // max size of the poll table

// SMF output msg type
typedef enum {
    SMF_OUTPUT=0,  // regular output msg
    SMF_PWD_PROMPT // password prompt msg
} SmfMsgType;

typedef struct {
    SmfMsgType msgType;
    int len;
} SmfMsgHdr;

class wdSMF {
  public:
        enum wdType {WD_NONE=0, WD_SMF, WD_PARENT, WD_CHILD};

        wdSMF(const char *instanceConfigDir, const char *tmpDir, const char *smfStartCmd);
        ~wdSMF();

        PRBool isSmfMode();
        PRBool calledBySmf();
        void setWatchdogType(wdType type);
        int startSmfService();
        void setupSmfEnvironment();
        int createSmfWatchdogChannel();
        int createWatchdogChannel();
        int captureData(int childPid=-1);
        int getPassword(char *prompt, int serial, char **pwdvalue);
        void logMsg(char *msg);

  private:
        void setNonInheritable();
        void closeSmfWatchdogChannel();
        int openSmfInputChannel();
        int openSmfOutputChannel();
        void closeWatchdogUsedChannel(PRBool closeFlagPipe=PR_TRUE);
        void closeWatchdogUnusedChannel();
        void redirectWatchdogOutputChannel(int fromFD);
        void redirectWatchdogInputChannel(int fromFD);
        SmfMsgHdr readHeader();
        int sendHeader(SmfMsgType type, int len);
        int sendPwd(char *pwdStr);
        int sendPwdPrompt(char *prompt);
        int waitForData();
        int processData(int numFds, int *dataToBeRead, SmfMsgHdr *hdr);
        PRStatus addPollEntry(int fd);
        PRStatus removePollEntry(int index);
        int writeToChannel(int fd, char *msg, int len);
        char *readFromChannel(int fd, int readLen, int *len);

        PRBool readSmfHeader;
        char *ws_inst_conf_dir;      // Web Server instance's config directory
        char *ws_temp_dir;           // Web Server's temporary directory
        char *smf_start_cmd;         // SMF start command to start the Web Server
        wdType wdtype;               // Watchdog type
        char *wd_smf_pipe_in_path;   // input named pipe path
        char *wd_smf_pipe_out_path;  // output named pipe path
        int wd_smf_pipe_in_create;   // flag set when smf input pipe created
        int wd_smf_pipe_out_create;  // flag set when smf output pipe created
        int smfPipeInFd;             // SMF input pipe file descriptor
        int smfPipeOutFd;            // SMF output pipe file descriptor
        int service_started;         // flag which indicates if the service has started or not
        int pwdFlag;                 // flag to indicate the need for password
        int wdPipes[3][2];           // regular watchdog pipes for input, output and password flag
        struct pollfd * smf_pa_table; // Table of poll fds used for SMF related communications
        int smf_fd_read_ready_table[PA_TABLE_SIZE]; // table which stores the status of the fds
        int smf_pa_count;             // number of poll fds used
        int smf_pa_table_size;        // number of poll fds allocated
};

#endif /* _WDSMF_H_ */
