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
#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include <linux/ptrace.h>
#include <sys/wait.h>
#include <dirent.h>

#include "ndklog.h"
#include "util.h"

// missing declaration
int tgkill(int tgid, int tid, int sig);

// ptrace with a log wrapper
long trace(int request, pid_t pid, void *addr, size_t data) {
    long ret = ptrace(request, pid, (caddr_t) addr, (void *) data);
    if (ret == -1) {
        LOGD("TRACE(%d, %d, %d, %d): %d", request, pid, (int)(uintptr_t)addr, (int)data, errno);
    }
    return ret;
}

// send SIGCONT via tgkill
int cont(pid_t group, pid_t target) {
    return tgkill(group, target, SIGCONT);
}

// send SIGSTOP via tgkill
int stop(pid_t group, pid_t target) {
    return tgkill(group, target, SIGSTOP);
}

// wait for target to stop
void wait_stop(pid_t target) {
    LOGD("[%d] wait_stop(%d)", target, target);
    struct timeval start = timestamp();
    while (1) {
        int status;
        int pid = waitpid(target, &status, __WALL | WNOHANG);
        LOGD("[%d] waitpid --> %d/%d", target, pid, status);
        if ((pid == target) && WIFSTOPPED(status)) {
            break;
        } else if (pid == -1) {
            // error
            LOGD("[%d] waitpid --> error", target);
            break;
        } else {
            if (timestamp_diff_ms(timestamp(), start) > 128) {
                LOGD("[%d] waitpid --> timeout, zombie?", target);
                break;
            }
            ms_sleep(1);
        }
    }
    LOGD("[%d] /wait_stop(%d)", target, target);
}

// stop target and wait for it to become stopped
int stop_and_wait_stop(pid_t group, pid_t target) {
    LOGD("[%d] stop_and_wait_stop(%d, %d)", group, group, target);
    if (stop(group, target) == 0) {
        wait_stop(target);
        return 0;
    }
    LOGD("[%d] /stop_and_wait_stop(%d, %d) ABORT", group, group, target);
    return 1;
}

// detach from thread and continue it
static void detach_tid(int pid, int tid) {
    if (tgkill(pid, tid, SIGSTOP) == 0) {
        wait_stop(tid);
        trace(PTRACE_SETOPTIONS, tid, NULL, 0);
        trace(PTRACE_DETACH, tid, NULL, 0);
        tgkill(pid, tid, SIGCONT);
    }
}

// stop target (thread) and detach from it
int stop_and_detach(pid_t group, pid_t target) {
    LOGD("[%d] stop_and_detach(%d, %d)", group, group, target);
    if (ptrace(PTRACE_SETOPTIONS, target, NULL, 0) == 0) {
        trace(PTRACE_DETACH, target, NULL, 0);
        LOGD("[%d] /stop_and_detach(%d, %d) DIRECT", group, group, target);
        return 0;
    } else if (stop_and_wait_stop(group, target) == 0) {
        trace(PTRACE_SETOPTIONS, target, NULL, 0);
        trace(PTRACE_DETACH, target, NULL, 0);
        LOGD("[%d] /stop_and_detach(%d, %d) STOP/WAIT", group, group, target);
        return 0;
    }
    LOGD("[%d] /stop_and_detach(%d, %d) OTHER", group, group, target);
    return 1;
}

// stop and detach from each of pid's threads, then continue pid's execution
void detach_pid(int pid) {
    char task[PATH_MAX];
    snprintf(task, PATH_MAX, "/proc/%d/task", pid);

    LOGD("[%d] detaching", pid);
    stop_and_detach(pid, pid);

    DIR* dir;
    struct dirent *ent;
    if ((dir = opendir(task)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            pid_t tid = atoi(ent->d_name);
            if ((tid > 0) && (tid != pid)) {
                stop_and_detach(pid, tid);
            }
        }
        closedir(dir);
    }

    cont(pid, pid);
    cont(pid, pid); // yes, twice

    LOGD("[%d] detached", pid);
}
