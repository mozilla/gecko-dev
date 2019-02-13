/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.AppConstants;

public class HealthReportConstants {
  public static final String HEALTH_AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".health";
  public static final String GLOBAL_LOG_TAG = "GeckoHealth";

  public static final String USER_AGENT = "Firefox-Android-HealthReport/" + AppConstants.MOZ_APP_VERSION + " (" + AppConstants.MOZ_APP_DISPLAYNAME + ")";

  /**
   * The earliest allowable value for the last ping time, corresponding to May 2nd 2013.
   * Used for sanity checks.
   */
  public static final long EARLIEST_LAST_PING = 1367500000000L;

  // Not `final` so we have the option to turn this on at runtime with a magic addon.
  public static boolean UPLOAD_FEATURE_DISABLED = false;

  // Android SharedPreferences branch where global (not per-profile) uploader
  // settings are stored.
  public static final String PREFS_BRANCH = "background";

  // How frequently the submission and prune policies are ticked over. This is how frequently our
  // intent is scheduled to be called by the Android Alarm Manager, not how frequently we
  // actually submit. These values are set as preferences rather than constants so that testing
  // addons can change their values.
  public static final String PREF_SUBMISSION_INTENT_INTERVAL_MSEC = "healthreport_submission_intent_interval_msec";
  public static final long DEFAULT_SUBMISSION_INTENT_INTERVAL_MSEC = GlobalConstants.MILLISECONDS_PER_DAY / 24;
  public static final String PREF_PRUNE_INTENT_INTERVAL_MSEC = "healthreport_prune_intent_interval_msec";
  public static final long DEFAULT_PRUNE_INTENT_INTERVAL_MSEC = GlobalConstants.MILLISECONDS_PER_DAY;

  public static final String ACTION_HEALTHREPORT_UPLOAD_PREF = AppConstants.ANDROID_PACKAGE_NAME + ".HEALTHREPORT_UPLOAD_PREF";
  public static final String ACTION_HEALTHREPORT_PRUNE = AppConstants.ANDROID_PACKAGE_NAME + ".HEALTHREPORT_PRUNE";

  public static final String PREF_MINIMUM_TIME_BETWEEN_UPLOADS = "healthreport_time_between_uploads";
  public static final long DEFAULT_MINIMUM_TIME_BETWEEN_UPLOADS = GlobalConstants.MILLISECONDS_PER_DAY;

  public static final String PREF_MINIMUM_TIME_BEFORE_FIRST_SUBMISSION = "healthreport_time_before_first_submission";
  public static final long DEFAULT_MINIMUM_TIME_BEFORE_FIRST_SUBMISSION = GlobalConstants.MILLISECONDS_PER_DAY;

  public static final String PREF_MINIMUM_TIME_AFTER_FAILURE = "healthreport_time_after_failure";
  public static final long DEFAULT_MINIMUM_TIME_AFTER_FAILURE = DEFAULT_SUBMISSION_INTENT_INTERVAL_MSEC;

  public static final String PREF_MAXIMUM_FAILURES_PER_DAY = "healthreport_maximum_failures_per_day";
  public static final long DEFAULT_MAXIMUM_FAILURES_PER_DAY = 2;

  // Authoritative.
  public static final String PREF_FIRST_RUN = "healthreport_first_run";
  public static final String PREF_NEXT_SUBMISSION = "healthreport_next_submission";
  public static final String PREF_CURRENT_DAY_FAILURE_COUNT = "healthreport_current_day_failure_count";
  public static final String PREF_CURRENT_DAY_RESET_TIME = "healthreport_current_day_reset_time";

  // Forensic.
  public static final String PREF_LAST_UPLOAD_REQUESTED = "healthreport_last_upload_requested";
  public static final String PREF_LAST_UPLOAD_SUCCEEDED = "healthreport_last_upload_succeeded";
  public static final String PREF_LAST_UPLOAD_FAILED = "healthreport_last_upload_failed";

  // Preferences for deleting obsolete documents.
  public static final String PREF_MINIMUM_TIME_BETWEEN_DELETES = "healthreport_time_between_deletes";
  public static final long DEFAULT_MINIMUM_TIME_BETWEEN_DELETES = DEFAULT_SUBMISSION_INTENT_INTERVAL_MSEC;

