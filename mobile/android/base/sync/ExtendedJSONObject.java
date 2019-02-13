/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.UnexpectedJSONException.BadRequiredFieldJSONException;

/**
 * Extend JSONObject to do little things, like, y'know, accessing members.
 *
 * @author rnewman
 *
 */
public class ExtendedJSONObject {

  public JSONObject object;

  /**
   * Return a <code>JSONParser</code> instance for immediate use.
   * <p>
   * <code>JSONParser</code> is not thread-safe, so we return a new instance
   * each call. This is extremely inefficient in execution time and especially
   * memory use -- each instance allocates a 16kb temporary buffer -- and we
   * hope to improve matters eventually.
   */
  protected static JSONParser getJSONParser() {
    return new JSONParser();
  }

  /**
   * Parse a JSON encoded string.
   *
   * @param in <code>Reader</code> over a JSON-encoded input to parse; not
   *            necessarily a JSON object.
   * @return a regular Java <code>Object</code>.
   * @throws ParseException
   * @throws IOException
   */
  protected static Object parseRaw(Reader in) throws ParseException, IOException {
    try {
      return getJSONParser().parse(in);
    } catch (Error e) {
      // Don't be stupid, org.json.simple. Bug 1042929.
      throw new ParseException(ParseException.ERROR_UNEXPECTED_EXCEPTION);
    }
  }

  /**
   * Parse a JSON encoded string.
   * <p>
   * You should prefer the streaming interface {@link #parseRaw(Reader)}.
   *
   * @param input JSON-encoded input string to parse; not necessarily a JSON object.
   * @return a regular Java <code>Object</code>.
   * @throws ParseException
   */
  protected static Object parseRaw(String input) throws ParseException {
    try {
      return getJSONParser().parse(input);
    } catch (Error e) {
      // Don't be stupid, org.json.simple. Bug 1042929.
      throw new ParseException(ParseException.ERROR_UNEXPECTED_EXCEPTION);
    }
  }

  /**
   * Helper method to get a JSON array from a stream.
   *
   * @param in <code>Reader</code> over a JSON-encoded array to parse.
   * @throws ParseException
   * @throws IOException
   * @throws NonArrayJSONException if the object is valid JSON, but not an array.
   */
  public static JSONArray parseJSONArray(Reader in)
      throws IOException, ParseException, NonArrayJSONException {
    Object o = parseRaw(in);

    if (o == null) {
      return null;
    }

    if (o instanceof JSONArray) {
      return (JSONArray) o;
    }

    throw new NonArrayJSONException("value must be a JSON array");
  }

  /**
   * Helper method to get a JSON array from a string.
   * <p>
   * You should prefer the stream interface {@link #parseJSONArray(Reader)}.
   *
   * @param jsonString input.
   * @throws ParseException
   * @throws IOException
   * @throws NonArrayJSONException if the object is valid JSON, but not an array.
   */
  public static JSONArray parseJSONArray(String jsonString)
      throws IOException, ParseException, NonArrayJSONException {
    Object o = parseRaw(jsonString);

    if (o == null) {
      return null;
    }

    if (o instanceof JSONArray) {
      return (JSONArray) o;
    }

    throw new NonArrayJSONException("value must be a JSON array");
  }

  /**
   * Helper method to get a JSON object from a stream.
   *
   * @param in input {@link Reader}.
   * @throws ParseException
   * @throws IOException
   * @throws NonArrayJSONException if the object is valid JSON, but not an object.
   */
  public static ExtendedJSONObject parseJSONObject(Reader in)
      throws IOException, ParseException, NonObjectJSONException {
    return new ExtendedJSONObject(in);
  }

  /**
   * Helper method to get a JSON object from a string.
   * <p>
   * You should prefer the stream interface {@link #parseJSONObject(Reader)}.
   *
   * @param jsonString input.
   * @throws ParseException
   * @throws IOException
   * @throws NonObjectJSONException if the object is valid JSON, but not an object.
   */
  public static ExtendedJSONObject parseJSONObject(String jsonString)
      throws IOException, ParseException, NonObjectJSONException {
    return new ExtendedJSONObject(jsonString);
  }

