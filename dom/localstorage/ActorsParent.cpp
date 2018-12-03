/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "LocalStorageCommon.h"
#include "LSObject.h"
#include "mozIStorageConnection.h"
#include "mozIStorageFunction.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/PBackgroundLSDatabaseParent.h"
#include "mozilla/dom/PBackgroundLSObserverParent.h"
#include "mozilla/dom/PBackgroundLSRequestParent.h"
#include "mozilla/dom/PBackgroundLSSharedTypes.h"
#include "mozilla/dom/PBackgroundLSSimpleRequestParent.h"
#include "mozilla/dom/PBackgroundLSSnapshotParent.h"
#include "mozilla/dom/StorageDBUpdater.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsRefPtrHashtable.h"
#include "ReportInternalError.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) \
  do {                             \
  } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

#if defined(MOZ_WIDGET_ANDROID)
#define LS_MOBILE
#endif

namespace mozilla {
namespace dom {

using namespace mozilla::dom::quota;
using namespace mozilla::dom::StorageUtils;
using namespace mozilla::ipc;

namespace {

struct ArchivedOriginInfo;
class ArchivedOriginScope;
class Connection;
class ConnectionThread;
class Database;
class PrepareDatastoreOp;
class PreparedDatastore;
class QuotaClient;
class Snapshot;

typedef nsClassHashtable<nsCStringHashKey, ArchivedOriginInfo>
    ArchivedOriginHashtable;

/*******************************************************************************
 * Constants
 ******************************************************************************/

// Major schema version. Bump for almost everything.
const uint32_t kMajorSchemaVersion = 1;

// Minor schema version. Should almost always be 0 (maybe bump on release
// branches if we have to).
const uint32_t kMinorSchemaVersion = 0;

// The schema version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 4 bits so the max value is
// 0xFFFFFFF. The minor version occupies the lower 4 bits and its max is 0xF.
static_assert(kMajorSchemaVersion <= 0xFFFFFFF,
              "Major version needs to fit in 28 bits.");
static_assert(kMinorSchemaVersion <= 0xF,
              "Minor version needs to fit in 4 bits.");

const int32_t kSQLiteSchemaVersion =
    int32_t((kMajorSchemaVersion << 4) + kMinorSchemaVersion);

// Changing the value here will override the page size of new databases only.
// A journal mode change and VACUUM are needed to change existing databases, so
// the best way to do that is to use the schema version upgrade mechanism.
const uint32_t kSQLitePageSizeOverride =
#ifdef LS_MOBILE
    512;
#else
    1024;
#endif

static_assert(kSQLitePageSizeOverride == /* mozStorage default */ 0 ||
                  (kSQLitePageSizeOverride % 2 == 0 &&
                   kSQLitePageSizeOverride >= 512 &&
                   kSQLitePageSizeOverride <= 65536),
              "Must be 0 (disabled) or a power of 2 between 512 and 65536!");

// Set to some multiple of the page size to grow the database in larger chunks.
const uint32_t kSQLiteGrowthIncrement = kSQLitePageSizeOverride * 2;

static_assert(kSQLiteGrowthIncrement >= 0 &&
                  kSQLiteGrowthIncrement % kSQLitePageSizeOverride == 0 &&
                  kSQLiteGrowthIncrement < uint32_t(INT32_MAX),
              "Must be 0 (disabled) or a positive multiple of the page size!");

/**
 * The database name for LocalStorage data in a per-origin directory.
 */
#define DATA_FILE_NAME "data.sqlite"
/**
 * The journal corresponding to DATA_FILE_NAME.  (We don't use WAL mode.)
 */
#define JOURNAL_FILE_NAME "data.sqlite-journal"

/**
 * How long between the first moment we know we have data to be written on a
 * `Connection` and when we should actually perform the write.  This helps
 * limit disk churn under silly usage patterns and is historically consistent
 * with the previous, legacy implementation.
 *
 * Note that flushing happens downstream of Snapshot checkpointing and its
 * batch mechanism which helps avoid wasteful IPC in the case of silly content
 * code.
 */
const uint32_t kFlushTimeoutMs = 5000;

const char kPrivateBrowsingObserverTopic[] = "last-pb-context-exited";

const uint32_t kDefaultOriginLimitKB = 5 * 1024;
const uint32_t kDefaultShadowWrites = true;
const uint32_t kDefaultSnapshotPrefill = 4096;
/**
 * LocalStorage data limit as determined by summing up the lengths of all string
 * keys and values.  This is consistent with the legacy implementation and other
 * browser engines.  This value should really only ever change in unit testing
 * where being able to lower it makes it easier for us to test certain edge
 * cases.
 */
const char kDefaultQuotaPref[] = "dom.storage.default_quota";
/**
 * Should all mutations also be reflected in the "shadow" database, which is
 * the legacy webappsstore.sqlite database.  When this is enabled, users can
 * downgrade their version of Firefox and/or otherwise fall back to the legacy
 * implementation without loss of data.  (Older versions of Firefox will
 * recognize the presence of ls-archive.sqlite and purge it and the other
 * LocalStorage directories so privacy is maintained.)
 */
const char kShadowWritesPref[] = "dom.storage.shadow_writes";
/**
 * Byte budget for sending data down to the LSSnapshot instance when it is first
 * created.  If there is less data than this (measured by tallying the string
 * length of the keys and values), all data is sent, otherwise partial data is
 * sent.  See `Snapshot`.
 */
const char kSnapshotPrefillPref[] = "dom.storage.snapshot_prefill";

/**
 * The amount of time a PreparedDatastore instance should stick around after a
 * preload is triggered in order to give time for the page to use LocalStorage
 * without triggering worst-case synchronous jank.
 */
const uint32_t kPreparedDatastoreTimeoutMs = 20000;

/**
 * Cold storage for LocalStorage data extracted from webappsstore.sqlite at
 * LSNG first-run that has not yet been migrated to its own per-origin directory
 * by use.
 *
 * In other words, at first run, LSNG copies the contents of webappsstore.sqlite
 * into this database.  As requests are made for that LocalStorage data, the
 * contents are removed from this database and placed into per-origin QM
 * storage.  So the contents of this database are always old, unused
 * LocalStorage data that we can potentially get rid of at some point in the
 * future.
 */
#define LS_ARCHIVE_FILE_NAME "ls-archive.sqlite"
/**
 * The legacy LocalStorage database.  Its contents are maintained as our
 * "shadow" database so that LSNG can be disabled without loss of user data.
 */
#define WEB_APPS_STORE_FILE_NAME "webappsstore.sqlite"

// Shadow database Write Ahead Log's maximum size is 512KB
const uint32_t kShadowMaxWALSize = 512 * 1024;

const uint32_t kShadowJournalSizeLimit = kShadowMaxWALSize * 3;

bool IsOnConnectionThread();

void AssertIsOnConnectionThread();

/*******************************************************************************
 * SQLite functions
 ******************************************************************************/

#if 0
int32_t
MakeSchemaVersion(uint32_t aMajorSchemaVersion,
                  uint32_t aMinorSchemaVersion)
{
  return int32_t((aMajorSchemaVersion << 4) + aMinorSchemaVersion);
}
#endif

nsCString GetArchivedOriginHashKey(const nsACString& aOriginSuffix,
                                   const nsACString& aOriginNoSuffix) {
  return aOriginSuffix + NS_LITERAL_CSTRING(":") + aOriginNoSuffix;
}

nsresult CreateTables(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // Table `database`
  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE database"
                         "( origin TEXT NOT NULL"
                         ", last_vacuum_time INTEGER NOT NULL DEFAULT 0"
                         ", last_analyze_time INTEGER NOT NULL DEFAULT 0"
                         ", last_vacuum_size INTEGER NOT NULL DEFAULT 0"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `data`
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE data"
                         "( key TEXT PRIMARY KEY"
                         ", value TEXT NOT NULL"
                         ", compressed INTEGER NOT NULL DEFAULT 0"
                         ", lastAccessTime INTEGER NOT NULL DEFAULT 0"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(kSQLiteSchemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

#if 0
nsresult
UpgradeSchemaFrom1_0To2_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(2, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}
#endif

nsresult SetDefaultPragmas(mozIStorageConnection* aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("PRAGMA synchronous = FULL;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifndef LS_MOBILE
  if (kSQLiteGrowthIncrement) {
    // This is just an optimization so ignore the failure if the disk is
    // currently too full.
    rv =
        aConnection->SetGrowthIncrement(kSQLiteGrowthIncrement, EmptyCString());
    if (rv != NS_ERROR_FILE_TOO_BIG && NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#endif  // LS_MOBILE

  return NS_OK;
}

nsresult CreateStorageConnection(nsIFile* aDBFile, const nsACString& aOrigin,
                                 mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDBFile);
  MOZ_ASSERT(aConnection);

  nsresult rv;

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabase(aDBFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Nuke the database file.
    rv = aDBFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenDatabase(aDBFile, getter_AddRefs(connection));
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetDefaultPragmas(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Check to make sure that the database schema is correct.
  int32_t schemaVersion;
  rv = connection->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (schemaVersion > kSQLiteSchemaVersion) {
    LS_WARNING("Unable to open LocalStorage database, schema is too high!");
    return NS_ERROR_FAILURE;
  }

  if (schemaVersion != kSQLiteSchemaVersion) {
    const bool newDatabase = !schemaVersion;

    if (newDatabase) {
      // Set the page size first.
      if (kSQLitePageSizeOverride) {
        rv = connection->ExecuteSimpleSQL(nsPrintfCString(
            "PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // We have to set the auto_vacuum mode before opening a transaction.
      rv = connection->ExecuteSimpleSQL(
#ifdef LS_MOBILE
          // Turn on full auto_vacuum mode to reclaim disk space on mobile
          // devices (at the cost of some COMMIT speed).
          NS_LITERAL_CSTRING("PRAGMA auto_vacuum = FULL;")
#else
          // Turn on incremental auto_vacuum mode on desktop builds.
          NS_LITERAL_CSTRING("PRAGMA auto_vacuum = INCREMENTAL;")
#endif
      );
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    mozStorageTransaction transaction(
        connection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (newDatabase) {
      rv = CreateTables(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)));
      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);

      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = connection->CreateStatement(
          NS_LITERAL_CSTRING("INSERT INTO database (origin) "
                             "VALUES (:origin)"),
          getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("origin"), aOrigin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // This logic needs to change next time we change the schema!
      static_assert(kSQLiteSchemaVersion == int32_t((1 << 4) + 0),
                    "Upgrade function needed due to schema version increase.");

      while (schemaVersion != kSQLiteSchemaVersion) {
#if 0
        if (schemaVersion == MakeSchemaVersion(1, 0)) {
          rv = UpgradeSchemaFrom1_0To2_0(connection);
        } else {
#endif
        LS_WARNING(
            "Unable to open LocalStorage database, no upgrade path is "
            "available!");
        return NS_ERROR_FAILURE;
#if 0
        }
#endif

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = connection->GetSchemaVersion(&schemaVersion);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);
    }

    rv = transaction.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (newDatabase) {
      // Windows caches the file size, let's force it to stat the file again.
      bool dummy;
      rv = aDBFile->Exists(&dummy);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      int64_t fileSize;
      rv = aDBFile->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(fileSize > 0);

      PRTime vacuumTime = PR_Now();
      MOZ_ASSERT(vacuumTime);

      nsCOMPtr<mozIStorageStatement> vacuumTimeStmt;
      rv = connection->CreateStatement(
          NS_LITERAL_CSTRING("UPDATE database "
                             "SET last_vacuum_time = :time"
                             ", last_vacuum_size = :size;"),
          getter_AddRefs(vacuumTimeStmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = vacuumTimeStmt->BindInt64ByName(NS_LITERAL_CSTRING("time"),
                                           vacuumTime);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv =
          vacuumTimeStmt->BindInt64ByName(NS_LITERAL_CSTRING("size"), fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = vacuumTimeStmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult GetStorageConnection(const nsAString& aDatabaseFilePath,
                              mozIStorageConnection** aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(StringEndsWith(aDatabaseFilePath, NS_LITERAL_STRING(".sqlite")));
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> databaseFile;
  nsresult rv =
      NS_NewLocalFile(aDatabaseFilePath, false, getter_AddRefs(databaseFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = databaseFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!exists)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabase(databaseFile, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetDefaultPragmas(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult GetArchiveFile(const nsAString& aStoragePath, nsIFile** aArchiveFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aStoragePath.IsEmpty());
  MOZ_ASSERT(aArchiveFile);

  nsCOMPtr<nsIFile> archiveFile;
  nsresult rv =
      NS_NewLocalFile(aStoragePath, false, getter_AddRefs(archiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = archiveFile->Append(NS_LITERAL_STRING(LS_ARCHIVE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  archiveFile.forget(aArchiveFile);
  return NS_OK;
}

nsresult CreateArchiveStorageConnection(const nsAString& aStoragePath,
                                        mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aStoragePath.IsEmpty());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> archiveFile;
  nsresult rv = GetArchiveFile(aStoragePath, getter_AddRefs(archiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // QuotaManager ensures this file always exists.
  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(archiveFile->Exists(&exists)));
  MOZ_ASSERT(exists);

  bool isDirectory;
  rv = archiveFile->IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isDirectory) {
    LS_WARNING("ls-archive is not a file!");
    *aConnection = nullptr;
    return NS_OK;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenUnsharedDatabase(archiveFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Don't throw an error, leave a corrupted ls-archive database as it is.
    *aConnection = nullptr;
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = StorageDBUpdater::Update(connection);
  if (NS_FAILED(rv)) {
    // Don't throw an error, leave a non-updateable ls-archive database as
    // it is.
    *aConnection = nullptr;
    return NS_OK;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult AttachArchiveDatabase(const nsAString& aStoragePath,
                               mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aStoragePath.IsEmpty());
  MOZ_ASSERT(aConnection);
  nsCOMPtr<nsIFile> archiveFile;

  nsresult rv = GetArchiveFile(aStoragePath, getter_AddRefs(archiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  bool exists;
  rv = archiveFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(exists);
#endif

  nsString path;
  rv = archiveFile->GetPath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("ATTACH DATABASE :path AS archive;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("path"), path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DetachArchiveDatabase(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DETACH DATABASE archive"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult GetShadowFile(const nsAString& aBasePath, nsIFile** aArchiveFile) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(!aBasePath.IsEmpty());
  MOZ_ASSERT(aArchiveFile);

  nsCOMPtr<nsIFile> archiveFile;
  nsresult rv = NS_NewLocalFile(aBasePath, false, getter_AddRefs(archiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = archiveFile->Append(NS_LITERAL_STRING(WEB_APPS_STORE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  archiveFile.forget(aArchiveFile);
  return NS_OK;
}

nsresult SetShadowJournalMode(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // Try enabling WAL mode. This can fail in various circumstances so we have to
  // check the results here.
  NS_NAMED_LITERAL_CSTRING(journalModeQueryStart, "PRAGMA journal_mode = ");
  NS_NAMED_LITERAL_CSTRING(journalModeWAL, "wal");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(
      journalModeQueryStart + journalModeWAL, getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(hasResult);

  nsCString journalMode;
  rv = stmt->GetUTF8String(0, journalMode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (journalMode.Equals(journalModeWAL)) {
    // WAL mode successfully enabled. Set limits on its size here.

    // Set the threshold for auto-checkpointing the WAL. We don't want giant
    // logs slowing down us.
    rv = aConnection->CreateStatement(NS_LITERAL_CSTRING("PRAGMA page_size;"),
                                      getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(hasResult);

    int32_t pageSize;
    rv = stmt->GetInt32(0, &pageSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(pageSize >= 512 && pageSize <= 65536);

    nsAutoCString pageCount;
    pageCount.AppendInt(static_cast<int32_t>(kShadowMaxWALSize / pageSize));

    rv = aConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("PRAGMA wal_autocheckpoint = ") + pageCount);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Set the maximum WAL log size to reduce footprint on mobile (large empty
    // WAL files will be truncated)
    nsAutoCString sizeLimit;
    sizeLimit.AppendInt(kShadowJournalSizeLimit);

    rv = aConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("PRAGMA journal_size_limit = ") + sizeLimit);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = aConnection->ExecuteSimpleSQL(journalModeQueryStart +
                                       NS_LITERAL_CSTRING("truncate"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult CreateShadowStorageConnection(const nsAString& aBasePath,
                                       mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aBasePath.IsEmpty());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> shadowFile;
  nsresult rv = GetShadowFile(aBasePath, getter_AddRefs(shadowFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenUnsharedDatabase(shadowFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    rv = shadowFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenUnsharedDatabase(shadowFile, getter_AddRefs(connection));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetShadowJournalMode(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = StorageDBUpdater::Update(connection);
  if (NS_FAILED(rv)) {
    rv = connection->Close();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = shadowFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenUnsharedDatabase(shadowFile, getter_AddRefs(connection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = SetShadowJournalMode(connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = StorageDBUpdater::Update(connection);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult GetShadowStorageConnection(const nsAString& aBasePath,
                                    mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aBasePath.IsEmpty());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> shadowFile;
  nsresult rv = GetShadowFile(aBasePath, getter_AddRefs(shadowFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = shadowFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!exists)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenUnsharedDatabase(shadowFile, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult AttachShadowDatabase(const nsAString& aBasePath,
                              mozIStorageConnection* aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aBasePath.IsEmpty());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> shadowFile;
  nsresult rv = GetShadowFile(aBasePath, getter_AddRefs(shadowFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  bool exists;
  rv = shadowFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(exists);
#endif

  nsString path;
  rv = shadowFile->GetPath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("ATTACH DATABASE :path AS shadow;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("path"), path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DetachShadowDatabase(mozIStorageConnection* aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DETACH DATABASE shadow"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/*******************************************************************************
 * Non-actor class declarations
 ******************************************************************************/

/**
 * Coalescing manipulation queue used by `Connection` and `DataStore`.  Used by
 * `Connection` to buffer and coalesce manipulations applied to the Datastore
 * in batches by Snapshot Checkpointing until flushed to disk.  Used by
 * `Datastore` to update `DataStore::mOrderedItems` efficiently/for code
 * simplification.  (DataStore does not actually depend on the coalescing, as
 * mutations are applied atomically when a Snapshot Checkpoints, and with
 * `Datastore::mValues` being updated at the same time the mutations are applied
 * to Datastore's mWriteOptimizer.)
 */
class WriteOptimizer final {
  class WriteInfo;
  class AddItemInfo;
  class UpdateItemInfo;
  class RemoveItemInfo;
  class ClearInfo;

  nsAutoPtr<WriteInfo> mClearInfo;
  nsClassHashtable<nsStringHashKey, WriteInfo> mWriteInfos;

 public:
  WriteOptimizer() {}

  WriteOptimizer(WriteOptimizer&& aWriteOptimizer)
      : mClearInfo(std::move(aWriteOptimizer.mClearInfo)) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(&aWriteOptimizer != this);

    mWriteInfos.SwapElements(aWriteOptimizer.mWriteInfos);
  }

  void AddItem(const nsString& aKey, const nsString& aValue);

  void UpdateItem(const nsString& aKey, const nsString& aValue);

  void RemoveItem(const nsString& aKey);

  void Clear();

  bool HasWrites() const {
    AssertIsOnBackgroundThread();

    return mClearInfo || !mWriteInfos.IsEmpty();
  }

  void ApplyWrites(nsTArray<LSItemInfo>& aOrderedItems);

  nsresult PerformWrites(Connection* aConnection, bool aShadowWrites);
};

/**
 * Base class for specific mutations.  Each subclass knows how to `Perform` the
 * manipulation against a `Connection` and the "shadow" database (legacy
 * webappsstore.sqlite database that exists so LSNG can be disabled/safely
 * downgraded from.)
 */
class WriteOptimizer::WriteInfo {
 public:
  enum Type { AddItem = 0, UpdateItem, RemoveItem, Clear };

  virtual Type GetType() = 0;

  virtual nsresult Perform(Connection* aConnection, bool aShadowWrites) = 0;

  virtual ~WriteInfo() = default;
};

/**
 * SetItem mutation where the key did not previously exist.
 */
class WriteOptimizer::AddItemInfo : public WriteInfo {
  nsString mKey;
  nsString mValue;

 public:
  AddItemInfo(const nsAString& aKey, const nsAString& aValue)
      : mKey(aKey), mValue(aValue) {}

  const nsAString& GetKey() const { return mKey; }

  const nsAString& GetValue() const { return mValue; }

 private:
  Type GetType() override { return AddItem; }

  nsresult Perform(Connection* aConnection, bool aShadowWrites) override;
};

/**
 * SetItem mutation where the key already existed.
 */
class WriteOptimizer::UpdateItemInfo final : public AddItemInfo {
 public:
  UpdateItemInfo(const nsAString& aKey, const nsAString& aValue)
      : AddItemInfo(aKey, aValue) {}

 private:
  Type GetType() override { return UpdateItem; }
};

class WriteOptimizer::RemoveItemInfo final : public WriteInfo {
  nsString mKey;

 public:
  explicit RemoveItemInfo(const nsAString& aKey) : mKey(aKey) {}

  const nsAString& GetKey() const { return mKey; }

 private:
  Type GetType() override { return RemoveItem; }

  nsresult Perform(Connection* aConnection, bool aShadowWrites) override;
};

/**
 * Clear mutation.
 */
class WriteOptimizer::ClearInfo final : public WriteInfo {
 public:
  ClearInfo() {}

 private:
  Type GetType() override { return Clear; }

  nsresult Perform(Connection* aConnection, bool aShadowWrites) override;
};

class DatastoreOperationBase : public Runnable {
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  nsresult mResultCode;
  Atomic<bool> mMayProceedOnNonOwningThread;
  bool mMayProceed;

 public:
  nsIEventTarget* OwningEventTarget() const {
    MOZ_ASSERT(mOwningEventTarget);

    return mOwningEventTarget;
  }

  bool IsOnOwningThread() const {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) &&
           current;
  }

  void AssertIsOnOwningThread() const {
    MOZ_ASSERT(IsOnBackgroundThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  nsresult ResultCode() const { return mResultCode; }

  void SetFailureCode(nsresult aErrorCode) {
    MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    mResultCode = aErrorCode;
  }

  void MaybeSetFailureCode(nsresult aErrorCode) {
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = aErrorCode;
    }
  }

  void NoteComplete() {
    AssertIsOnOwningThread();

    mMayProceed = false;
    mMayProceedOnNonOwningThread = false;
  }

  bool MayProceed() const {
    AssertIsOnOwningThread();

    return mMayProceed;
  }

  // May be called on any thread, but you should call MayProceed() if you know
  // you're on the background thread because it is slightly faster.
  bool MayProceedOnNonOwningThread() const {
    return mMayProceedOnNonOwningThread;
  }

 protected:
  DatastoreOperationBase()
      : Runnable("dom::DatastoreOperationBase"),
        mOwningEventTarget(GetCurrentThreadEventTarget()),
        mResultCode(NS_OK),
        mMayProceedOnNonOwningThread(true),
        mMayProceed(true) {}

  ~DatastoreOperationBase() override { MOZ_ASSERT(!mMayProceed); }
};

class ConnectionDatastoreOperationBase : public DatastoreOperationBase {
 protected:
  RefPtr<Connection> mConnection;

 public:
  // This callback will be called on the background thread before releasing the
  // final reference to this request object. Subclasses may perform any
  // additional cleanup here but must always call the base class implementation.
  virtual void Cleanup();

 protected:
  ConnectionDatastoreOperationBase(Connection* aConnection);

  ~ConnectionDatastoreOperationBase();

  // Must be overridden in subclasses. Called on the target thread to allow the
  // subclass to perform necessary datastore operations. A successful return
  // value will trigger an OnSuccess callback on the background thread while
  // while a failure value will trigger an OnFailure callback.
  virtual nsresult DoDatastoreWork() = 0;

  // Methods that subclasses may implement.
  virtual void OnSuccess();

  virtual void OnFailure(nsresult aResultCode);

 private:
  void RunOnConnectionThread();

  void RunOnOwningThread();

  // Not to be overridden by subclasses.
  NS_DECL_NSIRUNNABLE
};

class Connection final {
  friend class ConnectionThread;

 public:
  class CachedStatement;

 private:
  class FlushOp;
  class CloseOp;

  RefPtr<ConnectionThread> mConnectionThread;
  nsCOMPtr<nsITimer> mFlushTimer;
  nsCOMPtr<mozIStorageConnection> mStorageConnection;
  nsAutoPtr<ArchivedOriginScope> mArchivedOriginScope;
  nsInterfaceHashtable<nsCStringHashKey, mozIStorageStatement>
      mCachedStatements;
  WriteOptimizer mWriteOptimizer;
  const nsCString mOrigin;
  const nsString mFilePath;
  bool mFlushScheduled;
#ifdef DEBUG
  bool mInUpdateBatch;
#endif

 public:
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Connection)

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(Connection); }

  ArchivedOriginScope* GetArchivedOriginScope() const {
    return mArchivedOriginScope;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Methods which can only be called on the owning thread.

  // This method is used to asynchronously execute a connection datastore
  // operation on the connection thread.
  void Dispatch(ConnectionDatastoreOperationBase* aOp);

  // This method is used to asynchronously close the storage connection on the
  // connection thread.
  void Close(nsIRunnable* aCallback);

  void AddItem(const nsString& aKey, const nsString& aValue);

  void UpdateItem(const nsString& aKey, const nsString& aValue);

  void RemoveItem(const nsString& aKey);

  void Clear();

  void BeginUpdateBatch();

  void EndUpdateBatch();

  //////////////////////////////////////////////////////////////////////////////
  // Methods which can only be called on the connection thread.

  nsresult EnsureStorageConnection();

  mozIStorageConnection* StorageConnection() const {
    AssertIsOnConnectionThread();
    MOZ_ASSERT(mStorageConnection);

    return mStorageConnection;
  }

  void CloseStorageConnection();

  nsresult GetCachedStatement(const nsACString& aQuery,
                              CachedStatement* aCachedStatement);

 private:
  // Only created by ConnectionThread.
  Connection(ConnectionThread* aConnectionThread, const nsACString& aOrigin,
             const nsAString& aFilePath,
             nsAutoPtr<ArchivedOriginScope>&& aArchivedOriginScope);

  ~Connection();

  void ScheduleFlush();

  void Flush();

  static void FlushTimerCallback(nsITimer* aTimer, void* aClosure);
};

class Connection::CachedStatement final {
  friend class Connection;

  nsCOMPtr<mozIStorageStatement> mStatement;
  Maybe<mozStorageStatementScoper> mScoper;

 public:
  CachedStatement();
  ~CachedStatement();

  operator mozIStorageStatement*() const;

  mozIStorageStatement* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN;

 private:
  // Only called by Connection.
  void Assign(Connection* aConnection,
              already_AddRefed<mozIStorageStatement> aStatement);

  // No funny business allowed.
  CachedStatement(const CachedStatement&) = delete;
  CachedStatement& operator=(const CachedStatement&) = delete;
};

class Connection::FlushOp final : public ConnectionDatastoreOperationBase {
  RefPtr<QuotaClient> mQuotaClient;
  WriteOptimizer mWriteOptimizer;
  bool mShadowWrites;

 public:
  FlushOp(Connection* aConnection, WriteOptimizer&& aWriteOptimizer);

 private:
  nsresult DoDatastoreWork() override;
};

class Connection::CloseOp final : public ConnectionDatastoreOperationBase {
  nsCOMPtr<nsIRunnable> mCallback;

 public:
  CloseOp(Connection* aConnection, nsIRunnable* aCallback)
      : ConnectionDatastoreOperationBase(aConnection), mCallback(aCallback) {}

 private:
  nsresult DoDatastoreWork() override;

  void Cleanup() override;
};

class ConnectionThread final {
  friend class Connection;

  nsCOMPtr<nsIThread> mThread;
  nsRefPtrHashtable<nsCStringHashKey, Connection> mConnections;

 public:
  ConnectionThread();

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(ConnectionThread);
  }

  bool IsOnConnectionThread();

  void AssertIsOnConnectionThread();

  already_AddRefed<Connection> CreateConnection(
      const nsACString& aOrigin, const nsAString& aFilePath,
      nsAutoPtr<ArchivedOriginScope>&& aArchivedOriginScope);

  void Shutdown();

  NS_INLINE_DECL_REFCOUNTING(ConnectionThread)

 private:
  ~ConnectionThread();
};

/**
 * Canonical state of Storage for an origin, containing all keys and their
 * values in the parent process.  Specifically, this is the state that will
 * be handed out to freshly created Snapshots and that will be persisted to disk
 * when the Connection's flush completes.  State is mutated in batches as
 * Snapshot instances Checkpoint their mutations locally accumulated in the
 * child LSSnapshots.
 */
class Datastore final {
  RefPtr<DirectoryLock> mDirectoryLock;
  RefPtr<Connection> mConnection;
  RefPtr<QuotaObject> mQuotaObject;
  nsCOMPtr<nsIRunnable> mCompleteCallback;
  /**
   * PrepareDatastoreOps register themselves with the Datastore at
   * and unregister in PrepareDatastoreOp::Cleanup.
   */
  nsTHashtable<nsPtrHashKey<PrepareDatastoreOp>> mPrepareDatastoreOps;
  /**
   * PreparedDatastore instances register themselves with their associated
   * Datastore at construction time and unregister at destruction time.  They
   * hang around for kPreparedDatastoreTimeoutMs in order to keep the Datastore
   * from closing itself via MaybeClose(), thereby giving the document enough
   * time to load and access LocalStorage.
   */
  nsTHashtable<nsPtrHashKey<PreparedDatastore>> mPreparedDatastores;
  /**
   * A database is live (and in this hashtable) if it has a live LSDatabase
   * actor.  There is at most one Database per origin per content process.  Each
   * Database corresponds to an LSDatabase in its associated content process.
   */
  nsTHashtable<nsPtrHashKey<Database>> mDatabases;
  /**
   * A database is active if it has a non-null `mSnapshot`.  As long as there
   * are any active databases final deltas can't be calculated and
   * `UpdateUsage()` can't be invoked.
   */
  nsTHashtable<nsPtrHashKey<Database>> mActiveDatabases;
  /**
   * Non-authoritative hashtable representation of mOrderedItems for efficient
   * lookup.
   */
  nsDataHashtable<nsStringHashKey, nsString> mValues;
  /**
   * The authoritative ordered state of the Datastore; mValue also exists as an
   * unordered hashtable for efficient lookup.
   */
  nsTArray<LSItemInfo> mOrderedItems;
  nsTArray<int64_t> mPendingUsageDeltas;
  WriteOptimizer mWriteOptimizer;
  const nsCString mOrigin;
  const uint32_t mPrivateBrowsingId;
  int64_t mUsage;
  int64_t mUpdateBatchUsage;
  int64_t mSizeOfKeys;
  int64_t mSizeOfItems;
  bool mClosed;
#ifdef DEBUG
  bool mInUpdateBatch;
#endif

 public:
  // Created by PrepareDatastoreOp.
  Datastore(const nsACString& aOrigin, uint32_t aPrivateBrowsingId,
            int64_t aUsage, int64_t aSizeOfKeys, int64_t aSizeOfItems,
            already_AddRefed<DirectoryLock>&& aDirectoryLock,
            already_AddRefed<Connection>&& aConnection,
            already_AddRefed<QuotaObject>&& aQuotaObject,
            nsDataHashtable<nsStringHashKey, nsString>& aValues,
            nsTArray<LSItemInfo>& aOrderedItems);

  const nsCString& Origin() const { return mOrigin; }

  uint32_t PrivateBrowsingId() const { return mPrivateBrowsingId; }

  bool IsPersistent() const {
    // Private-browsing is forbidden from touching disk, but
    // StorageAccess::eSessionScoped is allowed to touch disk because
    // QuotaManager's storage for such origins is wiped at shutdown.
    return mPrivateBrowsingId == 0;
  }

  void Close();

  bool IsClosed() const {
    AssertIsOnBackgroundThread();

    return mClosed;
  }

  void WaitForConnectionToComplete(nsIRunnable* aCallback);

  void NoteLivePrepareDatastoreOp(PrepareDatastoreOp* aPrepareDatastoreOp);

  void NoteFinishedPrepareDatastoreOp(PrepareDatastoreOp* aPrepareDatastoreOp);

  void NoteLivePreparedDatastore(PreparedDatastore* aPreparedDatastore);

  void NoteFinishedPreparedDatastore(PreparedDatastore* aPreparedDatastore);

#ifdef DEBUG
  bool HasLivePreparedDatastores() const;
#endif

  void NoteLiveDatabase(Database* aDatabase);

  void NoteFinishedDatabase(Database* aDatabase);

#ifdef DEBUG
  bool HasLiveDatabases() const;
#endif

  void NoteActiveDatabase(Database* aDatabase);

  void NoteInactiveDatabase(Database* aDatabase);

  void GetSnapshotInitInfo(nsTHashtable<nsStringHashKey>& aLoadedItems,
                           nsTArray<LSItemInfo>& aItemInfos,
                           uint32_t& aTotalLength, int64_t& aInitialUsage,
                           int64_t& aPeakUsage,
                           LSSnapshot::LoadState& aLoadState);

  void GetItem(const nsString& aKey, nsString& aValue) const;

  void GetKeys(nsTArray<nsString>& aKeys) const;

  //////////////////////////////////////////////////////////////////////////////
  // Mutation Methods
  //
  // These are only called during Snapshot::RecvCheckpoint

  /**
   * Used by Snapshot::RecvCheckpoint to set a key/value pair as part of a an
   * explicit batch.
   */
  void SetItem(Database* aDatabase, const nsString& aDocumentURI,
               const nsString& aKey, const nsString& aOldValue,
               const nsString& aValue);

  void RemoveItem(Database* aDatabase, const nsString& aDocumentURI,
                  const nsString& aKey, const nsString& aOldValue);

  void Clear(Database* aDatabase, const nsString& aDocumentURI);

  void PrivateBrowsingClear();

  void BeginUpdateBatch(int64_t aSnapshotInitialUsage);

  int64_t EndUpdateBatch(int64_t aSnapshotPeakUsage);

  int64_t RequestUpdateUsage(int64_t aRequestedSize, int64_t aMinSize);

  NS_INLINE_DECL_REFCOUNTING(Datastore)

 private:
  // Reference counted.
  ~Datastore();

  bool UpdateUsage(int64_t aDelta);

  void MaybeClose();

  void ConnectionClosedCallback();

  void CleanupMetadata();

  void NotifySnapshots(Database* aDatabase, const nsAString& aKey,
                       const nsAString& aOldValue, bool aAffectsOrder);

  void MarkSnapshotsDirty();

  void NotifyObservers(Database* aDatabase, const nsString& aDocumentURI,
                       const nsString& aKey, const nsString& aOldValue,
                       const nsString& aNewValue);
};

class PreparedDatastore {
  RefPtr<Datastore> mDatastore;
  nsCOMPtr<nsITimer> mTimer;
  const Maybe<ContentParentId> mContentParentId;
  // Strings share buffers if possible, so it's not a problem to duplicate the
  // origin here.
  const nsCString mOrigin;
  uint64_t mDatastoreId;
  bool mForPreload;
  bool mInvalidated;

 public:
  PreparedDatastore(Datastore* aDatastore,
                    const Maybe<ContentParentId>& aContentParentId,
                    const nsACString& aOrigin, uint64_t aDatastoreId,
                    bool aForPreload)
      : mDatastore(aDatastore),
        mTimer(NS_NewTimer()),
        mContentParentId(aContentParentId),
        mOrigin(aOrigin),
        mDatastoreId(aDatastoreId),
        mForPreload(aForPreload),
        mInvalidated(false) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aDatastore);
    MOZ_ASSERT(mTimer);

    aDatastore->NoteLivePreparedDatastore(this);

    MOZ_ALWAYS_SUCCEEDS(mTimer->InitWithNamedFuncCallback(
        TimerCallback, this, kPreparedDatastoreTimeoutMs,
        nsITimer::TYPE_ONE_SHOT, "PreparedDatastore::TimerCallback"));
  }

  ~PreparedDatastore() {
    MOZ_ASSERT(mDatastore);
    MOZ_ASSERT(mTimer);

    mTimer->Cancel();

    mDatastore->NoteFinishedPreparedDatastore(this);
  }

  Datastore* GetDatastore() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatastore);

    return mDatastore;
  }

  const Maybe<ContentParentId>& GetContentParentId() const {
    return mContentParentId;
  }

  const nsCString& Origin() const { return mOrigin; }

  void Invalidate() {
    AssertIsOnBackgroundThread();

    mInvalidated = true;

    if (mForPreload) {
      mTimer->Cancel();

      MOZ_ALWAYS_SUCCEEDS(mTimer->InitWithNamedFuncCallback(
          TimerCallback, this, 0, nsITimer::TYPE_ONE_SHOT,
          "PreparedDatastore::TimerCallback"));
    }
  }

  bool IsInvalidated() const {
    AssertIsOnBackgroundThread();

    return mInvalidated;
  }

 private:
  void Destroy();

  static void TimerCallback(nsITimer* aTimer, void* aClosure);
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class Database final : public PBackgroundLSDatabaseParent {
  RefPtr<Datastore> mDatastore;
  Snapshot* mSnapshot;
  const PrincipalInfo mPrincipalInfo;
  const Maybe<ContentParentId> mContentParentId;
  // Strings share buffers if possible, so it's not a problem to duplicate the
  // origin here.
  nsCString mOrigin;
  uint32_t mPrivateBrowsingId;
  bool mAllowedToClose;
  bool mActorDestroyed;
  bool mRequestedAllowToClose;
#ifdef DEBUG
  bool mActorWasAlive;
#endif

 public:
  // Created in AllocPBackgroundLSDatabaseParent.
  Database(const PrincipalInfo& aPrincipalInfo,
           const Maybe<ContentParentId>& aContentParentId,
           const nsACString& aOrigin, uint32_t aPrivateBrowsingId);

  Datastore* GetDatastore() const {
    AssertIsOnBackgroundThread();
    return mDatastore;
  }

  const PrincipalInfo& GetPrincipalInfo() const { return mPrincipalInfo; }

  bool IsOwnedByProcess(ContentParentId aContentParentId) const {
    return mContentParentId && mContentParentId.value() == aContentParentId;
  }

  uint32_t PrivateBrowsingId() const { return mPrivateBrowsingId; }

  const nsCString& Origin() const { return mOrigin; }

  void SetActorAlive(Datastore* aDatastore);

  void RegisterSnapshot(Snapshot* aSnapshot);

  void UnregisterSnapshot(Snapshot* aSnapshot);

  Snapshot* GetSnapshot() const {
    AssertIsOnBackgroundThread();
    return mSnapshot;
  }

  void RequestAllowToClose();

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Database)

 private:
  // Reference counted.
  ~Database();

  void AllowToClose();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvAllowToClose() override;

  PBackgroundLSSnapshotParent* AllocPBackgroundLSSnapshotParent(
      const nsString& aDocumentURI, const bool& aIncreasePeakUsage,
      const int64_t& aRequestedSize, const int64_t& aMinSize,
      LSSnapshotInitInfo* aInitInfo) override;

  mozilla::ipc::IPCResult RecvPBackgroundLSSnapshotConstructor(
      PBackgroundLSSnapshotParent* aActor, const nsString& aDocumentURI,
      const bool& aIncreasePeakUsage, const int64_t& aRequestedSize,
      const int64_t& aMinSize, LSSnapshotInitInfo* aInitInfo) override;

  bool DeallocPBackgroundLSSnapshotParent(
      PBackgroundLSSnapshotParent* aActor) override;
};

/**
 * Attempts to capture the state of the underlying Datastore at the time of its
 * creation so run-to-completion semantics can be honored.
 *
 * Rather than simply duplicate the contents of `DataStore::mValues` and
 * `Datastore::mOrderedItems` at the time of their creation, the Snapshot tracks
 * mutations to the Datastore as they happen, saving off the state of values as
 * they existed when the Snapshot was created.  In other words, given an initial
 * Datastore state of { foo: 'bar', bar: 'baz' }, the Snapshot won't store those
 * values until it hears via `SaveItem` that "foo" is being over-written.  At
 * that time, it will save off foo='bar' in mValues.
 *
 * ## Quota Allocation ##
 *
 * ## States ##
 *
 */
class Snapshot final : public PBackgroundLSSnapshotParent {
  /**
   * The Database that owns this snapshot.  There is a 1:1 relationship between
   * snapshots and databases.
   */
  RefPtr<Database> mDatabase;
  RefPtr<Datastore> mDatastore;
  /**
   * The set of keys for which values have been sent to the child LSSnapshot.
   * Cleared once all values have been sent as indicated by
   * mLoadedItems.Count()==mTotalLength and therefore mLoadedAllItems should be
   * true.  No requests should be received for keys already in this set, and
   * this is enforced by fatal IPC error (unless fuzzing).
   */
  nsTHashtable<nsStringHashKey> mLoadedItems;
  /**
   * The set of keys for which a RecvLoadItem request was received but there
   * was no such key, and so null was returned.  The child LSSnapshot will also
   * cache these values, so redundant requests are also handled with fatal
   * process termination just like for mLoadedItems.  Also cleared when
   * mLoadedAllItems becomes true because then the child can infer that all
   * other values must be null.  (Note: this could also be done when
   * mLoadKeysReceived is true as a further optimization, but is not.)
   */
  nsTHashtable<nsStringHashKey> mUnknownItems;
  /**
   * Values that have changed in mDatastore as reported by SaveItem
   * notifications that are not yet known to the child LSSnapshot.
   *
   * The naive way to snapshot the state of mDatastore would be to duplicate its
   * internal mValues at the time of our creation, but that is wasteful if few
   * changes are made to the Datastore's state.  So we only track values that
   * are changed/evicted from the Datastore as they happen, as reported to us by
   * SaveItem notifications.
   */
  nsDataHashtable<nsStringHashKey, nsString> mValues;
  /**
   * Latched state of mDatastore's keys during a SaveItem notification with
   * aAffectsOrder=true.  The ordered keys needed to be saved off so that a
   * consistent ordering could be presented to the child LSSnapshot when it asks
   * for them via RecvLoadKeys.
   */
  nsTArray<nsString> mKeys;
  nsString mDocumentURI;
  /**
   * The number of key/value pairs that were present in the Datastore at the
   * time the snapshot was created.  Once we have sent this many values to the
   * child LSSnapshot, we can infer that it has received all of the keys/values
   * and set mLoadedAllItems to true and clear mLoadedItems and mUnknownItems.
   * Note that knowing the keys/values is not the same as knowing their ordering
   * and so mKeys may be retained.
   */
  uint32_t mTotalLength;
  int64_t mUsage;
  int64_t mPeakUsage;
  /**
   * True if SaveItem has saved mDatastore's keys into mKeys because a SaveItem
   * notification with aAffectsOrder=true was received.
   */
  bool mSavedKeys;
  bool mActorDestroyed;
  bool mFinishReceived;
  bool mLoadedReceived;
  /**
   * True if LSSnapshot's mLoadState should be LoadState::AllOrderedItems or
   * LoadState::AllUnorderedItems.  It will be AllOrderedItems if the initial
   * snapshot contained all the data or if the state was AllOrderedKeys and
   * successive RecvLoadItem requests have resulted in the LSSnapshot being told
   * all of the key/value pairs.  It will be AllUnorderedItems if the state was
   * LoadState::Partial and successive RecvLoadItem requests got all the
   * keys/values but the key ordering was not retrieved.
   */
  bool mLoadedAllItems;
  /**
   * True if LSSnapshot's mLoadState should be LoadState::AllOrderedItems or
   * AllOrderedKeys.  This can occur because of the initial snapshot, or because
   * a RecvLoadKeys request was received.
   */
  bool mLoadKeysReceived;
  bool mSentMarkDirty;

 public:
  // Created in AllocPBackgroundLSSnapshotParent.
  Snapshot(Database* aDatabase, const nsAString& aDocumentURI);

  void Init(nsTHashtable<nsStringHashKey>& aLoadedItems, uint32_t aTotalLength,
            int64_t aInitialUsage, int64_t aPeakUsage,
            LSSnapshot::LoadState aLoadState) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aInitialUsage >= 0);
    MOZ_ASSERT(aPeakUsage >= aInitialUsage);
    MOZ_ASSERT_IF(aLoadState == LSSnapshot::LoadState::AllOrderedItems,
                  aLoadedItems.Count() == 0);
    MOZ_ASSERT(mTotalLength == 0);
    MOZ_ASSERT(mUsage == -1);
    MOZ_ASSERT(mPeakUsage == -1);

    mLoadedItems.SwapElements(aLoadedItems);
    mTotalLength = aTotalLength;
    mUsage = aInitialUsage;
    mPeakUsage = aPeakUsage;
    if (aLoadState == LSSnapshot::LoadState::AllOrderedKeys) {
      mLoadKeysReceived = true;
    } else if (aLoadState == LSSnapshot::LoadState::AllOrderedItems) {
      mLoadedReceived = true;
      mLoadedAllItems = true;
      mLoadKeysReceived = true;
    }
  }

  /**
   * Called via NotifySnapshots by Datastore whenever it is updating its
   * internal state so that snapshots can save off the state of a value at the
   * time of their creation.
   */
  void SaveItem(const nsAString& aKey, const nsAString& aOldValue,
                bool aAffectsOrder);

  void MarkDirty();

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Snapshot)

 private:
  // Reference counted.
  ~Snapshot();

  void Finish();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvCheckpoint(
      nsTArray<LSWriteInfo>&& aWriteInfos) override;

  mozilla::ipc::IPCResult RecvFinish() override;

  mozilla::ipc::IPCResult RecvLoaded() override;

  mozilla::ipc::IPCResult RecvLoadItem(const nsString& aKey,
                                       nsString* aValue) override;

  mozilla::ipc::IPCResult RecvLoadKeys(nsTArray<nsString>* aKeys) override;

  mozilla::ipc::IPCResult RecvIncreasePeakUsage(const int64_t& aRequestedSize,
                                                const int64_t& aMinSize,
                                                int64_t* aSize) override;

  mozilla::ipc::IPCResult RecvPing() override;
};

class Observer final : public PBackgroundLSObserverParent {
  nsCString mOrigin;
  bool mActorDestroyed;

 public:
  // Created in AllocPBackgroundLSObserverParent.
  explicit Observer(const nsACString& aOrigin);

  const nsCString& Origin() const { return mOrigin; }

  void Observe(Database* aDatabase, const nsString& aDocumentURI,
               const nsString& aKey, const nsString& aOldValue,
               const nsString& aNewValue);

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Observer)

 private:
  // Reference counted.
  ~Observer();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;
};

class LSRequestBase : public DatastoreOperationBase,
                      public PBackgroundLSRequestParent {
 protected:
  enum class State {
    // Just created on the PBackground thread. Next step is Opening.
    Initial,

    // Waiting to open/opening on the main thread. Next step is either
    // Nesting if a subclass needs to process more nested states or
    // SendingReadyMessage if a subclass doesn't need any nested processing.
    Opening,

    // Doing nested processing.
    Nesting,

    // Waiting to send/sending the ready message on the PBackground thread. Next
    // step is WaitingForFinish.
    SendingReadyMessage,

    // Waiting for the finish message on the PBackground thread. Next step is
    // SendingResults.
    WaitingForFinish,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  nsCOMPtr<nsIEventTarget> mMainEventTarget;
  State mState;

 public:
  explicit LSRequestBase(nsIEventTarget* aMainEventTarget);

  void Dispatch();

 protected:
  ~LSRequestBase() override;

  virtual nsresult Open() = 0;

  virtual nsresult NestedRun();

  virtual void GetResponse(LSRequestResponse& aResponse) = 0;

  virtual void Cleanup() {}

 private:
  void SendReadyMessage();

  void SendResults();

 protected:
  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  mozilla::ipc::IPCResult RecvCancel() override;

  mozilla::ipc::IPCResult RecvFinish() override;
};

class PrepareDatastoreOp : public LSRequestBase, public OpenDirectoryListener {
  class LoadDataOp;

  enum class NestedState {
    // The nesting has not yet taken place. Next step is
    // CheckExistingOperations.
    BeforeNesting,

    // Checking if a prepare datastore operation is already running for given
    // origin on the PBackground thread. Next step is CheckClosingDatastore.
    CheckExistingOperations,

    // Checking if a datastore is closing the connection for given origin on
    // the PBackground thread. Next step is PreparationPending.
    CheckClosingDatastore,

    // Opening directory or initializing quota manager on the PBackground
    // thread. Next step is either DirectoryOpenPending if quota manager is
    // already initialized or QuotaManagerPending if quota manager needs to be
    // initialized.
    // If a datastore already exists for given origin then the next state is
    // SendingReadyMessage.
    PreparationPending,

    // Waiting for quota manager initialization to complete on the PBackground
    // thread. Next step is either SendingReadyMessage if initialization failed
    // or DirectoryOpenPending if initialization succeeded.
    QuotaManagerPending,

    // Waiting for directory open allowed on the PBackground thread. The next
    // step is either SendingReadyMessage if directory lock failed to acquire,
    // or DatabaseWorkOpen if directory lock is acquired.
    DirectoryOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. Its next step is
    // BeginLoadData.
    DatabaseWorkOpen,

    // Starting a load data operation on the PBackground thread. Next step is
    // DatabaseWorkLoadData.
    BeginLoadData,

    // Waiting to do/doing work on the connection thread. This involves waiting
    // for the LoadDataOp to do its work. Eventually the state will transition
    // to SendingReadyMessage.
    DatabaseWorkLoadData,

    // The nesting has completed.
    AfterNesting
  };

  nsCOMPtr<nsIEventTarget> mMainEventTarget;
  RefPtr<ContentParent> mContentParent;
  RefPtr<PrepareDatastoreOp> mDelayedOp;
  RefPtr<DirectoryLock> mDirectoryLock;
  RefPtr<Connection> mConnection;
  RefPtr<Datastore> mDatastore;
  nsAutoPtr<ArchivedOriginScope> mArchivedOriginScope;
  LoadDataOp* mLoadDataOp;
  nsDataHashtable<nsStringHashKey, nsString> mValues;
  nsTArray<LSItemInfo> mOrderedItems;
  const LSRequestPrepareDatastoreParams mParams;
  Maybe<ContentParentId> mContentParentId;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mMainThreadOrigin;
  nsCString mOrigin;
  nsString mDatabaseFilePath;
  uint32_t mPrivateBrowsingId;
  int64_t mUsage;
  int64_t mSizeOfKeys;
  int64_t mSizeOfItems;
  NestedState mNestedState;
  bool mDatabaseNotAvailable;
  bool mRequestedDirectoryLock;
  bool mInvalidated;

#ifdef DEBUG
  int64_t mDEBUGUsage;
#endif

 public:
  PrepareDatastoreOp(nsIEventTarget* aMainEventTarget,
                     already_AddRefed<ContentParent> aContentParent,
                     const LSRequestParams& aParams);

  bool OriginIsKnown() const {
    AssertIsOnOwningThread();

    return !mOrigin.IsEmpty();
  }

  const nsCString& Origin() const {
    AssertIsOnOwningThread();
    MOZ_ASSERT(OriginIsKnown());

    return mOrigin;
  }

  bool RequestedDirectoryLock() const {
    AssertIsOnOwningThread();

    return mRequestedDirectoryLock;
  }

  void Invalidate() {
    AssertIsOnOwningThread();

    mInvalidated = true;
  }

 private:
  ~PrepareDatastoreOp() override;

  nsresult Open() override;

  nsresult CheckExistingOperations();

  nsresult CheckClosingDatastoreInternal();

  nsresult CheckClosingDatastore();

  nsresult BeginDatastorePreparationInternal();

  nsresult BeginDatastorePreparation();

  nsresult QuotaManagerOpen();

  nsresult OpenDirectory();

  void SendToIOThread();

  nsresult DatabaseWork();

  nsresult DatabaseNotAvailable();

  nsresult EnsureDirectoryEntry(nsIFile* aEntry, bool aCreateIfNotExists,
                                bool aDirectory,
                                bool* aAlreadyExisted = nullptr);

  nsresult VerifyDatabaseInformation(mozIStorageConnection* aConnection);

  already_AddRefed<QuotaObject> GetQuotaObject();

  nsresult BeginLoadData();

  void FinishNesting();

  nsresult FinishNestingOnNonOwningThread();

  nsresult NestedRun() override;

  void GetResponse(LSRequestResponse& aResponse) override;

  void Cleanup() override;

  void ConnectionClosedCallback();

  void CleanupMetadata();

  NS_DECL_ISUPPORTS_INHERITED

  // IPDL overrides.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  // OpenDirectoryListener overrides.
  void DirectoryLockAcquired(DirectoryLock* aLock) override;

  void DirectoryLockFailed() override;
};

class PrepareDatastoreOp::LoadDataOp final
    : public ConnectionDatastoreOperationBase {
  RefPtr<PrepareDatastoreOp> mPrepareDatastoreOp;

 public:
  explicit LoadDataOp(PrepareDatastoreOp* aPrepareDatastoreOp)
      : ConnectionDatastoreOperationBase(aPrepareDatastoreOp->mConnection),
        mPrepareDatastoreOp(aPrepareDatastoreOp) {}

 private:
  ~LoadDataOp() = default;

  nsresult DoDatastoreWork() override;

  void OnSuccess() override;

  void OnFailure(nsresult aResultCode) override;

  void Cleanup() override;
};

class PrepareObserverOp : public LSRequestBase {
  const LSRequestPrepareObserverParams mParams;
  nsCString mOrigin;

 public:
  PrepareObserverOp(nsIEventTarget* aMainEventTarget,
                    const LSRequestParams& aParams);

 private:
  nsresult Open() override;

  void GetResponse(LSRequestResponse& aResponse) override;
};

class LSSimpleRequestBase : public DatastoreOperationBase,
                            public PBackgroundLSSimpleRequestParent {
 protected:
  enum class State {
    // Just created on the PBackground thread. Next step is Opening.
    Initial,

    // Waiting to open/opening on the main thread. Next step is SendingResults.
    Opening,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  State mState;

 public:
  LSSimpleRequestBase();

  void Dispatch();

 protected:
  ~LSSimpleRequestBase() override;

  virtual nsresult Open() = 0;

  virtual void GetResponse(LSSimpleRequestResponse& aResponse) = 0;

 private:
  void SendResults();

  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

class PreloadedOp : public LSSimpleRequestBase {
  const LSSimpleRequestPreloadedParams mParams;
  nsCString mOrigin;

 public:
  explicit PreloadedOp(const LSSimpleRequestParams& aParams);

 private:
  nsresult Open() override;

  void GetResponse(LSSimpleRequestResponse& aResponse) override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

struct ArchivedOriginInfo {
  OriginAttributes mOriginAttributes;
  nsCString mOriginNoSuffix;

  ArchivedOriginInfo(const OriginAttributes& aOriginAttributes,
                     const nsACString& aOriginNoSuffix)
      : mOriginAttributes(aOriginAttributes),
        mOriginNoSuffix(aOriginNoSuffix) {}
};

class ArchivedOriginScope {
  struct Origin {
    nsCString mOriginSuffix;
    nsCString mOriginNoSuffix;

    Origin(const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix)
        : mOriginSuffix(aOriginSuffix), mOriginNoSuffix(aOriginNoSuffix) {}

    const nsACString& OriginSuffix() const { return mOriginSuffix; }

    const nsACString& OriginNoSuffix() const { return mOriginNoSuffix; }
  };

  struct Prefix {
    nsCString mOriginNoSuffix;

    explicit Prefix(const nsACString& aOriginNoSuffix)
        : mOriginNoSuffix(aOriginNoSuffix) {}

    const nsACString& OriginNoSuffix() const { return mOriginNoSuffix; }
  };

  struct Pattern {
    UniquePtr<OriginAttributesPattern> mPattern;

    explicit Pattern(const OriginAttributesPattern& aPattern)
        : mPattern(MakeUnique<OriginAttributesPattern>(aPattern)) {}

    Pattern(const Pattern& aOther)
        : mPattern(MakeUnique<OriginAttributesPattern>(*aOther.mPattern)) {}

    Pattern(Pattern&& aOther) = default;

    const OriginAttributesPattern& GetPattern() const {
      MOZ_ASSERT(mPattern);
      return *mPattern;
    }
  };

  struct Null {};

  using DataType = Variant<Origin, Pattern, Prefix, Null>;

  DataType mData;

 public:
  static ArchivedOriginScope* CreateFromOrigin(nsIPrincipal* aPrincipal);

  static ArchivedOriginScope* CreateFromPrefix(nsIPrincipal* aPrincipal);

  static ArchivedOriginScope* CreateFromPattern(
      const OriginAttributesPattern& aPattern);

  static ArchivedOriginScope* CreateFromNull();

  bool IsOrigin() const { return mData.is<Origin>(); }

  bool IsPrefix() const { return mData.is<Prefix>(); }

  bool IsPattern() const { return mData.is<Pattern>(); }

  bool IsNull() const { return mData.is<Null>(); }

  const nsACString& OriginSuffix() const {
    MOZ_ASSERT(IsOrigin());

    return mData.as<Origin>().OriginSuffix();
  }

  const nsACString& OriginNoSuffix() const {
    MOZ_ASSERT(IsOrigin() || IsPrefix());

    if (IsOrigin()) {
      return mData.as<Origin>().OriginNoSuffix();
    }
    return mData.as<Prefix>().OriginNoSuffix();
  }

  const OriginAttributesPattern& GetPattern() const {
    MOZ_ASSERT(IsPattern());

    return mData.as<Pattern>().GetPattern();
  }

  void GetBindingClause(nsACString& aBindingClause) const;

  nsresult BindToStatement(mozIStorageStatement* aStatement) const;

  bool HasMatches(ArchivedOriginHashtable* aHashtable) const;

  void RemoveMatches(ArchivedOriginHashtable* aHashtable) const;

 private:
  // Move constructors
  explicit ArchivedOriginScope(const Origin&& aOrigin) : mData(aOrigin) {}

  explicit ArchivedOriginScope(const Pattern&& aPattern) : mData(aPattern) {}

  explicit ArchivedOriginScope(const Prefix&& aPrefix) : mData(aPrefix) {}

  explicit ArchivedOriginScope(const Null&& aNull) : mData(aNull) {}
};

class ArchivedOriginScopeHelper : public Runnable {
  Monitor mMonitor;
  const OriginAttributes mAttrs;
  const nsCString mSpec;
  nsAutoPtr<ArchivedOriginScope> mArchivedOriginScope;
  nsresult mMainThreadResultCode;
  bool mWaiting;
  bool mPrefix;

 public:
  ArchivedOriginScopeHelper(const nsACString& aSpec,
                            const OriginAttributes& aAttrs, bool aPrefix)
      : Runnable("dom::localstorage::ArchivedOriginScopeHelper"),
        mMonitor("ArchivedOriginScopeHelper::mMonitor"),
        mAttrs(aAttrs),
        mSpec(aSpec),
        mMainThreadResultCode(NS_OK),
        mWaiting(true),
        mPrefix(aPrefix) {
    AssertIsOnIOThread();
  }

  nsresult BlockAndReturnArchivedOriginScope(
      nsAutoPtr<ArchivedOriginScope>& aArchivedOriginScope);

 private:
  nsresult RunOnMainThread();

  NS_DECL_NSIRUNNABLE
};

class QuotaClient final : public mozilla::dom::quota::Client {
  class ClearPrivateBrowsingRunnable;
  class Observer;
  class MatchFunction;

  static QuotaClient* sInstance;
  static bool sObserversRegistered;

  Mutex mShadowDatabaseMutex;
  bool mShutdownRequested;

 public:
  QuotaClient();

  static QuotaClient* GetInstance() {
    AssertIsOnBackgroundThread();

    return sInstance;
  }

  static bool IsShuttingDownOnBackgroundThread() {
    AssertIsOnBackgroundThread();

    if (sInstance) {
      return sInstance->IsShuttingDown();
    }

    return QuotaManager::IsShuttingDown();
  }

  static bool IsShuttingDownOnNonBackgroundThread() {
    MOZ_ASSERT(!IsOnBackgroundThread());

    return QuotaManager::IsShuttingDown();
  }

  static nsresult RegisterObservers(nsIEventTarget* aBackgroundEventTarget);

  mozilla::Mutex& ShadowDatabaseMutex() {
    MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());

    return mShadowDatabaseMutex;
  }

  bool IsShuttingDown() const {
    AssertIsOnBackgroundThread();

    return mShutdownRequested;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::QuotaClient, override)

  Type GetType() override;

  nsresult InitOrigin(PersistenceType aPersistenceType,
                      const nsACString& aGroup, const nsACString& aOrigin,
                      const AtomicBool& aCanceled,
                      UsageInfo* aUsageInfo) override;

  nsresult GetUsageForOrigin(PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin,
                             const AtomicBool& aCanceled,
                             UsageInfo* aUsageInfo) override;

  nsresult AboutToClearOrigins(
      const Nullable<PersistenceType>& aPersistenceType,
      const OriginScope& aOriginScope) override;

  void OnOriginClearCompleted(PersistenceType aPersistenceType,
                              const nsACString& aOrigin) override;

  void ReleaseIOThreadObjects() override;

  void AbortOperations(const nsACString& aOrigin) override;

  void AbortOperationsForProcess(ContentParentId aContentParentId) override;

  void StartIdleMaintenance() override;

  void StopIdleMaintenance() override;

  void ShutdownWorkThreads() override;

 private:
  ~QuotaClient() override;

  nsresult CreateArchivedOriginScope(
      const OriginScope& aOriginScope,
      nsAutoPtr<ArchivedOriginScope>& aArchivedOriginScope);

  nsresult PerformDelete(mozIStorageConnection* aConnection,
                         const nsACString& aSchemaName,
                         ArchivedOriginScope* aArchivedOriginScope) const;
};

class QuotaClient::ClearPrivateBrowsingRunnable final : public Runnable {
 public:
  ClearPrivateBrowsingRunnable()
      : Runnable("mozilla::dom::ClearPrivateBrowsingRunnable") {
    MOZ_ASSERT(NS_IsMainThread());
  }

 private:
  ~ClearPrivateBrowsingRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

class QuotaClient::Observer final : public nsIObserver {
  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;

 public:
  explicit Observer(nsIEventTarget* aBackgroundEventTarget)
      : mBackgroundEventTarget(aBackgroundEventTarget) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_DECL_ISUPPORTS

 private:
  ~Observer() { MOZ_ASSERT(NS_IsMainThread()); }

  NS_DECL_NSIOBSERVER
};

class QuotaClient::MatchFunction final : public mozIStorageFunction {
  OriginAttributesPattern mPattern;

 public:
  explicit MatchFunction(const OriginAttributesPattern& aPattern)
      : mPattern(aPattern) {}

 private:
  ~MatchFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

/*******************************************************************************
 * Globals
 ******************************************************************************/

typedef nsTArray<PrepareDatastoreOp*> PrepareDatastoreOpArray;

StaticAutoPtr<PrepareDatastoreOpArray> gPrepareDatastoreOps;

typedef nsDataHashtable<nsCStringHashKey, Datastore*> DatastoreHashtable;

StaticAutoPtr<DatastoreHashtable> gDatastores;

uint64_t gLastDatastoreId = 0;

typedef nsClassHashtable<nsUint64HashKey, PreparedDatastore>
    PreparedDatastoreHashtable;

StaticAutoPtr<PreparedDatastoreHashtable> gPreparedDatastores;

typedef nsTArray<Database*> LiveDatabaseArray;

StaticAutoPtr<LiveDatabaseArray> gLiveDatabases;

StaticRefPtr<ConnectionThread> gConnectionThread;

uint64_t gLastObserverId = 0;

typedef nsRefPtrHashtable<nsUint64HashKey, Observer> PreparedObserverHashtable;

StaticAutoPtr<PreparedObserverHashtable> gPreparedObsevers;

typedef nsClassHashtable<nsCStringHashKey, nsTArray<Observer*>>
    ObserverHashtable;

StaticAutoPtr<ObserverHashtable> gObservers;

Atomic<uint32_t, Relaxed> gOriginLimitKB(kDefaultOriginLimitKB);
Atomic<bool> gShadowWrites(kDefaultShadowWrites);
Atomic<int32_t, Relaxed> gSnapshotPrefill(kDefaultSnapshotPrefill);

typedef nsDataHashtable<nsCStringHashKey, int64_t> UsageHashtable;

// Can only be touched on the Quota Manager I/O thread.
StaticAutoPtr<UsageHashtable> gUsages;

StaticAutoPtr<ArchivedOriginHashtable> gArchivedOrigins;

// Can only be touched on the Quota Manager I/O thread.
bool gInitializedShadowStorage = false;

bool IsOnConnectionThread() {
  MOZ_ASSERT(gConnectionThread);
  return gConnectionThread->IsOnConnectionThread();
}

void AssertIsOnConnectionThread() {
  MOZ_ASSERT(gConnectionThread);
  gConnectionThread->AssertIsOnConnectionThread();
}

void InitUsageForOrigin(const nsACString& aOrigin, int64_t aUsage) {
  AssertIsOnIOThread();

  if (!gUsages) {
    gUsages = new UsageHashtable();
  }

  MOZ_ASSERT(!gUsages->Contains(aOrigin));
  gUsages->Put(aOrigin, aUsage);
}

nsresult LoadArchivedOrigins() {
  AssertIsOnIOThread();
  MOZ_ASSERT(!gArchivedOrigins);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Ensure that the webappsstore.sqlite is moved to new place.
  nsresult rv = quotaManager->EnsureStorageIsInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateArchiveStorageConnection(quotaManager->GetStoragePath(),
                                      getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!connection) {
    gArchivedOrigins = new ArchivedOriginHashtable();
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT DISTINCT originAttributes, originKey "
                         "FROM webappsstore2;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoPtr<ArchivedOriginHashtable> archivedOrigins(
      new ArchivedOriginHashtable());

  bool hasResult;
  while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsCString originSuffix;
    rv = stmt->GetUTF8String(0, originSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString originNoSuffix;
    rv = stmt->GetUTF8String(1, originNoSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString hashKey = GetArchivedOriginHashKey(originSuffix, originNoSuffix);

    OriginAttributes originAttributes;
    if (NS_WARN_IF(!originAttributes.PopulateFromSuffix(originSuffix))) {
      return NS_ERROR_FAILURE;
    }

    nsAutoPtr<ArchivedOriginInfo> archivedOriginInfo(
        new ArchivedOriginInfo(originAttributes, originNoSuffix));

    archivedOrigins->Put(hashKey, archivedOriginInfo.forget());
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  gArchivedOrigins = archivedOrigins.forget();
  return NS_OK;
}

nsresult GetUsage(mozIStorageConnection* aConnection,
                  ArchivedOriginScope* aArchivedOriginScope, int64_t* aUsage) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aUsage);

  nsresult rv;

  nsCOMPtr<mozIStorageStatement> stmt;
  if (aArchivedOriginScope) {
    rv = aConnection->CreateStatement(
        NS_LITERAL_CSTRING("SELECT sum(length(key) + length(value)) "
                           "FROM webappsstore2 "
                           "WHERE originKey = :originKey "
                           "AND originAttributes = :originAttributes;"),
        getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aArchivedOriginScope->BindToStatement(stmt);
  } else {
    rv = aConnection->CreateStatement(
        NS_LITERAL_CSTRING("SELECT sum(length(key) + length(value)) "
                           "FROM data"),
        getter_AddRefs(stmt));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    return NS_ERROR_FAILURE;
  }

  int64_t usage;
  rv = stmt->GetInt64(0, &usage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aUsage = usage;
  return NS_OK;
}

void ShadowWritesPrefChangedCallback(const char* aPrefName, void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kShadowWritesPref));
  MOZ_ASSERT(!aClosure);

  gShadowWrites = Preferences::GetBool(aPrefName, kDefaultShadowWrites);
}

void SnapshotPrefillPrefChangedCallback(const char* aPrefName, void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kSnapshotPrefillPref));
  MOZ_ASSERT(!aClosure);

  int32_t snapshotPrefill =
      Preferences::GetInt(aPrefName, kDefaultSnapshotPrefill);

  // The magic -1 is for use only by tests.
  if (snapshotPrefill == -1) {
    snapshotPrefill = INT32_MAX;
  }

  gSnapshotPrefill = snapshotPrefill;
}

}  // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

PBackgroundLSDatabaseParent* AllocPBackgroundLSDatabaseParent(
    const PrincipalInfo& aPrincipalInfo, const uint32_t& aPrivateBrowsingId,
    const uint64_t& aDatastoreId) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  if (NS_WARN_IF(!gPreparedDatastores)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  PreparedDatastore* preparedDatastore = gPreparedDatastores->Get(aDatastoreId);
  if (NS_WARN_IF(!preparedDatastore)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // If we ever decide to return null from this point on, we need to make sure
  // that the datastore is closed and the prepared datastore is removed from the
  // gPreparedDatastores hashtable.
  // We also assume that IPDL must call RecvPBackgroundLSDatabaseConstructor
  // once we return a valid actor in this method.

  RefPtr<Database> database =
      new Database(aPrincipalInfo, preparedDatastore->GetContentParentId(),
                   preparedDatastore->Origin(), aPrivateBrowsingId);

  // Transfer ownership to IPDL.
  return database.forget().take();
}

bool RecvPBackgroundLSDatabaseConstructor(PBackgroundLSDatabaseParent* aActor,
                                          const PrincipalInfo& aPrincipalInfo,
                                          const uint32_t& aPrivateBrowsingId,
                                          const uint64_t& aDatastoreId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(gPreparedDatastores);
  MOZ_ASSERT(gPreparedDatastores->Get(aDatastoreId));
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built (it has a manager, channel and it's
  // registered as a subprotocol).
  // ActorDestroy will be called if we fail here.

  nsAutoPtr<PreparedDatastore> preparedDatastore;
  gPreparedDatastores->Remove(aDatastoreId, &preparedDatastore);
  MOZ_ASSERT(preparedDatastore);

  auto* database = static_cast<Database*>(aActor);

  database->SetActorAlive(preparedDatastore->GetDatastore());

  // It's possible that AbortOperations was called before the database actor
  // was created and became live. Let the child know that the database in no
  // longer valid.
  if (preparedDatastore->IsInvalidated()) {
    database->RequestAllowToClose();
  }

  return true;
}

bool DeallocPBackgroundLSDatabaseParent(PBackgroundLSDatabaseParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Database> actor = dont_AddRef(static_cast<Database*>(aActor));

  return true;
}

PBackgroundLSObserverParent* AllocPBackgroundLSObserverParent(
    const uint64_t& aObserverId) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  if (NS_WARN_IF(!gPreparedObsevers)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<Observer> observer = gPreparedObsevers->Get(aObserverId);
  if (NS_WARN_IF(!observer)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // observer->SetObject(this);

  // Transfer ownership to IPDL.
  return observer.forget().take();
}

bool RecvPBackgroundLSObserverConstructor(PBackgroundLSObserverParent* aActor,
                                          const uint64_t& aObserverId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(gPreparedObsevers);
  MOZ_ASSERT(gPreparedObsevers->GetWeak(aObserverId));

  RefPtr<Observer> observer;
  gPreparedObsevers->Remove(aObserverId, observer.StartAssignment());
  MOZ_ASSERT(observer);

  if (!gPreparedObsevers->Count()) {
    gPreparedObsevers = nullptr;
  }

  if (!gObservers) {
    gObservers = new ObserverHashtable();
  }

  nsTArray<Observer*>* array;
  if (!gObservers->Get(observer->Origin(), &array)) {
    array = new nsTArray<Observer*>();
    gObservers->Put(observer->Origin(), array);
  }
  array->AppendElement(observer);

  return true;
}

bool DeallocPBackgroundLSObserverParent(PBackgroundLSObserverParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Observer> actor = dont_AddRef(static_cast<Observer*>(aActor));

  return true;
}

PBackgroundLSRequestParent* AllocPBackgroundLSRequestParent(
    PBackgroundParent* aBackgroundActor, const LSRequestParams& aParams) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  // If we're in the same process as the actor, we need to get the target event
  // queue from the current RequestHelper.
  nsCOMPtr<nsIEventTarget> mainEventTarget;
  if (!BackgroundParent::IsOtherProcessActor(aBackgroundActor)) {
    mainEventTarget = LSObject::GetSyncLoopEventTarget();
  }

  RefPtr<LSRequestBase> actor;

  switch (aParams.type()) {
    case LSRequestParams::TLSRequestPrepareDatastoreParams: {
      RefPtr<ContentParent> contentParent =
          BackgroundParent::GetContentParent(aBackgroundActor);

      RefPtr<PrepareDatastoreOp> prepareDatastoreOp = new PrepareDatastoreOp(
          mainEventTarget, contentParent.forget(), aParams);

      if (!gPrepareDatastoreOps) {
        gPrepareDatastoreOps = new PrepareDatastoreOpArray();
      }
      gPrepareDatastoreOps->AppendElement(prepareDatastoreOp);

      actor = std::move(prepareDatastoreOp);

      break;
    }

    case LSRequestParams::TLSRequestPrepareObserverParams: {
      RefPtr<PrepareObserverOp> prepareObserverOp =
          new PrepareObserverOp(mainEventTarget, aParams);

      actor = std::move(prepareObserverOp);

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool RecvPBackgroundLSRequestConstructor(PBackgroundLSRequestParent* aActor,
                                         const LSRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != LSRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built.

  auto* op = static_cast<LSRequestBase*>(aActor);

  op->Dispatch();

  return true;
}

bool DeallocPBackgroundLSRequestParent(PBackgroundLSRequestParent* aActor) {
  AssertIsOnBackgroundThread();

  // Transfer ownership back from IPDL.
  RefPtr<LSRequestBase> actor =
      dont_AddRef(static_cast<LSRequestBase*>(aActor));

  return true;
}

PBackgroundLSSimpleRequestParent* AllocPBackgroundLSSimpleRequestParent(
    const LSSimpleRequestParams& aParams) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  RefPtr<LSSimpleRequestBase> actor;

  switch (aParams.type()) {
    case LSSimpleRequestParams::TLSSimpleRequestPreloadedParams: {
      RefPtr<PreloadedOp> preloadedOp = new PreloadedOp(aParams);

      actor = std::move(preloadedOp);

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool RecvPBackgroundLSSimpleRequestConstructor(
    PBackgroundLSSimpleRequestParent* aActor,
    const LSSimpleRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != LSSimpleRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built.

  auto* op = static_cast<LSSimpleRequestBase*>(aActor);

  op->Dispatch();

  return true;
}

bool DeallocPBackgroundLSSimpleRequestParent(
    PBackgroundLSSimpleRequestParent* aActor) {
  AssertIsOnBackgroundThread();

  // Transfer ownership back from IPDL.
  RefPtr<LSSimpleRequestBase> actor =
      dont_AddRef(static_cast<LSSimpleRequestBase*>(aActor));

  return true;
}

namespace localstorage {

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  RefPtr<QuotaClient> client = new QuotaClient();
  return client.forget();
}

}  // namespace localstorage

/*******************************************************************************
 * WriteOptimizer
 ******************************************************************************/

void WriteOptimizer::AddItem(const nsString& aKey, const nsString& aValue) {
  AssertIsOnBackgroundThread();

  WriteInfo* existingWriteInfo;
  nsAutoPtr<WriteInfo> newWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::RemoveItem) {
    newWriteInfo = new UpdateItemInfo(aKey, aValue);
  } else {
    newWriteInfo = new AddItemInfo(aKey, aValue);
  }
  mWriteInfos.Put(aKey, newWriteInfo.forget());
}

void WriteOptimizer::UpdateItem(const nsString& aKey, const nsString& aValue) {
  AssertIsOnBackgroundThread();

  WriteInfo* existingWriteInfo;
  nsAutoPtr<WriteInfo> newWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::AddItem) {
    newWriteInfo = new AddItemInfo(aKey, aValue);
  } else {
    newWriteInfo = new UpdateItemInfo(aKey, aValue);
  }
  mWriteInfos.Put(aKey, newWriteInfo.forget());
}

void WriteOptimizer::RemoveItem(const nsString& aKey) {
  AssertIsOnBackgroundThread();

  WriteInfo* existingWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::AddItem) {
    mWriteInfos.Remove(aKey);
    return;
  }

  nsAutoPtr<WriteInfo> newWriteInfo(new RemoveItemInfo(aKey));
  mWriteInfos.Put(aKey, newWriteInfo.forget());
}

void WriteOptimizer::Clear() {
  AssertIsOnBackgroundThread();

  mWriteInfos.Clear();

  if (!mClearInfo) {
    mClearInfo = new ClearInfo();
  }
}

void WriteOptimizer::ApplyWrites(nsTArray<LSItemInfo>& aOrderedItems) {
  AssertIsOnBackgroundThread();

  if (mClearInfo) {
    aOrderedItems.Clear();
    mClearInfo = nullptr;
  }

  for (int32_t index = aOrderedItems.Length() - 1; index >= 0; index--) {
    LSItemInfo& item = aOrderedItems[index];

    if (auto entry = mWriteInfos.Lookup(item.key())) {
      WriteInfo* writeInfo = entry.Data();

      switch (writeInfo->GetType()) {
        case WriteInfo::RemoveItem:
          aOrderedItems.RemoveElementAt(index);
          entry.Remove();
          break;

        case WriteInfo::UpdateItem: {
          auto updateItemInfo = static_cast<UpdateItemInfo*>(writeInfo);
          item.value() = updateItemInfo->GetValue();
          entry.Remove();
          break;
        }

        case WriteInfo::AddItem:
          break;

        default:
          MOZ_CRASH("Bad type!");
      }
    }
  }

  for (auto iter = mWriteInfos.ConstIter(); !iter.Done(); iter.Next()) {
    WriteInfo* writeInfo = iter.Data();

    MOZ_ASSERT(writeInfo->GetType() == WriteInfo::AddItem);

    auto addItemInfo = static_cast<AddItemInfo*>(writeInfo);

    LSItemInfo* itemInfo = aOrderedItems.AppendElement();
    itemInfo->key() = addItemInfo->GetKey();
    itemInfo->value() = addItemInfo->GetValue();
  }

  mWriteInfos.Clear();
}

nsresult WriteOptimizer::PerformWrites(Connection* aConnection,
                                       bool aShadowWrites) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  nsresult rv;

  if (mClearInfo) {
    rv = mClearInfo->Perform(aConnection, aShadowWrites);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  for (auto iter = mWriteInfos.ConstIter(); !iter.Done(); iter.Next()) {
    rv = iter.Data()->Perform(aConnection, aShadowWrites);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult WriteOptimizer::AddItemInfo::Perform(Connection* aConnection,
                                              bool aShadowWrites) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  Connection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("INSERT OR REPLACE INTO data (key, value) "
                         "VALUES(:key, :value)"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), mKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("value"), mValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aShadowWrites) {
    return NS_OK;
  }

  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING(
          "INSERT OR REPLACE INTO shadow.webappsstore2 "
          "(originAttributes, originKey, scope, key, value) "
          "VALUES (:originAttributes, :originKey, :scope, :key, :value) "),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ArchivedOriginScope* archivedOriginScope =
      aConnection->GetArchivedOriginScope();

  rv = archivedOriginScope->BindToStatement(stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString scope = Scheme0Scope(archivedOriginScope->OriginSuffix(),
                                 archivedOriginScope->OriginNoSuffix());

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"), scope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), mKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("value"), mValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult WriteOptimizer::RemoveItemInfo::Perform(Connection* aConnection,
                                                 bool aShadowWrites) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  Connection::CachedStatement stmt;
  nsresult rv =
      aConnection->GetCachedStatement(NS_LITERAL_CSTRING("DELETE FROM data "
                                                         "WHERE key = :key;"),
                                      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), mKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aShadowWrites) {
    return NS_OK;
  }

  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM shadow.webappsstore2 "
                         "WHERE originAttributes = :originAttributes "
                         "AND originKey = :originKey "
                         "AND key = :key;"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->GetArchivedOriginScope()->BindToStatement(stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), mKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult WriteOptimizer::ClearInfo::Perform(Connection* aConnection,
                                            bool aShadowWrites) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  Connection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM data;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aShadowWrites) {
    return NS_OK;
  }

  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM shadow.webappsstore2 "
                         "WHERE originAttributes = :originAttributes "
                         "AND originKey = :originKey;"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->GetArchivedOriginScope()->BindToStatement(stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/*******************************************************************************
 * DatastoreOperationBase
 ******************************************************************************/

/*******************************************************************************
 * ConnectionDatastoreOperationBase
 ******************************************************************************/

ConnectionDatastoreOperationBase::ConnectionDatastoreOperationBase(
    Connection* aConnection)
    : mConnection(aConnection) {
  MOZ_ASSERT(aConnection);
}

ConnectionDatastoreOperationBase::~ConnectionDatastoreOperationBase() {
  MOZ_ASSERT(!mConnection,
             "ConnectionDatabaseOperationBase::Cleanup() was not called by a "
             "subclass!");
}

void ConnectionDatastoreOperationBase::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  mConnection = nullptr;

  NoteComplete();
}

void ConnectionDatastoreOperationBase::OnSuccess() { AssertIsOnOwningThread(); }

void ConnectionDatastoreOperationBase::OnFailure(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));
}

void ConnectionDatastoreOperationBase::RunOnConnectionThread() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

  if (!MayProceedOnNonOwningThread()) {
    SetFailureCode(NS_ERROR_FAILURE);
  } else {
    nsresult rv = mConnection->EnsureStorageConnection();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SetFailureCode(rv);
    } else {
      MOZ_ASSERT(mConnection->StorageConnection());

      rv = DoDatastoreWork();
      if (NS_FAILED(rv)) {
        SetFailureCode(rv);
      }
    }
  }

  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));
}

void ConnectionDatastoreOperationBase::RunOnOwningThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  } else {
    if (NS_SUCCEEDED(ResultCode())) {
      OnSuccess();
    } else {
      OnFailure(ResultCode());
    }
  }

  Cleanup();
}

NS_IMETHODIMP
ConnectionDatastoreOperationBase::Run() {
  if (IsOnConnectionThread()) {
    RunOnConnectionThread();
  } else {
    RunOnOwningThread();
  }

  return NS_OK;
}

/*******************************************************************************
 * Connection implementation
 ******************************************************************************/

Connection::Connection(ConnectionThread* aConnectionThread,
                       const nsACString& aOrigin, const nsAString& aFilePath,
                       nsAutoPtr<ArchivedOriginScope>&& aArchivedOriginScope)
    : mConnectionThread(aConnectionThread),
      mArchivedOriginScope(std::move(aArchivedOriginScope)),
      mOrigin(aOrigin),
      mFilePath(aFilePath),
      mFlushScheduled(false)
#ifdef DEBUG
      ,
      mInUpdateBatch(false)
#endif
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(!aFilePath.IsEmpty());
}

Connection::~Connection() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mStorageConnection);
  MOZ_ASSERT(!mCachedStatements.Count());
  MOZ_ASSERT(!mInUpdateBatch);
  MOZ_ASSERT(!mFlushScheduled);
}

void Connection::Dispatch(ConnectionDatastoreOperationBase* aOp) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnectionThread);

  MOZ_ALWAYS_SUCCEEDS(
      mConnectionThread->mThread->Dispatch(aOp, NS_DISPATCH_NORMAL));
}

void Connection::Close(nsIRunnable* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);

  if (mFlushScheduled) {
    MOZ_ASSERT(mFlushTimer);
    MOZ_ALWAYS_SUCCEEDS(mFlushTimer->Cancel());

    Flush();

    mFlushTimer = nullptr;
  }

  RefPtr<CloseOp> op = new CloseOp(this, aCallback);

  Dispatch(op);
}

void Connection::AddItem(const nsString& aKey, const nsString& aValue) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  mWriteOptimizer.AddItem(aKey, aValue);
}

void Connection::UpdateItem(const nsString& aKey, const nsString& aValue) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  mWriteOptimizer.UpdateItem(aKey, aValue);
}

void Connection::RemoveItem(const nsString& aKey) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  mWriteOptimizer.RemoveItem(aKey);
}

void Connection::Clear() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  mWriteOptimizer.Clear();
}

void Connection::BeginUpdateBatch() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mInUpdateBatch);

#ifdef DEBUG
  mInUpdateBatch = true;
#endif
}

void Connection::EndUpdateBatch() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  if (mWriteOptimizer.HasWrites() && !mFlushScheduled) {
    ScheduleFlush();
  }

#ifdef DEBUG
  mInUpdateBatch = false;
#endif
}

nsresult Connection::EnsureStorageConnection() {
  AssertIsOnConnectionThread();

  if (!mStorageConnection) {
    nsCOMPtr<mozIStorageConnection> storageConnection;
    nsresult rv =
        GetStorageConnection(mFilePath, getter_AddRefs(storageConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mStorageConnection = storageConnection;
  }

  return NS_OK;
}

void Connection::CloseStorageConnection() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);

  mCachedStatements.Clear();

  MOZ_ALWAYS_SUCCEEDS(mStorageConnection->Close());
  mStorageConnection = nullptr;
}

nsresult Connection::GetCachedStatement(const nsACString& aQuery,
                                        CachedStatement* aCachedStatement) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aQuery.IsEmpty());
  MOZ_ASSERT(aCachedStatement);
  MOZ_ASSERT(mStorageConnection);

  nsCOMPtr<mozIStorageStatement> stmt;

  if (!mCachedStatements.Get(aQuery, getter_AddRefs(stmt))) {
    nsresult rv =
        mStorageConnection->CreateStatement(aQuery, getter_AddRefs(stmt));
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      nsCString msg;
      MOZ_ALWAYS_SUCCEEDS(mStorageConnection->GetLastErrorString(msg));

      nsAutoCString error =
          NS_LITERAL_CSTRING("The statement '") + aQuery +
          NS_LITERAL_CSTRING("' failed to compile with the error message '") +
          msg + NS_LITERAL_CSTRING("'.");

      NS_WARNING(error.get());
#endif
      return rv;
    }

    mCachedStatements.Put(aQuery, stmt);
  }

  aCachedStatement->Assign(this, stmt.forget());
  return NS_OK;
}

void Connection::ScheduleFlush() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mWriteOptimizer.HasWrites());
  MOZ_ASSERT(!mFlushScheduled);

  if (!mFlushTimer) {
    mFlushTimer = NS_NewTimer();
    MOZ_ASSERT(mFlushTimer);
  }

  MOZ_ALWAYS_SUCCEEDS(mFlushTimer->InitWithNamedFuncCallback(
      FlushTimerCallback, this, kFlushTimeoutMs, nsITimer::TYPE_ONE_SHOT,
      "Connection::FlushTimerCallback"));

  mFlushScheduled = true;
}

void Connection::Flush() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFlushScheduled);

  if (mWriteOptimizer.HasWrites()) {
    RefPtr<FlushOp> op = new FlushOp(this, std::move(mWriteOptimizer));

    Dispatch(op);
  }

  mFlushScheduled = false;
}

// static
void Connection::FlushTimerCallback(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(aClosure);

  auto* self = static_cast<Connection*>(aClosure);
  MOZ_ASSERT(self);
  MOZ_ASSERT(self->mFlushScheduled);

  self->Flush();
}

Connection::CachedStatement::CachedStatement() {
  AssertIsOnConnectionThread();

  MOZ_COUNT_CTOR(Connection::CachedStatement);
}

Connection::CachedStatement::~CachedStatement() {
  AssertIsOnConnectionThread();

  MOZ_COUNT_DTOR(Connection::CachedStatement);
}

Connection::CachedStatement::operator mozIStorageStatement*() const {
  AssertIsOnConnectionThread();

  return mStatement;
}

mozIStorageStatement* Connection::CachedStatement::operator->() const {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStatement);

  return mStatement;
}

void Connection::CachedStatement::Assign(
    Connection* aConnection,
    already_AddRefed<mozIStorageStatement> aStatement) {
  AssertIsOnConnectionThread();

  mScoper.reset();

  mStatement = aStatement;

  if (mStatement) {
    mScoper.emplace(mStatement);
  }
}

Connection::FlushOp::FlushOp(Connection* aConnection,
                             WriteOptimizer&& aWriteOptimizer)
    : ConnectionDatastoreOperationBase(aConnection),
      mQuotaClient(QuotaClient::GetInstance()),
      mWriteOptimizer(std::move(aWriteOptimizer)),
      mShadowWrites(gShadowWrites) {
  MOZ_ASSERT(mQuotaClient);
}

nsresult Connection::FlushOp::DoDatastoreWork() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<mozIStorageConnection> storageConnection =
      mConnection->StorageConnection();
  MOZ_ASSERT(storageConnection);

  nsresult rv;

  Maybe<MutexAutoLock> shadowDatabaseLock;

  if (mShadowWrites) {
    MOZ_ASSERT(mQuotaClient);

    shadowDatabaseLock.emplace(mQuotaClient->ShadowDatabaseMutex());

    rv = AttachShadowDatabase(quotaManager->GetBasePath(), storageConnection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  CachedStatement stmt;
  rv = mConnection->GetCachedStatement(NS_LITERAL_CSTRING("BEGIN IMMEDIATE;"),
                                       &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mWriteOptimizer.PerformWrites(mConnection, mShadowWrites);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mConnection->GetCachedStatement(NS_LITERAL_CSTRING("COMMIT;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mShadowWrites) {
    rv = DetachShadowDatabase(storageConnection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult Connection::CloseOp::DoDatastoreWork() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  mConnection->CloseStorageConnection();

  return NS_OK;
}

void Connection::CloseOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  mConnection->mConnectionThread->mConnections.Remove(mConnection->mOrigin);

  nsCOMPtr<nsIRunnable> callback;
  mCallback.swap(callback);

  callback->Run();

  ConnectionDatastoreOperationBase::Cleanup();
}

/*******************************************************************************
 * ConnectionThread implementation
 ******************************************************************************/

ConnectionThread::ConnectionThread() {
  AssertIsOnOwningThread();
  AssertIsOnBackgroundThread();

  MOZ_ALWAYS_SUCCEEDS(NS_NewNamedThread("LS Thread", getter_AddRefs(mThread)));
}

ConnectionThread::~ConnectionThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mConnections.Count());
}

bool ConnectionThread::IsOnConnectionThread() {
  MOZ_ASSERT(mThread);

  bool current;
  return NS_SUCCEEDED(mThread->IsOnCurrentThread(&current)) && current;
}

void ConnectionThread::AssertIsOnConnectionThread() {
  MOZ_ASSERT(IsOnConnectionThread());
}

already_AddRefed<Connection> ConnectionThread::CreateConnection(
    const nsACString& aOrigin, const nsAString& aFilePath,
    nsAutoPtr<ArchivedOriginScope>&& aArchivedOriginScope) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(!mConnections.GetWeak(aOrigin));

  RefPtr<Connection> connection =
      new Connection(this, aOrigin, aFilePath, std::move(aArchivedOriginScope));
  mConnections.Put(aOrigin, connection);

  return connection.forget();
}

void ConnectionThread::Shutdown() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mThread);

  mThread->Shutdown();
}

/*******************************************************************************
 * Datastore
 ******************************************************************************/

Datastore::Datastore(const nsACString& aOrigin, uint32_t aPrivateBrowsingId,
                     int64_t aUsage, int64_t aSizeOfKeys, int64_t aSizeOfItems,
                     already_AddRefed<DirectoryLock>&& aDirectoryLock,
                     already_AddRefed<Connection>&& aConnection,
                     already_AddRefed<QuotaObject>&& aQuotaObject,
                     nsDataHashtable<nsStringHashKey, nsString>& aValues,
                     nsTArray<LSItemInfo>& aOrderedItems)
    : mDirectoryLock(std::move(aDirectoryLock)),
      mConnection(std::move(aConnection)),
      mQuotaObject(std::move(aQuotaObject)),
      mOrigin(aOrigin),
      mPrivateBrowsingId(aPrivateBrowsingId),
      mUsage(aUsage),
      mUpdateBatchUsage(-1),
      mSizeOfKeys(aSizeOfKeys),
      mSizeOfItems(aSizeOfItems),
      mClosed(false)
#ifdef DEBUG
      ,
      mInUpdateBatch(false)
#endif
{
  AssertIsOnBackgroundThread();

  mValues.SwapElements(aValues);
  mOrderedItems.SwapElements(aOrderedItems);
}

Datastore::~Datastore() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mClosed);
}

void Datastore::Close() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mDatabases.Count());
  MOZ_ASSERT(mDirectoryLock);

  mClosed = true;

  if (IsPersistent()) {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(mQuotaObject);

    // We can't release the directory lock and unregister itself from the
    // hashtable until the connection is fully closed.
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("dom::Datastore::ConnectionClosedCallback", this,
                          &Datastore::ConnectionClosedCallback);
    mConnection->Close(callback);
  } else {
    MOZ_ASSERT(!mConnection);
    MOZ_ASSERT(!mQuotaObject);

    // There's no connection, so it's safe to release the directory lock and
    // unregister itself from the hashtable.

    mDirectoryLock = nullptr;

    CleanupMetadata();
  }
}

void Datastore::WaitForConnectionToComplete(nsIRunnable* aCallback) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mCompleteCallback);
  MOZ_ASSERT(mClosed);

  mCompleteCallback = aCallback;
}

void Datastore::NoteLivePrepareDatastoreOp(
    PrepareDatastoreOp* aPrepareDatastoreOp) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPrepareDatastoreOp);
  MOZ_ASSERT(!mPrepareDatastoreOps.GetEntry(aPrepareDatastoreOp));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPrepareDatastoreOps.PutEntry(aPrepareDatastoreOp);
}

void Datastore::NoteFinishedPrepareDatastoreOp(
    PrepareDatastoreOp* aPrepareDatastoreOp) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOps.GetEntry(aPrepareDatastoreOp));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPrepareDatastoreOps.RemoveEntry(aPrepareDatastoreOp);

  MaybeClose();
}

void Datastore::NoteLivePreparedDatastore(
    PreparedDatastore* aPreparedDatastore) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPreparedDatastore);
  MOZ_ASSERT(!mPreparedDatastores.GetEntry(aPreparedDatastore));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPreparedDatastores.PutEntry(aPreparedDatastore);
}

void Datastore::NoteFinishedPreparedDatastore(
    PreparedDatastore* aPreparedDatastore) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPreparedDatastore);
  MOZ_ASSERT(mPreparedDatastores.GetEntry(aPreparedDatastore));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPreparedDatastores.RemoveEntry(aPreparedDatastore);

  MaybeClose();
}

#ifdef DEBUG
bool Datastore::HasLivePreparedDatastores() const {
  AssertIsOnBackgroundThread();

  return mPreparedDatastores.Count();
}
#endif

void Datastore::NoteLiveDatabase(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mDatabases.PutEntry(aDatabase);
}

void Datastore::NoteFinishedDatabase(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(!mActiveDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mDatabases.RemoveEntry(aDatabase);

  MaybeClose();
}

#ifdef DEBUG
bool Datastore::HasLiveDatabases() const {
  AssertIsOnBackgroundThread();

  return mDatabases.Count();
}
#endif

void Datastore::NoteActiveDatabase(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(!mActiveDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(!mClosed);

  mActiveDatabases.PutEntry(aDatabase);
}

void Datastore::NoteInactiveDatabase(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(mActiveDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(!mClosed);

  mActiveDatabases.RemoveEntry(aDatabase);

  if (!mActiveDatabases.Count() && mPendingUsageDeltas.Length()) {
    int64_t finalDelta = 0;

    for (auto delta : mPendingUsageDeltas) {
      finalDelta += delta;
    }

    MOZ_ASSERT(finalDelta <= 0);

    if (finalDelta != 0) {
      DebugOnly<bool> ok = UpdateUsage(finalDelta);
      MOZ_ASSERT(ok);
    }

    mPendingUsageDeltas.Clear();
  }
}

void Datastore::GetSnapshotInitInfo(nsTHashtable<nsStringHashKey>& aLoadedItems,
                                    nsTArray<LSItemInfo>& aItemInfos,
                                    uint32_t& aTotalLength,
                                    int64_t& aInitialUsage, int64_t& aPeakUsage,
                                    LSSnapshot::LoadState& aLoadState) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mInUpdateBatch);

#ifdef DEBUG
  int64_t sizeOfKeys = 0;
  int64_t sizeOfItems = 0;
  for (auto item : mOrderedItems) {
    int64_t sizeOfKey = static_cast<int64_t>(item.key().Length());
    sizeOfKeys += sizeOfKey;
    sizeOfItems += sizeOfKey + static_cast<int64_t>(item.value().Length());
  }
  MOZ_ASSERT(mSizeOfKeys == sizeOfKeys);
  MOZ_ASSERT(mSizeOfItems == sizeOfItems);
#endif

  int64_t size = 0;
  if (mSizeOfKeys <= gSnapshotPrefill) {
    if (mSizeOfItems <= gSnapshotPrefill) {
      aItemInfos.AppendElements(mOrderedItems);
      aLoadState = LSSnapshot::LoadState::AllOrderedItems;
    } else {
      nsString value;
      for (auto item : mOrderedItems) {
        if (!value.IsVoid()) {
          value = item.value();

          size += static_cast<int64_t>(item.key().Length()) +
                  static_cast<int64_t>(value.Length());

          if (size <= gSnapshotPrefill) {
            aLoadedItems.PutEntry(item.key());
          } else {
            value.SetIsVoid(true);
          }
        }

        LSItemInfo* itemInfo = aItemInfos.AppendElement();
        itemInfo->key() = item.key();
        itemInfo->value() = value;
      }

      aLoadState = LSSnapshot::LoadState::AllOrderedKeys;
    }
  } else {
    for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
      const nsAString& key = iter.Key();
      const nsString& value = iter.Data();

      size += static_cast<int64_t>(key.Length()) +
              static_cast<int64_t>(value.Length());

      if (size > gSnapshotPrefill) {
        break;
      }

      aLoadedItems.PutEntry(key);

      LSItemInfo* itemInfo = aItemInfos.AppendElement();
      itemInfo->key() = iter.Key();
      itemInfo->value() = iter.Data();
    }

    MOZ_ASSERT(aItemInfos.Length() < mOrderedItems.Length());
    aLoadState = LSSnapshot::LoadState::Partial;
  }

  aTotalLength = mValues.Count();

  aInitialUsage = mUsage;
  aPeakUsage = aInitialUsage;
}

void Datastore::GetItem(const nsString& aKey, nsString& aValue) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  if (!mValues.Get(aKey, &aValue)) {
    aValue.SetIsVoid(true);
  }
}

void Datastore::GetKeys(nsTArray<nsString>& aKeys) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  for (auto item : mOrderedItems) {
    aKeys.AppendElement(item.key());
  }
}

void Datastore::SetItem(Database* aDatabase, const nsString& aDocumentURI,
                        const nsString& aKey, const nsString& aOldValue,
                        const nsString& aValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mInUpdateBatch);

