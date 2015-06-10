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
  var uint8a = this._data[key];
  var sliced = uint8a.slice(sourceOffset, sourceOffset + length);
  for(var i = targetOffset, copied = 0; i < length; i++, copied++){
    targetBuffer[i] = sliced[copied];
  }
};

MemoryDataStore.writeEntry = function (key, data, cb){
  this._data[key] = new Uint8Array(data);
  cb(null);
};

this.EXPORTED_SYMBOLS = ['MemoryDataStore'];
this.MemoryDataStore = MemoryDataStore;
