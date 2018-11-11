/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_FIELD_TRIAL_H_
#define WEBRTC_TEST_FIELD_TRIAL_H_

#include <string>

namespace webrtc {
namespace test {

// Parses enabled field trials from a string config, such as the one passed
// to chrome's argument --force-fieldtrials and initializes webrtc::field_trial
// with such a config.
//  E.g.:
//    "WebRTC-experimentFoo/Enabled/WebRTC-experimentBar/Enabled100kbps/"
//    Assigns the process to group "Enabled" on WebRTCExperimentFoo trial
//    and to group "Enabled100kbps" on WebRTCExperimentBar.
//
//  E.g. invalid config:
//    "WebRTC-experiment1/Enabled"  (note missing / separator at the end).
//
// Note: This method crashes with an error message if an invalid config is
// passed to it. That can be used to find out if a binary is parsing the flags.
void InitFieldTrialsFromString(const std::string& config);

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_FIELD_TRIAL_H_
