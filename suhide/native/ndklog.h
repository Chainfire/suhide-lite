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

// Slightly modified cutils/log.h for use in NDK
//
// define LOG_TAG prior to inclusion
//
// if you define DEBUG or LOG_DEBUG before inclusion, VDIWE are logged
// if LOG_SILENT is defined, nothing is logged (overrides (LOG_)DEBUG)
// if none of the above are defined, VD are ignored and IWE are logged

#ifndef _NDK_LOG_H
#define _NDK_LOG_H

#include <android/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG_TAG
#define LOG_TAG "NDK_LOG"
#endif

#ifdef DEBUG
	#define LOG_DEBUG
#endif

#ifndef LOG_SILENT
	#define LOG(...) __android_log_print(__VA_ARGS__)
#else
	#define LOG(...) (void)0
#endif

#ifdef LOG_DEBUG
	#define LOGV(...) LOG(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
	#define LOGD(...) LOG(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
	#define LOGV(...) (void)0
	#define LOGD(...) (void)0
#endif
#define LOGI(...) LOG(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) LOG(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) LOG(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define CONDITION(cond) (__builtin_expect((cond)!=0, 0))

#define LOGV_IF(cond, ...) ((CONDITION(cond)) ? LOGV(__VA_ARGS__) : (void)0)
#define LOGD_IF(cond, ...) ((CONDITION(cond)) ? LOGD(__VA_ARGS__) : (void)0)
#define LOGI_IF(cond, ...) ((CONDITION(cond)) ? LOGI(__VA_ARGS__) : (void)0)
#define LOGW_IF(cond, ...) ((CONDITION(cond)) ? LOGW(__VA_ARGS__) : (void)0)
#define LOGE_IF(cond, ...) ((CONDITION(cond)) ? LOGE(__VA_ARGS__) : (void)0)

#ifdef __cplusplus
}
#endif

#endif // _NDK_LOG_H