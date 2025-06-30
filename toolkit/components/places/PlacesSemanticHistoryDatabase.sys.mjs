/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", function () {
  return lazy.PlacesUtils.getLogger({
    prefix: "PlacesSemanticHistoryDatabase",
  });
});

// Every time the schema or the underlying data changes, you must bump up the
// schema version. This is also necessary for example if we change the embedding
// size.

// Remember to:
// 1. Bump up the version number
// 2. Add a migration function to migrate the data to the new schema.
// 3. Update #createDatabaseEntities and #checkDatabaseEntities
// 4. Add a test to check that the migration works correctly.

// Note downgrades are not supported, so when you bump up the version and the
// user downgrades, the database will be deleted and recreated.
// If a migration throws, the database will also be deleted and recreated.

const CURRENT_SCHEMA_VERSION = 2;

export class PlacesSemanticHistoryDatabase {
  #asyncShutdownBlocker;
  #conn;
  #databaseFolderPath;
  #embeddingSize;
  databaseFileName;
  #schemaVersion = CURRENT_SCHEMA_VERSION;

  constructor({ embeddingSize, fileName }) {
    this.#embeddingSize = embeddingSize;
    this.databaseFileName = fileName;
    this.#databaseFolderPath = PathUtils.profileDir;
  }

  get currentSchemaVersion() {
    return this.#schemaVersion;
  }

  async setCurrentSchemaVersionForTests(version) {
    this.#schemaVersion = version;
    if (this.#conn) {
      await this.#conn.setSchemaVersion(version);
    }
  }

  /**
   * Connects to the semantic.sqlite database and attaches the Places DB.
   *
   * @returns {Promise<object>}
   *   A promise resolving to the database connection.
   */
  async getConnection() {
    if (this.#conn) {
      return this.#conn;
    }
    try {
      // Connect to the database
      this.#conn = await this.#openConnection();
    } catch (e) {
      if (
        e.result == Cr.NS_ERROR_FILE_CORRUPTED ||
        e.errors?.some(error => error.result == Ci.mozIStorageError.NOTADB)
      ) {
        lazy.logger.info("Removing corrupted database files");
        await this.removeDatabaseFiles();
        this.#conn = await this.#openConnection();
      } else {
        lazy.logger.error("Failed to open connection", e);
        // Re-throw the exception for the caller.
        throw e;
      }
    }

    // Add shutdown blocker to close connection gracefully
    this.#asyncShutdownBlocker = async () => {
      await this.closeConnection();
    };
    lazy.Sqlite.shutdown.addBlocker(
      "PlacesSemanticHistoryDatabase: Shutdown",
      this.#asyncShutdownBlocker
    );

    try {
      lazy.logger.info("Initializing schema");
      await this.#initializeSchema();
    } catch (e) {
      lazy.logger.warn(`Schema initialization failed: ${e}`);
      // If the schema cannot be initialized close the connection and create
      // a new database file.
      await this.closeConnection();
      await this.removeDatabaseFiles();
      this.#conn = await this.#openConnection();
      await this.#initializeSchema();
    }

