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

/* Main file for suhide[32|64]. This process attaches to zygote(64) and monitors forks and
 * clones. As soon as an app forks from zygote it checks if that app should have root or not,
 * and if necessary unmounts all root-related mounts.
 *
 * All of this is based around PTRACE, though it is not architecture-specific. PTRACE is
 * tricky to work with, and there are many odd edge-cases. The current code has been
 * extensively tested to work as expected, don't touch it unless you're absolutely
 * sure you know what you're doing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sched.h>
#include <sys/mount.h>

#include "ndklog.h"
#include "util.h"
#include "trace.h"
#include "config.h"

// unmount all root-related mounts from pid; done by forking a child which enters the target's
// namespace, enumerates mounts, and unmounts the non-standard ones
static void unmount_root(char* name, pid_t zygote, pid_t pid) {
    pid_t child = fork();
    if (child == 0) {
        // child
        char path1[PATH_MAX];
        char path2[PATH_MAX];
        char ns1[PATH_MAX];
        char ns2[PATH_MAX];
        snprintf(path1, PATH_MAX, "/proc/%d/ns/mnt", zygote);
        snprintf(path2, PATH_MAX, "/proc/%d/ns/mnt", pid);
        ssize_t len1 = readlink(path1, ns1, PATH_MAX);
        ssize_t len2 = readlink(path2, ns2, PATH_MAX);
        if ((len1 > 0) && (len2 > 0)) {
            ns1[len1] = '\0';
            ns2[len2] = '\0';
            if (strcmp(ns1, ns2) != 0) {
                // we truly have different namespaces
                int nsfd = open(path2, O_RDONLY);
                if (nsfd >= 0) {
                    if (syscall(__NR_setns, nsfd, CLONE_NEWNS) == 0) {
                        // read mounts

                        int fd = open("/proc/self/mountinfo", O_RDONLY);
                        if (fd >= 0) {
                            char buf[32768];
                            int total = 0;
                            int size = 32768;
                            while (1) {
                                int r = read(fd, &buf[total], size - total);
                                if (r <= 0) break;
                                total += r;
                            }
                            close(fd);

                            if (total > 0) {
                                char* start = buf;
                                for (int i = 0; i < total; i++) {
                                    if (buf[i] == '\n') {
                                        buf[i] = '\0';

                                        char p1[PATH_MAX];
                                        char p2[PATH_MAX];
                                        char p3[PATH_MAX];
                                        char source[PATH_MAX];
                                        char target[PATH_MAX];
                                        char p6[PATH_MAX];
                                        char p7[PATH_MAX];
                                        char p8[PATH_MAX];
                                        char fs[PATH_MAX];

                                        if (sscanf(start, "%s %s %s %s %s %s %s %s %s", p1, p2, p3, source, target, p6, p7, p8, fs) == 9) {
                                            if (
                                                (strcmp(target, "/sbin") == 0) ||
                                                (strncmp(target, "/sbin/", 6) == 0) ||
                                                (strcmp(target, "/root/sbin") == 0) ||
                                                (strncmp(target, "/root/sbin/", 11) == 0) ||
                                                (strcmp(target, "/data/adb/su") == 0) ||  //TODO readlink /sbin/supersu ?
                                                (strncmp(target, "/data/adb/su/", 13) == 0) ||
                                                (strstr(source, "/adb/su") != NULL) ||
                                                (strstr(target, "/system/") != NULL) ||
                                                (strstr(target, "/vendor/") != NULL) ||
                                                (strstr(target, "/original/") != NULL) ||
                                                (
                                                    (
                                                        (strcmp(fs, "tmpfs") == 0)
                                                    ) && (
                                                        (strcmp(target, "/system") == 0) ||
                                                        (strcmp(target, "/vendor") == 0) ||
                                                        (strcmp(target, "/oem") == 0) ||
                                                        (strcmp(target, "/odm") == 0)
                                                    )
                                                )
                                            ) {
                                                if (umount2(target, MNT_DETACH) == 0) {
                                                    LOGD("[%d] [%s] unmounted", pid, target);
                                                } else {
                                                    LOGD("[%d] [%s] unmount failed", pid, target);
                                                }
                                            }
                                        }

                                        start = &buf[i + 1];
                                    }
                                }
                            } else {
                                LOGD("[%d] empty read from mountinfo", pid);
                            }
                        } else {
                            LOGD("[%d] failed to read mountinfo", pid);
                        }
                    } else {
                        LOGD("[%d] failed to join namespace", pid);
                    }
                    close(nsfd);
                } else {
                    LOGD("[%d] failed to join namespace", pid);
                }
            }
        }
        exit(EXIT_SUCCESS);
    } else {
        // parent
        waitpid(child, NULL, 0);
    }
}

// detects if a pid (that has been forked/cloned from zygote) has changed its name to its
// final form (usually based on package name), check if that package is supposed to have root,
// and if not, unmount root-related mounts from its namespace.
static int detect_package_and_unmount(int pid, int zygote) {
    char path[PATH_MAX];
    struct stat stat;
    snprintf(path, PATH_MAX, "/proc/%d/task", pid);
    lstat(path, &stat);
    if (stat.st_uid == 0) return 0;

    char cmdline[128];
    snprintf(path, PATH_MAX, "/proc/%d/cmdline", pid);
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        int len = read(fd, cmdline, 128);
        if ((len > 0) && (len < 128)) {
            cmdline[len] = '\0';
            for (int i = 0; i < len; i++) {
                if ((cmdline[i] == ' ') || (cmdline[i] == ':') || (cmdline[i] == '\0')) {
                    cmdline[i] = '\0';
                    break;
                }
            }
        }
        close(fd);

        if ((strcmp(cmdline, "zygote") != 0) && (strcmp(cmdline, "zygote64") != 0) && (strncmp(cmdline, "<", 1) != 0)) {
            // Name has been prettified at this point, (see com_android_internal_os_Zygote.cpp::setThreadName() or
            // ZygoteConnection.java::handleChildProc()).
            // The process's mount namespace should already be private (see com_android_internal_os_Zygote.cpp::MountEmulatedStorage()).
            // Just after those two things happen, zygote is still single-threaded, but an Android
            // app never is. This code here is executed when the second thread is created.

            LOGD("[%d] forked [%s] (%d)", pid, cmdline, stat.st_uid);

            load_config();
            if (!allow_root_for_uid(stat.st_uid) || !allow_root_for_name(cmdline)) {
                unmount_root(cmdline, zygote, pid);
            }
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[], char** envp) {
    (void)detach_tid; // prevent unused function error

    if (argc != 2) {
        LOGD("Usage: %s <pid>", LOG_TAG);
        return 1;
    }
    pid_t target = atol(argv[1]);
    if (target == 0) {
        LOGD("Invalid pid passed [%s]", argv[1]);
        return 1;
    }

#ifndef DEBUG
    // make ourselves less obvious in ps output
    prettify(argc, argv, strstr(LOG_TAG, "64") == 0 ? "zygote64" : "zygote");
#endif

    // attach to target and monitor its forks and clones
    if (trace(PTRACE_ATTACH, target, NULL, 0) != -1) {
        LOGD("Attached to [%d]", target);
        wait_stop(target);

        trace(PTRACE_SETOPTIONS, target, NULL,
            PTRACE_O_TRACECLONE |
//            PTRACE_O_TRACEEXEC | #do not want
            PTRACE_O_TRACEEXIT |
            PTRACE_O_TRACEFORK |
//            PTRACE_O_TRACESYSGOOD | #do not want
            PTRACE_O_TRACEVFORK // |
//            PTRACE_O_TRACEVFORKDONE | #do not want
//            PTRACE_O_TRACESECCOMP | #do not want
//            PTRACE_O_SUSPEND_SECCOMP #do not want and does not exist in headers
        );

#define PID_MAX 32768
        int first_stop[PID_MAX] = {0};
        int forked[PID_MAX] = {0};
        int parent[PID_MAX] = {0};

        int status;
        trace(PTRACE_CONT, target, NULL, 0);
        while (1) {
            int detached = 0;
            int pid = waitpid(-1, &status, __WALL);
            int signal = 0;
            if (pid > 0) {
                LOGD("[%d] waitpid", pid);
                if (WIFSTOPPED(status)) {
                    LOGD("[%d] stopped", pid);
                    if (WSTOPSIG(status) == SIGTRAP) {
                        if (WEVENT(status) != 0) { // not sure yet why those happen
                            // see https://lwn.net/Articles/446593/ for some of this handling
#ifdef DEBUG
                            char* event = "?";
                            switch (WEVENT(status)) {
                                case PTRACE_EVENT_FORK: event = "FORK"; break;
                                case PTRACE_EVENT_VFORK: event = "VFORK"; break;
                                case PTRACE_EVENT_CLONE: event = "CLONE"; break;
                                case PTRACE_EVENT_EXIT: event = "EXIT"; break;
                            }
#endif
                            int childpid = -1;
                            trace(PTRACE_GETEVENTMSG, pid, 0, (size_t)&childpid);
                            LOGD("[%d] trapped: [%s][%d] [%d]", pid, event, WEVENT(status), childpid);

                            if ((WEVENT(status) == PTRACE_EVENT_FORK) || (WEVENT(status) == PTRACE_EVENT_VFORK) || (WEVENT(status) == PTRACE_EVENT_CLONE)) {
                                if ((pid == target) && (WEVENT(status) != PTRACE_EVENT_CLONE)) { // fork of target
                                    forked[childpid] = 1;
                                    parent[childpid] = childpid;
                                } else if (forked[pid] && (WEVENT(status) == PTRACE_EVENT_CLONE)) { // clone of fork
                                    forked[childpid] = 1;
                                    int p = pid;
                                    while ((p != 0) && (parent[p] != p)) p = parent[p];
                                    parent[childpid] = p;

                                    if (detect_package_and_unmount(p, target)) {
                                        LOGD("[%d] package detected [%d]", pid, childpid);
                                        signal = -1;
                                        if (trace(PTRACE_CONT, pid, NULL, 0) != ESRCH) {
                                            detach_pid(p);
                                        }
                                    } else {
                                        LOGD("[%d] package NOT detected [%d]", pid, childpid);
                                    }
                                } else {
                                    forked[childpid] = 0;
                                    parent[childpid] = 0;
                                }
                                first_stop[childpid] = 1;
                            } else if (WEVENT(status) == PTRACE_EVENT_EXIT) {
                                // use pid here, not childpid !
                                if (pid == target)
                                    break;
                                trace(PTRACE_CONT, pid, NULL, 0);
                                detached = 1;
                            }
                        }
                    } else {
                        if (first_stop[pid]) {
                            // new fork or clone starts with a STOP signal

                            first_stop[pid] = 0;
                            if (forked[pid]) {
                                // we don't want forks of our forks to be traced, but we do want clones (threads)
                                trace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACECLONE);
                            }

                            LOGD("[%d] stopped (first): %d [%08x]", pid, WSTOPSIG(status), status);
                        } else if (WSTOPSIG(status) != SIGSTOP) { // we cause SIGSTOP, ignore and drop
                            pid_t from = -1;
                            (void)from; // unused variable error
                            siginfo_t siginfo;
                            if (ptrace(PTRACE_GETSIGINFO, pid, 0, (size_t)&siginfo) == 0) {
                                from = siginfo.si_pid;
                            }

                            LOGD("[%d] stopped: %d from [%d]", pid, WSTOPSIG(status), from);
                            signal = WSTOPSIG(status);
                        }
                    }
                } else if (WIFSIGNALED(status)) {
                    LOGD("[%d] signaled: %d", pid, WTERMSIG(status));
                    if (pid == target)
                        break;
                    detached = 1;
                } else if (status == 0) {
                    LOGD("[%d] died: %d", pid, status);
                    if (pid == target)
                        break;
                    detached = 1;
                } else {
                    LOGD("[%d] status: %d", pid, status);
                }
                if (!detached) {
                    if (signal >= 0) {
                        trace(PTRACE_CONT, pid, NULL, signal);
                    }
                } else {
                    first_stop[pid] = 0;
                    forked[pid] = 0;
                    parent[pid] = 0;
                }
            }
        }

        // detach
        trace(PTRACE_DETACH, target, NULL, 0);
        kill(target, SIGCONT);
        LOGD("Detached from [%d]", target);
    } else {
        LOGD("Attach failed [%d]", errno);
        return 1;
    }

    return 0;
}