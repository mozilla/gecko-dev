/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.content.SharedPreferences
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers.anyFloat
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.Mock
import org.mockito.Mockito.anyString
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations.openMocks

class SharedPreferencesTest {
    @Mock private lateinit var sharedPrefs: SharedPreferences

    @Mock private lateinit var editor: SharedPreferences.Editor

    @Before
    fun setup() {
        openMocks(this)

        `when`(sharedPrefs.edit()).thenReturn(editor)
        `when`(editor.putFloat(anyString(), anyFloat())).thenReturn(editor)
        `when`(editor.putInt(anyString(), anyInt())).thenReturn(editor)
        `when`(editor.putLong(anyString(), anyLong())).thenReturn(editor)
        `when`(editor.putString(anyString(), anyString())).thenReturn(editor)
        `when`(editor.putStringSet(anyString(), any())).thenReturn(editor)
    }

    @Test
    fun `getter returns float from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getFloat(eq("float"), anyFloat())).thenReturn(2.4f)

        assertEquals(2.4f, holder.float)
        verify(sharedPrefs).getFloat("float", 0f)
    }

    @Test
    fun `setter applies float to shared preferences`() {
        val holder = MockPreferencesHolder(defaultFloat = 1f)
        holder.float = 0f

        verify(editor).putFloat("float", 0f)
        verify(editor).apply()
    }

    @Test
    fun `getter uses default float value`() {
        val holderDefault = MockPreferencesHolder(defaultFloat = 0f)
        // Call the getter for the test
        holderDefault.float

        verify(sharedPrefs).getFloat("float", 0f)

        val holderOther = MockPreferencesHolder(defaultFloat = 12f)
        // Call the getter for the test
        holderOther.float

        verify(sharedPrefs).getFloat("float", 12f)
    }

    @Test
    fun `getter returns int from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getInt(eq("int"), anyInt())).thenReturn(5)

        assertEquals(5, holder.int)
        verify(sharedPrefs).getInt("int", 0)
    }

    @Test
    fun `setter applies int to shared preferences`() {
        val holder = MockPreferencesHolder(defaultInt = 1)
        holder.int = 0

        verify(editor).putInt("int", 0)
        verify(editor).apply()
    }

    @Test
    fun `getter uses default int value`() {
        val holderDefault = MockPreferencesHolder(defaultInt = 0)
        // Call the getter for the test
        holderDefault.int

        verify(sharedPrefs).getInt("int", 0)

        val holderOther = MockPreferencesHolder(defaultInt = 23)
        // Call the getter for the test
        holderOther.int

        verify(sharedPrefs).getInt("int", 23)
    }

    @Test
    fun `getter returns long from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getLong(eq("long"), anyLong())).thenReturn(200L)

        assertEquals(200L, holder.long)
        verify(sharedPrefs).getLong("long", 0)
    }

    @Test
    fun `setter applies long to shared preferences`() {
        val holder = MockPreferencesHolder(defaultLong = 1)
        holder.long = 0

        verify(editor).putLong("long", 0)
        verify(editor).apply()
    }

    @Test
    fun `getter uses default long value`() {
        val holderDefault = MockPreferencesHolder(defaultLong = 0)
        // Call the getter for the test
        holderDefault.long

        verify(sharedPrefs).getLong("long", 0)

        val holderOther = MockPreferencesHolder(defaultLong = 23)
        // Call the getter for the test
        holderOther.long

        verify(sharedPrefs).getLong("long", 23)
    }

    @Test
    fun `getter returns string set from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getStringSet(eq("string_set"), any())).thenReturn(setOf("foo"))

        assertEquals(setOf("foo"), holder.stringSet)
        verify(sharedPrefs).getStringSet("string_set", null)
    }

    @Test
    fun `setter applies string set to shared preferences`() {
        val holder = MockPreferencesHolder(defaultString = "foo")
        holder.stringSet = setOf("bar")

        verify(editor).putStringSet("string_set", setOf("bar"))
        verify(editor).apply()
    }

    @Test
    fun `GIVEN default string set WHEN key is missing THEN default is used`() {
        val holderDefault = MockPreferencesHolder()
        // Call the getter for the test
        var result = holderDefault.stringSet

        verify(sharedPrefs).getStringSet("string_set", null)
        assertEquals(emptySet<String>(), result)

        `when`(sharedPrefs.getStringSet("string_set", null)).thenReturn(setOf("hello", "world"))
        val holderOther = MockPreferencesHolder(defaultSet = setOf("hello", "world"))
        // Call the getter for the test
        result = holderOther.stringSet

        verify(sharedPrefs, times(2)).getStringSet("string_set", null)
        assertEquals(setOf("hello", "world"), result)
    }

    @Test
    fun `GIVEN key exists in SharedPreferences WHEN stringPreference is accessed THEN stored value is returned`() {
        `when`(sharedPrefs.contains("string")).thenReturn(true)
        `when`(sharedPrefs.getString("string", null)).thenReturn("hello")

        val holder = MockPreferencesHolder()
        assertEquals("hello", holder.string)

        verify(sharedPrefs).getString("string", null)
    }

    @Test
    fun `GIVEN stringPreference is set WHEN value is assigned THEN value is persisted in SharedPreferences`() {
        val holder = MockPreferencesHolder()
        holder.string = "hello world"

        verify(editor).putString("string", "hello world")
        verify(editor).apply()
    }

    @Test
    fun `GIVEN default string value WHEN key is missing THEN default is used`() {
        `when`(sharedPrefs.contains("string")).thenReturn(false)

        val holder = MockPreferencesHolder(defaultString = "default")
        assertEquals("default", holder.string)
    }

    @Test
    fun `GIVEN key does not exist AND persistDefaultIfNotExists is true WHEN stringPreference is accessed THEN default value is persisted`() {
        `when`(sharedPrefs.contains("persist_string")).thenReturn(false)

        val holder = MockPreferencesHolder(defaultString = "persist_me")
        assertEquals("persist_me", holder.persistString)

        verify(editor).putString("persist_string", "persist_me")
        verify(editor).apply()
    }

    @Test
    fun `GIVEN intPreference THEN default is not initialized until access`() {
        var initialized = false

        class MockPreferencesHolder : PreferencesHolder {
            override val preferences = sharedPrefs
            var int by intPreference(
                "int",
                default = {
                    initialized = true
                     0
                },
            )
        }

        val holder = MockPreferencesHolder()

        // default is not accessed
        assertEquals(false, initialized)

        holder.int
        assertEquals(true, initialized)
    }

    @Test
    fun `GIVEN floatPreference THEN default is not initialized until access`() {
        var initialized = false

        class MockPreferencesHolder : PreferencesHolder {
            override val preferences = sharedPrefs
            var float by floatPreference(
                "float",
                default = {
                    initialized = true
                    0f
                },
            )
        }

        val holder = MockPreferencesHolder()

        // default is not accessed
        assertEquals(false, initialized)

        holder.float
        assertEquals(true, initialized)
    }

    @Test
    fun `GIVEN longPreference THEN default is not initialized until access`() {
        var initialized = false

        class MockPreferencesHolder : PreferencesHolder {
            override val preferences = sharedPrefs
            var long by longPreference(
                "long",
                default = {
                    initialized = true
                    0L
                },
            )
        }

        val holder = MockPreferencesHolder()

        // default is not accessed
        assertEquals(false, initialized)

        holder.long
        assertEquals(true, initialized)
    }

    @Test
    fun `GIVEN stringPreference THEN default is not initialized until access`() {
        var initialized = false

        class MockPreferencesHolder : PreferencesHolder {
            override val preferences = sharedPrefs
            var string by stringPreference(
                "string",
                default = {
                    initialized = true
                    ""
                },
            )
        }

        val holder = MockPreferencesHolder()

        // default is not accessed
        assertEquals(false, initialized)

        holder.string
        assertEquals(true, initialized)
    }

    @Test
    fun `GIVEN stringSetPreference THEN default is not initialized until access`() {
        var initialized = false
        `when`(sharedPrefs.getStringSet(eq("string_set"), any())).thenReturn(null)

        class MockPreferencesHolder : PreferencesHolder {
            override val preferences = sharedPrefs
            var stringSet by stringSetPreference(
                "string_set",
                default = {
                    initialized = true
                    emptySet()
                },
            )
        }

        val holder = MockPreferencesHolder()

        // default is not accessed
        assertEquals(false, initialized)

        holder.stringSet
        assertEquals(true, initialized)
    }

    private inner class MockPreferencesHolder(
        defaultFloat: Float = 0f,
        defaultInt: Int = 0,
        defaultLong: Long = 0L,
        defaultString: String = "",
        defaultSet: Set<String> = emptySet(),
    ) : PreferencesHolder {
        override val preferences = sharedPrefs

        var float by floatPreference("float", default = defaultFloat)

        var int by intPreference("int", default = defaultInt)

        var long by longPreference("long", default = defaultLong)

        var string by stringPreference("string", default = defaultString)

        var persistString by stringPreference("persist_string", default = defaultString, persistDefaultIfNotExists = true)

        var stringSet by stringSetPreference("string_set", default = defaultSet)
    }
}
