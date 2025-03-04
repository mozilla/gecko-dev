/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/field_trials.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"
#include "system_wrappers/include/field_trial.h"

namespace {

webrtc::flat_map<std::string, std::string> InsertIntoMap(absl::string_view s) {
  webrtc::flat_map<std::string, std::string> key_value_map;
  while (!s.empty()) {
    absl::string_view::size_type separator_pos = s.find('/');
    RTC_CHECK_NE(separator_pos, absl::string_view::npos)
        << "Missing separator '/' after field trial key.";
    RTC_CHECK_GT(separator_pos, 0) << "Field trial key cannot be empty.";
    absl::string_view key = s.substr(0, separator_pos);
    s.remove_prefix(separator_pos + 1);

    RTC_CHECK(!s.empty())
        << "Missing value after field trial key. String ended.";
    separator_pos = s.find('/');
    RTC_CHECK_NE(separator_pos, absl::string_view::npos)
        << "Missing terminating '/' in field trial string.";
    RTC_CHECK_GT(separator_pos, 0) << "Field trial value cannot be empty.";
    absl::string_view value = s.substr(0, separator_pos);
    s.remove_prefix(separator_pos + 1);

    // If a key is specified multiple times, only the value linked to the first
    // key is stored. note: This will crash in debug build when calling
    // InitFieldTrialsFromString().
    key_value_map.emplace(key, value);
  }

  return key_value_map;
}

// Makes sure that only one instance is created, since the usage
// of global string makes behaviour unpredicatable otherwise.
// TODO(bugs.webrtc.org/10335): Remove once global string is gone.
std::atomic<bool> instance_created_{false};

}  // namespace

namespace webrtc {

FieldTrials::FieldTrials(absl::string_view s)
    : uses_global_(true),
      field_trial_string_(s),
      previous_field_trial_string_(webrtc::field_trial::GetFieldTrialString()),
      key_value_map_(InsertIntoMap(s)) {
  // TODO(bugs.webrtc.org/10335): Remove the global string!
  field_trial::InitFieldTrialsFromString(field_trial_string_.c_str());
  RTC_CHECK(!instance_created_.exchange(true))
      << "Only one instance may be instanciated at any given time!";
}

std::unique_ptr<FieldTrials> FieldTrials::CreateNoGlobal(absl::string_view s) {
  return std::unique_ptr<FieldTrials>(new FieldTrials(s, true));
}

FieldTrials::FieldTrials(absl::string_view s, bool)
    : uses_global_(false),
      previous_field_trial_string_(nullptr),
      key_value_map_(InsertIntoMap(s)) {}

FieldTrials::~FieldTrials() {
  // TODO(bugs.webrtc.org/10335): Remove the global string!
  if (uses_global_) {
    field_trial::InitFieldTrialsFromString(previous_field_trial_string_);
    RTC_CHECK(instance_created_.exchange(false));
  }
}

std::string FieldTrials::GetValue(absl::string_view key) const {
  auto it = key_value_map_.find(key);
  if (it != key_value_map_.end())
    return it->second;

  // Check the global string so that programs using
  // a mix between FieldTrials and the global string continue to work
  // TODO(bugs.webrtc.org/10335): Remove the global string!
  if (uses_global_) {
    return field_trial::FindFullName(key);
  }
  return "";
}

}  // namespace webrtc
