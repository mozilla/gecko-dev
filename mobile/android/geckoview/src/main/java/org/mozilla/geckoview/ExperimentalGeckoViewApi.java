/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.geckoview;

import androidx.annotation.RequiresOptIn;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * GeckoView APIs annotated with {@link ExperimentalGeckoViewApi} are subject to change or else
 * inherently more dangerous or unstable.
 *
 * <p>This annotation covers:
 *
 * <ul>
 *   <li>API signatures that may change without notice or deprecation period.
 *   <li>API behavior and interactions that may change without notice.
 *   <li>Features that are under heavy development and are considered unstable.
 *   <li>Features that have inherent risk or potentially destabilize other parts of GeckoView.
 * </ul>
 *
 * <p>Only use an API annotated with this if you fully understand the API and accept the risks.
 */
@Retention(RetentionPolicy.CLASS)
@Target({
  ElementType.TYPE,
  ElementType.TYPE_USE,
  ElementType.METHOD,
  ElementType.CONSTRUCTOR,
  ElementType.PACKAGE
})
@RequiresOptIn(
    level = RequiresOptIn.Level.ERROR,
    message =
        "This API is experimental and should only be used with caution. "
            + "Annotate with @OptIn to accept the risk.")
public @interface ExperimentalGeckoViewApi {}
