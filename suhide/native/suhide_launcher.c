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

/* Main file for suhide (launcher), which launches suhide[32|64] children that
 * attach to zygote(64) as debugger to hide root. Additionally, it monitors
 * hardware button input and (un)hides packages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <linux/input.h>

#include "ndklog.h"
#include "util.h"
#include "getevent.h"

// pids for currently running versions of suhide and zygote
pid_t suhide32 = 0;
pid_t suhide64 = 0;
pid_t zygote32 = 0;
pid_t zygote64 = 0;

// do we have 64-bit versions?
int have64 = 0;

// get path to executable, self must be PATH_MAX in size, returns 0 on success
static int get_self(char* self) {
    int len = readlink("/proc/self/exe", self, PATH_MAX);
    if ((len == 0) || (len >= PATH_MAX)) return 1;
    self[len] = '\0';
    return 0;
}

// get path to suhide executable, self and suhide must be PATH_MAX in size, returns 0 on success
static int get_suhide(char* bits, char* self, char* suhide) {
    memcpy(suhide, self, strlen(self));
    memcpy(&suhide[strlen(self)], bits, strlen(bits) + 1);
    if (access(suhide, X_OK) != 0) {
        suhide[0] = '\0';
        return 1;
    }
    return 0;
}

// find pid for process, returns 0 on error
static pid_t find_process(char* name) {
    char buf[PATH_MAX], path[PATH_MAX];
    pid_t ret = 0;
    DIR* dir;
    struct dirent *ent;
    if ((dir = opendir("/proc/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            pid_t pid = atoi(ent->d_name);
            if (pid > 0) {
                memset(path, 0, 64);
                snprintf(path, 64, "/proc/%d/exe", pid);
                int len = readlink(path, buf, PATH_MAX);
                if ((len >= 0) && (len < PATH_MAX)) {
                    buf[len] = '\0';
                    if (strstr(buf, "app_process") != NULL) {
                        memset(path, 0, 64);
                        snprintf(path, 64, "/proc/%d/cmdline", pid);
                        int fd = open(path, O_RDONLY);
                        if (fd >= 0) {
                            if (read(fd, buf, PATH_MAX) > strlen(name)) {
                                if ((strncmp(buf, name, strlen(name)) == 0) && ((buf[strlen(name)] == '\0') || (buf[strlen(name)] == ' '))) {
                                    ret = pid;
                                }
                            }
                            close(fd);
                        }
                    }
                }
            }
            if (ret > 0) break;
        }
        closedir(dir);
    }
    return ret;
}

// launch child suhide process with zygote parameter, returns new pid
static pid_t launch_child(char* path, pid_t zygote) {
    if (strlen(path) == 0) return 0;
    if (zygote == 0) return 0;

#ifdef DEBUG
    fprintf(stderr, "launching: %s\n", path);
#endif

    pid_t child = fork();
    if (child == 0) {
        char param[PATH_MAX];
        snprintf(param, PATH_MAX, "%d", zygote);
        execl(path, path, param, (char*)NULL);
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    fprintf(stderr, "-- pid: %d\n", child);
#endif

    return child;
}

static void stop_android() {
// just for testing purposes, requires calling 'start' twice, doesn't work right with 32+64 bit systems
/*
    pid_t child = fork();
    if (child == 0) {
        execl("/system/bin/stop", "stop", NULL);
        exit(EXIT_FAILURE);
    }
    int status;
    waitpid(child, &status, 0);
*/
}