  /**
   * Helper method to get a JSON object from a UTF-8 byte array.
   *
   * @param in UTF-8 bytes.
   * @throws ParseException
   * @throws NonObjectJSONException if the object is valid JSON, but not an object.
   * @throws IOException
   */
  public static ExtendedJSONObject parseUTF8AsJSONObject(byte[] in)
      throws ParseException, NonObjectJSONException, IOException {
    return parseJSONObject(new String(in, "UTF-8"));
  }

  public ExtendedJSONObject() {
    this.object = new JSONObject();
  }

  public ExtendedJSONObject(JSONObject o) {
    this.object = o;
  }

  public ExtendedJSONObject deepCopy() {
    final ExtendedJSONObject out = new ExtendedJSONObject();
    @SuppressWarnings("unchecked")
    final Set<Map.Entry<String, Object>> entries = this.object.entrySet();
    for (Map.Entry<String, Object> entry : entries) {
      final String key = entry.getKey();
      final Object value = entry.getValue();
      if (value instanceof JSONArray) {
        // Oh god.
        try {
          out.put(key, new JSONParser().parse(((JSONArray) value).toJSONString()));
        } catch (ParseException e) {
          // This should never occur, because we're round-tripping.
        }
        continue;
      }
      if (value instanceof JSONObject) {
        out.put(key, new ExtendedJSONObject((JSONObject) value).deepCopy().object);
        continue;
      }
      if (value instanceof ExtendedJSONObject) {
        out.put(key, ((ExtendedJSONObject) value).deepCopy());
        continue;
      }
      // Oh well.
      out.put(key, value);
    }

    return out;
  }

  public ExtendedJSONObject(Reader in) throws IOException, ParseException, NonObjectJSONException {
    if (in == null) {
      this.object = new JSONObject();
      return;
    }

    Object obj = parseRaw(in);
    if (obj instanceof JSONObject) {
      this.object = ((JSONObject) obj);
    } else {
      throw new NonObjectJSONException("value must be a JSON object");
    }
  }

  public ExtendedJSONObject(String jsonString) throws IOException, ParseException, NonObjectJSONException {
    this(jsonString == null ? null : new StringReader(jsonString));
  }

  @Override
  public ExtendedJSONObject clone() {
    return new ExtendedJSONObject((JSONObject) this.object.clone());
  }

  // Passthrough methods.
  public Object get(String key) {
    return this.object.get(key);
  }

  public long getLong(String key, long def) {
    if (!object.containsKey(key)) {
      return def;
    }

    Long val = getLong(key);
    if (val == null) {
      return def;
    }
    return val.longValue();
  }

  public Long getLong(String key) {
    return (Long) this.get(key);
  }

  public String getString(String key) {
    return (String) this.get(key);
  }

  public Boolean getBoolean(String key) {
    return (Boolean) this.get(key);
  }

  /**
   * Return an Integer if the value for this key is an Integer, Long, or String
   * that can be parsed as a base 10 Integer.
   * Passes through null.
   *
   * @throws NumberFormatException
   */
  public Integer getIntegerSafely(String key) throws NumberFormatException {
    Object val = this.object.get(key);
    if (val == null) {
      return null;
    }
    if (val instanceof Integer) {
      return (Integer) val;
    }
    if (val instanceof Long) {
      return ((Long) val).intValue();
    }
    if (val instanceof String) {
      return Integer.parseInt((String) val, 10);
    }
    throw new NumberFormatException("Expecting Integer, got " + val.getClass());
  }

  /**
   * Return a server timestamp value as milliseconds since epoch.
   *
   * @param key
   * @return A Long, or null if the value is non-numeric or doesn't exist.
   */
  public Long getTimestamp(String key) {
    Object val = this.object.get(key);

    // This is absurd.
    if (val instanceof Double) {
      double millis = ((Double) val) * 1000;
      return Double.valueOf(millis).longValue();
    }
    if (val instanceof Float) {
      double millis = ((Float) val).doubleValue() * 1000;
      return Double.valueOf(millis).longValue();
    }
    if (val instanceof Number) {
      // Must be an integral number.
      return ((Number) val).longValue() * 1000;
    }

    return null;
  }

