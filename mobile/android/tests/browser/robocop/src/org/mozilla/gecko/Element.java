/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

/** 
 *  Element provides access to a specific UI view (android.view.View). 
 *  See also Driver.findElement().
 */
public interface Element {

    /** Click on the element's view. Returns true on success. */
    boolean click();

    /** Returns true if the element is currently displayed */
    boolean isDisplayed();

    /** 
     * Returns the text currently displayed on the element, or null
     * if the text cannot be retrieved.
     */
    String getText();

    /** Returns the view ID */
    Integer getId();
}