  nsString oldValue;
  GetItem(aKey, oldValue);

  if (oldValue != aValue || oldValue.IsVoid() != aValue.IsVoid()) {
    bool isNewItem = oldValue.IsVoid();

    NotifySnapshots(aDatabase, aKey, oldValue, /* affectsOrder */ isNewItem);

    mValues.Put(aKey, aValue);

    if (isNewItem) {
      mWriteOptimizer.AddItem(aKey, aValue);

      int64_t sizeOfKey = static_cast<int64_t>(aKey.Length());
      int64_t sizeOfItem = sizeOfKey + static_cast<int64_t>(aValue.Length());

      mUpdateBatchUsage += sizeOfItem;

      mSizeOfKeys += sizeOfKey;
      mSizeOfItems += sizeOfItem;
    } else {
      mWriteOptimizer.UpdateItem(aKey, aValue);

      int64_t delta = static_cast<int64_t>(aValue.Length()) -
                      static_cast<int64_t>(oldValue.Length());

      mUpdateBatchUsage += delta;

      mSizeOfItems += delta;
    }

    if (IsPersistent()) {
      if (oldValue.IsVoid()) {
        mConnection->AddItem(aKey, aValue);
      } else {
        mConnection->UpdateItem(aKey, aValue);
      }
    }
  }

