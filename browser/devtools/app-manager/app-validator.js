/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let {Ci,Cu,CC} = require("chrome");
const promise = require("devtools/toolkit/deprecated-sync-thenables");

const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
let XMLHttpRequest = CC("@mozilla.org/xmlextras/xmlhttprequest;1");
let strings = Services.strings.createBundle("chrome://browser/locale/devtools/app-manager.properties");

function AppValidator({ type, location }) {
  this.type = type;
  this.location = location;
  this.errors = [];
  this.warnings = [];
}

AppValidator.prototype.error = function (message) {
  this.errors.push(message);
};

AppValidator.prototype.warning = function (message) {
  this.warnings.push(message);
};

AppValidator.prototype._getPackagedManifestFile = function () {
  let manifestFile = FileUtils.File(this.location);
  if (!manifestFile.exists()) {
    this.error(strings.GetStringFromName("validator.nonExistingFolder"));
    return null;
  }
  if (!manifestFile.isDirectory()) {
    this.error(strings.GetStringFromName("validator.expectProjectFolder"));
    return null;
  }
  manifestFile.append("manifest.webapp");
  if (!manifestFile.exists() || !manifestFile.isFile()) {
    this.error(strings.GetStringFromName("validator.wrongManifestFileName"));
    return null;
  }
  return manifestFile;
};

AppValidator.prototype._getPackagedManifestURL = function () {
  let manifestFile = this._getPackagedManifestFile();
  if (!manifestFile) {
    return null;
  }
  return Services.io.newFileURI(manifestFile).spec;
};

AppValidator.checkManifest = function(manifestURL) {
  let deferred = promise.defer();
  let error;

  let req = new XMLHttpRequest();
  req.overrideMimeType('text/plain');

  try {
    req.open("GET", manifestURL, true);
  } catch(e) {
    error = strings.formatStringFromName("validator.invalidManifestURL", [manifestURL], 1);
    deferred.reject(error);
    return deferred.promise;
  }
  req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE | Ci.nsIRequest.INHIBIT_CACHING;

  req.onload = function () {
    let manifest = null;
    try {
      manifest = JSON.parse(req.responseText);
    } catch(e) {
      error = strings.formatStringFromName("validator.invalidManifestJSON", [e, manifestURL], 2);
      deferred.reject(error);
    }

    deferred.resolve({manifest, manifestURL});
  };

  req.onerror = function () {
    error = strings.formatStringFromName("validator.noAccessManifestURL", [req.statusText, manifestURL], 2);
    deferred.reject(error);
 };

  try {
    req.send(null);
  } catch(e) {
    error = strings.formatStringFromName("validator.noAccessManifestURL", [e, manifestURL], 2);
    deferred.reject(error);
  }

  return deferred.promise;
};

AppValidator.findManifestAtOrigin = function(manifestURL) {
  let fixedManifest = Services.io.newURI(manifestURL, null, null).prePath + '/manifest.webapp';
  return AppValidator.checkManifest(fixedManifest);
};

AppValidator.findManifestPath = function(manifestURL) {
  let deferred = promise.defer();

  if (manifestURL.endsWith('manifest.webapp')) {
    deferred.reject();
  } else {
    let fixedManifest = manifestURL + '/manifest.webapp';
    deferred.resolve(AppValidator.checkManifest(fixedManifest));
  }

  return deferred.promise;
};

AppValidator.checkAlternateManifest = function(manifestURL) {
  return Task.spawn(function*() {
    let result;
    try {
      result = yield AppValidator.findManifestPath(manifestURL);
    } catch(e) {
      result = yield AppValidator.findManifestAtOrigin(manifestURL);
    }

    return result;
  });
};

AppValidator.prototype._fetchManifest = function (manifestURL) {
  let deferred = promise.defer();
  this.manifestURL = manifestURL;

  AppValidator.checkManifest(manifestURL)
              .then(({manifest, manifestURL}) => {
                deferred.resolve(manifest);
              }, error => {
                AppValidator.checkAlternateManifest(manifestURL)
                            .then(({manifest, manifestURL}) => {
                              this.manifestURL = manifestURL;
                              deferred.resolve(manifest);
                            }, () => {
                              this.error(error);
                              deferred.resolve(null);
                            });
                });

  return deferred.promise;
};

