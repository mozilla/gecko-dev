add_task(function test_ESModule() {
  const URL1 = "resource://test/es6module_loaded-1.sys.mjs";
  const URL2 = "resource://test/es6module_loaded-2.sys.mjs";
  const URL3 = "resource://test/es6module_loaded-3.sys.mjs";

  Assert.ok(!Cu.loadedESModules.includes(URL1));
  Assert.ok(!Cu.isESModuleLoaded(URL1));
  Assert.ok(!Cu.loadedESModules.includes(URL2));
  Assert.ok(!Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.importESModule(URL1);

  Assert.ok(Cu.loadedESModules.includes(URL1));
  Assert.ok(Cu.isESModuleLoaded(URL1));
  Assert.ok(!Cu.loadedESModules.includes(URL2));
  Assert.ok(!Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.importESModule(URL2);

  Assert.ok(Cu.loadedESModules.includes(URL1));
  Assert.ok(Cu.isESModuleLoaded(URL1));
  Assert.ok(Cu.loadedESModules.includes(URL2));
  Assert.ok(Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.importESModule(URL3);

  Assert.ok(Cu.loadedESModules.includes(URL1));
  Assert.ok(Cu.isESModuleLoaded(URL1));
  Assert.ok(Cu.loadedESModules.includes(URL2));
  Assert.ok(Cu.isESModuleLoaded(URL2));
  Assert.ok(Cu.loadedESModules.includes(URL3));
  Assert.ok(Cu.isESModuleLoaded(URL3));
});