  NotifyObservers(aDatabase, aDocumentURI, aKey, aOldValue, aValue);
}

void Datastore::RemoveItem(Database* aDatabase, const nsString& aDocumentURI,
                           const nsString& aKey, const nsString& aOldValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mInUpdateBatch);

  nsString oldValue;
  GetItem(aKey, oldValue);

  if (!oldValue.IsVoid()) {
    NotifySnapshots(aDatabase, aKey, oldValue, /* aAffectsOrder */ true);

    mValues.Remove(aKey);

    mWriteOptimizer.RemoveItem(aKey);

    int64_t sizeOfKey = static_cast<int64_t>(aKey.Length());
    int64_t sizeOfItem = sizeOfKey + static_cast<int64_t>(oldValue.Length());

    mUpdateBatchUsage -= sizeOfItem;

    mSizeOfKeys -= sizeOfKey;
    mSizeOfItems -= sizeOfItem;

    if (IsPersistent()) {
      mConnection->RemoveItem(aKey);
    }
  }

  NotifyObservers(aDatabase, aDocumentURI, aKey, aOldValue, VoidString());
}

void Datastore::Clear(Database* aDatabase, const nsString& aDocumentURI) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mInUpdateBatch);

  if (mValues.Count()) {
    int64_t updateBatchUsage = mUpdateBatchUsage;
    for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
      const nsAString& key = iter.Key();
      const nsAString& value = iter.Data();

      updateBatchUsage -= (static_cast<int64_t>(key.Length()) +
                           static_cast<int64_t>(value.Length()));

      NotifySnapshots(aDatabase, key, value, /* aAffectsOrder */ true);
    }

    mValues.Clear();

    mWriteOptimizer.Clear();

    mUpdateBatchUsage = updateBatchUsage;

    mSizeOfKeys = 0;
    mSizeOfItems = 0;

    if (IsPersistent()) {
      mConnection->Clear();
    }
  }

  NotifyObservers(aDatabase, aDocumentURI, VoidString(), VoidString(),
                  VoidString());
}

