const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/MemoryDataStore.jsm');
const { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});

var DataCache = {};

DataCache.init = function (blockSize) {
  this._MAX_DATA_CACHE_ENTRY = 25;
  this._BLOCK_SIZE = blockSize;

  this._dataStore = MemoryDataStore;
  this._dataStore.init();

  this._fileDataCache = {};
  this._priorityQueue = [];
};

DataCache.update = function (key, data) {
  if (this._fileDataCache.hasOwnProperty(key) ) {
    // Update an exist cache entry.
    this._updateEntry(key, data);
  } else {
    // Add a new data cache.
    if (this._priorityQueue.length < this._MAX_DATA_CACHE_ENTRY) {
      // Entry is sufficient to add a new one.
      this._pushEntry(key, data);
    } else {
      // Remove an entry and delete cache file.
      var deletingCandidateKey = this._popLowPriorityKey();
      this._removeEntry(deletingCandidateKey);
      this._pushEntry(key, data);
    }
  }
};

DataCache._updateEntry = function (key, data) {
  this._fileDataCache[key] = data;
};

DataCache._pushEntry = function (key, data) {
  this._fileDataCache[key] = data;
  this._priorityQueue.push(key);
};

DataCache._removeEntry = function (key) {
  delete this._fileDataCache[key];
  this._dataStore.deleteEntry(key);
};

DataCache._popLowPriorityKey = function () {
  return this._priorityQueue.shift();
};

DataCache.updateStatus = function (md5sum, status) {
  this._fileDataCache[md5sum].status = status;
};

DataCache.get = function (md5sum) {
  if (this._fileDataCache.hasOwnProperty(md5sum)) {
    return this._fileDataCache[md5sum];
  }
  return null;
};

DataCache.writeCache = function (task, data, cb){
  var self = this;
  this._dataStore.writeEntry(task.md5sum, data, function(err) {
    if(err) {
      console.log(err);
    } else {
      self.updateStatus(task.md5sum, 'DONE');
      console.log('The file was saved!');
    }
    cb();
  });
};

DataCache.readCache = function (path, buffer, offset, size, requestList, cb){
  var seek = 0,
    writeSize = 0,
    cursor_moved = 0;

  for (var i in requestList) {
    var task = requestList[i];
    if (task.priority === 'PREFETCH') {
      continue;
    }
    if (this._fileDataCache[task.md5sum] &&
      this._fileDataCache[task.md5sum].status === 'DONE') {
      seek = ( offset + cursor_moved ) % this._BLOCK_SIZE;
      writeSize = this._BLOCK_SIZE - seek;
      if ((writeSize + cursor_moved ) > size) {
        writeSize = size - cursor_moved;
      }

      this._dataStore.readEntry(task.md5sum,
        buffer, cursor_moved, seek, writeSize);

      cursor_moved += writeSize ;
    } else {
      console.error('======= Critical Error =======');
      console.error(path);
      console.error(offset);
      console.error(size);
      console.error(requestList);
      console.error(this._fileDataCache);

      throw Error('data is not finished.');
    }
  }
  cb();
};

DataCache.generateKey = function (task){
  function hashCode(str) {
    var hash = 0, i, chr, len;
    if (str.length == 0) return hash;
    for (i = 0, len = str.length; i < len; i++) {
      chr   = str.charCodeAt(i);
      hash  = ((hash << 5) - hash) + chr;
      hash |= 0; // Convert to 32bit integer
    }
    return hash;
  }

  var obscured = task.path.replace(/\//g, ':');
  return hashCode(task.path) + '@' + obscured + '@' + task.offset;
};

this.EXPORTED_SYMBOLS = ['DataCache'];
this.DataCache = DataCache;
