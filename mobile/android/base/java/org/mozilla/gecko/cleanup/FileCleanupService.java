/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.cleanup;

import android.app.IntentService;
import android.content.Intent;
import android.util.Log;

import java.io.File;
import java.util.ArrayList;

/**
 * An IntentService to delete files.
 *
 * It takes an {@link ArrayList} of String file paths to delete via the extra
 * {@link #EXTRA_FILE_PATHS_TO_DELETE}. If these file paths are directories, they will
 * not be traversed recursively and will only be deleted if empty. This is to avoid accidentally
 * trashing a users' profile if a folder is accidentally listed.
 *
 * An IntentService was chosen because:
 *   * It generally won't be killed when the Activity is
 *   * (unlike HandlerThread) The system handles scheduling, prioritizing,
 * and shutting down the underlying background thread
 *   * (unlike an existing background thread) We don't block our background operations
 * for this, which doesn't directly affect the user.
 *
 * The major trade-off is that this Service is very dangerous if it's exported... so don't do that!
 */
public class FileCleanupService extends IntentService {
    private static final String LOGTAG = "Gecko" + FileCleanupService.class.getSimpleName();
    private static final String WORKER_THREAD_NAME = LOGTAG + "Worker";

    public static final String ACTION_DELETE_FILES = "org.mozilla.gecko.intent.action.DELETE_FILES";
    public static final String EXTRA_FILE_PATHS_TO_DELETE = "org.mozilla.gecko.file_paths_to_delete";

    public FileCleanupService() {
        super(WORKER_THREAD_NAME);

        // We're likely to get scheduled again - let's wait until then in order to avoid:
        //   * The coding complexity of re-running this
        //   * Consuming system resources: we were probably killed for resource conservation purposes
        setIntentRedelivery(false);
    }

    @Override
    protected void onHandleIntent(final Intent intent) {
        if (!isIntentValid(intent)) {
            return;
        }

        final ArrayList<String> filesToDelete = intent.getStringArrayListExtra(EXTRA_FILE_PATHS_TO_DELETE);
        for (final String path : filesToDelete) {
            final File file = new File(path);
            file.delete();
        }
    }

    private static boolean isIntentValid(final Intent intent) {
        if (intent == null) {
            Log.w(LOGTAG, "Received null intent");
            return false;
        }

        if (!intent.getAction().equals(ACTION_DELETE_FILES)) {
            Log.w(LOGTAG, "Received unknown intent action: " + intent.getAction());
            return false;
        }

        if (!intent.hasExtra(EXTRA_FILE_PATHS_TO_DELETE)) {
            Log.w(LOGTAG, "Received intent with no files extra");
            return false;
        }

        return true;
    }
}
