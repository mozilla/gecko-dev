/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_virtualfilesystem_virtualfilesystemlog_h__
#define mozilla_dom_virtualfilesystem_virtualfilesystemlog_h__

#undef USE_DEBUG
#define USE_DEBUG 0

#if !defined(VIRTUAL_FILE_SYSTEM_LOG_TAG)
#define VIRTUAL_FILE_SYSTEM_LOG_TAG  "VirtualFileSystem"
#endif

#undef LOG
#undef ERR
#ifdef MOZ_WIDGET_GONK
#define LOG(msg, ...)  __android_log_print(ANDROID_LOG_INFO,  VIRTUAL_FILE_SYSTEM_LOG_TAG, msg, ##__VA_ARGS__)
#define ERR(msg, ...)  __android_log_print(ANDROID_LOG_ERROR, VIRTUAL_FILE_SYSTEM_LOG_TAG, msg, ##__VA_ARGS__)
#else
#define LOG(msg, ...) printf_stderr(msg, ##__VA_ARGS__)
#define ERR(msg, ...) printf_stderr(msg, ##__VA_ARGS__)
#endif

#undef DBG
#if USE_DEBUG
#ifdef MOZ_WIDGET_GONK
#define DBG(msg, ...)  __android_log_print(ANDROID_LOG_DEBUG, VIRTUAL_FILE_SYSTEM_LOG_TAG, msg, ##__VA_ARGS__)
#else
#define DBG(msg, ...) printf_stderr(msg, ##__VA_ARGS__)
#endif
#else
#define DBG(args...)
#endif

#endif  // mozilla_dom_virtualfilesystem_virtualfilesystemlog_h__