// run the switch_packages script to hide/unhide selected packages
static void switch_packages() {
    pid_t child = fork();
    if (child == 0) {
        execl("/sbin/supersu/suhide/switch_packages", "switch_packages", (char*)NULL);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[], char** envp) {
    // start with --nodaemon for debugging purposes
    if (!((argc >= 2) && (strcmp(argv[1], "--nodaemon") == 0))) {
        fork_daemon(0);
    }

#ifndef DEBUG
    // make ourselves less obvious in ps output
    prettify(argc, argv, "system_server");
#endif

    // find paths to 32 and 64-bit version of suhide
    char path_self[PATH_MAX], path_suhide32[PATH_MAX], path_suhide64[PATH_MAX];
    if (get_self(path_self) != 0) return 1;
    get_suhide("32", path_self, path_suhide32);
    have64 = get_suhide("64", path_self, path_suhide64) == 0 ? 1 : 0;

    // last 6 input_events
    struct input_event events[6];
    memset(&events[0], 0, sizeof(events[0]) * 6);

    // never quit
    while (1) {
#ifdef DEBUG
        fprintf(stderr, "outer loop\n");
#endif

        // find 32 and 64-bit zygote processes, waiting for them to start if not yet running
        while ((zygote32 == 0) || ((zygote64 == 0) && (have64 == 1))) {
            int found = 0;
            if (zygote32 == 0) {
                zygote32 = find_process("zygote");
                if (zygote32 != 0) {
                    fprintf(stderr, "zygote32: %d\n", zygote32);
                    found = 1;
                }
            } else if ((zygote64 == 0) && (have64 == 1)) {
                zygote64 = find_process("zygote64");
                if (zygote64 != 0) {
                    fprintf(stderr, "zygote64: %d\n", zygote64);
                    found = 1;
                }
            }

            if (!found)
                ms_sleep(128);
        }

        // launch suhide processes if not yet running
        if (suhide32 == 0) suhide32 = launch_child(path_suhide32, zygote32);
        if (suhide64 == 0) suhide64 = launch_child(path_suhide64, zygote64);

        // (re-)initialize input event monitoring
        reset_getevent();

        // loop until one of our suhide children exits
        while (1) {
#ifdef DEBUG
            fprintf(stderr, "inner loop\n");
#endif

            // input detection loop, waits one second for input and keeps looping until
            // there hasn't been any input for again one second
            struct input_event event;
            int event_count = 0;
            while ((event_count = get_event(&event, 1000)) > 0) {
#ifdef DEBUG
                fprintf(stderr, "event loop\n");
#endif

                if (
                        (event.type == EV_KEY) &&
                        (event.value == 1) // down
                ) {
                    int i;
                    for (i = 0; i < 5; i++) {
                        events[i] = events[i + 1];
                    }
                    events[5] = event;

                    // UP, DOWN, UP, DOWN, UP DOWN, 3 seconds, no other keys
                    if (
                        (events[0].code == KEY_VOLUMEUP) &&
                        (events[1].code == KEY_VOLUMEDOWN) &&
                        (events[2].code == KEY_VOLUMEUP) &&
                        (events[3].code == KEY_VOLUMEDOWN) &&
                        (events[4].code == KEY_VOLUMEUP) &&
                        (events[5].code == KEY_VOLUMEDOWN)
                    ) {
                        struct timeval diff;
                        timersub(&events[3].time, &events[0].time, &diff);
                        if ((int)diff.tv_sec < 3) {
                            memset(&events[0], 0, sizeof(events[0]) * 6);
                            fprintf(stderr, "Switching package visibility...\n");
                            switch_packages();
                        }
                    }
                }
            }

            // error occurred, probably a device got added or removed, re-init
            if (event_count < 0) break;

#ifdef DEBUG
            fprintf(stderr, "waitpid\n");
#endif

            // check if our suhide children are still alive, break parent loop and re-init otherwise
            int status;
            pid_t waited = waitpid(0, &status, WNOHANG);
            if ((waited > 0) && ((waited == suhide32) || (waited == suhide64))) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    if (waited == suhide32) {
                        stop_android();
                        fprintf(stderr, "suhide32 stopped, restarting\n");
                        suhide32 = 0;
                        zygote32 = 0;
                        break;
                    } else if (waited == suhide64) {
                        stop_android();
                        fprintf(stderr, "suhide64 stopped, restarting\n");
                        suhide64 = 0;
                        zygote64 = 0;
                        break;
                    }
                }
            }
        }
    }

    return 0;
}