/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdio.h>
#include <jni.h>
#include <android/log.h>
#include "dlfcn.h"
#include "APKOpen.h"
#include "ElfLoader.h"
#include "SQLiteBridge.h"

#ifdef DEBUG
#define LOG(x...) __android_log_print(ANDROID_LOG_INFO, "GeckoJNI", x)
#else
#define LOG(x...)
#endif

#define SQLITE_WRAPPER_INT(name) name ## _t f_ ## name;

SQLITE_WRAPPER_INT(sqlite3_open)
SQLITE_WRAPPER_INT(sqlite3_errmsg)
SQLITE_WRAPPER_INT(sqlite3_prepare_v2)
SQLITE_WRAPPER_INT(sqlite3_bind_parameter_count)
SQLITE_WRAPPER_INT(sqlite3_bind_null)
SQLITE_WRAPPER_INT(sqlite3_bind_text)
SQLITE_WRAPPER_INT(sqlite3_step)
SQLITE_WRAPPER_INT(sqlite3_column_count)
SQLITE_WRAPPER_INT(sqlite3_finalize)
SQLITE_WRAPPER_INT(sqlite3_close)
SQLITE_WRAPPER_INT(sqlite3_column_name)
SQLITE_WRAPPER_INT(sqlite3_column_type)
SQLITE_WRAPPER_INT(sqlite3_column_blob)
SQLITE_WRAPPER_INT(sqlite3_column_bytes)
SQLITE_WRAPPER_INT(sqlite3_column_text)
SQLITE_WRAPPER_INT(sqlite3_changes)
SQLITE_WRAPPER_INT(sqlite3_last_insert_rowid)

void setup_sqlite_functions(void *sqlite_handle)
{
#define GETFUNC(name) f_ ## name = (name ## _t) (uintptr_t) __wrap_dlsym(sqlite_handle, #name)
  GETFUNC(sqlite3_open);
  GETFUNC(sqlite3_errmsg);
  GETFUNC(sqlite3_prepare_v2);
  GETFUNC(sqlite3_bind_parameter_count);
  GETFUNC(sqlite3_bind_null);
  GETFUNC(sqlite3_bind_text);
  GETFUNC(sqlite3_step);
  GETFUNC(sqlite3_column_count);
  GETFUNC(sqlite3_finalize);
  GETFUNC(sqlite3_close);
  GETFUNC(sqlite3_column_name);
  GETFUNC(sqlite3_column_type);
  GETFUNC(sqlite3_column_blob);
  GETFUNC(sqlite3_column_bytes);
  GETFUNC(sqlite3_column_text);
  GETFUNC(sqlite3_changes);
  GETFUNC(sqlite3_last_insert_rowid);
#undef GETFUNC
}

static bool initialized = false;
static jclass stringClass;
static jclass objectClass;
static jclass byteBufferClass;
static jclass cursorClass;
static jmethodID jByteBufferAllocateDirect;
static jmethodID jCursorConstructor;
static jmethodID jCursorAddRow;

static jobject sqliteInternalCall(JNIEnv* jenv, sqlite3 *db, jstring jQuery,
                                  jobjectArray jParams, jlongArray jQueryRes);

static void throwSqliteException(JNIEnv* jenv, const char* aFormat, ...)
{
    va_list ap;
    va_start(ap, aFormat);
    char* msg = nullptr;
    vasprintf(&msg, aFormat, ap);
    LOG("Error in SQLiteBridge: %s\n", msg);
    JNI_Throw(jenv, "org/mozilla/gecko/sqlite/SQLiteBridgeException", msg);
    free(msg);
    va_end(ap);
}

