/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ZipUtils", "resource://gre/modules/ZipUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads", "resource://gre/modules/Downloads.jsm");

const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm");
const {AppProjects} = require("devtools/app-manager/app-projects");
const APP_CREATOR_LIST = "devtools.fxide.templatesURL";
const {AppManager} = require("devtools/app-manager");

let gTemplateList = null;

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  let projectNameNode = document.querySelector("#project-name");
  projectNameNode.addEventListener("input", canValidate, true);
  getJSON();
}, true);

function getJSON() {
  let xhr = new XMLHttpRequest();
  xhr.overrideMimeType('text/plain');
  xhr.onload = function() {
    let list;
    try {
      list = JSON.parse(this.responseText);
      if (!Array.isArray(list)) {
        throw new Error("JSON response not an array");
      }
      if (list.length == 0) {
        throw new Error("JSON response is an empty array");
      }
    } catch(e) {
      return failAndBail("Invalid response from server");
    }
    gTemplateList = list;
    let templatelistNode = document.querySelector("#templatelist");
    templatelistNode.innerHTML = "";
    for (let template of list) {
      let richlistitemNode = document.createElement("richlistitem");
      let imageNode = document.createElement("image");
      imageNode.setAttribute("src", template.icon);
      let labelNode = document.createElement("label");
      labelNode.setAttribute("value", template.name);
      let descriptionNode = document.createElement("description");
      descriptionNode.textContent = template.description;
      let vboxNode = document.createElement("vbox");
      vboxNode.setAttribute("flex", "1");
      richlistitemNode.appendChild(imageNode);
      vboxNode.appendChild(labelNode);
      vboxNode.appendChild(descriptionNode);
      richlistitemNode.appendChild(vboxNode);
      templatelistNode.appendChild(richlistitemNode);
    }
    templatelistNode.selectedIndex = 0;
  };
  xhr.onerror = function() {
    failAndBail("Can't download app templates");
  };
  let url = Services.prefs.getCharPref(APP_CREATOR_LIST);
  xhr.open("get", url);
  xhr.send();
}

function failAndBail(msg) {
  let promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].getService(Ci.nsIPromptService);
  promptService.alert(window, "error", msg);
  window.close();
}

function canValidate() {
  let projectNameNode = document.querySelector("#project-name");
  let dialogNode = document.querySelector("dialog");
  if (projectNameNode.value.length > 0) {
    dialogNode.removeAttribute("buttondisabledaccept");
  } else {
    dialogNode.setAttribute("buttondisabledaccept", "true");
  }
}

function doOK() {
  let projectName = document.querySelector("#project-name").value;

  if (!projectName) {
    AppManager.console.error("No project name");
    return false;
  }

  if (!gTemplateList) {
    AppManager.console.error("No template index");
    return false;
  }

  let templatelistNode = document.querySelector("#templatelist");
  if (templatelistNode.selectedIndex < 0) {
    AppManager.console.error("No template selected");
    return false;
  }

  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  fp.init(window, "Select directory where to create app directory", Ci.nsIFilePicker.modeGetFolder);
  let res = fp.show();
  if (res == Ci.nsIFilePicker.returnCancel) {
    AppManager.console.error("No directory selected");
    return false;
  }
  let folder = fp.file;

  // Create subfolder with fs-friendly name of project
  let subfolder = projectName.replace(/\W/g, '').toLowerCase();
  folder.append(subfolder);

  try {
    folder.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  } catch(e) {
    AppManager.console.error(e);
    return false;
  }

  // Download boilerplate zip
  let template = gTemplateList[templatelistNode.selectedIndex];
  let source = template.file;
  let target = folder.clone();
  target.append(subfolder + ".zip");

  let bail = (e) => {
    AppManager.console.error(e);
    window.close();
  };

  Downloads.fetch(source, target).then(() => {
    ZipUtils.extractFiles(target, folder);
    target.remove(false);
    AppProjects.addPackaged(folder).then((project) => {
      window.arguments[0].location = project.location;
      AppManager.validateProject(project).then(() => {
        if (project.manifest) {
          project.manifest.name = projectName;
          AppManager.writeManifest(project).then(() => {
            AppManager.validateProject(project).then(
              () => {window.close()}, bail)
          }, bail)
        } else {
          bail("Manifest not found");
        }
      }, bail)
    }, bail)
  }, bail);

  return false;
}
