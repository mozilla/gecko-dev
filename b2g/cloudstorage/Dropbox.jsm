var XMLHttpRequest, getXMLHttpRequest;
const ENV_JS_ENGINE = 'gecko';
if(ENV_JS_ENGINE === 'gecko'){
  getXMLHttpRequest = function () {
    return Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"].
      createInstance(Components.interfaces.nsIXMLHttpRequest);
  };
}else {
  XMLHttpRequest = require('xhr2');
  getXMLHttpRequest = function () {
    return new XMLHttpRequest();
  };
}

var Dropbox = {};

Dropbox.init = function (){
  if(ENV_JS_ENGINE === 'gecko'){
    this.USERTOKEN =
      'SAMPLE_ACCESS_TOKEN';
    return ;
  }
  var tokenFileName = process.env.HOME + '/.dropbox_token';
  try {
    var fs = require('fs');
    var stats = fs.statSync(tokenFileName);
    this.USERTOKEN = stats.isFile() ? fs.readFileSync( tokenFileName ) : null;
  } catch (e) {
    this.USERTOKEN = null;
  }
};

Dropbox._handleJson = function (xmlhttp, cb){
  if(xmlhttp.readyState !== 4)
    return ;

  var response = {
    data: null
  };
  if (xmlhttp.status == 200) {
    try {
      response.data = JSON.parse(xmlhttp.responseText);
      cb(null, response);
    } catch (e) {
      cb({
        error: 'parsing failed.'
      }, response);
    }
  } else {
    cb({
      error: 'http status: ' + xmlhttp.status
    }, response);
  }
};

Dropbox.quota = function (cb){
  var self = this;
  var xmlhttp = getXMLHttpRequest();
  xmlhttp.open('get', 'https://api.dropbox.com/1/account/info', true);
  xmlhttp.setRequestHeader('Authorization', 'Bearer ' + self.USERTOKEN);
  xmlhttp.setRequestHeader('Accept', 'application/json');
  xmlhttp.onreadystatechange = function () {
    self._handleJson(xmlhttp, function (error, response){
      if (response.data) {
        var quotaInfo = response.data.quota_info;
        var result = {
          data:{
            quota: quotaInfo.quota,
            used: (quotaInfo.normal + quotaInfo.shared)
          }
        };
        cb(error, result);
      } else {
        cb(error, response);
      }
    });
  };

  xmlhttp.send();
};

Dropbox._convertItem = function (data){
  return {
    isdir: data.is_dir ? 1 : 0,
    path: data.path,
    size: data.bytes,
    // Date format:
    // "%a, %d %b %Y %H:%M:%S %z"
    //
    // Example: "Sat, 21 Aug 2010 22:31:20 +0000"
    mtime: new Date(data.modified).getTime(),
    ctime: new Date(data.modified).getTime()
  };
};

Dropbox.getFileMeta = function (path, cb){
  var self = this;
  var xmlhttp = getXMLHttpRequest();
  xmlhttp.open('get', 'https://api.dropbox.com/1/metadata/auto' + path, true);
  xmlhttp.setRequestHeader('Authorization', 'Bearer ' + self.USERTOKEN);
  xmlhttp.setRequestHeader('Accept', 'application/json');
  xmlhttp.onreadystatechange = function () {
    self._handleJson(xmlhttp, function (error, response){
      if (response.data) {
        var data = response.data;
        var result = {
          data:{
            list: [self._convertItem(data)]
          }
        };
        cb(error, result);
      } else {
        cb(error, response);
      }
    });
  };

  xmlhttp.send('list=false');
};

Dropbox.getFileList = function (path, cb){
  var self = this;
  var xmlhttp = getXMLHttpRequest();
  xmlhttp.open('get', 'https://api.dropbox.com/1/metadata/auto' + path, true);
  xmlhttp.setRequestHeader('Authorization', 'Bearer ' + self.USERTOKEN);
  xmlhttp.setRequestHeader('Accept', 'application/json');
  xmlhttp.onreadystatechange = function () {
    self._handleJson(xmlhttp, function (error, response){
      if (response.data) {
        var data = response.data;
        var resultList = [];
        for(var i = 0; i < data.contents.length; i++){
          var content = data.contents[i];
          resultList.push(self._convertItem(content));
        }
        var result = {
          data:{
            list: resultList
          }
        };
        cb(error, result);
      } else {
        cb(error, response);
      }
    });
  };

  xmlhttp.send('list=true');
};

Dropbox.getFileDownload = function (path, offset, size, cb){
  var self = this;
  var xmlhttp = getXMLHttpRequest();
  xmlhttp.open('get', 'https://api-content.dropbox.com/1/files/auto' + path, true);
  xmlhttp.setRequestHeader('Authorization', 'Bearer ' + self.USERTOKEN);
  xmlhttp.setRequestHeader('Range', 'bytes=' + offset + '-' + ( offset + size - 1 ));
  xmlhttp.responseType = 'blob';
  xmlhttp.onload = function () {
    cb(null, {
      data: xmlhttp.response
    });
  };

  xmlhttp.send();
};

Dropbox._tokenRequest = function (link, auth, params, cb){
  var self = this;
  unirest.post(link)
  .auth(auth)
  .header('Accept', 'application/json')
  .send(params)
  .end(function (httpResponse) {
    self._handleJson(httpResponse, cb);
  });
};

/*
step 1:
https://www.dropbox.com/1/oauth2/authorize?
client_id=<API_KEY>&
response_type=code

step 2:
curl https://api.dropbox.com/1/oauth2/token \
-d code=<USER_CODE> \
-d grant_type=authorization_code \
-u <API_KEY>:<API_SECRET>

Response:
{
  "access_token": "<ACCESS_TOKEN>",
  "token_type": "bearer",
  "uid": "??????"
}
*/

Dropbox.getAccessToken = function (api_key, api_secret, cb){
  var self = this;
  var device_code = null;

  var link = 'https://www.dropbox.com/1/oauth2/authorize?' +
    'client_id=' + api_key + '&' +
    'response_type=code';

  var rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });

  rl.question("Please go to the link\n" + link + "\nand input the code for oauth:\n", function(answer) {
    device_code = answer;
    console.log('\nYour code: ' + device_code);
    rl.close();

    var linkToken = 'https://api.dropbox.com/1/oauth2/token';
    self._tokenRequest(linkToken, {
      'user': api_key,
      'pass': api_secret
    }, {
      'code': device_code,
      'grant_type': 'authorization_code'
    }, function(error, response){
      console.log(response);
    });
  });

};

this.EXPORTED_SYMBOLS = ['Dropbox'];
this.Dropbox = Dropbox;