void Datastore::PrivateBrowsingClear() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mPrivateBrowsingId);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mInUpdateBatch);

  if (mValues.Count()) {
    MarkSnapshotsDirty();

    mValues.Clear();

    mOrderedItems.Clear();

    DebugOnly<bool> ok = UpdateUsage(-mSizeOfItems);
    MOZ_ASSERT(ok);

    mSizeOfKeys = 0;
    mSizeOfItems = 0;
  }
}

void Datastore::BeginUpdateBatch(int64_t aSnapshotInitialUsage) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSnapshotInitialUsage >= 0);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mUpdateBatchUsage == -1);
  MOZ_ASSERT(!mInUpdateBatch);

  mUpdateBatchUsage = aSnapshotInitialUsage;

  if (IsPersistent()) {
    mConnection->BeginUpdateBatch();
  }

#ifdef DEBUG
  mInUpdateBatch = true;
#endif
}

int64_t Datastore::EndUpdateBatch(int64_t aSnapshotPeakUsage) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mInUpdateBatch);

  mWriteOptimizer.ApplyWrites(mOrderedItems);

  if (aSnapshotPeakUsage >= 0) {
    int64_t delta = mUpdateBatchUsage - aSnapshotPeakUsage;

    if (mActiveDatabases.Count()) {
      // We can't apply deltas while other databases are still active.
      // The final delta must be zero or negative, but individual deltas can
      // be positive. A positive delta can't be applied asynchronously since
      // there's no way to fire the quota exceeded error event.

      mPendingUsageDeltas.AppendElement(delta);
    } else {
      MOZ_ASSERT(delta <= 0);
      if (delta != 0) {
        DebugOnly<bool> ok = UpdateUsage(delta);
        MOZ_ASSERT(ok);
      }
    }
  }

  int64_t result = mUpdateBatchUsage;
  mUpdateBatchUsage = -1;

  if (IsPersistent()) {
    mConnection->EndUpdateBatch();
  }

