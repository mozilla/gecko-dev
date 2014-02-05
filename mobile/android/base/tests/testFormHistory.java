package org.mozilla.gecko.tests;

import org.mozilla.gecko.db.BrowserContract.FormHistory;

import android.content.ContentValues;
import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import java.io.File;

/**
 * A basic form history contentprovider test.
 * - inserts an element in form history when it is not yet set up
 * - inserts an element in form history
 * - updates an element in form history
 * - deletes an element in form history
 */
public class testFormHistory extends BaseTest {
    private static final String DB_NAME = "formhistory.sqlite";

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testFormHistory() {
        Context context = (Context)getActivity();
        ContentResolver cr = context.getContentResolver();
        ContentValues[] cvs = new ContentValues[1];
        cvs[0] = new ContentValues();
 
        blockForGeckoDelayedStartup();

        Uri formHistoryUri;
        Uri insertUri;
        Uri expectedUri;
        int numUpdated;
        int numDeleted;

        cvs[0].put("fieldname", "fieldname");
        cvs[0].put("value", "value");
        cvs[0].put("timesUsed", "0");
        cvs[0].put("guid", "guid");

        // Attempt to insert into the db
        formHistoryUri = FormHistory.CONTENT_URI;
        Uri.Builder builder = formHistoryUri.buildUpon();
        formHistoryUri = builder.appendQueryParameter("profilePath", mProfile).build();

        insertUri = cr.insert(formHistoryUri, cvs[0]);
        expectedUri = formHistoryUri.buildUpon().appendPath("1").build();
        mAsserter.is(expectedUri.toString(), insertUri.toString(), "Insert returned correct uri");
        SqliteCompare(DB_NAME, "SELECT * FROM moz_formhistory", cvs);
  
        cvs[0].put("fieldname", "fieldname2");
        cvs[0].putNull("guid");
  
        numUpdated = cr.update(formHistoryUri, cvs[0], null, null);
        mAsserter.is(1, numUpdated, "Correct number updated");
        SqliteCompare(DB_NAME, "SELECT * FROM moz_formhistory", cvs);
  
        numDeleted = cr.delete(formHistoryUri, null, null);
        mAsserter.is(1, numDeleted, "Correct number deleted");
        cvs = new ContentValues[0];
        SqliteCompare(DB_NAME, "SELECT * FROM moz_formhistory", cvs);
        
        cvs = new ContentValues[1];
        cvs[0] = new ContentValues();
        cvs[0].put("fieldname", "fieldname");
        cvs[0].put("value", "value");
        cvs[0].put("timesUsed", "0");
        cvs[0].putNull("guid");

        insertUri = cr.insert(formHistoryUri, cvs[0]);
        expectedUri = formHistoryUri.buildUpon().appendPath("1").build();
        mAsserter.is(expectedUri.toString(), insertUri.toString(), "Insert returned correct uri");
        SqliteCompare(DB_NAME, "SELECT * FROM moz_formhistory", cvs);
  
        cvs[0].put("guid", "guid");
  
        numUpdated = cr.update(formHistoryUri, cvs[0], null, null);
        mAsserter.is(1, numUpdated, "Correct number updated");
        SqliteCompare(DB_NAME, "SELECT * FROM moz_formhistory", cvs);
 
        numDeleted = cr.delete(formHistoryUri, null, null);
        mAsserter.is(1, numDeleted, "Correct number deleted");
        cvs = new ContentValues[0];
        SqliteCompare(DB_NAME, "SELECT * FROM moz_formhistory", cvs);
    }

    @Override
    public void tearDown() throws Exception {
        // remove the entire signons.sqlite file
        File profile = new File(mProfile);
        File db = new File(profile, "formhistory.sqlite");
        if (db.delete()) {
            mAsserter.dumpLog("tearDown deleted "+db.toString());
        } else {
            mAsserter.dumpLog("tearDown did not delete "+db.toString());
        }

        super.tearDown();
    }
}
