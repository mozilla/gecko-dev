/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;

public class AndroidBrowserHistoryDataAccessor extends
    AndroidBrowserRepositoryDataAccessor {

  public AndroidBrowserHistoryDataAccessor(Context context) {
    super(context);
  }

  @Override
  protected Uri getUri() {
    return BrowserContractHelpers.HISTORY_CONTENT_URI;
  }

  @Override
  protected ContentValues getContentValues(Record record) {
    ContentValues cv = new ContentValues();
    HistoryRecord rec = (HistoryRecord) record;
    cv.put(BrowserContract.History.GUID, rec.guid);
    cv.put(BrowserContract.History.TITLE, rec.title);
    cv.put(BrowserContract.History.URL, rec.histURI);
    if (rec.visits != null) {
      JSONArray visits = rec.visits;
      long mostRecent = getLastVisited(visits);

      // Fennec stores history timestamps in milliseconds, and visit timestamps in microseconds.
      // The rest of Sync works in microseconds. This is the conversion point for records coming form Sync.
      cv.put(BrowserContract.History.DATE_LAST_VISITED, mostRecent / 1000);
      cv.put(BrowserContract.History.REMOTE_DATE_LAST_VISITED, mostRecent / 1000);
      cv.put(BrowserContract.History.VISITS, Long.toString(visits.size()));
    }
    return cv;
  }

  @Override
  protected String[] getAllColumns() {
    return BrowserContractHelpers.HistoryColumns;
  }

  @Override
  public Uri insert(Record record) {
    HistoryRecord rec = (HistoryRecord) record;

    Logger.debug(LOG_TAG, "Storing record " + record.guid);
    Uri newRecordUri = super.insert(record);

    Logger.debug(LOG_TAG, "Storing visits for " + record.guid);
    context.getContentResolver().bulkInsert(
            BrowserContract.Visits.CONTENT_URI,
            VisitsHelper.getVisitsContentValues(rec.guid, rec.visits)
    );

    return newRecordUri;
  }

  /**
   * Given oldGUID, first updates corresponding history record with new values (super operation),
   * and then inserts visits from the new record.
   * Existing visits from the old record are updated on database level to point to new GUID if necessary.
   *
   * @param oldGUID GUID of old <code>HistoryRecord</code>
   * @param newRecord new <code>HistoryRecord</code> to replace old one with, and insert visits from
   */
  @Override
  public void update(String oldGUID, Record newRecord) {
    // First, update existing history records with new values. This might involve changing history GUID,
    // and thanks to ON UPDATE CASCADE clause on Visits.HISTORY_GUID foreign key, visits will be "ported over"
    // to the new GUID.
    super.update(oldGUID, newRecord);

    // Now we need to insert any visits from the new record
    HistoryRecord rec = (HistoryRecord) newRecord;
    String newGUID = newRecord.guid;
    Logger.debug(LOG_TAG, "Storing visits for " + newGUID + ", replacing " + oldGUID);

    context.getContentResolver().bulkInsert(
            BrowserContract.Visits.CONTENT_URI,
            VisitsHelper.getVisitsContentValues(newGUID, rec.visits)
    );
  }

  /**
   * Insert records.
   * <p>
   * This inserts all the records (using <code>ContentProvider.bulkInsert</code>),
   * then inserts all the visit information (also using <code>ContentProvider.bulkInsert</code>).
   *
   * @param records
   *          the records to insert.
   * @return
   *          the number of records actually inserted.
   * @throws NullCursorException
   */
  public int bulkInsert(ArrayList<HistoryRecord> records) throws NullCursorException {
    if (records.isEmpty()) {
      Logger.debug(LOG_TAG, "No records to insert, returning.");
    }

    int size = records.size();
    ContentValues[] cvs = new ContentValues[size];
    int index = 0;
    for (Record record : records) {
      if (record.guid == null) {
        throw new IllegalArgumentException("Record with null GUID passed in to bulkInsert.");
      }
      cvs[index] = getContentValues(record);
      index += 1;
    }

    // First update the history records.
    int inserted = context.getContentResolver().bulkInsert(getUri(), cvs);
    if (inserted == size) {
      Logger.debug(LOG_TAG, "Inserted " + inserted + " records, as expected.");
    } else {
      Logger.debug(LOG_TAG, "Inserted " +
                   inserted + " records but expected " +
                   size     + " records; continuing to update visits.");
    }

    final ContentValues remoteVisitAggregateValues = new ContentValues();
    final Uri historyIncrementRemoteAggregateUri = getUri().buildUpon()
            .appendQueryParameter(BrowserContract.PARAM_INCREMENT_REMOTE_AGGREGATES, "true")
            .build();
    for (Record record : records) {
      HistoryRecord rec = (HistoryRecord) record;
      if (rec.visits != null && rec.visits.size() != 0) {
        int remoteVisitsInserted = context.getContentResolver().bulkInsert(
                BrowserContract.Visits.CONTENT_URI,
                VisitsHelper.getVisitsContentValues(rec.guid, rec.visits)
        );

        // If we just inserted any visits, update remote visit aggregate values.
        // While inserting visits, we might not insert all of rec.visits - if we already have a local
        // visit record with matching (guid,date), we will skip that visit.
        // Remote visits aggregate value will be incremented by number of visits inserted.
        // Note that we don't need to set REMOTE_DATE_LAST_VISITED, because it already gets set above.
        if (remoteVisitsInserted > 0) {
          // Note that REMOTE_VISITS must be set before calling cr.update(...) with a URI
          // that has PARAM_INCREMENT_REMOTE_AGGREGATES=true.
          remoteVisitAggregateValues.put(BrowserContract.History.REMOTE_VISITS, remoteVisitsInserted);
          context.getContentResolver().update(
                  historyIncrementRemoteAggregateUri,
                  remoteVisitAggregateValues,
                  BrowserContract.History.GUID + " = ?", new String[] {rec.guid}
          );
        }
      }
    }

    return inserted;
  }

  /**
   * Helper method used to find largest <code>VisitsHelper.SYNC_DATE_KEY</code> value in a provided JSONArray.
   *
   * @param visits Array of objects which will be searched.
   * @return largest value of <code>VisitsHelper.SYNC_DATE_KEY</code>.
     */
  private long getLastVisited(JSONArray visits) {
    long mostRecent = 0;
    for (int i = 0; i < visits.size(); i++) {
      final JSONObject visit = (JSONObject) visits.get(i);
      long visitDate = (Long) visit.get(VisitsHelper.SYNC_DATE_KEY);
      if (visitDate > mostRecent) {
        mostRecent = visitDate;
      }
    }
    return mostRecent;
  }
}
