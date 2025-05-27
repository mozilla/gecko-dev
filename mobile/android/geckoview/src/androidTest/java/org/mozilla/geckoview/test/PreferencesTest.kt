/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import junit.framework.TestCase.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.gecko.EventDispatcher.QueryException
import org.mozilla.geckoview.GeckoPreferenceController
import org.mozilla.geckoview.GeckoPreferenceController.GeckoPreference
import org.mozilla.geckoview.GeckoPreferenceController.PREF_BRANCH_DEFAULT
import org.mozilla.geckoview.GeckoPreferenceController.PREF_BRANCH_USER
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
    fun multiPrefObservationRegistrationAndDeregistration() {
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
                .registerPreferences(listOf(intPref, stringPref, floatPref, boolPref, unknownPref)),
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

        sessionRule.waitForResult(
            GeckoPreferenceController.Observer
                .unregisterPreferences(listOf(intPref, stringPref, floatPref, boolPref, unknownPref)),
        )

        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                intPref to 4,
                stringPref to "#111111",
                floatPref to "2.2",
                boolPref to true,
                unknownPref to "hello-world-2",
            ),
        )
        assertEquals(
            "Unregistered successfully, subsequent pref changes didn't trigger onGeckoPreferenceChange: $timesCalled",
            5,
            timesCalled,
        )
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

    /**
     * Checks if getting from the user branch behaves as expected.
     */
    @Test
    fun gettingUserGeckoPreference() {
        // Arbitrary preferences selected from StaticPrefList.yaml
        val intPref = "dom.user_activation.transient.timeout"
        val intExpected = sessionRule.getPrefs(intPref)[0]
        val intActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(intPref))
        assertEquals("Getting int worked as expected.", intExpected, intActual.value)
        assertEquals("Default value is as expected.", intExpected, intActual.defaultValue)
        assertEquals("User value is as expected.", null, intActual.userValue)
        assertFalse("User and default value have not diverged.", intActual.hasUserChangedValue)
        assertEquals("Correct name for int.", intPref, intActual.pref)
        assertEquals("Correct type for int.", PREF_TYPE_INT, intActual.type)

        val stringPref = "editor.background_color"
        val stringExpected = sessionRule.getPrefs(stringPref)[0]
        val stringActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(stringPref))
        assertEquals("Getting string worked as expected.", stringExpected, stringActual.value)
        assertEquals("Default value is as expected.", stringExpected, stringActual.defaultValue)
        assertEquals("User value is as expected.", null, stringActual.userValue)
        assertFalse("User and default value have not diverged.", stringActual.hasUserChangedValue)
        assertEquals("Correct name for string.", stringPref, stringActual.pref)
        assertEquals("Correct type for string.", PREF_TYPE_STRING, stringActual.type)

        val floatPref = "dom.media.silence_duration_for_audibility"
        val floatExpected = sessionRule.getPrefs(floatPref)[0]
        val floatActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(floatPref))
        assertEquals("Getting float worked as expected.", floatExpected, floatActual.value)
        assertEquals("Default value is as expected.", floatExpected, floatActual.defaultValue)
        assertEquals("User value is as expected.", null, floatActual.userValue)
        assertFalse("User and default value have not diverged.", floatActual.hasUserChangedValue)
        assertEquals("Correct name for float.", floatPref, floatActual.pref)
        assertEquals("Correct type for float.", PREF_TYPE_STRING, floatActual.type)

        val boolPref = "dom.allow_cut_copy"
        val boolExpected = sessionRule.getPrefs(boolPref)[0]
        val boolActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(boolPref))
        assertEquals("Getting bool worked as expected.", boolExpected, boolActual.value)
        assertEquals("Default value is as expected.", boolExpected, boolActual.defaultValue)
        assertEquals("User value is as expected.", null, boolActual.userValue)
        assertFalse("User and default value have not diverged.", boolActual.hasUserChangedValue)
        assertEquals("Correct name for bool.", boolPref, boolActual.pref)
        assertEquals("Correct type for bool.", PREF_TYPE_BOOL, boolActual.type)

        val unknownPref = "pref.unknown.does.not.exist"
        val unknownActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(unknownPref))
        assertEquals("Getting unknown pref worked as expected.", null, unknownActual.value)
        assertEquals("Default value is as expected.", null, unknownActual.defaultValue)
        assertEquals("User value is as expected.", null, unknownActual.userValue)
        assertFalse("User and default value have not diverged.", unknownActual.hasUserChangedValue)
        assertEquals("Correct name for unknown pref.", unknownPref, unknownActual.pref)
        assertEquals("Correct type for unknown pref.", PREF_TYPE_INVALID, unknownActual.type)
    }

    /**
     * Checks if getting from the default branch behaves as expected.
     */
    @Test
    fun gettingDefaultGeckoPreference() {
        // Arbitrary preferences selected from StaticPrefList.yaml
        // Initial values are presumed actual defaults (no existing junit test API to get defaults directly)
        val intPref = "dom.popup_maximum"
        val intInitial = sessionRule.getPrefs(intPref)[0] as Int
        val intSet = intInitial + 1

        val stringPref = "editor.background_color"
        val stringInitial = sessionRule.getPrefs(stringPref)[0] as String
        val stringSet = stringInitial + "A"

        val floatPref = "dom.media.silence_duration_for_audibility"
        val floatInitial = sessionRule.getPrefs(floatPref)[0] as String
        val floatSet = floatInitial + "1"

        val boolPref = "dom.allow_cut_copy"
        val boolInitial = sessionRule.getPrefs(boolPref)[0] as Boolean
        val boolSet = !boolInitial

        // Change away from the default
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                intPref to intSet,
                stringPref to stringSet,
                floatPref to floatSet,
                boolPref to boolSet,
            ),
        )

        // Confirm user prefs set
        val valuesSet = sessionRule.getPrefs(intPref, stringPref, floatPref, boolPref)
        assertEquals("Int user pref set as expected", intSet, valuesSet[0])
        assertEquals("String user pref set as expected", stringSet, valuesSet[1])
        assertEquals("Float user pref set as expected", floatSet, valuesSet[2])
        assertEquals("Bool user pref set as expected", boolSet, valuesSet[3])

        // Check default prefs
        val intActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(intPref))
        assertEquals("Getting int worked as expected.", intSet, intActual.value)
        assertEquals("Default value is as expected.", intInitial, intActual.defaultValue)
        assertEquals("User value is as expected.", intSet, intActual.userValue)
        assertTrue("User and default value have diverged.", intActual.hasUserChangedValue)
        assertEquals("Correct name for int.", intPref, intActual.pref)
        assertEquals("Correct type for int.", PREF_TYPE_INT, intActual.type)

        val stringActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(stringPref))
        assertEquals("Getting string worked as expected.", stringSet, stringActual.value)
        assertEquals("Default value is as expected.", stringInitial, stringActual.defaultValue)
        assertEquals("User value is as expected.", stringSet, stringActual.userValue)
        assertTrue("User and default value have diverged.", stringActual.hasUserChangedValue)
        assertEquals("Correct name for string.", stringPref, stringActual.pref)
        assertEquals("Correct type for string.", PREF_TYPE_STRING, stringActual.type)

        val floatActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(floatPref))
        assertEquals("Getting float worked as expected.", floatSet, floatActual.value)
        assertEquals("Default value is as expected.", floatInitial, floatActual.defaultValue)
        assertEquals("User value is as expected.", floatSet, floatActual.userValue)
        assertTrue("User and default value have diverged.", floatActual.hasUserChangedValue)
        assertEquals("Correct name for float.", floatPref, floatActual.pref)
        assertEquals("Correct type for float.", PREF_TYPE_STRING, floatActual.type)

        val boolActual = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(boolPref))

        assertEquals("Getting bool worked as expected.", boolSet, boolActual.value)
        assertEquals("Default value is as expected.", boolInitial, boolActual.defaultValue)
        assertEquals("User value is as expected.", boolSet, boolActual.userValue)
        assertTrue("User and default value have diverged.", boolActual.hasUserChangedValue)
        assertEquals("Correct name for bool.", boolPref, boolActual.pref)
        assertEquals("Correct type for bool.", PREF_TYPE_BOOL, boolActual.type)
    }

    /**
     * Checks if setting from the user branch behaves as expected.
     */
    @Test
    fun settingUserGeckoPreference() {
        val branch = PREF_BRANCH_USER
        // Arbitrary preferences selected from StaticPrefList.yaml
        // Note: The setting tests are specifically different from the rest in this file to prevent test harness interference
        // during concurrent runs.
        val intPref = "dom.fullscreen.force_exit_on_multiple_escape_interval"
        val stringPref = "browser.active_color"
        val floatPref = "dom.vr.controller_trigger_threshold"
        val boolPref = "dom.animations.offscreen-throttling"
        val unknownPref = "pref.unknown.does.not.exist.v2"

        val intInitial = sessionRule.getPrefs(intPref)[0] as Int
        val stringInitial = sessionRule.getPrefs(stringPref)[0] as String
        val floatInitial = sessionRule.getPrefs(floatPref)[0] as String
        val boolInitial = sessionRule.getPrefs(boolPref)[0] as Boolean

        val intSet = intInitial + 1
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(intPref, intSet, branch))

        val stringSet = stringInitial + "A"
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(stringPref, stringSet, branch))

        val floatSet = floatInitial + "1"
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(floatPref, floatSet, branch))

        val boolSet = !boolInitial
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(boolPref, boolSet, branch))

        val unknownSet = "hello-world"
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(unknownPref, unknownSet, branch))

        val intExpected = sessionRule.getPrefs(intPref)[0] as Int
        assertEquals("Setting int set as expected.", intSet, intExpected)

        val stringExpected = sessionRule.getPrefs(stringPref)[0] as String
        assertEquals("Setting string set as expected.", stringSet, stringExpected)

        val floatExpected = sessionRule.getPrefs(floatPref)[0] as String
        assertEquals("Setting float set as expected.", floatSet, floatExpected)

        val boolExpected = sessionRule.getPrefs(boolPref)[0] as Boolean
        assertEquals("Setting bool set as expected.", boolSet, boolExpected)

        val unknownExpected = sessionRule.getPrefs(unknownPref)[0] as String
        assertEquals("Setting bool set as expected.", unknownSet, unknownExpected)
    }

    /**
     * Checks if setting from the default branch behaves as expected.
     */
    @Test
    fun settingDefaultGeckoPreference() {
        val branch = PREF_BRANCH_DEFAULT
        // Arbitrary preferences selected from StaticPrefList.yaml
        // Note: The setting tests are specifically different from the rest in this file to prevent test harness interference
        // during concurrent runs.
        val intPref = "dom.innerSize.rounding"
        val stringPref = "browser.active_color.dark"
        val floatPref = "general.smoothScroll.currentVelocityWeighting"
        val boolPref = "dom.animations.commit-styles-endpoint-inclusive"

        val intInitial = sessionRule.getPrefs(intPref)[0] as Int
        val stringInitial = sessionRule.getPrefs(stringPref)[0] as String
        val floatInitial = sessionRule.getPrefs(floatPref)[0] as String
        val boolInitial = sessionRule.getPrefs(boolPref)[0] as Boolean

        val intDefaultSet = intInitial + 1
        val stringDefaultSet = stringInitial + "A"
        val floatDefaultSet = floatInitial + "1"
        val boolDefaultSet = !boolInitial

        // Set new defaults
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(intPref, intDefaultSet, branch))
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(stringPref, stringDefaultSet, branch))
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(floatPref, floatDefaultSet, branch))
        sessionRule.waitForResult(GeckoPreferenceController.setGeckoPref(boolPref, boolDefaultSet, branch))

        // Check setting occurred both on default and user branches
        val intDefault = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(intPref))
        val intUser = sessionRule.getPrefs(intPref)[0] as Int
        assertEquals("Default int set as expected.", intDefaultSet, intDefault.defaultValue)
        assertEquals("User int set as expected to the default.", intDefaultSet, intUser)

        val stringDefault = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(stringPref))
        val stringUser = sessionRule.getPrefs(stringPref)[0] as String
        assertEquals("Default string set as expected.", stringDefaultSet, stringDefault.defaultValue)
        assertEquals("User string set as expected to the default.", stringDefaultSet, stringUser)

        val floatDefault = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(floatPref))
        val floatUser = sessionRule.getPrefs(floatPref)[0] as String
        assertEquals("Default float set as expected.", floatDefaultSet, floatDefault.defaultValue)
        assertEquals("User float set as expected to the default.", floatDefaultSet, floatUser)

        val boolDefault = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(boolPref))
        val boolUser = sessionRule.getPrefs(boolPref)[0] as Boolean
        assertEquals("Default bool set as expected.", boolDefaultSet, boolDefault.defaultValue)
        assertEquals("User bool set as expected to the default.", boolDefaultSet, boolUser)

        // Change the user pref, but not the default
        val intSetUserPost = intDefaultSet + 1
        val stringSetUserPost = stringDefaultSet + "A"
        val floatSetUserPost = floatDefaultSet + "1"
        val boolSetUserPost = !boolDefaultSet
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                intPref to intSetUserPost,
                stringPref to stringSetUserPost,
                floatPref to floatSetUserPost,
                boolPref to boolSetUserPost,
            ),
        )

        // Confirm default remained unchanged
        val intDefaultPostChange = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(intPref))
        val intUserPost = sessionRule.getPrefs(intPref)[0] as Int
        assertEquals("Default int set as expected.", intDefaultSet, intDefaultPostChange.defaultValue)
        assertEquals("User int set as expected.", intSetUserPost, intUserPost)

        val stringDefaultPostChange = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(stringPref))
        val stringUserPost = sessionRule.getPrefs(stringPref)[0] as String
        assertEquals("Default string set as expected.", stringDefaultSet, stringDefaultPostChange.defaultValue)
        assertEquals("User string set as expected.", stringSetUserPost, stringUserPost)

        val floatDefaultPostChange = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(floatPref))
        val floatUserPost = sessionRule.getPrefs(floatPref)[0] as String
        assertEquals("Default float set as expected.", floatDefaultSet, floatDefaultPostChange.defaultValue)
        assertEquals("User float set as expected.", floatSetUserPost, floatUserPost)

        val boolDefaultPostChange = sessionRule.waitForResult(GeckoPreferenceController.getGeckoPref(boolPref))
        val boolUserPost = sessionRule.getPrefs(boolPref)[0] as Boolean
        assertEquals("Default bool set as expected.", boolDefaultSet, boolDefaultPostChange.defaultValue)
        assertEquals("User bool set as expected.", boolSetUserPost, boolUserPost)
    }

    /**
     * Checks setting using the wrong API behaves as expected.
     */
    @Test
    fun settingUserGeckoPreferenceWrongAPI() {
        val intPref = "dom.navigation.navigationRateLimit.timespan"
        val intInitial = sessionRule.getPrefs(intPref)[0]
        val intSetUserPost = intInitial as Int + 1

        // Setting incorrectly as String when it is an Int pref
        try {
            sessionRule.waitForResult(
                GeckoPreferenceController.setGeckoPref(
                    intPref,
                    intSetUserPost.toString(),
                    PREF_BRANCH_USER,
                ),
            )
            fail("Should not complete requests on a pref of a different type.")
        } catch (e: RuntimeException) {
            val cause = e.cause as QueryException
            assertEquals(
                "Correctly could not set preference.",
                "There was an issue with the preference.",
                cause.data,
            )
        }
        val result =
            sessionRule.waitForResult(
                GeckoPreferenceController.getGeckoPref(
                    intPref,
                ),
            )

        // It'll retrieve the original registered value
        assertEquals("Pref name matches as expected.", intPref, result.pref)
        assertEquals("Pref type matches as expected.", PREF_TYPE_INT, result.type)
        assertEquals("Pref value matches as expected.", intInitial, result.value)
    }

    /**
     * Basic test of clearing a user pref.
     */
    @Test
    fun clearUserPref() {
        val arbitraryPref = "some.arbitrary.pref.test"
        val arbitraryPrefValue = "hello-world"
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                arbitraryPref to arbitraryPrefValue,
            ),
        )
        val initiallyExists =
            sessionRule.waitForResult(
                GeckoPreferenceController.getGeckoPref(
                    arbitraryPref,
                ),
            )
        assertEquals("Pref exists as expected.", arbitraryPref, initiallyExists.pref)
        assertEquals("Pref value is as expected.", arbitraryPrefValue, initiallyExists.value)
        assertEquals("Pref type is as expected.", PREF_TYPE_STRING, initiallyExists.type)

        sessionRule.waitForResult(GeckoPreferenceController.clearGeckoUserPref(arbitraryPref))

        val postClearing =
            sessionRule.waitForResult(
                GeckoPreferenceController.getGeckoPref(
                    arbitraryPref,
                ),
            )
        assertEquals("Pref name after clearing is as expected.", arbitraryPref, postClearing.pref)
        assertEquals("Pref value after clearing is null as expected.", null, postClearing.value)
        assertEquals("Pref type after clearing is as expected.", PREF_TYPE_INVALID, postClearing.type)
    }
}