  public static final String PREF_OBSOLETE_DOCUMENT_IDS_TO_DELETION_ATTEMPTS_REMAINING = "healthreport_obsolete_document_ids_to_deletions_remaining";

  // We don't want to try to delete forever, but we also don't want to orphan
  // obsolete document IDs from devices that fail to reach the server for a few
  // days.  This tries to delete document IDs for at least one week (of upload
  // failures).  Note that if the device is really offline, no upload is
  // performed and our count of attempts is not altered.
  public static final long DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID = (DEFAULT_MAXIMUM_FAILURES_PER_DAY + 1) * 7;

  // If we absolutely know that a document ID reached the server, we really
  // don't want to orphan it.  This tries to delete document IDs that will
  // definitely be orphaned for at least six weeks (of upload failures).  Note
  // that if the device is really offline, no upload is performed and our count
  // of attempts is not altered.
  public static final long DELETION_ATTEMPTS_PER_KNOWN_TO_BE_ON_SERVER_DOCUMENT_ID = (DEFAULT_MAXIMUM_FAILURES_PER_DAY + 1) * 7 * 6;

  // We don't want to allocate unbounded storage for obsolete IDs, but we also
  // don't want to orphan obsolete document IDs from devices that fail to delete
  // for a few days.  This stores as many IDs as are expected to be generated in
  // a month.  Note that if the device is really offline, no upload is performed
  // and our count of attempts is not altered.
  public static final long MAXIMUM_STORED_OBSOLETE_DOCUMENT_IDS = (DEFAULT_MAXIMUM_FAILURES_PER_DAY + 1) * 30;

  // Forensic.
  public static final String PREF_LAST_DELETE_REQUESTED = "healthreport_last_delete_requested";
  public static final String PREF_LAST_DELETE_SUCCEEDED = "healthreport_last_delete_succeeded";
  public static final String PREF_LAST_DELETE_FAILED = "healthreport_last_delete_failed";

  // Preferences for upload client.
  public static final String PREF_LAST_UPLOAD_LOCAL_TIME  = "healthreport_last_upload_local_time";
  public static final String PREF_LAST_UPLOAD_DOCUMENT_ID  = "healthreport_last_upload_document_id";

  public static final String PREF_DOCUMENT_SERVER_URI = "healthreport_document_server_uri";
  public static final String DEFAULT_DOCUMENT_SERVER_URI = "https://fhr.data.mozilla.com/";

  public static final String PREF_DOCUMENT_SERVER_NAMESPACE = "healthreport_document_server_namespace";
  public static final String DEFAULT_DOCUMENT_SERVER_NAMESPACE = "metrics";

  // One UUID is 36 characters (like e56542e0-e4d2-11e2-a28f-0800200c9a66), so
  // we limit the number of obsolete IDs passed so that each request is not a
  // large upload (and therefore more likely to fail).  We also don't want to
  // push Bagheera to make too many deletes, since we don't know how the cluster
  // will handle such API usage.  This obsoletes 2 days worth of old documents
  // at a time.
  public static final int MAXIMUM_DELETIONS_PER_POST = ((int) DEFAULT_MAXIMUM_FAILURES_PER_DAY + 1) * 2;

  public static final String PREF_PRUNE_BY_SIZE_TIME = "healthreport_prune_by_size_time";
  public static final long MINIMUM_TIME_BETWEEN_PRUNE_BY_SIZE_CHECKS_MILLIS =
      GlobalConstants.MILLISECONDS_PER_DAY;
  public static final int MAX_ENVIRONMENT_COUNT = 50;
  public static final int ENVIRONMENT_COUNT_AFTER_PRUNE = 35;
  public static final int MAX_EVENT_COUNT = 10000;
  public static final int EVENT_COUNT_AFTER_PRUNE = 8000;

  public static final String PREF_EXPIRATION_TIME = "healthreport_expiration_time";
  public static final long MINIMUM_TIME_BETWEEN_EXPIRATION_CHECKS_MILLIS = GlobalConstants.MILLISECONDS_PER_DAY * 7;
  public static final long EVENT_EXISTENCE_DURATION = GlobalConstants.MILLISECONDS_PER_SIX_MONTHS;

  public static final String PREF_CLEANUP_TIME = "healthreport_cleanup_time";
  public static final long MINIMUM_TIME_BETWEEN_CLEANUP_CHECKS_MILLIS = GlobalConstants.MILLISECONDS_PER_DAY * 30;
}