#ifdef DEBUG
  mInUpdateBatch = false;
#endif

  return result;
}

int64_t Datastore::RequestUpdateUsage(int64_t aRequestedSize,
                                      int64_t aMinSize) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aRequestedSize > 0);
  MOZ_ASSERT(aMinSize > 0);

  if (UpdateUsage(aRequestedSize)) {
    return aRequestedSize;
  }

  if (UpdateUsage(aMinSize)) {
    return aMinSize;
  }

  return 0;
}

bool Datastore::UpdateUsage(int64_t aDelta) {
  AssertIsOnBackgroundThread();

  // Check internal LocalStorage origin limit.
  int64_t newUsage = mUsage + aDelta;
  if (newUsage > gOriginLimitKB * 1024) {
    return false;
  }

  // Check QuotaManager limits (group and global limit).
  if (IsPersistent()) {
    MOZ_ASSERT(mQuotaObject);

    if (!mQuotaObject->MaybeUpdateSize(newUsage, /* aTruncate */ true)) {
      return false;
    }
  }

  // Quota checks passed, set new usage.

  mUsage = newUsage;

  if (IsPersistent()) {
    RefPtr<Runnable> runnable = NS_NewRunnableFunction(
        "Datastore::UpdateUsage", [origin = mOrigin, newUsage]() {
          MOZ_ASSERT(gUsages);
          MOZ_ASSERT(gUsages->Contains(origin));
          gUsages->Put(origin, newUsage);
        });

    QuotaManager* quotaManager = QuotaManager::Get();
    MOZ_ASSERT(quotaManager);

    MOZ_ALWAYS_SUCCEEDS(
        quotaManager->IOThread()->Dispatch(runnable, NS_DISPATCH_NORMAL));
  }

  return true;
}

void Datastore::MaybeClose() {
  AssertIsOnBackgroundThread();

  if (!mPrepareDatastoreOps.Count() && !mPreparedDatastores.Count() &&
      !mDatabases.Count()) {
    Close();
  }
}

