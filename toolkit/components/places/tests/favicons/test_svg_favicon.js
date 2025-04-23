const PAGEURI = NetUtil.newURI("http://deliciousbacon.com/");

add_task(async function () {
  // First, add a history entry or else Places can't save a favicon.
  await PlacesTestUtils.addVisits({
    uri: PAGEURI,
    transition: TRANSITION_LINK,
    visitDate: Date.now() * 1000,
  });

  await PlacesUtils.favicons.setFaviconForPage(
    PAGEURI,
    SMALLSVG_DATA_URI,
    SMALLSVG_DATA_URI
  );

  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
      PAGEURI,
      function (aURI, aDataLen, aData, aMimeType) {
        Assert.equal(
          aURI.spec,
          SMALLSVG_DATA_URI.spec,
          "setFavicon aURI check"
        );
        Assert.equal(aDataLen, 263, "setFavicon aDataLen check");
        Assert.equal(aMimeType, "image/svg+xml", "setFavicon aMimeType check");
        resolve();
      }
    );
  });

  let data = await PlacesUtils.promiseFaviconData(PAGEURI.spec);
  equal(data.uri.spec, SMALLSVG_DATA_URI.spec, "getFavicon aURI check");
  equal(data.dataLen, 263, "getFavicon aDataLen check");
  equal(data.mimeType, "image/svg+xml", "getFavicon aMimeType check");
});
