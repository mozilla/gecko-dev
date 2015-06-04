var Sample = {};

Sample.init = function (){
  this._TEST_DATA = {
    'hello.txt': 'Hello world!\n',
    'dir1': {
      'welcome.txt': 'Welcome to UniDisk\n',
      'dir2': {
        'dummy.txt': 'Dummy\n',
        'dummy2.txt': 'Dummy 2\n'
      },
      'readme': 'Sample cloud drive is for people to customize your own module.\n'
    },
    'goodbye.txt': 'Goodbye\n'
  };
};

Sample._isObject = function (obj) {
  return typeof obj === 'object';
};

Sample._findMeta = function (path) {
  if (path === '/') {
    return this._TEST_DATA;
  }
  var nodes = path.split('/');
  var currentNode = this._TEST_DATA[nodes[1]];
  for (var i = 2; i < nodes.length; i++) {
    if (currentNode) {
       currentNode = currentNode[nodes[i]];
    }else {
      return null;
    }
  }
  return currentNode;
};

Sample._getMeta = function (path) {
  var meta = this._findMeta(path);
  if (meta) {
    if (typeof meta === 'string') {
      return {
        isdir: 0,
        path: path,
        size: meta.length,
        mtime: Date.now(),
        ctime: Date.now()
      };
    } else if (this._isObject(meta)) {
      return {
        isdir: 1,
        path: path,
        size: 0,
        mtime: Date.now(),
        ctime: Date.now()
      };
    }
  } else {
    return null;
  }
};

Sample.quota = function (cb){
  cb(null, {
    data:{
      quota: 2313913630720,
      used: 58410672308,
      request_id: 1741802854
    }
  });
}

Sample.getFileMeta = function (path, cb){
  var meta = this._getMeta(path);
  var result = meta ? {list: [meta]} : null;
  cb(null, {
    data: result
  });
}

Sample.getFileDownload = function (path, offset, size, cb){
  var meta = this._findMeta(path);
  if (meta && typeof meta === 'string') {
    var result = meta.substr(offset, size);
    cb(null, {
      data: result,
      length: result.length
    });
  } else {
    cb(null, null);
  }
}

Sample.getFileList = function (path, cb){
  var meta = this._getMeta(path);
  var resultList = [];
  if (meta && meta.isdir === 1) {
    var list = this._findMeta(path);
    var keys = Object.keys(list);
    for(var i = 0; i < keys.length; i++){
      var childPath = path + '/' + keys[i];
      childPath = childPath.replace('//', '/', 'gi');
      var childMeta = this._getMeta(childPath);
      resultList.push(childMeta);
    }
    cb(null, {data: {
      list: resultList
    }});
  } else {
    cb(null, null);
  }
}

this.EXPORTED_SYMBOLS = ['Sample'];
this.Sample = Sample;
