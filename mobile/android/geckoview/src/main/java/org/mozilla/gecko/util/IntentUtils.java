/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Locale;

/** Utilities for Intents. */
public class IntentUtils {
  private static final String LOGTAG = "IntentUtils";
  private static final boolean DEBUG = false;

  private static final String EXTERNAL_STORAGE_PROVIDER_AUTHORITY =
      "com.android.externalstorage.documents";
  private static final int DOCURI_MAX_DEPTH = 5;

  private IntentUtils() {}

  /**
   * Return a Uri instance which is equivalent to uri, but with a guaranteed-lowercase scheme as if
   * the API level 16 method Uri.normalizeScheme had been called.
   *
   * @param uri The URI string to normalize.
   * @return The corresponding normalized Uri.
   */
  private static Uri normalizeUriScheme(final Uri uri) {
    final String scheme = uri.getScheme();
    if (scheme == null) {
      return uri;
    }
    final String lower = scheme.toLowerCase(Locale.ROOT);
    if (lower.equals(scheme)) {
      return uri;
    }

    // Otherwise, return a new URI with a normalized scheme.
    return uri.buildUpon().scheme(lower).build();
  }

  /**
   * Return a normalized Uri instance that corresponds to the given URI string with cross-API-level
   * compatibility.
   *
   * @param aUri The URI string to normalize.
   * @return The corresponding normalized Uri.
   */
  public static Uri normalizeUri(final String aUri) {
    return normalizeUriScheme(
        aUri.indexOf(':') >= 0 ? Uri.parse(aUri) : new Uri.Builder().scheme(aUri).build());
  }

  public static boolean isUriSafeForScheme(final String aUri) {
    return isUriSafeForScheme(normalizeUri(aUri));
  }

  /**
   * Verify whether the given URI is considered safe to load in respect to its scheme. Unsafe URIs
   * should be blocked from further handling.
   *
   * @param aUri The URI instance to test.
   * @return Whether the provided URI is considered safe in respect to its scheme.
   */
  public static boolean isUriSafeForScheme(final Uri aUri) {
    final String scheme = aUri.getScheme();
    if ("tel".equals(scheme) || "sms".equals(scheme)) {
      // Bug 794034 - We don't want to pass MWI or USSD codes to the
      // dialer, and ensure the Uri class doesn't parse a URI
      // containing a fragment ('#')
      final String number = aUri.getSchemeSpecificPart();
      if (number.contains("#") || number.contains("*") || aUri.getFragment() != null) {
        return false;
      }
    }

    if (("intent".equals(scheme) || "android-app".equals(scheme))) {
      // Bug 1356893 - Rject intents with file data schemes.
      return getSafeIntent(aUri) != null;
    }

    if ("fido".equals(scheme)) {
      return false;
    }

    return true;
  }

  /**
   * Create a safe intent for the given URI. Intents with file data schemes are considered unsafe.
   *
   * @param aUri The URI for the intent.
   * @return A safe intent for the given URI or null if URI is considered unsafe.
   */
  public static Intent getSafeIntent(final Uri aUri) {
    final Intent intent;
    try {
      intent = Intent.parseUri(aUri.toString(), 0);
    } catch (final URISyntaxException e) {
      return null;
    }

    final Uri data = intent.getData();
    if (data != null && "file".equals(normalizeUriScheme(data).getScheme())) {
      return null;
    }

    // Only open applications which can accept arbitrary data from a browser.
    intent.addCategory(Intent.CATEGORY_BROWSABLE);

    // Prevent site from explicitly opening our internal activities,
    // which can leak data.
    intent.setComponent(null);
    nullIntentSelector(intent);

    return intent;
  }

  // We create a separate method to better encapsulate the @TargetApi use.
  private static void nullIntentSelector(final Intent intent) {
    intent.setSelector(null);
  }

  /**
   * Return a local path from the Uri that is content schema.
   *
   * @param context The context.
   * @param uri The URI.
   * @return A local path if resolved. If this cannot resolve URI, return null.
   */
  public static String resolveContentUri(final Context context, final Uri uri) {
    final ContentResolver cr = context.getContentResolver();
    try (final Cursor cur =
        cr.query(
            uri, new String[] {"_data"}, /* selection */ null, /* args */ null, /* sort */ null)) {
      final int idx = cur.getColumnIndex("_data");
      if (idx < 0 || !cur.moveToFirst()) {
        return null;
      }
      do {
        try {
          final String path = cur.getString(idx);
          if (path != null && !path.isEmpty()) {
            return path;
          }
        } catch (final Exception e) {
        }
      } while (cur.moveToNext());
    } catch (final UnsupportedOperationException e) {
      Log.e(LOGTAG, "Failed to query child documents", e);
    }

    if (DEBUG) {
      Log.e(LOGTAG, "Failed to resolve uri. uri=" + uri.toString());
    }
    return null;
  }

  /**
   * Return a local path from tree Uri.
   *
   * @param context The context.
   * @param uri The uri that @{link DoumentContract#isTreeUri} returns true.
   * @return A local path if resolved. If this cannot resolve URI, return null.
   */
  public static String resolveTreeUri(final Context context, final Uri uri) {
    final Uri docDirUri =
        DocumentsContract.buildDocumentUriUsingTree(uri, DocumentsContract.getTreeDocumentId(uri));
    return resolveDocumentUri(context, docDirUri);
  }

