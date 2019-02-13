/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that dragging and dropping sites works as expected.
 * Sites contained in the grid need to shift around to indicate the result
 * of the drag-and-drop operation. If the grid is full and we're dragging
 * a new site into it another one gets pushed out.
 */
function runTests() {
  requestLongerTimeout(2);
  yield addNewTabPageTab();

  // test a simple drag-and-drop scenario
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  yield simulateDrop(0, 1);
  checkGrid("1,0p,2,3,4,5,6,7,8");

  // drag a cell to its current cell and make sure it's not pinned afterwards
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  yield simulateDrop(0, 0);
  checkGrid("0,1,2,3,4,5,6,7,8");

  // ensure that pinned pages aren't moved if that's not necessary
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",1,2");

  yield addNewTabPageTab();
  checkGrid("0,1p,2p,3,4,5,6,7,8");

  yield simulateDrop(0, 3);
  checkGrid("3,1p,2p,0p,4,5,6,7,8");

  // pinned sites should always be moved around as blocks. if a pinned site is
  // moved around, neighboring pinned are affected as well
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1");

  yield addNewTabPageTab();
  checkGrid("0p,1p,2,3,4,5,6,7,8");

  yield simulateDrop(2, 0);
  checkGrid("2p,0p,1p,3,4,5,6,7,8");

  // pinned sites should not be pushed out of the grid (unless there are only
  // pinned ones left on the grid)
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7p,8p");

  yield simulateDrop(2, 5);
  checkGrid("0,1,3,4,5,2p,6,7p,8p");

  // make sure that pinned sites are re-positioned correctly
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,5");

  yield addNewTabPageTab();
  checkGrid("0p,1p,2p,3,4,5p,6,7,8");

  yield simulateDrop(0, 4);
  checkGrid("3,1p,2p,4,0p,5p,6,7,8");
}