void Datastore::ConnectionClosedCallback() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mQuotaObject);
  MOZ_ASSERT(mClosed);

  // Release the quota object first.
  mQuotaObject = nullptr;

  // Now it's safe to release the directory lock and unregister itself from
  // the hashtable.

  mDirectoryLock = nullptr;
  mConnection = nullptr;

  CleanupMetadata();

  if (mCompleteCallback) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mCompleteCallback.forget()));
  }
}

void Datastore::CleanupMetadata() {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(gDatastores);
  MOZ_ASSERT(gDatastores->Get(mOrigin));
  gDatastores->Remove(mOrigin);

  if (!gDatastores->Count()) {
    gDatastores = nullptr;
  }
}

void Datastore::NotifySnapshots(Database* aDatabase, const nsAString& aKey,
                                const nsAString& aOldValue,
                                bool aAffectsOrder) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);

  for (auto iter = mDatabases.ConstIter(); !iter.Done(); iter.Next()) {
    Database* database = iter.Get()->GetKey();
    if (database == aDatabase) {
      continue;
    }

    Snapshot* snapshot = database->GetSnapshot();
    if (snapshot) {
      snapshot->SaveItem(aKey, aOldValue, aAffectsOrder);
    }
  }
}

void Datastore::MarkSnapshotsDirty() {
  AssertIsOnBackgroundThread();

  for (auto iter = mDatabases.ConstIter(); !iter.Done(); iter.Next()) {
    Database* database = iter.Get()->GetKey();

    Snapshot* snapshot = database->GetSnapshot();
    if (snapshot) {
      snapshot->MarkDirty();
    }
  }
}

void Datastore::NotifyObservers(Database* aDatabase,
                                const nsString& aDocumentURI,
                                const nsString& aKey, const nsString& aOldValue,
                                const nsString& aNewValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);

  if (!gObservers) {
    return;
  }

  nsTArray<Observer*>* array;
  if (!gObservers->Get(mOrigin, &array)) {
    return;
  }

  MOZ_ASSERT(array);

  // We do not want to send information about events back to the content process
  // that caused the change.
  PBackgroundParent* databaseBackgroundActor = aDatabase->Manager();

  for (Observer* observer : *array) {
    if (observer->Manager() != databaseBackgroundActor) {
      observer->Observe(aDatabase, aDocumentURI, aKey, aOldValue, aNewValue);
    }
  }
}

/*******************************************************************************
 * PreparedDatastore
 ******************************************************************************/

void PreparedDatastore::Destroy() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(gPreparedDatastores);
  MOZ_ASSERT(gPreparedDatastores->Get(mDatastoreId));

  nsAutoPtr<PreparedDatastore> preparedDatastore;
  gPreparedDatastores->Remove(mDatastoreId, &preparedDatastore);
  MOZ_ASSERT(preparedDatastore);
}

// static
void PreparedDatastore::TimerCallback(nsITimer* aTimer, void* aClosure) {
  AssertIsOnBackgroundThread();

  auto* self = static_cast<PreparedDatastore*>(aClosure);
  MOZ_ASSERT(self);

  self->Destroy();
}

/*******************************************************************************
 * Database
 ******************************************************************************/

Database::Database(const PrincipalInfo& aPrincipalInfo,
                   const Maybe<ContentParentId>& aContentParentId,
                   const nsACString& aOrigin, uint32_t aPrivateBrowsingId)
    : mSnapshot(nullptr),
      mPrincipalInfo(aPrincipalInfo),
      mContentParentId(aContentParentId),
      mOrigin(aOrigin),
      mPrivateBrowsingId(aPrivateBrowsingId),
      mAllowedToClose(false),
      mActorDestroyed(false),
      mRequestedAllowToClose(false)
#ifdef DEBUG
      ,
      mActorWasAlive(false)
#endif
{
  AssertIsOnBackgroundThread();
}

Database::~Database() {
  MOZ_ASSERT_IF(mActorWasAlive, mAllowedToClose);
  MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);
}

void Database::SetActorAlive(Datastore* aDatastore) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorWasAlive = true;
#endif

  mDatastore = aDatastore;

  mDatastore->NoteLiveDatabase(this);

  if (!gLiveDatabases) {
    gLiveDatabases = new LiveDatabaseArray();
  }

  gLiveDatabases->AppendElement(this);
}

void Database::RegisterSnapshot(Snapshot* aSnapshot) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSnapshot);
  MOZ_ASSERT(!mSnapshot);
  MOZ_ASSERT(!mAllowedToClose);

  // Only one snapshot at a time is currently supported.
  mSnapshot = aSnapshot;

  mDatastore->NoteActiveDatabase(this);
}

void Database::UnregisterSnapshot(Snapshot* aSnapshot) {
  MOZ_ASSERT(aSnapshot);
  MOZ_ASSERT(mSnapshot == aSnapshot);

  mSnapshot = nullptr;

  mDatastore->NoteInactiveDatabase(this);
}

void Database::RequestAllowToClose() {
  AssertIsOnBackgroundThread();

  if (mRequestedAllowToClose) {
    return;
  }

  mRequestedAllowToClose = true;

  // Send the RequestAllowToClose message to the child to avoid racing with the
  // child actor. Except the case when the actor was already destroyed.
  if (mActorDestroyed) {
    MOZ_ASSERT(mAllowedToClose);
  } else {
    Unused << SendRequestAllowToClose();
  }
}

void Database::AllowToClose() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mAllowedToClose);
  MOZ_ASSERT(mDatastore);

  mAllowedToClose = true;

  mDatastore->NoteFinishedDatabase(this);

  mDatastore = nullptr;

  MOZ_ASSERT(gLiveDatabases);
  gLiveDatabases->RemoveElement(this);

  if (gLiveDatabases->IsEmpty()) {
    gLiveDatabases = nullptr;
  }
}

void Database::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!mAllowedToClose) {
    AllowToClose();
  }
}

mozilla::ipc::IPCResult Database::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSDatabaseParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Database::RecvAllowToClose() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  AllowToClose();

  return IPC_OK();
}

PBackgroundLSSnapshotParent* Database::AllocPBackgroundLSSnapshotParent(
    const nsString& aDocumentURI, const bool& aIncreasePeakUsage,
    const int64_t& aRequestedSize, const int64_t& aMinSize,
    LSSnapshotInitInfo* aInitInfo) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(aIncreasePeakUsage && aRequestedSize <= 0)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(aIncreasePeakUsage && aMinSize <= 0)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<Snapshot> snapshot = new Snapshot(this, aDocumentURI);

  // Transfer ownership to IPDL.
  return snapshot.forget().take();
}

mozilla::ipc::IPCResult Database::RecvPBackgroundLSSnapshotConstructor(
    PBackgroundLSSnapshotParent* aActor, const nsString& aDocumentURI,
    const bool& aIncreasePeakUsage, const int64_t& aRequestedSize,
    const int64_t& aMinSize, LSSnapshotInitInfo* aInitInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT_IF(aIncreasePeakUsage, aRequestedSize > 0);
  MOZ_ASSERT_IF(aIncreasePeakUsage, aMinSize > 0);
  MOZ_ASSERT(aInitInfo);
  MOZ_ASSERT(!mAllowedToClose);

  auto* snapshot = static_cast<Snapshot*>(aActor);

  // TODO: This can be optimized depending on which operation triggers snapshot
  //       creation. For example clear() doesn't need to receive items at all.
  nsTHashtable<nsStringHashKey> loadedItems;
  nsTArray<LSItemInfo> itemInfos;
  uint32_t totalLength;
  int64_t initialUsage;
  int64_t peakUsage;
  LSSnapshot::LoadState loadState;
  mDatastore->GetSnapshotInitInfo(loadedItems, itemInfos, totalLength,
                                  initialUsage, peakUsage, loadState);

  if (aIncreasePeakUsage) {
    int64_t size = mDatastore->RequestUpdateUsage(aRequestedSize, aMinSize);
    peakUsage += size;
  }

  snapshot->Init(loadedItems, totalLength, initialUsage, peakUsage, loadState);

  RegisterSnapshot(snapshot);

  aInitInfo->itemInfos() = std::move(itemInfos);
  aInitInfo->totalLength() = totalLength;
  aInitInfo->initialUsage() = initialUsage;
  aInitInfo->peakUsage() = peakUsage;
  aInitInfo->loadState() = loadState;

  return IPC_OK();
}

bool Database::DeallocPBackgroundLSSnapshotParent(
    PBackgroundLSSnapshotParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Snapshot> actor = dont_AddRef(static_cast<Snapshot*>(aActor));

  return true;
}

/*******************************************************************************
 * Snapshot
 ******************************************************************************/

Snapshot::Snapshot(Database* aDatabase, const nsAString& aDocumentURI)
    : mDatabase(aDatabase),
      mDatastore(aDatabase->GetDatastore()),
      mDocumentURI(aDocumentURI),
      mTotalLength(0),
      mUsage(-1),
      mPeakUsage(-1),
      mSavedKeys(false),
      mActorDestroyed(false),
      mFinishReceived(false),
      mLoadedReceived(false),
      mLoadedAllItems(false),
      mLoadKeysReceived(false),
      mSentMarkDirty(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
}

Snapshot::~Snapshot() {
  MOZ_ASSERT(mActorDestroyed);
  MOZ_ASSERT(mFinishReceived);
}

void Snapshot::SaveItem(const nsAString& aKey, const nsAString& aOldValue,
                        bool aAffectsOrder) {
  AssertIsOnBackgroundThread();

  MarkDirty();

  if (mLoadedAllItems) {
    return;
  }

  if (!mLoadedItems.GetEntry(aKey) && !mUnknownItems.GetEntry(aKey)) {
    nsString oldValue(aOldValue);
    mValues.LookupForAdd(aKey).OrInsert([oldValue]() { return oldValue; });
  }

  if (aAffectsOrder && !mSavedKeys && !mLoadKeysReceived) {
    mDatastore->GetKeys(mKeys);
    mSavedKeys = true;
  }
}

void Snapshot::MarkDirty() {
  AssertIsOnBackgroundThread();

  if (!mSentMarkDirty) {
    Unused << SendMarkDirty();
    mSentMarkDirty = true;
  }
}

void Snapshot::Finish() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mDatastore);
  MOZ_ASSERT(!mFinishReceived);

  mDatastore->BeginUpdateBatch(mUsage);

  mDatastore->EndUpdateBatch(mPeakUsage);

  mDatabase->UnregisterSnapshot(this);

  mFinishReceived = true;
}

void Snapshot::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!mFinishReceived) {
    Finish();
  }
}

mozilla::ipc::IPCResult Snapshot::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSSnapshotParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvCheckpoint(
    nsTArray<LSWriteInfo>&& aWriteInfos) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mUsage >= 0);
  MOZ_ASSERT(mPeakUsage >= mUsage);

  if (NS_WARN_IF(aWriteInfos.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->BeginUpdateBatch(mUsage);

  for (uint32_t index = 0; index < aWriteInfos.Length(); index++) {
    const LSWriteInfo& writeInfo = aWriteInfos[index];
    switch (writeInfo.type()) {
      case LSWriteInfo::TLSSetItemInfo: {
        const LSSetItemInfo& info = writeInfo.get_LSSetItemInfo();

        mDatastore->SetItem(mDatabase, mDocumentURI, info.key(),
                            info.oldValue(), info.value());

        break;
      }

      case LSWriteInfo::TLSRemoveItemInfo: {
        const LSRemoveItemInfo& info = writeInfo.get_LSRemoveItemInfo();

        mDatastore->RemoveItem(mDatabase, mDocumentURI, info.key(),
                               info.oldValue());

        break;
      }

      case LSWriteInfo::TLSClearInfo: {
        mDatastore->Clear(mDatabase, mDocumentURI);

        break;
      }

      default:
        MOZ_CRASH("Should never get here!");
    }
  }

  mUsage = mDatastore->EndUpdateBatch(-1);

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvFinish() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  Finish();

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvLoaded() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedAllItems)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadKeysReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mLoadedReceived = true;

  mLoadedItems.Clear();
  mUnknownItems.Clear();
  mValues.Clear();
  mKeys.Clear();
  mLoadedAllItems = true;
  mLoadKeysReceived = true;

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvLoadItem(const nsString& aKey,
                                               nsString* aValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aValue);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedAllItems)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (mLoadedItems.GetEntry(aKey) || mUnknownItems.GetEntry(aKey)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (auto entry = mValues.Lookup(aKey)) {
    *aValue = entry.Data();
    entry.Remove();
  } else {
    mDatastore->GetItem(aKey, *aValue);
  }

  if (aValue->IsVoid()) {
    mUnknownItems.PutEntry(aKey);
  } else {
    mLoadedItems.PutEntry(aKey);

    if (mLoadedItems.Count() == mTotalLength) {
      mLoadedItems.Clear();
      mUnknownItems.Clear();
#ifdef DEBUG
      for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
        MOZ_ASSERT(iter.Data().IsVoid());
      }
#endif
      mValues.Clear();
      mLoadedAllItems = true;
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvLoadKeys(nsTArray<nsString>* aKeys) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aKeys);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadKeysReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mLoadKeysReceived = true;

  if (mSavedKeys) {
    aKeys->AppendElements(std::move(mKeys));
  } else {
    mDatastore->GetKeys(*aKeys);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvIncreasePeakUsage(
    const int64_t& aRequestedSize, const int64_t& aMinSize, int64_t* aSize) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSize);

  if (NS_WARN_IF(aRequestedSize <= 0)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aMinSize <= 0)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  int64_t size = mDatastore->RequestUpdateUsage(aRequestedSize, aMinSize);

  mPeakUsage += size;

  *aSize = size;

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvPing() {
  AssertIsOnBackgroundThread();

  // Do nothing here. This is purely a sync message allowing the child to
  // confirm that the actor has received previous async message.

  return IPC_OK();
}

/*******************************************************************************
 * Observer
 ******************************************************************************/

Observer::Observer(const nsACString& aOrigin)
    : mOrigin(aOrigin), mActorDestroyed(false) {
  AssertIsOnBackgroundThread();
}

Observer::~Observer() { MOZ_ASSERT(mActorDestroyed); }

void Observer::Observe(Database* aDatabase, const nsString& aDocumentURI,
                       const nsString& aKey, const nsString& aOldValue,
                       const nsString& aNewValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);

  Unused << SendObserve(aDatabase->GetPrincipalInfo(),
                        aDatabase->PrivateBrowsingId(), aDocumentURI, aKey,
                        aOldValue, aNewValue);
}

void Observer::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  MOZ_ASSERT(gObservers);

  nsTArray<Observer*>* array;
  gObservers->Get(mOrigin, &array);
  MOZ_ASSERT(array);

  array->RemoveElement(this);

  if (array->IsEmpty()) {
    gObservers->Remove(mOrigin);
  }

  if (!gObservers->Count()) {
    gObservers = nullptr;
  }
}

mozilla::ipc::IPCResult Observer::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSObserverParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

/*******************************************************************************
 * LSRequestBase
 ******************************************************************************/

LSRequestBase::LSRequestBase(nsIEventTarget* aMainEventTarget)
    : mMainEventTarget(aMainEventTarget), mState(State::Initial) {}

LSRequestBase::~LSRequestBase() {
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
}

void LSRequestBase::Dispatch() {
  AssertIsOnOwningThread();

  mState = State::Opening;

  if (mMainEventTarget) {
    MOZ_ALWAYS_SUCCEEDS(mMainEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
  } else {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
  }
}

nsresult LSRequestBase::NestedRun() { return NS_OK; }

void LSRequestBase::SendReadyMessage() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingReadyMessage);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  }

  if (MayProceed()) {
    Unused << SendReady();

    mState = State::WaitingForFinish;
  } else {
    Cleanup();

    mState = State::Completed;
  }
}

void LSRequestBase::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  }

  if (MayProceed()) {
    LSRequestResponse response;

    if (NS_SUCCEEDED(ResultCode())) {
      GetResponse(response);
    } else {
      response = ResultCode();
    }

    Unused << PBackgroundLSRequestParent::Send__delete__(this, response);
  }

  Cleanup();

  mState = State::Completed;
}

NS_IMETHODIMP
LSRequestBase::Run() {
  nsresult rv;

  switch (mState) {
    case State::Opening:
      rv = Open();
      break;

    case State::Nesting:
      rv = NestedRun();
      break;

    case State::SendingReadyMessage:
      SendReadyMessage();
      return NS_OK;

    case State::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::SendingReadyMessage) {
    MaybeSetFailureCode(rv);

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::SendingReadyMessage;

    if (IsOnOwningThread()) {
      SendReadyMessage();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void LSRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteComplete();
}

mozilla::ipc::IPCResult LSRequestBase::RecvCancel() {
  AssertIsOnOwningThread();

  IProtocol* mgr = Manager();
  if (!PBackgroundLSRequestParent::Send__delete__(this, NS_ERROR_FAILURE)) {
    return IPC_FAIL_NO_REASON(mgr);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult LSRequestBase::RecvFinish() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForFinish);

  mState = State::SendingResults;

  // This LSRequestBase can only be held alive by the IPDL. Run() can end up
  // with clearing that last reference. So we need to add a self reference here.
  RefPtr<LSRequestBase> kungFuDeathGrip = this;

  MOZ_ALWAYS_SUCCEEDS(this->Run());

  return IPC_OK();
}

/*******************************************************************************
 * PrepareDatastoreOp
 ******************************************************************************/

PrepareDatastoreOp::PrepareDatastoreOp(
    nsIEventTarget* aMainEventTarget,
    already_AddRefed<ContentParent> aContentParent,
    const LSRequestParams& aParams)
    : LSRequestBase(aMainEventTarget),
      mMainEventTarget(aMainEventTarget),
      mContentParent(std::move(aContentParent)),
      mLoadDataOp(nullptr),
      mParams(aParams.get_LSRequestPrepareDatastoreParams()),
      mPrivateBrowsingId(0),
      mUsage(0),
      mSizeOfKeys(0),
      mSizeOfItems(0),
      mNestedState(NestedState::BeforeNesting),
      mDatabaseNotAvailable(false),
      mRequestedDirectoryLock(false),
      mInvalidated(false)
#ifdef DEBUG
      ,
      mDEBUGUsage(0)
#endif
{
  MOZ_ASSERT(aParams.type() ==
             LSRequestParams::TLSRequestPrepareDatastoreParams);

  if (mContentParent) {
    mContentParentId = Some(mContentParent->ChildID());
  }
}

PrepareDatastoreOp::~PrepareDatastoreOp() {
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
  MOZ_ASSERT(!mLoadDataOp);
}

nsresult PrepareDatastoreOp::Open() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Opening);
  MOZ_ASSERT(mNestedState == NestedState::BeforeNesting);

  // Swap this to the stack now to ensure that we release it on this thread.
  RefPtr<ContentParent> contentParent;
  mContentParent.swap(contentParent);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalInfo& principalInfo = mParams.principalInfo();

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(&mSuffix, &mGroup, &mOrigin);
  } else {
    MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
        PrincipalInfoToPrincipal(principalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = QuotaManager::GetInfoFromPrincipal(principal, &mSuffix, &mGroup,
                                            &mMainThreadOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = principal->GetPrivateBrowsingId(&mPrivateBrowsingId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mArchivedOriginScope = ArchivedOriginScope::CreateFromOrigin(principal);
    if (NS_WARN_IF(!mArchivedOriginScope)) {
      return NS_ERROR_FAILURE;
    }
  }

  // This service has to be started on the main thread currently.
  nsCOMPtr<mozIStorageService> ss;
  if (NS_WARN_IF(!(ss = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID)))) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = QuotaClient::RegisterObservers(OwningEventTarget());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mState = State::Nesting;
  mNestedState = NestedState::CheckExistingOperations;

  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult PrepareDatastoreOp::CheckExistingOperations() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::CheckExistingOperations);
  MOZ_ASSERT(gPrepareDatastoreOps);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  // Normally it's safe to access member variables without a mutex because even
  // though we hop between threads, the variables are never accessed by multiple
  // threads at the same time.
  // However, the methods OriginIsKnown and Origin can be called at any time.
  // So we have to make sure the member variable is set on the same thread as
  // those methods are called.
  mOrigin = mMainThreadOrigin;

  MOZ_ASSERT(!mOrigin.IsEmpty());

  mNestedState = NestedState::CheckClosingDatastore;

  // See if this PrepareDatastoreOp needs to wait.
  bool foundThis = false;
  for (uint32_t index = gPrepareDatastoreOps->Length(); index > 0; index--) {
    PrepareDatastoreOp* existingOp = (*gPrepareDatastoreOps)[index - 1];

    if (existingOp == this) {
      foundThis = true;
      continue;
    }

    if (foundThis && existingOp->Origin() == mOrigin) {
      // Only one op can be delayed.
      MOZ_ASSERT(!existingOp->mDelayedOp);
      existingOp->mDelayedOp = this;

      return NS_OK;
    }
  }

  nsresult rv = CheckClosingDatastoreInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::CheckClosingDatastore() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::CheckClosingDatastore);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = CheckClosingDatastoreInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::CheckClosingDatastoreInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::CheckClosingDatastore);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  mNestedState = NestedState::PreparationPending;

  RefPtr<Datastore> datastore;
  if (gDatastores && (datastore = gDatastores->Get(mOrigin)) &&
      datastore->IsClosed()) {
    datastore->WaitForConnectionToComplete(this);

    return NS_OK;
  }

  nsresult rv = BeginDatastorePreparationInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::BeginDatastorePreparation() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::PreparationPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = BeginDatastorePreparationInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::BeginDatastorePreparationInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::PreparationPending);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  if (gDatastores && (mDatastore = gDatastores->Get(mOrigin))) {
    MOZ_ASSERT(!mDatastore->IsClosed());

    mDatastore->NoteLivePrepareDatastoreOp(this);

    FinishNesting();

    return NS_OK;
  }

  if (QuotaManager::Get()) {
    nsresult rv = OpenDirectory();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  mNestedState = NestedState::QuotaManagerPending;
  QuotaManager::GetOrCreate(this, mMainEventTarget);

  return NS_OK;
}

