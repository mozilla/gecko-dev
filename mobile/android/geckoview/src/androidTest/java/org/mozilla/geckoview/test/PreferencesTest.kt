/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoPreferenceController
import org.mozilla.geckoview.GeckoPreferenceController.GeckoPreference
import org.mozilla.geckoview.GeckoPreferenceController.PREF_TYPE_BOOL
import org.mozilla.geckoview.GeckoPreferenceController.PREF_TYPE_INT
import org.mozilla.geckoview.GeckoPreferenceController.PREF_TYPE_INVALID
import org.mozilla.geckoview.GeckoPreferenceController.PREF_TYPE_STRING

@RunWith(AndroidJUnit4::class)
@MediumTest
class PreferencesTest : BaseSessionTest() {
    /**
     * Checking if delegate getter and setter behave as expected.
     */
    @Test
    fun settingPreferenceDelegate() {
        class ExamplePrefDelegate : GeckoPreferenceController.Observer.Delegate {
            override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
            }
        }
        val delegate = ExamplePrefDelegate()
        sessionRule.runtime.preferencesObserverDelegate = delegate
        assertEquals(
            "The delegate was set as expected on the runtime.",
            delegate,
            sessionRule.runtime.preferencesObserverDelegate,
        )
    }

    /**
     * Basic observer delegate test to check registration on ints.
     */
    @Test
    fun intPrefObservationTest() {
        // Arbitrary int preference selected from StaticPrefList.yaml
        val intPref = "dom.popup_maximum"
        val changeValue = 3
        var timesCalled = 0

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    assertEquals(
                        "Registered and observed preference name should always match.",
                        intPref,
                        observedGeckoPreference.pref,
                    )
                    assertEquals("Observation requested should match.", intPref, observedGeckoPreference.pref)
                    assertEquals("Changed value matches.", changeValue, observedGeckoPreference.userValue)
                    assertEquals("Type is as expected for observed.", PREF_TYPE_INT, observedGeckoPreference.type)
                    timesCalled++
                }
            },
        )

        sessionRule.waitForResult(
            GeckoPreferenceController.Observer.registerPreference(intPref),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                intPref to changeValue,
            ),
        )
        assertEquals("Called onGeckoPreferenceChange the expected times: $timesCalled", 1, timesCalled)
    }

    /**
     * Basic observer delegate test to check registration on strings.
     */
    @Test
    fun stringPrefObservationTest() {
        // Arbitrary string preference selected from StaticPrefList.yaml
        val stringPref = "editor.background_color"
        val changeValue = "#000000"
        var timesCalled = 0

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    assertEquals(
                        "Registered and observed preference name should always match.",
                        stringPref,
                        observedGeckoPreference.pref,
                    )
                    assertEquals("Observation requested should match.", stringPref, observedGeckoPreference.pref)
                    assertEquals("Changed value matches.", changeValue, observedGeckoPreference.userValue)
                    assertEquals("Type is as expected for observed.", PREF_TYPE_STRING, observedGeckoPreference.type)
                    timesCalled++
                }
            },
        )
        sessionRule.waitForResult(
            GeckoPreferenceController.Observer.registerPreference(stringPref),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                stringPref to changeValue,
            ),
        )
        assertEquals("Called onGeckoPreferenceChange the expected times: $timesCalled", 1, timesCalled)
    }

    /**
     * Basic observer delegate test to check registration on floats.
     */
    @Test
    fun floatPrefObservationTest() {
        // Arbitrary float preference selected from StaticPrefList.yaml
        val floatPref = "dom.media.silence_duration_for_audibility"
        // Floats are treated as strings in Gecko
        val changeValue = "2.1"
        var timesCalled = 0

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    assertEquals(
                        "Registered and observed preference name should always match.",
                        floatPref,
                        observedGeckoPreference.pref,
                    )
                    assertEquals("Observation requested should match.", floatPref, observedGeckoPreference.pref)
                    assertEquals("Changed value matches.", changeValue, observedGeckoPreference.userValue)

                    // Floats are strings in pref world
                    assertEquals("Type is as expected for observed.", PREF_TYPE_STRING, observedGeckoPreference.type)
                    timesCalled++
                }
            },
        )
        sessionRule.waitForResult(
            GeckoPreferenceController.Observer.registerPreference(floatPref),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                floatPref to changeValue,
            ),
        )
        assertEquals("Called onGeckoPreferenceChange the expected times: $timesCalled", 1, timesCalled)
    }

    /**
     * Basic observer delegate test to check registration on bools.
     */
    @Test
    fun boolPrefObservationTest() {
        // Arbitrary boolean preference selected from StaticPrefList.yaml
        val boolPref = "dom.allow_cut_copy"
        val changeValue = false
        var timesCalled = 0

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    assertEquals(
                        "Registered and observed preference name should always match.",
                        boolPref,
                        observedGeckoPreference.pref,
                    )
                    assertEquals("Observation requested should match.", boolPref, observedGeckoPreference.pref)
                    assertEquals("Changed value matches.", changeValue, observedGeckoPreference.userValue)
                    assertEquals("Type is as expected for observed.", PREF_TYPE_BOOL, observedGeckoPreference.type)
                    timesCalled++
                }
            },
        )

        sessionRule.waitForResult(
            GeckoPreferenceController.Observer.registerPreference(boolPref),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                boolPref to changeValue,
            ),
        )
        assertEquals("Called onGeckoPreferenceChange the expected times: $timesCalled", timesCalled, 1)
    }

    /**
     * Checking all pref types and multiple observations. Using pref examples defined in StaticPrefList.yaml.
     */
    @Test
    fun multiPrefObservation() {
        var timesCalled = 0

        // Arbitrarily selected based on pref type
        val intPref = "dom.popup_maximum"
        val stringPref = "editor.background_color"
        // Floats are a string according to static prefs file "Note that float prefs are stored internally as strings."
        val floatPref = "dom.media.silence_duration_for_audibility"
        val boolPref = "dom.allow_cut_copy"
        val unknownPref = "pref.unknown.does.not.exist"

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    timesCalled++
                    when (observedGeckoPreference.type) {
                        PREF_TYPE_INT -> assertEquals("Int observation requested should match.", intPref, observedGeckoPreference.pref)
                        PREF_TYPE_STRING -> {
                            when (observedGeckoPreference.pref) {
                                stringPref ->
                                    assertEquals(
                                        "String observation requested should match.",
                                        stringPref,
                                        observedGeckoPreference.pref,
                                    )

                                floatPref ->
                                    assertEquals(
                                        "Float observation requested should match.",
                                        floatPref,
                                        observedGeckoPreference.pref,
                                    )

                                unknownPref -> {
                                    assertEquals(
                                        "Unknown observation requested should match.",
                                        unknownPref,
                                        observedGeckoPreference.pref,
                                    )
                                    assertEquals(
                                        "Unknown value matches as expected.",
                                        "hello-world",
                                        observedGeckoPreference.value,
                                    )
                                }
                                else -> fail("An unexpected type returned.")
                            }
                        }

                        PREF_TYPE_BOOL -> assertEquals("Bool observation requested should match.", boolPref, observedGeckoPreference.pref)
                        else -> fail("An invalid type returned.")
                    }
                }
            },
        )
        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .registerPreference(intPref),
        )
        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .registerPreference(stringPref),
        )
        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .registerPreference(floatPref),
        )
        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .registerPreference(boolPref),
        )
        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .registerPreference(unknownPref),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                intPref to 3,
                stringPref to "#000000",
                floatPref to "2.1",
                boolPref to false,
                unknownPref to "hello-world",
            ),
        )
        assertEquals("Called onGeckoPreferenceChange the expected times: $timesCalled", 5, timesCalled)
    }

    /**
     * Checking singular deregistration mechanisms.
     */
    @Test
    fun unregisterPrefFromObservation() {
        // Arbitrary int preference selected from StaticPrefList.yaml
        val intPref = "dom.popup_maximum"
        var timesCalled = 0

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    assertEquals(
                        "Registered and observed preference name should always match.",
                        intPref,
                        observedGeckoPreference.pref,
                    )
                    assertEquals("Observation requested should match.", intPref, observedGeckoPreference.pref)
                    timesCalled++
                }
            },
        )

        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .registerPreference(intPref),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                intPref to 1,
            ),
        )
        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .unregisterPreference(intPref),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                intPref to 4,
            ),
        )
        assertEquals("Called onGeckoPreferenceChange the expected times: $timesCalled", 1, timesCalled)
    }

    /**
     * Tests what happens in an observation when a pref is removed.
     */
    @Test
    fun observationWhenPrefIsRemoved() {
        val arbitraryPref = "arbitrary.test-only.pref"
        var timesCalled = 0

        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                arbitraryPref to "hello-world",
            ),
        )

        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .registerPreference(arbitraryPref),
        )

        // Removing the pref, so it'll no longer be valid
        sessionRule.clearUserPref(arbitraryPref)

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    timesCalled++
                    assertEquals("Pref matches as expected.", arbitraryPref, observedGeckoPreference.pref)
                    assertEquals("Invalid type matches as expected.", PREF_TYPE_INVALID, observedGeckoPreference.type)
                    assertEquals("Value is as expected.", null, observedGeckoPreference.value)
                    assertEquals("User value is as expected.", null, observedGeckoPreference.userValue)
                    assertEquals("Default value is as expected.", null, observedGeckoPreference.defaultValue)
                    assertFalse("Pref hasn't changed.", observedGeckoPreference.hasUserChangedValue)
                }
            },
        )
    }

    /**
     * If a pref "changes" to the same value, it should be a no-op.
     */
    @Test
    fun noObservationOnSameChange() {
        // Arbitrary int preference selected from StaticPrefList.yaml
        val intPref = "dom.popup_maximum"
        val initialValue = sessionRule.getPrefs(intPref)[0]
        var timesCalled = 0

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    timesCalled++
                }
            },
        )

        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .registerPreference(intPref),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                intPref to initialValue,
            ),
        )
        assertEquals("Called onGeckoPreferenceChange the expected times: $timesCalled", 0, timesCalled)
    }

    /**
     * The pref is not real.
     */
    @Test
    fun invalidObservation() {
        var timesCalled = 0

        sessionRule.addExternalDelegateUntilTestEnd(
            GeckoPreferenceController.Observer.Delegate::class,
            sessionRule::setPreferenceDelegate,
            { sessionRule.setPreferenceDelegate(null) },
            object : GeckoPreferenceController.Observer.Delegate {
                override fun onGeckoPreferenceChange(observedGeckoPreference: GeckoPreference<*>) {
                    timesCalled++
                }
            },
        )
        assertEquals(
            "Called onGeckoPreferenceChange the expected times: $timesCalled",
            0,
            timesCalled,
        )
    }
}