  /**
   * Return a local path from document Uri.
   *
   * @param context The context.
   * @param uri The uri that @{link DoumentContract#isDocumentUri} returns true.
   * @return A local path if resolved. If this cannot resolve URI, return null.
   */
  public static String resolveDocumentUri(final Context context, final Uri uri) {
    if (EXTERNAL_STORAGE_PROVIDER_AUTHORITY.equals(uri.getAuthority())) {
      final String docId = DocumentsContract.getDocumentId(uri);
      final String[] split = docId.split(":");

      if (split[0].equals("primary")) {
        // This is the internal storage.
        final StringBuilder sb =
            new StringBuilder(Environment.getExternalStorageDirectory().toString());
        if (split.length > 1) {
          sb.append("/").append(split[1]);
        }
        return sb.toString();
      }

      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        // This might be sd card. /storage/xxxx-xxxx/...
        final StringBuilder sb = new StringBuilder(Environment.getStorageDirectory().toString());
        sb.append("/").append(split[0]);
        if (split.length > 1) {
          sb.append("/").append(split[1]);
        }
        return sb.toString();
      }
    }
    if (DEBUG) {
      Log.e(LOGTAG, "Failed to resolve uri. uri=" + uri.toString());
    }
    return null;
  }

  /** The Information of content by document URI. */
  public static class ContentMetaData {
    /** Constructor of content information from document URI. */
    /* packages */ ContentMetaData(
        @Nullable final String filePath,
        @NonNull final Uri uri,
        @NonNull final String displayName,
        @NonNull final String mimeType,
        final long lastModified) {
      if (filePath == null) {
        this.filePath = "";
      } else {
        this.filePath = filePath;
      }
      this.uri = uri;
      this.displayName = displayName;
      this.mimeType = mimeType;
      this.lastModified = lastModified;
    }

    /** Serializer for JavaScript. */
    public @NonNull GeckoBundle toGeckoBundle() {
      final GeckoBundle bundle = new GeckoBundle();

      bundle.putString("filePath", this.filePath);
      bundle.putString("uri", this.uri.toString());
      bundle.putString("name", this.displayName);
      bundle.putString("type", this.mimeType);
      bundle.putLong("lastModified", this.lastModified);

      return bundle;
    }

    @Override
    public String toString() {
      final StringBuilder sb = new StringBuilder();
      if (this.filePath != "") {
        sb.append("filePath=").append(this.filePath).append(", ");
      }
      sb.append("uri=")
          .append(this.uri)
          .append(", displayName=")
          .append(this.displayName)
          .append(", mimeType=")
          .append(this.mimeType)
          .append(", lastModified=")
          .append(this.lastModified);
      return sb.toString();
    }

    /** Local file path if resolved. If not resolved, empty string. */
    public @NonNull final String filePath;

    /** document URI. */
    public @NonNull final Uri uri;

    /** Display name in document tree. */
    public @NonNull final String displayName;

    /** MIME type. */
    public @NonNull final String mimeType;

    /** Last modified time. */
    public final long lastModified;
  }

  private static void queryTreeDocumentUri(
      final Context context,
      final Uri uri,
      final int currentDepth,
      final ArrayList<ContentMetaData> children) {
    if (currentDepth > DOCURI_MAX_DEPTH) {
      // We don't allow deep directory depth due to memory concern etc.
      Log.e(LOGTAG, "Failed to query child documents due to deep depth");
      return;
    }

    final ContentResolver cr = context.getContentResolver();
    final String[] columns =
        new String[] {
          DocumentsContract.Document.COLUMN_DOCUMENT_ID,
          DocumentsContract.Document.COLUMN_DISPLAY_NAME,
          DocumentsContract.Document.COLUMN_MIME_TYPE,
          DocumentsContract.Document.COLUMN_LAST_MODIFIED,
        };
    try (Cursor cursor =
        cr.query(uri, columns, /* selection */ null, /* args */ null, /* sort */ null)) {
      while (cursor.moveToNext()) {
        if (cursor.isNull(0)) {
          continue;
        }

        final String docId = cursor.getString(0);
        final String mimeType = cursor.isNull(2) ? "" : cursor.getString(2);
        final boolean isDirectory = DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType);
        if (isDirectory) {
          final Uri childUri = DocumentsContract.buildChildDocumentsUriUsingTree(uri, docId);
          queryTreeDocumentUri(context, childUri, currentDepth + 1, children);
          continue;
        }

        final Uri docUri = DocumentsContract.buildDocumentUriUsingTree(uri, docId);
        final String displayName = cursor.isNull(1) ? "" : cursor.getString(1);
        final long lastModified = cursor.isNull(3) ? 0 : cursor.getLong(3);

        final String filePath = resolveDocumentUri(context, docUri);
        children.add(new ContentMetaData(filePath, docUri, displayName, mimeType, lastModified));
      }
    } catch (final UnsupportedOperationException e) {
      Log.e(LOGTAG, "Failed to query child documents", e);
    }
  }

  /**
   * Returns list of content meta data into the given tree URI
   *
   * @param context A context
   * @param uri A tree URI
   * @return The list of content meta data
   */
  public static @NonNull ArrayList<ContentMetaData> traverseTreeUri(
      final Context context, final Uri uri) {
    final Uri queryUri =
        DocumentsContract.buildChildDocumentsUriUsingTree(
            uri, DocumentsContract.getTreeDocumentId(uri));
    final ArrayList<ContentMetaData> children = new ArrayList<ContentMetaData>();
    queryTreeDocumentUri(context, queryUri, 0, children);
    if (DEBUG) {
      for (final ContentMetaData data : children) {
        Log.d(LOGTAG, data.toString());
      }
    }
    return children;
  }
}
