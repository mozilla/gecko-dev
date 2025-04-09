package mozilla.components.feature.search.icons

import mozilla.appservices.remotesettings.Attachment
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.`when`

class SearchConfigIconsParserTest {

    private lateinit var parser: SearchConfigIconsParser
    private lateinit var mockRecord: RemoteSettingsRecord
    private lateinit var mockFields: JSONObject
    private lateinit var mockAttachment: Attachment

    @Before
    fun setUp() {
        parser = SearchConfigIconsParser()
        mockRecord = mock(RemoteSettingsRecord::class.java)
        mockFields = mock(JSONObject::class.java)
        mockAttachment = mock(Attachment::class.java)

        `when`(mockRecord.fields).thenReturn(mockFields)
        `when`(mockRecord.attachment).thenReturn(null)
    }

    @Test
    fun `Given record with all fields and attachment When parseRecord is called Then valid model is returned`() {
        val mockJsonArray = mock(JSONArray::class.java)
        `when`(mockJsonArray.length()).thenReturn(2)
        `when`(mockJsonArray.get(0)).thenReturn("google")
        `when`(mockJsonArray.get(1)).thenReturn("bing")

        `when`(mockFields.getLong("schema")).thenReturn(1L)
        `when`(mockFields.getInt("imageSize")).thenReturn(64)
        `when`(mockFields.getJSONArray("engineIdentifiers")).thenReturn(mockJsonArray)
        `when`(mockFields.optString("filter_expression")).thenReturn("test-filter")

        `when`(mockAttachment.filename).thenReturn("icon.png")
        `when`(mockAttachment.mimetype).thenReturn("image/png")
        `when`(mockAttachment.location).thenReturn("location/path")
        `when`(mockAttachment.hash).thenReturn("abc123hash")
        `when`(mockAttachment.size).thenReturn(1024u)

        `when`(mockRecord.attachment).thenReturn(mockAttachment)

        val result = parser.parseRecord(mockRecord)

        assertNotNull(result)
        assertEquals(1L, result!!.schema)
        assertEquals(64, result.imageSize)
        assertEquals(listOf("google", "bing"), result.engineIdentifier)
        assertEquals("test-filter", result.filterExpression)

        assertNotNull(result.attachment)
        assertEquals("icon.png", result.attachment!!.filename)
        assertEquals("image/png", result.attachment!!.mimetype)
        assertEquals("location/path", result.attachment!!.location)
        assertEquals("abc123hash", result.attachment!!.hash)
        assertEquals(1024u, result.attachment!!.size)
    }

    @Test
    fun `Given record with missing optional fields When parseRecord is called Then valid model with null attachment is returned`() {
        val mockJsonArray = mock(JSONArray::class.java)
        `when`(mockJsonArray.length()).thenReturn(1)
        `when`(mockJsonArray.get(0)).thenReturn("duckduckgo")

        `when`(mockFields.getLong("schema")).thenReturn(2L)
        `when`(mockFields.getInt("imageSize")).thenReturn(32)
        `when`(mockFields.getJSONArray("engineIdentifiers")).thenReturn(mockJsonArray)
        `when`(mockFields.optString("filter_expression")).thenReturn("")

        val result = parser.parseRecord(mockRecord)

        assertNotNull(result)
        assertEquals(2L, result!!.schema)
        assertEquals(32, result.imageSize)
        assertEquals(listOf("duckduckgo"), result.engineIdentifier)
        assertEquals("", result.filterExpression)
        assertNull(result.attachment)
    }

    @Test
    fun `Given record that causes JSONException during field parsing When parseRecord is called Then null is returned`() {
        `when`(mockFields.getLong("schema")).thenThrow(JSONException("Test exception on schema"))

        val result = parser.parseRecord(mockRecord)

        assertNull(result)
    }
}