nsresult PrepareDatastoreOp::QuotaManagerOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::QuotaManagerPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!QuotaManager::Get())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = OpenDirectory();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::PreparationPending ||
             mNestedState == NestedState::QuotaManagerPending);
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());
  MOZ_ASSERT(QuotaManager::Get());

  mNestedState = NestedState::DirectoryOpenPending;
  QuotaManager::Get()->OpenDirectory(PERSISTENCE_TYPE_DEFAULT, mGroup, mOrigin,
                                     mozilla::dom::quota::Client::LS,
                                     /* aExclusive */ false, this);

  mRequestedDirectoryLock = true;

  return NS_OK;
}

void PrepareDatastoreOp::SendToIOThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  // Skip all disk related stuff and transition to SendingReadyMessage if we
  // are preparing a datastore for private browsing.
  // Note that we do use a directory lock for private browsing even though we
  // don't do any stuff on disk. The thing is that without a directory lock,
  // quota manager wouldn't call AbortOperations for our private browsing
  // origin when a clear origin operation is requested. AbortOperations
  // requests all databases to close and the datastore is destroyed in the end.
  // Any following LocalStorage API call will trigger preparation of a new
  // (empty) datastore.
  if (mPrivateBrowsingId) {
    FinishNesting();

    return;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread.
  mNestedState = NestedState::DatabaseWorkOpen;

  MOZ_ALWAYS_SUCCEEDS(
      quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL));
}

nsresult PrepareDatastoreOp::DatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mArchivedOriginScope);
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DatabaseWorkOpen);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsresult rv;

  if (!gArchivedOrigins) {
    rv = LoadArchivedOrigins();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(gArchivedOrigins);
  }

  bool hasDataForMigration = mArchivedOriginScope->HasMatches(gArchivedOrigins);

  bool createIfNotExists = mParams.createIfNotExists() || hasDataForMigration;

  nsCOMPtr<nsIFile> directoryEntry;
  rv = quotaManager->EnsureOriginIsInitialized(
      PERSISTENCE_TYPE_DEFAULT, mSuffix, mGroup, mOrigin, createIfNotExists,
      getter_AddRefs(directoryEntry));
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return DatabaseNotAvailable();
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = EnsureDirectoryEntry(directoryEntry, createIfNotExists,
                            /* aIsDirectory */ true);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return DatabaseNotAvailable();
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool alreadyExisted;
  rv = EnsureDirectoryEntry(directoryEntry, createIfNotExists,
                            /* aIsDirectory */ false, &alreadyExisted);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return DatabaseNotAvailable();
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (alreadyExisted) {
    MOZ_ASSERT(gUsages);
    MOZ_ASSERT(gUsages->Get(mOrigin, &mUsage));
  } else {
    MOZ_ASSERT(mUsage == 0);
    InitUsageForOrigin(mOrigin, mUsage);
  }

  rv = directoryEntry->GetPath(mDatabaseFilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateStorageConnection(directoryEntry, mOrigin,
                               getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = VerifyDatabaseInformation(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasDataForMigration) {
    MOZ_ASSERT(mUsage == 0);

    rv = AttachArchiveDatabase(quotaManager->GetStoragePath(), connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int64_t newUsage;
    rv = GetUsage(connection, mArchivedOriginScope, &newUsage);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    RefPtr<QuotaObject> quotaObject = GetQuotaObject();
    MOZ_ASSERT(quotaObject);

    if (!quotaObject->MaybeUpdateSize(newUsage, /* aTruncate */ true)) {
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }

    mozStorageTransaction transaction(
        connection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(
        NS_LITERAL_CSTRING("INSERT INTO data (key, value) "
                           "SELECT key, value "
                           "FROM webappsstore2 "
                           "WHERE originKey = :originKey "
                           "AND originAttributes = :originAttributes;"

                           ),
        getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mArchivedOriginScope->BindToStatement(stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = connection->CreateStatement(
        NS_LITERAL_CSTRING("DELETE FROM webappsstore2 "
                           "WHERE originKey = :originKey "
                           "AND originAttributes = :originAttributes;"),
        getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mArchivedOriginScope->BindToStatement(stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = transaction.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = DetachArchiveDatabase(connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(gArchivedOrigins);
    MOZ_ASSERT(mArchivedOriginScope->HasMatches(gArchivedOrigins));
    mArchivedOriginScope->RemoveMatches(gArchivedOrigins);

    mUsage = newUsage;

    MOZ_ASSERT(gUsages);
    MOZ_ASSERT(gUsages->Contains(mOrigin));
    gUsages->Put(mOrigin, newUsage);
  }

  nsCOMPtr<mozIStorageConnection> shadowConnection;
  if (!gInitializedShadowStorage) {
    rv = CreateShadowStorageConnection(quotaManager->GetBasePath(),
                                       getter_AddRefs(shadowConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    gInitializedShadowStorage = true;
  }

  // Must close connections before dispatching otherwise we might race with the
  // connection thread which needs to open the same databases.
  MOZ_ALWAYS_SUCCEEDS(connection->Close());

  if (shadowConnection) {
    MOZ_ALWAYS_SUCCEEDS(shadowConnection->Close());
  }

  // Must set this before dispatching otherwise we will race with the owning
  // thread.
  mNestedState = NestedState::BeginLoadData;

  rv = OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::DatabaseNotAvailable() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DatabaseWorkOpen);

  mDatabaseNotAvailable = true;

  nsresult rv = FinishNestingOnNonOwningThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::EnsureDirectoryEntry(nsIFile* aEntry,
                                                  bool aCreateIfNotExists,
                                                  bool aIsDirectory,
                                                  bool* aAlreadyExisted) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aEntry);

  bool exists;
  nsresult rv = aEntry->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    if (!aCreateIfNotExists) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    if (aIsDirectory) {
      rv = aEntry->Create(nsIFile::DIRECTORY_TYPE, 0755);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }
#ifdef DEBUG
  else {
    bool isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(aEntry->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory == aIsDirectory);
  }
#endif

  if (aAlreadyExisted) {
    *aAlreadyExisted = exists;
  }
  return NS_OK;
}

nsresult PrepareDatastoreOp::VerifyDatabaseInformation(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv =
      aConnection->CreateStatement(NS_LITERAL_CSTRING("SELECT origin "
                                                      "FROM database"),
                                   getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  nsCString origin;
  rv = stmt->GetUTF8String(0, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!QuotaManager::AreOriginsEqualOnDisk(mOrigin, origin))) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

already_AddRefed<QuotaObject> PrepareDatastoreOp::GetQuotaObject() {
  MOZ_ASSERT(IsOnOwningThread() || IsOnIOThread());
  MOZ_ASSERT(!mGroup.IsEmpty());
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  RefPtr<QuotaObject> quotaObject = quotaManager->GetQuotaObject(
      PERSISTENCE_TYPE_DEFAULT, mGroup, mOrigin, mDatabaseFilePath, mUsage);
  MOZ_ASSERT(quotaObject);

  return quotaObject.forget();
}

nsresult PrepareDatastoreOp::BeginLoadData() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::BeginLoadData);
  MOZ_ASSERT(!mConnection);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  if (!gConnectionThread) {
    gConnectionThread = new ConnectionThread();
  }

  mConnection = gConnectionThread->CreateConnection(
      mOrigin, mDatabaseFilePath, std::move(mArchivedOriginScope));
  MOZ_ASSERT(mConnection);

  // Must set this before dispatching otherwise we will race with the
  // connection thread.
  mNestedState = NestedState::DatabaseWorkLoadData;

  // Can't assign to mLoadDataOp directly since that's a weak reference and
  // LoadDataOp is reference counted.
  RefPtr<LoadDataOp> loadDataOp = new LoadDataOp(this);

  // This add refs loadDataOp.
  mConnection->Dispatch(loadDataOp);

  // This is cleared in LoadDataOp::Cleanup() before the load data op is
  // destroyed.
  mLoadDataOp = loadDataOp;

  return NS_OK;
}

void PrepareDatastoreOp::FinishNesting() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);

  // The caller holds a strong reference to us, no need for a self reference
  // before calling Run().

  mState = State::SendingReadyMessage;
  mNestedState = NestedState::AfterNesting;

  MOZ_ALWAYS_SUCCEEDS(Run());
}

nsresult PrepareDatastoreOp::FinishNestingOnNonOwningThread() {
  MOZ_ASSERT(!IsOnOwningThread());
  MOZ_ASSERT(mState == State::Nesting);

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  mState = State::SendingReadyMessage;
  mNestedState = NestedState::AfterNesting;

  nsresult rv = OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::NestedRun() {
  nsresult rv;

  switch (mNestedState) {
    case NestedState::CheckExistingOperations:
      rv = CheckExistingOperations();
      break;

    case NestedState::CheckClosingDatastore:
      rv = CheckClosingDatastore();
      break;

    case NestedState::PreparationPending:
      rv = BeginDatastorePreparation();
      break;

    case NestedState::QuotaManagerPending:
      rv = QuotaManagerOpen();
      break;

    case NestedState::DatabaseWorkOpen:
      rv = DatabaseWork();
      break;

    case NestedState::BeginLoadData:
      rv = BeginLoadData();
      break;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    mNestedState = NestedState::AfterNesting;

    return rv;
  }

  return NS_OK;
}

void PrepareDatastoreOp::GetResponse(LSRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

  if (mDatabaseNotAvailable) {
    MOZ_ASSERT(!mParams.createIfNotExists());

    LSRequestPrepareDatastoreResponse prepareDatastoreResponse;
    prepareDatastoreResponse.datastoreId() = null_t();

    aResponse = prepareDatastoreResponse;

    return;
  }

  if (!mDatastore) {
    MOZ_ASSERT(mUsage == mDEBUGUsage);

    RefPtr<QuotaObject> quotaObject;

    if (mPrivateBrowsingId == 0) {
      quotaObject = GetQuotaObject();
      MOZ_ASSERT(quotaObject);
    }

    mDatastore = new Datastore(mOrigin, mPrivateBrowsingId, mUsage, mSizeOfKeys,
                               mSizeOfItems, mDirectoryLock.forget(),
                               mConnection.forget(), quotaObject.forget(),
                               mValues, mOrderedItems);

    mDatastore->NoteLivePrepareDatastoreOp(this);

    if (!gDatastores) {
      gDatastores = new DatastoreHashtable();
    }

    MOZ_ASSERT(!gDatastores->Get(mOrigin));
    gDatastores->Put(mOrigin, mDatastore);
  }

  uint64_t datastoreId = ++gLastDatastoreId;

  nsAutoPtr<PreparedDatastore> preparedDatastore(
      new PreparedDatastore(mDatastore, mContentParentId, mOrigin, datastoreId,
                            /* aForPreload */ !mParams.createIfNotExists()));

  if (!gPreparedDatastores) {
    gPreparedDatastores = new PreparedDatastoreHashtable();
  }
  gPreparedDatastores->Put(datastoreId, preparedDatastore);

  if (mInvalidated) {
    preparedDatastore->Invalidate();
  }

  preparedDatastore.forget();

  LSRequestPrepareDatastoreResponse prepareDatastoreResponse;
  prepareDatastoreResponse.datastoreId() = datastoreId;

  aResponse = prepareDatastoreResponse;
}

void PrepareDatastoreOp::Cleanup() {
  AssertIsOnOwningThread();

  if (mDatastore) {
    MOZ_ASSERT(!mDirectoryLock);
    MOZ_ASSERT(!mConnection);

    if (NS_FAILED(ResultCode())) {
      MOZ_ASSERT(!mDatastore->IsClosed());
      MOZ_ASSERT(!mDatastore->HasLiveDatabases());
      MOZ_ASSERT(!mDatastore->HasLivePreparedDatastores());
      mDatastore->Close();
    }

    // Make sure to release the datastore on this thread.

    mDatastore->NoteFinishedPrepareDatastoreOp(this);

    mDatastore = nullptr;

    CleanupMetadata();
  } else if (mConnection) {
    // If we have a connection then the operation must have failed and there
    // must be a directory lock too.
    MOZ_ASSERT(NS_FAILED(ResultCode()));
    MOZ_ASSERT(mDirectoryLock);

    // We must close the connection on the connection thread before releasing
    // it on this thread. The directory lock can't be released either.
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("dom::OpenDatabaseOp::ConnectionClosedCallback", this,
                          &PrepareDatastoreOp::ConnectionClosedCallback);

    mConnection->Close(callback);
  } else {
    // If we don't have a connection, but we do have a directory lock then the
    // operation must have failed or we were preloading a datastore and there
    // was no physical database on disk.
    MOZ_ASSERT_IF(mDirectoryLock,
                  NS_FAILED(ResultCode()) || mDatabaseNotAvailable);

    // There's no connection, so it's safe to release the directory lock and
    // unregister itself from the array.

    mDirectoryLock = nullptr;

    CleanupMetadata();
  }
}

void PrepareDatastoreOp::ConnectionClosedCallback() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(ResultCode()));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mConnection);

  mConnection = nullptr;
  mDirectoryLock = nullptr;

  CleanupMetadata();
}

void PrepareDatastoreOp::CleanupMetadata() {
  AssertIsOnOwningThread();

  if (mDelayedOp) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mDelayedOp.forget()));
  }

  MOZ_ASSERT(gPrepareDatastoreOps);
  gPrepareDatastoreOps->RemoveElement(this);

  if (gPrepareDatastoreOps->IsEmpty()) {
    gPrepareDatastoreOps = nullptr;
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(PrepareDatastoreOp, LSRequestBase)

void PrepareDatastoreOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  LSRequestBase::ActorDestroy(aWhy);

  if (mLoadDataOp) {
    mLoadDataOp->NoteComplete();
  }
}

void PrepareDatastoreOp::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);

    FinishNesting();

    return;
  }

  mDirectoryLock = aLock;

  SendToIOThread();
}

void PrepareDatastoreOp::DirectoryLockFailed() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  MaybeSetFailureCode(NS_ERROR_FAILURE);

  FinishNesting();
}

nsresult PrepareDatastoreOp::LoadDataOp::DoDatastoreWork() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mState == State::Nesting);
  MOZ_ASSERT(mPrepareDatastoreOp->mNestedState ==
             NestedState::DatabaseWorkLoadData);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  Connection::CachedStatement stmt;
  nsresult rv =
      mConnection->GetCachedStatement(NS_LITERAL_CSTRING("SELECT key, value "
                                                         "FROM data;"),
                                      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsString key;
    rv = stmt->GetString(0, key);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsString value;
    rv = stmt->GetString(1, value);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mPrepareDatastoreOp->mValues.Put(key, value);
    auto item = mPrepareDatastoreOp->mOrderedItems.AppendElement();
    item->key() = key;
    item->value() = value;
    mPrepareDatastoreOp->mSizeOfKeys += key.Length();
    mPrepareDatastoreOp->mSizeOfItems += key.Length() + value.Length();
#ifdef DEBUG
    mPrepareDatastoreOp->mDEBUGUsage += key.Length() + value.Length();
#endif
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void PrepareDatastoreOp::LoadDataOp::OnSuccess() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mState == State::Nesting);
  MOZ_ASSERT(mPrepareDatastoreOp->mNestedState ==
             NestedState::DatabaseWorkLoadData);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->FinishNesting();
}

void PrepareDatastoreOp::LoadDataOp::OnFailure(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mState == State::Nesting);
  MOZ_ASSERT(mPrepareDatastoreOp->mNestedState ==
             NestedState::DatabaseWorkLoadData);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->SetFailureCode(aResultCode);

  mPrepareDatastoreOp->FinishNesting();
}

void PrepareDatastoreOp::LoadDataOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->mLoadDataOp = nullptr;
  mPrepareDatastoreOp = nullptr;

  ConnectionDatastoreOperationBase::Cleanup();
}

/*******************************************************************************
 * PrepareObserverOp
 ******************************************************************************/

PrepareObserverOp::PrepareObserverOp(nsIEventTarget* aMainEventTarget,
                                     const LSRequestParams& aParams)
    : LSRequestBase(aMainEventTarget),
      mParams(aParams.get_LSRequestPrepareObserverParams()) {
  MOZ_ASSERT(aParams.type() ==
             LSRequestParams::TLSRequestPrepareObserverParams);
}

