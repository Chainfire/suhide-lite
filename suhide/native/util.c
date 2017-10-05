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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <dirent.h>

#include "ndklog.h"

#ifndef DEBUG
// prettify process name for ps output
// we should really be using prctl(PR_SET_NAME, ...) or pthread_getname_np for this...
void prettify(int argc, char* argv[], char* pretty) {
    for (int i = 0; i < argc; i++) {
        memset(argv[i], ' ', strlen(argv[i]));
    }
    memcpy(argv[0], pretty, strlen(pretty) + 1);
}
#endif

// close all fds we don't need
void close_parent_fds(int* except, int len) {
    DIR* dir;
    struct dirent *ent;
    if ((dir = opendir("/proc/self/fd")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            int fd = atoi(ent->d_name);
            if (fd > 2) {
                int doclose = 1;

                int i;
                for (i = 0; i < len; i++) {
                    if (except[i] == fd) {
                        doclose = 0;
                        break;
                    }
                }

                if (doclose) {
                    char path[PATH_MAX];
                    char link[PATH_MAX];
                    memset(path, 0, PATH_MAX);
                    memset(link, 0, PATH_MAX);
                    snprintf(path, sizeof(path), "/proc/self/fd/%s", ent->d_name);

                    int linklen = readlink(path, link, PATH_MAX);
                    if (linklen > 0) {
                        if (strncmp("/dev/__properties__", link, 19) == 0) {
                            // keep properties readable
                            doclose = 0;
                        }
                    }
                }

                if (doclose) {
                    close(fd);
                }
            }
        }
        closedir(dir);
    }

    return;
}

// become a daemon by forking twice with a setsid in between; exits current process unless
// returnParent is set; returns -1 for error. Note that a second fork error is not detected,
// though these are extremely rare.
int fork_daemon(int returnParent) {
    pid_t child = fork();
    if (child == 0) {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        int devNull = open("/dev/null", O_RDWR);
        dup2(devNull, STDIN_FILENO);
        dup2(devNull, STDOUT_FILENO);
        dup2(devNull, STDERR_FILENO);
        close(devNull);

        close_parent_fds(NULL, 0);

        setsid();
        pid_t child2 = fork();
        if (child2 <= 0) return 0; // success child or error on 2nd fork
        exit(EXIT_SUCCESS);
    }
    if (child < 0) return -1; // error on 1st fork
    int status;
    waitpid(child, &status, 0);
    if (!returnParent) exit(EXIT_SUCCESS);
    return 1; // success parent
}

// sleep for ms ms, returns ms left if EINTR occurs
int ms_sleep(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    if ((nanosleep(&ts,&ts) == -1) && (errno == EINTR)) {
        int ret = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
        if (ret < 1) ret = 1;
        return ret;
    }
    return 0;
}

// get current timestamp
struct timeval timestamp() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv;
}

// get the difference in ms between two timestamps
int timestamp_diff_ms(struct timeval x, struct timeval y) {
    if (x.tv_usec < y.tv_usec) {
        int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
        y.tv_usec -= 1000000 * nsec;
        y.tv_sec += nsec;
    }
    if (x.tv_usec - y.tv_usec > 1000000) {
        int nsec = (x.tv_usec - y.tv_usec) / 1000000;
        y.tv_usec += 1000000 * nsec;
        y.tv_sec -= nsec;
    }
    return ((x.tv_sec - y.tv_sec) * 1000) + ((x.tv_usec - y.tv_usec) / 1000);
}
