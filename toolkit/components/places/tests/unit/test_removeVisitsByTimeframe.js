/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const bmsvc = PlacesUtils.bookmarks;
const histsvc = PlacesUtils.history;

const NOW = Date.now() * 1000;
const TEST_URL = "http://example.com/";
const TEST_URI = uri(TEST_URL);
const PLACE_URL = "place:queryType=0&sort=8&maxResults=10";
const PLACE_URI = uri(PLACE_URL);

var tests = [
  {
    desc: "Remove some visits outside valid timeframe from an unbookmarked URI",
    run:   function () {
      print("Add 10 visits for the URI from way in the past.");
      let visits = [];
      for (let i = 0; i < 10; i++) {
        visits.push({ uri: TEST_URI, visitDate: NOW - 1000 - i });
      }
      promiseAddVisits(visits).then(this.continue_run.bind(this));
    },
    continue_run: function () {
      print("Remove visits using timerange outside the URI's visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that all visits still exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 10);
      for (let i = 0; i < resultRoot.childCount; i++) {
        var visitTime = resultRoot.getChild(i).time;
        do_check_eq(visitTime, NOW - 1000 - i);
      }
      resultRoot.containerOpen = false;

      print("asyncHistory.isURIVisited should return true.");
      PlacesUtils.asyncHistory.isURIVisited(TEST_URI, function(aURI, aIsVisited) {
        do_check_true(aIsVisited);

        promiseAsyncUpdates().then(function () {
          print("Frecency should be positive.")
          do_check_true(frecencyForUrl(TEST_URI) > 0);
          run_next_test();
        });
      });
    }
  },

  {
    desc: "Remove some visits outside valid timeframe from a bookmarked URI",
    run:   function () {
      print("Add 10 visits for the URI from way in the past.");
      let visits = [];
      for (let i = 0; i < 10; i++) {
        visits.push({ uri: TEST_URI, visitDate: NOW - 1000 - i });
      }
      promiseAddVisits(visits).then(function () {
        print("Bookmark the URI.");
        bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                             TEST_URI,
                             bmsvc.DEFAULT_INDEX,
                             "bookmark title");

        promiseAsyncUpdates().then(this.continue_run.bind(this));
      }.bind(this));
    },
    continue_run: function () {
      print("Remove visits using timerange outside the URI's visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that all visits still exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 10);
      for (let i = 0; i < resultRoot.childCount; i++) {
        var visitTime = resultRoot.getChild(i).time;
        do_check_eq(visitTime, NOW - 1000 - i);
      }
      resultRoot.containerOpen = false;

      print("asyncHistory.isURIVisited should return true.");
      PlacesUtils.asyncHistory.isURIVisited(TEST_URI, function(aURI, aIsVisited) {
        do_check_true(aIsVisited);

        promiseAsyncUpdates().then(function () {
          print("Frecency should be positive.")
          do_check_true(frecencyForUrl(TEST_URI) > 0);
          run_next_test();
        });
      });
    }
  },

  {
    desc: "Remove some visits from an unbookmarked URI",
    run:   function () {
      print("Add 10 visits for the URI from now to 9 usecs in the past.");
      let visits = [];
      for (let i = 0; i < 10; i++) {
        visits.push({ uri: TEST_URI, visitDate: NOW - i });
      }
      promiseAddVisits(visits).then(this.continue_run.bind(this));
    },
    continue_run: function () {
      print("Remove the 5 most recent visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 4, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that only the older 5 visits " +
            "still exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 5);
      for (let i = 0; i < resultRoot.childCount; i++) {
        var visitTime = resultRoot.getChild(i).time;
        do_check_eq(visitTime, NOW - i - 5);
      }
      resultRoot.containerOpen = false;

      print("asyncHistory.isURIVisited should return true.");
      PlacesUtils.asyncHistory.isURIVisited(TEST_URI, function(aURI, aIsVisited) {
        do_check_true(aIsVisited);

        promiseAsyncUpdates().then(function () {
          print("Frecency should be positive.")
          do_check_true(frecencyForUrl(TEST_URI) > 0);
          run_next_test();
        });
      });
    }
  },

  {
    desc: "Remove some visits from a bookmarked URI",
    run:   function () {
      print("Add 10 visits for the URI from now to 9 usecs in the past.");
      let visits = [];
      for (let i = 0; i < 10; i++) {
        visits.push({ uri: TEST_URI, visitDate: NOW - i });
      }
      promiseAddVisits(visits).then(function () {
        print("Bookmark the URI.");
        bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                             TEST_URI,
                             bmsvc.DEFAULT_INDEX,
                             "bookmark title");
        promiseAsyncUpdates().then(this.continue_run.bind(this));
      }.bind(this));
    },
    continue_run: function () {
      print("Remove the 5 most recent visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 4, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that only the older 5 visits " +
            "still exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 5);
      for (let i = 0; i < resultRoot.childCount; i++) {
        var visitTime = resultRoot.getChild(i).time;
        do_check_eq(visitTime, NOW - i - 5);
      }
      resultRoot.containerOpen = false;

      print("asyncHistory.isURIVisited should return true.");
      PlacesUtils.asyncHistory.isURIVisited(TEST_URI, function(aURI, aIsVisited) {
        do_check_true(aIsVisited);

        promiseAsyncUpdates().then(function () {
          print("Frecency should be positive.")
          do_check_true(frecencyForUrl(TEST_URI) > 0);
          run_next_test();
        });
      });
    }
  },

  {
    desc: "Remove all visits from an unbookmarked URI",
    run:   function () {
      print("Add some visits for the URI.");
      let visits = [];
      for (let i = 0; i < 10; i++) {
        visits.push({ uri: TEST_URI, visitDate: NOW - i });
      }
      promiseAddVisits(visits).then(this.continue_run.bind(this));
    },
    continue_run: function () {
      print("Remove all visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should no longer exist in moz_places.");
      do_check_false(page_in_database(TEST_URL));

      print("Run a history query and check that no visits exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 0);
      resultRoot.containerOpen = false;

      print("asyncHistory.isURIVisited should return false.");
      PlacesUtils.asyncHistory.isURIVisited(TEST_URI, function(aURI, aIsVisited) {
        do_check_false(aIsVisited);
        run_next_test();
      });
    }
  },

  {
    desc: "Remove all visits from an unbookmarked place: URI",
    run:   function () {
      print("Add some visits for the URI.");
      let visits = [];
      for (let i = 0; i < 10; i++) {
        visits.push({ uri: PLACE_URI, visitDate: NOW - i });
      }
      promiseAddVisits(visits).then(this.continue_run.bind(this));
    },
    continue_run: function () {
      print("Remove all visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(PLACE_URL));

      print("Run a history query and check that no visits exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 0);
      resultRoot.containerOpen = false;

      print("asyncHistory.isURIVisited should return false.");
      PlacesUtils.asyncHistory.isURIVisited(PLACE_URI, function(aURI, aIsVisited) {
        do_check_false(aIsVisited);

        promiseAsyncUpdates().then(function () {
          print("Frecency should be zero.")
          do_check_eq(frecencyForUrl(PLACE_URL), 0);
          run_next_test();
        });
      });
    }
  },

  {
    desc: "Remove all visits from a bookmarked URI",
    run:   function () {
      print("Add some visits for the URI.");
      let visits = [];
      for (let i = 0; i < 10; i++) {
        visits.push({ uri: TEST_URI, visitDate: NOW - i });
      }
      promiseAddVisits(visits).then(function () {
        print("Bookmark the URI.");
        bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                             TEST_URI,
                             bmsvc.DEFAULT_INDEX,
                             "bookmark title");
        promiseAsyncUpdates().then(this.continue_run.bind(this));
      }.bind(this));
    },
    continue_run: function () {
      print("Remove all visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that no visits exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 0);
      resultRoot.containerOpen = false;

      print("asyncHistory.isURIVisited should return false.");
      PlacesUtils.asyncHistory.isURIVisited(TEST_URI, function(aURI, aIsVisited) {
        do_check_false(aIsVisited);

        print("nsINavBookmarksService.isBookmarked should return true.");
        do_check_true(bmsvc.isBookmarked(TEST_URI));

        promiseAsyncUpdates().then(function () {
          print("Frecency should be negative.")
          do_check_true(frecencyForUrl(TEST_URI) < 0);
          run_next_test();
        });
      });
    }
  },

  {
    desc: "Remove some visits from a zero frecency URI retains zero frecency",
    run: function () {
      do_log_info("Add some visits for the URI.");
      promiseAddVisits([{ uri: TEST_URI, transition: TRANSITION_FRAMED_LINK,
                          visitDate: (NOW - 86400000000) },
                        { uri: TEST_URI, transition: TRANSITION_FRAMED_LINK,
                          visitDate: NOW }]).then(
                       this.continue_run.bind(this));
    },
    continue_run: function () {
      do_log_info("Remove newer visit.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      promiseAsyncUpdates().then(function() {
        do_log_info("URI should still exist in moz_places.");
        do_check_true(page_in_database(TEST_URL));
        do_log_info("Frecency should be zero.")
        do_check_eq(frecencyForUrl(TEST_URI), 0);
        run_next_test();
      });
    }
  }
];

///////////////////////////////////////////////////////////////////////////////

function run_test()
{
  do_test_pending();
  run_next_test();
}

function run_next_test() {
  if (tests.length) {
    let test = tests.shift();
    print("\n ***Test: " + test.desc);
    promiseClearHistory().then(function() {
      remove_all_bookmarks();
      DBConn().executeSimpleSQL("DELETE FROM moz_places");
      test.run.call(test);
    });
  }
  else {
    do_test_finished();
  }
}
