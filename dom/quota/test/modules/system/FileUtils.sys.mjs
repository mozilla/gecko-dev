/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

export const FileUtils = {
  getProfileDirectory() {
    return Services.dirsvc.get("ProfD", Ci.nsIFile);
  },

  getFile(relativePath, baseFile) {
    if (!baseFile) {
      baseFile = this.getProfileDirectory();
    }

    let file = baseFile.clone();

    if (Services.appinfo.OS === "WINNT") {
      const winFile = file.QueryInterface(Ci.nsILocalFileWin);
      winFile.useDOSDevicePathSyntax = true;
    }

    relativePath.split("/").forEach(function (component) {
      if (component == "..") {
        file = file.parent;
      } else {
        file.append(component);
      }
    });

    return file;
  },
};
