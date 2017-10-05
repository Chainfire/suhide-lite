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

#ifndef _TRACE_H
#define _TRACE_H

#define SIGSTOPSYSTRACE 0x80 //see PTRACE_O_TRACESYSGOOD

#define WEVENT(s) (((s) & 0xffff0000) >> 16)

long trace(int request, pid_t pid, void *addr, size_t data);
int cont(pid_t group, pid_t target);
int stop(pid_t group, pid_t target);
void wait_stop(pid_t target);
int stop_and_wait_stop(pid_t group, pid_t target);
int stop_and_detach(pid_t group, pid_t target);
void detach_pid(int pid);
void detach_tid(int pid, int tid);

#endif