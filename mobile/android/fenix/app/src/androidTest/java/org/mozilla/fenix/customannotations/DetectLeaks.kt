/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customannotations

/**
 * Custom annotation to opt in to memory leak detection
 *
 * You can annotate a test function or the test class to enable memory leak checks for the test or
 * test suite respectively.
 */
@Target(AnnotationTarget.FUNCTION, AnnotationTarget.CLASS)
@Retention(AnnotationRetention.RUNTIME)
annotation class DetectLeaks
