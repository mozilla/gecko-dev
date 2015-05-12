var MetaCache = {};

MetaCache.init = function (){
  this._fileMetaCache = {};
  this._fileListCache = {};
};

MetaCache.update = function (path, data) {
  this._fileMetaCache[path] = data;
};

MetaCache.get = function (path) {
  if (this._fileMetaCache.hasOwnProperty(path)) {
    return this._fileMetaCache[path];
  }
  return null;
};

MetaCache.updateList = function (path, data) {
  this._fileListCache[path] = data;
};

MetaCache.getList = function (path) {
  if (this._fileListCache.hasOwnProperty(path)) {
    return this._fileListCache[path];
  }
  return null;
};

this.EXPORTED_SYMBOLS = ['MetaCache'];
this.MetaCache = MetaCache;
