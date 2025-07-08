/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Telemetry_h__
#define Telemetry_h__

#include "mozilla/Maybe.h"
#include "mozilla/TelemetryEventEnums.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/TelemetryScalarEnums.h"
#include "mozilla/TimeStamp.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

/******************************************************************************
 * This implements the Telemetry system.
 * It allows recording into histograms as well some more specialized data
 * points and gives access to the data.
 *
 * For documentation on how to add and use new Telemetry probes, see:
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/start/adding-a-new-probe.html
 *
 * For more general information on Telemetry see:
 * https://wiki.mozilla.org/Telemetry
 *****************************************************************************/

namespace mozilla {
namespace Telemetry {

struct HistogramAccumulation;
struct KeyedHistogramAccumulation;
struct ScalarAction;
struct KeyedScalarAction;
struct ChildEventData;

struct EventExtraEntry {
  nsCString key;
  nsCString value;
};

/**
 * Initialize the Telemetry service on the main thread at startup.
 */
void Init();

/**
 * Shutdown the Telemetry service.
 */
void ShutdownTelemetry();

const char* GetHistogramName(HistogramID id);

/**
 * Indicates whether Telemetry base data recording is turned on. Added for
 * future uses.
 */
bool CanRecordBase();

/**
 * Indicates whether Telemetry extended data recording is turned on.  This is
 * intended to guard calls to Accumulate when the statistic being recorded is
 * expensive to compute.
 */
bool CanRecordExtended();

/**
 * Indicates whether Telemetry release data recording is turned on. Usually
 * true.
 *
 * @see nsITelemetry.canRecordReleaseData
 */
bool CanRecordReleaseData();

/**
 * Indicates whether Telemetry pre-release data recording is turned on. Tends
 * to be true on pre-release channels.
 *
 * @see nsITelemetry.canRecordPrereleaseData
 */
bool CanRecordPrereleaseData();

/**
 * Records slow SQL statements for Telemetry reporting.
 *
 * @param statement - offending SQL statement to record
 * @param dbName - DB filename
 * @param delay - execution time in milliseconds
 */
void RecordSlowSQLStatement(const nsACString& statement,
                            const nsACString& dbName, uint32_t delay);

/**
 * Initialize I/O Reporting
 * Initially this only records I/O for files in the binary directory.
 *
 * @param aXreDir - XRE directory
 */
void InitIOReporting(nsIFile* aXreDir);

/**
 * Set the profile directory. Once called, files in the profile directory will
 * be included in I/O reporting. We can't use the directory
 * service to obtain this information because it isn't running yet.
 */
void SetProfileDir(nsIFile* aProfD);

/**
 * Called to inform Telemetry that startup has completed.
 */
void LeavingStartupStage();

/**
 * Called to inform Telemetry that shutdown is commencing.
 */
void EnteringShutdownStage();

/**
 * Thresholds for a statement to be considered slow, in milliseconds
 */
const uint32_t kSlowSQLThresholdForMainThread = 50;
const uint32_t kSlowSQLThresholdForHelperThreads = 100;

/**
 * Record a failed attempt at locking the user's profile.
 *
 * @param aProfileDir The profile directory whose lock attempt failed
 */
void WriteFailedProfileLock(nsIFile* aProfileDir);

}  // namespace Telemetry
}  // namespace mozilla

#endif  // Telemetry_h__
