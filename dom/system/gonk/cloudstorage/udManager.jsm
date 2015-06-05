const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/foco.jsm');
const { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'setTimeout',
  'resource://gre/modules/Timer.jsm');

var UD_BLOCK_SIZE = 1*1024*1024;
var UD_QUEUE_SIZE = 3;
var UD_PREFETCH_SIZE = 10 * UD_BLOCK_SIZE;

var udManager = {};

udManager._isIllegalFileName = function (path) {
	var list = path.split('/');
	for(var i = 0; i < list.length; i++){
		if( list[i].indexOf('.') === 0 ){
			return true;
		}
	}
	return false;
}

udManager.queueHandler = function (id, task, callback) {
	console.log('  [B] ' + task.path + "|" + task.offset + '| downloading...');
	var self = this;
	task.status = "DOWNLOADING";
	this.downloadFileInRange(task.path, task.offset, task.size, function(error, response){
		console.log(task.path + "|" + task.offset + '| done!! ' + response.length);

		// Write the buffer to the cache file.
		self.dataCache.writeCache(task, response.data, function(){
			callback();
		});
	});
};

udManager.init = function(options){
	this.webStorage = options.webStorageModule;
	this.metaCache = options.metaCacheModule;
	this.dataCache = options.dataCacheModule;

	this.webStorage.init({
		accessToken: options.accessToken
	});
	this.metaCache.init();
	this.dataCache.init(UD_BLOCK_SIZE);
	this.FileDownloadQueue = foco.priorityQueue(
		this.queueHandler.bind(this), UD_QUEUE_SIZE);
}

udManager.showStat = function (cb) {
	var self = this;
	var retry = function () {
		if( udManager.QuotaCache ) {
			cb(null, udManager.QuotaCache );
		} else {
			self.webStorage.quota(function(error, response){
				if(error){
					console.log("" + new Date () + "| " + error);
					retry();
				}else{
					udManager.QuotaCache = response;
					cb(error, response);
				}
			});
		}
	};
	retry();
}

udManager.getFileMeta = function (path, cb) {
	if(udManager._isIllegalFileName(path)){
		cb(null, {data: null});
		return ;
	}
	var self = this;
	var retry = function () {
		var meta = self.metaCache.get(path);
		if (meta) {
			cb(null, { data : meta });
		}else{
			self.webStorage.getFileMeta(path, function(error, response){
				if(error){
					console.log("" + new Date () + "| " + error);
					retry();
				}else{
					self.metaCache.update(path, response.data);
					cb(error, response);
				}
			});
		}
	};
	retry();
}

udManager.getFileList = function (path, cb) {
	if(udManager._isIllegalFileName(path)){
		cb(null, {data: null});
		return ;
	}
	var self = this;
	var retry = function () {
		var list = self.metaCache.getList(path);
		if (list) {
			cb(null, { data : list });
		}else{
			self.webStorage.getFileList(path, function(error, response){
				if(error){
					console.log("" + new Date () + "| " + error);
					retry();
				}else{
					self.metaCache.updateList(path, response.data);
					cb(error, response);
				}
			});
		}
	};
	retry();
}

udManager._generateRequestList = function(fileMeta, offset, size, fileSize){
	const endPos = offset + size;
	var requestList = [];

	var alignedOffset = Math.floor( offset / UD_BLOCK_SIZE) * UD_BLOCK_SIZE;
	for(; alignedOffset < endPos && alignedOffset < fileSize; alignedOffset += UD_BLOCK_SIZE ){
		var task = {
			path: fileMeta.path,
			totalSize: fileMeta.size,
			mtime: fileMeta.mtime,
			status: "INIT",
			priority: "HIGH",
			md5sum: "",
			offset: alignedOffset,
			size: ((alignedOffset + UD_BLOCK_SIZE) > fileSize ? (fileSize - alignedOffset) : UD_BLOCK_SIZE )
		};
		var taskMd5sum = this.dataCache.generateKey(task);
		task.md5sum = taskMd5sum;

		requestList.push(task);
	}

	const prefetchEndPos = endPos + UD_PREFETCH_SIZE;
	for(; alignedOffset < prefetchEndPos && alignedOffset < fileSize; alignedOffset += UD_BLOCK_SIZE ){
		var task = {
			path: fileMeta.path,
			totalSize: fileMeta.size,
			mtime: fileMeta.mtime,
			status: "INIT",
			priority: "PREFETCH",
			md5sum: "",
			offset: alignedOffset,
			size: ((alignedOffset + UD_BLOCK_SIZE) > fileSize ? (fileSize - alignedOffset) : UD_BLOCK_SIZE )
		};
		var taskMd5sum = this.dataCache.generateKey(task);
		task.md5sum = taskMd5sum;

		requestList.push(task);
	}

	return requestList;
}

