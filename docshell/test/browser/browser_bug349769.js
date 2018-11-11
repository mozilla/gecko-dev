add_task(function* test() {
  const secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);
  const uris = [undefined, "about:blank"];

  function checkContentProcess(uri) {
    yield ContentTask.spawn(newBrowser, uri, function* (uri) {
      var prin = content.document.nodePrincipal;
      Assert.notEqual(prin, null, "Loaded principal must not be null when adding " + uri);
      Assert.notEqual(prin, undefined, "Loaded principal must not be undefined when loading " + uri);

      const secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
                       .getService(Ci.nsIScriptSecurityManager);
      Assert.equal(secMan.isSystemPrincipal(prin), false,
         "Loaded principal must not be system when loading " + uri);
    });
  }

  for (var uri of uris) {
    yield BrowserTestUtils.withNewTab({ gBrowser }, function* (newBrowser) {
      yield BrowserTestUtils.loadURI(newBrowser, uri);

      var prin = newBrowser.contentPrincipal;
      isnot(prin, null, "Forced principal must not be null when loading " + uri);
      isnot(prin, undefined,
            "Forced principal must not be undefined when loading " + uri);
      is(secMan.isSystemPrincipal(prin), false,
         "Forced principal must not be system when loading " + uri);

      // Belt-and-suspenders e10s check: make sure that the same checks hold
      // true in the content process.
      checkContentProcess(uri);

      yield BrowserTestUtils.browserLoaded(newBrowser);

      prin = newBrowser.contentPrincipal;
      isnot(prin, null, "Loaded principal must not be null when adding " + uri);
      isnot(prin, undefined, "Loaded principal must not be undefined when loading " + uri);
      is(secMan.isSystemPrincipal(prin), false,
         "Loaded principal must not be system when loading " + uri);

      // Belt-and-suspenders e10s check: make sure that the same checks hold
      // true in the content process.
      checkContentProcess(uri);
    });
  }
});

