/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_cloudstoragelog_h__
#define mozilla_system_cloudstoragelog_h__

#undef USE_DEBUG
#define USE_DEBUG 0

#if !defined(CLOUD_STORAGE_LOG_TAG)
#define CLOUD_STORAGE_LOG_TAG  "CloudStorage"
#endif

#undef LOG
#undef ERR
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO,  CLOUD_STORAGE_LOG_TAG, ## args)
#define ERR(args...)  __android_log_print(ANDROID_LOG_ERROR, CLOUD_STORAGE_LOG_TAG, ## args)

#undef DBG
#if USE_DEBUG
#define DBG(args...)  __android_log_print(ANDROID_LOG_DEBUG, CLOUD_STORAGE_LOG_TAG, ## args)
#else
#define DBG(args...)
#endif

#endif  // mozilla_system_cloudstoragelog_h__