    return this.#conn;
  }

  async #openConnection() {
    lazy.logger.info("Trying to open connection");
    let conn = await lazy.Sqlite.openConnection({
      path: this.databaseFilePath,
      extensions: ["vec"],
    });

    // WAL is generally faster and allows for concurrent reads and writes.
    await conn.execute("PRAGMA journal_mode = WAL");
    await conn.execute("PRAGMA wal_autocheckpoint = 16");

    // We're not hooking up this to the vacuum manager yet, but let's start
    // storing vacuum information, in case we want to do that in the future.
    await conn.execute("PRAGMA auto_vacuum = INCREMENTAL");

    // Attach the Places database, as we need to join on it.
    let placesDbPath = PathUtils.join(
      this.#databaseFolderPath,
      "places.sqlite"
    );
    await conn.execute(`ATTACH DATABASE '${placesDbPath}' AS places`);
    return conn;
  }

  /**
   * Closes the connection to the database, if it's open.
   * @returns {Promise<void>} resolves when done.
   */
  async closeConnection() {
    if (this.#conn) {
      lazy.logger.info("Closing connection");
      lazy.Sqlite.shutdown.removeBlocker(this.#asyncShutdownBlocker);
      await this.#conn.close();
      this.#conn = null;
    }
  }

  /**
   * Initializes the semantic database, creating virtual tables if needed.
   * Any exception thrown here should be handled by the caller replacing the
   * database.
   */
  async #initializeSchema() {
    let version = await this.#conn.getSchemaVersion();
    lazy.logger.debug(`Database schema version: ${version}`);
    if (version > CURRENT_SCHEMA_VERSION) {
      lazy.logger.warn(`Database schema downgrade`);
      throw new Error("Downgrade of the schema is not supported");
    }
    if (version == CURRENT_SCHEMA_VERSION) {
      let healthy = await this.#checkDatabaseEntities(this.#embeddingSize);
      if (!healthy) {
        lazy.logger.error(`Database schema is not healthy`);
        throw new Error("Database schema is not healthy");
      }
      return;
    }

    await this.#conn.executeTransaction(async () => {
      if (version == 0) {
        // This is a newly created database, just create the entities.
        lazy.logger.info("Creating database schema");
        await this.#createDatabaseEntities(this.#embeddingSize);
        await this.#conn.setSchemaVersion(CURRENT_SCHEMA_VERSION);
        // eslint-disable-next-line no-useless-return
        return;
      }

      lazy.logger.info("Migrating database schema");

      // Put migrations here with a brief description of what they do.
      // If you want to fully replace the database with a new one, as the data
      // cannot be easily migrated, just throw an Error from the migration.

      if (version < 2) {
        // We found a critical issue in the relations between embeddings
        // and URLs, so we need to replace the database.
        throw new Error("Replacing semantic history database");
      }

      await this.#conn.setSchemaVersion(CURRENT_SCHEMA_VERSION);
    });
  }

  /**
   * Creates the necessary virtual tables in the semantic.sqlite database.
   * @param {number} embeddingSize - The size of the embedding.
   * @returns {Promise<void>} resolves when done.
   */
  async #createDatabaseEntities(embeddingSize) {
    await this.#conn.execute(`
      CREATE VIRTUAL TABLE vec_history USING vec0(
      embedding FLOAT[${embeddingSize}],
      embedding_coarse bit[${embeddingSize}]
      );
    `);
    await this.#conn.execute(`
      CREATE TABLE vec_history_mapping (
      rowid INTEGER PRIMARY KEY,
      url_hash INTEGER NOT NULL UNIQUE
      );
    `);
  }

  /**
   * Verifies that the schema is current, there's no missing entities or
   * changed embedding size.
   * @param {number} embeddingSize - The size of the embedding.
   * @returns {Promise<boolean>} whether the schema is consistent or not.
   */
  async #checkDatabaseEntities(embeddingSize) {
    let tables = await this.#conn.execute(
      `SELECT name FROM sqlite_master WHERE type='table'`
    );
    let tableNames = tables.map(row => row.getResultByName("name"));
    if (
      !tableNames.includes("vec_history") ||
      !tableNames.includes("vec_history_mapping")
    ) {
      lazy.logger.error(`Missing tables in the database`);
      return false;
    }

    // If the embedding size changed the database should be recreated. This
    // should be handled by a migration, but we check to be overly safe.
    let embeddingSizeMatches = (
      await this.#conn.execute(
        `SELECT INSTR(sql, :needle) > 0
       FROM sqlite_master WHERE name = 'vec_history'`,
        { needle: `FLOAT[${embeddingSize}]` }
      )
    )[0].getResultByIndex(0);
    if (!embeddingSizeMatches) {
      lazy.logger.error(`Embeddings size doesn't match`);
      return false;
    }

    return true;
  }

  /**
   * Returns the path to the semantic database.
   * @returns {string} The path to the semantic database.
   */
  get databaseFilePath() {
    return PathUtils.join(PathUtils.profileDir, this.databaseFileName);
  }

  /**
   * Removes the semantic database file and auxiliary files.
   * @returns {Promise<void>} resolves when done.
   */
  async removeDatabaseFiles() {
    lazy.logger.info("Removing database files");
    await this.closeConnection();
    try {
      for (let file of [
        this.databaseFilePath,
        PathUtils.join(
          this.#databaseFolderPath,
          this.databaseFileName + "-wal"
        ),
        PathUtils.join(
          this.#databaseFolderPath,
          this.databaseFileName + "-shm"
        ),
      ]) {
        await IOUtils.remove(file, {
          retryReadonly: true,
          recursive: true,
          ignoreAbsent: true,
        });
      }
    } catch (e) {
      // Try to clear on next startup.
      Services.prefs.setBoolPref(
        "places.semanticHistory.removeOnStartup",
        true
      );
      // Re-throw the exception for the caller.
      throw e;
    }
  }
}
