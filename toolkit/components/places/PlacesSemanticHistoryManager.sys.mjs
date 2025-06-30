/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * PlacesSemanticHistoryManager manages the semantic.sqlite database and provides helper
 * methods for initializing, querying, and updating semantic data.
 *
 * This module handles embeddings-based semantic search capabilities using the
 * Places database and an ML engine for vector operations.
 */

/**
 * @import {OpenedConnection} from "resource://gre/modules/Sqlite.sys.mjs"
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PlacesSemanticHistoryDatabase:
    "resource://gre/modules/PlacesSemanticHistoryDatabase.sys.mjs",
  EmbeddingsGenerator: "chrome://global/content/ml/EmbeddingsGenerator.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", function () {
  return lazy.PlacesUtils.getLogger({ prefix: "PlacesSemanticHistoryManager" });
});

// Constants to support an alternative frecency algorithm.
ChromeUtils.defineLazyGetter(lazy, "PAGES_FRECENCY_FIELD", () => {
  return lazy.PlacesUtils.history.isAlternativeFrecencyEnabled
    ? "alt_frecency"
    : "frecency";
});

// Time between deferred task executions.
const DEFERRED_TASK_INTERVAL_MS = 3000;
// Maximum time to wait for an idle before the task is executed anyway.
const DEFERRED_TASK_MAX_IDLE_WAIT_MS = 2 * 60000;
// Number of entries to update at once.
const DEFAULT_CHUNK_SIZE = 50;
const ONE_MiB = 1024 * 1024;
// minimum title length threshold; Usage len(title || description) > MIN_TITLE_LENGTH
const MIN_TITLE_LENGTH = 4;

class PlacesSemanticHistoryManager {
  #promiseConn;
  #engine = undefined;
  #embeddingSize;
  #rowLimit;
  #samplingAttrib;
  #changeThresholdCount;
  #distanceThreshold;
  #finalized = false;
  #updateTask = null;
  #prevPagesRankChangedCount = 0;
  #pendingUpdates = true;
  testFlag = false;
  #updateTaskLatency = [];
  embedder;
  qualifiedForSemanticSearch = false;
  #promiseRemoved = null;
  enoughEntries = false;
  #shutdownProgress = { state: "Not started" };
  #deferredTaskInterval = DEFERRED_TASK_INTERVAL_MS;
  #lastMaxChunksCount = 0;

  /**
   * Constructor for PlacesSemanticHistoryManager.
   *
   * @param {Object} options - Configuration options.
   * @param {number} [options.embeddingSize=384] - Size of embeddings used for vector operations.
   * @param {number} [options.rowLimit=600] - Maximum number of rows to process from the database.
   * @param {string} [options.samplingAttrib="frecency"] - Attribute used for sampling rows.
   * @param {number} [options.changeThresholdCount=3] - Threshold of changed rows to trigger updates.
   * @param {number} [options.distanceThreshold=0.6] - Cosine distance threshold to determine similarity.
   * @param {boolean} [options.testFlag=false] - Flag for test behavior.
   */
  constructor({
    embeddingSize = 384,
    rowLimit = 600,
    samplingAttrib = "frecency",
    changeThresholdCount = 3,
    distanceThreshold = 0.6,
    testFlag = false,
  } = {}) {
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);

    // Do not initialize during shutdown.
    if (
      Services.startup.isInOrBeyondShutdownPhase(
        Ci.nsIAppStartup.SHUTDOWN_PHASE_APPSHUTDOWNCONFIRMED
      )
    ) {
      this.#finalized = true;
      return;
    }
    this.embedder = new lazy.EmbeddingsGenerator(embeddingSize);
    this.semanticDB = new lazy.PlacesSemanticHistoryDatabase({
      embeddingSize,
      fileName: "places_semantic.sqlite",
    });
    this.qualifiedForSemanticSearch =
      this.embedder.isEnoughPhysicalMemoryAvailable() &&
      this.embedder.isEnoughCpuCoresAvailable();

    lazy.AsyncShutdown.appShutdownConfirmed.addBlocker(
      "SemanticManager: shutdown",
      () => this.shutdown(),
      { fetchState: () => this.#shutdownProgress }
    );

    // Add the observer for pages-rank-changed and history-cleared topics
    this.handlePlacesEvents = this.handlePlacesEvents.bind(this);
    lazy.PlacesUtils.observers.addListener(
      ["pages-rank-changed", "history-cleared", "page-removed"],
      this.handlePlacesEvents
    );

    this.#rowLimit = rowLimit;
    this.#embeddingSize = embeddingSize;
    this.#samplingAttrib = samplingAttrib;
    this.#changeThresholdCount = changeThresholdCount;
    this.#distanceThreshold = distanceThreshold;
    this.testFlag = testFlag;
    this.#updateTaskLatency = [];
    lazy.logger.trace("PlaceSemanticManager constructor");

    // When semantic history is disabled or not available anymore due to system
    // requirements, we want to remove the database files, though we don't want
    // to check on disk on every startup, thus we use a pref. The removal is
    // done on startup anyway, as it's less likely to fail.
    // We check UserValue because users may set it to false to try to disable
    // the feature, then if we'd check the value the files would not be removed.
    let wasInitialized = Services.prefs.prefHasUserValue(
      "places.semanticHistory.initialized"
    );
    let isAvailable = this.canUseSemanticSearch;
    let removeFiles =
      (wasInitialized && !isAvailable) ||
      Services.prefs.getBoolPref(
        "places.semanticHistory.removeOnStartup",
        false
      );
    if (removeFiles) {
      lazy.logger.info("Removing database files on startup");
      Services.prefs.clearUserPref("places.semanticHistory.removeOnStartup");
      this.#promiseRemoved = this.semanticDB
        .removeDatabaseFiles()
        .catch(console.error);
    }
    if (!isAvailable) {
      Services.prefs.clearUserPref("places.semanticHistory.initialized");
    } else if (!wasInitialized) {
      Services.prefs.setBoolPref("places.semanticHistory.initialized", true);
    }
  }

  /**
   * Connects to the semantic.sqlite database and attaches the Places DB.
   *
   * @returns {Promise<object>}
   *   A promise resolving to the database connection.
   */
  async getConnection() {
    if (
      Services.startup.isInOrBeyondShutdownPhase(
        Ci.nsIAppStartup.SHUTDOWN_PHASE_APPSHUTDOWNCONFIRMED
      ) ||
      !this.canUseSemanticSearch
    ) {
      return null;
    }
    // We must eventually wait for removal to finish.
    await this.#promiseRemoved;

    // Avoid re-entrance using a cached promise rather than handing off a conn.
    if (!this.#promiseConn) {
      this.#promiseConn = this.semanticDB.getConnection().then(conn => {
        // Kick off updates.
        this.#createOrUpdateTask();
        this.onPagesRankChanged();
        return conn;
      });
    }
    return this.#promiseConn;
  }

  /**
   * Checks whether the semantic-history vector DB is *sufficiently populated*.
   *
   * We look at the **top N** Places entries (N = `#rowLimit`, ordered by
   * `#samplingAttrib`) and count how many of them already have an embedding in
   * `vec_history_mapping`.  If **more than completionThreshold %** are *missing* we consider the
   * DB **not ready** and set to true when the completionThreshold reaches
   *
   * The boolean result is memoised in `this.enoughEntries`; subsequent
   * calls return that cached value to avoid repeating the query.
   *
   * @returns {Promise<boolean>}
   *   `true`  – **not enough** entries yet (pending / total ≥ completionThreshold)
   *   `false` – DB is sufficiently populated (pending / total < completionThreshold)
   */
  async hasSufficientEntriesForSearching() {
    if (this.enoughEntries) {
      // Return cached answer if we already ran once.
      return true;
    }
    let conn = await this.getConnection();

    // Compute total candidates and how many of them updated with vectors.
    const [row] = await conn.execute(
      `
      WITH top_places AS (
        SELECT url_hash FROM moz_places
        WHERE title NOTNULL
          AND length(title || ifnull(description,'')) > :min_title_length
          AND last_visit_date NOTNULL
          AND frecency > 0
        ORDER BY ${this.#samplingAttrib} DESC
        LIMIT :rowLimit
      )
      SELECT
        (SELECT COUNT(*) FROM top_places) AS total,
        (SELECT COUNT(*) FROM top_places tp
         JOIN vec_history_mapping map USING (url_hash)) AS completed
      `,
      {
        rowLimit: this.#rowLimit,
        min_title_length: MIN_TITLE_LENGTH,
      }
    );

    const total = row.getResultByName("total");
    const completed = row.getResultByName("completed");
    const ratio = total ? completed / total : 0;

    const completionThreshold = Services.prefs.getFloatPref(
      "places.semanticHistory.completionThreshold",
      0.5
    );
    // Ready once ≥ completionThreshold % completed.
    this.enoughEntries = ratio >= completionThreshold;

    if (this.enoughEntries) {
      lazy.logger.debug(
        `Semantic-DB status — completed: ${completed}/${total} ` +
          `(${(ratio * 100).toFixed(1)} %). ` +
          (this.enoughEntries
            ? "Threshold met; update task can run at normal cadence."
            : "Below threshold; updater remains armed for frequent updates.")
      );
    }

    return this.enoughEntries;
  }

  /**
   * Determines if semantic search can be used based on preferences
   * and hardware qualification criteria
   *
   * @returns {boolean} - Returns `true` if semantic search can be used,
   *   else false
   */
  get canUseSemanticSearch() {
    return (
      this.qualifiedForSemanticSearch &&
      Services.prefs.getBoolPref("browser.ml.enable", true) &&
      Services.prefs.getBoolPref("places.semanticHistory.featureGate", false)
    );
  }

  handlePlacesEvents(events) {
    for (const { type } of events) {
      switch (type) {
        case "pages-rank-changed":
        case "history-cleared":
        case "page-removed":
          this.onPagesRankChanged();
          break;
      }
    }
  }

  /**
   * Handles updates triggered by database changes or rank changes.
   *
   * This is invoked whenever the `"pages-rank-changed"` or
   * `"history-cleared"` event is observed.
   * It re-arms the DeferredTask for updates if not finalized.
   *
   * @private
   * @returns Promise<void>
   */
  async onPagesRankChanged() {
    if (this.#updateTask && !this.#updateTask.isFinalized) {
      lazy.logger.trace("Arm update task");
      this.#updateTask.arm();
    }
  }

  // getter for testing purposes
  getUpdateTaskLatency() {
    return this.#updateTaskLatency;
  }

  /**
   * Sets the DeferredTask interval for testing purposes.
   * @param {number} val minimum milliseconds between deferred task executions.
   */
  setDeferredTaskIntervalForTests(val) {
    this.#deferredTaskInterval = val;
  }

  /**
   * Creates or updates the DeferredTask for managing updates to the semantic DB.
   */
  #createOrUpdateTask() {
    if (this.#finalized) {
      lazy.logger.trace(`Not resurrecting #updateTask because finalized`);
      return;
    }
    if (this.#updateTask) {
      this.#updateTask.disarm();
      this.#updateTask.finalize().catch(console.error);
    }

    // Syncs the semantic search database with history changes. It first checks
    // if enough page changes have occurred to warrant an update. If so, it
    // finds history entries that need to be added or removed from the vector
    // database. It then processes a chunk of additions, for which it generates
    // embeddings, and deletions in batches. It will re-arm itself if more work
    // remains, otherwise marks the update as complete and notifies.
    this.#updateTask = new lazy.DeferredTask(
      async () => {
        if (this.#finalized) {
          return;
        }

        // Capture updateTask startTime.
        const updateStartTime = Cu.now();

        try {
          lazy.logger.info("Running vector DB update task...");
          let conn = await this.getConnection();
          let pagesRankChangedCount =
            PlacesObservers.counts.get("pages-rank-changed") +
            PlacesObservers.counts.get("history-cleared") +
            PlacesObservers.counts.get("page-removed");
          if (
            pagesRankChangedCount - this.#prevPagesRankChangedCount <
              this.#changeThresholdCount &&
            !this.#pendingUpdates &&
            !this.testFlag
          ) {
            lazy.logger.info("No significant changes detected.");
            return;
          }

          this.#prevPagesRankChangedCount = pagesRankChangedCount;
          const startTime = Cu.now();

          lazy.logger.info(
            `Changes exceed threshold (${this.#changeThresholdCount}).`
          );

          let { count: addCount, results: addRows } =
            await this.findAddsChunk(conn);
          let { count: deleteCount, results: deleteRows } =
            await this.findDeletesChunk(conn);

          // We already have startTime for profile markers, so just use it
          // instead of tracking timer within the distribution.
          Glean.places.semanticHistoryFindChunksTime.accumulateSingleSample(
            Cu.now() - startTime
          );

          lazy.logger.info(
            `Total rows to add: ${addCount}, delete: ${deleteCount}`
          );

          if (addCount || deleteCount) {
            let chunkTimer =
              Glean.places.semanticHistoryChunkCalculateTime.start();

            let chunksCount =
              Math.ceil(addCount / DEFAULT_CHUNK_SIZE) +
              Math.ceil(deleteCount / DEFAULT_CHUNK_SIZE);
            if (chunksCount > this.#lastMaxChunksCount) {
              this.#lastMaxChunksCount = chunksCount;
              Glean.places.semanticHistoryMaxChunksCount.set(chunksCount);
            }

            await this.updateVectorDB(conn, addRows, deleteRows);
            ChromeUtils.addProfilerMarker(
              "updateVectorDB",
              startTime,
              "Details about updateVectorDB event"
            );

            Glean.places.semanticHistoryChunkCalculateTime.stopAndAccumulate(
              chunkTimer
            );
          }

          if (
            addCount > DEFAULT_CHUNK_SIZE ||
            deleteCount > DEFAULT_CHUNK_SIZE
          ) {
            // There's still entries to update, re-arm the task.
            this.#pendingUpdates = true;
            this.#updateTask.arm();
            return;
          }

          this.#pendingUpdates = false;
          Services.obs.notifyObservers(
            null,
            "places-semantichistorymanager-update-complete"
          );
          if (this.testFlag) {
            this.#updateTask.arm();
          }
        } catch (error) {
          lazy.logger.error("Error executing vector DB update task:", error);
        } finally {
          lazy.logger.info("Vector DB update task completed.");
          const updateEndTime = Cu.now();
          const updateTaskTime = updateEndTime - updateStartTime;
          this.#updateTaskLatency.push(updateTaskTime);

          lazy.logger.info(
            `DeferredTask update completed in ${updateTaskTime} ms.`
          );
        }
      },
      this.#deferredTaskInterval,
      DEFERRED_TASK_MAX_IDLE_WAIT_MS
    );
    lazy.logger.info("Update task armed.");
  }

  /**
   * Finalizes the PlacesSemanticHistoryManager by cleaning up resources.
   *
   * This ensures any tasks are finalized and the manager is properly
   * cleaned up during shutdown.
   *
   */
  #finalize() {
    lazy.logger.trace("Finalizing SemanticManager");
    // We don't mind about tasks completiion, since we can execute them in the
    // next session.
    this.#updateTask?.disarm();
    this.#updateTask?.finalize().catch(console.error);
    this.#finalized = true;
  }

  /**
   * Find semantic vector entries to be added.
   *
   * @param {OpenedConnection} conn a SQLite connection to the database.
   * @returns Promise<{count: number, results: { url_hash: string } }>}
   *   Resolves to an array of objects containing results, limited to
   *   DEFAULT_CHUNK_SIZE elements, and the total count of found entries.
   */
  async findAddsChunk(conn) {
    // find any adds after successful checkForChanges
    const rows = await conn.executeCached(
      `
      WITH top_places AS (
        SELECT url_hash, trim(title || " " || IFNULL(description, '')) AS content
        FROM moz_places
        WHERE title NOTNULL
          AND length(title || ifnull(description,'')) > :min_title_length
          AND last_visit_date NOTNULL
          AND frecency > 0
        ORDER BY ${this.#samplingAttrib} DESC
        LIMIT :rowLimit
      ),
      updates AS (
        SELECT top.url_hash, top.content
        FROM top_places top
        LEFT JOIN vec_history_mapping map USING (url_hash)
        WHERE map.url_hash IS NULL
      )
      SELECT url_hash, content, (SELECT count(*) FROM updates) AS total
      FROM updates
      LIMIT :chunkSize
    `,
      {
        rowLimit: this.#rowLimit,
        min_title_length: MIN_TITLE_LENGTH,
        chunkSize: DEFAULT_CHUNK_SIZE,
      }
    );

    return {
      count: rows[0]?.getResultByName("total") || 0,
      results: rows.map(row => ({
        url_hash: row.getResultByName("url_hash"),
        content: row.getResultByName("content"),
      })),
    };
  }

  /**
   * Find semantic vector entries to eventually delete due to:
   * - Orphaning: URLs no longer in top_places
   * - Broken Mappings: rowid has no corresponding entry in vec_history
   *
   * @param {OpenedConnection} conn a SQLite connection to the database.
   * @returns Promise<{count: number, results: { url_hash: string } }>}
   *   Resolves to an array of objects containing results, limited to
   *   DEFAULT_CHUNK_SIZE elements, and the total count of found entries.
   */
  async findDeletesChunk(conn) {
    // find any deletes after successful checkForChanges
    const rows = await conn.executeCached(
      `
      WITH top_places AS (
        SELECT url_hash
        FROM moz_places
        WHERE title NOTNULL
          AND length(title || ifnull(description,'')) > :min_title_length
          AND last_visit_date NOTNULL
          AND frecency > 0
        ORDER BY ${this.#samplingAttrib} DESC
        LIMIT :rowLimit
      ),
      orphans AS (
        SELECT url_hash FROM vec_history_mapping
        EXCEPT
        SELECT url_hash FROM top_places
      ),
      updates AS (
        SELECT url_hash FROM orphans
        UNION
        SELECT url_hash FROM vec_history_mapping
        LEFT JOIN vec_history v USING (rowid)
        WHERE v.rowid IS NULL
      )
      SELECT url_hash, (SELECT count(*) FROM updates) AS total
      FROM updates
      LIMIT :chunkSize
    `,
      {
        rowLimit: this.#rowLimit,
        min_title_length: MIN_TITLE_LENGTH,
        chunkSize: DEFAULT_CHUNK_SIZE,
      }
    );

    return {
      count: rows[0]?.getResultByName("total") || 0,
      results: rows.map(row => ({
        url_hash: row.getResultByName("url_hash"),
      })),
    };
  }

  async updateVectorDB(conn, rowsToAdd, rowsToDelete) {
    await this.embedder.createEngineIfNotPresent();

    let batchTensors;
    if (rowsToAdd.length) {
      // Instead of calling engineRun in a loop for each row,
      // you prepare an array of requests.
      batchTensors = await this.embedder.embedMany(
        rowsToAdd.map(r => r.content)
      );
      if (batchTensors.length != rowsToAdd.length) {
        throw new Error(
          `Expected ${rowsToAdd.length} tensors, got ${batchTensors.length}`
        );
      }
    }

    await conn.executeTransaction(async () => {
      // Process each new row and the corresponding tensor.
      for (let i = 0; i < rowsToAdd.length; i++) {
        const { url_hash } = rowsToAdd[i];
        const tensor = batchTensors[i];
        try {
          if (!Array.isArray(tensor) || tensor.length !== this.#embeddingSize) {
            lazy.logger.error(
              `Got tensor with invalid length: ${Array.isArray(tensor) ? tensor.length : "non-array value"}`
            );
            continue;
          }

          // We first insert the url into vec_history_mapping, get the rowid
          // and then insert the embedding into vec_history using that.
          // Doing the opposite doesn't work, as RETURNING is not properly
          // supported by the vec extension.
          // See https://github.com/asg017/sqlite-vec/issues/229.

          // Normally there should be no conflict on url_hash, as we previously
          // checked for its existence in vec_history_mapping. Though, since
          // the hash is not unique, we may try to insert two pages with the
          // same hash value as part of the same chunk.
          let rows = await conn.executeCached(
            `
            INSERT INTO vec_history_mapping (rowid, url_hash)
            VALUES (NULL, :url_hash)
            /* This is apparently useless, but it makes RETURNING always return
               a value, while DO NOTHING would not. */
            ON CONFLICT(url_hash) DO UPDATE SET url_hash = :url_hash
            RETURNING rowid
            `,
            { url_hash }
          );
          const rowid = rows[0].getResultByName("rowid");
          if (!rowid) {
            lazy.logger.error(`Unable to get inserted rowid for: ${url_hash}`);
            continue;
          }

          // UPSERT or INSERT OR REPLACE are not yet supported by the sqlite-vec
          // extension, so we must manage the conflict manually.
          // See https://github.com/asg017/sqlite-vec/issues/127.
          try {
            await conn.executeCached(
              `
              INSERT INTO vec_history (rowid, embedding, embedding_coarse)
              VALUES (:rowid, :vector, vec_quantize_binary(:vector))
              `,
              { rowid, vector: this.tensorToBindable(tensor) }
            );
          } catch (error) {
            lazy.logger.trace(
              `Error while inserting new vector, possible conflict. Error (${error.result}): ${error.message}`
            );
            // Ideally we'd check for `error.result == Cr.NS_ERROR_STORAGE_CONSTRAINT`,
            // unfortunately sqlite-vec doesn't generate a SQLITE_CONSTRAINT
            // error in this case, so we get a generic NS_ERROR_FAILURE.
            await conn.executeCached(
              `
              DELETE FROM vec_history WHERE rowid = :rowid
              `,
              { rowid }
            );
            await conn.executeCached(
              `
              INSERT INTO vec_history (rowid, embedding, embedding_coarse)
              VALUES (:rowid, :vector, vec_quantize_binary(:vector))
              `,
              { rowid, vector: this.tensorToBindable(tensor) }
            );
          }

          lazy.logger.info(
            `Added embedding and mapping for url_hash: ${url_hash}`
          );
        } catch (error) {
          lazy.logger.error(
            `Failed to insert embedding for url_hash: ${url_hash}. Error: ${error.message}`
          );
        }
      }

      // Now apply deletions.
      for (let { url_hash } of rowsToDelete) {
        try {
          // Delete the mapping from vec_history_mapping table
          const rows = await conn.executeCached(
            `
            DELETE FROM vec_history_mapping
            WHERE url_hash = :url_hash
            RETURNING rowid
            `,
            { url_hash }
          );

          if (rows.length === 0) {
            lazy.logger.warn(`No mapping found for url_hash: ${url_hash}`);
            continue;
          }

          const rowid = rows[0].getResultByName("rowid");

          // Delete the embedding from vec_history table
          await conn.executeCached(
            `
            DELETE FROM vec_history
            WHERE rowid = :rowid
            `,
            { rowid }
          );

          lazy.logger.info(
            `Deleted embedding and mapping for url_hash: ${url_hash}`
          );
        } catch (error) {
          lazy.logger.error(
            `Failed to delete for url_hash: ${url_hash}. Error: ${error.message}`
          );
        }
      }
    });
  }

  /**
   * Shuts down the manager, ensuring cleanup of tasks and connections.
   */
  async shutdown() {
    this.#shutdownProgress.state = "In progress";
    await this.#finalize();
    this.#shutdownProgress.state = "Task finalized";
    await this.semanticDB.closeConnection();
    this.#shutdownProgress.state = "Connection closed";

    lazy.PlacesUtils.observers.removeListener(
      ["pages-rank-changed", "history-cleared", "page-removed"],
      this.handlePlacesEvents
    );

    this.#shutdownProgress.state = "Complete";
    lazy.logger.info("PlacesSemanticHistoryManager shut down.");
  }

  tensorToBindable(tensor) {
    if (!tensor) {
      throw new Error("tensorToBindable received an undefined tensor");
    }
    return new Uint8ClampedArray(new Float32Array(tensor).buffer);
  }

  /**
   * Executes an inference operation using the ML engine.
   *
   * This runs the engine's inference pipeline on the provided request and
   * checks if changes to the rank warrant triggering an update.
   *
   * @param {object} queryContext
   *   The request to run through the engine.
   * @param {string} queryContext.searchString
   *   The search string used for the request.
   * @returns {Promise<object>}
   *   The result of the engine's inference pipeline.
   */
  async infer(queryContext) {
    const inferStartTime = Cu.now();
    let results = [];
    await this.embedder.ensureEngine();
    let tensor = await this.embedder.embed(queryContext.searchString);

    if (!tensor) {
      return results;
    }

    let metrics = tensor.metrics;

    // If tensor is a nested array with a single element, extract the inner array.
    if (
      Array.isArray(tensor) &&
      tensor.length === 1 &&
      Array.isArray(tensor[0])
    ) {
      tensor = tensor[0];
    }

    if (!Array.isArray(tensor) || tensor.length !== this.#embeddingSize) {
      lazy.logger.info(`Got tensor with length ${tensor.length}`);
      return results;
    }
    let conn = await this.getConnection();

    let rows = await conn.executeCached(
      `
      WITH coarse_matches AS (
        SELECT rowid,
               embedding
        FROM vec_history
        WHERE embedding_coarse match vec_quantize_binary(:vector)
        ORDER BY distance
        LIMIT 100
      ),
      matches AS (
        SELECT url_hash, vec_distance_cosine(embedding, :vector) AS distance
        FROM vec_history_mapping
        JOIN coarse_matches USING (rowid)
        WHERE distance <= :distanceThreshold
        ORDER BY distance
        LIMIT 2
      )
      SELECT id,
             title,
             url,
             distance,
             frecency,
             last_visit_date
      FROM moz_places
      JOIN matches USING (url_hash)
      WHERE ${lazy.PAGES_FRECENCY_FIELD} <> 0
      ORDER BY distance
      `,
      {
        vector: this.tensorToBindable(tensor),
        distanceThreshold: this.#distanceThreshold,
      }
    );

    for (let row of rows) {
      results.push({
        id: row.getResultByName("id"),
        title: row.getResultByName("title"),
        url: row.getResultByName("url"),
        distance: row.getResultByName("distance"),
        frecency: row.getResultByName("frecency"),
        lastVisit: row.getResultByName("last_visit_date"),
      });
    }

    // Add a duration marker, representing a span of time, with some additional text
    ChromeUtils.addProfilerMarker(
      "semanticHistorySearch",
      inferStartTime,
      "semanticHistorySearch details"
    );

    return { results, metrics };
  }

  // for easier testing purpose.
  async engineRun(request) {
    return await this.#engine.run(request);
  }

  /**
   * Performs a WAL checkpoint to flush all pending writes from WAL to the main database file.
   * Then measures the final disk size of semantic.sqlite.
   * **This method is for test purposes only.**
   *
   * @returns {Promise<number>} - The size of `semantic.sqlite` in bytes after checkpointing.
   */
  async checkpointAndMeasureDbSize() {
    let conn = await this.getConnection();

    try {
      lazy.logger.info("Starting WAL checkpoint on semantic.sqlite");

      // Perform a full checkpoint to move WAL data into the main database file
      await conn.execute(`PRAGMA wal_checkpoint(FULL);`);
      await conn.execute(`PRAGMA wal_checkpoint(TRUNCATE);`);

      // Ensure database is in WAL mode
      let journalMode = await conn.execute(`PRAGMA journal_mode;`);
      lazy.logger.info(
        `Journal Mode after checkpoint: ${journalMode[0].getResultByName("journal_mode")}`
      );

      // Measure the size of `semantic.sqlite` after checkpoint
      const semanticDbPath = this.semanticDB.databaseFilePath;
      let { size } = await IOUtils.stat(semanticDbPath);
      const sizeInMB = size / ONE_MiB;

      lazy.logger.info(
        `Size of semantic.sqlite after checkpoint: ${sizeInMB} mb`
      );
      return sizeInMB;
    } catch (error) {
      lazy.logger.error(
        "Error during WAL checkpoint and size measurement:",
        error
      );
      return null;
    }
  }

  //getters
  getEmbeddingSize() {
    return this.#embeddingSize;
  }

  getRowLimit() {
    return this.#rowLimit;
  }

  getPrevPagesRankChangeCount() {
    return this.#prevPagesRankChangedCount;
  }

  getPendingUpdatesStatus() {
    return this.#pendingUpdates;
  }

  //for test purposes
  stopProcess() {
    this.#finalize();
  }
}

/**
 * @type {PlacesSemanticHistoryManager}
 *   Internal holder for the singleton.
 */
let gSingleton = null;

/**
 * Get the one shared semantic‐history manager.
 * @param {Object} [options] invokes PlacesSemanticHistoryManager constructor on first call or if recreate==true
 * @param {boolean} recreate set could true only for testing purposes and should not be true in production
 */
export function getPlacesSemanticHistoryManager(
  options = {},
  recreate = false
) {
  if (!gSingleton || recreate) {
    gSingleton = new PlacesSemanticHistoryManager(options);
  }
  return gSingleton;
}
