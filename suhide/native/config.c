/*
 * Copyright (C) 2017 Jorrit "Chainfire" Jongma & CCMT
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "ndklog.h"

#ifndef AID_USER
#define AID_USER 100000
#endif
#ifndef AID_APP
#define AID_APP 10000
#endif

#define UIDFILE "/sbin/supersu/suhide/suhide.uid"

static uid_t* uids = NULL;
static int uid_count = 0;

static char** processes = NULL;
static int process_count = 0;

static time_t last_uid_time = 0;

// load uids and process names root should be hidden from, checks last modification of config file
void load_config() {
    struct stat stat;
    lstat(UIDFILE, &stat);
    if (stat.st_mtime == last_uid_time) return;
    last_uid_time = stat.st_mtime;

    int fd = open(UIDFILE, O_RDONLY);
    if (fd < 0) return;

    int buf_size = (int)stat.st_size;
    char buf[buf_size];
    int buf_read = 0;

    while (1) {
        int r = read(fd, &buf[buf_read], buf_size - buf_read - 1);
        if (r > 0) {
            buf_read += r;
        } else {
            break;
        }
    }

    if (buf_read == 0) {
        close(fd);
        return;
    }

    buf[buf_read] = '\0';
    buf_read++;

    if (uids != NULL) {
        free(uids);
        uids = NULL;
    }
    if (processes != NULL) {
        free(processes);
        processes = NULL;
    }
    int count_uid = 0;
    int count_process = 0;
    for (int loop = 0; loop < 2; loop++) {
        if (loop == 1) {
            uids = (uid_t*)malloc(sizeof(uid_t) * count_uid);
            uid_count = count_uid;
            count_uid = 0;

            processes = (char**)malloc(sizeof(char*) * count_process);
            process_count = count_process;
            count_process = 0;
        }

        int start = 0;
        for (int i = 0; i < buf_read; i++) {
            if ((buf[i] == '\r') || (buf[i] == '\n') || (buf[i] == ' ')) buf[i] = '\0';
            if (buf[i] == '\0') {
                if ((start > -1) && (start < i - 1)) {
                    uid_t uid = atoi(&buf[start]);
                    if (uid > 0) {
                        if (loop == 1) {
                            LOGD("[%d] uid[%d]-->[%d]", getpid(), count_uid, uid);
                            uids[count_uid] = uid;
                        }
                        count_uid++;
                    } else {
                        if (loop == 1) {
                            LOGD("[%d] process[%d]-->[%s]", getpid(), count_process, &buf[start]);
                            processes[count_process] = malloc(strlen(&buf[start]) + 1);
                            strcpy(processes[count_process], &buf[start]);
                        }
                        count_process++;
                    }
                }
                start = i + 1;
            }
        }
    }

    close(fd);
}

// is root access allowed for gid ?
int allow_root_for_uid(gid_t gid) {
    if ((gid % AID_USER) < AID_APP) return 1;

    if (uid_count == 0) return 1;

    for (int i = 0; i < uid_count; i++) {
        if (uids[i] == gid) return 0;
    }

    return 1;
}

// is root access allowed for process name ?
int allow_root_for_name(char* name) {
    if (process_count == 0) return 1;

    for (int i = 0; i < process_count; i++) {
        if (strcmp(processes[i], name) == 0) return 0;
    }

    return 1;
}