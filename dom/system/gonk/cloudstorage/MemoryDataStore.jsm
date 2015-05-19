var MemoryDataStore = {};

MemoryDataStore.init = function (){
  this._data = {};
};

MemoryDataStore.deleteEntry = function (key){
  delete this._data[key];
  this._data[key] = null;
};

MemoryDataStore.readEntry =
  function (key, targetBuffer, targetOffset, sourceOffset, length){
  this._data[key].slice(sourceOffset, sourceOffset + length).
    copyWithin(targetBuffer, targetOffset);
};

MemoryDataStore.writeEntry = function (key, data, cb){
  this._data[key] = new Uint8Array(data);
  cb(null);
};

this.EXPORTED_SYMBOLS = ['MemoryDataStore'];
this.MemoryDataStore = MemoryDataStore;
