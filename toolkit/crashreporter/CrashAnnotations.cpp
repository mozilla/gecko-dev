/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashAnnotations.h"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "nsString.h"

using std::begin;
using std::end;
using std::find_if;

namespace CrashReporter {

using mozilla::Nothing;
using mozilla::Some;

Maybe<Annotation> AnnotationFromString(const nsACString& aValue) {
  const auto* elem = find_if(
      begin(kAnnotationStrings), end(kAnnotationStrings),
      [&aValue](const char* aString) { return aValue.Equals(aString); });

  if (elem == end(kAnnotationStrings)) {
    return Nothing();
  }

  return Some(static_cast<Annotation>(elem - begin(kAnnotationStrings)));
}

template <size_t N>
static bool AnnotationInList(Annotation aAnnotation,
                             const Annotation (&aList)[N]) {
  const auto* elem = find_if(
      begin(aList), end(aList),
      [&aAnnotation](Annotation aElement) { return aElement == aAnnotation; });

  return elem != end(aList);
}

bool IsAnnotationAllowedForPing(Annotation aAnnotation) {
  return AnnotationInList(aAnnotation, kCrashPingAllowedList);
}

bool IsAnnotationAllowedForReport(Annotation aAnnotation) {
  return AnnotationInList(aAnnotation, kCrashPingAllowedList) ||
         AnnotationInList(aAnnotation, kCrashReportAllowedList);
}

bool ShouldIncludeAnnotation(Annotation aAnnotation, const char* aValue) {
  const auto* elem = find_if(begin(kSkipIfList), end(kSkipIfList),
                             [&aAnnotation](AnnotationSkipValue aElement) {
                               return aElement.annotation == aAnnotation;
                             });

  if (elem != end(kSkipIfList)) {
    if (strcmp(aValue, elem->value) == 0) {
      return false;
    }
  }

  return true;
}

}  // namespace CrashReporter