AppValidator.prototype._getManifest = function () {
  let manifestURL;
  if (this.type == "packaged") {
    manifestURL = this._getPackagedManifestURL();
    if (!manifestURL)
      return promise.resolve(null);
  } else if (this.type == "hosted") {
    manifestURL = this.location;
    try {
      Services.io.newURI(manifestURL, null, null);
    } catch(e) {
      this.error(strings.formatStringFromName("validator.invalidHostedManifestURL", [manifestURL, e.message], 2));
      return promise.resolve(null);
    }
  } else {
    this.error(strings.formatStringFromName("validator.invalidProjectType", [this.type], 1));
    return promise.resolve(null);
  }
  return this._fetchManifest(manifestURL);
};

AppValidator.prototype.validateManifest = function (manifest) {
  if (!manifest.name) {
    this.error(strings.GetStringFromName("validator.missNameManifestProperty"));
  }

  if (!manifest.icons || Object.keys(manifest.icons).length === 0) {
    this.warning(strings.GetStringFromName("validator.missIconsManifestProperty"));
  } else if (!manifest.icons["128"]) {
    this.warning(strings.GetStringFromName("validator.missIconMarketplace2"));
  }
};

AppValidator.prototype._getOriginURL = function () {
  if (this.type == "packaged") {
    let manifestURL = Services.io.newURI(this.manifestURL, null, null);
    return Services.io.newURI(".", null, manifestURL).spec;
  } else if (this.type == "hosted") {
    return Services.io.newURI(this.location, null, null).prePath;
  }
};

AppValidator.prototype.validateLaunchPath = function (manifest) {
  // Addons don't use index page (yet?)
  if (manifest.role && manifest.role === "addon") {
    return promise.resolve();
  }
  let deferred = promise.defer();
  // The launch_path field has to start with a `/`
  if (manifest.launch_path && manifest.launch_path[0] !== "/") {
    this.error(strings.formatStringFromName("validator.nonAbsoluteLaunchPath", [manifest.launch_path], 1));
    deferred.resolve();
    return deferred.promise;
  }
  let origin = this._getOriginURL();
  let path;
  if (this.type == "packaged") {
    path = "." + ( manifest.launch_path || "/index.html" );
  } else if (this.type == "hosted") {
    path = manifest.launch_path || "/";
  }
  let indexURL;
  try {
    indexURL = Services.io.newURI(path, null, Services.io.newURI(origin, null, null)).spec;
  } catch(e) {
    this.error(strings.formatStringFromName("validator.accessFailedLaunchPath", [origin + path], 1));
    deferred.resolve();
    return deferred.promise;
  }

  let req = new XMLHttpRequest();
  req.overrideMimeType('text/plain');
  try {
    req.open("HEAD", indexURL, true);
  } catch(e) {
    this.error(strings.formatStringFromName("validator.accessFailedLaunchPath", [indexURL], 1));
    deferred.resolve();
    return deferred.promise;
  }
  req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE | Ci.nsIRequest.INHIBIT_CACHING;
  req.onload = () => {
    if (req.status >= 400)
      this.error(strings.formatStringFromName("validator.accessFailedLaunchPathBadHttpCode", [indexURL, req.status], 2));
    deferred.resolve();
  };
  req.onerror = () => {
    this.error(strings.formatStringFromName("validator.accessFailedLaunchPath", [indexURL], 1));
    deferred.resolve();
  };

  try {
    req.send(null);
  } catch(e) {
    this.error(strings.formatStringFromName("validator.accessFailedLaunchPath", [indexURL], 1));
    deferred.resolve();
  }

  return deferred.promise;
};

AppValidator.prototype.validateType = function (manifest) {
  let appType = manifest.type || "web";
  if (["web", "trusted", "privileged", "certified"].indexOf(appType) === -1) {
    this.error(strings.formatStringFromName("validator.invalidAppType", [appType], 1));
  } else if (this.type == "hosted" &&
             ["certified", "privileged"].indexOf(appType) !== -1) {
    this.error(strings.formatStringFromName("validator.invalidHostedPriviledges", [appType], 1));
  }

  // certified app are not fully supported on the simulator
  if (appType === "certified") {
    this.warning(strings.GetStringFromName("validator.noCertifiedSupport"));
  }
};

AppValidator.prototype.validate = function () {
  this.errors = [];
  this.warnings = [];
  return this._getManifest().
    then((function (manifest) {
      if (manifest) {
        this.manifest = manifest;
        this.validateManifest(manifest);
        this.validateType(manifest);
        return this.validateLaunchPath(manifest);
      }
    }).bind(this));
};

exports.AppValidator = AppValidator;