nsresult PrepareObserverOp::Open() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Opening);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalInfo& principalInfo = mParams.principalInfo();

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(nullptr, nullptr, &mOrigin);
  } else {
    MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
        PrincipalInfoToPrincipal(principalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = QuotaManager::GetInfoFromPrincipal(principal, nullptr, nullptr,
                                            &mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mState = State::SendingReadyMessage;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void PrepareObserverOp::GetResponse(LSRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

  uint64_t observerId = ++gLastObserverId;

  RefPtr<Observer> observer = new Observer(mOrigin);

  if (!gPreparedObsevers) {
    gPreparedObsevers = new PreparedObserverHashtable();
  }
  gPreparedObsevers->Put(observerId, observer);

  LSRequestPrepareObserverResponse prepareObserverResponse;
  prepareObserverResponse.observerId() = observerId;

  aResponse = prepareObserverResponse;
}

/*******************************************************************************
+ * LSSimpleRequestBase
+
******************************************************************************/

LSSimpleRequestBase::LSSimpleRequestBase() : mState(State::Initial) {}

LSSimpleRequestBase::~LSSimpleRequestBase() {
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
}

void LSSimpleRequestBase::Dispatch() {
  AssertIsOnOwningThread();

  mState = State::Opening;

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
}

void LSSimpleRequestBase::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  }

  if (MayProceed()) {
    LSSimpleRequestResponse response;

    if (NS_SUCCEEDED(ResultCode())) {
      GetResponse(response);
    } else {
      response = ResultCode();
    }

    Unused << PBackgroundLSSimpleRequestParent::Send__delete__(this, response);
  }

  mState = State::Completed;
}

NS_IMETHODIMP
LSSimpleRequestBase::Run() {
  nsresult rv;

  switch (mState) {
    case State::Opening:
      rv = Open();
      break;

    case State::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::SendingResults) {
    MaybeSetFailureCode(rv);

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::SendingResults;

    if (IsOnOwningThread()) {
      SendResults();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void LSSimpleRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteComplete();
}

/*******************************************************************************
 * PreloadedOp
 ******************************************************************************/

PreloadedOp::PreloadedOp(const LSSimpleRequestParams& aParams)
    : mParams(aParams.get_LSSimpleRequestPreloadedParams()) {
  MOZ_ASSERT(aParams.type() ==
             LSSimpleRequestParams::TLSSimpleRequestPreloadedParams);
}

nsresult PreloadedOp::Open() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Opening);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalInfo& principalInfo = mParams.principalInfo();

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(nullptr, nullptr, &mOrigin);
  } else {
    MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
        PrincipalInfoToPrincipal(principalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = QuotaManager::GetInfoFromPrincipal(principal, nullptr, nullptr,
                                            &mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void PreloadedOp::GetResponse(LSSimpleRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

  bool preloaded;
  RefPtr<Datastore> datastore;
  if (gDatastores && (datastore = gDatastores->Get(mOrigin)) &&
      !datastore->IsClosed()) {
    preloaded = true;
  } else {
    preloaded = false;
  }

  LSSimpleRequestPreloadedResponse preloadedResponse;
  preloadedResponse.preloaded() = preloaded;

  aResponse = preloadedResponse;
}

/*******************************************************************************
 * ArchivedOriginScope
 ******************************************************************************/

// static
ArchivedOriginScope* ArchivedOriginScope::CreateFromOrigin(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  nsCString originAttrSuffix;
  nsCString originKey;
  nsresult rv = GenerateOriginKey(aPrincipal, originAttrSuffix, originKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return new ArchivedOriginScope(
      std::move(Origin(originAttrSuffix, originKey)));
}

// static
ArchivedOriginScope* ArchivedOriginScope::CreateFromPrefix(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  nsCString originAttrSuffix;
  nsCString originKey;
  nsresult rv = GenerateOriginKey(aPrincipal, originAttrSuffix, originKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return new ArchivedOriginScope(std::move(Prefix(originKey)));
}

// static
ArchivedOriginScope* ArchivedOriginScope::CreateFromPattern(
    const OriginAttributesPattern& aPattern) {
  return new ArchivedOriginScope(std::move(Pattern(aPattern)));
}

// static
ArchivedOriginScope* ArchivedOriginScope::CreateFromNull() {
  return new ArchivedOriginScope(std::move(Null()));
}

void ArchivedOriginScope::GetBindingClause(nsACString& aBindingClause) const {
  struct Matcher {
    nsACString* mBindingClause;

    explicit Matcher(nsACString* aBindingClause)
        : mBindingClause(aBindingClause) {}

    void match(const Origin& aOrigin) {
      *mBindingClause = NS_LITERAL_CSTRING(
          " WHERE originKey = :originKey "
          "AND originAttributes = :originAttributes");
    }

    void match(const Prefix& aPrefix) {
      *mBindingClause = NS_LITERAL_CSTRING(" WHERE originKey = :originKey");
    }

    void match(const Pattern& aPattern) {
      *mBindingClause = NS_LITERAL_CSTRING(
          " WHERE originAttributes MATCH :originAttributesPattern");
    }

    void match(const Null& aNull) { *mBindingClause = EmptyCString(); }
  };

  mData.match(Matcher(&aBindingClause));
}

nsresult ArchivedOriginScope::BindToStatement(
    mozIStorageStatement* aStmt) const {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(aStmt);

  struct Matcher {
    mozIStorageStatement* mStmt;

    explicit Matcher(mozIStorageStatement* aStmt) : mStmt(aStmt) {}

    nsresult match(const Origin& aOrigin) {
      nsresult rv = mStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("originKey"),
                                                aOrigin.OriginNoSuffix());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = mStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("originAttributes"),
                                       aOrigin.OriginSuffix());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }

    nsresult match(const Prefix& aPrefix) {
      nsresult rv = mStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("originKey"),
                                                aPrefix.OriginNoSuffix());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }

    nsresult match(const Pattern& aPattern) {
      nsresult rv = mStmt->BindUTF8StringByName(
          NS_LITERAL_CSTRING("originAttributesPattern"),
          NS_LITERAL_CSTRING("pattern1"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }

    nsresult match(const Null& aNull) { return NS_OK; }
  };

  nsresult rv = mData.match(Matcher(aStmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool ArchivedOriginScope::HasMatches(
    ArchivedOriginHashtable* aHashtable) const {
  AssertIsOnIOThread();
  MOZ_ASSERT(aHashtable);

  struct Matcher {
    ArchivedOriginHashtable* mHashtable;

    explicit Matcher(ArchivedOriginHashtable* aHashtable)
        : mHashtable(aHashtable) {}

    bool match(const Origin& aOrigin) {
      nsCString hashKey = GetArchivedOriginHashKey(aOrigin.OriginSuffix(),
                                                   aOrigin.OriginNoSuffix());

      ArchivedOriginInfo* archivedOriginInfo;
      return mHashtable->Get(hashKey, &archivedOriginInfo);
    }

    bool match(const Prefix& aPrefix) {
      for (auto iter = mHashtable->ConstIter(); !iter.Done(); iter.Next()) {
        ArchivedOriginInfo* archivedOriginInfo = iter.Data();

        if (archivedOriginInfo->mOriginNoSuffix == aPrefix.OriginNoSuffix()) {
          return true;
        }
      }

      return false;
    }

    bool match(const Pattern& aPattern) {
      for (auto iter = mHashtable->ConstIter(); !iter.Done(); iter.Next()) {
        ArchivedOriginInfo* archivedOriginInfo = iter.Data();

        if (aPattern.GetPattern().Matches(
                archivedOriginInfo->mOriginAttributes)) {
          return true;
        }
      }

      return false;
    }

    bool match(const Null& aNull) { return mHashtable->Count(); }
  };

  return mData.match(Matcher(aHashtable));
}

void ArchivedOriginScope::RemoveMatches(
    ArchivedOriginHashtable* aHashtable) const {
  AssertIsOnIOThread();
  MOZ_ASSERT(aHashtable);

  struct Matcher {
    ArchivedOriginHashtable* mHashtable;

    explicit Matcher(ArchivedOriginHashtable* aHashtable)
        : mHashtable(aHashtable) {}

    void match(const Origin& aOrigin) {
      nsCString hashKey = GetArchivedOriginHashKey(aOrigin.OriginSuffix(),
                                                   aOrigin.OriginNoSuffix());

      mHashtable->Remove(hashKey);
    }

    void match(const Prefix& aPrefix) {
      for (auto iter = mHashtable->Iter(); !iter.Done(); iter.Next()) {
        ArchivedOriginInfo* archivedOriginInfo = iter.Data();

        if (archivedOriginInfo->mOriginNoSuffix == aPrefix.OriginNoSuffix()) {
          iter.Remove();
        }
      }
    }

    void match(const Pattern& aPattern) {
      for (auto iter = mHashtable->Iter(); !iter.Done(); iter.Next()) {
        ArchivedOriginInfo* archivedOriginInfo = iter.Data();

        if (aPattern.GetPattern().Matches(
                archivedOriginInfo->mOriginAttributes)) {
          iter.Remove();
        }
      }
    }

    void match(const Null& aNull) { mHashtable->Clear(); }
  };

  mData.match(Matcher(aHashtable));
}

/*******************************************************************************
 * ArchivedOriginScopeHelper
 ******************************************************************************/

nsresult ArchivedOriginScopeHelper::BlockAndReturnArchivedOriginScope(
    nsAutoPtr<ArchivedOriginScope>& aArchivedOriginScope) {
  AssertIsOnIOThread();

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));

  mozilla::MonitorAutoLock lock(mMonitor);
  while (mWaiting) {
    lock.Wait();
  }

  if (NS_WARN_IF(NS_FAILED(mMainThreadResultCode))) {
    return mMainThreadResultCode;
  }

  aArchivedOriginScope = std::move(mArchivedOriginScope);
  return NS_OK;
}

nsresult ArchivedOriginScopeHelper::RunOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), mSpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateCodebasePrincipal(uri, mAttrs);
  if (NS_WARN_IF(!principal)) {
    return NS_ERROR_FAILURE;
  }

  if (mPrefix) {
    mArchivedOriginScope = ArchivedOriginScope::CreateFromPrefix(principal);
  } else {
    mArchivedOriginScope = ArchivedOriginScope::CreateFromOrigin(principal);
  }
  if (NS_WARN_IF(!mArchivedOriginScope)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
ArchivedOriginScopeHelper::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = RunOnMainThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mMainThreadResultCode = rv;
  }

  mozilla::MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mWaiting);

  mWaiting = false;
  lock.Notify();

  return NS_OK;
}

/*******************************************************************************
 * QuotaClient
 ******************************************************************************/

QuotaClient* QuotaClient::sInstance = nullptr;
bool QuotaClient::sObserversRegistered = false;

QuotaClient::QuotaClient()
    : mShadowDatabaseMutex("LocalStorage mShadowDatabaseMutex"),
      mShutdownRequested(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!sInstance, "We expect this to be a singleton!");

  sInstance = this;
}

QuotaClient::~QuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this, "We expect this to be a singleton!");

  sInstance = nullptr;
}

mozilla::dom::quota::Client::Type QuotaClient::GetType() {
  return QuotaClient::LS;
}

// static
nsresult QuotaClient::RegisterObservers(
    nsIEventTarget* aBackgroundEventTarget) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundEventTarget);

  if (!sObserversRegistered) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIObserver> observer = new Observer(aBackgroundEventTarget);

    nsresult rv =
        obs->AddObserver(observer, kPrivateBrowsingObserverTopic, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_FAILED(Preferences::AddAtomicUintVarCache(
            &gOriginLimitKB, kDefaultQuotaPref, kDefaultOriginLimitKB))) {
      NS_WARNING("Unable to respond to default quota pref changes!");
    }

    Preferences::RegisterCallbackAndCall(ShadowWritesPrefChangedCallback,
                                         kShadowWritesPref);

    Preferences::RegisterCallbackAndCall(SnapshotPrefillPrefChangedCallback,
                                         kSnapshotPrefillPref);

    sObserversRegistered = true;
  }

  return NS_OK;
}

nsresult QuotaClient::InitOrigin(PersistenceType aPersistenceType,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin,
                                 const AtomicBool& aCanceled,
                                 UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = quotaManager->GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                                    getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(directory);

  rv = directory->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
#ifdef DEBUG
  rv = directory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(exists);
#endif

  nsCOMPtr<nsIFile> file;
  rv = directory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(isDirectory)) {
      return NS_ERROR_FAILURE;
    }

    // TODO: Use a special file that contains logical size of the database.
    //       For now, get the usage from the database.

    nsCOMPtr<mozIStorageConnection> connection;
    rv = CreateStorageConnection(file, aOrigin, getter_AddRefs(connection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int64_t usage;
    rv = GetUsage(connection, /* aArchivedOriginScope */ nullptr, &usage);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    InitUsageForOrigin(aOrigin, usage);

    aUsageInfo->AppendToDatabaseUsage(uint64_t(usage));
  }

  // Report unknown files, don't fail, just warn.

  nsCOMPtr<nsIDirectoryEnumerator> directoryEntries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(directoryEntries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!directoryEntries) {
    return NS_OK;
  }

  while (true) {
    if (aCanceled) {
      break;
    }

    nsCOMPtr<nsIFile> file;
    rv = directoryEntries->GetNextFile(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!file) {
      break;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (leafName.EqualsLiteral(DATA_FILE_NAME)) {
      // Don't need to check if it is a directory or file. We did that above.
      continue;
    }

    if (leafName.EqualsLiteral(JOURNAL_FILE_NAME)) {
      bool isDirectory;
      rv = file->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!isDirectory) {
        continue;
      }
    }

    LS_WARNING("Something (%s) in the directory that doesn't belong!",
               NS_ConvertUTF16toUTF8(leafName).get());
  }

  return NS_OK;
}

nsresult QuotaClient::GetUsageForOrigin(PersistenceType aPersistenceType,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin,
                                        const AtomicBool& aCanceled,
                                        UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(aUsageInfo);

  // We can't open the database at this point, since it can be already used
  // by the connection thread. Use the cached value instead.

  if (gUsages) {
    int64_t usage;
    if (gUsages->Get(aOrigin, &usage)) {
      aUsageInfo->AppendToDatabaseUsage(usage);
    }
  }

  return NS_OK;
}

nsresult QuotaClient::AboutToClearOrigins(
    const Nullable<PersistenceType>& aPersistenceType,
    const OriginScope& aOriginScope) {
  AssertIsOnIOThread();

  // This method is not called when the clearing is triggered by the eviction
  // process. It's on purpose to avoid a problem with the origin access time
  // which can be described as follows:
  // When there's a storage pressure condition and quota manager starts
  // collecting origins for eviction, there can be an origin that hasn't been
  // touched for long time. However, the old implementation of local storage
  // could have touched the origin only recently and the new implementation
  // hasn't had a chance to create a new per origin database for it yet (the
  // data is still in the archive database), so the origin access time hasn't
  // been updated either. In the end, the origin would be evicted despite the
  // fact that there was recent local storage activity.
  // So this method clears the archived data and shadow database entries for
  // given origin scope, but only if it's a privacy-related origin clearing.

  if (!aPersistenceType.IsNull() &&
      aPersistenceType.Value() != PERSISTENCE_TYPE_DEFAULT) {
    return NS_OK;
  }

  bool shadowWrites = gShadowWrites;

  nsAutoPtr<ArchivedOriginScope> archivedOriginScope;
  nsresult rv = CreateArchivedOriginScope(aOriginScope, archivedOriginScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!gArchivedOrigins) {
    rv = LoadArchivedOrigins();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(gArchivedOrigins);
  }

  bool hasDataForRemoval = archivedOriginScope->HasMatches(gArchivedOrigins);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsString basePath = quotaManager->GetBasePath();

  {
    MutexAutoLock shadowDatabaseLock(mShadowDatabaseMutex);

    nsCOMPtr<mozIStorageConnection> connection;
    if (gInitializedShadowStorage) {
      rv = GetShadowStorageConnection(basePath, getter_AddRefs(connection));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      rv = CreateShadowStorageConnection(basePath, getter_AddRefs(connection));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      gInitializedShadowStorage = true;
    }

    if (hasDataForRemoval) {
      rv = AttachArchiveDatabase(quotaManager->GetStoragePath(), connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (archivedOriginScope->IsPattern()) {
      nsCOMPtr<mozIStorageFunction> function(
          new MatchFunction(archivedOriginScope->GetPattern()));

      rv = connection->CreateFunction(NS_LITERAL_CSTRING("match"), 2, function);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(NS_LITERAL_CSTRING("BEGIN IMMEDIATE;"),
                                     getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (shadowWrites) {
      rv = PerformDelete(connection, NS_LITERAL_CSTRING("main"),
                         archivedOriginScope);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (hasDataForRemoval) {
      rv = PerformDelete(connection, NS_LITERAL_CSTRING("archive"),
                         archivedOriginScope);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = connection->CreateStatement(NS_LITERAL_CSTRING("COMMIT;"),
                                     getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    stmt = nullptr;

    if (archivedOriginScope->IsPattern()) {
      rv = connection->RemoveFunction(NS_LITERAL_CSTRING("match"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (hasDataForRemoval) {
      rv = DetachArchiveDatabase(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(gArchivedOrigins);
      MOZ_ASSERT(archivedOriginScope->HasMatches(gArchivedOrigins));
      archivedOriginScope->RemoveMatches(gArchivedOrigins);
    }

    rv = connection->Close();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aOriginScope.IsNull()) {
    nsCOMPtr<nsIFile> shadowFile;
    rv = GetShadowFile(basePath, getter_AddRefs(shadowFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = shadowFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    gInitializedShadowStorage = false;
  }

  return NS_OK;
}

void QuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                         const nsACString& aOrigin) {
  AssertIsOnIOThread();

  if (aPersistenceType != PERSISTENCE_TYPE_DEFAULT) {
    return;
  }

  if (gUsages) {
    gUsages->Remove(aOrigin);
  }
}

void QuotaClient::ReleaseIOThreadObjects() {
  AssertIsOnIOThread();

  gUsages = nullptr;

  // Delete archived origins hashtable since QuotaManager clears the whole
  // storage directory including ls-archive.sqlite.

  gArchivedOrigins = nullptr;
}

void QuotaClient::AbortOperations(const nsACString& aOrigin) {
  AssertIsOnBackgroundThread();

  // A PrepareDatastoreOp object could already acquire a directory lock for
  // the given origin. Its last step is creation of a Datastore object (which
  // will take ownership of the directory lock) and a PreparedDatastore object
  // which keeps the Datastore alive until a database actor is created.
  // We need to invalidate the PreparedDatastore object when it's created,
  // otherwise the Datastore object can block the origin clear operation for
  // long time. It's not a problem that we don't fail the PrepareDatastoreOp
  // immediatelly (avoiding the creation of the Datastore and PreparedDatastore
  // object). We will call RequestAllowToClose on the database actor once it's
  // created and the child actor will respond by sending AllowToClose which
  // will close the Datastore on the parent side (the closing releases the
  // directory lock).

  if (gPrepareDatastoreOps) {
    for (PrepareDatastoreOp* prepareDatastoreOp : *gPrepareDatastoreOps) {
      MOZ_ASSERT(prepareDatastoreOp);

      // Explicitely check if a directory lock has been requested.
      // Origin clearing can't be blocked by this PrepareDatastoreOp if it
      // hasn't requested a directory lock yet, so we can just ignore it.
      // This will also guarantee that PrepareDatastoreOp has a known origin.
      // And it also ensures that the ordering is right. Without the check we
      // could invalidate ops whose directory locks were requested after we
      // requested a directory lock for origin clearing.
      if (!prepareDatastoreOp->RequestedDirectoryLock()) {
        continue;
      }

      MOZ_ASSERT(prepareDatastoreOp->OriginIsKnown());

      if (aOrigin.IsVoid() || prepareDatastoreOp->Origin() == aOrigin) {
        prepareDatastoreOp->Invalidate();
      }
    }
  }

  if (gPreparedDatastores) {
    for (auto iter = gPreparedDatastores->ConstIter(); !iter.Done();
         iter.Next()) {
      PreparedDatastore* preparedDatastore = iter.Data();
      MOZ_ASSERT(preparedDatastore);

      if (aOrigin.IsVoid() || preparedDatastore->Origin() == aOrigin) {
        preparedDatastore->Invalidate();
      }
    }
  }

  if (gLiveDatabases) {
    for (Database* database : *gLiveDatabases) {
      if (aOrigin.IsVoid() || database->Origin() == aOrigin) {
        // TODO: This just allows the database to close, but we can actually
        //       set a flag to abort any existing operations, so we can
        //       eventually close faster.

        database->RequestAllowToClose();
      }
    }
  }
}

void QuotaClient::AbortOperationsForProcess(ContentParentId aContentParentId) {
  AssertIsOnBackgroundThread();

  if (gLiveDatabases) {
    for (Database* database : *gLiveDatabases) {
      if (database->IsOwnedByProcess(aContentParentId)) {
        database->RequestAllowToClose();
      }
    }
  }
}

void QuotaClient::StartIdleMaintenance() { AssertIsOnBackgroundThread(); }

void QuotaClient::StopIdleMaintenance() { AssertIsOnBackgroundThread(); }

void QuotaClient::ShutdownWorkThreads() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  mShutdownRequested = true;

  // gPrepareDatastoreOps are short lived objects running a state machine.
  // The shutdown flag is checked between states, so we don't have to notify
  // all the objects here.
  // Allocation of a new PrepareDatastoreOp object is prevented once the
  // shutdown flag is set.
  // When the last PrepareDatastoreOp finishes, the gPrepareDatastoreOps array
  // is destroyed.

  if (gPreparedDatastores) {
    gPreparedDatastores->Clear();
    gPreparedDatastores = nullptr;
  }

  if (gLiveDatabases) {
    for (Database* database : *gLiveDatabases) {
      database->RequestAllowToClose();
    }
  }

  if (gPreparedObsevers) {
    gPreparedObsevers->Clear();
    gPreparedObsevers = nullptr;
  }

  // This should release any local storage related quota objects or directory
  // locks.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() {
    // Don't have to check gPreparedDatastores since we nulled it out above.
    return !gPrepareDatastoreOps && !gDatastores && !gLiveDatabases;
  }));

  // And finally, shutdown the connection thread.
  if (gConnectionThread) {
    gConnectionThread->Shutdown();

    gConnectionThread = nullptr;
  }
}

nsresult QuotaClient::CreateArchivedOriginScope(
    const OriginScope& aOriginScope,
    nsAutoPtr<ArchivedOriginScope>& aArchivedOriginScope) {
  AssertIsOnIOThread();

  nsresult rv;

  nsAutoPtr<ArchivedOriginScope> archivedOriginScope;

  if (aOriginScope.IsOrigin()) {
    nsCString spec;
    OriginAttributes attrs;
    if (NS_WARN_IF(!QuotaManager::ParseOrigin(aOriginScope.GetOrigin(), spec,
                                              &attrs))) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<ArchivedOriginScopeHelper> helper =
        new ArchivedOriginScopeHelper(spec, attrs, /* aPrefix */ false);

    rv = helper->BlockAndReturnArchivedOriginScope(archivedOriginScope);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (aOriginScope.IsPrefix()) {
    nsCString spec;
    OriginAttributes attrs;
    if (NS_WARN_IF(!QuotaManager::ParseOrigin(aOriginScope.GetOriginNoSuffix(),
                                              spec, &attrs))) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<ArchivedOriginScopeHelper> helper =
        new ArchivedOriginScopeHelper(spec, attrs, /* aPrefix */ true);

    rv = helper->BlockAndReturnArchivedOriginScope(archivedOriginScope);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (aOriginScope.IsPattern()) {
    archivedOriginScope =
        ArchivedOriginScope::CreateFromPattern(aOriginScope.GetPattern());
  } else {
    MOZ_ASSERT(aOriginScope.IsNull());

    archivedOriginScope = ArchivedOriginScope::CreateFromNull();
  }

  MOZ_ASSERT(archivedOriginScope);

  aArchivedOriginScope = std::move(archivedOriginScope);
  return NS_OK;
}

nsresult QuotaClient::PerformDelete(
    mozIStorageConnection* aConnection, const nsACString& aSchemaName,
    ArchivedOriginScope* aArchivedOriginScope) const {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aArchivedOriginScope);

  nsresult rv;

  nsCString bindingClause;
  aArchivedOriginScope->GetBindingClause(bindingClause);

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM ") +
                                        aSchemaName +
                                        NS_LITERAL_CSTRING(".webappsstore2") +
                                        bindingClause + NS_LITERAL_CSTRING(";"),
                                    getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aArchivedOriginScope->BindToStatement(stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
QuotaClient::ClearPrivateBrowsingRunnable::Run() {
  AssertIsOnBackgroundThread();

  if (gDatastores) {
    for (auto iter = gDatastores->ConstIter(); !iter.Done(); iter.Next()) {
      Datastore* datastore = iter.Data();
      MOZ_ASSERT(datastore);

      if (datastore->PrivateBrowsingId()) {
        datastore->PrivateBrowsingClear();
      }
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaClient::Observer, nsIObserver)

NS_IMETHODIMP
QuotaClient::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, kPrivateBrowsingObserverTopic)) {
    RefPtr<ClearPrivateBrowsingRunnable> runnable =
        new ClearPrivateBrowsingRunnable();

    MOZ_ALWAYS_SUCCEEDS(
        mBackgroundEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL));

    return NS_OK;
  }

  NS_WARNING("Unknown observer topic!");
  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaClient::MatchFunction, mozIStorageFunction)

NS_IMETHODIMP
QuotaClient::MatchFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aFunctionArguments);
  MOZ_ASSERT(aResult);

  nsCString suffix;
  nsresult rv = aFunctionArguments->GetUTF8String(1, suffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  OriginAttributes oa;
  if (NS_WARN_IF(!oa.PopulateFromSuffix(suffix))) {
    return NS_ERROR_FAILURE;
  }

  bool result = mPattern.Matches(oa);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsBool(result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  outVar.forget(aResult);
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
