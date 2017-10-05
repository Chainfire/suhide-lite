/*
 * Copyright (C) 2014 The Android Open Source Project
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

 // Loosely based on: http://androidxref.com/7.1.1_r6/xref/system/core/debuggerd/getevent.cpp

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/limits.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <errno.h>

static struct pollfd* ufds = NULL;
static int nfds = 0;

// opens an input device, returns 0 on success
static int open_device(const char* device) {
    int fd;
    struct pollfd* new_ufds;
    int version;
    struct input_id id;

    fd = open(device, O_RDWR);
    if (fd < 0) return -1;
    if (ioctl(fd, EVIOCGVERSION, &version)) return -1;
    if (ioctl(fd, EVIOCGID, &id)) return -1;

    new_ufds = (struct pollfd*)realloc(ufds, sizeof(ufds[0]) * (nfds + 1));
    ufds = new_ufds;
    ufds[nfds].fd = fd;
    ufds[nfds].events = POLLIN;
    nfds++;

    return 0;
}

// scan a dir and open all contained input devices
static int scan_dir(const char* dirname) {
    char devname[PATH_MAX];
    char* filename;
    DIR* dir;
    struct dirent* de;
    dir = opendir(dirname);
    if (dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while ((de = readdir(dir))) {
        if ((de->d_name[0] == '.' && de->d_name[1] == '\0') ||
           (de->d_name[1] == '.' && de->d_name[2] == '\0'))
            continue;
        strcpy(filename, de->d_name);
        open_device(devname);
    }
    closedir(dir);
    return 0;
}

// (re-)init event monitoring
void reset_getevent() {
    const char* device_path = "/dev/input";
    if (ufds != NULL) {
        int i;
        for (i = 0; i < nfds; i++) {
            close(ufds[i].fd);
        }
        free(ufds);
        ufds = NULL;
    }
    nfds = 0;
    scan_dir(device_path);
}

// get most recent event with a timeout. returns number of events returned [0..1] or -1 on error
int get_event(struct input_event* event, int timeout) {
    int res;
    int i;
    int pollres;
    while (1) {
        pollres = poll(ufds, nfds, timeout);
        if (pollres == 0) {
            return 0;
        } else if (pollres == -1) {
            return -1;
        }
        for (i = 0; i < nfds; i++) {
            if (ufds[i].revents) {
                if (ufds[i].revents & POLLIN) {
                    res = read(ufds[i].fd, event, sizeof(*event));
                    if (res < (int)sizeof(event)) {
                        return -1;
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}