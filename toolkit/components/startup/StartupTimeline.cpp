/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StartupTimeline.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "nsXULAppAPI.h"

namespace mozilla {

TimeStamp StartupTimeline::sStartupTimeline[StartupTimeline::MAX_EVENT_ID];
const char *StartupTimeline::sStartupTimelineDesc[StartupTimeline::MAX_EVENT_ID] = {
#define mozilla_StartupTimeline_Event(ev, desc) desc,
#include "StartupTimeline.h"
#undef mozilla_StartupTimeline_Event
};

} /* namespace mozilla */

using mozilla::StartupTimeline;
using mozilla::TimeStamp;

/**
 * The XRE_StartupTimeline_Record function is to be used by embedding
 * applications that can't use mozilla::StartupTimeline::Record() directly.
 *
 * @param aEvent The event to be recorded, must correspond to an element of the
 *               mozilla::StartupTimeline::Event enumartion
 * @param aWhen  The time at which the event happened
 */
void
XRE_StartupTimelineRecord(int aEvent, TimeStamp aWhen)
{
  StartupTimeline::Record((StartupTimeline::Event)aEvent, aWhen);
}
