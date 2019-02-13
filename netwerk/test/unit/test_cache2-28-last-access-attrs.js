function run_test()
{
  do_get_profile();
  function NowSeconds() {
    return parseInt((new Date()).getTime() / 1000);
  }
  function do_check_time(t, min, max) {
    do_check_true(t >= min);
    do_check_true(t <= max);
  }

  var timeStart = NowSeconds();

  asyncOpenCacheEntry("http://t/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "m", "d", function(entry) {

      var firstOpen = NowSeconds();
      do_check_eq(entry.fetchCount, 1);
      do_check_time(entry.lastFetched, timeStart, firstOpen);
      do_check_time(entry.lastModified, timeStart, firstOpen);

      do_timeout(2000, () => {
        asyncOpenCacheEntry("http://t/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
          new OpenCallback(NORMAL, "m", "d", function(entry) {

            var secondOpen = NowSeconds();
            do_check_eq(entry.fetchCount, 2);
            do_check_time(entry.lastFetched, firstOpen, secondOpen);
            do_check_time(entry.lastModified, timeStart, firstOpen);

            finish_cache2_test();
          })
        );
      })
    })
  );

  do_test_pending();
}
