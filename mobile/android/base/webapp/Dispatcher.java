/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class Dispatcher extends Activity {
    private static final String LOGTAG = "GeckoWebappDispatcher";

    @Override
    protected void onCreate(Bundle bundle) {
        super.onCreate(bundle);

        Allocator allocator = Allocator.getInstance(getApplicationContext());

        if (bundle == null) {
            bundle = getIntent().getExtras();
        }

        if (bundle == null) {
            Log.e(LOGTAG, "Passed intent data missing.");
            return;
        }

        String packageName = bundle.getString("packageName");

        if (packageName == null) {
            Log.e(LOGTAG, "Package name data missing.");
            return;
        }

        int index = allocator.getIndexForApp(packageName);
        boolean isInstalled = index >= 0;
        if (!isInstalled) {
            index = allocator.findOrAllocatePackage(packageName);
        }

        // Copy the intent, without interfering with it.
        Intent intent = new Intent(getIntent());

        // Only change its destination.
        intent.setClassName(getApplicationContext(), "org.mozilla.gecko.webapp.Webapps$Webapp" + index);

        // If and only if we haven't seen this before.
        intent.putExtra("isInstalled", isInstalled);

        startActivity(intent);
    }
}