udManager._isAllRequestDone = function (downloadRequest){
	var done = true;
	for(var req in downloadRequest){
		var task = downloadRequest[req];
		var taskMd5sum = task.md5sum;
		if( task.priority === "PREFETCH" ){
			continue;
		}
		var data = this.dataCache.get(taskMd5sum);
		if (data && data.status === 'DONE') {
			// do nothing.
		}else{
			done = false;
			break;
		}
	}
	return done;
}

udManager._requestPushAndDownload = function (path, downloadRequest, cb){
	var self = this;
	foco.each(downloadRequest, function(index, task, callback){
		var taskMd5sum = task.md5sum;
		var data = self.dataCache.get(taskMd5sum);

		if (data) {
			console.log('  [C1] ' + data.path + " is in cache: " + data.status + "| " + task.offset);
			udManager.FileDownloadQueue.priorityChange(taskMd5sum, 0);
			callback();
		} else if ( task.priority === "PREFETCH" ) {
			self.dataCache.update(taskMd5sum, task);
			udManager.FileDownloadQueue.push(taskMd5sum, 1, task);
			callback();
		}else{
			self.dataCache.update(taskMd5sum, task);
			udManager.FileDownloadQueue.push(taskMd5sum, 0, task);
			callback();
		}
	}, function(err){
		// Verify the download request is all finished or not.
		function retry () {
			if( udManager._isAllRequestDone(downloadRequest) ){
				console.log('  [D] ' + 'All requests are done.');
				cb();
			}else {
				console.log("retry to wait all requests done...");
				setTimeout(retry, 1000);
			}
		}
		retry();
	});
}

udManager.downloadFileInRangeByCache = function(path, buffer, offset, size, cb) {
	console.log('{{');
	console.log('  [A] ' + path + ' ' + offset + ' ' + size);
	var self = this;
	udManager.getFileMeta(path, function(error, response){
		const totalSize = response.data.list[0].size;
		// 1. Split the download request.
		var requestList = udManager._generateRequestList(response.data.list[0], parseInt(offset), parseInt(size), totalSize);

		// 2. Push downloading request.
		udManager._requestPushAndDownload(path, requestList, function(){
			// 3. All requests are done. Aggregate all data.
			// Read the request data from files.
			self.dataCache.readCache(path, buffer, offset, size, requestList, function(){
				console.log('  [E] data is prepared.');
				console.log('}}');
				cb(null);
			});
		});
	});
}

udManager.downloadFileInRange = function(path, offset, size, cb) {
	var self = this;
	var retry = function () {
		self.webStorage.getFileDownload(path, offset, size, function(error, response){
			if(error){
				console.log('[ERROR] retry, error happened: ' + error);
				setTimeout(retry , 800);
			}else if( !response || !response.data || !response.data instanceof ArrayBuffer ){
				console.log('[ERROR] retry, error response: ' + response);
				setTimeout(retry , 800);
			}else if( size != response.length ){
				console.log('[ERROR] retry, size error: ' + offset + " " + size + " " + response.length);
				setTimeout(retry , 800);
			}else{
				cb(error, response);
			}
		});
	}
	retry();
}

udManager.downloadFileInMultiRange = function(path, list, cb) {
	var listArray = null;
	if( typeof list === "string" ){
		try {
			listArray = JSON.parse(list).list;
		} catch (e) {
			cb("Incorrect download list.", null);
			return ;
		}
	}else{
		listArray = list.list;
	}
	foco.each(listArray, function(index, item, callback){
		udManager.downloadFileInRange(path, item.offset, item.size, function(error, response){
			console.log(response.data);
			callback();
		});
	}, function(){
		cb(null, {
			data: "OK!"
		});
	});
}

this.EXPORTED_SYMBOLS = ['udManager'];
this.udManager = udManager;