  public boolean containsKey(String key) {
    return this.object.containsKey(key);
  }

  public String toJSONString() {
    return this.object.toJSONString();
  }

  public String toString() {
    return this.object.toString();
  }

  public void put(String key, Object value) {
    @SuppressWarnings("unchecked")
    Map<Object, Object> map = this.object;
    map.put(key, value);
  }

  @SuppressWarnings({ "unchecked", "rawtypes" })
  public void putAll(Map map) {
    this.object.putAll(map);
  }

  /**
   * Remove key-value pair from JSONObject.
   *
   * @param key
   *          to be removed.
   * @return true if key exists and was removed, false otherwise.
   */
  public boolean remove(String key) {
    Object res = this.object.remove(key);
    return (res != null);
  }

  public ExtendedJSONObject getObject(String key) throws NonObjectJSONException {
    Object o = this.object.get(key);
    if (o == null) {
      return null;
    }
    if (o instanceof ExtendedJSONObject) {
      return (ExtendedJSONObject) o;
    }
    if (o instanceof JSONObject) {
      return new ExtendedJSONObject((JSONObject) o);
    }
    throw new NonObjectJSONException("value must be a JSON object for key: " + key);
  }

  @SuppressWarnings("unchecked")
  public Set<Entry<String, Object>> entrySet() {
    return this.object.entrySet();
  }

  @SuppressWarnings("unchecked")
  public Set<String> keySet() {
    return this.object.keySet();
  }

  public org.json.simple.JSONArray getArray(String key) throws NonArrayJSONException {
    Object o = this.object.get(key);
    if (o == null) {
      return null;
    }
    if (o instanceof JSONArray) {
      return (JSONArray) o;
    }
    throw new NonArrayJSONException("key must be a JSON array: " + key);
  }

  public int size() {
    return this.object.size();
  }

  @Override
  public int hashCode() {
    if (this.object == null) {
      return getClass().hashCode();
    }
    return this.object.hashCode() ^ getClass().hashCode();
  }

  @Override
  public boolean equals(Object o) {
    if (!(o instanceof ExtendedJSONObject)) {
      return false;
    }
    if (o == this) {
      return true;
    }
    ExtendedJSONObject other = (ExtendedJSONObject) o;
    if (this.object == null) {
      return other.object == null;
    }
    return this.object.equals(other.object);
  }

  /**
   * Throw if keys are missing or values have wrong types.
   *
   * @param requiredFields list of required keys.
   * @param requiredFieldClass class that values must be coercable to; may be null, which means don't check.
   * @throws UnexpectedJSONException
   */
  public void throwIfFieldsMissingOrMisTyped(String[] requiredFields, Class<?> requiredFieldClass) throws BadRequiredFieldJSONException {
    // Defensive as possible: verify object has expected key(s) with string value.
    for (String k : requiredFields) {
      Object value = get(k);
      if (value == null) {
        throw new BadRequiredFieldJSONException("Expected key not present in result: " + k);
      }
      if (requiredFieldClass != null && !(requiredFieldClass.isInstance(value))) {
        throw new BadRequiredFieldJSONException("Value for key not an instance of " + requiredFieldClass + ": " + k);
      }
    }
  }

  /**
   * Return a base64-encoded string value as a byte array.
   */
  public byte[] getByteArrayBase64(String key) {
    String s = (String) this.object.get(key);
    if (s == null) {
      return null;
    }
    return Base64.decodeBase64(s);
  }

  /**
   * Return a hex-encoded string value as a byte array.
   */
  public byte[] getByteArrayHex(String key) {
    String s = (String) this.object.get(key);
    if (s == null) {
      return null;
    }
    return Utils.hex2Byte(s);
  }
}
