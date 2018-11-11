/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_EVENT_POSIX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_EVENT_POSIX_H_

#include "webrtc/system_wrappers/interface/event_wrapper.h"

#include <pthread.h>
#include <time.h>

#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace webrtc {

enum State {
  kUp = 1,
  kDown = 2
};

class EventPosix : public EventWrapper {
 public:
  static EventWrapper* Create();

  ~EventPosix() override;

  EventTypeWrapper Wait(unsigned long max_time) override;
  bool Set() override;

  bool StartTimer(bool periodic, unsigned long time) override;
  bool StopTimer() override;

 private:
  EventPosix();

  static bool Run(void* obj);
  bool Process();
  EventTypeWrapper Wait(timespec* end_at);

 private:
  pthread_cond_t  cond_;
  pthread_mutex_t mutex_;
  bool event_set_;

  rtc::scoped_ptr<ThreadWrapper> timer_thread_;
  EventPosix*    timer_event_;
  timespec       created_at_;

  bool          periodic_;
  unsigned long time_;  // In ms
  unsigned long count_;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_EVENT_POSIX_H_
