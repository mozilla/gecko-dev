Components.utils.import("resource://gre/modules/Downloads.jsm");
Components.utils.import("resource://gre/modules/osfile.jsm");

let currentId = 0;
const downloadsList = {};

const downloads = Object.freeze({
  download(chromeOptions, callback) {
    Downloads.getPreferredDownloadsDirectory().then(downloadsDir => {
      const mozOptions = {
        source: chromeOptions.url,
        // TODO use file path to check and concatenate
        target: chromeOptions.target ?
          OS.Path.join(downloadsDir, chromeOptions.target):
          downloadsDir,
        // TODO do something with conflictAction,
        // TODO do something with saveAs,
      };

      return Downloads.createDownload(mozOptions);
    }).then(download => {
      downloadsList[++currentId] = download;
      return currentId;
    }).catch(e => {
      // TODO put something in runtime.lastError
      return undefined;
    }).then(callback);
  }
});

extensions.registerAPI((extension, context) => ({ downloads }));