static void
JNI_Setup(JNIEnv* jenv)
{
    if (initialized) return;

    jclass lObjectClass       = jenv->FindClass("java/lang/Object");
    jclass lStringClass       = jenv->FindClass("java/lang/String");
    jclass lByteBufferClass   = jenv->FindClass("java/nio/ByteBuffer");
    jclass lCursorClass       = jenv->FindClass("org/mozilla/gecko/sqlite/MatrixBlobCursor");

    if (lStringClass == nullptr
        || lObjectClass == nullptr
        || lByteBufferClass == nullptr
        || lCursorClass == nullptr) {
        throwSqliteException(jenv, "FindClass error");
        return;
    }

    // Those are only local references. Make them global so they work
    // across calls and threads.
    objectClass = (jclass)jenv->NewGlobalRef(lObjectClass);
    stringClass = (jclass)jenv->NewGlobalRef(lStringClass);
    byteBufferClass = (jclass)jenv->NewGlobalRef(lByteBufferClass);
    cursorClass = (jclass)jenv->NewGlobalRef(lCursorClass);

    if (stringClass == nullptr || objectClass == nullptr
        || byteBufferClass == nullptr
        || cursorClass == nullptr) {
        throwSqliteException(jenv, "NewGlobalRef error");
        return;
    }

    // public static ByteBuffer allocateDirect(int capacity)
    jByteBufferAllocateDirect =
        jenv->GetStaticMethodID(byteBufferClass, "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
    // new MatrixBlobCursor(String [])
    jCursorConstructor =
        jenv->GetMethodID(cursorClass, "<init>", "([Ljava/lang/String;)V");
    // public void addRow (Object[] columnValues)
    jCursorAddRow =
        jenv->GetMethodID(cursorClass, "addRow", "([Ljava/lang/Object;)V");

    if (jByteBufferAllocateDirect == nullptr
        || jCursorConstructor == nullptr
        || jCursorAddRow == nullptr) {
        throwSqliteException(jenv, "GetMethodId error");
        return;
    }

    initialized = true;
}

extern "C" NS_EXPORT jobject MOZ_JNICALL
Java_org_mozilla_gecko_sqlite_SQLiteBridge_sqliteCall(JNIEnv* jenv, jclass,
                                                      jstring jDb,
                                                      jstring jQuery,
                                                      jobjectArray jParams,
                                                      jlongArray jQueryRes)
{
    JNI_Setup(jenv);

    int rc;
    jobject jCursor = nullptr;
    const char* dbPath;
    sqlite3 *db;

    dbPath = jenv->GetStringUTFChars(jDb, nullptr);
    rc = f_sqlite3_open(dbPath, &db);
    jenv->ReleaseStringUTFChars(jDb, dbPath);
    if (rc != SQLITE_OK) {
        throwSqliteException(jenv,
            "Can't open database: %s", f_sqlite3_errmsg(db));
        f_sqlite3_close(db); // close db even if open failed
        return nullptr;
    }
    jCursor = sqliteInternalCall(jenv, db, jQuery, jParams, jQueryRes);
    f_sqlite3_close(db);
    return jCursor;
}

extern "C" NS_EXPORT jobject MOZ_JNICALL
Java_org_mozilla_gecko_sqlite_SQLiteBridge_sqliteCallWithDb(JNIEnv* jenv, jclass,
                                                            jlong jDb,
                                                            jstring jQuery,
                                                            jobjectArray jParams,
                                                            jlongArray jQueryRes)
{
    JNI_Setup(jenv);

    jobject jCursor = nullptr;
    sqlite3 *db = (sqlite3*)jDb;
    jCursor = sqliteInternalCall(jenv, db, jQuery, jParams, jQueryRes);
    return jCursor;
}

extern "C" NS_EXPORT jlong MOZ_JNICALL
Java_org_mozilla_gecko_sqlite_SQLiteBridge_openDatabase(JNIEnv* jenv, jclass,
                                                        jstring jDb)
{
    JNI_Setup(jenv);

    int rc;
    const char* dbPath;
    sqlite3 *db;

    dbPath = jenv->GetStringUTFChars(jDb, nullptr);
    rc = f_sqlite3_open(dbPath, &db);
    jenv->ReleaseStringUTFChars(jDb, dbPath);
    if (rc != SQLITE_OK) {
        throwSqliteException(jenv,
            "Can't open database: %s", f_sqlite3_errmsg(db));
        f_sqlite3_close(db); // close db even if open failed
        return 0;
    }
    return (jlong)db;
}

extern "C" NS_EXPORT void MOZ_JNICALL
Java_org_mozilla_gecko_sqlite_SQLiteBridge_closeDatabase(JNIEnv* jenv, jclass,
                                                        jlong jDb)
{
    JNI_Setup(jenv);

    sqlite3 *db = (sqlite3*)jDb;
    f_sqlite3_close(db);
}

static jobject
sqliteInternalCall(JNIEnv* jenv,
                   sqlite3 *db,
                   jstring jQuery,
                   jobjectArray jParams,
                   jlongArray jQueryRes)
{
    JNI_Setup(jenv);

    jobject jCursor = nullptr;
    jsize numPars = 0;

    const char *pzTail;
    sqlite3_stmt *ppStmt;
    int rc;

    const char* queryStr;
    queryStr = jenv->GetStringUTFChars(jQuery, nullptr);

    rc = f_sqlite3_prepare_v2(db, queryStr, -1, &ppStmt, &pzTail);
    if (rc != SQLITE_OK || ppStmt == nullptr) {
        throwSqliteException(jenv,
            "Can't prepare statement: %s", f_sqlite3_errmsg(db));
        return nullptr;
    }
    jenv->ReleaseStringUTFChars(jQuery, queryStr);

    // Check if number of parameters matches
    if (jParams != nullptr) {
        numPars = jenv->GetArrayLength(jParams);
    }
    int sqlNumPars;
    sqlNumPars = f_sqlite3_bind_parameter_count(ppStmt);
    if (numPars != sqlNumPars) {
        throwSqliteException(jenv,
            "Passed parameter count (%d) "
            "doesn't match SQL parameter count (%d)",
            numPars, sqlNumPars);
        return nullptr;
    }

    if (jParams != nullptr) {
        // Bind parameters, if any
        if (numPars > 0) {
            for (int i = 0; i < numPars; i++) {
                jobject jObjectParam = jenv->GetObjectArrayElement(jParams, i);
                // IsInstanceOf or isAssignableFrom? String is final, so IsInstanceOf
                // should be OK.
                jboolean isString = jenv->IsInstanceOf(jObjectParam, stringClass);
                if (isString != JNI_TRUE) {
                    throwSqliteException(jenv,
                        "Parameter is not of String type");
                    return nullptr;
                }

                // SQLite parameters index from 1.
                if (jObjectParam == nullptr) {
                  rc = f_sqlite3_bind_null(ppStmt, i + 1);
                } else {
                  jstring jStringParam = (jstring) jObjectParam;
                  const char* paramStr = jenv->GetStringUTFChars(jStringParam, nullptr);
                  rc = f_sqlite3_bind_text(ppStmt, i + 1, paramStr, -1, SQLITE_TRANSIENT);
                  jenv->ReleaseStringUTFChars(jStringParam, paramStr);
                }

                if (rc != SQLITE_OK) {
                    throwSqliteException(jenv, "Error binding query parameter");
                    return nullptr;
                }
            }
        }
    }

    // Execute the query and step through the results
    rc = f_sqlite3_step(ppStmt);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        throwSqliteException(jenv,
            "Can't step statement: (%d) %s", rc, f_sqlite3_errmsg(db));
        return nullptr;
    }

    // Get the column count and names
    int cols;
    cols = f_sqlite3_column_count(ppStmt);

    {
        // Allocate a String[cols]
        jobjectArray jStringArray = jenv->NewObjectArray(cols,
                                                         stringClass,
                                                         nullptr);
        if (jStringArray == nullptr) {
            throwSqliteException(jenv, "Can't allocate String[]");
            return nullptr;
        }

        // Assign column names to the String[]
        for (int i = 0; i < cols; i++) {
            const char* colName = f_sqlite3_column_name(ppStmt, i);
            jstring jStr = jenv->NewStringUTF(colName);
            jenv->SetObjectArrayElement(jStringArray, i, jStr);
        }

        // Construct the MatrixCursor(String[]) with given column names
        jCursor = jenv->NewObject(cursorClass,
                                  jCursorConstructor,
                                  jStringArray);
        if (jCursor == nullptr) {
            throwSqliteException(jenv, "Can't allocate MatrixBlobCursor");
            return nullptr;
        }
    }

    // Return the id and number of changed rows in jQueryRes
    {
        jlong id = f_sqlite3_last_insert_rowid(db);
        jenv->SetLongArrayRegion(jQueryRes, 0, 1, &id);

        jlong changed = f_sqlite3_changes(db);
        jenv->SetLongArrayRegion(jQueryRes, 1, 1, &changed);
    }

    // For each row, add an Object[] to the passed ArrayList,
    // with that containing either String or ByteArray objects
    // containing the columns
    while (rc != SQLITE_DONE) {
        // Process row
        // Construct Object[]
        jobjectArray jRow = jenv->NewObjectArray(cols,
                                                 objectClass,
                                                 nullptr);
        if (jRow == nullptr) {
            throwSqliteException(jenv, "Can't allocate jRow Object[]");
            return nullptr;
        }

        for (int i = 0; i < cols; i++) {
            int colType = f_sqlite3_column_type(ppStmt, i);
            if (colType == SQLITE_BLOB) {
                // Treat as blob
                const void* blob = f_sqlite3_column_blob(ppStmt, i);
                int colLen = f_sqlite3_column_bytes(ppStmt, i);

                // Construct ByteBuffer of correct size
                jobject jByteBuffer =
                    jenv->CallStaticObjectMethod(byteBufferClass,
                                                 jByteBufferAllocateDirect,
                                                 colLen);
                if (jByteBuffer == nullptr) {
                    throwSqliteException(jenv,
                        "Failure calling ByteBuffer.allocateDirect");
                    return nullptr;
                }

                // Get its backing array
                void* bufferArray = jenv->GetDirectBufferAddress(jByteBuffer);
                if (bufferArray == nullptr) {
                    throwSqliteException(jenv,
                        "Failure calling GetDirectBufferAddress");
                    return nullptr;
                }
                memcpy(bufferArray, blob, colLen);

                jenv->SetObjectArrayElement(jRow, i, jByteBuffer);
                jenv->DeleteLocalRef(jByteBuffer);
            } else if (colType == SQLITE_NULL) {
                jenv->SetObjectArrayElement(jRow, i, nullptr);
            } else {
                // Treat everything else as text
                const char* txt = (const char*)f_sqlite3_column_text(ppStmt, i);
                jstring jStr = jenv->NewStringUTF(txt);
                jenv->SetObjectArrayElement(jRow, i, jStr);
                jenv->DeleteLocalRef(jStr);
            }
        }

        // Append Object[] to Cursor
        jenv->CallVoidMethod(jCursor, jCursorAddRow, jRow);

        // Clean up
        jenv->DeleteLocalRef(jRow);

        // Get next row
        rc = f_sqlite3_step(ppStmt);
        // Real error?
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            throwSqliteException(jenv,
                "Can't re-step statement:(%d) %s", rc, f_sqlite3_errmsg(db));
            return nullptr;
        }
    }

    rc = f_sqlite3_finalize(ppStmt);
    if (rc != SQLITE_OK) {
        throwSqliteException(jenv,
            "Can't finalize statement: %s", f_sqlite3_errmsg(db));
        return nullptr;
    }

    return jCursor;
}
