/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.android;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class BrowserContractHelpersTest {
    @Test
    public void testBookmarkCodes() {
        final String[] strings = {
                // Observe omissions: "microsummary", "item".
                "folder", "bookmark", "separator", "livemark", "query"
        };
        for (int i = 0; i < strings.length; ++i) {
            assertEquals(strings[i], BrowserContractHelpers.typeStringForCode(i));
            assertEquals(i, BrowserContractHelpers.typeCodeForString(strings[i]));
        }
        assertEquals(null, BrowserContractHelpers.typeStringForCode(-1));
        assertEquals(null, BrowserContractHelpers.typeStringForCode(100));

        assertEquals(-1, BrowserContractHelpers.typeCodeForString(null));
        assertEquals(-1, BrowserContractHelpers.typeCodeForString("folder "));
        assertEquals(-1, BrowserContractHelpers.typeCodeForString("FOLDER"));
        assertEquals(-1, BrowserContractHelpers.typeCodeForString(""));
        assertEquals(-1, BrowserContractHelpers.typeCodeForString("nope"));
    }
}