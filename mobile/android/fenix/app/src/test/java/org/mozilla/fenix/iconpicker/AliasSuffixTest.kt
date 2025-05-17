/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.iconpicker

import junit.framework.TestCase.assertEquals
import org.junit.Test

class AliasSuffixTest {

    @Test
    fun `GIVEN a valid alias suffix WHEN fromString is invoked THEN matching enum is returned`() {
        val input = "AppGradientSunset"

        val result = AliasSuffix.fromString(input)

        assertEquals(AliasSuffix.AppGradientSunset, result)
    }

    @Test
    fun `GIVEN an unknown alias suffix WHEN fromString is called THEN AppDefault is returned`() {
        val input = "NonExistentAlias"

        val result = AliasSuffix.fromString(input)

        assertEquals(AliasSuffix.AppDefault, result)
    }

    @Test
    fun `GIVEN all enum values WHEN fromString is called for each THEN the same enum is returned`() {
        for (value in AliasSuffix.entries) {
            assertEquals(
                value,
                AliasSuffix.fromString(value.suffix),
            )
        }
    }
}
