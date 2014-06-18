/**
 * @license  OpenTok JavaScript Library v2.2.5
 * http://www.tokbox.com/
 *
 * Copyright (c) 2014 TokBox, Inc.
 * Released under the MIT license
 * http://opensource.org/licenses/MIT
 *
 * Date: May 22 07:14:18 2014
 */

(function(window) {
  if (!window.OT) window.OT = {};

  OT.properties = {
    version: 'v2.2.5',         // The current version (eg. v2.0.4) (This is replaced by gradle)
    build: '12d9384',    // The current build hash (This is replaced by gradle)

    // Whether or not to turn on debug logging by default
    debug: 'false',
    // The URL of the tokbox website
    websiteURL: 'http://www.tokbox.com',

    // The URL of the CDN
    // XXX: patched for loop so we use local files
    cdnURL: 'loop/otcdn',
    // The URL to use for logging
    loggingURL: 'https://hlg.tokbox.com/prod',
    // The anvil API URL
    apiURL: 'https://anvil.opentok.com',

    // What protocol to use when connecting to the rumor web socket
    messagingProtocol: 'wss',
    // What port to use when connection to the rumor web socket
    messagingPort: 443,

    // If this environment supports SSL
    supportSSL: 'true',
    // The CDN to use if we're using SSL
    // XXX: patched for loop so we use local files
    cdnURLSSL: 'loop/otcdn',
    // The loggging URL to use if we're using SSL
    loggingURLSSL: 'https://hlg.tokbox.com/prod',
    // The anvil API URL to use if we're using SSL
    apiURLSSL: 'https://anvil.opentok.com',

    minimumVersion: {
      firefox: parseFloat('26'),
      chrome: parseFloat('32')
    }
  };

})(window);
/**
 * @license  Common JS Helpers on OpenTok 0.2.0 1f056b9 master
 * http://www.tokbox.com/
 *
 * Copyright (c) 2014 TokBox, Inc.
 * Released under the MIT license
 * http://opensource.org/licenses/MIT
 *
 * Date: May 19 04:04:43 2014
 *
 */

// OT Helper Methods
//
// helpers.js                           <- the root file
// helpers/lib/{helper topic}.js        <- specialised helpers for specific tasks/topics
//                                          (i.e. video, dom, etc)
//
// @example Getting a DOM element by it's id
//  var element = OTHelpers('domId');
//
// @example Testing for web socket support
//  if (OT.supportsWebSockets()) {
//      // do some stuff with websockets
//  }
//

/*jshint browser:true, smarttabs:true*/

!(function(window, undefined) {


  var OTHelpers = function(domId) {
    return document.getElementById(domId);
  };

  var previousOTHelpers = window.OTHelpers;

  window.OTHelpers = OTHelpers;

  OTHelpers.keys = Object.keys || function(object) {
    var keys = [], hasOwnProperty = Object.prototype.hasOwnProperty;
    for(var key in object) {
      if(hasOwnProperty.call(object, key)) {
        keys.push(key);
      }
    }
    return keys;
  };

  var _each = Array.prototype.forEach || function(iter, ctx) {
    for(var idx = 0, count = this.length || 0; idx < count; ++idx) {
      if(idx in this) {
        iter.call(ctx, this[idx], idx);
      }
    }
  };

  OTHelpers.forEach = function(array, iter, ctx) {
    return _each.call(array, iter, ctx);
  };

  var _map = Array.prototype.map || function(iter, ctx) {
    var collect = [];
    _each.call(this, function(item, idx) {
      collect.push(iter.call(ctx, item, idx));
    });
    return collect;
  };

  OTHelpers.map = function(array, iter) {
    return _map.call(array, iter);
  };

  var _filter = Array.prototype.filter || function(iter, ctx) {
    var collect = [];
    _each.call(this, function(item, idx) {
      if(iter.call(ctx, item, idx)) {
        collect.push(item);
      }
    });
    return collect;
  };

  OTHelpers.filter = function(array, iter, ctx) {
    return _filter.call(array, iter, ctx);
  };

  var _some = Array.prototype.some || function(iter, ctx) {
    var any = false;
    for(var idx = 0, count = this.length || 0; idx < count; ++idx) {
      if(idx in this) {
        if(iter.call(ctx, this[idx], idx)) {
          any = true;
          break;
        }
      }
    }
    return any;
  };

  OTHelpers.some = function(array, iter, ctx) {
    return _some.call(array, iter, ctx);
  };

  var _indexOf = Array.prototype.indexOf || function(searchElement, fromIndex) {
    var i,
        pivot = (fromIndex) ? fromIndex : 0,
        length;

    if (!this) {
      throw new TypeError();
    }

    length = this.length;

    if (length === 0 || pivot >= length) {
      return -1;
    }

    if (pivot < 0) {
      pivot = length - Math.abs(pivot);
    }

    for (i = pivot; i < length; i++) {
      if (this[i] === searchElement) {
        return i;
      }
    }
    return -1;
  };

  OTHelpers.arrayIndexOf = function(array, searchElement, fromIndex) {
    return _indexOf.call(array, searchElement, fromIndex);
  };

  var _bind = Function.prototype.bind || function() {
    var args = Array.prototype.slice.call(arguments),
        ctx = args.shift(),
        fn = this;
    return function() {
      return fn.apply(ctx, args.concat(Array.prototype.slice.call(arguments)));
    };
  };

  OTHelpers.bind = function() {
    var args = Array.prototype.slice.call(arguments),
        fn = args.shift();
    return _bind.apply(fn, args);
  };

  var _trim = String.prototype.trim || function() {
    return this.replace(/^\s+|\s+$/g, '');
  };

  OTHelpers.trim = function(str) {
    return _trim.call(str);
  };

  OTHelpers.noConflict = function() {
    OTHelpers.noConflict = function() {
      return OTHelpers;
    };
    window.OTHelpers = previousOTHelpers;
    return OTHelpers;
  };

  OTHelpers.isNone = function(obj) {
    return obj === undefined || obj === null;
  };

  OTHelpers.isObject = function(obj) {
    return obj === Object(obj);
  };

  OTHelpers.isFunction = function(obj) {
    return !!obj && (obj.toString().indexOf('()') !== -1 ||
      Object.prototype.toString.call(obj) === '[object Function]');
  };

  OTHelpers.isArray = OTHelpers.isFunction(Array.isArray) && Array.isArray ||
    function (vArg) {
      return Object.prototype.toString.call(vArg) === '[object Array]';
    };

  OTHelpers.isEmpty = function(obj) {
    if (obj === null || obj === undefined) return true;
    if (OTHelpers.isArray(obj) || typeof(obj) === 'string') return obj.length === 0;

    // Objects without enumerable owned properties are empty.
    for (var key in obj) {
      if (obj.hasOwnProperty(key)) return false;
    }

    return true;
  };

// Extend a target object with the properties from one or
// more source objects
//
// @example:
//    dest = OTHelpers.extend(dest, source1, source2, source3);
//
  OTHelpers.extend = function(/* dest, source1[, source2, ..., , sourceN]*/) {
    var sources = Array.prototype.slice.call(arguments),
        dest = sources.shift();

    OTHelpers.forEach(sources, function(source) {
      for (var key in source) {
        dest[key] = source[key];
      }
    });

    return dest;
  };

// Ensures that the target object contains certain defaults.
//
// @example
//   var options = OTHelpers.defaults(options, {
//     loading: true     // loading by default
//   });
//
  OTHelpers.defaults = function(/* dest, defaults1[, defaults2, ..., , defaultsN]*/) {
    var sources = Array.prototype.slice.call(arguments),
        dest = sources.shift();

    OTHelpers.forEach(sources, function(source) {
      for (var key in source) {
        if (dest[key] === void 0) dest[key] = source[key];
      }
    });

    return dest;
  };

  OTHelpers.clone = function(obj) {
    if (!OTHelpers.isObject(obj)) return obj;
    return OTHelpers.isArray(obj) ? obj.slice() : OTHelpers.extend({}, obj);
  };



// Handy do nothing function
  OTHelpers.noop = function() {};

// Returns true if the client supports WebSockets, false otherwise.
  OTHelpers.supportsWebSockets = function() {
    return 'WebSocket' in window;
  };

// Returns the number of millisceonds since the the UNIX epoch, this is functionally
// equivalent to executing new Date().getTime().
//
// Where available, we use 'performance.now' which is more accurate and reliable,
// otherwise we default to new Date().getTime().
  OTHelpers.now = (function() {
    var performance = window.performance || {},
        navigationStart,
        now =  performance.now       ||
               performance.mozNow    ||
               performance.msNow     ||
               performance.oNow      ||
               performance.webkitNow;

    if (now) {
      now = OTHelpers.bind(now, performance);
      navigationStart = performance.timing.navigationStart;

      return  function() { return navigationStart + now(); };
    } else {
      return function() { return new Date().getTime(); };
    }
  })();

  var _browser = function() {
    var userAgent = window.navigator.userAgent.toLowerCase(),
        appName = window.navigator.appName,
        navigatorVendor,
        browser = 'unknown',
        version = -1;

    if (userAgent.indexOf('opera') > -1 || userAgent.indexOf('opr') > -1) {
      browser = 'Opera';

      if (/opr\/([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if (userAgent.indexOf('firefox') > -1)   {
      browser = 'Firefox';

      if (/firefox\/([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if (appName === 'Microsoft Internet Explorer') {
      // IE 10 and below
      browser = 'IE';

      if (/msie ([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if (appName === 'Netscape' && userAgent.indexOf('trident') > -1) {
      // IE 11+

      browser = 'IE';

      if (/trident\/.*rv:([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if (userAgent.indexOf('chrome') > -1) {
      browser = 'Chrome';

      if (/chrome\/([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }

    } else if ((navigatorVendor = window.navigator.vendor) &&
      navigatorVendor.toLowerCase().indexOf('apple') > -1) {
      browser = 'Safari';

      if (/version\/([0-9]{1,}[\.0-9]{0,})/.exec(userAgent) !== null) {
        version = parseFloat( RegExp.$1 );
      }
    }

    return {
      browser: browser,
      version: version,
      iframeNeedsLoad: userAgent.indexOf('webkit') < 0
    };
  }();

  OTHelpers.browser = function() {
    return _browser.browser;
  };

  OTHelpers.browserVersion = function() {
    return _browser;
  };


  OTHelpers.canDefineProperty = true;

  try {
    Object.defineProperty({}, 'x', {});
  } catch (err) {
    OTHelpers.canDefineProperty = false;
  }

// A helper for defining a number of getters at once.
//
// @example: from inside an object
//   OTHelpers.defineGetters(this, {
//     apiKey: function() { return _apiKey; },
//     token: function() { return _token; },
//     connected: function() { return this.is('connected'); },
//     capabilities: function() { return _socket.capabilities; },
//     sessionId: function() { return _sessionId; },
//     id: function() { return _sessionId; }
//   });
//
  OTHelpers.defineGetters = function(self, getters, enumerable) {
    var propsDefinition = {};

    if (enumerable === void 0) enumerable = false;

    for (var key in getters) {
      propsDefinition[key] = {
        get: getters[key],
        enumerable: enumerable
      };
    }

    OTHelpers.defineProperties(self, propsDefinition);
  };

  var generatePropertyFunction = function(object, getter, setter) {
    if(getter && !setter) {
      return function() {
        return getter.call(object);
      };
    } else if(getter && setter) {
      return function(value) {
        if(value !== void 0) {
          setter.call(object, value);
        }
        return getter.call(object);
      };
    } else {
      return function(value) {
        if(value !== void 0) {
          setter.call(object, value);
        }
      };
    }
  };

  OTHelpers.defineProperties = function(object, getterSetters) {
    for (var key in getterSetters) {
      object[key] = generatePropertyFunction(object, getterSetters[key].get,
        getterSetters[key].set);
    }
  };


// Polyfill Object.create for IE8
//
// See https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/create
//
  if (!Object.create) {
    Object.create = function (o) {
      if (arguments.length > 1) {
        throw new Error('Object.create implementation only accepts the first parameter.');
      }
      function F() {}
      F.prototype = o;
      return new F();
    };
  }

  OTHelpers.setCookie = function(key, value) {
    try {
      localStorage.setItem(key, value);
    } catch (err) {
      // Store in browser cookie
      var date = new Date();
      date.setTime(date.getTime()+(365*24*60*60*1000));
      var expires = '; expires=' + date.toGMTString();
      document.cookie = key + '=' + value + expires + '; path=/';
    }
  };

  OTHelpers.getCookie = function(key) {
    var value;

    try {
      value = localStorage.getItem('opentok_client_id');
      return value;
    } catch (err) {
      // Check browser cookies
      var nameEQ = key + '=';
      var ca = document.cookie.split(';');
      for(var i=0;i < ca.length;i++) {
        var c = ca[i];
        while (c.charAt(0) === ' ') {
          c = c.substring(1,c.length);
        }
        if (c.indexOf(nameEQ) === 0) {
          value = c.substring(nameEQ.length,c.length);
        }
      }

      if (value) {
        return value;
      }
    }

    return null;
  };


// These next bits are included from Underscore.js. The original copyright
// notice is below.
//
// http://underscorejs.org
// (c) 2009-2011 Jeremy Ashkenas, DocumentCloud Inc.
// (c) 2011-2013 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
// Underscore may be freely distributed under the MIT license.

  // Invert the keys and values of an object. The values must be serializable.
  OTHelpers.invert = function(obj) {
    var result = {};
    for (var key in obj) if (obj.hasOwnProperty(key)) result[obj[key]] = key;
    return result;
  };


  // List of HTML entities for escaping.
  var entityMap = {
    escape: {
      '&':  '&amp;',
      '<':  '&lt;',
      '>':  '&gt;',
      '"':  '&quot;',
      '\'': '&#x27;',
      '/':  '&#x2F;'
    }
  };

  entityMap.unescape = OTHelpers.invert(entityMap.escape);

  // Regexes containing the keys and values listed immediately above.
  var entityRegexes = {
    escape:   new RegExp('[' + OTHelpers.keys(entityMap.escape).join('') + ']', 'g'),
    unescape: new RegExp('(' + OTHelpers.keys(entityMap.unescape).join('|') + ')', 'g')
  };

  // Functions for escaping and unescaping strings to/from HTML interpolation.
  OTHelpers.forEach(['escape', 'unescape'], function(method) {
    OTHelpers[method] = function(string) {
      if (string === null || string === undefined) return '';
      return ('' + string).replace(entityRegexes[method], function(match) {
        return entityMap[method][match];
      });
    };
  });

// By default, Underscore uses ERB-style template delimiters, change the
// following template settings to use alternative delimiters.
  OTHelpers.templateSettings = {
    evaluate    : /<%([\s\S]+?)%>/g,
    interpolate : /<%=([\s\S]+?)%>/g,
    escape      : /<%-([\s\S]+?)%>/g
  };

// When customizing `templateSettings`, if you don't want to define an
// interpolation, evaluation or escaping regex, we need one that is
// guaranteed not to match.
  var noMatch = /(.)^/;

// Certain characters need to be escaped so that they can be put into a
// string literal.
  var escapes = {
    '\'':     '\'',
    '\\':     '\\',
    '\r':     'r',
    '\n':     'n',
    '\t':     't',
    '\u2028': 'u2028',
    '\u2029': 'u2029'
  };

  var escaper = /\\|'|\r|\n|\t|\u2028|\u2029/g;

// JavaScript micro-templating, similar to John Resig's implementation.
// Underscore templating handles arbitrary delimiters, preserves whitespace,
// and correctly escapes quotes within interpolated code.
  OTHelpers.template = function(text, data, settings) {
    var render;
    settings = OTHelpers.defaults({}, settings, OTHelpers.templateSettings);

    // Combine delimiters into one regular expression via alternation.
    var matcher = new RegExp([
      (settings.escape || noMatch).source,
      (settings.interpolate || noMatch).source,
      (settings.evaluate || noMatch).source
    ].join('|') + '|$', 'g');

    // Compile the template source, escaping string literals appropriately.
    var index = 0;
    var source = '__p+=\'';
    text.replace(matcher, function(match, escape, interpolate, evaluate, offset) {
      source += text.slice(index, offset)
        .replace(escaper, function(match) { return '\\' + escapes[match]; });

      if (escape) {
        source += '\'+\n((__t=(' + escape + '))==null?\'\':OTHelpers.escape(__t))+\n\'';
      }
      if (interpolate) {
        source += '\'+\n((__t=(' + interpolate + '))==null?\'\':__t)+\n\'';
      }
      if (evaluate) {
        source += '\';\n' + evaluate + '\n__p+=\'';
      }
      index = offset + match.length;
      return match;
    });
    source += '\';\n';

    // If a variable is not specified, place data values in local scope.
    if (!settings.variable) source = 'with(obj||{}){\n' + source + '}\n';

    source = 'var __t,__p=\'\',__j=Array.prototype.join,' +
      'print=function(){__p+=__j.call(arguments,\'\');};\n' +
      source + 'return __p;\n';

    try {
      // evil is necessary for the new Function line
      /*jshint evil:true */
      render = new Function(settings.variable || 'obj', source);
    } catch (e) {
      e.source = source;
      throw e;
    }

    if (data) return render(data);
    var template = function(data) {
      return render.call(this, data);
    };

    // Provide the compiled function source as a convenience for precompilation.
    template.source = 'function(' + (settings.variable || 'obj') + '){\n' + source + '}';

    return template;
  };

})(window);

/*jshint browser:true, smarttabs:true*/

// tb_require('../../helpers.js')

(function(window, OTHelpers, undefined) {

  OTHelpers.statable = function(self, possibleStates, initialState, stateChanged,
    stateChangedFailed) {
    var previousState,
        currentState = self.currentState = initialState;

    var setState = function(state) {
      if (currentState !== state) {
        if (OTHelpers.arrayIndexOf(possibleStates, state) === -1) {
          if (stateChangedFailed && OTHelpers.isFunction(stateChangedFailed)) {
            stateChangedFailed('invalidState', state);
          }
          return;
        }

        self.previousState = previousState = currentState;
        self.currentState = currentState = state;
        if (stateChanged && OTHelpers.isFunction(stateChanged)) stateChanged(state, previousState);
      }
    };


    // Returns a number of states and returns true if the current state
    // is any of them.
    //
    // @example
    // if (this.is('connecting', 'connected')) {
    //   // do some stuff
    // }
    //
    self.is = function (/* state0:String, state1:String, ..., stateN:String */) {
      return OTHelpers.arrayIndexOf(arguments, currentState) !== -1;
    };


    // Returns a number of states and returns true if the current state
    // is none of them.
    //
    // @example
    // if (this.isNot('connecting', 'connected')) {
    //   // do some stuff
    // }
    //
    self.isNot = function (/* state0:String, state1:String, ..., stateN:String */) {
      return OTHelpers.arrayIndexOf(arguments, currentState) === -1;
    };

    return setState;
  };

})(window, window.OTHelpers);

/*!
 *  This is a modified version of Robert Kieffer awesome uuid.js library.
 *  The only modifications we've made are to remove the Node.js specific
 *  parts of the code and the UUID version 1 generator (which we don't
 *  use). The original copyright notice is below.
 *
 *     node-uuid/uuid.js
 *
 *     Copyright (c) 2010 Robert Kieffer
 *     Dual licensed under the MIT and GPL licenses.
 *     Documentation and details at https://github.com/broofa/node-uuid
 */
// tb_require('../helpers.js')

/*global crypto:true, Uint32Array:true, Buffer:true */
/*jshint browser:true, smarttabs:true*/

(function(window, OTHelpers, undefined) {

  // Unique ID creation requires a high quality random # generator, but
  // Math.random() does not guarantee "cryptographic quality".  So we feature
  // detect for more robust APIs, normalizing each method to return 128-bits
  // (16 bytes) of random data.
  var mathRNG, whatwgRNG;

  // Math.random()-based RNG.  All platforms, very fast, unknown quality
  var _rndBytes = new Array(16);
  mathRNG = function() {
    var r, b = _rndBytes, i = 0;

    for (i = 0; i < 16; i++) {
      if ((i & 0x03) === 0) r = Math.random() * 0x100000000;
      b[i] = r >>> ((i & 0x03) << 3) & 0xff;
    }

    return b;
  };

  // WHATWG crypto-based RNG - http://wiki.whatwg.org/wiki/Crypto
  // WebKit only (currently), moderately fast, high quality
  if (window.crypto && crypto.getRandomValues) {
    var _rnds = new Uint32Array(4);
    whatwgRNG = function() {
      crypto.getRandomValues(_rnds);

      for (var c = 0 ; c < 16; c++) {
        _rndBytes[c] = _rnds[c >> 2] >>> ((c & 0x03) * 8) & 0xff;
      }
      return _rndBytes;
    };
  }

  // Select RNG with best quality
  var _rng = whatwgRNG || mathRNG;

  // Buffer class to use
  var BufferClass = typeof(Buffer) == 'function' ? Buffer : Array;

  // Maps for number <-> hex string conversion
  var _byteToHex = [];
  var _hexToByte = {};
  for (var i = 0; i < 256; i++) {
    _byteToHex[i] = (i + 0x100).toString(16).substr(1);
    _hexToByte[_byteToHex[i]] = i;
  }

  // **`parse()` - Parse a UUID into it's component bytes**
  function parse(s, buf, offset) {
    var i = (buf && offset) || 0, ii = 0;

    buf = buf || [];
    s.toLowerCase().replace(/[0-9a-f]{2}/g, function(oct) {
      if (ii < 16) { // Don't overflow!
        buf[i + ii++] = _hexToByte[oct];
      }
    });

    // Zero out remaining bytes if string was short
    while (ii < 16) {
      buf[i + ii++] = 0;
    }

    return buf;
  }

  // **`unparse()` - Convert UUID byte array (ala parse()) into a string**
  function unparse(buf, offset) {
    var i = offset || 0, bth = _byteToHex;
    return  bth[buf[i++]] + bth[buf[i++]] +
            bth[buf[i++]] + bth[buf[i++]] + '-' +
            bth[buf[i++]] + bth[buf[i++]] + '-' +
            bth[buf[i++]] + bth[buf[i++]] + '-' +
            bth[buf[i++]] + bth[buf[i++]] + '-' +
            bth[buf[i++]] + bth[buf[i++]] +
            bth[buf[i++]] + bth[buf[i++]] +
            bth[buf[i++]] + bth[buf[i++]];
  }

  // **`v4()` - Generate random UUID**

  // See https://github.com/broofa/node-uuid for API details
  function v4(options, buf, offset) {
    // Deprecated - 'format' argument, as supported in v1.2
    var i = buf && offset || 0;

    if (typeof(options) == 'string') {
      buf = options == 'binary' ? new BufferClass(16) : null;
      options = null;
    }
    options = options || {};

    var rnds = options.random || (options.rng || _rng)();

    // Per 4.4, set bits for version and `clock_seq_hi_and_reserved`
    rnds[6] = (rnds[6] & 0x0f) | 0x40;
    rnds[8] = (rnds[8] & 0x3f) | 0x80;

    // Copy bytes to buffer, if provided
    if (buf) {
      for (var ii = 0; ii < 16; ii++) {
        buf[i + ii] = rnds[ii];
      }
    }

    return buf || unparse(rnds);
  }

  // Export public API
  var uuid = v4;
  uuid.v4 = v4;
  uuid.parse = parse;
  uuid.unparse = unparse;
  uuid.BufferClass = BufferClass;

  // Export RNG options
  uuid.mathRNG = mathRNG;
  uuid.whatwgRNG = whatwgRNG;

  OTHelpers.uuid = uuid;

}(window, window.OTHelpers));
/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

(function(window, OTHelpers, undefined) {

OTHelpers.useLogHelpers = function(on){

    // Log levels for OTLog.setLogLevel
    on.DEBUG    = 5;
    on.LOG      = 4;
    on.INFO     = 3;
    on.WARN     = 2;
    on.ERROR    = 1;
    on.NONE     = 0;

    var _logLevel = on.NONE,
        _logs = [],
        _canApplyConsole = true;

    try {
        Function.prototype.bind.call(window.console.log, window.console);
    } catch (err) {
        _canApplyConsole = false;
    }

    // Some objects can't be logged in the console, mostly these are certain
    // types of native objects that are exposed to JS. This is only really a
    // problem with IE, hence only the IE version does anything.
    var makeLogArgumentsSafe = function(args) { return args; };

    if (OTHelpers.browser() === 'IE') {
        makeLogArgumentsSafe = function(args) {
            return [toDebugString(Array.prototype.slice.apply(args))];
        };
    }

    // Generates a logging method for a particular method and log level.
    //
    // Attempts to handle the following cases:
    // * the desired log method doesn't exist, call fallback (if available) instead
    // * the console functionality isn't available because the developer tools (in IE)
    // aren't open, call fallback (if available)
    // * attempt to deal with weird IE hosted logging methods as best we can.
    //
    function generateLoggingMethod(method, level, fallback) {
        return function() {
            if (on.shouldLog(level)) {
                var cons = window.console,
                    args = makeLogArgumentsSafe(arguments);

                // In IE, window.console may not exist if the developer tools aren't open
                // This also means that cons and cons[method] can appear at any moment
                // hence why we retest this every time.
                if (cons && cons[method]) {
                    // the desired console method isn't a real object, which means
                    // that we can't use apply on it. We force it to be a real object
                    // using Function.bind, assuming that's available.
                    if (cons[method].apply || _canApplyConsole) {
                        if (!cons[method].apply) {
                            cons[method] = Function.prototype.bind.call(cons[method], cons);
                        }

                        cons[method].apply(cons, args);
                    }
                    else {
                        // This isn't the same result as the above, but it's better
                        // than nothing.
                        cons[method](args);
                    }
                }
                else if (fallback) {
                    fallback.apply(on, args);

                    // Skip appendToLogs, we delegate entirely to the fallback
                    return;
                }

                appendToLogs(method, makeLogArgumentsSafe(arguments));
            }
        };
    }

    on.log = generateLoggingMethod('log', on.LOG);

    // Generate debug, info, warn, and error logging methods, these all fallback to on.log
    on.debug = generateLoggingMethod('debug', on.DEBUG, on.log);
    on.info = generateLoggingMethod('info', on.INFO, on.log);
    on.warn = generateLoggingMethod('warn', on.WARN, on.log);
    on.error = generateLoggingMethod('error', on.ERROR, on.log);


    on.setLogLevel = function(level) {
        _logLevel = typeof(level) === 'number' ? level : 0;
        on.debug("TB.setLogLevel(" + _logLevel + ")");
        return _logLevel;
    };

    on.getLogs = function() {
        return _logs;
    };

    // Determine if the level is visible given the current logLevel.
    on.shouldLog = function(level) {
        return _logLevel >= level;
    };

    // Format the current time nicely for logging. Returns the current
    // local time.
    function formatDateStamp() {
        var now = new Date();
        return now.toLocaleTimeString() + now.getMilliseconds();
    }

    function toJson(object) {
        try {
            return JSON.stringify(object);
        } catch(e) {
            return object.toString();
        }
    }

    function toDebugString(object) {
        var components = [];

        if (typeof(object) === 'undefined') {
            // noop
        }
        else if (object === null) {
            components.push('NULL');
        }
        else if (OTHelpers.isArray(object)) {
            for (var i=0; i<object.length; ++i) {
                components.push(toJson(object[i]));
            }
        }
        else if (OTHelpers.isObject(object)) {
            for (var key in object) {
                var stringValue;

                if (!OTHelpers.isFunction(object[key])) {
                    stringValue = toJson(object[key]);
                }
                else if (object.hasOwnProperty(key)) {
                    stringValue = 'function ' + key + '()';
                }

                components.push(key + ': ' + stringValue);
            }
        }
        else if (OTHelpers.isFunction(object)) {
            try {
                components.push(object.toString());
            } catch(e) {
                components.push('function()');
            }
        }
        else  {
            components.push(object.toString());
        }

        return components.join(", ");
    }

    // Append +args+ to logs, along with the current log level and the a date stamp.
    function appendToLogs(level, args) {
        if (!args) return;

        var message = toDebugString(args);
        if (message.length <= 2) return;

        _logs.push(
            [level, formatDateStamp(), message]
        );
    }
};

OTHelpers.useLogHelpers(OTHelpers);
OTHelpers.setLogLevel(OTHelpers.ERROR);

})(window, window.OTHelpers);

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

(function(window, OTHelpers, undefined) {

OTHelpers.castToBoolean = function(value, defaultValue) {
    if (value === undefined) return defaultValue;
    return value === 'true' || value === true;
};

OTHelpers.roundFloat = function(value, places) {
    return Number(value.toFixed(places));
};

})(window, window.OTHelpers);
/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('../vendor/uuid.js')

(function(window, OTHelpers, undefined) {

  var timeouts = [],
      messageName = 'OTHelpers.' + OTHelpers.uuid.v4() + '.zero-timeout';

  var handleMessage = function(event) {
    if (event.data === messageName) {
      if(OTHelpers.isFunction(event.stopPropagation)) {
        event.stopPropagation();
      }
      event.cancelBubble = true;
      if (timeouts.length > 0) {
        var args = timeouts.shift(),
            fn = args.shift();

        fn.apply(null, args);
      }
    }
  };

  if(window.addEventListener) {
    window.addEventListener('message', handleMessage, true);
  } else if(window.attachEvent) {
    window.attachEvent('onmessage', handleMessage);
  }

  // Calls the function +fn+ asynchronously with the current execution.
  // This is most commonly used to execute something straight after
  // the current function.
  //
  // Any arguments in addition to +fn+ will be passed to +fn+ when it's
  // called.
  //
  // You would use this inplace of setTimeout(fn, 0) type constructs. callAsync
  // is preferable as it executes in a much more predictable time window,
  // unlike setTimeout which could execute anywhere from 2ms to several thousand
  // depending on the browser/context.
  //
  OTHelpers.callAsync = function (/* fn, [arg1, arg2, ..., argN] */) {
    timeouts.push(Array.prototype.slice.call(arguments));
    window.postMessage(messageName, '*');
  };


  // Wraps +handler+ in a function that will execute it asynchronously
  // so that it doesn't interfere with it's exceution context if it raises
  // an exception.
  OTHelpers.createAsyncHandler = function(handler) {
    return function() {
      var args = Array.prototype.slice.call(arguments);

      OTHelpers.callAsync(function() {
        handler.apply(null, args);
      });
    };
  };

})(window, window.OTHelpers);

/*jshint browser:true, smarttabs:true*/
/*global jasmine:true*/

// tb_require('../../helpers.js')
// tb_require('../callbacks.js')

(function(window, OTHelpers, undefined) {

/**
* This base class defines the <code>on</code>, <code>once</code>, and <code>off</code>
* methods of objects that can dispatch events.
*
* @class EventDispatcher
*/
  OTHelpers.eventing = function(self, syncronous) {
    var _events = {};


    // Call the defaultAction, passing args
    function executeDefaultAction(defaultAction, args) {
      if (!defaultAction) return;

      defaultAction.apply(null, args.slice());
    }

    // Execute each handler in +listeners+ with +args+.
    //
    // Each handler will be executed async. On completion the defaultAction
    // handler will be executed with the args.
    //
    // @param [Array] listeners
    //    An array of functions to execute. Each will be passed args.
    //
    // @param [Array] args
    //    An array of arguments to execute each function in  +listeners+ with.
    //
    // @param [String] name
    //    The name of this event.
    //
    // @param [Function, Null, Undefined] defaultAction
    //    An optional function to execute after every other handler. This will execute even
    //    if +listeners+ is empty. +defaultAction+ will be passed args as a normal
    //    handler would.
    //
    // @return Undefined
    //
    function executeListenersAsyncronously(name, args, defaultAction) {
      var listeners = _events[name];
      if (!listeners || listeners.length === 0) return;

      var listenerAcks = listeners.length;

      OTHelpers.forEach(listeners, function(listener) { // , index
        function filterHandlerAndContext(_listener) {
          return _listener.context === listener.context && _listener.handler === listener.handler;
        }

        // We run this asynchronously so that it doesn't interfere with execution if an
        // error happens
        OTHelpers.callAsync(function() {
          try {
            // have to check if the listener has not been removed
            if (_events[name] && OTHelpers.some(_events[name], filterHandlerAndContext)) {
              (listener.closure || listener.handler).apply(listener.context || null, args);
            }
          }
          finally {
            listenerAcks--;

            if (listenerAcks === 0) {
              executeDefaultAction(defaultAction, args);
            }
          }
        });
      });
    }


    // This is identical to executeListenersAsyncronously except that handlers will
    // be executed syncronously.
    //
    // On completion the defaultAction handler will be executed with the args.
    //
    // @param [Array] listeners
    //    An array of functions to execute. Each will be passed args.
    //
    // @param [Array] args
    //    An array of arguments to execute each function in  +listeners+ with.
    //
    // @param [String] name
    //    The name of this event.
    //
    // @param [Function, Null, Undefined] defaultAction
    //    An optional function to execute after every other handler. This will execute even
    //    if +listeners+ is empty. +defaultAction+ will be passed args as a normal
    //    handler would.
    //
    // @return Undefined
    //
    function executeListenersSyncronously(name, args) { // defaultAction is not used
      var listeners = _events[name];
      if (!listeners || listeners.length === 0) return;

      OTHelpers.forEach(listeners, function(listener) { // index
        (listener.closure || listener.handler).apply(listener.context || null, args);
      });
    }

    var executeListeners = syncronous === true ?
      executeListenersSyncronously : executeListenersAsyncronously;


    var removeAllListenersNamed = function (eventName, context) {
      if (_events[eventName]) {
        if (context) {
          // We are removing by context, get only events that don't
          // match that context
          _events[eventName] = OTHelpers.filter(_events[eventName], function(listener){
            return listener.context !== context;
          });
        }
        else {
          delete _events[eventName];
        }
      }
    };

    var addListeners = OTHelpers.bind(function (eventNames, handler, context, closure) {
      var listener = {handler: handler};
      if (context) listener.context = context;
      if (closure) listener.closure = closure;

      OTHelpers.forEach(eventNames, function(name) {
        if (!_events[name]) _events[name] = [];
        _events[name].push(listener);
      });
    }, self);


    var removeListeners = function (eventNames, handler, context) {
      function filterHandlerAndContext(listener) {
        return !(listener.handler === handler && listener.context === context);
      }

      OTHelpers.forEach(eventNames, OTHelpers.bind(function(name) {
        if (_events[name]) {
          _events[name] = OTHelpers.filter(_events[name], filterHandlerAndContext);
          if (_events[name].length === 0) delete _events[name];
        }
      }, self));

    };

    // Execute any listeners bound to the +event+ Event.
    //
    // Each handler will be executed async. On completion the defaultAction
    // handler will be executed with the args.
    //
    // @param [Event] event
    //    An Event object.
    //
    // @param [Function, Null, Undefined] defaultAction
    //    An optional function to execute after every other handler. This will execute even
    //    if there are listeners bound to this event. +defaultAction+ will be passed
    //    args as a normal handler would.
    //
    // @return this
    //
    self.dispatchEvent = function(event, defaultAction) {
      if (!event.type) {
        OTHelpers.error('OTHelpers.Eventing.dispatchEvent: Event has no type');
        OTHelpers.error(event);

        throw new Error('OTHelpers.Eventing.dispatchEvent: Event has no type');
      }

      if (!event.target) {
        event.target = this;
      }

      if (!_events[event.type] || _events[event.type].length === 0) {
        executeDefaultAction(defaultAction, [event]);
        return;
      }

      executeListeners(event.type, [event], defaultAction);

      return this;
    };

    // Execute each handler for the event called +name+.
    //
    // Each handler will be executed async, and any exceptions that they throw will
    // be caught and logged
    //
    // How to pass these?
    //  * defaultAction
    //
    // @example
    //  foo.on('bar', function(name, message) {
    //    alert("Hello " + name + ": " + message);
    //  });
    //
    //  foo.trigger('OpenTok', 'asdf');     // -> Hello OpenTok: asdf
    //
    //
    // @param [String] eventName
    //    The name of this event.
    //
    // @param [Array] arguments
    //    Any additional arguments beyond +eventName+ will be passed to the handlers.
    //
    // @return this
    //
    self.trigger = function(eventName) {
      if (!_events[eventName] || _events[eventName].length === 0) {
        return;
      }

      var args = Array.prototype.slice.call(arguments);

      // Remove the eventName arg
      args.shift();

      executeListeners(eventName, args);

      return this;
    };

   /**
    * Adds an event handler function for one or more events.
    *
    * <p>
    * The following code adds an event handler for one event:
    * </p>
    *
    * <pre>
    * obj.on("eventName", function (event) {
    *     // This is the event handler.
    * });
    * </pre>
    *
    * <p>If you pass in multiple event names and a handler method, the handler is
    * registered for each of those events:</p>
    *
    * <pre>
    * obj.on("eventName1 eventName2",
    *        function (event) {
    *            // This is the event handler.
    *        });
    * </pre>
    *
    * <p>You can also pass in a third <code>context</code> parameter (which is optional) to
    * define the value of <code>this</code> in the handler method:</p>
    *
    * <pre>obj.on("eventName",
    *        function (event) {
    *            // This is the event handler.
    *        },
    *        obj);
    * </pre>
    *
    * <p>
    * The method also supports an alternate syntax, in which the first parameter is an object
    * that is a hash map of event names and handler functions and the second parameter (optional)
    * is the context for this in each handler:
    * </p>
    * <pre>
    * obj.on(
    *    {
    *       eventName1: function (event) {
    *               // This is the handler for eventName1.
    *           },
    *       eventName2:  function (event) {
    *               // This is the handler for eventName2.
    *           }
    *    },
    *    obj);
    * </pre>
    *
    * <p>
    * If you do not add a handler for an event, the event is ignored locally.
    * </p>
    *
    * @param {String} type The string identifying the type of event. You can specify multiple event
    * names in this string, separating them with a space. The event handler will process each of
    * the events.
    * @param {Function} handler The handler function to process the event. This function takes
    * the event object as a parameter.
    * @param {Object} context (Optional) Defines the value of <code>this</code> in the event
    * handler function.
    *
    * @returns {EventDispatcher} The EventDispatcher object.
    *
    * @memberOf EventDispatcher
    * @method #on
    * @see <a href="#off">off()</a>
    * @see <a href="#once">once()</a>
    * @see <a href="#events">Events</a>
    */
    self.on = function(eventNames, handlerOrContext, context) {
      if (typeof(eventNames) === 'string' && handlerOrContext) {
        addListeners(eventNames.split(' '), handlerOrContext, context);
      }
      else {
        for (var name in eventNames) {
          if (eventNames.hasOwnProperty(name)) {
            addListeners([name], eventNames[name], handlerOrContext);
          }
        }
      }

      return this;
    };

   /**
    * Removes an event handler or handlers.
    *
    * <p>If you pass in one event name and a handler method, the handler is removed for that
    * event:</p>
    *
    * <pre>obj.off("eventName", eventHandler);</pre>
    *
    * <p>If you pass in multiple event names and a handler method, the handler is removed for
    * those events:</p>
    *
    * <pre>obj.off("eventName1 eventName2", eventHandler);</pre>
    *
    * <p>If you pass in an event name (or names) and <i>no</i> handler method, all handlers are
    * removed for those events:</p>
    *
    * <pre>obj.off("event1Name event2Name");</pre>
    *
    * <p>If you pass in no arguments, <i>all</i> event handlers are removed for all events
    * dispatched by the object:</p>
    *
    * <pre>obj.off();</pre>
    *
    * <p>
    * The method also supports an alternate syntax, in which the first parameter is an object that
    * is a hash map of event names and handler functions and the second parameter (optional) is
    * the context for this in each handler:
    * </p>
    * <pre>
    * obj.off(
    *    {
    *       eventName1: event1Handler,
    *       eventName2: event2Handler
    *    });
    * </pre>
    *
    * @param {String} type (Optional) The string identifying the type of event. You can
    * use a space to specify multiple events, as in "accessAllowed accessDenied
    * accessDialogClosed". If you pass in no <code>type</code> value (or other arguments),
    * all event handlers are removed for the object.
    * @param {Function} handler (Optional) The event handler function to remove. The handler
    * must be the same function object as was passed into <code>on()</code>. Be careful with
    * helpers like <code>bind()</code> that return a new function when called. If you pass in
    * no <code>handler</code>, all event handlers are removed for the specified event
    * <code>type</code>.
    * @param {Object} context (Optional) If you specify a <code>context</code>, the event handler
    * is removed for all specified events and handlers that use the specified context. (The
    * context must match the context passed into <code>on()</code>.)
    *
    * @returns {Object} The object that dispatched the event.
    *
    * @memberOf EventDispatcher
    * @method #off
    * @see <a href="#on">on()</a>
    * @see <a href="#once">once()</a>
    * @see <a href="#events">Events</a>
    */
    self.off = function(eventNames, handlerOrContext, context) {
      if (typeof eventNames === 'string') {
        if (handlerOrContext && OTHelpers.isFunction(handlerOrContext)) {
          removeListeners(eventNames.split(' '), handlerOrContext, context);
        }
        else {
          OTHelpers.forEach(eventNames.split(' '), function(name) {
            removeAllListenersNamed(name, handlerOrContext);
          }, this);
        }

      } else if (!eventNames) {
        // remove all bound events
        _events = {};

      } else {
        for (var name in eventNames) {
          if (eventNames.hasOwnProperty(name)) {
            removeListeners([name], eventNames[name], handlerOrContext);
          }
        }
      }

      return this;
    };


   /**
    * Adds an event handler function for one or more events. Once the handler is called,
    * the specified handler method is removed as a handler for this event. (When you use
    * the <code>on()</code> method to add an event handler, the handler is <i>not</i>
    * removed when it is called.) The <code>once()</code> method is the equivilent of
    * calling the <code>on()</code>
    * method and calling <code>off()</code> the first time the handler is invoked.
    *
    * <p>
    * The following code adds a one-time event handler for the <code>accessAllowed</code> event:
    * </p>
    *
    * <pre>
    * obj.once("eventName", function (event) {
    *    // This is the event handler.
    * });
    * </pre>
    *
    * <p>If you pass in multiple event names and a handler method, the handler is registered
    * for each of those events:</p>
    *
    * <pre>obj.once("eventName1 eventName2"
    *          function (event) {
    *              // This is the event handler.
    *          });
    * </pre>
    *
    * <p>You can also pass in a third <code>context</code> parameter (which is optional) to define
    * the value of
    * <code>this</code> in the handler method:</p>
    *
    * <pre>obj.once("eventName",
    *          function (event) {
    *              // This is the event handler.
    *          },
    *          obj);
    * </pre>
    *
    * <p>
    * The method also supports an alternate syntax, in which the first parameter is an object that
    * is a hash map of event names and handler functions and the second parameter (optional) is the
    * context for this in each handler:
    * </p>
    * <pre>
    * obj.once(
    *    {
    *       eventName1: function (event) {
    *                  // This is the event handler for eventName1.
    *           },
    *       eventName2:  function (event) {
    *                  // This is the event handler for eventName1.
    *           }
    *    },
    *    obj);
    * </pre>
    *
    * @param {String} type The string identifying the type of event. You can specify multiple
    * event names in this string, separating them with a space. The event handler will process
    * the first occurence of the events. After the first event, the handler is removed (for
    * all specified events).
    * @param {Function} handler The handler function to process the event. This function takes
    * the event object as a parameter.
    * @param {Object} context (Optional) Defines the value of <code>this</code> in the event
    * handler function.
    *
    * @returns {Object} The object that dispatched the event.
    *
    * @memberOf EventDispatcher
    * @method #once
    * @see <a href="#on">on()</a>
    * @see <a href="#off">off()</a>
    * @see <a href="#events">Events</a>
    */
    self.once = function(eventNames, handler, context) {
      var names = eventNames.split(' '),
          fun = OTHelpers.bind(function() {
            var result = handler.apply(context || null, arguments);
            removeListeners(names, handler, context);

            return result;
          }, this);

      addListeners(names, handler, context, fun);
      return this;
    };


    /**
    * Deprecated; use <a href="#on">on()</a> or <a href="#once">once()</a> instead.
    * <p>
    * This method registers a method as an event listener for a specific event.
    * <p>
    *
    * <p>
    *   If a handler is not registered for an event, the event is ignored locally. If the
    *   event listener function does not exist, the event is ignored locally.
    * </p>
    * <p>
    *   Throws an exception if the <code>listener</code> name is invalid.
    * </p>
    *
    * @param {String} type The string identifying the type of event.
    *
    * @param {Function} listener The function to be invoked when the object dispatches the event.
    *
    * @param {Object} context (Optional) Defines the value of <code>this</code> in the event
    * handler function.
    *
    * @memberOf EventDispatcher
    * @method #addEventListener
    * @see <a href="#on">on()</a>
    * @see <a href="#once">once()</a>
    * @see <a href="#events">Events</a>
    */
    // See 'on' for usage.
    // @depreciated will become a private helper function in the future.
    self.addEventListener = function(eventName, handler, context) {
      OTHelpers.warn('The addEventListener() method is deprecated. Use on() or once() instead.');
      addListeners([eventName], handler, context);
    };


    /**
    * Deprecated; use <a href="#on">on()</a> or <a href="#once">once()</a> instead.
    * <p>
    * Removes an event listener for a specific event.
    * <p>
    *
    * <p>
    *   Throws an exception if the <code>listener</code> name is invalid.
    * </p>
    *
    * @param {String} type The string identifying the type of event.
    *
    * @param {Function} listener The event listener function to remove.
    *
    * @param {Object} context (Optional) If you specify a <code>context</code>, the event
    * handler is removed for all specified events and event listeners that use the specified
    context. (The context must match the context passed into
    * <code>addEventListener()</code>.)
    *
    * @memberOf EventDispatcher
    * @method #removeEventListener
    * @see <a href="#off">off()</a>
    * @see <a href="#events">Events</a>
    */
    // See 'off' for usage.
    // @depreciated will become a private helper function in the future.
    self.removeEventListener = function(eventName, handler, context) {
      OTHelpers.warn('The removeEventListener() method is deprecated. Use off() instead.');
      removeListeners([eventName], handler, context);
    };





    return self;
  };

  OTHelpers.eventing.Event = function() {

    return function (type, cancelable) {
      this.type = type;
      this.cancelable = cancelable !== undefined ? cancelable : true;

      var _defaultPrevented = false;

      this.preventDefault = function() {
        if (this.cancelable) {
          _defaultPrevented = true;
        } else {
          OTHelpers.warn('Event.preventDefault :: Trying to preventDefault ' +
            'on an Event that isn\'t cancelable');
        }
      };

      this.isDefaultPrevented = function() {
        return _defaultPrevented;
      };
    };

  };
  
})(window, window.OTHelpers);

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./callbacks.js')

// DOM helpers
(function(window, OTHelpers, undefined) {

OTHelpers.isElementNode = function(node) {
    return node && typeof node === 'object' && node.nodeType == 1;
};

// Returns true if the client supports element.classList
OTHelpers.supportsClassList = function() {
    var hasSupport = typeof(document !== "undefined") && ("classList" in document.createElement("a"));
    OTHelpers.supportsClassList = function() { return hasSupport; };

    return hasSupport;
};

OTHelpers.removeElement = function(element) {
    if (element && element.parentNode) {
        element.parentNode.removeChild(element);
    }
};

OTHelpers.removeElementById = function(elementId) {
    this.removeElement(OTHelpers(elementId));
};

OTHelpers.removeElementsByType = function(parentElem, type) {
    if (!parentElem) return;

    var elements = parentElem.getElementsByTagName(type);

    // elements is a "live" NodesList collection. Meaning that the collection
    // itself will be mutated as we remove elements from the DOM. This means
    // that "while there are still elements" is safer than "iterate over each
    // element" as the collection length and the elements indices will be modified
    // with each iteration.
    while (elements.length) {
        parentElem.removeChild(elements[0]);
    }
};

OTHelpers.emptyElement = function(element) {
    while (element.firstChild) {
        element.removeChild(element.firstChild);
    }
    return element;
};

OTHelpers.createElement = function(nodeName, attributes, children, doc) {
    var element = (doc || document).createElement(nodeName);

    if (attributes) {
        for (var name in attributes) {
            if (typeof(attributes[name]) === 'object') {
                if (!element[name]) element[name] = {};

                var subAttrs = attributes[name];
                for (var n in subAttrs) {
                    element[name][n] = subAttrs[n];
                }
            }
            else if (name === 'className') {
                element.className = attributes[name];
            }
            else {
                element.setAttribute(name, attributes[name]);
            }
        }
    }

    var setChildren = function(child) {
        if(typeof child === 'string') {
            element.innerHTML = element.innerHTML + child;
        } else {
            element.appendChild(child);
        }
    };

    if(OTHelpers.isArray(children)) {
        OTHelpers.forEach(children, setChildren);
    } else if(children) {
        setChildren(children);
    }

    return element;
};

OTHelpers.createButton = function(innerHTML, attributes, events) {
    var button = OTHelpers.createElement('button', attributes, innerHTML);

    if (events) {
        for (var name in events) {
            if (events.hasOwnProperty(name)) {
                OTHelpers.on(button, name, events[name]);
            }
        }

        button._boundEvents = events;
    }

    return button;
};

// Helper function for adding event listeners to dom elements.
// WARNING: This doesn't preserve event types, your handler could be getting all kinds of different
// parameters depending on the browser. You also may have different scopes depending on the browser
// and bubbling and cancelable are not supported.
OTHelpers.on = function(element, eventName,  handler) {
    if (element.addEventListener) {
        element.addEventListener(eventName, handler, false);
    } else if (element.attachEvent) {
        element.attachEvent("on" + eventName, handler);
    } else {
        var oldHandler = element["on"+eventName];
        element["on"+eventName] = function() {
          handler.apply(this, arguments);
          if (oldHandler) oldHandler.apply(this, arguments);
        };
    }
    return element;
};

// Helper function for removing event listeners from dom elements.
OTHelpers.off = function(element, eventName, handler) {
    if (element.removeEventListener) {
        element.removeEventListener (eventName, handler,false);
    }
    else if (element.detachEvent) {
        element.detachEvent("on" + eventName, handler);
    }
};


// Detects when an element is not part of the document flow because it or one of it's ancesters has display:none.
OTHelpers.isDisplayNone = function(element) {
    if ( (element.offsetWidth === 0 || element.offsetHeight === 0) && OTHelpers.css(element, 'display') === 'none') return true;
    if (element.parentNode && element.parentNode.style) return OTHelpers.isDisplayNone(element.parentNode);
    return false;
};

OTHelpers.findElementWithDisplayNone = function(element) {
    if ( (element.offsetWidth === 0 || element.offsetHeight === 0) && OTHelpers.css(element, 'display') === 'none') return element;
    if (element.parentNode && element.parentNode.style) return OTHelpers.findElementWithDisplayNone(element.parentNode);
    return null;
};

function objectHasProperties(obj) {
    for (var key in obj) {
        if (obj.hasOwnProperty(key)) return true;
    }
    return false;
}


// Allows an +onChange+ callback to be triggered when specific style properties
// of +element+ are notified. The callback accepts a single parameter, which is
// a hash where the keys are the style property that changed and the values are
// an array containing the old and new values ([oldValue, newValue]).
//
// Width and Height changes while the element is display: none will not be
// fired until such time as the element becomes visible again.
//
// This function returns the MutationObserver itself. Once you no longer wish
// to observe the element you should call disconnect on the observer.
//
// Observing changes:
//  // observe changings to the width and height of object
//  dimensionsObserver = OTHelpers.observeStyleChanges(object, ['width', 'height'], function(changeSet) {
//      OT.debug("The new width and height are " + changeSet.width[1] + ',' + changeSet.height[1]);
//  });
//
// Cleaning up
//  // stop observing changes
//  dimensionsObserver.disconnect();
//  dimensionsObserver = null;
//
OTHelpers.observeStyleChanges = function(element, stylesToObserve, onChange) {
    var oldStyles = {};

    var getStyle = function getStyle(style) {
            switch (style) {
            case 'width':
                return OTHelpers.width(element);

            case 'height':
                return OTHelpers.height(element);

            default:
                return OTHelpers.css(element);
            }
        };

    // get the inital values
    OTHelpers.forEach(stylesToObserve, function(style) {
        oldStyles[style] = getStyle(style);
    });

    var observer = new MutationObserver(function(mutations) {
        var changeSet = {};

        OTHelpers.forEach(mutations, function(mutation) {
            if (mutation.attributeName !== 'style') return;

            var isHidden = OTHelpers.isDisplayNone(element);

            OTHelpers.forEach(stylesToObserve, function(style) {
                if(isHidden && (style == 'width' || style == 'height')) return;
                
                var newValue = getStyle(style);

                if (newValue !== oldStyles[style]) {
                    // OT.debug("CHANGED " + style + ": " + oldStyles[style] + " -> " + newValue);

                    changeSet[style] = [oldStyles[style], newValue];
                    oldStyles[style] = newValue;
                }
            });
        });

        if (objectHasProperties(changeSet)) {
            // Do this after so as to help avoid infinite loops of mutations.
            OTHelpers.callAsync(function() {
                onChange.call(null, changeSet);
            });
        }
    });

    observer.observe(element, {
        attributes:true,
        attributeFilter: ['style'],
        childList:false,
        characterData:false,
        subtree:false
    });

    return observer;
};


// trigger the +onChange+ callback whenever
// 1. +element+ is removed
// 2. or an immediate child of +element+ is removed.
//
// This function returns the MutationObserver itself. Once you no longer wish
// to observe the element you should call disconnect on the observer.
//
// Observing changes:
//  // observe changings to the width and height of object
//  nodeObserver = OTHelpers.observeNodeOrChildNodeRemoval(object, function(removedNodes) {
//      OT.debug("Some child nodes were removed");
//      OTHelpers.forEach(removedNodes, function(node) {
//          OT.debug(node);
//      });
//  });
//
// Cleaning up
//  // stop observing changes
//  nodeObserver.disconnect();
//  nodeObserver = null;
//
OTHelpers.observeNodeOrChildNodeRemoval = function(element, onChange) {
    var observer = new MutationObserver(function(mutations) {
        var removedNodes = [];

        OTHelpers.forEach(mutations, function(mutation) {
            if (mutation.removedNodes.length) {
                removedNodes = removedNodes.concat(Array.prototype.slice.call(mutation.removedNodes));
            }
        });

        if (removedNodes.length) {
            // Do this after so as to help avoid infinite loops of mutations.
            OTHelpers.callAsync(function() {
                onChange(removedNodes);
            });
        }
    });

    observer.observe(element, {
        attributes:false,
        childList:true,
        characterData:false,
        subtree:true
    });

    return observer;
};

})(window, window.OTHelpers);


/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./dom.js')

(function(window, OTHelpers, undefined) {

  OTHelpers.Modal = function(options) {

    OTHelpers.eventing(this, true);

    var callback = arguments[arguments.length - 1];

    if(!OTHelpers.isFunction(callback)) {
      throw new Error('OTHelpers.Modal2 must be given a callback');
    }

    if(arguments.length < 2) {
      options = {};
    }

    var domElement = document.createElement('iframe');

    domElement.id = options.id || OTHelpers.uuid();
    domElement.style.position = 'absolute';
    domElement.style.position = 'fixed';
    domElement.style.height = '100%';
    domElement.style.width = '100%';
    domElement.style.top = '0px';
    domElement.style.left = '0px';
    domElement.style.right = '0px';
    domElement.style.bottom = '0px';
    domElement.style.zIndex = 1000;
    domElement.style.border = '0';

    try {
      domElement.style.backgroundColor = 'rgba(0,0,0,0.2)';
    } catch (err) {
      // Old IE browsers don't support rgba and we still want to show the upgrade message
      // but we just make the background of the iframe completely transparent.
      domElement.style.backgroundColor = 'transparent';
      domElement.setAttribute('allowTransparency', 'true');
    }

    domElement.scrolling = 'no';
    domElement.setAttribute('scrolling', 'no');

    var wrappedCallback = function() {
      var doc = domElement.contentDocument || domElement.contentWindow.document;
      doc.body.style.backgroundColor = 'transparent';
      doc.body.style.border = 'none';
      callback(
        domElement.contentWindow,
        doc
      );
    };

    document.body.appendChild(domElement);
    
    if(OTHelpers.browserVersion().iframeNeedsLoad) {
      OTHelpers.on(domElement, 'load', wrappedCallback);
    } else {
      setTimeout(wrappedCallback);
    }

    this.close = function() {
      OTHelpers.removeElement(domElement);
      this.trigger('closed');
      this.element = domElement = null;
      return this;
    };

    this.element = domElement;

  };
  
})(window, window.OTHelpers);

/*
 * getComputedStyle from 
 * https://github.com/jonathantneal/Polyfills-for-IE8/blob/master/getComputedStyle.js

// tb_require('../helpers.js')
// tb_require('./dom.js')

/*jshint strict: false, eqnull: true, browser:true, smarttabs:true*/

(function(window, OTHelpers, undefined) {

  /*jshint eqnull: true, browser: true */


  function getPixelSize(element, style, property, fontSize) {
    var sizeWithSuffix = style[property],
        size = parseFloat(sizeWithSuffix),
        suffix = sizeWithSuffix.split(/\d/)[0],
        rootSize;

    fontSize = fontSize != null ?
      fontSize : /%|em/.test(suffix) && element.parentElement ?
        getPixelSize(element.parentElement, element.parentElement.currentStyle, 'fontSize', null) :
        16;
    rootSize = property === 'fontSize' ?
      fontSize : /width/i.test(property) ? element.clientWidth : element.clientHeight;

    return (suffix === 'em') ?
      size * fontSize : (suffix === 'in') ?
        size * 96 : (suffix === 'pt') ?
          size * 96 / 72 : (suffix === '%') ?
            size / 100 * rootSize : size;
  }

  function setShortStyleProperty(style, property) {
    var
    borderSuffix = property === 'border' ? 'Width' : '',
    t = property + 'Top' + borderSuffix,
    r = property + 'Right' + borderSuffix,
    b = property + 'Bottom' + borderSuffix,
    l = property + 'Left' + borderSuffix;

    style[property] = (style[t] === style[r] === style[b] === style[l] ? [style[t]]
    : style[t] === style[b] && style[l] === style[r] ? [style[t], style[r]]
    : style[l] === style[r] ? [style[t], style[r], style[b]]
    : [style[t], style[r], style[b], style[l]]).join(' ');
  }

  function CSSStyleDeclaration(element) {
    var currentStyle = element.currentStyle,
        style = this,
        fontSize = getPixelSize(element, currentStyle, 'fontSize', null),
        property;

    for (property in currentStyle) {
      if (/width|height|margin.|padding.|border.+W/.test(property) && style[property] !== 'auto') {
        style[property] = getPixelSize(element, currentStyle, property, fontSize) + 'px';
      } else if (property === 'styleFloat') {
        /*jshint -W069 */
        style['float'] = currentStyle[property];
      } else {
        style[property] = currentStyle[property];
      }
    }

    setShortStyleProperty(style, 'margin');
    setShortStyleProperty(style, 'padding');
    setShortStyleProperty(style, 'border');

    style.fontSize = fontSize + 'px';

    return style;
  }

  CSSStyleDeclaration.prototype = {
    constructor: CSSStyleDeclaration,
    getPropertyPriority: function () {},
    getPropertyValue: function ( prop ) {
      return this[prop] || '';
    },
    item: function () {},
    removeProperty: function () {},
    setProperty: function () {},
    getPropertyCSSValue: function () {}
  };

  function getComputedStyle(element) {
    return new CSSStyleDeclaration(element);
  }


  OTHelpers.getComputedStyle = function(element) {
    if(element &&
        element.ownerDocument &&
        element.ownerDocument.defaultView &&
        element.ownerDocument.defaultView.getComputedStyle) {
      return element.ownerDocument.defaultView.getComputedStyle(element);
    } else {
      return getComputedStyle(element);
    }
  };

})(window, window.OTHelpers);

// DOM Attribute helpers helpers

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./dom.js')

(function(window, OTHelpers, undefined) {

OTHelpers.addClass = function(element, value) {
    // Only bother targeting Element nodes, ignore Text Nodes, CDATA, etc
    if (element.nodeType !== 1) {
        return;
    }

    var classNames = OTHelpers.trim(value).split(/\s+/),
        i, l;

    if (OTHelpers.supportsClassList()) {
        for (i=0, l=classNames.length; i<l; ++i) {
            element.classList.add(classNames[i]);
        }

        return;
    }

    // Here's our fallback to browsers that don't support element.classList

    if (!element.className && classNames.length === 1) {
        element.className = value;
    }
    else {
        var setClass = " " + element.className + " ";

        for (i=0, l=classNames.length; i<l; ++i) {
            if ( !~setClass.indexOf( " " + classNames[i] + " ")) {
                setClass += classNames[i] + " ";
            }
        }

        element.className = OTHelpers.trim(setClass);
    }

    return this;
};

OTHelpers.removeClass = function(element, value) {
    if (!value) return;

    // Only bother targeting Element nodes, ignore Text Nodes, CDATA, etc
    if (element.nodeType !== 1) {
        return;
    }

    var newClasses = OTHelpers.trim(value).split(/\s+/),
        i, l;

    if (OTHelpers.supportsClassList()) {
        for (i=0, l=newClasses.length; i<l; ++i) {
            element.classList.remove(newClasses[i]);
        }

        return;
    }

    var className = (" " + element.className + " ").replace(/[\s+]/, ' ');

    for (i=0,l=newClasses.length; i<l; ++i) {
        className = className.replace(' ' + newClasses[i] + ' ', ' ');
    }

    element.className = OTHelpers.trim(className);

    return this;
};


/**
 * Methods to calculate element widths and heights.
 */

var _width = function(element) {
        if (element.offsetWidth > 0) {
            return element.offsetWidth + 'px';
        }

        return OTHelpers.css(element, 'width');
    },

    _height = function(element) {
        if (element.offsetHeight > 0) {
            return element.offsetHeight + 'px';
        }

        return OTHelpers.css(element, 'height');
    };

OTHelpers.width = function(element, newWidth) {
    if (newWidth) {
        OTHelpers.css(element, 'width', newWidth);
        return this;
    }
    else {
        if (OTHelpers.isDisplayNone(element)) {
            // We can't get the width, probably since the element is hidden.
            return OTHelpers.makeVisibleAndYield(element, function() {
                return _width(element);
            });
        }
        else {
            return _width(element);
        }
    }
};

OTHelpers.height = function(element, newHeight) {
    if (newHeight) {
        OTHelpers.css(element, 'height', newHeight);
        return this;
    }
    else {
        if (OTHelpers.isDisplayNone(element)) {
            // We can't get the height, probably since the element is hidden.
            return OTHelpers.makeVisibleAndYield(element, function() {
                return _height(element);
            });
        }
        else {
            return _height(element);
        }
    }
};

// Centers +element+ within the window. You can pass through the width and height
// if you know it, if you don't they will be calculated for you.
OTHelpers.centerElement = function(element, width, height) {
    if (!width) width = parseInt(OTHelpers.width(element), 10);
    if (!height) height = parseInt(OTHelpers.height(element), 10);

    var marginLeft = -0.5 * width + "px";
    var marginTop = -0.5 * height + "px";
    OTHelpers.css(element, "margin", marginTop + " 0 0 " + marginLeft);
    OTHelpers.addClass(element, "OT_centered");
};

})(window, window.OTHelpers);

// CSS helpers helpers

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')
// tb_require('./dom.js')
// tb_require('./getcomputedstyle.js')

(function(window, OTHelpers, undefined) {

  var displayStateCache = {},
      defaultDisplays = {};

  var defaultDisplayValueForElement = function(element) {
      if (defaultDisplays[element.ownerDocument] &&
        defaultDisplays[element.ownerDocument][element.nodeName]) {
        return defaultDisplays[element.ownerDocument][element.nodeName];
      }

      if (!defaultDisplays[element.ownerDocument]) defaultDisplays[element.ownerDocument] = {};
    
      // We need to know what display value to use for this node. The easiest way
      // is to actually create a node and read it out.
      var testNode = element.ownerDocument.createElement(element.nodeName),
          defaultDisplay;

      element.ownerDocument.body.appendChild(testNode);
      defaultDisplay = defaultDisplays[element.ownerDocument][element.nodeName] =
        OTHelpers.css(testNode, 'display');

      OTHelpers.removeElement(testNode);
      testNode = null;

      return defaultDisplay;
    };

  var isHidden = function(element) {
    var computedStyle = OTHelpers.getComputedStyle(element);
    return computedStyle.getPropertyValue('display') === 'none';
  };

  OTHelpers.show = function(element) {
    var display = element.style.display;

    if (display === '' || display === 'none') {
      element.style.display = displayStateCache[element] || '';
      delete displayStateCache[element];
    }

    if (isHidden(element)) {
      // It's still hidden so there's probably a stylesheet that declares this
      // element as display:none;
      displayStateCache[element] = 'none';

      element.style.display = defaultDisplayValueForElement(element);
    }

    return this;
  };

  OTHelpers.hide = function(element) {
    if (element.style.display === 'none') return;

    displayStateCache[element] = element.style.display;
    element.style.display = 'none';

    return this;
  };

  OTHelpers.css = function(element, nameOrHash, value) {
    if (typeof(nameOrHash) !== 'string') {
      var style = element.style;

      for (var cssName in nameOrHash) {
        style[cssName] = nameOrHash[cssName];
      }

      return this;

    } else if (value !== undefined) {
      element.style[nameOrHash] = value;
      return this;

    } else {
      // Normalise vendor prefixes from the form MozTranform to -moz-transform
      // except for ms extensions, which are weird...

      var name = nameOrHash.replace( /([A-Z]|^ms)/g, '-$1' ).toLowerCase(),
          computedStyle = OTHelpers.getComputedStyle(element),
          currentValue = computedStyle.getPropertyValue(name);

      if (currentValue === '') {
        currentValue = element.style[name];
      }

      return currentValue;
    }
  };


// Apply +styles+ to +element+ while executing +callback+, restoring the previous
// styles after the callback executes.
  OTHelpers.applyCSS = function(element, styles, callback) {
    var oldStyles = {},
        name,
        ret;

    // Backup the old styles
    for (name in styles) {
      if (styles.hasOwnProperty(name)) {
        // We intentionally read out of style here, instead of using the css
        // helper. This is because the css helper uses querySelector and we
        // only want to pull values out of the style (domeElement.style) hash.
        oldStyles[name] = element.style[name];

        OTHelpers.css(element, name, styles[name]);
      }
    }

    ret = callback();

    // Restore the old styles
    for (name in styles) {
      if (styles.hasOwnProperty(name)) {
        OTHelpers.css(element, name, oldStyles[name] || '');
      }
    }

    return ret;
  };

// Make +element+ visible while executing +callback+.
  OTHelpers.makeVisibleAndYield = function(element, callback) {
    // find whether it's the element or an ancester that's display none and
    // then apply to whichever it is
    var targetElement = OTHelpers.findElementWithDisplayNone(element);
    if (!targetElement) return;

    return OTHelpers.applyCSS(targetElement, {
      display: 'block',
      visibility: 'hidden'
    }, callback);
  };

})(window, window.OTHelpers);

// AJAX helpers

/*jshint browser:true, smarttabs:true*/

// tb_require('../helpers.js')

(function(window, OTHelpers, undefined) {

  function formatPostData(data) { //, contentType
    // If it's a string, we assume it's properly encoded
    if (typeof(data) === 'string') return data;

    var queryString = [];

    for (var key in data) {
      queryString.push(
        encodeURIComponent(key) + '=' + encodeURIComponent(data[key])
      );
    }

    return queryString.join('&').replace(/\+/g, '%20');
  }

  OTHelpers.getJSON = function(url, options, callback) {
    options = options || {};

    var done = function(error, event) {
      if(error) {
        callback(error, event && event.target && event.target.responseText);
      } else {
        var response;

        try {
          response = JSON.parse(event.target.responseText);
        } catch(e) {
          // Badly formed JSON
          callback(e, event && event.target && event.target.responseText);
          return;
        }

        callback(null, response, event);
      }
    };

    if(options.xdomainrequest) {
      OTHelpers.xdomainRequest(url, { method: 'GET' }, done);
    } else {
      var extendedHeaders = OTHelpers.extend({
        'Accept': 'application/json'
      }, options.headers || {});

      OTHelpers.get(url, OTHelpers.extend(options || {}, {
        headers: extendedHeaders
      }), done);
    }

  };

  OTHelpers.xdomainRequest = function(url, options, callback) {
    /*global XDomainRequest*/
    var xdr = new XDomainRequest(),
        _options = options || {},
        _method = _options.method;

    if(!_method) {
      callback(new Error('No HTTP method specified in options'));
      return;
    }

    _method = _method.toUpperCase();

    if(!(_method === 'GET' || _method === 'POST')) {
      callback(new Error('HTTP method can only be '));
      return;
    }

    function done(err, event) {
      xdr.onload = xdr.onerror = xdr.ontimeout = function() {};
      xdr = void 0;
      callback(err, event);
    }


    xdr.onload = function() {
      done(null, {
        target: {
          responseText: xdr.responseText,
          headers: {
            'content-type': xdr.contentType
          }
        }
      });
    };

    xdr.onerror = function() {
      done(new Error('XDomainRequest of ' + url + ' failed'));
    };

    xdr.ontimeout = function() {
      done(new Error('XDomainRequest of ' + url + ' timed out'));
    };

    xdr.open(_method, url);
    xdr.send(options.body && formatPostData(options.body));

  };

  OTHelpers.request = function(url, options, callback) {
    var request = new XMLHttpRequest(),
        _options = options || {},
        _method = _options.method;

    if(!_method) {
      callback(new Error('No HTTP method specified in options'));
      return;
    }

    // Setup callbacks to correctly respond to success and error callbacks. This includes
    // interpreting the responses HTTP status, which XmlHttpRequest seems to ignore
    // by default.
    if(callback) {
      request.addEventListener('load', function(event) {
        var status = event.target.status;

        // We need to detect things that XMLHttpRequest considers a success,
        // but we consider to be failures.
        if ( status >= 200 && status < 300 || status === 304 ) {
          callback(null, event);
        } else {
          callback(event);
        }
      }, false);

      request.addEventListener('error', callback, false);
    }

    request.open(options.method, url, true);

    if (!_options.headers) _options.headers = {};

    for (var name in _options.headers) {
      request.setRequestHeader(name, _options.headers[name]);
    }

    request.send(options.body && formatPostData(options.body));
  };

  OTHelpers.get = function(url, options, callback) {
    var _options = OTHelpers.extend(options || {}, {
      method: 'GET'
    });
    OTHelpers.request(url, _options, callback);
  };

  OTHelpers.post = function(url, options, callback) {
    var _options = OTHelpers.extend(options || {}, {
      method: 'POST'
    });

    if(_options.xdomainrequest) {
      OTHelpers.xdomainRequest(url, _options, callback);
    } else {
      OTHelpers.request(url, _options, callback);
    }
  };

})(window, window.OTHelpers);
!(function() {

  OT.Dialogs = {};

  var addCss = function(document, url, callback) {
    var head = document.head || document.getElementsByTagName('head')[0];
    var cssTag = OT.$.createElement('link', {
      type: 'text/css',
      media: 'screen',
      rel: 'stylesheet',
      href: url
    });
    head.appendChild(cssTag);
    OT.$.on(cssTag, 'error', function(error) {
      OT.error('Could not load CSS for dialog', url, error && error.message || error);
    });
    OT.$.on(cssTag, 'load', callback);
  };

  var addDialogCSS = function(document, urls, callback) {
    var allURLs = [
      '//fonts.googleapis.com/css?family=Didact+Gothic',
      OT.properties.cssURL
    ].concat(urls);
    var remainingStylesheets = allURLs.length;
    OT.$.forEach(allURLs, function(stylesheetUrl) {
      addCss(document, stylesheetUrl, function() {
        if(--remainingStylesheets <= 0) {
          callback();
        }
      });
    });

  };
  
  var templateElement = function(classes, children, tagName) {
    var el = OT.$.createElement(tagName || 'div', { 'class': classes }, children, this);
    el.on = OT.$.bind(OT.$.on, OT.$, el);
    return el;
  };

  OT.Dialogs.AllowDeny = {};
  OT.Dialogs.AllowDeny.Chrome = {};
  OT.Dialogs.AllowDeny.Firefox = {};

  OT.Dialogs.AllowDeny.Chrome.initialPrompt = function() {
    var modal = new OT.$.Modal(function(window, document) {

      var el = templateElement.bind(document),
          close, root;

      close = el('OT_closeButton', '&times;')
        .on('click', function() {
          modal.close();
        });

      root = el('OT_root OT_dialog OT_dialog-allow-deny-chrome-first', [
        close,
        el('OT_dialog-messages', [
          el('OT_dialog-messages-main', 'Allow camera and mic access'),
          el('OT_dialog-messages-minor', 'Click the Allow button in the upper-right corner ' +
            'of your browser to enable real-time communication.'),
          el('OT_dialog-allow-highlight-chrome')
        ])
      ]);

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
      });

    });
    return modal;
  };

  OT.Dialogs.AllowDeny.Chrome.previouslyDenied = function(website) {
    var modal = new OT.$.Modal(function(window, document) {

      var el = templateElement.bind(document),
          close,
          root;

      close = el('OT_closeButton', '&times;')
        .on('click', function() {
          modal.close();
        });

      root = el('OT_root OT_dialog OT_dialog-allow-deny-chrome-pre-denied', [
        close,
        el('OT_dialog-messages', [
          el('OT_dialog-messages-main', 'Allow camera and mic access'),
          el('OT_dialog-messages-minor', [
            'To interact with this app, follow these 3 steps:',
            el('OT_dialog-3steps', [
              el('OT_dialog-3steps-step', [
                el('OT_dialog-3steps-step-num', '1'),
                'Find this icon in the URL bar and click it',
                el('OT_dialog-allow-camera-icon')
              ]),
              el('OT_dialog-3steps-seperator'),
              el('OT_dialog-3steps-step', [
                el('OT_dialog-3steps-step-num', '2'),
                'Select "Ask if ' + website + ' wants to access your camera and mic" ' +
                  'and then click Done.'
              ]),
              el('OT_dialog-3steps-seperator'),
              el('OT_dialog-3steps-step', [
                el('OT_dialog-3steps-step-num', '3'),
                'Refresh your browser.'
              ])
            ])
          ])
        ])
      ]);

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
      });

    });
    return modal;
  };

  OT.Dialogs.AllowDeny.Chrome.deniedNow = function() {
    var modal = new OT.$.Modal(function(window, document) {

      var el = templateElement.bind(document),
          root;

      root = el('OT_root OT_dialog-blackout',
        el('OT_dialog OT_dialog-allow-deny-chrome-now-denied', [
          el('OT_dialog-messages', [
            el('OT_dialog-messages-main ',
              el('OT_dialog-allow-camera-icon')
            ),
            el('OT_dialog-messages-minor',
              'Find & click this icon to allow camera and mic access.'
            )
          ])
        ])
      );

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
      });

    });
    return modal;
  };

  OT.Dialogs.AllowDeny.Firefox.maybeDenied = function() {
    var modal = new OT.$.Modal(function(window, document) {

      var el = templateElement.bind(document),
          close,
          root;

      close = el('OT_closeButton', '&times;')
        .on('click', function() {
          modal.close();
        });

      root = el('OT_root OT_dialog OT_dialog-allow-deny-firefox-maybe-denied', [
        close,
        el('OT_dialog-messages', [
          el('OT_dialog-messages-main', 'Please allow camera & mic access'),
          el('OT_dialog-messages-minor', [
            'To interact with this app, follow these 3 steps:',
            el('OT_dialog-3steps', [
              el('OT_dialog-3steps-step', [
                el('OT_dialog-3steps-step-num', '1'),
                'Reload the page, or click the camera icon ' +
                  'in the browser URL bar.'
              ]),
              el('OT_dialog-3steps-seperator'),
              el('OT_dialog-3steps-step', [
                el('OT_dialog-3steps-step-num', '2'),
                'In the menu, select your camera & mic.'
              ]),
              el('OT_dialog-3steps-seperator'),
              el('OT_dialog-3steps-step', [
                el('OT_dialog-3steps-step-num', '3'),
                'Click "Share Selected Devices."'
              ])
            ])
          ])
        ])
      ]);

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
      });

    });
    return modal;
  };


  OT.Dialogs.AllowDeny.Firefox.denied = function() {
    var modal = new OT.$.Modal(function(window, document) {

      var el = templateElement.bind(document),
          btn = templateElement.bind(document, 'OT_dialog-button OT_dialog-button-large'),
          root,
          refreshButton;

      refreshButton = btn('Reload')
        .on('click', function() {
          modal.trigger('refresh');
        });

      root = el('OT_root OT_dialog-blackout',
        el('OT_dialog OT_dialog-allow-deny-firefox-denied', [
          el('OT_dialog-messages', [
            el('OT_dialog-messages-minor',
              'Access to camera and microphone has been denied. ' +
              'Click the button to reload page.'
            )
          ]),
          el('OT_dialog-single-button', refreshButton)
        ])
      );

      addDialogCSS(document, [], function() {
        document.body.appendChild(root);
      });

    });
    return modal;
  };

})();
!(function(window) {

  /* global OTHelpers */

  if (!window.OT) window.OT = {};

  // Bring OTHelpers in as OT.$
  OT.$ = OTHelpers.noConflict();

  // Allow events to be bound on OT
  OT.$.eventing(OT);

  // REMOVE THIS POST IE MERGE

  OT.$.defineGetters = function(self, getters, enumerable) {
    var propsDefinition = {};

    if (enumerable === void 0) enumerable = false;

    for (var key in getters) {
      propsDefinition[key] = {
        get: getters[key],
        enumerable: enumerable
      };
    }

    Object.defineProperties(self, propsDefinition);
  };

  // STOP REMOVING HERE

  // OT.$.Modal was OT.Modal before the great common-js-helpers move
  OT.Modal = OT.$.Modal;

  // Add logging methods
  OT.$.useLogHelpers(OT);

  var _debugHeaderLogged = false,
      _setLogLevel = OT.setLogLevel;

  // On the first time log level is set to DEBUG (or higher) show version info.
  OT.setLogLevel = function(level) {
    // Set OT.$ to the same log level
    OT.$.setLogLevel(level);
    var retVal = _setLogLevel.call(OT, level);
    if (OT.shouldLog(OT.DEBUG) && !_debugHeaderLogged) {
      OT.debug('OpenTok JavaScript library ' + OT.properties.version);
      OT.debug('Release notes: ' + OT.properties.websiteURL +
        '/opentok/webrtc/docs/js/release-notes.html');
      OT.debug('Known issues: ' + OT.properties.websiteURL +
        '/opentok/webrtc/docs/js/release-notes.html#knownIssues');
      _debugHeaderLogged = true;
    }
    OT.debug('OT.setLogLevel(' + retVal + ')');
    return retVal;
  };

  OT.setLogLevel(OT.properties.debug ? OT.DEBUG : OT.ERROR);

  /**
  * Sets the API log level.
  * <p>
  * Calling <code>OT.setLogLevel()</code> sets the log level for runtime log messages that
  * are the OpenTok library generates. The default value for the log level is <code>OT.ERROR</code>.
  * </p>
  * <p>
  * The OpenTok JavaScript library displays log messages in the debugger console (such as
  * Firebug), if one exists.
  * </p>
  * <p>
  * The following example logs the session ID to the console, by calling <code>OT.log()</code>.
  * The code also logs an error message when it attempts to publish a stream before the Session
  * object dispatches a <code>sessionConnected</code> event.
  * </p>
  * <pre>
  * OT.setLogLevel(OT.LOG);
  * session = OT.initSession(sessionId);
  * OT.log(sessionId);
  * publisher = OT.initPublisher("publishContainer");
  * session.publish(publisher);
  * </pre>
  *
  * @param {Number} logLevel The degree of logging desired by the developer:
  *
  * <p>
  * <ul>
  *   <li>
  *     <code>OT.NONE</code> &#151; API logging is disabled.
  *   </li>
  *   <li>
  *     <code>OT.ERROR</code> &#151; Logging of errors only.
  *   </li>
  *   <li>
  *     <code>OT.WARN</code> &#151; Logging of warnings and errors.
  *   </li>
  *   <li>
  *     <code>OT.INFO</code> &#151; Logging of other useful information, in addition to
  *     warnings and errors.
  *   </li>
  *   <li>
  *     <code>OT.LOG</code> &#151; Logging of <code>OT.log()</code> messages, in addition
  *     to OpenTok info, warning,
  *     and error messages.
  *   </li>
  *   <li>
  *     <code>OT.DEBUG</code> &#151; Fine-grained logging of all API actions, as well as
  *     <code>OT.log()</code> messages.
  *   </li>
  * </ul>
  * </p>
  *
  * @name OT.setLogLevel
  * @memberof OT
  * @function
  * @see <a href="#log">OT.log()</a>
  */

  /**
  * Sends a string to the the debugger console (such as Firebug), if one exists.
  * However, the function only logs to the console if you have set the log level
  * to <code>OT.LOG</code> or <code>OT.DEBUG</code>,
  * by calling <code>OT.setLogLevel(OT.LOG)</code> or <code>OT.setLogLevel(OT.DEBUG)</code>.
  *
  * @param {String} message The string to log.
  *
  * @name OT.log
  * @memberof OT
  * @function
  * @see <a href="#setLogLevel">OT.setLogLevel()</a>
  */

})(window);
!(function(window) {

  // IMPORTANT This file should be included straight after helpers.js
  if (!window.OT) window.OT = {};

  if (!OT.properties) {
    throw new Error('OT.properties does not exist, please ensure that you include a valid ' +
      'properties file.');
  }

  // Consumes and overwrites OT.properties. Makes it better and stronger!
  OT.properties = function(properties) {
    var props = OT.$.clone(properties);

    props.debug = properties.debug === 'true' || properties.debug === true;
    props.supportSSL = properties.supportSSL === 'true' || properties.supportSSL === true;

    if (props.supportSSL && (window.location.protocol.indexOf('https') >= 0 ||
      window.location.protocol.indexOf('chrome-extension') >= 0)) {
      props.assetURL = props.cdnURLSSL + '/webrtc/' + props.version;
      props.loggingURL = props.loggingURLSSL;
    } else {
      props.assetURL = props.cdnURL + '/webrtc/' + props.version;
    }

    props.configURL = props.assetURL + '/js/dynamic_config.min.js';
    props.cssURL = props.assetURL + '/css/ot.min.css';

    return props;
  }(OT.properties);

})(window);
!(function() {

//--------------------------------------
// JS Dynamic Config
//--------------------------------------


  OT.Config = (function() {
    var _loaded = false,
        _global = {},
        _partners = {},
        _script,
        _head = document.head || document.getElementsByTagName('head')[0],
        _loadTimer,

        _clearTimeout = function() {
          if (_loadTimer) {
            clearTimeout(_loadTimer);
            _loadTimer = null;
          }
        },

        _cleanup = function() {
          _clearTimeout();

          if (_script) {
            _script.onload = _script.onreadystatechange = null;

            if ( _head && _script.parentNode ) {
              _head.removeChild( _script );
            }

            _script = undefined;
          }
        },

        _onLoad = function() {
          // Only IE and Opera actually support readyState on Script elements.
          if (_script.readyState && !/loaded|complete/.test( _script.readyState )) {
              // Yeah, we're not ready yet...
            return;
          }

          _clearTimeout();

          if (!_loaded) {
            // Our config script is loaded but there is not config (as
            // replaceWith wasn't called). Something went wrong. Possibly
            // the file we loaded wasn't actually a valid config file.
            _this._onLoadTimeout();
          }
        },

        _getModule = function(moduleName, apiKey) {
          if (apiKey && _partners[apiKey] && _partners[apiKey][moduleName]) {
            return _partners[apiKey][moduleName];
          }

          return _global[moduleName];
        },

        _this;

    _this = {
      // In ms
      loadTimeout: 4000,

      load: function(configUrl) {
        if (!configUrl) throw new Error('You must pass a valid configUrl to Config.load');

        _loaded = false;

        setTimeout(function() {
          _script = document.createElement( 'script' );
          _script.async = 'async';
          _script.src = configUrl;
          _script.onload = _script.onreadystatechange = _onLoad.bind(this);
          _head.appendChild(_script);
        },1);

        _loadTimer = setTimeout(function() {
          _this._onLoadTimeout();
        }, this.loadTimeout);
      },

      _onLoadTimeout: function() {
        _cleanup();

        OT.warn('TB DynamicConfig failed to load in ' + _this.loadTimeout + ' ms');
        this.trigger('dynamicConfigLoadFailed');
      },

      isLoaded: function() {
        return _loaded;
      },

      reset: function() {
        _cleanup();
        _loaded = false;
        _global = {};
        _partners = {};
      },

      // This is public so that the dynamic config file can load itself.
      // Using it for other purposes is discouraged, but not forbidden.
      replaceWith: function(config) {
        _cleanup();

        if (!config) config = {};

        _global = config.global || {};
        _partners = config.partners || {};

        if (!_loaded) _loaded = true;
        this.trigger('dynamicConfigChanged');
      },

      // @example Get the value that indicates whether exceptionLogging is enabled
      //  OT.Config.get('exceptionLogging', 'enabled');
      //
      // @example Get a key for a specific partner, fallback to the default if there is
      // no key for that partner
      //  OT.Config.get('exceptionLogging', 'enabled', 'apiKey');
      //
      get: function(moduleName, key, apiKey) {
        var module = _getModule(moduleName, apiKey);
        return module ? module[key] : null;
      }
    };

    OT.$.eventing(_this);

    return _this;
  })();

})(window);
!(function() {
/*global OT:true */

  var defaultAspectRatio = 4.0/3.0,
      miniWidth = 128,
      miniHeight = 128,
      microWidth = 64,
      microHeight = 64;

  // This code positions the video element so that we don't get any letterboxing.
  // It will take into consideration aspect ratios other than 4/3 but only when
  // the video element is first created. If the aspect ratio changes at a later point
  // this calculation will become incorrect.
  function fixAspectRatio(element, width, height, desiredAspectRatio, rotated) {
    if (!width) width = parseInt(OT.$.width(element.parentNode), 10);
    else width = parseInt(width, 10);

    if (!height) height = parseInt(OT.$.height(element.parentNode), 10);
    else height = parseInt(height, 10);

    if (width === 0 || height === 0) return;

    if (!desiredAspectRatio) desiredAspectRatio = defaultAspectRatio;

    var actualRatio = (width + 0.0)/height,
        props;

    props = {
      width: '100%',
      height: '100%',
      left: 0,
      top: 0
    };

    if (actualRatio > desiredAspectRatio) {
        // Width is largest so we blow up the height so we don't have letterboxing
      var newHeight = (actualRatio / desiredAspectRatio) * 100;

      props.height = newHeight + '%';
      props.top = '-' + ((newHeight - 100) / 2) + '%';
    } else if (actualRatio < desiredAspectRatio) {
      // Height is largest, blow up the width
      var newWidth = (desiredAspectRatio / actualRatio) * 100;

      props.width = newWidth + '%';
      props.left = '-' + ((newWidth - 100) / 2) + '%';
    }

    OT.$.css(element, props);

    var video = element.querySelector('video');
    if(video) {
      if(rotated) {
        var w = element.offsetWidth,
            h = element.offsetHeight,
            diff = w - h;
        props = {
          width: h + 'px',
          height: w + 'px',
          marginTop: -(diff / 2) + 'px',
          marginLeft: (diff / 2) + 'px'
        };
        OT.$.css(video, props);
      } else {
        OT.$.css(video, { width: '', height: '', marginTop: '', marginLeft: ''});
      }
    }
  }

  function fixMini(container, width, height) {
    var w = parseInt(width, 10),
        h = parseInt(height, 10);

    if(w < microWidth || h < microHeight) {
      OT.$.addClass(container, 'OT_micro');
    } else {
      OT.$.removeClass(container, 'OT_micro');
    }
    if(w < miniWidth || h < miniHeight) {
      OT.$.addClass(container, 'OT_mini');
    } else {
      OT.$.removeClass(container, 'OT_mini');
    }
  }

  var getOrCreateContainer = function getOrCreateContainer(elementOrDomId, insertMode) {
    var container,
        domId;

    if (elementOrDomId && elementOrDomId.nodeName) {
      // It looks like we were given a DOM element. Grab the id or generate
      // one if it doesn't have one.
      container = elementOrDomId;
      if (!container.getAttribute('id') || container.getAttribute('id').length === 0) {
        container.setAttribute('id', 'OT_' + OT.$.uuid());
      }

      domId = container.getAttribute('id');
    } else {
      // We may have got an id, try and get it's DOM element.
      container = OT.$(elementOrDomId);
      domId = elementOrDomId || ('OT_' + OT.$.uuid());
    }

    if (!container) {
      container = OT.$.createElement('div', {id: domId});
      container.style.backgroundColor = '#000000';
      document.body.appendChild(container);
    } else {
      if(!(insertMode == null || insertMode === 'replace')) {
        var placeholder = document.createElement('div');
        placeholder.id = ('OT_' + OT.$.uuid());
        if(insertMode === 'append') {
          container.appendChild(placeholder);
          container = placeholder;
        } else if(insertMode === 'before') {
          container.parentNode.insertBefore(placeholder, container);
          container = placeholder;
        } else if(insertMode === 'after') {
          container.parentNode.insertBefore(placeholder, container.nextSibling);
          container = placeholder;
        }
      } else {
        OT.$.emptyElement(container);
      }
    }

    return container;
  };

  // Creates the standard container that the Subscriber and Publisher use to hold
  // their video element and other chrome.
  OT.WidgetView = function(targetElement, properties) {
    var container = getOrCreateContainer(targetElement, properties && properties.insertMode),
        videoContainer = document.createElement('div'),
        oldContainerStyles = {},
        dimensionsObserver,
        videoElement,
        videoObserver,
        posterContainer,
        loadingContainer,
        width,
        height,
        loading = true;

    if (properties) {
      width = properties.width;
      height = properties.height;

      if (width) {
        if (typeof(width) === 'number') {
          width = width + 'px';
        }
      }

      if (height) {
        if (typeof(height) === 'number') {
          height = height + 'px';
        }
      }

      container.style.width = width ? width : '264px';
      container.style.height = height ? height : '198px';
      container.style.overflow = 'hidden';
      fixMini(container, width || '264px', height || '198px');

      if (properties.mirror === undefined || properties.mirror) {
        OT.$.addClass(container, 'OT_mirrored');
      }
    }

    if (properties.classNames) OT.$.addClass(container, properties.classNames);
    OT.$.addClass(container, 'OT_loading');


    OT.$.addClass(videoContainer, 'OT_video-container');
    videoContainer.style.width = container.style.width;
    videoContainer.style.height = container.style.height;
    container.appendChild(videoContainer);
    fixAspectRatio(videoContainer, container.offsetWidth, container.offsetHeight);

    loadingContainer = document.createElement('div');
    OT.$.addClass(loadingContainer, 'OT_video-loading');
    videoContainer.appendChild(loadingContainer);

    posterContainer = document.createElement('div');
    OT.$.addClass(posterContainer, 'OT_video-poster');
    videoContainer.appendChild(posterContainer);

    oldContainerStyles.width = container.offsetWidth;
    oldContainerStyles.height = container.offsetHeight;

    // Observe changes to the width and height and update the aspect ratio
    dimensionsObserver = OT.$.observeStyleChanges(container, ['width', 'height'],
      function(changeSet) {
      var width = changeSet.width ? changeSet.width[1] : container.offsetWidth,
          height = changeSet.height ? changeSet.height[1] : container.offsetHeight;
      fixMini(container, width, height);
      fixAspectRatio(videoContainer, width, height, videoElement ?
        videoElement.aspectRatio : null);
    });


    // @todo observe if the video container or the video element get removed
    // if they do we should do some cleanup
    videoObserver = OT.$.observeNodeOrChildNodeRemoval(container, function(removedNodes) {
      if (!videoElement) return;

      // This assumes a video element being removed is the main video element. This may
      // not be the case.
      var videoRemoved = removedNodes.some(function(node) {
        return node === videoContainer || node.nodeName === 'VIDEO';
      });

      if (videoRemoved) {
        videoElement.destroy();
        videoElement = null;
      }

      if (videoContainer) {
        OT.$.removeElement(videoContainer);
        videoContainer = null;
      }

      if (dimensionsObserver) {
        dimensionsObserver.disconnect();
        dimensionsObserver = null;
      }

      if (videoObserver) {
        videoObserver.disconnect();
        videoObserver = null;
      }
    });

    this.destroy = function() {
      if (dimensionsObserver) {
        dimensionsObserver.disconnect();
        dimensionsObserver = null;
      }

      if (videoObserver) {
        videoObserver.disconnect();
        videoObserver = null;
      }

      if (videoElement) {
        videoElement.destroy();
        videoElement = null;
      }

      if (container) {
        OT.$.removeElement(container);
        container = null;
      }
    };

    Object.defineProperties(this, {

      showPoster: {
        get: function() {
          return !OT.$.isDisplayNone(posterContainer);
        },
        set: function(shown) {
          if(shown) {
            OT.$.show(posterContainer);
          } else {
            OT.$.hide(posterContainer);
          }
        }
      },

      poster: {
        get: function() {
          return OT.$.css(posterContainer, 'backgroundImage');
        },
        set: function(src) {
          OT.$.css(posterContainer, 'backgroundImage', 'url(' + src + ')');
        }
      },

      loading: {
        get: function() { return loading; },
        set: function(l) {
          loading = l;

          if (loading) {
            OT.$.addClass(container, 'OT_loading');
          } else {
            OT.$.removeClass(container, 'OT_loading');
          }
        }
      },

      video: {
        get: function() { return videoElement; },
        set: function(video) {
          // remove the old video element if it exists
          // @todo this might not be safe, publishers/subscribers use this as well...
          if (videoElement) videoElement.destroy();

          video.appendTo(videoContainer);
          videoElement = video;

          videoElement.on({
            orientationChanged: function(){
              fixAspectRatio(videoContainer, container.offsetWidth, container.offsetHeight,
                videoElement.aspectRatio, videoElement.isRotated);
            }
          });

          if (videoElement) {
            var fix = function() {
              fixAspectRatio(videoContainer, container.offsetWidth, container.offsetHeight,
                videoElement ? videoElement.aspectRatio : null,
                videoElement ? videoElement.isRotated : null);
            };
            if(isNaN(videoElement.aspectRatio)) {
              videoElement.on('streamBound', fix);
            } else {
              fix();
            }
          }
        }
      },

      domElement: {
        get: function() { return container; }
      },

      domId: {
        get: function() { return container.getAttribute('id'); }
      }
    });

    this.addError = function(errorMsg, helpMsg, classNames) {
      container.innerHTML = '<p>' + errorMsg +
        (helpMsg ? ' <span class="ot-help-message">' + helpMsg + '</span>' : '') +
        '</p>';
      OT.$.addClass(container, classNames || 'OT_subscriber_error');
      if(container.querySelector('p').offsetHeight > container.offsetHeight) {
        container.querySelector('span').style.display = 'none';
      }
    };
  };

})(window);
// Web OT Helpers
!(function(window) {

  /* global mozRTCPeerConnection */

  var nativeGetUserMedia,
      mozToW3CErrors,
      chromeToW3CErrors,
      gumNamesToMessages,
      mapVendorErrorName,
      parseErrorEvent,
      areInvalidConstraints;

  // Handy cross-browser getUserMedia shim. Inspired by some code from Adam Barth
  nativeGetUserMedia = (function() {
    if (navigator.getUserMedia) {
      return navigator.getUserMedia.bind(navigator);
    } else if (navigator.mozGetUserMedia) {
      return navigator.mozGetUserMedia.bind(navigator);
    } else if (navigator.webkitGetUserMedia) {
      return navigator.webkitGetUserMedia.bind(navigator);
    }
  })();


  if (navigator.webkitGetUserMedia) {
    /*global webkitMediaStream, webkitRTCPeerConnection*/
    // Stub for getVideoTracks for Chrome < 26
    if (!webkitMediaStream.prototype.getVideoTracks) {
      webkitMediaStream.prototype.getVideoTracks = function() {
        return this.videoTracks;
      };
    }

    // Stubs for getAudioTracks for Chrome < 26
    if (!webkitMediaStream.prototype.getAudioTracks) {
      webkitMediaStream.prototype.getAudioTracks = function() {
        return this.audioTracks;
      };
    }

    if (!webkitRTCPeerConnection.prototype.getLocalStreams) {
      webkitRTCPeerConnection.prototype.getLocalStreams = function() {
        return this.localStreams;
      };
    }

    if (!webkitRTCPeerConnection.prototype.getRemoteStreams) {
      webkitRTCPeerConnection.prototype.getRemoteStreams = function() {
        return this.remoteStreams;
      };
    }

  } else if (navigator.mozGetUserMedia) {
    // Firefox < 23 doesn't support get Video/Audio tracks, we'll just stub them out for now.
    /* global MediaStream */
    if (!MediaStream.prototype.getVideoTracks) {
      MediaStream.prototype.getVideoTracks = function() {
        return [];
      };
    }

    if (!MediaStream.prototype.getAudioTracks) {
      MediaStream.prototype.getAudioTracks = function() {
        return [];
      };
    }

    // This won't work as mozRTCPeerConnection is a weird internal Firefox
    // object (a wrapped native object I think).
    // if (!window.mozRTCPeerConnection.prototype.getLocalStreams) {
    //     window.mozRTCPeerConnection.prototype.getLocalStreams = function() {
    //         return this.localStreams;
    //     };
    // }

    // This won't work as mozRTCPeerConnection is a weird internal Firefox
    // object (a wrapped native object I think).
    // if (!window.mozRTCPeerConnection.prototype.getRemoteStreams) {
    //     window.mozRTCPeerConnection.prototype.getRemoteStreams = function() {
    //         return this.remoteStreams;
    //     };
    // }
  }


  // Mozilla error strings and the equivalent W3C names. NOT_SUPPORTED_ERROR does not
  // exist in the spec right now, so we'll include Mozilla's error description.
  mozToW3CErrors = {
    PERMISSION_DENIED: 'PermissionDeniedError',
    NOT_SUPPORTED_ERROR: 'NotSupportedError',
    MANDATORY_UNSATISFIED_ERROR: ' ConstraintNotSatisfiedError',
    NO_DEVICES_FOUND: 'NoDevicesFoundError',
    HARDWARE_UNAVAILABLE: 'HardwareUnavailableError'
  };

  // Chrome only seems to expose a single error with a code of 1 right now.
  chromeToW3CErrors = {
    1: 'PermissionDeniedError'
  };

  gumNamesToMessages = {
    PermissionDeniedError: 'End-user denied permission to hardware devices',
    NotSupportedError: 'A constraint specified is not supported by the browser.',
    ConstraintNotSatisfiedError: 'It\'s not possible to satisfy one or more constraints ' +
      'passed into the getUserMedia function',
    OverconstrainedError: 'Due to changes in the environment, one or more mandatory ' +
      'constraints can no longer be satisfied.',
    NoDevicesFoundError: 'No voice or video input devices are available on this machine.',
    HardwareUnavailableError: 'The selected voice or video devices are unavailable. Verify ' +
      'that the chosen devices are not in use by another application.'
  };

// Map vendor error strings to names and messages
  mapVendorErrorName = function mapVendorErrorName (vendorErrorName, vendorErrors) {
    var errorName = vendorErrors[vendorErrorName],
        errorMessage = gumNamesToMessages[errorName];

    if (!errorMessage) {
      // This doesn't map to a known error from the Media Capture spec, it's
      // probably a custom vendor error message.
      errorMessage = null; // This is undefined?
      errorName = vendorErrorName;
    }

    return {
      name: errorName,
      message: errorMessage
    };
  };

  // Parse and normalise a getUserMedia error event from Chrome or Mozilla
  // @ref http://dev.w3.org/2011/webrtc/editor/getusermedia.html#idl-def-NavigatorUserMediaError
  parseErrorEvent = function parseErrorObject (event) {
    var error;

    if (OT.$.isObject(event) && event.name) {
      error = {
        name: event.name,
        message: event.message || gumNamesToMessages[event.name],
        constraintName: event.constraintName
      };

    } else if (OT.$.isObject(event)) {
      error = mapVendorErrorName(event.code, chromeToW3CErrors);

      // message and constraintName are probably missing if the
      // property is also omitted, but just in case they aren't.
      if (event.message) error.message = event.message;
      if (event.constraintName) error.constraintName = event.constraintName;

    } else if (event && mozToW3CErrors.hasOwnProperty(event)) {
      error = mapVendorErrorName(event, mozToW3CErrors);

    } else {
      error = {
        message: 'Unknown Error while getting user media'
      };
    }

    return error;
  };

  // Validates a Hash of getUserMedia constraints. Currently we only
  // check to see if there is at least one non-false constraint.
  areInvalidConstraints = function(constraints) {
    if (!constraints || !OT.$.isObject(constraints)) return true;

    for (var key in constraints) {
      if (constraints[key]) return false;
    }

    return true;
  };

  // Returns true if the client supports Web RTC, false otherwise.
  //
  // Chrome Issues:
  // * The explicit prototype.addStream check is because webkitRTCPeerConnection was
  // partially implemented, but not functional, in Chrome 22.
  //
  // Firefox Issues:
  // * No real support before Firefox 19
  // * Firefox 19 has issues with generating Offers.
  // * Firefox 20 doesn't interoperate with Chrome.
  //
  OT.$.supportsWebRTC = function() {
    var _supportsWebRTC = false;

    var browser = OT.$.browserVersion(),
        minimumVersions = OT.properties.minimumVersion || {},
        minimumVersion = minimumVersions[browser.browser.toLowerCase()];

    if(minimumVersion && minimumVersion > browser.version) {
      OT.debug('Support for', browser.browser, 'is disabled because we require',
        minimumVersion, 'but this is', browser.version);
      _supportsWebRTC = false;

    } else if (navigator.webkitGetUserMedia) {
      _supportsWebRTC = typeof(webkitRTCPeerConnection) === 'function' &&
        !!webkitRTCPeerConnection.prototype.addStream;

    } else if (navigator.mozGetUserMedia) {
      if (typeof(mozRTCPeerConnection) === 'function' && browser.version > 20.0) {
        try {
          new mozRTCPeerConnection();
          _supportsWebRTC = true;
        } catch (err) {
          _supportsWebRTC = false;
        }
      }
    }

    OT.$.supportsWebRTC = function() {
      return _supportsWebRTC;
    };

    return _supportsWebRTC;
  };

  // Returns a String representing the supported WebRTC crypto scheme. The possible
  // values are SDES_SRTP, DTLS_SRTP, and NONE;
  //
  // Broadly:
  // * Firefox only supports DTLS
  // * Older versions of Chrome (<= 24) only support SDES
  // * Newer versions of Chrome (>= 25) support DTLS and SDES
  //
  OT.$.supportedCryptoScheme = function() {
    if (!OT.$.supportsWebRTC()) return 'NONE';

    var chromeVersion = window.navigator.userAgent.toLowerCase().match(/chrome\/([0-9\.]+)/i);
    return chromeVersion && parseFloat(chromeVersion[1], 10) < 25 ? 'SDES_SRTP' : 'DTLS_SRTP';
  };

  // Returns true if the browser supports bundle
  //
  // Broadly:
  // * Firefox doesn't support bundle
  // * Chrome support bundle
  //
  OT.$.supportsBundle = function() {
    return OT.$.supportsWebRTC() && OT.$.browser() === 'Chrome';
  };

  // Returns true if the browser supports rtcp mux
  //
  // Broadly:
  // * Older versions of Firefox (<= 25) don't support rtcp mux
  // * Older versions of Firefox (>= 26) support rtcp mux (not tested yet)
  // * Chrome support bundle
  //
  OT.$.supportsRtcpMux = function() {
    return OT.$.supportsWebRTC() && OT.$.browser() === 'Chrome';
  };

  // A wrapper for the builtin navigator.getUserMedia. In addition to the usual
  // getUserMedia behaviour, this helper method also accepts a accessDialogOpened
  // and accessDialogClosed callback.
  //
  // @memberof OT.$
  // @private
  //
  // @param {Object} constraints
  //      A dictionary of constraints to pass to getUserMedia. See
  //      http://dev.w3.org/2011/webrtc/editor/getusermedia.html#idl-def-MediaStreamConstraints
  //      in the Media Capture and Streams spec for more info.
  //
  // @param {function} success
  //      Called when getUserMedia completes successfully. The callback will be passed a WebRTC
  //      Stream object.
  //
  // @param {function} failure
  //      Called when getUserMedia fails to access a user stream. It will be passed an object
  //      with a code property representing the error that occurred.
  //
  // @param {function} accessDialogOpened
  //      Called when the access allow/deny dialog is opened.
  //
  // @param {function} accessDialogClosed
  //      Called when the access allow/deny dialog is closed.
  //
  // @param {function} accessDenied
  //      Called when access is denied to the camera/mic. This will be either because
  //      the user has clicked deny or because a particular origin is permanently denied.
  //

  OT.$.getUserMedia = function(constraints, success, failure, accessDialogOpened,
    accessDialogClosed, accessDenied, customGetUserMedia) {

    var getUserMedia = nativeGetUserMedia;

    if(OT.$.isFunction(customGetUserMedia)) {
      getUserMedia = customGetUserMedia;
    }

    // All constraints are false, we don't allow this. This may be valid later
    // depending on how/if we integrate data channels.
    if (areInvalidConstraints(constraints)) {
      OT.error('Couldn\'t get UserMedia: All constraints were false');
      // Using a ugly dummy-code for now.
      failure.call(null, {
        name: 'NO_VALID_CONSTRAINTS',
        message: 'Video and Audio was disabled, you need to enabled at least one'
      });

      return;
    }

    var triggerOpenedTimer = null,
        displayedPermissionDialog = false,

        finaliseAccessDialog = function() {
          if (triggerOpenedTimer) {
            clearTimeout(triggerOpenedTimer);
          }

          if (displayedPermissionDialog && accessDialogClosed) accessDialogClosed();
        },

        triggerOpened = function() {
          triggerOpenedTimer = null;
          displayedPermissionDialog = true;

          if (accessDialogOpened) accessDialogOpened();
        },

        onStream = function(stream) {
          finaliseAccessDialog();
          success.call(null, stream);
        },

        onError = function(event) {
          finaliseAccessDialog();
          var error = parseErrorEvent(event);

          // The error name 'PERMISSION_DENIED' is from an earlier version of the spec
          if (error.name === 'PermissionDeniedError') {
            var MST = window.MediaStreamTrack;
            if(MST != null && OT.$.isFunction(MST.getSources)) {
              window.MediaStreamTrack.getSources(function(sources) {
                if(sources.length > 0) {
                  accessDenied.call(null, error);
                } else {
                  failure.call(null, {
                    name: 'NoDevicesFoundError',
                    message: gumNamesToMessages.NoDevicesFoundError
                  });
                }
              });
            } else {
              accessDenied.call(null, error);
            }

          } else {
            failure.call(null, error);
          }
        };

    try {
      getUserMedia(constraints, onStream, onError);
    } catch (e) {
      OT.error('Couldn\'t get UserMedia: ' + e.toString());
      onError();
      return;
    }

    // The 'remember me' functionality of WebRTC only functions over HTTPS, if
    // we aren't on HTTPS then we should definitely be displaying the access
    // dialog.
    //
    // If we are on HTTPS, we'll wait 500ms to see if we get a stream
    // immediately. If we do then the user had clicked 'remember me'. Otherwise
    // we assume that the accessAllowed dialog is visible.
    //
    // @todo benchmark and see if 500ms is a reasonable number. It seems like
    // we should know a lot quicker.
    //
    if (location.protocol.indexOf('https') === -1) {
      // Execute after, this gives the client a chance to bind to the
      // accessDialogOpened event.
      triggerOpenedTimer = setTimeout(triggerOpened, 100);

    } else {
      // wait a second and then trigger accessDialogOpened
      triggerOpenedTimer = setTimeout(triggerOpened, 500);
    }
  };
  
  OT.$.createPeerConnection = function (config, options) {
    var NativeRTCPeerConnection = (window.webkitRTCPeerConnection || window.mozRTCPeerConnection);
    return new NativeRTCPeerConnection(config, options);
  };

})(window);
!(function(window) {

  var _videoErrorCodes = {},
      VideoOrientationTransforms;

  VideoOrientationTransforms = {
    0: 'rotate(0deg)',
    270: 'rotate(90deg)',
    90: 'rotate(-90deg)',
    180: 'rotate(180deg)'
  };

  OT.VideoOrientation = {
    ROTATED_NORMAL: 0,
    ROTATED_LEFT: 270,
    ROTATED_RIGHT: 90,
    ROTATED_UPSIDE_DOWN: 180
  };

  //   var _videoElement = new OT.VideoElement({
  //     fallbackText: 'blah'
  //   });
  //
  //   _videoElement.on({
  //     streamBound: function() {...},
  //     loadError: function() {...},
  //     error: function() {...}
  //   });
  //
  //   _videoElement.bindToStream(webRtcStream);      // => VideoElement
  //   _videoElement.appendTo(DOMElement)             // => VideoElement
  //
  //   _videoElement.stream                           // => Web RTC stream
  //   _videoElement.domElement                       // => DomNode
  //   _videoElement.parentElement                    // => DomNode
  //
  //   _videoElement.imgData                          // => PNG Data string
  //
  //   _videoElement.orientation = OT.VideoOrientation.ROTATED_LEFT;
  //
  //   _videoElement.unbindStream();
  //   _videoElement.destroy()                        // => Completely cleans up and
  //                                                  // removes the video element
  //
  //
  OT.VideoElement = function(options) {
    var _stream,
        _domElement,
        _parentElement,
        _streamBound = false,
        _videoElementMovedWarning = false,
        _options,
        _onVideoError,
        _onStreamBound,
        _onStreamBoundError,
        _playVideoOnPause;

    _options = OT.$.defaults(options || {}, {
      fallbackText: 'Sorry, Web RTC is not available in your browser'
    });

    OT.$.eventing(this);

    /// Private API
    _onVideoError = function(event) {
      var reason = 'There was an unexpected problem with the Video Stream: ' +
        videoElementErrorCodeToStr(event.target.error.code);
      this.trigger('error', null, reason, this, 'VideoElement');
    }.bind(this);

    _onStreamBound = function() {
      _streamBound = true;
      _domElement.addEventListener('error', _onVideoError, false);
      this.trigger('streamBound', this);
    }.bind(this);

    _onStreamBoundError = function(reason) {
      this.trigger('loadError', OT.ExceptionCodes.P2P_CONNECTION_FAILED, reason, this,
        'VideoElement');
    }.bind(this);

    // The video element pauses itself when it's reparented, this is
    // unfortunate. This function plays the video again and is triggered
    // on the pause event.
    _playVideoOnPause = function() {
      if(!_videoElementMovedWarning) {
        OT.warn('Video element paused, auto-resuming. If you intended to do this, use ' +
          'publishVideo(false) or subscribeToVideo(false) instead.');
        _videoElementMovedWarning = true;
      }
      _domElement.play();
    };


    _domElement = createVideoElement(_options.fallbackText, _options.attributes);

    _domElement.addEventListener('pause', _playVideoOnPause);

    /// Public Properties
    Object.defineProperties(this, {
      stream: {
        get: function() {return _stream; }
      },
      domElement: {
        get: function() {return _domElement; }
      },
      parentElement: {
        get: function() {return _parentElement; }
      },
      isBoundToStream: {
        get: function() { return _streamBound; }
      },
      poster: {
        get: function() {
          return _domElement.getAttribute('poster');
        },
        set: function(src) {
          _domElement.setAttribute('poster', src);
        }
      }
    });


    /// Public methods

    // Append the Video DOM element to a parent node
    this.appendTo = function(parentDomElement) {
      _parentElement = parentDomElement;
      _parentElement.appendChild(_domElement);

      return this;
    };

    // Bind a stream to the video element.
    this.bindToStream = function(webRtcStream) {
      _streamBound = false;
      _stream = webRtcStream;

      bindStreamToVideoElement(_domElement, _stream, _onStreamBound, _onStreamBoundError);

      return this;
    };

    // Unbind the currently bound stream from the video element.
    this.unbindStream = function() {
      if (!_stream) return this;

      if (_domElement) {
        if (!navigator.mozGetUserMedia) {
          // The browser would have released this on unload anyway, but
          // we're being a good citizen.
          window.URL.revokeObjectURL(_domElement.src);
        } else {
          _domElement.mozSrcObject = null;
        }
      }

      _stream = null;

      return this;
    };

    this.setAudioVolume = function(value) {
      if (_domElement) _domElement.volume = OT.$.roundFloat(value / 100, 2);
    };

    this.getAudioVolume = function() {
      // Return the actual volume of the DOM element
      if (_domElement) return parseInt(_domElement.volume * 100, 10);
      return 50;
    };

    this.whenTimeIncrements = function(callback, context) {
      if(_domElement) {
        var lastTime, handler;
        handler = function() {
          if(!lastTime || lastTime >= _domElement.currentTime) {
            lastTime = _domElement.currentTime;
          } else {
            _domElement.removeEventListener('timeupdate', handler, false);
            callback.call(context, this);
          }
        }.bind(this);
        _domElement.addEventListener('timeupdate', handler, false);
      }
    };

    this.destroy = function() {
      // unbind all events so they don't fire after the object is dead
      this.off();

      this.unbindStream();

      if (_domElement) {
        // Unbind this first, otherwise it will trigger when the
        // video element is removed from the DOM.
        _domElement.removeEventListener('pause', _playVideoOnPause);

        OT.$.removeElement(_domElement);
        _domElement = null;
      }

      _parentElement = null;

      return undefined;
    };
  };

  // Checking for window.defineProperty for IE compatibility,
  // just so we don't throw exceptions when the script is included
  if (OT.$.canDefineProperty) {
    // Extracts a snapshot from a video element and returns it's as a PNG Data string.
    Object.defineProperties(OT.VideoElement.prototype, {
      imgData: {
        get: function() {
          var canvas,
              imgData;

          canvas = OT.$.createElement('canvas', {
            width: this.domElement.videoWidth,
            height: this.domElement.videoHeight,
            style: {
              display: 'none'
            }
          });

          document.body.appendChild(canvas);

          try {
            canvas.getContext('2d').drawImage(this.domElement, 0, 0, canvas.width, canvas.height);
          } catch(err) {
            OT.warn('Cannot get image data yet');
            return null;
          }

          imgData = canvas.toDataURL('image/png');

          OT.$.removeElement(canvas);

          return imgData.replace('data:image/png;base64,', '').trim();
        }
      },

      videoWidth: {
        get: function() {
          return this.domElement['video' + (this.isRotated ? 'Height' : 'Width')];
        }
      },

      videoHeight: {
        get: function() {
          return this.domElement['video' + (this.isRotated ? 'Width' : 'Height')];
        }
      },

      aspectRatio: {
        get: function() {
          return (this.videoWidth + 0.0) / this.videoHeight;
        }
      },

      isRotated: {
        get: function() {
          return this._orientation && (
            this._orientation.videoOrientation === 270 ||
            this._orientation.videoOrientation === 90
          );
        }
      },

      orientation: {
        get: function() { return this._orientation; },
        set: function(orientation) {
          var transform = VideoOrientationTransforms[orientation.videoOrientation] ||
              VideoOrientationTransforms.ROTATED_NORMAL;

          this._orientation = orientation;

          switch(OT.$.browser()) {
            case 'Chrome':
            case 'Safari':
              this.domElement.style.webkitTransform = transform;
              break;

            case 'IE':
              this.domElement.style.msTransform = transform;
              break;

            default:
              // The standard version, just Firefox, Opera, and IE > 9
              this.domElement.style.transform = transform;
          }

          this.trigger('orientationChanged');
        }
      }
    });
  }

/// Private Helper functions

  function createVideoElement(fallbackText, attributes) {
    var videoElement = document.createElement('video');
    videoElement.setAttribute('autoplay', '');
    videoElement.innerHTML = fallbackText;

    if (attributes) {
      if (attributes.muted === true) {
        delete attributes.muted;
        videoElement.muted = 'true';
      }

      for (var key in attributes) {
        videoElement.setAttribute(key, attributes[key]);
      }
    }

    return videoElement;
  }


  // See http://www.w3.org/TR/2010/WD-html5-20101019/video.html#error-codes
  // Checking for window.MediaError for IE compatibility, just so we don't throw
  // exceptions when the script is included
  if (window.MediaError) {
    _videoErrorCodes[window.MediaError.MEDIA_ERR_ABORTED] = 'The fetching process for the media ' +
      'resource was aborted by the user agent at the user\'s request.';
    _videoErrorCodes[window.MediaError.MEDIA_ERR_NETWORK] = 'A network error of some description ' +
      'caused the user agent to stop fetching the media resource, after the resource was ' +
      'established to be usable.';
    _videoErrorCodes[window.MediaError.MEDIA_ERR_DECODE] = 'An error of some description ' +
      'occurred while decoding the media resource, after the resource was established to be ' +
      ' usable.';
    _videoErrorCodes[window.MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED] = 'The media resource ' +
      'indicated by the src attribute was not suitable.';
  }

  function videoElementErrorCodeToStr(errorCode) {
    return _videoErrorCodes[parseInt(errorCode, 10)] || 'An unknown error occurred.';
  }

  function bindStreamToVideoElement(videoElement, webRTCStream, onStreamBound, onStreamBoundError) {
    var cleanup,
        onLoad,
        onError,
        onStoppedLoading,
        timeout;

    // Note: onloadedmetadata doesn't fire in Chrome for audio only crbug.com/110938
    if (navigator.mozGetUserMedia || (
      webRTCStream.getVideoTracks().length > 0 && webRTCStream.getVideoTracks()[0].enabled)) {

      cleanup = function cleanup () {
        clearTimeout(timeout);
        videoElement.removeEventListener('loadedmetadata', onLoad, false);
        videoElement.removeEventListener('error', onError, false);
        webRTCStream.onended = null;
      };

      onLoad = function onLoad () {
        cleanup();
        onStreamBound();
      };

      onError = function onError (event) {
        cleanup();
        onStreamBoundError('There was an unexpected problem with the Video Stream: ' +
          videoElementErrorCodeToStr(event.target.error.code));
      };

      onStoppedLoading = function onStoppedLoading () {
        // The stream ended before we fully bound it. Maybe the other end called
        // stop on it or something else went wrong.
        cleanup();
        onStreamBoundError('Stream ended while trying to bind it to a video element.');
      };

      // Timeout if it takes too long
      timeout = setTimeout(function() {
        if (videoElement.currentTime === 0) {
          onStreamBoundError('The video stream failed to connect. Please notify the site ' +
            'owner if this continues to happen.');
        } else {
          // This should never happen
          OT.warn('Never got the loadedmetadata event but currentTime > 0');
          onStreamBound();
        }
      }.bind(this), 30000);


      videoElement.addEventListener('loadedmetadata', onLoad, false);
      videoElement.addEventListener('error', onError, false);
      webRTCStream.onended = onStoppedLoading;
    } else {
      onStreamBound();
    }

    // The official spec way is 'srcObject', we are slowly converging there.
    if (videoElement.srcObject !== void 0) {
      videoElement.srcObject = webRTCStream;
    } else if (videoElement.mozSrcObject !== void 0) {
      videoElement.mozSrcObject = webRTCStream;
    } else {
      videoElement.src = window.URL.createObjectURL(webRTCStream);
    }

    videoElement.play();
  }

})(window);
!(function(window) {

  // Singleton interval
  var logQueue = [],
      queueRunning = false;


  OT.Analytics = function() {

    var endPoint = OT.properties.loggingURL + '/logging/ClientEvent',
        endPointQos = OT.properties.loggingURL + '/logging/ClientQos',

        reportedErrors = {},

        // Map of camel-cased keys to underscored
        camelCasedKeys,

        send = function(data, isQos, callback) {
          OT.$.post(isQos ? endPointQos : endPoint, {
            body: data,
            headers: {
              'Content-Type': 'application/x-www-form-urlencoded'
            }
          }, callback);
        },

        throttledPost = function() {
          // Throttle logs so that they only happen 1 at a time
          if (!queueRunning && logQueue.length > 0) {
            queueRunning = true;
            var curr = logQueue[0];

            // Remove the current item and send the next log
            var processNextItem = function() {
              logQueue.shift();
              queueRunning = false;
              throttledPost();
            };

            if (curr) {
              send(curr.data, curr.isQos, function(err) {
                if(err) {
                  OT.debug('Failed to send ClientEvent, moving on to the next item.');
                } else {
                  curr.onComplete();
                }
                setTimeout(processNextItem, 50);
              });
            }
          }
        },

        post = function(data, onComplete, isQos) {
          logQueue.push({
            data: data,
            onComplete: onComplete,
            isQos: isQos
          });

          throttledPost();
        },

        shouldThrottleError = function(code, type, partnerId) {
          if (!partnerId) return false;

          var errKey = [partnerId, type, code].join('_'),
          //msgLimit = DynamicConfig.get('exceptionLogging', 'messageLimitPerPartner', partnerId);
            msgLimit = 100;
          if (msgLimit === null || msgLimit === undefined) return false;
          return (reportedErrors[errKey] || 0) <= msgLimit;
        };

    camelCasedKeys = {
      payloadType: 'payload_type',
      partnerId: 'partner_id',
      streamId: 'stream_id',
      sessionId: 'session_id',
      connectionId: 'connection_id',
      widgetType: 'widget_type',
      widgetId: 'widget_id',
      avgAudioBitrate: 'avg_audio_bitrate',
      avgVideoBitrate: 'avg_video_bitrate',
      localCandidateType: 'local_candidate_type',
      remoteCandidateType: 'remote_candidate_type',
      transportType: 'transport_type'
    };

    // Log an error via ClientEvents.
    //
    // @param [String] code
    // @param [String] type
    // @param [String] message
    // @param [Hash] details additional error details
    //
    // @param [Hash] options the options to log the client event with.
    // @option options [String] action The name of the Event that we are logging. E.g. 
    //  'TokShowLoaded'. Required.
    // @option options [String] variation Usually used for Split A/B testing, when you 
    //  have multiple variations of the +_action+.
    // @option options [String] payloadType A text description of the payload. Required.
    // @option options [String] payload The payload. Required.
    // @option options [String] sessionId The active OpenTok session, if there is one
    // @option options [String] connectionId The active OpenTok connectionId, if there is one
    // @option options [String] partnerId
    // @option options [String] guid ...
    // @option options [String] widgetId ...
    // @option options [String] streamId ...
    // @option options [String] section ...
    // @option options [String] build ...
    //
    // Reports will be throttled to X reports (see exceptionLogging.messageLimitPerPartner
    // from the dynamic config for X) of each error type for each partner. Reports can be
    // disabled/enabled globally or on a per partner basis (per partner settings
    // take precedence) using exceptionLogging.enabled.
    //
    this.logError = function(code, type, message, details, options) {
      if (!options) options = {};
      var partnerId = options.partnerId;

      if (OT.Config.get('exceptionLogging', 'enabled', partnerId) !== true) {
        return;
      }

      if (shouldThrottleError(code, type, partnerId)) {
        //OT.log('ClientEvents.error has throttled an error of type ' + type + '.' + 
        // code + ' for partner ' + (partnerId || 'No Partner Id'));
        return;
      }

      var errKey = [partnerId, type, code].join('_'),

      payload = this.escapePayload(OT.$.extend(details || {}, {
        message: payload,
        userAgent: navigator.userAgent
      }));


      reportedErrors[errKey] = typeof(reportedErrors[errKey]) !== 'undefined' ?
        reportedErrors[errKey] + 1 : 1;

      return this.logEvent(OT.$.extend(options, {
        action: type + '.' + code,
        payloadType: payload[0],
        payload: payload[1]
      }));
    };

    // Log a client event to the analytics backend.
    //
    // @example Logs a client event called 'foo'
    //  OT.ClientEvents.log({
    //      action: 'foo',
    //      payload_type: 'foo's payload',
    //      payload: 'bar',
    //      session_id: sessionId,
    //      connection_id: connectionId
    //  })
    //
    // @param [Hash] options the options to log the client event with.
    // @option options [String] action The name of the Event that we are logging. 
    //  E.g. 'TokShowLoaded'. Required.
    // @option options [String] variation Usually used for Split A/B testing, when 
    //  you have multiple variations of the +_action+.
    // @option options [String] payloadType A text description of the payload. Required.
    // @option options [String] payload The payload. Required.
    // @option options [String] session_id The active OpenTok session, if there is one
    // @option options [String] connection_id The active OpenTok connectionId, if there is one
    // @option options [String] partner_id
    // @option options [String] guid ...
    // @option options [String] widget_id ...
    // @option options [String] stream_id ...
    // @option options [String] section ...
    // @option options [String] build ...
    //
    this.logEvent = function(options) {
      var partnerId = options.partnerId;

      if (!options) options = {};

      // Set a bunch of defaults
      var data = OT.$.extend({
        'variation' : '',
        'guid' : this.getClientGuid(),
        'widget_id' : '',
        'session_id': '',
        'connection_id': '',
        'stream_id' : '',
        'partner_id' : partnerId,
        'source' : window.location.href,
        'section' : '',
        'build' : ''
      }, options),

      onComplete = function(){
        //  OT.log('logged: ' + '{action: ' + data['action'] + ', variation: ' + data['variation']
        //  + ', payload_type: ' + data['payload_type'] + ', payload: ' + data['payload'] + '}');
      };

      // We camel-case our names, but the ClientEvents backend wants them
      // underscored...
      for (var key in camelCasedKeys) {
        if (camelCasedKeys.hasOwnProperty(key) && data[key]) {
          data[camelCasedKeys[key]] = data[key];
          delete data[key];
        }
      }

      post(data, onComplete, false);
    };

    // Log a client QOS to the analytics backend.
    //
    this.logQOS = function(options) {
      var partnerId = options.partnerId;

      if (!options) options = {};

      // Set a bunch of defaults
      var data = OT.$.extend({
        'guid' : this.getClientGuid(),
        'widget_id' : '',
        'session_id': '',
        'connection_id': '',
        'stream_id' : '',
        'partner_id' : partnerId,
        'source' : window.location.href,
        'build' : '',
        'duration' : 0 //in milliseconds
      }, options),

      onComplete = function(){
        // OT.log('logged: ' + '{action: ' + data['action'] + ', variation: ' + data['variation']
        //  + ', payload_type: ' + data['payload_type'] + ', payload: ' + data['payload'] + '}');
      };

      // We camel-case our names, but the ClientEvents backend wants them
      // underscored...
      for (var key in camelCasedKeys) {
        if (camelCasedKeys.hasOwnProperty(key) && data[key]) {
          data[camelCasedKeys[key]] = data[key];
          delete data[key];
        }
      }

      post(data, onComplete, true);
    };

    // Converts +payload+ to two pipe seperated strings. Doesn't currently handle
    // edgecases, e.g. escaping '\\|' will break stuff.
    //
    // *Note:* It strip any keys that have null values.
    this.escapePayload = function(payload) {
      var escapedPayload = [],
          escapedPayloadDesc = [];

      for (var key in payload) {
        if (payload.hasOwnProperty(key) && payload[key] !== null && payload[key] !== undefined) {
          escapedPayload.push( payload[key] ? payload[key].toString().replace('|', '\\|') : '' );
          escapedPayloadDesc.push( key.toString().replace('|', '\\|') );
        }
      }

      return [
        escapedPayloadDesc.join('|'),
        escapedPayload.join('|')
      ];
    };

    // Uses HTML5 local storage to save a client ID.
    this.getClientGuid = function() {
      var guid = OT.$.getCookie('opentok_client_id');
      if (!guid) {
        guid = OT.$.uuid();
        OT.$.setCookie('opentok_client_id', guid);
      }
      // once we have a guid, memoise this function so if cookies & local storage are disabled
      // we still hand back the same guid each call within this page at least. OPENTOK-14015
      this.getClientGuid = function() {
        return guid;
      };
      return guid;
    };
  };

})(window);
!(function(window) {

  // This is not obvious, so to prevent end-user frustration we'll let them know
  // explicitly rather than failing with a bunch of permission errors. We don't
  // handle this using an OT Exception as it's really only a development thing.
  if (location.protocol === 'file:') {
    /*global alert*/
    alert('You cannot test a page using WebRTC through the file system due to browser ' +
      'permissions. You must run it over a web server.');
  }

  if (!window.OT) window.OT = {};

  if (!window.URL && window.webkitURL) {
    window.URL = window.webkitURL;
  }

  var // Global parameters used by upgradeSystemRequirements
      _intervalId,
      _lastHash = document.location.hash;


/**
* The first step in using the OpenTok API is to call the <code>OT.initSession()</code>
* method. Other methods of the OT object check for system requirements and set up error logging.
*
* @class OT
*/

/**
* <p class="mSummary">
* Initializes and returns the local session object for a specified session ID.
* </p>
* <p>
* You connect to an OpenTok session using the <code>connect()</code> method
* of the Session object returned by the <code>OT.initSession()</code> method.
* Note that calling <code>OT.initSession()</code> does not initiate communications
* with the cloud. It simply initializes the Session object that you can use to
* connect (and to perform other operations once connected).
* </p>
*
*  <p>
*    For an example, see <a href="Session.html#connect">Session.connect()</a>.
*  </p>
*
* @method OT.initSession
* @memberof OT
* @param {String} apiKey Your OpenTok API key (see <a href="https://dashboard.tokbox.com">the
* OpenTok dashboard</a>).
* @param {String} sessionId The session ID identifying the OpenTok session. For more
* information, see <a href="/opentok/tutorials/create-session/">Session creation</a>.
* @returns {Session} The session object through which all further interactions with
* the session will occur.
*/
  OT.initSession = function(apiKey, sessionId) {

    if(sessionId == null) {
      sessionId = apiKey;
      apiKey = null;
    }

    var session = OT.sessions.get(sessionId);

    if (!session) {
      session = new OT.Session(apiKey, sessionId);
      OT.sessions.add(session);
    }

    return session;
  };

/**
* <p class="mSummary">
*   Initializes and returns a Publisher object. You can then pass this Publisher
*   object to <code>Session.publish()</code> to publish a stream to a session.
* </p>
* <p>
*   <i>Note:</i> If you intend to reuse a Publisher object created using
*   <code>OT.initPublisher()</code> to publish to different sessions sequentially,
*   call either <code>Session.disconnect()</code> or <code>Session.unpublish()</code>.
*   Do not call both. Then call the <code>preventDefault()</code> method of the
*   <code>streamDestroyed</code> or <code>sessionDisconnected</code> event object to prevent the
*   Publisher object from being removed from the page.
* </p>
*
* @param {Object} targetElement (Optional) The DOM element or the <code>id</code> attribute of the
* existing DOM element used to determine the location of the Publisher video in the HTML DOM. See
* the <code>insertMode</code> property of the <code>properties</code> parameter. If you do not
* specify a <code>targetElement</code>, the application appends a new DOM element to the HTML
* <code>body</code>.
*
* <p>
*       The application throws an error if an element with an ID set to the
*       <code>targetElement</code> value does not exist in the HTML DOM.
* </p>
*
* @param {Object} properties (Optional) This object contains the following properties (each of which
* are optional):
* </p>
* <ul>
* <li>
*   <strong>frameRate</strong> (Number) &#151; The desired frame rate, in frames per second,
*   of the video. Valid values are 30, 15, 7, and 1. The published stream will use the closest
*   value supported on the publishing client. The frame rate can differ slightly from the value
*   you set, depending on the browser of the client. And the video will only use the desired
*   frame rate if the client configuration supports it.
*   <br><br><p>If the publisher specifies a frame rate, the actual frame rate of the video stream
*   is set as the <code>frameRate</code> property of the Stream object, though the actual frame rate
*   will vary based on changing network and system conditions. If the developer does not specify a
*   frame rate, this property is undefined.
*   <p>
*   For OpenTok cloud-enabled sessions, lowering the frame rate or lowering the resolution reduces
*   the maximum bandwidth the stream can use. However, in peer-to-peer sessions, lowering the frame 
*   rate or resolution may not reduce the stream's bandwidth.
*   </p>
*   <p>
*   You can also restrict the frame rate of a Subscriber's video stream. To restrict the frame rate
*   a Subscriber, call the <code>restrictFrameRate()</code> method of the subscriber, passing in
*   <code>true</code>.
*   (See <a href="Subscriber.html#restrictFrameRate">Subscriber.restrictFrameRate()</a>.)
*   </p>
* </li>
* <li>
*   <strong>height</strong> (Number) &#151; The desired height, in pixels, of the
*   displayed Publisher video stream (default: 198). <i>Note:</i> Use the
*   <code>height</code> and <code>width</code> properties to set the dimensions
*   of the publisher video; do not set the height and width of the DOM element
*   (using CSS).
* </li>
* <li>
*   <strong>insertMode</strong> (String) &#151; Specifies how the Publisher object will be
*   inserted in the HTML DOM. See the <code>targetElement</code> parameter. This string can
*   have the following values:
*   <ul>
*     <li><code>"replace"</code> &#151; The Publisher object replaces contents of the
*       targetElement. This is the default.</li>
*     <li><code>"after"</code> &#151; The Publisher object is a new element inserted after
*       the targetElement in the HTML DOM. (Both the Publisher and targetElement have the
*       same parent element.)</li>
*     <li><code>"before"</code> &#151; The Publisher object is a new element inserted before
*       the targetElement in the HTML DOM. (Both the Publisher and targetElement have the same
*       parent element.)</li>
*     <li><code>"append"</code> &#151; The Publisher object is a new element added as a child
*       of the targetElement. If there are other child elements, the Publisher is appended as
*       the last child element of the targetElement.</li>
*   </ul>
* </li>
* <li>
*   <strong>mirror</strong> (Boolean) &#151; Whether the publisher's video image
*   is mirrored in the publisher's page<. The default value is <code>true</code>
*   (the video image is mirrored). This property does not affect the display
*   on other subscribers' web pages.
* </li>
* <li>
*   <strong>name</strong> (String) &#151; The name for this stream. The name appears at
*   the bottom of Subscriber videos. The default value is "" (an empty string). Setting
*   this to a string longer than 1000 characters results in an runtime exception.
* </li>
* <li>
*   <strong>publishAudio</strong> (Boolean) &#151; Whether to initially publish audio
*   for the stream (default: <code>true</code>). This setting applies when you pass
*   the Publisher object in a call to the <code>Session.publish()</code> method.
* </li>
* <li>
*   <strong>publishVideo</strong> (Boolean) &#151; Whether to initially publish video
*   for the stream (default: <code>true</code>). This setting applies when you pass
*   the Publisher object in a call to the <code>Session.publish()</code> method.
* </li>
* <li>
*   <strong>resolution</strong> (String) &#151; The desired resolution of the video. The format
*   of the string is <code>"widthxheight"</code>, where the width and height are represented in
*   pixels. Valid values are <code>"1280x720"</code>, <code>"640x480"</code>, and
*   <code>"320x240"</code>. The published video will only use the desired resolution if the
*   client configuration supports it.
*   <br><br><p>
*   The requested resolution of a video stream is set as the <code>videoDimensions.width</code> and
*   <code>videoDimensions.height</code> properties of the Stream object.
*   </p>
*   <p>
*   The default resolution for a stream (if you do not specify a resolution) is 640x480 pixels.
*   If the client system cannot support the resolution you requested, the the stream will use the 
*   next largest setting supported.
*   </p>
*   <p>
*   For OpenTok cloud-enabled sessions, lowering the frame rate or lowering the resolution reduces
*   the maximum bandwidth the stream can use. However, in peer-to-peer sessions, lowering the frame
*   rate or resolution may not reduce the stream's bandwidth.
*   </p>
* </li>
* <li>
*   <strong>style</strong> (Object) &#151; An object containing properties that define the initial
*   appearance of user interface controls of the Publisher. The <code>style</code> object includes
*   the following properties:
*     <ul>
*       <li><code>backgroundImageURI</code> (String) &mdash; A URI for an image to display as
*       the background image when a video is not displayed. (A video may not be displayed if
*       you call <code>publishVideo(false)</code> on the Publisher object). You can pass an http
*       or https URI to a PNG, JPEG, or non-animated GIF file location. You can also use the
*       <code>data</code> URI scheme (instead of http or https) and pass in base-64-encrypted
*       PNG data, such as that obtained from the
*       <a href="Publisher.html#getImgData">Publisher.getImgData()</a> method. For example,
*       you could set the property to <code>"data:VBORw0KGgoAA..."</code>, where the portion of the
*       string after <code>"data:"</code> is the result of a call to
*       <code>Publisher.getImgData()</code>. If the URL or the image data is invalid, the property
*       is ignored (the attempt to set the image fails silently).</li>
*
*       <li><code>buttonDisplayMode</code> (String) &mdash; How to display the microphone controls
*       Possible values are: <code>"auto"</code> (controls are displayed when the stream is first
*       displayed and when the user mouses over the display), <code>"off"</code> (controls are not
*       displayed), and <code>"on"</code> (controls are always displayed).</li>
*
*       <li><code>nameDisplayMode</code> (String) &#151; Whether to display the stream name.
*       Possible values are: <code>"auto"</code> (the name is displayed when the stream is first
*       displayed and when the user mouses over the display), <code>"off"</code> (the name is not
*       displayed), and <code>"on"</code> (the name is always displayed).</li>
*   </ul>
* </li>
* <li>
*   <strong>width</strong> (Number) &#151; The desired width, in pixels, of the
*   displayed Publisher video stream (default: 264). <i>Note:</i> Use the
*   <code>height</code> and <code>width</code> properties to set the dimensions
*   of the publisher video; do not set the height and width of the DOM element
*   (using CSS).
* </li>
* </ul>
* @param {Function} completionHandler (Optional) A function to be called when the method succeeds
* or fails in initializing a Publisher object. This function takes one parameter &mdash;
* <code>error</code>. On success, the <code>error</code> object is set to <code>null</code>. On
* failure, the <code>error</code> object has two properties: <code>code</code> (an integer) and
* <code>message</code> (a string), which identify the cause of the failure. The method succeeds
* when the user grants access to the camera and microphone. The method fails if the user denies
* access to the camera and microphone. The <code>completionHandler</code> function is called
* before the Publisher dispatches an <code>accessAllowed</code> (success) event or an
* <code>accessDenied</code> (failure) event.
* <p>
* The following code adds a <code>completionHandler</code> when calling the
* <code>OT.initPublisher()</code> method:
* </p>
* <pre>
* var publisher = OT.initPublisher('publisher', null, function (error) {
*   if (error) {
*     console.log(error);
*   } else {
*     console.log("Publisher initialized.");
*   }
* });
* </pre>
*
* @returns {Publisher} The Publisher object.
* @see <a href="Session#publish>Session.publish()</a>
* @method OT.initPublisher
* @memberof OT
*/
  OT.initPublisher = function(targetElement, properties, completionHandler) {
    OT.debug('OT.initPublisher('+targetElement+')');

    if(targetElement != null && !(
      (typeof targetElement === 'object' && targetElement.nodeType === Node.ELEMENT_NODE) ||
      (typeof targetElement === 'string' && document.getElementById(targetElement))) &&
      typeof targetElement !== 'function') {
      targetElement = properties;
      properties = completionHandler;
      completionHandler = arguments[3];
    }
    
    if(typeof targetElement === 'function') {
      completionHandler = targetElement;
      properties = undefined;
      targetElement = undefined;
    }

    if(typeof properties === 'function') {
      completionHandler = properties;
      properties = undefined;
    }

    var publisher = new OT.Publisher();
    OT.publishers.add(publisher);

    var triggerCallback = function triggerCallback (err) {
      if (err) {
        OT.dispatchError(err.code, err.message, completionHandler, publisher.session);
      } else if (completionHandler && OT.$.isFunction(completionHandler)) {
        completionHandler.apply(null, arguments);
      }
    },

    removeInitSuccessAndCallComplete = function removeInitSuccessAndCallComplete (err) {
      publisher.off('publishComplete', removeHandlersAndCallComplete);
      triggerCallback(err);
    },

    removeHandlersAndCallComplete = function removeHandlersAndCallComplete (err) {
      publisher.off('initSuccess', removeInitSuccessAndCallComplete);

      // We're only handling the error case here as we're just
      // initing the publisher, not actually attempting to publish.
      if (err) triggerCallback(err);
    };


    publisher.once('initSuccess', removeInitSuccessAndCallComplete);
    publisher.once('publishComplete', removeHandlersAndCallComplete);

    publisher.publish(targetElement, properties);

    return publisher;
  };



/**
* Checks if the system supports OpenTok for WebRTC.
* @return {Number} Whether the system supports OpenTok for WebRTC (1) or not (0).
* @see <a href="#upgradeSystemRequirements">OT.upgradeSystemRequirements()</a>
* @method OT.checkSystemRequirements
* @memberof OT
*/
  OT.checkSystemRequirements = function() {
    OT.debug('OT.checkSystemRequirements()');

    var systemRequirementsMet = OT.$.supportsWebSockets() && OT.$.supportsWebRTC() ?
      this.HAS_REQUIREMENTS : this.NOT_HAS_REQUIREMENTS;

    OT.checkSystemRequirements = function() {
      OT.debug('OT.checkSystemRequirements()');
      return systemRequirementsMet;
    };

    return systemRequirementsMet;
  };


/**
* Displays information about system requirments for OpenTok for WebRTC. This
* information is displayed in an iframe element that fills the browser window.
* <p>
* <i>Note:</i> this information is displayed automatically when you call the
* <code>OT.initSession()</code> or the <code>OT.initPublisher()</code> method
* if the client does not support OpenTok for WebRTC.
* </p>
* @see <a href="#checkSystemRequirements">OT.checkSystemRequirements()</a>
* @method OT.upgradeSystemRequirements
* @memberof OT
*/
  OT.upgradeSystemRequirements = function(){
    // trigger after the OT environment has loaded
    OT.onLoad( function() {
      var id = '_upgradeFlash';

         // Load the iframe over the whole page.
      document.body.appendChild((function() {
        var d = document.createElement('iframe');
        d.id = id;
        d.style.position = 'absolute';
        d.style.position = 'fixed';
        d.style.height = '100%';
        d.style.width = '100%';
        d.style.top = '0px';
        d.style.left = '0px';
        d.style.right = '0px';
        d.style.bottom = '0px';
        d.style.zIndex = 1000;
        try {
          d.style.backgroundColor = 'rgba(0,0,0,0.2)';
        } catch (err) {
          // Old IE browsers don't support rgba and we still want to show the upgrade message
          // but we just make the background of the iframe completely transparent.
          d.style.backgroundColor = 'transparent';
          d.setAttribute('allowTransparency', 'true');
        }
        d.setAttribute('frameBorder', '0');
        d.frameBorder = '0';
        d.scrolling = 'no';
        d.setAttribute('scrolling', 'no');
        var browser = OT.$.browserVersion(),
            isSupportedButOld = OT.properties.minimumVersion[browser.browser.toLowerCase()];
        d.src = OT.properties.assetURL + '/html/upgrade.html#' +
                          encodeURIComponent(isSupportedButOld ? 'true' : 'false') + ',' +
                          encodeURIComponent(JSON.stringify(OT.properties.minimumVersion)) + '|' +
                          encodeURIComponent(document.location.href);
        return d;
      })());

      // Now we need to listen to the event handler if the user closes this dialog.
      // Since this is from an IFRAME within another domain we are going to listen to hash
      // changes. The best cross browser solution is to poll for a change in the hashtag.
      if (_intervalId) clearInterval(_intervalId);
      _intervalId = setInterval(function(){
        var hash = document.location.hash,
            re = /^#?\d+&/;
        if (hash !== _lastHash && re.test(hash)) {
          _lastHash = hash;
          if (hash.replace(re, '') === 'close_window'){
            document.body.removeChild(document.getElementById(id));
            document.location.hash = '';
          }
        }
      }, 100);
    });
  };


  OT.reportIssue = function(){
    OT.warn('ToDo: haven\'t yet implemented OT.reportIssue');
  };

  OT.components = {};
  OT.sessions = {};

  // namespaces
  OT.rtc = {};

// Define the APIKEY this is a global parameter which should not change
  OT.APIKEY = (function(){
    // Script embed
    var scriptSrc = (function(){
      var s = document.getElementsByTagName('script');
      s = s[s.length - 1];
      s = s.getAttribute('src') || s.src;
      return s;
    })();

    var m = scriptSrc.match(/[\?\&]apikey=([^&]+)/i);
    return m ? m[1] : '';
  })();

  OT.HAS_REQUIREMENTS = 1;
  OT.NOT_HAS_REQUIREMENTS = 0;

/**
* This method is deprecated. Use <a href="#on">on()</a> or <a href="#once">once()</a> instead.
* 
* <p>
* Registers a method as an event listener for a specific event.
* </p>
*
* <p>
* The OT object dispatches one type of event &#151; an <code>exception</code> event. The
* following code adds an event listener for the <code>exception</code> event:
* </p>
*
* <pre>
* OT.addEventListener("exception", exceptionHandler);
*
* function exceptionHandler(event) {
*    alert("exception event. \n  code == " + event.code + "\n  message == " + event.message);
* }
* </pre>
*
* <p>
*   If a handler is not registered for an event, the event is ignored locally. If the event
*   listener function does not exist, the event is ignored locally.
* </p>
* <p>
*   Throws an exception if the <code>listener</code> name is invalid.
* </p>
*
* @param {String} type The string identifying the type of event.
*
* @param {Function} listener The function to be invoked when the OT object dispatches the event.
* @see <a href="#on">on()</a>
* @see <a href="#once">once()</a>
* @memberof OT
* @method addEventListener
*/

/**
* This method is deprecated. Use <a href="#off">off()</a> instead.
* 
* <p>
* Removes an event listener for a specific event.
* </p>
*
* <p>
*   Throws an exception if the <code>listener</code> name is invalid.
* </p>
*
* @param {String} type The string identifying the type of event.
*
* @param {Function} listener The event listener function to remove.
*
* @see <a href="#off">off()</a>
* @memberof OT
* @method removeEventListener
*/


/**
* Adds an event handler function for one or more events.
*
* <p>
* The OT object dispatches one type of event &#151; an <code>exception</code> event. The following 
* code adds an event
* listener for the <code>exception</code> event:
* </p>
*
* <pre>
* OT.on("exception", function (event) {
*   // This is the event handler.
* });
* </pre>
*
* <p>You can also pass in a third <code>context</code> parameter (which is optional) to define the 
* value of
* <code>this</code> in the handler method:</p>
*
* <pre>
* OT.on("exception",
*   function (event) {
*     // This is the event handler.
*   }),
*   session
* );
* </pre>
*
* <p>
* If you do not add a handler for an event, the event is ignored locally.
* </p>
*
* @param {String} type The string identifying the type of event.
* @param {Function} handler The handler function to process the event. This function takes the event
* object as a parameter.
* @param {Object} context (Optional) Defines the value of <code>this</code> in the event handler 
* function.
*
* @memberof OT
* @method on
* @see <a href="#off">off()</a>
* @see <a href="#once">once()</a>
* @see <a href="#events">Events</a>
*/

/**
* Adds an event handler function for an event. Once the handler is called, the specified handler 
* method is
* removed as a handler for this event. (When you use the <code>OT.on()</code> method to add an event
* handler, the handler
* is <i>not</i> removed when it is called.) The <code>OT.once()</code> method is the equivilent of 
* calling the <code>OT.on()</code>
* method and calling <code>OT.off()</code> the first time the handler is invoked.
*
* <p>
* The following code adds a one-time event handler for the <code>exception</code> event:
* </p>
*
* <pre>
* OT.once("exception", function (event) {
*   console.log(event);
* }
* </pre>
*
* <p>You can also pass in a third <code>context</code> parameter (which is optional) to define the 
* value of
* <code>this</code> in the handler method:</p>
*
* <pre>
* OT.once("exception",
*   function (event) {
*     // This is the event handler.
*   },
*   session
* );
* </pre>
*
* <p>
* The method also supports an alternate syntax, in which the first parameter is an object that is a 
* hash map of
* event names and handler functions and the second parameter (optional) is the context for this in 
* each handler:
* </p>
* <pre>
* OT.once(
*   {exeption: function (event) {
*     // This is the event handler.
*     }
*   },
*   session
* );
* </pre>
*
* @param {String} type The string identifying the type of event. You can specify multiple event 
* names in this string,
* separating them with a space. The event handler will process the first occurence of the events. 
* After the first event,
* the handler is removed (for all specified events).
* @param {Function} handler The handler function to process the event. This function takes the event
* object as a parameter.
* @param {Object} context (Optional) Defines the value of <code>this</code> in the event handler 
* function.
*
* @memberof OT
* @method once
* @see <a href="#on">on()</a>
* @see <a href="#once">once()</a>
* @see <a href="#events">Events</a>
*/


/**
* Removes an event handler.
*
* <p>Pass in an event name and a handler method, the handler is removed for that event:</p>
*
* <pre>OT.off("exceptionEvent", exceptionEventHandler);</pre>
*
* <p>If you pass in an event name and <i>no</i> handler method, all handlers are removed for that 
* events:</p>
*
* <pre>OT.off("exceptionEvent");</pre>
*
* <p>
* The method also supports an alternate syntax, in which the first parameter is an object that is a 
* hash map of
* event names and handler functions and the second parameter (optional) is the context for matching 
* handlers:
* </p>
* <pre>
* OT.off(
*   {
*     exceptionEvent: exceptionEventHandler
*   },
*   this
* );
* </pre>
*
* @param {String} type (Optional) The string identifying the type of event. You can use a space to 
* specify multiple events, as in "eventName1 eventName2 eventName3". If you pass in no
* <code>type</code> value (or other arguments), all event handlers are removed for the object.
* @param {Function} handler (Optional) The event handler function to remove. If you pass in no 
* <code>handler</code>, all event handlers are removed for the specified event <code>type</code>.
* @param {Object} context (Optional) If you specify a <code>context</code>, the event handler is 
* removed for all specified events and handlers that use the specified context.
*
* @memberof OT
* @method off
* @see <a href="#on">on()</a>
* @see <a href="#once">once()</a>
* @see <a href="#events">Events</a>
*/

/**
 * Dispatched by the OT class when the app encounters an exception.
 * Note that you set up an event handler for the <code>exception</code> event by calling the
 * <code>OT.on()</code> method.
 *
 * @name exception
 * @event
 * @borrows ExceptionEvent#message as this.message
 * @memberof OT
 * @see ExceptionEvent
 */

  if (!window.OT) window.OT = OT;
  if (!window.TB) window.TB = OT;

})(window);
!(function() {

  OT.Collection = function(idField) {
    var _models = [],
        _byId = {},
        _idField = idField || 'id';

    OT.$.eventing(this, true);

    var onModelUpdate = function onModelUpdate (event) {
          this.trigger('update', event);
          this.trigger('update:'+event.target.id, event);
        }.bind(this),

        onModelDestroy = function onModelDestroyed (event) {
          this.remove(event.target, event.reason);
        }.bind(this);


    this.reset = function() {
      // Stop listening on the models, they are no longer our problem
      _models.forEach(function(model) {
        model.off('updated', onModelUpdate, this);
        model.off('destroyed', onModelDestroy, this);
      }, this);

      _models = [];
      _byId = {};
    };

    this.destroy = function() {
      _models.forEach(function(model) {
        if(model && typeof model.destroy === 'function') {
          model.destroy(void 0, true);
        }
      });

      this.reset();
      this.off();
    };

    this.get = function(id) { return id && _byId[id] !== void 0 ? _models[_byId[id]] : void 0; };
    this.has = function(id) { return id && _byId[id] !== void 0; };

    this.toString = function() { return _models.toString(); };

    // Return only models filtered by either a dict of properties
    // or a filter function.
    //
    // @example Return all publishers with a streamId of 1
    //   OT.publishers.where({streamId: 1})
    //
    // @example The same thing but filtering using a filter function
    //   OT.publishers.where(function(publisher) {
    //     return publisher.stream.id === 4;
    //   });
    //
    // @example The same thing but filtering using a filter function
    //          executed with a specific this
    //   OT.publishers.where(function(publisher) {
    //     return publisher.stream.id === 4;
    //   }, self);
    //
    this.where = function(attrsOrFilterFn, context) {
      if (OT.$.isFunction(attrsOrFilterFn)) return _models.filter(attrsOrFilterFn, context);

      return _models.filter(function(model) {
        for (var key in attrsOrFilterFn) {
          if (model[key] !== attrsOrFilterFn[key]) return false;
        }

        return true;
      });
    };

    // Similar to where in behaviour, except that it only returns
    // the first match.
    this.find = function(attrsOrFilterFn, context) {
      var filterFn;

      if (OT.$.isFunction(attrsOrFilterFn)) {
        filterFn = attrsOrFilterFn;
      }
      else {
        filterFn = function(model) {
          for (var key in attrsOrFilterFn) {
            if (model[key] !== attrsOrFilterFn[key]) return false;
          }

          return true;
        };
      }

      filterFn = filterFn.bind(context);

      for (var i=0; i<_models.length; ++i) {
        if (filterFn(_models[i]) === true) return _models[i];
      }

      return null;
    };

    this.add = function(model) {
      var id = model[_idField];

      if (this.has(id)) {
        OT.warn('Model ' + id + ' is already in the collection', _models);
        return this;
      }

      _byId[id] = _models.push(model) - 1;

      model.on('updated', onModelUpdate, this);
      model.on('destroyed', onModelDestroy, this);

      this.trigger('add', model);
      this.trigger('add:'+id, model);

      return this;
    };

    this.remove = function(model, reason) {
      var id = model[_idField];

      _models.splice(_byId[id], 1);

      // Shuffle everyone down one
      for (var i=_byId[id]; i<_models.length; ++i) {
        _byId[_models[i][_idField]] = i;
      }

      delete _byId[id];

      model.off('updated', onModelUpdate, this);
      model.off('destroyed', onModelDestroy, this);

      this.trigger('remove', model, reason);
      this.trigger('remove:'+id, model, reason);

      return this;
    };

    // Used by session connecto fire add events after adding listeners
    this._triggerAddEvents = function() {
      var models = this.where.apply(this, arguments);
      models.forEach(function(model) {
        this.trigger('add', model);
        this.trigger('add:' + model[_idField], model);
      }, this);
    };

    OT.$.defineGetters(this, {
      length: function() { return _models.length; }
    });
  };

}(this));
!(function() {

  /**
   * The Event object defines the basic OpenTok event object that is passed to
   * event listeners. Other OpenTok event classes implement the properties and methods of
   * the Event object.</p>
   *
   * <p>For example, the Stream object dispatches a <code>streamPropertyChanged</code> event when
   * the stream's properties are updated. You add a callback for an event using the
   * <code>on()</code> method of the Stream object:</p>
   *
   * <pre>
   * stream.on("streamPropertyChanged", function (event) {
   *     alert("Properties changed for stream " + event.target.streamId);
   * });</pre>
   *
   * @class Event
   * @property {Boolean} cancelable Whether the event has a default behavior that is cancelable
   * (<code>true</code>) or not (<code>false</code>). You can cancel the default behavior by
   * calling the <code>preventDefault()</code> method of the Event object in the callback
   * function. (See <a href="#preventDefault">preventDefault()</a>.)
   *
   * @property {Object} target The object that dispatched the event.
   *
   * @property {String} type  The type of event.
   */
  OT.Event = OT.$.eventing.Event();
  /**
  * Prevents the default behavior associated with the event from taking place.
  *
  * <p>To see whether an event has a default behavior, check the <code>cancelable</code> property
  * of the event object. </p>
  *
  * <p>Call the <code>preventDefault()</code> method in the callback function for the event.</p>
  *
  * <p>The following events have default behaviors:</p>
  *
  * <ul>
  *
  *   <li><code>sessionDisconnect</code> &#151; See
  *   <a href="SessionDisconnectEvent.html#preventDefault">
  *   SessionDisconnectEvent.preventDefault()</a>.</li>
  *
  *   <li><code>streamDestroyed</code> &#151; See <a href="StreamEvent.html#preventDefault">
  *   StreamEvent.preventDefault()</a>.</li>
  *
  *   <li><code>accessDialogOpened</code> &#151; See the
  *   <a href="Publisher.html#event:accessDialogOpened">accessDialogOpened event</a>.</li>
  *
  *   <li><code>accessDenied</code> &#151; See the <a href="Publisher.html#event:accessDenied">
  *   accessDenied event</a>.</li>
  *
  * </ul>
  *
  * @method #preventDefault
  * @memberof Event
  */
  /**
  * Whether the default event behavior has been prevented via a call to
  * <code>preventDefault()</code> (<code>true</code>) or not (<code>false</code>).
  * See <a href="#preventDefault">preventDefault()</a>.
  * @method #isDefaultPrevented
  * @return {Boolean}
  * @memberof Event
  */

  // Event names lookup
  OT.Event.names = {
    // Activity Status for cams/mics
    ACTIVE: 'active',
    INACTIVE: 'inactive',
    UNKNOWN: 'unknown',

    // Archive types
    PER_SESSION: 'perSession',
    PER_STREAM: 'perStream',

    // OT Events
    EXCEPTION: 'exception',
    ISSUE_REPORTED: 'issueReported',

    // Session Events
    SESSION_CONNECTED: 'sessionConnected',
    SESSION_DISCONNECTED: 'sessionDisconnected',
    STREAM_CREATED: 'streamCreated',
    STREAM_DESTROYED: 'streamDestroyed',
    CONNECTION_CREATED: 'connectionCreated',
    CONNECTION_DESTROYED: 'connectionDestroyed',
    SIGNAL: 'signal',
    STREAM_PROPERTY_CHANGED: 'streamPropertyChanged',
    MICROPHONE_LEVEL_CHANGED: 'microphoneLevelChanged',


    // Publisher Events
    RESIZE: 'resize',
    SETTINGS_BUTTON_CLICK: 'settingsButtonClick',
    DEVICE_INACTIVE: 'deviceInactive',
    INVALID_DEVICE_NAME: 'invalidDeviceName',
    ACCESS_ALLOWED: 'accessAllowed',
    ACCESS_DENIED: 'accessDenied',
    ACCESS_DIALOG_OPENED: 'accessDialogOpened',
    ACCESS_DIALOG_CLOSED: 'accessDialogClosed',
    ECHO_CANCELLATION_MODE_CHANGED: 'echoCancellationModeChanged',
    PUBLISHER_DESTROYED: 'destroyed',

    // Subscriber Events
    SUBSCRIBER_DESTROYED: 'destroyed',

    // DeviceManager Events
    DEVICES_DETECTED: 'devicesDetected',

    // DevicePanel Events
    DEVICES_SELECTED: 'devicesSelected',
    CLOSE_BUTTON_CLICK: 'closeButtonClick',

    MICLEVEL : 'microphoneActivityLevel',
    MICGAINCHANGED : 'microphoneGainChanged',

    // Environment Loader
    ENV_LOADED: 'envLoaded'
  };

  OT.ExceptionCodes = {
    JS_EXCEPTION: 2000,
    AUTHENTICATION_ERROR: 1004,
    INVALID_SESSION_ID: 1005,
    CONNECT_FAILED: 1006,
    CONNECT_REJECTED: 1007,
    CONNECTION_TIMEOUT: 1008,
    NOT_CONNECTED: 1010,
    P2P_CONNECTION_FAILED: 1013,
    API_RESPONSE_FAILURE: 1014,
    UNABLE_TO_PUBLISH: 1500,
    UNABLE_TO_SUBSCRIBE: 1501,
    UNABLE_TO_FORCE_DISCONNECT: 1520,
    UNABLE_TO_FORCE_UNPUBLISH: 1530
  };

  /**
  * The {@link OT} class dispatches <code>exception</code> events when the OpenTok API encounters
  * an exception (error). The ExceptionEvent object defines the properties of the event
  * object that is dispatched.
  *
  * <p>Note that you set up a callback for the <code>exception</code> event by calling the
  * <code>OT.on()</code> method.</p>
  *
  * @class ExceptionEvent
  * @property {Number} code The error code. The following is a list of error codes:</p>
  *
  * <table class="docs_table">
  *  <tbody><tr>
  *   <td>
  *   <b>code</b>
  *
  *   </td>
  *   <td>
  *   <b>title</b>
  *   </td>
  *  </tr>
  *
  *  <tr>
  *   <td>
  *   1004
  *
  *   </td>
  *   <td>
  *   Authentication error
  *   </td>
  *  </tr>
  *
  *  <tr>
  *   <td>
  *   1005
  *
  *   </td>
  *   <td>
  *   Invalid Session ID
  *   </td>
  *  </tr>
  *  <tr>
  *   <td>
  *   1006
  *
  *   </td>
  *   <td>
  *   Connect Failed
  *   </td>
  *  </tr>
  *  <tr>
  *   <td>
  *   1007
  *
  *   </td>
  *   <td>
  *   Connect Rejected
  *   </td>
  *  </tr>
  *  <tr>
  *   <td>
  *   1008
  *
  *   </td>
  *   <td>
  *   Connect Time-out
  *   </td>
  *  </tr>
  *  <tr>
  *   <td>
  *   1009
  *
  *   </td>
  *   <td>
  *   Security Error
  *   </td>
  *  </tr>
  *   <tr>
  *    <td>
  *    1010
  *
  *    </td>
  *    <td>
  *    Not Connected
  *    </td>
  *   </tr>
  *   <tr>
  *    <td>
  *    1011
  *
  *    </td>
  *    <td>
  *    Invalid Parameter
  *    </td>
  *   </tr>
  *   <tr>
  *    <td>
  *    1013
  *    </td>
  *    <td>
  *    Connection Failed
  *    </td>
  *   </tr>
  *   <tr>
  *    <td>
  *    1014
  *    </td>
  *    <td>
  *    API Response Failure
  *    </td>
  *   </tr>
  *
  *  <tr>
  *    <td>
  *    1500
  *    </td>
  *    <td>
  *    Unable to Publish
  *    </td>
  *   </tr>
  *
  *  <tr>
  *    <td>
  *    1520
  *    </td>
  *    <td>
  *    Unable to Force Disconnect
  *    </td>
  *   </tr>
  *
  *  <tr>
  *    <td>
  *    1530
  *    </td>
  *    <td>
  *    Unable to Force Unpublish
  *    </td>
  *   </tr>
  *  <tr>
  *    <td>
  *    1535
  *    </td>
  *    <td>
  *    Force Unpublish on Invalid Stream
  *    </td>
  *   </tr>
  *
  *  <tr>
  *    <td>
  *    2000
  *
  *    </td>
  *    <td>
  *    Internal Error
  *    </td>
  *  </tr>
  *
  *  <tr>
  *    <td>
  *    2010
  *
  *    </td>
  *    <td>
  *    Report Issue Failure
  *    </td>
  *  </tr>
  *
  *
  *  </tbody></table>
  *
  *  <p>Check the <code>message</code> property for more details about the error.</p>
  *
  * @property {String} message The error message.
  *
  * @property {Object} target The object that the event pertains to. For an
  * <code>exception</code> event, this will be an object other than the OT object
  * (such as a Session object or a Publisher object).
  *
  * @property {String} title The error title.
  * @augments Event
  */
  OT.ExceptionEvent = function (type, message, title, code, component, target) {
    OT.Event.call(this, type);

    this.message = message;
    this.title = title;
    this.code = code;
    this.component = component;
    this.target = target;
  };


  OT.IssueReportedEvent = function (type, issueId) {
    OT.Event.call(this, type);
    this.issueId = issueId;
  };

  // Triggered when the JS dynamic config and the DOM have loaded.
  OT.EnvLoadedEvent = function (type) {
    OT.Event.call(this, type);
  };


/**
 * Connection event is an event that can have type "connectionCreated" or "connectionDestroyed".
 * These events are dispatched by the Session object when another client connects to or
 * disconnects from a {@link Session}. For the local client, the Session object dispatches a
 * "sessionConnected" or "sessionDisconnected" event, defined by the SessionConnectEvent and
 * SessionDisconnectEvent classes.
 *
 * <h5><a href="example"></a>Example</h5>
 *
 * <p>The following code keeps a running total of the number of connections to a session
 * by monitoring the <code>connections</code> property of the <code>sessionConnect</code>,
 * <code>connectionCreated</code> and <code>connectionDestroyed</code> events:</p>
 *
 * <pre>var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
 * var sessionID = ""; // Replace with your own session ID.
 *                     // See https://dashboard.tokbox.com/projects
 * var token = ""; // Replace with a generated token that has been assigned the moderator role.
 *                 // See https://dashboard.tokbox.com/projects
 * var connectionCount = 0;
 *
 * var session = OT.initSession(apiKey, sessionID);
 * session.on("sessionConnected", function(event) {
 *    connectionCount = 1; // This represent's your client's connection to the session
 *    displayConnectionCount();
 * });
 * session.on("connectionCreated", function(event) {
 *    connectionCount += 1;
 *    displayConnectionCount();
 * });
 * session.on("connectionDestroyed", function(event) {
 *    connectionCount -= 1;
 *    displayConnectionCount();
 * });
 * session.connect(token);
 *
 * function displayConnectionCount() {
 *     document.getElementById("connectionCountField").value = connectionCount.toString();
 * }</pre>
 *
 * <p>This example assumes that there is an input text field in the HTML DOM
 * with the <code>id</code> set to <code>"connectionCountField"</code>:</p>
 *
 * <pre>&lt;input type="text" id="connectionCountField" value="0"&gt;&lt;/input&gt;</pre>
 *
 *
 * @property {Connection} connection A Connection objects for the connections that was
 * created or deleted.
 *
 * @property {Array} connections Deprecated. Use the <code>connection</code> property. A
 * <code>connectionCreated</code> or <code>connectionDestroyed</code> event is dispatched
 * for each connection created and destroyed in the session.
 *
 * @property {String} reason For a <code>connectionDestroyed</code> event,
 *  a description of why the connection ended. This property can have two values:
 * </p>
 * <ul>
 *  <li><code>"clientDisconnected"</code> &#151; A client disconnected from the session by calling
 *     the <code>disconnect()</code> method of the Session object or by closing the browser.
 *     (See <a href="Session.html#disconnect">Session.disconnect()</a>.)</li>
 *
 *  <li><code>"forceDisconnected"</code> &#151; A moderator has disconnected the publisher
 *      from the session, by calling the <code>forceDisconnect()</code> method of the Session
 *      object. (See <a href="Session.html#forceDisconnect">Session.forceDisconnect()</a>.)</li>
 *
 *  <li><code>"networkDisconnected"</code> &#151; The network connection terminated abruptly
 *      (for example, the client lost their internet connection).</li>
 * </ul>
 *
 * <p>Depending on the context, this description may allow the developer to refine
 * the course of action they take in response to an event.</p>
 *
 * <p>For a <code>connectionCreated</code> event, this string is undefined.</p>
 *
 * @class ConnectionEvent
 * @augments Event
 */
  var connectionEventPluralDeprecationWarningShown = false;
  OT.ConnectionEvent = function (type, connection, reason) {
    OT.Event.call(this, type, false);

    if (OT.$.canDefineProperty) {
      Object.defineProperty(this, 'connections', {
        get: function() {
          if(!connectionEventPluralDeprecationWarningShown) {
            OT.warn('OT.ConnectionEvent connections property is deprecated, ' +
              'use connection instead.');
            connectionEventPluralDeprecationWarningShown = true;
          }
          return [connection];
        }
      });
    } else {
      this.connections = [connection];
    }
    
    this.connection = connection;
    this.reason = reason;
  };

/**
 * StreamEvent is an event that can have the type "streamCreated" or "streamDestroyed".
 * These events are dispatched by the Session object when another client starts or
 * stops publishing a stream to a {@link Session}. For a local client's stream, the
 * Publisher object dispatches the event.
 *
 * <h4><a href="example_streamCreated"></a>Example &#151; streamCreated event dispatched
 * by the Session object</h4>
 *  <p>The following code initializes a session and sets up an event listener for when
 *    a stream published by another client is created:</p>
 *
 * <pre>
 * session.on("streamCreated", function(event) {
 *   // streamContainer is a DOM element
 *   subscriber = session.subscribe(event.stream, targetElement);
 * }).connect(token);
 * </pre>
 *
 *  <h4><a href="example_streamDestroyed"></a>Example &#151; streamDestroyed event dispatched
 * by the Session object</h4>
 *
 *    <p>The following code initializes a session and sets up an event listener for when
 *       other clients' streams end:</p>
 *
 * <pre>
 * session.on("streamDestroyed", function(event) {
 *     console.log("Stream " + event.stream.name + " ended. " + event.reason);
 * }).connect(token);
 * </pre>
 *
 * <h4><a href="example_streamCreated_publisher"></a>Example &#151; streamCreated event dispatched
 * by a Publisher object</h4>
 *  <p>The following code publishes a stream and adds an event listener for when the streaming
 * starts</p>
 *
 * <pre>
 * var publisher = session.publish(targetElement)
 *   .on("streamCreated", function(event) {
 *     console.log("Publisher started streaming.");
 *   );
 * </pre>
 *
 *  <h4><a href="example_streamDestroyed_publisher"></a>Example &#151; streamDestroyed event
 * dispatched by a Publisher object</h4>
 *
 *  <p>The following code publishes a stream, and leaves the Publisher in the HTML DOM
 * when the streaming stops:</p>
 *
 * <pre>
 * var publisher = session.publish(targetElement)
 *   .on("streamDestroyed", function(event) {
 *     event.preventDefault();
 *     console.log("Publisher stopped streaming.");
 *   );
 * </pre>
 *
 * @class StreamEvent
 * @property {Stream} stream A Stream object corresponding to the stream that was added (in the
 * case of a <code>streamCreated</code> event) or deleted (in the case of a
 * <code>streamDestroyed</code> event).
 *
 * @property {Array} streams Deprecated. Use the <code>stream</code> property. A
 * <code>streamCreated</code> or <code>streamDestroyed</code> event is dispatched for
 * each stream added or destroyed.
 *
 * @property {String} reason For a <code>streamDestroyed</code> event,
 *  a description of why the session disconnected. This property can have one of the following
 *  values:
 * </p>
 * <ul>
 *  <li><code>"clientDisconnected"</code> &#151; A client disconnected from the session by calling
 *     the <code>disconnect()</code> method of the Session object or by closing the browser.
 *     (See <a href="Session.html#disconnect">Session.disconnect()</a>.)</li>
 *
 *  <li><code>"forceDisconnected"</code> &#151; A moderator has disconnected the publisher of the
 *    stream from the session, by calling the <code>forceDisconnect()</code> method of the Session
*     object. (See <a href="Session.html#forceDisconnect">Session.forceDisconnect()</a>.)</li>
 *
 *  <li><code>"forceUnpublished"</code> &#151; A moderator has forced the publisher of the stream
 *    to stop publishing the stream, by calling the <code>forceUnpublish()</code> method of the
 *    Session object. (See <a href="Session.html#forceUnpublish">Session.forceUnpublish()</a>.)</li>
 *
 *  <li><code>"networkDisconnected"</code> &#151; The network connection terminated abruptly (for
 *      example, the client lost their internet connection).</li>
 *
 * </ul>
 *
 * <p>Depending on the context, this description may allow the developer to refine
 * the course of action they take in response to an event.</p>
 *
 * <p>For a <code>streamCreated</code> event, this string is undefined.</p>
 *
 *
 * @property {Boolean} cancelable   Whether the event has a default behavior that is cancelable
 *  (<code>true</code>) or not (<code>false</code>). You can cancel the default behavior by calling
 *  the <code>preventDefault()</code> method of the StreamEvent object in the event listener
 *  function. The <code>streamDestroyed</code>
 *  event is cancelable. (See <a href="#preventDefault">preventDefault()</a>.)
 * @augments Event
 */

  var streamEventPluralDeprecationWarningShown = false;
  OT.StreamEvent = function (type, stream, reason, cancelable) {
    OT.Event.call(this, type, cancelable);

    if (OT.$.canDefineProperty) {
      Object.defineProperty(this, 'streams', {
        get: function() {
          if(!streamEventPluralDeprecationWarningShown) {
            OT.warn('OT.StreamEvent streams property is deprecated, use stream instead.');
            streamEventPluralDeprecationWarningShown = true;
          }
          return [stream];
        }
      });
    } else {
      this.streams = [stream];
    }

    this.stream = stream;
    this.reason = reason;
  };

/**
* Prevents the default behavior associated with the event from taking place.
*
* <p>For the <code>streamDestroyed</code> event dispatched by the Session object,
* the default behavior is that all Subscriber objects that are subscribed to the stream are
* unsubscribed and removed from the HTML DOM. Each Subscriber object dispatches a
* <code>destroyed</code> event when the element is removed from the HTML DOM. If you call the
* <code>preventDefault()</code> method in the event listener for the <code>streamDestroyed</code>
* event, the default behavior is prevented and you can clean up Subscriber objects using your
* own code. See
* <a href="Session.html#getSubscribersForStream">Session.getSubscribersForStream()</a>.</p>
* <p>
* For the <code>streamDestroyed</code> event dispatched by a Publisher object, the default
* behavior is that the Publisher object is removed from the HTML DOM. The Publisher object
* dispatches a <code>destroyed</code> event when the element is removed from the HTML DOM.
* If you call the <code>preventDefault()</code> method in the event listener for the
* <code>streamDestroyed</code> event, the default behavior is prevented, and you can
* retain the Publisher for reuse or clean it up using your own code.
*</p>
* <p>To see whether an event has a default behavior, check the <code>cancelable</code> property of
* the event object. </p>
*
* <p>Call the <code>preventDefault()</code> method in the event listener function for the event.</p>
*
* @method #preventDefault
* @memberof StreamEvent
*/

/**
 * The Session object dispatches SessionConnectEvent object when a session has successfully
 * connected in response to a call to the <code>connect()</code> method of the Session object.
 * <p>
 * In version 2.2, the completionHandler of the <code>Session.connect()</code> method
 * indicates success or failure in connecting to the session.
 *
 * @class SessionConnectEvent
 * @property {Array} connections Deprecated in version 2.2 (and set to an empty array). In
 * version 2.2, listen for the <code>connectionCreated</code> event dispatched by the Session
 * object. In version 2.2, the Session object dispatches a <code>connectionCreated</code> event
 * for each connection other than that of your client. This includes connections
 * present when you first connect to the session.
 *
 * @property {Array} streams Deprecated in version 2.2 (and set to an empty array). In version
 * 2.2, listen for the <code>streamCreated</code> event dispatched by the Session object. In
 * version 2.2, the Session object dispatches a <code>streamCreated</code> event for each stream
 * other than those published by your client. This includes streams
 * present when you first connect to the session.
 *
 * @see <a href="Session.html#connect">Session.connect()</a></p>
 * @augments Event
 */

  var sessionConnectedConnectionsDeprecationWarningShown = false;
  var sessionConnectedStreamsDeprecationWarningShown = false;
  var sessionConnectedArchivesDeprecationWarningShown = false;
  var sessionConnectedGroupsDeprecationWarningShown = false;

  OT.SessionConnectEvent = function (type) {
    OT.Event.call(this, type, false);
    if (OT.$.canDefineProperty) {
      Object.defineProperties(this, {
        connections: {
          get: function() {
            if(!sessionConnectedConnectionsDeprecationWarningShown) {
              OT.warn('OT.SessionConnectedEvent no longer includes connections. Listen ' +
                'for connectionCreated events instead.');
              sessionConnectedConnectionsDeprecationWarningShown = true;
            }
            return [];
          }
        },
        streams: {
          get: function() {
            if(!sessionConnectedStreamsDeprecationWarningShown) {
              OT.warn('OT.SessionConnectedEvent no longer includes streams. Listen for ' +
                'streamCreated events instead.');
              sessionConnectedConnectionsDeprecationWarningShown = true;
            }
            return [];
          }
        },
        archives: {
          get: function() {
            if(!sessionConnectedArchivesDeprecationWarningShown) {
              OT.warn('OT.SessionConnectedEvent no longer includes archives. Listen for ' +
                'archiveStarted events instead.');
              sessionConnectedArchivesDeprecationWarningShown = true;
            }
            return [];
          }
        },
        groups: {
          get: function() {
            if(!sessionConnectedGroupsDeprecationWarningShown) {
              OT.error('OT.SessionConnectedEvent no longer includes groups. There is no ' +
                'equivelant in OpenTok v2.2');
              sessionConnectedGroupsDeprecationWarningShown = true;
            }
            return [];
          }
        }
      });
    } else {
      this.connections = [];
      this.streams = [];
      this.archives = [];
      // Deprecated in OpenTok v0.91.48
      this.groups = [];
    }
  };

/**
 * The Session object dispatches SessionDisconnectEvent object when a session has disconnected. 
 * This event may be dispatched asynchronously in response to a successful call to the 
 * <code>disconnect()</code> method of the session object.
 *
 *  <h4>
 *    <a href="example"></a>Example
 *  </h4>
 *  <p>
 *    The following code initializes a session and sets up an event listener for when a session is 
 * disconnected.
 *  </p>
 * <pre>var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
 *  var sessionID = ""; // Replace with your own session ID.
 *                      // See https://dashboard.tokbox.com/projects
 *  var token = ""; // Replace with a generated token that has been assigned the moderator role.
 *                  // See https://dashboard.tokbox.com/projects
 *
 *  var session = OT.initSession(apiKey, sessionID);
 *  session.on("sessionDisconnected", function(event) {
 *      alert("The session disconnected. " + event.reason);
 *  });
 *  session.connect(token);
 *  </pre>
 *
 * @property {String} reason A description of why the session disconnected.
 *   This property can have two values:
 *  </p>
 *  <ul>
 *    <li><code>"clientDisconnected"</code> &#151; A client disconnected from the session by calling
 *     the <code>disconnect()</code> method of the Session object or by closing the browser.
 *      ( See <a href="Session.html#disconnect">Session.disconnect()</a>.)</li>
 *    <li><code>"forceDisconnected"</code> &#151; A moderator has disconnected you from the session
 *     by calling the <code>forceDisconnect()</code> method of the Session object. (See
 *       <a href="Session.html#forceDisconnect">Session.forceDisconnect()</a>.)</li>
 *    <li><code>"networkDisconnected"</code> &#151; The network connection terminated abruptly 
 *       (for example, the client lost their internet connection).</li>
 *  </ul>
 *
 * @class SessionDisconnectEvent
 * @augments Event
 */
  OT.SessionDisconnectEvent = function (type, reason, cancelable) {
    OT.Event.call(this, type, cancelable);
    this.reason = reason;
  };

/**
* Prevents the default behavior associated with the event from taking place.
*
* <p>For the <code>sessionDisconnectEvent</code>, the default behavior is that all Subscriber
* objects are unsubscribed and removed from the HTML DOM. Each Subscriber object dispatches a
* <code>destroyed</code> event when the element is removed from the HTML DOM. If you call the
* <code>preventDefault()</code> method in the event listener for the <code>sessionDisconnect</code> 
* event, the default behavior is prevented, and you can, optionally, clean up Subscriber objects 
* using your own code).
*
* <p>To see whether an event has a default behavior, check the <code>cancelable</code> property of 
* the event object. </p>
*
* <p>Call the <code>preventDefault()</code> method in the event listener function for the event.</p>
*
* @method #preventDefault
* @memberof SessionDisconnectEvent
*/

/**
 * The Session object dispatches a <code>streamPropertyChanged</code> event in the 
 * following circumstances:
 *
 * <ul>
 *
 *  <li>When a publisher starts or stops publishing audio or video. This change causes 
 *  the <code>hasAudio</code> or <code>hasVideo</code> property of the Stream object to 
 *  change. This change results from a call to the <code>publishAudio()</code> or 
 *  <code>publishVideo()</code> methods of the Publish object.</li>
 *
 *  <li>When the <code>videoDimensions</code> property of a stream changes. For more information,
 *  see <a href="Stream.html#properties">Stream.videoDimensions</a>.</li>
 *
 * </ul>
 *
 * @class StreamPropertyChangedEvent
 * @property {String} changedProperty The property of the stream that changed. This value 
 * is either <code>"hasAudio"</code>, <code>"hasVideo"</code>, or <code>"videoDimensions"</code>.
 * @property {Stream} stream The Stream object for which a property has changed.
 * @property {Object} newValue The new value of the property (after the change).
 * @property {Object} oldValue The old value of the property (before the change).
 *
 * @see <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a></p>
 * @see <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a></p>
 * @see <a href="Stream.html#properties">Stream.videoDimensions</a></p>
 * @augments Event
 */
  OT.StreamPropertyChangedEvent = function (type, stream, changedProperty, oldValue, newValue) {
    OT.Event.call(this, type, false);
    this.type = type;
    this.stream = stream;
    this.changedProperty = changedProperty;
    this.oldValue = oldValue;
    this.newValue = newValue;
  };

  OT.ArchiveEvent = function (type, archive) {
    OT.Event.call(this, type, false);
    this.type = type;
    this.id = archive.id;
    this.name = archive.name;
    this.status = archive.status;
    this.archive = archive;
  };

  OT.ArchiveUpdatedEvent = function (stream, key, oldValue, newValue) {
    OT.Event.call(this, 'updated', false);
    this.target = stream;
    this.changedProperty = key;
    this.oldValue = oldValue;
    this.newValue = newValue;
  };

/**
 * The Session object dispatches a signal event when the client receives a signal from the session.
 *
 * @class SignalEvent
 * @property {String} type The type assigned to the signal (if there is one). Use the type to 
 * filter signals received (by adding an event handler for signal:type1 or signal:type2, etc.)
 * @property {String} data The data string sent with the signal (if there is one).
 * @property {Connection} from The Connection corresponding to the client that sent with the signal.
 *
 * @see <a href="Session.html#signal">Session.signal()</a></p>
 * @see <a href="Session.html#events">Session events (signal and signal:type)</a></p>
 * @augments Event
 */
  OT.SignalEvent = function(type, data, from) {
    OT.Event.call(this, type ? 'signal:' + type : OT.Event.names.SIGNAL, false);
    this.data = data;
    this.from = from;
  };


  OT.StreamUpdatedEvent = function (stream, key, oldValue, newValue) {
    OT.Event.call(this, 'updated', false);
    this.target = stream;
    this.changedProperty = key;
    this.oldValue = oldValue;
    this.newValue = newValue;
  };

  OT.DestroyedEvent = function(type, target, reason) {
    OT.Event.call(this, type, false);
    this.target = target;
    this.reason = reason;
  };

})(window);
/* jshint ignore:start */
// https://code.google.com/p/stringencoding/
// An implementation of http://encoding.spec.whatwg.org/#api

/**
 * @license  Copyright 2014 Joshua Bell
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Original source: https://github.com/inexorabletash/text-encoding
 ***/

(function(global) {
  'use strict';

  if ( (global.TextEncoder !== void 0) && (global.TextDecoder !== void 0))  {
    // defer to the native ones
    // @todo is this a good idea?
    return;
  }

  //
  // Utilities
  //

  /**
   * @param {number} a The number to test.
   * @param {number} min The minimum value in the range, inclusive.
   * @param {number} max The maximum value in the range, inclusive.
   * @return {boolean} True if a >= min and a <= max.
   */
  function inRange(a, min, max) {
    return min <= a && a <= max;
  }

  /**
   * @param {number} n The numerator.
   * @param {number} d The denominator.
   * @return {number} The result of the integer division of n by d.
   */
  function div(n, d) {
    return Math.floor(n / d);
  }


  //
  // Implementation of Encoding specification
  // http://dvcs.w3.org/hg/encoding/raw-file/tip/Overview.html
  //

  //
  // 3. Terminology
  //

  //
  // 4. Encodings
  //

  /** @const */ var EOF_byte = -1;
  /** @const */ var EOF_code_point = -1;

  /**
   * @constructor
   * @param {Uint8Array} bytes Array of bytes that provide the stream.
   */
  function ByteInputStream(bytes) {
    /** @type {number} */
    var pos = 0;

    /** @return {number} Get the next byte from the stream. */
    this.get = function() {
      return (pos >= bytes.length) ? EOF_byte : Number(bytes[pos]);
    };

    /** @param {number} n Number (positive or negative) by which to
     *      offset the byte pointer. */
    this.offset = function(n) {
      pos += n;
      if (pos < 0) {
        throw new Error('Seeking past start of the buffer');
      }
      if (pos > bytes.length) {
        throw new Error('Seeking past EOF');
      }
    };

    /**
     * @param {Array.<number>} test Array of bytes to compare against.
     * @return {boolean} True if the start of the stream matches the test
     *     bytes.
     */
    this.match = function(test) {
      if (test.length > pos + bytes.length) {
        return false;
      }
      var i;
      for (i = 0; i < test.length; i += 1) {
        if (Number(bytes[pos + i]) !== test[i]) {
          return false;
        }
      }
      return true;
    };
  }

  /**
   * @constructor
   * @param {Array.<number>} bytes The array to write bytes into.
   */
  function ByteOutputStream(bytes) {
    /** @type {number} */
    var pos = 0;

    /**
     * @param {...number} var_args The byte or bytes to emit into the stream.
     * @return {number} The last byte emitted.
     */
    this.emit = function(var_args) {
      /** @type {number} */
      var last = EOF_byte;
      var i;
      for (i = 0; i < arguments.length; ++i) {
        last = Number(arguments[i]);
        bytes[pos++] = last;
      }
      return last;
    };
  }

  /**
   * @constructor
   * @param {string} string The source of code units for the stream.
   */
  function CodePointInputStream(string) {
    /**
     * @param {string} string Input string of UTF-16 code units.
     * @return {Array.<number>} Code points.
     */
    function stringToCodePoints(string) {
      /** @type {Array.<number>} */
      var cps = [];
      // Based on http://www.w3.org/TR/WebIDL/#idl-DOMString
      var i = 0, n = string.length;
      while (i < string.length) {
        var c = string.charCodeAt(i);
        if (!inRange(c, 0xD800, 0xDFFF)) {
          cps.push(c);
        } else if (inRange(c, 0xDC00, 0xDFFF)) {
          cps.push(0xFFFD);
        } else { // (inRange(cu, 0xD800, 0xDBFF))
          if (i === n - 1) {
            cps.push(0xFFFD);
          } else {
            var d = string.charCodeAt(i + 1);
            if (inRange(d, 0xDC00, 0xDFFF)) {
              var a = c & 0x3FF;
              var b = d & 0x3FF;
              i += 1;
              cps.push(0x10000 + (a << 10) + b);
            } else {
              cps.push(0xFFFD);
            }
          }
        }
        i += 1;
      }
      return cps;
    }

    /** @type {number} */
    var pos = 0;
    /** @type {Array.<number>} */
    var cps = stringToCodePoints(string);

    /** @param {number} n The number of bytes (positive or negative)
     *      to advance the code point pointer by.*/
    this.offset = function(n) {
      pos += n;
      if (pos < 0) {
        throw new Error('Seeking past start of the buffer');
      }
      if (pos > cps.length) {
        throw new Error('Seeking past EOF');
      }
    };


    /** @return {number} Get the next code point from the stream. */
    this.get = function() {
      if (pos >= cps.length) {
        return EOF_code_point;
      }
      return cps[pos];
    };
  }

  /**
   * @constructor
   */
  function CodePointOutputStream() {
    /** @type {string} */
    var string = '';

    /** @return {string} The accumulated string. */
    this.string = function() {
      return string;
    };

    /** @param {number} c The code point to encode into the stream. */
    this.emit = function(c) {
      if (c <= 0xFFFF) {
        string += String.fromCharCode(c);
      } else {
        c -= 0x10000;
        string += String.fromCharCode(0xD800 + ((c >> 10) & 0x3ff));
        string += String.fromCharCode(0xDC00 + (c & 0x3ff));
      }
    };
  }

  /**
   * @constructor
   * @param {string} message Description of the error.
   */
  function EncodingError(message) {
    this.name = 'EncodingError';
    this.message = message;
    this.code = 0;
  }
  EncodingError.prototype = Error.prototype;

  /**
   * @param {boolean} fatal If true, decoding errors raise an exception.
   * @param {number=} opt_code_point Override the standard fallback code point.
   * @return {number} The code point to insert on a decoding error.
   */
  function decoderError(fatal, opt_code_point) {
    if (fatal) {
      throw new EncodingError('Decoder error');
    }
    return opt_code_point || 0xFFFD;
  }

  /**
   * @param {number} code_point The code point that could not be encoded.
   */
  function encoderError(code_point) {
    throw new EncodingError('The code point ' + code_point +
                            ' could not be encoded.');
  }

  /**
   * @param {string} label The encoding label.
   * @return {?{name:string,labels:Array.<string>}}
   */
  function getEncoding(label) {
    label = String(label).trim().toLowerCase();
    if (Object.prototype.hasOwnProperty.call(label_to_encoding, label)) {
      return label_to_encoding[label];
    }
    return null;
  }

  /** @type {Array.<{encodings: Array.<{name:string,labels:Array.<string>}>,
   *      heading: string}>} */
  var encodings = [
    {
      'encodings': [
        {
          'labels': [
            'unicode-1-1-utf-8',
            'utf-8',
            'utf8'
          ],
          'name': 'utf-8'
        }
      ],
      'heading': 'The Encoding'
    },
    {
      'encodings': [
        {
          'labels': [
            'cp864',
            'ibm864'
          ],
          'name': 'ibm864'
        },
        {
          'labels': [
            'cp866',
            'ibm866'
          ],
          'name': 'ibm866'
        },
        {
          'labels': [
            'csisolatin2',
            'iso-8859-2',
            'iso-ir-101',
            'iso8859-2',
            'iso_8859-2',
            'l2',
            'latin2'
          ],
          'name': 'iso-8859-2'
        },
        {
          'labels': [
            'csisolatin3',
            'iso-8859-3',
            'iso_8859-3',
            'iso-ir-109',
            'l3',
            'latin3'
          ],
          'name': 'iso-8859-3'
        },
        {
          'labels': [
            'csisolatin4',
            'iso-8859-4',
            'iso_8859-4',
            'iso-ir-110',
            'l4',
            'latin4'
          ],
          'name': 'iso-8859-4'
        },
        {
          'labels': [
            'csisolatincyrillic',
            'cyrillic',
            'iso-8859-5',
            'iso_8859-5',
            'iso-ir-144'
          ],
          'name': 'iso-8859-5'
        },
        {
          'labels': [
            'arabic',
            'csisolatinarabic',
            'ecma-114',
            'iso-8859-6',
            'iso_8859-6',
            'iso-ir-127'
          ],
          'name': 'iso-8859-6'
        },
        {
          'labels': [
            'csisolatingreek',
            'ecma-118',
            'elot_928',
            'greek',
            'greek8',
            'iso-8859-7',
            'iso_8859-7',
            'iso-ir-126'
          ],
          'name': 'iso-8859-7'
        },
        {
          'labels': [
            'csisolatinhebrew',
            'hebrew',
            'iso-8859-8',
            'iso-8859-8-i',
            'iso-ir-138',
            'iso_8859-8',
            'visual'
          ],
          'name': 'iso-8859-8'
        },
        {
          'labels': [
            'csisolatin6',
            'iso-8859-10',
            'iso-ir-157',
            'iso8859-10',
            'l6',
            'latin6'
          ],
          'name': 'iso-8859-10'
        },
        {
          'labels': [
            'iso-8859-13'
          ],
          'name': 'iso-8859-13'
        },
        {
          'labels': [
            'iso-8859-14',
            'iso8859-14'
          ],
          'name': 'iso-8859-14'
        },
        {
          'labels': [
            'iso-8859-15',
            'iso_8859-15'
          ],
          'name': 'iso-8859-15'
        },
        {
          'labels': [
            'iso-8859-16'
          ],
          'name': 'iso-8859-16'
        },
        {
          'labels': [
            'koi8-r',
            'koi8_r'
          ],
          'name': 'koi8-r'
        },
        {
          'labels': [
            'koi8-u'
          ],
          'name': 'koi8-u'
        },
        {
          'labels': [
            'csmacintosh',
            'mac',
            'macintosh',
            'x-mac-roman'
          ],
          'name': 'macintosh'
        },
        {
          'labels': [
            'iso-8859-11',
            'tis-620',
            'windows-874'
          ],
          'name': 'windows-874'
        },
        {
          'labels': [
            'windows-1250',
            'x-cp1250'
          ],
          'name': 'windows-1250'
        },
        {
          'labels': [
            'windows-1251',
            'x-cp1251'
          ],
          'name': 'windows-1251'
        },
        {
          'labels': [
            'ascii',
            'ansi_x3.4-1968',
            'csisolatin1',
            'iso-8859-1',
            'iso8859-1',
            'iso_8859-1',
            'l1',
            'latin1',
            'us-ascii',
            'windows-1252'
          ],
          'name': 'windows-1252'
        },
        {
          'labels': [
            'cp1253',
            'windows-1253'
          ],
          'name': 'windows-1253'
        },
        {
          'labels': [
            'csisolatin5',
            'iso-8859-9',
            'iso-ir-148',
            'l5',
            'latin5',
            'windows-1254'
          ],
          'name': 'windows-1254'
        },
        {
          'labels': [
            'cp1255',
            'windows-1255'
          ],
          'name': 'windows-1255'
        },
        {
          'labels': [
            'cp1256',
            'windows-1256'
          ],
          'name': 'windows-1256'
        },
        {
          'labels': [
            'windows-1257'
          ],
          'name': 'windows-1257'
        },
        {
          'labels': [
            'cp1258',
            'windows-1258'
          ],
          'name': 'windows-1258'
        },
        {
          'labels': [
            'x-mac-cyrillic',
            'x-mac-ukrainian'
          ],
          'name': 'x-mac-cyrillic'
        }
      ],
      'heading': 'Legacy single-byte encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'chinese',
            'csgb2312',
            'csiso58gb231280',
            'gb2312',
            'gbk',
            'gb_2312',
            'gb_2312-80',
            'iso-ir-58',
            'x-gbk'
          ],
          'name': 'gbk'
        },
        {
          'labels': [
            'gb18030'
          ],
          'name': 'gb18030'
        },
        {
          'labels': [
            'hz-gb-2312'
          ],
          'name': 'hz-gb-2312'
        }
      ],
      'heading': 'Legacy multi-byte Chinese (simplified) encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'big5',
            'big5-hkscs',
            'cn-big5',
            'csbig5',
            'x-x-big5'
          ],
          'name': 'big5'
        }
      ],
      'heading': 'Legacy multi-byte Chinese (traditional) encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'cseucpkdfmtjapanese',
            'euc-jp',
            'x-euc-jp'
          ],
          'name': 'euc-jp'
        },
        {
          'labels': [
            'csiso2022jp',
            'iso-2022-jp'
          ],
          'name': 'iso-2022-jp'
        },
        {
          'labels': [
            'csshiftjis',
            'ms_kanji',
            'shift-jis',
            'shift_jis',
            'sjis',
            'windows-31j',
            'x-sjis'
          ],
          'name': 'shift_jis'
        }
      ],
      'heading': 'Legacy multi-byte Japanese encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'cseuckr',
            'csksc56011987',
            'euc-kr',
            'iso-ir-149',
            'korean',
            'ks_c_5601-1987',
            'ks_c_5601-1989',
            'ksc5601',
            'ksc_5601',
            'windows-949'
          ],
          'name': 'euc-kr'
        },
        {
          'labels': [
            'csiso2022kr',
            'iso-2022-kr'
          ],
          'name': 'iso-2022-kr'
        }
      ],
      'heading': 'Legacy multi-byte Korean encodings'
    },
    {
      'encodings': [
        {
          'labels': [
            'utf-16',
            'utf-16le'
          ],
          'name': 'utf-16'
        },
        {
          'labels': [
            'utf-16be'
          ],
          'name': 'utf-16be'
        }
      ],
      'heading': 'Legacy utf-16 encodings'
    }
  ];

  var name_to_encoding = {};
  var label_to_encoding = {};
  encodings.forEach(function(category) {
    category.encodings.forEach(function(encoding) {
      name_to_encoding[encoding.name] = encoding;
      encoding.labels.forEach(function(label) {
        label_to_encoding[label] = encoding;
      });
    });
  });

  //
  // 5. Indexes
  //

  /**
   * @param {number} pointer The |pointer| to search for.
   * @param {Array.<?number>} index The |index| to search within.
   * @return {?number} The code point corresponding to |pointer| in |index|,
   *     or null if |code point| is not in |index|.
   */
  function indexCodePointFor(pointer, index) {
    return (index || [])[pointer] || null;
  }

  /**
   * @param {number} code_point The |code point| to search for.
   * @param {Array.<?number>} index The |index| to search within.
   * @return {?number} The first pointer corresponding to |code point| in
   *     |index|, or null if |code point| is not in |index|.
   */
  function indexPointerFor(code_point, index) {
    var pointer = index.indexOf(code_point);
    return pointer === -1 ? null : pointer;
  }

  /** @type {Object.<string, (Array.<number>|Array.<Array.<number>>)>} */
  var indexes = global['encoding-indexes'] || {};

  /**
   * @param {number} pointer The |pointer| to search for in the gb18030 index.
   * @return {?number} The code point corresponding to |pointer| in |index|,
   *     or null if |code point| is not in the gb18030 index.
   */
  function indexGB18030CodePointFor(pointer) {
    if ((pointer > 39419 && pointer < 189000) || (pointer > 1237575)) {
      return null;
    }
    var /** @type {number} */ offset = 0,
        /** @type {number} */ code_point_offset = 0,
        /** @type {Array.<Array.<number>>} */ index = indexes['gb18030'];
    var i;
    for (i = 0; i < index.length; ++i) {
      var entry = index[i];
      if (entry[0] <= pointer) {
        offset = entry[0];
        code_point_offset = entry[1];
      } else {
        break;
      }
    }
    return code_point_offset + pointer - offset;
  }

  /**
   * @param {number} code_point The |code point| to locate in the gb18030 index.
   * @return {number} The first pointer corresponding to |code point| in the
   *     gb18030 index.
   */
  function indexGB18030PointerFor(code_point) {
    var /** @type {number} */ offset = 0,
        /** @type {number} */ pointer_offset = 0,
        /** @type {Array.<Array.<number>>} */ index = indexes['gb18030'];
    var i;
    for (i = 0; i < index.length; ++i) {
      var entry = index[i];
      if (entry[1] <= code_point) {
        offset = entry[1];
        pointer_offset = entry[0];
      } else {
        break;
      }
    }
    return pointer_offset + code_point - offset;
  }

  //
  // 7. The encoding
  //

  // 7.1 utf-8

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function UTF8Decoder(options) {
    var fatal = options.fatal;
    var /** @type {number} */ utf8_code_point = 0,
        /** @type {number} */ utf8_bytes_needed = 0,
        /** @type {number} */ utf8_bytes_seen = 0,
        /** @type {number} */ utf8_lower_boundary = 0;

    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte) {
        if (utf8_bytes_needed !== 0) {
          return decoderError(fatal);
        }
        return EOF_code_point;
      }
      byte_pointer.offset(1);

      if (utf8_bytes_needed === 0) {
        if (inRange(bite, 0x00, 0x7F)) {
          return bite;
        }
        if (inRange(bite, 0xC2, 0xDF)) {
          utf8_bytes_needed = 1;
          utf8_lower_boundary = 0x80;
          utf8_code_point = bite - 0xC0;
        } else if (inRange(bite, 0xE0, 0xEF)) {
          utf8_bytes_needed = 2;
          utf8_lower_boundary = 0x800;
          utf8_code_point = bite - 0xE0;
        } else if (inRange(bite, 0xF0, 0xF4)) {
          utf8_bytes_needed = 3;
          utf8_lower_boundary = 0x10000;
          utf8_code_point = bite - 0xF0;
        } else {
          return decoderError(fatal);
        }
        utf8_code_point = utf8_code_point * Math.pow(64, utf8_bytes_needed);
        return null;
      }
      if (!inRange(bite, 0x80, 0xBF)) {
        utf8_code_point = 0;
        utf8_bytes_needed = 0;
        utf8_bytes_seen = 0;
        utf8_lower_boundary = 0;
        byte_pointer.offset(-1);
        return decoderError(fatal);
      }
      utf8_bytes_seen += 1;
      utf8_code_point = utf8_code_point + (bite - 0x80) *
          Math.pow(64, utf8_bytes_needed - utf8_bytes_seen);
      if (utf8_bytes_seen !== utf8_bytes_needed) {
        return null;
      }
      var code_point = utf8_code_point;
      var lower_boundary = utf8_lower_boundary;
      utf8_code_point = 0;
      utf8_bytes_needed = 0;
      utf8_bytes_seen = 0;
      utf8_lower_boundary = 0;
      if (inRange(code_point, lower_boundary, 0x10FFFF) &&
          !inRange(code_point, 0xD800, 0xDFFF)) {
        return code_point;
      }
      return decoderError(fatal);
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function UTF8Encoder(options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0xD800, 0xDFFF)) {
        return encoderError(code_point);
      }
      if (inRange(code_point, 0x0000, 0x007f)) {
        return output_byte_stream.emit(code_point);
      }
      var count, offset;
      if (inRange(code_point, 0x0080, 0x07FF)) {
        count = 1;
        offset = 0xC0;
      } else if (inRange(code_point, 0x0800, 0xFFFF)) {
        count = 2;
        offset = 0xE0;
      } else if (inRange(code_point, 0x10000, 0x10FFFF)) {
        count = 3;
        offset = 0xF0;
      }
      var result = output_byte_stream.emit(
          div(code_point, Math.pow(64, count)) + offset);
      while (count > 0) {
        var temp = div(code_point, Math.pow(64, count - 1));
        result = output_byte_stream.emit(0x80 + (temp % 64));
        count -= 1;
      }
      return result;
    };
  }

  name_to_encoding['utf-8'].getEncoder = function(options) {
    return new UTF8Encoder(options);
  };
  name_to_encoding['utf-8'].getDecoder = function(options) {
    return new UTF8Decoder(options);
  };

  //
  // 8. Legacy single-byte encodings
  //

  /**
   * @constructor
   * @param {Array.<number>} index The encoding index.
   * @param {{fatal: boolean}} options
   */
  function SingleByteDecoder(index, options) {
    var fatal = options.fatal;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte) {
        return EOF_code_point;
      }
      byte_pointer.offset(1);
      if (inRange(bite, 0x00, 0x7F)) {
        return bite;
      }
      var code_point = index[bite - 0x80];
      if (code_point === null) {
        return decoderError(fatal);
      }
      return code_point;
    };
  }

  /**
   * @constructor
   * @param {Array.<?number>} index The encoding index.
   * @param {{fatal: boolean}} options
   */
  function SingleByteEncoder(index, options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0x0000, 0x007F)) {
        return output_byte_stream.emit(code_point);
      }
      var pointer = indexPointerFor(code_point, index);
      if (pointer === null) {
        encoderError(code_point);
      }
      return output_byte_stream.emit(pointer + 0x80);
    };
  }

  (function() {
    ['ibm864', 'ibm866', 'iso-8859-2', 'iso-8859-3', 'iso-8859-4',
     'iso-8859-5', 'iso-8859-6', 'iso-8859-7', 'iso-8859-8', 'iso-8859-10',
     'iso-8859-13', 'iso-8859-14', 'iso-8859-15', 'iso-8859-16', 'koi8-r',
     'koi8-u', 'macintosh', 'windows-874', 'windows-1250', 'windows-1251',
     'windows-1252', 'windows-1253', 'windows-1254', 'windows-1255',
     'windows-1256', 'windows-1257', 'windows-1258', 'x-mac-cyrillic'
    ].forEach(function(name) {
      var encoding = name_to_encoding[name];
      var index = indexes[name];
      encoding.getDecoder = function(options) {
        return new SingleByteDecoder(index, options);
      };
      encoding.getEncoder = function(options) {
        return new SingleByteEncoder(index, options);
      };
    });
  }());

  //
  // 9. Legacy multi-byte Chinese (simplified) encodings
  //

  // 9.1 gbk

  /**
   * @constructor
   * @param {boolean} gb18030 True if decoding gb18030, false otherwise.
   * @param {{fatal: boolean}} options
   */
  function GBKDecoder(gb18030, options) {
    var fatal = options.fatal;
    var /** @type {number} */ gbk_first = 0x00,
        /** @type {number} */ gbk_second = 0x00,
        /** @type {number} */ gbk_third = 0x00;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte && gbk_first === 0x00 &&
          gbk_second === 0x00 && gbk_third === 0x00) {
        return EOF_code_point;
      }
      if (bite === EOF_byte &&
          (gbk_first !== 0x00 || gbk_second !== 0x00 || gbk_third !== 0x00)) {
        gbk_first = 0x00;
        gbk_second = 0x00;
        gbk_third = 0x00;
        decoderError(fatal);
      }
      byte_pointer.offset(1);
      var code_point;
      if (gbk_third !== 0x00) {
        code_point = null;
        if (inRange(bite, 0x30, 0x39)) {
          code_point = indexGB18030CodePointFor(
              (((gbk_first - 0x81) * 10 + (gbk_second - 0x30)) * 126 +
               (gbk_third - 0x81)) * 10 + bite - 0x30);
        }
        gbk_first = 0x00;
        gbk_second = 0x00;
        gbk_third = 0x00;
        if (code_point === null) {
          byte_pointer.offset(-3);
          return decoderError(fatal);
        }
        return code_point;
      }
      if (gbk_second !== 0x00) {
        if (inRange(bite, 0x81, 0xFE)) {
          gbk_third = bite;
          return null;
        }
        byte_pointer.offset(-2);
        gbk_first = 0x00;
        gbk_second = 0x00;
        return decoderError(fatal);
      }
      if (gbk_first !== 0x00) {
        if (inRange(bite, 0x30, 0x39) && gb18030) {
          gbk_second = bite;
          return null;
        }
        var lead = gbk_first;
        var pointer = null;
        gbk_first = 0x00;
        var offset = bite < 0x7F ? 0x40 : 0x41;
        if (inRange(bite, 0x40, 0x7E) || inRange(bite, 0x80, 0xFE)) {
          pointer = (lead - 0x81) * 190 + (bite - offset);
        }
        code_point = pointer === null ? null :
            indexCodePointFor(pointer, indexes['gbk']);
        if (pointer === null) {
          byte_pointer.offset(-1);
        }
        if (code_point === null) {
          return decoderError(fatal);
        }
        return code_point;
      }
      if (inRange(bite, 0x00, 0x7F)) {
        return bite;
      }
      if (bite === 0x80) {
        return 0x20AC;
      }
      if (inRange(bite, 0x81, 0xFE)) {
        gbk_first = bite;
        return null;
      }
      return decoderError(fatal);
    };
  }

  /**
   * @constructor
   * @param {boolean} gb18030 True if decoding gb18030, false otherwise.
   * @param {{fatal: boolean}} options
   */
  function GBKEncoder(gb18030, options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0x0000, 0x007F)) {
        return output_byte_stream.emit(code_point);
      }
      var pointer = indexPointerFor(code_point, indexes['gbk']);
      if (pointer !== null) {
        var lead = div(pointer, 190) + 0x81;
        var trail = pointer % 190;
        var offset = trail < 0x3F ? 0x40 : 0x41;
        return output_byte_stream.emit(lead, trail + offset);
      }
      if (pointer === null && !gb18030) {
        return encoderError(code_point);
      }
      pointer = indexGB18030PointerFor(code_point);
      var byte1 = div(div(div(pointer, 10), 126), 10);
      pointer = pointer - byte1 * 10 * 126 * 10;
      var byte2 = div(div(pointer, 10), 126);
      pointer = pointer - byte2 * 10 * 126;
      var byte3 = div(pointer, 10);
      var byte4 = pointer - byte3 * 10;
      return output_byte_stream.emit(byte1 + 0x81,
                                     byte2 + 0x30,
                                     byte3 + 0x81,
                                     byte4 + 0x30);
    };
  }

  name_to_encoding['gbk'].getEncoder = function(options) {
    return new GBKEncoder(false, options);
  };
  name_to_encoding['gbk'].getDecoder = function(options) {
    return new GBKDecoder(false, options);
  };

  // 9.2 gb18030
  name_to_encoding['gb18030'].getEncoder = function(options) {
    return new GBKEncoder(true, options);
  };
  name_to_encoding['gb18030'].getDecoder = function(options) {
    return new GBKDecoder(true, options);
  };

  // 9.3 hz-gb-2312

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function HZGB2312Decoder(options) {
    var fatal = options.fatal;
    var /** @type {boolean} */ hzgb2312 = false,
        /** @type {number} */ hzgb2312_lead = 0x00;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte && hzgb2312_lead === 0x00) {
        return EOF_code_point;
      }
      if (bite === EOF_byte && hzgb2312_lead !== 0x00) {
        hzgb2312_lead = 0x00;
        return decoderError(fatal);
      }
      byte_pointer.offset(1);
      if (hzgb2312_lead === 0x7E) {
        hzgb2312_lead = 0x00;
        if (bite === 0x7B) {
          hzgb2312 = true;
          return null;
        }
        if (bite === 0x7D) {
          hzgb2312 = false;
          return null;
        }
        if (bite === 0x7E) {
          return 0x007E;
        }
        if (bite === 0x0A) {
          return null;
        }
        byte_pointer.offset(-1);
        return decoderError(fatal);
      }
      if (hzgb2312_lead !== 0x00) {
        var lead = hzgb2312_lead;
        hzgb2312_lead = 0x00;
        var code_point = null;
        if (inRange(bite, 0x21, 0x7E)) {
          code_point = indexCodePointFor((lead - 1) * 190 +
                                         (bite + 0x3F), indexes['gbk']);
        }
        if (bite === 0x0A) {
          hzgb2312 = false;
        }
        if (code_point === null) {
          return decoderError(fatal);
        }
        return code_point;
      }
      if (bite === 0x7E) {
        hzgb2312_lead = 0x7E;
        return null;
      }
      if (hzgb2312) {
        if (inRange(bite, 0x20, 0x7F)) {
          hzgb2312_lead = bite;
          return null;
        }
        if (bite === 0x0A) {
          hzgb2312 = false;
        }
        return decoderError(fatal);
      }
      if (inRange(bite, 0x00, 0x7F)) {
        return bite;
      }
      return decoderError(fatal);
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function HZGB2312Encoder(options) {
    var fatal = options.fatal;
    var hzgb2312 = false;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0x0000, 0x007F) && hzgb2312) {
        code_point_pointer.offset(-1);
        hzgb2312 = false;
        return output_byte_stream.emit(0x7E, 0x7D);
      }
      if (code_point === 0x007E) {
        return output_byte_stream.emit(0x7E, 0x7E);
      }
      if (inRange(code_point, 0x0000, 0x007F)) {
        return output_byte_stream.emit(code_point);
      }
      if (!hzgb2312) {
        code_point_pointer.offset(-1);
        hzgb2312 = true;
        return output_byte_stream.emit(0x7E, 0x7B);
      }
      var pointer = indexPointerFor(code_point, indexes['gbk']);
      if (pointer === null) {
        return encoderError(code_point);
      }
      var lead = div(pointer, 190) + 1;
      var trail = pointer % 190 - 0x3F;
      if (!inRange(lead, 0x21, 0x7E) || !inRange(trail, 0x21, 0x7E)) {
        return encoderError(code_point);
      }
      return output_byte_stream.emit(lead, trail);
    };
  }

  name_to_encoding['hz-gb-2312'].getEncoder = function(options) {
    return new HZGB2312Encoder(options);
  };
  name_to_encoding['hz-gb-2312'].getDecoder = function(options) {
    return new HZGB2312Decoder(options);
  };

  //
  // 10. Legacy multi-byte Chinese (traditional) encodings
  //

  // 10.1 big5

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function Big5Decoder(options) {
    var fatal = options.fatal;
    var /** @type {number} */ big5_lead = 0x00,
        /** @type {?number} */ big5_pending = null;

    /**
     * @param {ByteInputStream} byte_pointer The byte steram to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      // NOTE: Hack to support emitting two code points
      if (big5_pending !== null) {
        var pending = big5_pending;
        big5_pending = null;
        return pending;
      }
      var bite = byte_pointer.get();
      if (bite === EOF_byte && big5_lead === 0x00) {
        return EOF_code_point;
      }
      if (bite === EOF_byte && big5_lead !== 0x00) {
        big5_lead = 0x00;
        return decoderError(fatal);
      }
      byte_pointer.offset(1);
      if (big5_lead !== 0x00) {
        var lead = big5_lead;
        var pointer = null;
        big5_lead = 0x00;
        var offset = bite < 0x7F ? 0x40 : 0x62;
        if (inRange(bite, 0x40, 0x7E) || inRange(bite, 0xA1, 0xFE)) {
          pointer = (lead - 0x81) * 157 + (bite - offset);
        }
        if (pointer === 1133) {
          big5_pending = 0x0304;
          return 0x00CA;
        }
        if (pointer === 1135) {
          big5_pending = 0x030C;
          return 0x00CA;
        }
        if (pointer === 1164) {
          big5_pending = 0x0304;
          return 0x00EA;
        }
        if (pointer === 1166) {
          big5_pending = 0x030C;
          return 0x00EA;
        }
        var code_point = (pointer === null) ? null :
            indexCodePointFor(pointer, indexes['big5']);
        if (pointer === null) {
          byte_pointer.offset(-1);
        }
        if (code_point === null) {
          return decoderError(fatal);
        }
        return code_point;
      }
      if (inRange(bite, 0x00, 0x7F)) {
        return bite;
      }
      if (inRange(bite, 0x81, 0xFE)) {
        big5_lead = bite;
        return null;
      }
      return decoderError(fatal);
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function Big5Encoder(options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0x0000, 0x007F)) {
        return output_byte_stream.emit(code_point);
      }
      var pointer = indexPointerFor(code_point, indexes['big5']);
      if (pointer === null) {
        return encoderError(code_point);
      }
      var lead = div(pointer, 157) + 0x81;
      //if (lead < 0xA1) {
      //  return encoderError(code_point);
      //}
      var trail = pointer % 157;
      var offset = trail < 0x3F ? 0x40 : 0x62;
      return output_byte_stream.emit(lead, trail + offset);
    };
  }

  name_to_encoding['big5'].getEncoder = function(options) {
    return new Big5Encoder(options);
  };
  name_to_encoding['big5'].getDecoder = function(options) {
    return new Big5Decoder(options);
  };


  //
  // 11. Legacy multi-byte Japanese encodings
  //

  // 11.1 euc.jp

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function EUCJPDecoder(options) {
    var fatal = options.fatal;
    var /** @type {number} */ eucjp_first = 0x00,
        /** @type {number} */ eucjp_second = 0x00;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte) {
        if (eucjp_first === 0x00 && eucjp_second === 0x00) {
          return EOF_code_point;
        }
        eucjp_first = 0x00;
        eucjp_second = 0x00;
        return decoderError(fatal);
      }
      byte_pointer.offset(1);

      var lead, code_point;
      if (eucjp_second !== 0x00) {
        lead = eucjp_second;
        eucjp_second = 0x00;
        code_point = null;
        if (inRange(lead, 0xA1, 0xFE) && inRange(bite, 0xA1, 0xFE)) {
          code_point = indexCodePointFor((lead - 0xA1) * 94 + bite - 0xA1,
                                         indexes['jis0212']);
        }
        if (!inRange(bite, 0xA1, 0xFE)) {
          byte_pointer.offset(-1);
        }
        if (code_point === null) {
          return decoderError(fatal);
        }
        return code_point;
      }
      if (eucjp_first === 0x8E && inRange(bite, 0xA1, 0xDF)) {
        eucjp_first = 0x00;
        return 0xFF61 + bite - 0xA1;
      }
      if (eucjp_first === 0x8F && inRange(bite, 0xA1, 0xFE)) {
        eucjp_first = 0x00;
        eucjp_second = bite;
        return null;
      }
      if (eucjp_first !== 0x00) {
        lead = eucjp_first;
        eucjp_first = 0x00;
        code_point = null;
        if (inRange(lead, 0xA1, 0xFE) && inRange(bite, 0xA1, 0xFE)) {
          code_point = indexCodePointFor((lead - 0xA1) * 94 + bite - 0xA1,
                                         indexes['jis0208']);
        }
        if (!inRange(bite, 0xA1, 0xFE)) {
          byte_pointer.offset(-1);
        }
        if (code_point === null) {
          return decoderError(fatal);
        }
        return code_point;
      }
      if (inRange(bite, 0x00, 0x7F)) {
        return bite;
      }
      if (bite === 0x8E || bite === 0x8F || (inRange(bite, 0xA1, 0xFE))) {
        eucjp_first = bite;
        return null;
      }
      return decoderError(fatal);
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function EUCJPEncoder(options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0x0000, 0x007F)) {
        return output_byte_stream.emit(code_point);
      }
      if (code_point === 0x00A5) {
        return output_byte_stream.emit(0x5C);
      }
      if (code_point === 0x203E) {
        return output_byte_stream.emit(0x7E);
      }
      if (inRange(code_point, 0xFF61, 0xFF9F)) {
        return output_byte_stream.emit(0x8E, code_point - 0xFF61 + 0xA1);
      }

      var pointer = indexPointerFor(code_point, indexes['jis0208']);
      if (pointer === null) {
        return encoderError(code_point);
      }
      var lead = div(pointer, 94) + 0xA1;
      var trail = pointer % 94 + 0xA1;
      return output_byte_stream.emit(lead, trail);
    };
  }

  name_to_encoding['euc-jp'].getEncoder = function(options) {
    return new EUCJPEncoder(options);
  };
  name_to_encoding['euc-jp'].getDecoder = function(options) {
    return new EUCJPDecoder(options);
  };

  // 11.2 iso-2022-jp

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function ISO2022JPDecoder(options) {
    var fatal = options.fatal;
    /** @enum */
    var state = {
      ASCII: 0,
      escape_start: 1,
      escape_middle: 2,
      escape_final: 3,
      lead: 4,
      trail: 5,
      Katakana: 6
    };
    var /** @type {number} */ iso2022jp_state = state.ASCII,
        /** @type {boolean} */ iso2022jp_jis0212 = false,
        /** @type {number} */ iso2022jp_lead = 0x00;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite !== EOF_byte) {
        byte_pointer.offset(1);
      }
      switch (iso2022jp_state) {
        default:
        case state.ASCII:
          if (bite === 0x1B) {
            iso2022jp_state = state.escape_start;
            return null;
          }
          if (inRange(bite, 0x00, 0x7F)) {
            return bite;
          }
          if (bite === EOF_byte) {
            return EOF_code_point;
          }
          return decoderError(fatal);

        case state.escape_start:
          if (bite === 0x24 || bite === 0x28) {
            iso2022jp_lead = bite;
            iso2022jp_state = state.escape_middle;
            return null;
          }
          if (bite !== EOF_byte) {
            byte_pointer.offset(-1);
          }
          iso2022jp_state = state.ASCII;
          return decoderError(fatal);

        case state.escape_middle:
          var lead = iso2022jp_lead;
          iso2022jp_lead = 0x00;
          if (lead === 0x24 && (bite === 0x40 || bite === 0x42)) {
            iso2022jp_jis0212 = false;
            iso2022jp_state = state.lead;
            return null;
          }
          if (lead === 0x24 && bite === 0x28) {
            iso2022jp_state = state.escape_final;
            return null;
          }
          if (lead === 0x28 && (bite === 0x42 || bite === 0x4A)) {
            iso2022jp_state = state.ASCII;
            return null;
          }
          if (lead === 0x28 && bite === 0x49) {
            iso2022jp_state = state.Katakana;
            return null;
          }
          if (bite === EOF_byte) {
            byte_pointer.offset(-1);
          } else {
            byte_pointer.offset(-2);
          }
          iso2022jp_state = state.ASCII;
          return decoderError(fatal);

        case state.escape_final:
          if (bite === 0x44) {
            iso2022jp_jis0212 = true;
            iso2022jp_state = state.lead;
            return null;
          }
          if (bite === EOF_byte) {
            byte_pointer.offset(-2);
          } else {
            byte_pointer.offset(-3);
          }
          iso2022jp_state = state.ASCII;
          return decoderError(fatal);

        case state.lead:
          if (bite === 0x0A) {
            iso2022jp_state = state.ASCII;
            return decoderError(fatal, 0x000A);
          }
          if (bite === 0x1B) {
            iso2022jp_state = state.escape_start;
            return null;
          }
          if (bite === EOF_byte) {
            return EOF_code_point;
          }
          iso2022jp_lead = bite;
          iso2022jp_state = state.trail;
          return null;

        case state.trail:
          iso2022jp_state = state.lead;
          if (bite === EOF_byte) {
            return decoderError(fatal);
          }
          var code_point = null;
          var pointer = (iso2022jp_lead - 0x21) * 94 + bite - 0x21;
          if (inRange(iso2022jp_lead, 0x21, 0x7E) &&
              inRange(bite, 0x21, 0x7E)) {
            code_point = (iso2022jp_jis0212 === false) ?
                indexCodePointFor(pointer, indexes['jis0208']) :
                indexCodePointFor(pointer, indexes['jis0212']);
          }
          if (code_point === null) {
            return decoderError(fatal);
          }
          return code_point;

        case state.Katakana:
          if (bite === 0x1B) {
            iso2022jp_state = state.escape_start;
            return null;
          }
          if (inRange(bite, 0x21, 0x5F)) {
            return 0xFF61 + bite - 0x21;
          }
          if (bite === EOF_byte) {
            return EOF_code_point;
          }
          return decoderError(fatal);
      }
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function ISO2022JPEncoder(options) {
    var fatal = options.fatal;
    /** @enum */
    var state = {
      ASCII: 0,
      lead: 1,
      Katakana: 2
    };
    var /** @type {number} */ iso2022jp_state = state.ASCII;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if ((inRange(code_point, 0x0000, 0x007F) ||
           code_point === 0x00A5 || code_point === 0x203E) &&
          iso2022jp_state !== state.ASCII) {
        code_point_pointer.offset(-1);
        iso2022jp_state = state.ASCII;
        return output_byte_stream.emit(0x1B, 0x28, 0x42);
      }
      if (inRange(code_point, 0x0000, 0x007F)) {
        return output_byte_stream.emit(code_point);
      }
      if (code_point === 0x00A5) {
        return output_byte_stream.emit(0x5C);
      }
      if (code_point === 0x203E) {
        return output_byte_stream.emit(0x7E);
      }
      if (inRange(code_point, 0xFF61, 0xFF9F) &&
          iso2022jp_state !== state.Katakana) {
        code_point_pointer.offset(-1);
        iso2022jp_state = state.Katakana;
        return output_byte_stream.emit(0x1B, 0x28, 0x49);
      }
      if (inRange(code_point, 0xFF61, 0xFF9F)) {
        return output_byte_stream.emit(code_point - 0xFF61 - 0x21);
      }
      if (iso2022jp_state !== state.lead) {
        code_point_pointer.offset(-1);
        iso2022jp_state = state.lead;
        return output_byte_stream.emit(0x1B, 0x24, 0x42);
      }
      var pointer = indexPointerFor(code_point, indexes['jis0208']);
      if (pointer === null) {
        return encoderError(code_point);
      }
      var lead = div(pointer, 94) + 0x21;
      var trail = pointer % 94 + 0x21;
      return output_byte_stream.emit(lead, trail);
    };
  }

  name_to_encoding['iso-2022-jp'].getEncoder = function(options) {
    return new ISO2022JPEncoder(options);
  };
  name_to_encoding['iso-2022-jp'].getDecoder = function(options) {
    return new ISO2022JPDecoder(options);
  };

  // 11.3 shift_jis

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function ShiftJISDecoder(options) {
    var fatal = options.fatal;
    var /** @type {number} */ shiftjis_lead = 0x00;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte && shiftjis_lead === 0x00) {
        return EOF_code_point;
      }
      if (bite === EOF_byte && shiftjis_lead !== 0x00) {
        shiftjis_lead = 0x00;
        return decoderError(fatal);
      }
      byte_pointer.offset(1);
      if (shiftjis_lead !== 0x00) {
        var lead = shiftjis_lead;
        shiftjis_lead = 0x00;
        if (inRange(bite, 0x40, 0x7E) || inRange(bite, 0x80, 0xFC)) {
          var offset = (bite < 0x7F) ? 0x40 : 0x41;
          var lead_offset = (lead < 0xA0) ? 0x81 : 0xC1;
          var code_point = indexCodePointFor((lead - lead_offset) * 188 +
                                             bite - offset, indexes['jis0208']);
          if (code_point === null) {
            return decoderError(fatal);
          }
          return code_point;
        }
        byte_pointer.offset(-1);
        return decoderError(fatal);
      }
      if (inRange(bite, 0x00, 0x80)) {
        return bite;
      }
      if (inRange(bite, 0xA1, 0xDF)) {
        return 0xFF61 + bite - 0xA1;
      }
      if (inRange(bite, 0x81, 0x9F) || inRange(bite, 0xE0, 0xFC)) {
        shiftjis_lead = bite;
        return null;
      }
      return decoderError(fatal);
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function ShiftJISEncoder(options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0x0000, 0x0080)) {
        return output_byte_stream.emit(code_point);
      }
      if (code_point === 0x00A5) {
        return output_byte_stream.emit(0x5C);
      }
      if (code_point === 0x203E) {
        return output_byte_stream.emit(0x7E);
      }
      if (inRange(code_point, 0xFF61, 0xFF9F)) {
        return output_byte_stream.emit(code_point - 0xFF61 + 0xA1);
      }
      var pointer = indexPointerFor(code_point, indexes['jis0208']);
      if (pointer === null) {
        return encoderError(code_point);
      }
      var lead = div(pointer, 188);
      var lead_offset = lead < 0x1F ? 0x81 : 0xC1;
      var trail = pointer % 188;
      var offset = trail < 0x3F ? 0x40 : 0x41;
      return output_byte_stream.emit(lead + lead_offset, trail + offset);
    };
  }

  name_to_encoding['shift_jis'].getEncoder = function(options) {
    return new ShiftJISEncoder(options);
  };
  name_to_encoding['shift_jis'].getDecoder = function(options) {
    return new ShiftJISDecoder(options);
  };

  //
  // 12. Legacy multi-byte Korean encodings
  //

  // 12.1 euc-kr

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function EUCKRDecoder(options) {
    var fatal = options.fatal;
    var /** @type {number} */ euckr_lead = 0x00;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte && euckr_lead === 0) {
        return EOF_code_point;
      }
      if (bite === EOF_byte && euckr_lead !== 0) {
        euckr_lead = 0x00;
        return decoderError(fatal);
      }
      byte_pointer.offset(1);
      if (euckr_lead !== 0x00) {
        var lead = euckr_lead;
        var pointer = null;
        euckr_lead = 0x00;

        if (inRange(lead, 0x81, 0xC6)) {
          var temp = (26 + 26 + 126) * (lead - 0x81);
          if (inRange(bite, 0x41, 0x5A)) {
            pointer = temp + bite - 0x41;
          } else if (inRange(bite, 0x61, 0x7A)) {
            pointer = temp + 26 + bite - 0x61;
          } else if (inRange(bite, 0x81, 0xFE)) {
            pointer = temp + 26 + 26 + bite - 0x81;
          }
        }

        if (inRange(lead, 0xC7, 0xFD) && inRange(bite, 0xA1, 0xFE)) {
          pointer = (26 + 26 + 126) * (0xC7 - 0x81) + (lead - 0xC7) * 94 +
              (bite - 0xA1);
        }

        var code_point = (pointer === null) ? null :
            indexCodePointFor(pointer, indexes['euc-kr']);
        if (pointer === null) {
          byte_pointer.offset(-1);
        }
        if (code_point === null) {
          return decoderError(fatal);
        }
        return code_point;
      }

      if (inRange(bite, 0x00, 0x7F)) {
        return bite;
      }

      if (inRange(bite, 0x81, 0xFD)) {
        euckr_lead = bite;
        return null;
      }

      return decoderError(fatal);
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function EUCKREncoder(options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0x0000, 0x007F)) {
        return output_byte_stream.emit(code_point);
      }
      var pointer = indexPointerFor(code_point, indexes['euc-kr']);
      if (pointer === null) {
        return encoderError(code_point);
      }
      var lead, trail;
      if (pointer < ((26 + 26 + 126) * (0xC7 - 0x81))) {
        lead = div(pointer, (26 + 26 + 126)) + 0x81;
        trail = pointer % (26 + 26 + 126);
        var offset = trail < 26 ? 0x41 : trail < 26 + 26 ? 0x47 : 0x4D;
        return output_byte_stream.emit(lead, trail + offset);
      }
      pointer = pointer - (26 + 26 + 126) * (0xC7 - 0x81);
      lead = div(pointer, 94) + 0xC7;
      trail = pointer % 94 + 0xA1;
      return output_byte_stream.emit(lead, trail);
    };
  }

  name_to_encoding['euc-kr'].getEncoder = function(options) {
    return new EUCKREncoder(options);
  };
  name_to_encoding['euc-kr'].getDecoder = function(options) {
    return new EUCKRDecoder(options);
  };

  // 12.2 iso-2022-kr

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function ISO2022KRDecoder(options) {
    var fatal = options.fatal;
    /** @enum */
    var state = {
      ASCII: 0,
      escape_start: 1,
      escape_middle: 2,
      escape_end: 3,
      lead: 4,
      trail: 5
    };
    var /** @type {number} */ iso2022kr_state = state.ASCII,
        /** @type {number} */ iso2022kr_lead = 0x00;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite !== EOF_byte) {
        byte_pointer.offset(1);
      }
      switch (iso2022kr_state) {
        default:
        case state.ASCII:
          if (bite === 0x0E) {
            iso2022kr_state = state.lead;
            return null;
          }
          if (bite === 0x0F) {
            return null;
          }
          if (bite === 0x1B) {
            iso2022kr_state = state.escape_start;
            return null;
          }
          if (inRange(bite, 0x00, 0x7F)) {
            return bite;
          }
          if (bite === EOF_byte) {
            return EOF_code_point;
          }
          return decoderError(fatal);
        case state.escape_start:
          if (bite === 0x24) {
            iso2022kr_state = state.escape_middle;
            return null;
          }
          if (bite !== EOF_byte) {
            byte_pointer.offset(-1);
          }
          iso2022kr_state = state.ASCII;
          return decoderError(fatal);
        case state.escape_middle:
          if (bite === 0x29) {
            iso2022kr_state = state.escape_end;
            return null;
          }
          if (bite === EOF_byte) {
            byte_pointer.offset(-1);
          } else {
            byte_pointer.offset(-2);
          }
          iso2022kr_state = state.ASCII;
          return decoderError(fatal);
        case state.escape_end:
          if (bite === 0x43) {
            iso2022kr_state = state.ASCII;
            return null;
          }
          if (bite === EOF_byte) {
            byte_pointer.offset(-2);
          } else {
            byte_pointer.offset(-3);
          }
          iso2022kr_state = state.ASCII;
          return decoderError(fatal);
        case state.lead:
          if (bite === 0x0A) {
            iso2022kr_state = state.ASCII;
            return decoderError(fatal, 0x000A);
          }
          if (bite === 0x0E) {
            return null;
          }
          if (bite === 0x0F) {
            iso2022kr_state = state.ASCII;
            return null;
          }
          if (bite === EOF_byte) {
            return EOF_code_point;
          }
          iso2022kr_lead = bite;
          iso2022kr_state = state.trail;
          return null;
        case state.trail:
          iso2022kr_state = state.lead;
          if (bite === EOF_byte) {
            return decoderError(fatal);
          }
          var code_point = null;
          if (inRange(iso2022kr_lead, 0x21, 0x46) &&
              inRange(bite, 0x21, 0x7E)) {
            code_point = indexCodePointFor((26 + 26 + 126) *
                (iso2022kr_lead - 1) +
                26 + 26 + bite - 1,
                indexes['euc-kr']);
          } else if (inRange(iso2022kr_lead, 0x47, 0x7E) &&
              inRange(bite, 0x21, 0x7E)) {
            code_point = indexCodePointFor((26 + 26 + 126) * (0xC7 - 0x81) +
                (iso2022kr_lead - 0x47) * 94 +
                (bite - 0x21),
                indexes['euc-kr']);
          }
          if (code_point !== null) {
            return code_point;
          }
          return decoderError(fatal);
      }
    };
  }

  /**
   * @constructor
   * @param {{fatal: boolean}} options
   */
  function ISO2022KREncoder(options) {
    var fatal = options.fatal;
    /** @enum */
    var state = {
      ASCII: 0,
      lead: 1
    };
    var /** @type {boolean} */ iso2022kr_initialization = false,
        /** @type {number} */ iso2022kr_state = state.ASCII;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      if (!iso2022kr_initialization) {
        iso2022kr_initialization = true;
        output_byte_stream.emit(0x1B, 0x24, 0x29, 0x43);
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0x0000, 0x007F) &&
          iso2022kr_state !== state.ASCII) {
        code_point_pointer.offset(-1);
        iso2022kr_state = state.ASCII;
        return output_byte_stream.emit(0x0F);
      }
      if (inRange(code_point, 0x0000, 0x007F)) {
        return output_byte_stream.emit(code_point);
      }
      if (iso2022kr_state !== state.lead) {
        code_point_pointer.offset(-1);
        iso2022kr_state = state.lead;
        return output_byte_stream.emit(0x0E);
      }
      var pointer = indexPointerFor(code_point, indexes['euc-kr']);
      if (pointer === null) {
        return encoderError(code_point);
      }
      var lead, trail;
      if (pointer < (26 + 26 + 126) * (0xC7 - 0x81)) {
        lead = div(pointer, (26 + 26 + 126)) + 1;
        trail = pointer % (26 + 26 + 126) - 26 - 26 + 1;
        if (!inRange(lead, 0x21, 0x46) || !inRange(trail, 0x21, 0x7E)) {
          return encoderError(code_point);
        }
        return output_byte_stream.emit(lead, trail);
      }
      pointer = pointer - (26 + 26 + 126) * (0xC7 - 0x81);
      lead = div(pointer, 94) + 0x47;
      trail = pointer % 94 + 0x21;
      if (!inRange(lead, 0x47, 0x7E) || !inRange(trail, 0x21, 0x7E)) {
        return encoderError(code_point);
      }
      return output_byte_stream.emit(lead, trail);
    };
  }

  name_to_encoding['iso-2022-kr'].getEncoder = function(options) {
    return new ISO2022KREncoder(options);
  };
  name_to_encoding['iso-2022-kr'].getDecoder = function(options) {
    return new ISO2022KRDecoder(options);
  };


  //
  // 13. Legacy utf-16 encodings
  //

  // 13.1 utf-16

  /**
   * @constructor
   * @param {boolean} utf16_be True if big-endian, false if little-endian.
   * @param {{fatal: boolean}} options
   */
  function UTF16Decoder(utf16_be, options) {
    var fatal = options.fatal;
    var /** @type {?number} */ utf16_lead_byte = null,
        /** @type {?number} */ utf16_lead_surrogate = null;
    /**
     * @param {ByteInputStream} byte_pointer The byte stream to decode.
     * @return {?number} The next code point decoded, or null if not enough
     *     data exists in the input stream to decode a complete code point.
     */
    this.decode = function(byte_pointer) {
      var bite = byte_pointer.get();
      if (bite === EOF_byte && utf16_lead_byte === null &&
          utf16_lead_surrogate === null) {
        return EOF_code_point;
      }
      if (bite === EOF_byte && (utf16_lead_byte !== null ||
                                utf16_lead_surrogate !== null)) {
        return decoderError(fatal);
      }
      byte_pointer.offset(1);
      if (utf16_lead_byte === null) {
        utf16_lead_byte = bite;
        return null;
      }
      var code_point;
      if (utf16_be) {
        code_point = (utf16_lead_byte << 8) + bite;
      } else {
        code_point = (bite << 8) + utf16_lead_byte;
      }
      utf16_lead_byte = null;
      if (utf16_lead_surrogate !== null) {
        var lead_surrogate = utf16_lead_surrogate;
        utf16_lead_surrogate = null;
        if (inRange(code_point, 0xDC00, 0xDFFF)) {
          return 0x10000 + (lead_surrogate - 0xD800) * 0x400 +
              (code_point - 0xDC00);
        }
        byte_pointer.offset(-2);
        return decoderError(fatal);
      }
      if (inRange(code_point, 0xD800, 0xDBFF)) {
        utf16_lead_surrogate = code_point;
        return null;
      }
      if (inRange(code_point, 0xDC00, 0xDFFF)) {
        return decoderError(fatal);
      }
      return code_point;
    };
  }

  /**
   * @constructor
   * @param {boolean} utf16_be True if big-endian, false if little-endian.
   * @param {{fatal: boolean}} options
   */
  function UTF16Encoder(utf16_be, options) {
    var fatal = options.fatal;
    /**
     * @param {ByteOutputStream} output_byte_stream Output byte stream.
     * @param {CodePointInputStream} code_point_pointer Input stream.
     * @return {number} The last byte emitted.
     */
    this.encode = function(output_byte_stream, code_point_pointer) {
      function convert_to_bytes(code_unit) {
        var byte1 = code_unit >> 8;
        var byte2 = code_unit & 0x00FF;
        if (utf16_be) {
          return output_byte_stream.emit(byte1, byte2);
        }
        return output_byte_stream.emit(byte2, byte1);
      }
      var code_point = code_point_pointer.get();
      if (code_point === EOF_code_point) {
        return EOF_byte;
      }
      code_point_pointer.offset(1);
      if (inRange(code_point, 0xD800, 0xDFFF)) {
        encoderError(code_point);
      }
      if (code_point <= 0xFFFF) {
        return convert_to_bytes(code_point);
      }
      var lead = div((code_point - 0x10000), 0x400) + 0xD800;
      var trail = ((code_point - 0x10000) % 0x400) + 0xDC00;
      convert_to_bytes(lead);
      return convert_to_bytes(trail);
    };
  }

  name_to_encoding['utf-16'].getEncoder = function(options) {
    return new UTF16Encoder(false, options);
  };
  name_to_encoding['utf-16'].getDecoder = function(options) {
    return new UTF16Decoder(false, options);
  };

  // 13.2 utf-16be
  name_to_encoding['utf-16be'].getEncoder = function(options) {
    return new UTF16Encoder(true, options);
  };
  name_to_encoding['utf-16be'].getDecoder = function(options) {
    return new UTF16Decoder(true, options);
  };


  // NOTE: currently unused
  /**
   * @param {string} label The encoding label.
   * @param {ByteInputStream} input_stream The byte stream to test.
   */
  function detectEncoding(label, input_stream) {
    if (input_stream.match([0xFF, 0xFE])) {
      input_stream.offset(2);
      return 'utf-16';
    }
    if (input_stream.match([0xFE, 0xFF])) {
      input_stream.offset(2);
      return 'utf-16be';
    }
    if (input_stream.match([0xEF, 0xBB, 0xBF])) {
      input_stream.offset(3);
      return 'utf-8';
    }
    return label;
  }

  /**
   * @param {string} label The encoding label.
   * @param {ByteInputStream} input_stream The byte stream to test.
   */
  function consumeBOM(label, input_stream) {
    if (input_stream.match([0xFF, 0xFE]) && label === 'utf-16') {
      input_stream.offset(2);
      return;
    }
    if (input_stream.match([0xFE, 0xFF]) && label == 'utf-16be') {
      input_stream.offset(2);
      return;
    }
    if (input_stream.match([0xEF, 0xBB, 0xBF]) && label == 'utf-8') {
      input_stream.offset(3);
      return;
    }
  }

  //
  // Implementation of Text Encoding Web API
  //

  /** @const */ var DEFAULT_ENCODING = 'utf-8';

  /**
   * @constructor
   * @param {string=} opt_encoding The label of the encoding;
   *     defaults to 'utf-8'.
   * @param {{fatal: boolean}=} options
   */
  function TextEncoder(opt_encoding, options) {
    if (!this || this === global) {
      return new TextEncoder(opt_encoding, options);
    }
    opt_encoding = opt_encoding ? String(opt_encoding) : DEFAULT_ENCODING;
    options = Object(options);
    /** @private */
    this._encoding = getEncoding(opt_encoding);
    if (this._encoding === null || (this._encoding.name !== 'utf-8' &&
                                    this._encoding.name !== 'utf-16' &&
                                    this._encoding.name !== 'utf-16be'))
      throw new TypeError('Unknown encoding: ' + opt_encoding);
    /* @private @type {boolean} */
    this._streaming = false;
    /** @private */
    this._encoder = null;
    /* @private @type {{fatal: boolean}=} */
    this._options = { fatal: Boolean(options.fatal) };

    if (Object.defineProperty) {
      Object.defineProperty(
          this, 'encoding',
          { get: function() { return this._encoding.name; } });
    } else {
      this.encoding = this._encoding.name;
    }

    return this;
  }

  TextEncoder.prototype = {
    /**
     * @param {string=} opt_string The string to encode.
     * @param {{stream: boolean}=} options
     */
    encode: function encode(opt_string, options) {
      opt_string = opt_string ? String(opt_string) : '';
      options = Object(options);
      // TODO: any options?
      if (!this._streaming) {
        this._encoder = this._encoding.getEncoder(this._options);
      }
      this._streaming = Boolean(options.stream);

      var bytes = [];
      var output_stream = new ByteOutputStream(bytes);
      var input_stream = new CodePointInputStream(opt_string);
      while (input_stream.get() !== EOF_code_point) {
        this._encoder.encode(output_stream, input_stream);
      }
      if (!this._streaming) {
        var last_byte;
        do {
          last_byte = this._encoder.encode(output_stream, input_stream);
        } while (last_byte !== EOF_byte);
        this._encoder = null;
      }
      return new Uint8Array(bytes);
    }
  };


  /**
   * @constructor
   * @param {string=} opt_encoding The label of the encoding;
   *     defaults to 'utf-8'.
   * @param {{fatal: boolean}=} options
   */
  function TextDecoder(opt_encoding, options) {
    if (!this || this === global) {
      return new TextDecoder(opt_encoding, options);
    }
    opt_encoding = opt_encoding ? String(opt_encoding) : DEFAULT_ENCODING;
    options = Object(options);
    /** @private */
    this._encoding = getEncoding(opt_encoding);
    if (this._encoding === null)
      throw new TypeError('Unknown encoding: ' + opt_encoding);

    /* @private @type {boolean} */
    this._streaming = false;
    /** @private */
    this._decoder = null;
    /* @private @type {{fatal: boolean}=} */
    this._options = { fatal: Boolean(options.fatal) };

    if (Object.defineProperty) {
      Object.defineProperty(
          this, 'encoding',
          { get: function() { return this._encoding.name; } });
    } else {
      this.encoding = this._encoding.name;
    }

    return this;
  }

  // TODO: Issue if input byte stream is offset by decoder
  // TODO: BOM detection will not work if stream header spans multiple calls
  // (last N bytes of previous stream may need to be retained?)
  TextDecoder.prototype = {
    /**
     * @param {ArrayBufferView=} opt_view The buffer of bytes to decode.
     * @param {{stream: boolean}=} options
     */
    decode: function decode(opt_view, options) {
      if (opt_view && !('buffer' in opt_view && 'byteOffset' in opt_view &&
                        'byteLength' in opt_view)) {
        throw new TypeError('Expected ArrayBufferView');
      } else if (!opt_view) {
        opt_view = new Uint8Array(0);
      }
      options = Object(options);

      if (!this._streaming) {
        this._decoder = this._encoding.getDecoder(this._options);
      }
      this._streaming = Boolean(options.stream);

      var bytes = new Uint8Array(opt_view.buffer,
                                 opt_view.byteOffset,
                                 opt_view.byteLength);
      var input_stream = new ByteInputStream(bytes);

      if (!this._BOMseen) {
        // TODO: Don't do this until sufficient bytes are present
        this._BOMseen = true;
        consumeBOM(this._encoding.name, input_stream);
      }

      var output_stream = new CodePointOutputStream(), code_point;
      while (input_stream.get() !== EOF_byte) {
        code_point = this._decoder.decode(input_stream);
        if (code_point !== null && code_point !== EOF_code_point) {
          output_stream.emit(code_point);
        }
      }
      if (!this._streaming) {
        do {
          code_point = this._decoder.decode(input_stream);
          if (code_point !== null && code_point !== EOF_code_point) {
            output_stream.emit(code_point);
          }
        } while (code_point !== EOF_code_point &&
                 input_stream.get() != EOF_byte);
        this._decoder = null;
      }
      return output_stream.string();
    }
  };

  global['TextEncoder'] = global['TextEncoder'] || TextEncoder;
  global['TextDecoder'] = global['TextDecoder'] || TextDecoder;
}(this));
/* jshint ignore:end */
!(function() {

  // Rumor Messaging for JS
  //
  // https://tbwiki.tokbox.com/index.php/Rumor_:_Messaging_FrameWork
  //
  // @todo Rumor {
  //     Add error codes for all the error cases
  //     Add Dependability commands
  // }

  OT.Rumor = {
    MessageType: {
      // This is used to subscribe to address/addresses. The address/addresses the
      // client specifies here is registered on the server. Once any message is sent to
      // that address/addresses, the client receives that message.
      SUBSCRIBE: 0,

      // This is used to unsubscribe to address / addresses. Once the client unsubscribe
      // to an address, it will stop getting messages sent to that address.
      UNSUBSCRIBE: 1,

      // This is used to send messages to arbitrary address/ addresses. Messages can be
      // anything and Rumor will not care about what is included.
      MESSAGE: 2,

      // This will be the first message that the client sends to the server. It includes
      // the uniqueId for that client connection and a disconnect_notify address that will
      // be notified once the client disconnects.
      CONNECT: 3,

      // This will be the message used by the server to notify an address that a
      // client disconnected.
      DISCONNECT: 4,

      //Enhancements to support Keepalives
      PING: 7,
      PONG: 8,
      STATUS: 9
    }
  };

}(this));
!(function() {

  // The interval between polling the websocket's send buffer
  var BUFFER_DRAIN_INTERVAL = 100,
      // The total number of times to retest the websocket's send buffer
      BUFFER_DRAIN_MAX_RETRIES = 10,

      WEB_SOCKET_KEEP_ALIVE_INTERVAL = 9000,

      // Magic Connectivity Timeout Constant: We wait 9*the keep alive interval,
      // on the third keep alive we trigger the timeout if we haven't received the
      // server pong.
      WEB_SOCKET_CONNECTIVITY_TIMEOUT = 5*WEB_SOCKET_KEEP_ALIVE_INTERVAL - 100,

      wsCloseErrorCodes;



  // https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent#Close_codes
  // http://docs.oracle.com/javaee/7/api/javax/websocket/CloseReason.CloseCodes.html
  wsCloseErrorCodes = {
    1002:  'The endpoint is terminating the connection due to a protocol error. ' +
      '(CLOSE_PROTOCOL_ERROR)',
    1003:  'The connection is being terminated because the endpoint received data of ' +
      'a type it cannot accept (for example, a text-only endpoint received binary data). ' +
      '(CLOSE_UNSUPPORTED)',
    1004:  'The endpoint is terminating the connection because a data frame was received ' +
      'that is too large. (CLOSE_TOO_LARGE)',
    1005:  'Indicates that no status code was provided even though one was expected. ' +
    '(CLOSE_NO_STATUS)',
    1006:  'Used to indicate that a connection was closed abnormally (that is, with no ' +
      'close frame being sent) when a status code is expected. (CLOSE_ABNORMAL)',
    1007: 'Indicates that an endpoint is terminating the connection because it has received ' +
      'data within a message that was not consistent with the type of the message (e.g., ' +
      'non-UTF-8 [RFC3629] data within a text message)',
    1008: 'Indicates that an endpoint is terminating the connection because it has received a ' +
      'message that violates its policy.  This is a generic status code that can be returned ' +
      'when there is no other more suitable status code (e.g., 1003 or 1009) or if there is a ' +
      'need to hide specific details about the policy',
    1009: 'Indicates that an endpoint is terminating the connection because it has received a ' +
      'message that is too big for it to process',
    1011: 'Indicates that a server is terminating the connection because it encountered an ' +
      'unexpected condition that prevented it from fulfilling the request',

    // .... codes in the 4000-4999 range are available for use by applications.
    4001:   'Connectivity loss was detected as it was too long since the socket received the ' +
      'last PONG message'
  };

  OT.Rumor.SocketError = function(code, message) {
    this.code = code;
    this.message = message;
  };

  // The NativeSocket bit is purely to make testing simpler, it defaults to WebSocket
  // so in normal operation you would omit it.
  OT.Rumor.Socket = function(messagingServer, notifyDisconnectAddress, NativeSocket) {
    var states = ['disconnected',  'error', 'connected', 'connecting', 'disconnecting'],
        server = messagingServer,
        webSocket,
        id,
        onOpen,
        onError,
        onClose,
        onMessage,
        connectCallback,
        bufferDrainTimeout,           // Timer to poll whether th send buffer has been drained
        connectTimeout,
        lastMessageTimestamp,         // The timestamp of the last message received
        keepAliveTimer;               // Timer for the connectivity checks


    //// Private API
    var stateChanged = function(newState) {
          switch (newState) {
            case 'disconnected':
            case 'error':
              webSocket = null;
              if (onClose) {
                var error;
                if(hasLostConnectivity()) {
                  error = new Error(wsCloseErrorCodes[4001]);
                  error.code = 4001;
                }
                onClose(error);
              }
              break;
          }
        },

        setState = OT.$.statable(this, states, 'disconnected', stateChanged),

        validateCallback = function validateCallback (name, callback) {
          if (callback === null || !OT.$.isFunction(callback) ) {
            throw new Error('The Rumor.Socket ' + name +
              ' callback must be a valid function or null');
          }
        },

        error = function error (errorMessage) {
          OT.error('Rumor.Socket: ' + errorMessage);

          var socketError = new OT.Rumor.SocketError(null, errorMessage || 'Unknown Socket Error');

          if (connectTimeout) clearTimeout(connectTimeout);

          setState('error');

          if (this.previousState === 'connecting' && connectCallback) {
            connectCallback(socketError, null);
            connectCallback = null;
          }

          if (onError) onError(socketError);
        }.bind(this),

        // Immediately close the socket, only used by disconnectWhenSendBufferIsDrained
        close = function close() {
          setState('disconnecting');

          if (bufferDrainTimeout) {
            clearTimeout(bufferDrainTimeout);
            bufferDrainTimeout = null;
          }

          webSocket.close();
        },

        // Ensure that the WebSocket send buffer is fully drained before disconnecting
        // the socket. If the buffer doesn't drain after a certain length of time
        // we give up and close it anyway.
        disconnectWhenSendBufferIsDrained =
          function disconnectWhenSendBufferIsDrained (bufferDrainRetries) {
          if (!webSocket) return;

          if (bufferDrainRetries === void 0) bufferDrainRetries = 0;
          if (bufferDrainTimeout) clearTimeout(bufferDrainTimeout);

          if (webSocket.bufferedAmount > 0 &&
            (bufferDrainRetries + 1) <= BUFFER_DRAIN_MAX_RETRIES) {
            bufferDrainTimeout = setTimeout(disconnectWhenSendBufferIsDrained,
              BUFFER_DRAIN_INTERVAL, bufferDrainRetries+1);
          }
          else {
            close();
          }
        },

        hasLostConnectivity = function hasLostConnectivity () {
          if (!lastMessageTimestamp) return false;

          return (OT.$.now() - lastMessageTimestamp) >= WEB_SOCKET_CONNECTIVITY_TIMEOUT;
        },

        sendKeepAlive = function sendKeepAlive () {
          if (!this.is('connected')) return;

          if ( hasLostConnectivity() ) {
            webSocketDisconnected({code: 4001});
          }
          else  {
            webSocket.send(OT.Rumor.Message.Ping().serialize());
            keepAliveTimer = setTimeout(sendKeepAlive.bind(this), WEB_SOCKET_KEEP_ALIVE_INTERVAL);
          }
        }.bind(this);


    //// Private Event Handlers
    var webSocketConnected = function webSocketConnected () {
          if (connectTimeout) clearTimeout(connectTimeout);
          if (this.isNot('connecting')) {
            OT.debug('webSocketConnected reached in state other than connecting');
            return;
          }

          // Connect to Rumor by registering our connection id and the
          // app server address to notify if we disconnect.
          //
          // We don't need to wait for a reply to this message.
          webSocket.send(OT.Rumor.Message.Connect(id, notifyDisconnectAddress).serialize());

          setState('connected');
          if (connectCallback) {
            connectCallback(null, id);
            connectCallback = null;
          }

          if (onOpen) onOpen(id);

          setTimeout(function() {
            lastMessageTimestamp = OT.$.now();
            sendKeepAlive();
          }, WEB_SOCKET_KEEP_ALIVE_INTERVAL);
        }.bind(this),

        webSocketConnectTimedOut = function webSocketConnectTimedOut () {
          var webSocketWas = webSocket;
          error('Timed out while waiting for the Rumor socket to connect.');
          // This will prevent a socket eventually connecting
          // But call it _after_ the error just in case any of
          // the callbacks fire synchronously, breaking the error
          // handling code.
          try {
            webSocketWas.close();
          } catch(x) {}
        },

        webSocketError = function webSocketError () {},
          // var errorMessage = 'Unknown Socket Error';
          // @fixme We MUST be able to do better than this!

          // All errors seem to result in disconnecting the socket, the close event
          // has a close reason and code which gives some error context. This,
          // combined with the fact that the errorEvent argument contains no
          // error info at all, means we'll delay triggering the error handlers
          // until the socket is closed.
          // error(errorMessage);

        webSocketDisconnected = function webSocketDisconnected (closeEvent) {
          if (connectTimeout) clearTimeout(connectTimeout);
          if (keepAliveTimer) clearTimeout(keepAliveTimer);

          if (closeEvent.code !== 1000 && closeEvent.code !== 1001) {
            var reason = closeEvent.reason || closeEvent.message;
            if (!reason && wsCloseErrorCodes.hasOwnProperty(closeEvent.code)) {
              reason = wsCloseErrorCodes[closeEvent.code];
            }

            error('Rumor Socket Disconnected: ' + reason);
          }

          if (this.isNot('error')) setState('disconnected');
        }.bind(this),

        webSocketReceivedMessage = function webSocketReceivedMessage (message) {
          lastMessageTimestamp = OT.$.now();

          if (onMessage) {

            var msg = OT.Rumor.Message.deserialize(message.data);

            if (msg.type !== OT.Rumor.MessageType.PONG) {
              onMessage(msg);
            }
          }
        };


    //// Public API

    this.publish = function (topics, message, headers) {
      webSocket.send(OT.Rumor.Message.Publish(topics, message, headers).serialize());
    };

    this.subscribe = function(topics) {
      webSocket.send(OT.Rumor.Message.Subscribe(topics).serialize());
    };

    this.unsubscribe = function(topics) {
      webSocket.send(OT.Rumor.Message.Unsubscribe(topics).serialize());
    };

    this.connect = function (connectionId, complete) {
      if (this.is('connecting', 'connected')) {
        complete(new OT.Rumor.SocketError(null,
            'Rumor.Socket cannot connect when it is already connecting or connected.'));
        return;
      }

      id = connectionId;
      connectCallback = complete;

      try {
        setState('connecting');

        var TheWebSocket = NativeSocket || WebSocket;
        webSocket = new TheWebSocket(server);
        webSocket.binaryType = 'arraybuffer';

        webSocket.onopen = webSocketConnected;
        webSocket.onclose = webSocketDisconnected;
        webSocket.onerror = webSocketError;
        webSocket.onmessage = webSocketReceivedMessage;

        connectTimeout = setTimeout(webSocketConnectTimedOut, OT.Rumor.Socket.CONNECT_TIMEOUT);
      }
      catch(e) {
        OT.error(e);

        // @todo add an actual error message
        error('Could not connect to the Rumor socket, possibly because of a blocked port.');
      }
    };

    this.disconnect = function() {
      if (connectTimeout) clearTimeout(connectTimeout);
      if (keepAliveTimer) clearTimeout(keepAliveTimer);

      if (!webSocket) {
        if (this.isNot('error')) setState('disconnected');
        return;
      }

      if (webSocket.readyState === 3/* CLOSED */) {
        if (this.isNot('error')) setState('disconnected');
      }
      else {
        if (this.is('connected')) {
          // Look! We are nice to the rumor server ;-)
          webSocket.send(OT.Rumor.Message.Disconnect().serialize());
        }

        // Wait until the socket is ready to close
        disconnectWhenSendBufferIsDrained();
      }
    };



    Object.defineProperties(this, {
      id: {
        get: function() { return id; }
      },

      onOpen: {
        set: function(callback) {
          validateCallback('onOpen', callback);
          onOpen = callback;
        },

        get: function() { return onOpen; }
      },

      onError: {
        set: function(callback) {
          validateCallback('onError', callback);
          onError = callback;
        },

        get: function() { return onError; }
      },

      onClose: {
        set: function(callback) {
          validateCallback('onClose', callback);
          onClose = callback;
        },

        get: function() { return onClose; }
      },

      onMessage: {
        set: function(callback) {
          validateCallback('onMessage', callback);
          onMessage = callback;
        },

        get: function() { return onMessage; }
      }
    });
  };

  // The number of ms to wait for the websocket to connect
  OT.Rumor.Socket.CONNECT_TIMEOUT = 15000;

}(this));
!(function() {

  /*global TextEncoder, TextDecoder */

  //
  //
  // @references
  // * https://tbwiki.tokbox.com/index.php/Rumor_Message_Packet
  // * https://tbwiki.tokbox.com/index.php/Rumor_Protocol
  //
  OT.Rumor.Message = function (type, toAddress, headers, data) {
    this.type = type;
    this.toAddress = toAddress;
    this.headers = headers;
    this.data = data;

    this.transactionId = this.headers['TRANSACTION-ID'];
    this.status = this.headers.STATUS;
    this.isError = !(this.status && this.status[0] === '2');
  };

  OT.Rumor.Message.prototype.serialize = function () {
    var offset = 8,
        cBuf = 7,
        address = [],
        headerKey = [],
        headerVal = [],
        strArray,
        dataView,
        i,
        j;

    // The number of addresses
    cBuf++;

    // Write out the address.
    for (i = 0; i < this.toAddress.length; i++) {
      /*jshint newcap:false */
      address.push(new TextEncoder('utf-8').encode(this.toAddress[i]));
      cBuf += 2;
      cBuf += address[i].length;
    }

    // The number of parameters
    cBuf++;

    // Write out the params
    i = 0;

    for (var key in this.headers) {
      headerKey.push(new TextEncoder('utf-8').encode(key));
      headerVal.push(new TextEncoder('utf-8').encode(this.headers[key]));
      cBuf += 4;
      cBuf += headerKey[i].length;
      cBuf += headerVal[i].length;

      i++;
    }

    dataView = new TextEncoder('utf-8').encode(this.data);
    cBuf += dataView.length;

    // Let's allocate a binary blob of this size
    var buffer = new ArrayBuffer(cBuf);
    var uint8View = new Uint8Array(buffer, 0, cBuf);

    // We don't include the header in the lenght.
    cBuf -= 4;

    // Write out size (in network order)
    uint8View[0] = (cBuf & 0xFF000000) >>> 24;
    uint8View[1] = (cBuf & 0x00FF0000) >>> 16;
    uint8View[2] = (cBuf & 0x0000FF00) >>>  8;
    uint8View[3] = (cBuf & 0x000000FF) >>>  0;

    // Write out reserved bytes
    uint8View[4] = 0;
    uint8View[5] = 0;

    // Write out message type
    uint8View[6] = this.type;
    uint8View[7] = this.toAddress.length;

    // Now just copy over the encoded values..
    for (i = 0; i < address.length; i++) {
      strArray = address[i];
      uint8View[offset++] = strArray.length >> 8 & 0xFF;
      uint8View[offset++] = strArray.length >> 0 & 0xFF;
      for (j = 0; j < strArray.length; j++) {
        uint8View[offset++] = strArray[j];
      }
    }

    uint8View[offset++] = headerKey.length;

    // Write out the params
    for (i = 0; i < headerKey.length; i++) {
      strArray = headerKey[i];
      uint8View[offset++] = strArray.length >> 8 & 0xFF;
      uint8View[offset++] = strArray.length >> 0 & 0xFF;
      for (j = 0; j < strArray.length; j++) {
        uint8View[offset++] = strArray[j];
      }

      strArray = headerVal[i];
      uint8View[offset++] = strArray.length >> 8 & 0xFF;
      uint8View[offset++] = strArray.length >> 0 & 0xFF;
      for (j = 0; j < strArray.length; j++) {
        uint8View[offset++] = strArray[j];
      }
    }

    // And finally the data
    for (i = 0; i < dataView.length; i++) {
      uint8View[offset++] = dataView[i];
    }

    return buffer;
  };

  function toArrayBuffer(buffer) {
    var ab = new ArrayBuffer(buffer.length);
    var view = new Uint8Array(ab);
    for (var i = 0; i < buffer.length; ++i) {
      view[i] = buffer[i];
    }
    return ab;
  }

  OT.Rumor.Message.deserialize = function (buffer) {

    if(typeof Buffer !== 'undefined' &&
      Buffer.isBuffer(buffer)) {
      buffer = toArrayBuffer(buffer);
    }
    var cBuf = 0,
        type,
        offset = 8,
        uint8View = new Uint8Array(buffer),
        strView,
        headerlen,
        headers,
        keyStr,
        valStr,
        length,
        i;

    // Write out size (in network order)
    cBuf += uint8View[0] << 24;
    cBuf += uint8View[1] << 16;
    cBuf += uint8View[2] <<  8;
    cBuf += uint8View[3] <<  0;

    type = uint8View[6];
    var address = [];

    for (i = 0; i < uint8View[7]; i++) {
      length = uint8View[offset++] << 8;
      length += uint8View[offset++];
      strView = new Uint8Array(buffer, offset, length);
      /*jshint newcap:false */
      address[i] = new TextDecoder('utf-8').decode(strView);
      offset += length;
    }

    headerlen = uint8View[offset++];
    headers = {};

    for (i = 0; i < headerlen; i++) {
      length = uint8View[offset++] << 8;
      length += uint8View[offset++];
      strView = new Uint8Array(buffer, offset, length);
      keyStr = new TextDecoder('utf-8').decode(strView);
      offset += length;

      length = uint8View[offset++] << 8;
      length += uint8View[offset++];
      strView = new Uint8Array(buffer, offset, length);
      valStr = new TextDecoder('utf-8').decode(strView);
      headers[keyStr] = valStr;
      offset += length;
    }

    var dataView = new Uint8Array(buffer, offset);
    var data = new TextDecoder('utf-8').decode(dataView);

    return new OT.Rumor.Message(type, address, headers, data);
  };


  OT.Rumor.Message.Connect = function (uniqueId, notifyDisconnectAddress) {
    var headers = {
      uniqueId: uniqueId,
      notifyDisconnectAddress: notifyDisconnectAddress
    };

    return new OT.Rumor.Message(OT.Rumor.MessageType.CONNECT, [], headers, '');
  };

  OT.Rumor.Message.Disconnect = function () {
    return new OT.Rumor.Message(OT.Rumor.MessageType.DISCONNECT, [], [], '');
  };

  OT.Rumor.Message.Subscribe = function(topics) {
    return new OT.Rumor.Message(OT.Rumor.MessageType.SUBSCRIBE, topics, [], '');
  };

  OT.Rumor.Message.Unsubscribe = function(topics) {
    return new OT.Rumor.Message(OT.Rumor.MessageType.UNSUBSCRIBE, topics, [], '');
  };

  OT.Rumor.Message.Publish = function(topics, message, headers) {
    return new OT.Rumor.Message(OT.Rumor.MessageType.MESSAGE, topics, headers||[], message);
  };

  // This message is used to implement keepalives on the persistent
  // socket connection between the client and server. Every time the
  // client sends a PING to the server, the server will respond with
  // a PONG.
  OT.Rumor.Message.Ping = function() {
    return new OT.Rumor.Message(OT.Rumor.MessageType.PING, [], [], '');
  };

}(this));
!(function() {

  // Rumor Messaging for JS
  //
  // https://tbwiki.tokbox.com/index.php/Raptor_Messages_(Sent_as_a_RumorMessage_payload_in_JSON)
  //
  // @todo Raptor {
  //     Look at disconnection cleanup: i.e. subscriber + publisher cleanup
  //     Add error codes for all the error cases
  //     Write unit tests for SessionInfo
  //     Write unit tests for Session
  //     Make use of the new DestroyedEvent
  //     Remove dependency on OT.properties
  //     OT.Capabilities must be part of the Raptor namespace
  //     Add Dependability commands
  //     Think about noConflict, or whether we should just use the OT namespace
  //     Think about how to expose OT.publishers, OT.subscribers, and OT.sessions if messaging was
  //        being included as a component
  //     Another solution to the problem of having publishers/subscribers/etc would be to make
  //        Raptor Socket a separate component from Dispatch (dispatch being more business logic)
  //     Look at the coupling of OT.sessions to OT.Raptor.Socket
  // }
  //
  // @todo Raptor Docs {
  //   Document payload formats for incoming messages (what are the payloads for 
  //    STREAM CREATED/MODIFIED for example)
  //   Document how keepalives work
  //   Document all the Raptor actions and types
  //   Document the session connect flow (including error cases)
  // }

  OT.Raptor = {
    Actions: {
      //General
      CONNECT: 100,
      CREATE: 101,
      UPDATE: 102,
      DELETE: 103,
      STATE: 104,

      //Moderation
      FORCE_DISCONNECT: 105,
      FORCE_UNPUBLISH: 106,
      SIGNAL: 107,

      //Archives
      CREATE_ARCHIVE: 108,
      CLOSE_ARCHIVE: 109,
      START_RECORDING_SESSION: 110,
      STOP_RECORDING_SESSION: 111,
      START_RECORDING_STREAM: 112,
      STOP_RECORDING_STREAM: 113,
      LOAD_ARCHIVE: 114,
      START_PLAYBACK: 115,
      STOP_PLAYBACK: 116,

      //AppState
      APPSTATE_PUT: 117,
      APPSTATE_DELETE: 118,

      // JSEP
      OFFER: 119,
      ANSWER: 120,
      PRANSWER: 121,
      CANDIDATE: 122,
      SUBSCRIBE: 123,
      UNSUBSCRIBE: 124,
      QUERY: 125,
      SDP_ANSWER: 126,

      //KeepAlive
      PONG: 127,
      REGISTER: 128, //Used for registering streams.

      QUALITY_CHANGED: 129
    },

    Types: {
      //RPC
      RPC_REQUEST: 100,
      RPC_RESPONSE: 101,

      //EVENT
      STREAM: 102,
      ARCHIVE: 103,
      CONNECTION: 104,
      APPSTATE: 105,
      CONNECTIONCOUNT: 106,
      MODERATION: 107,
      SIGNAL: 108,
      SUBSCRIBER: 110,

      //JSEP Protocol
      JSEP: 109
    }
  };

}(this));
!(function() {


  OT.Raptor.serializeMessage = function (message) {
    return JSON.stringify(message);
  };


  // Deserialising a Raptor message mainly means doing a JSON.parse on it.
  // We do decorate the final message with a few extra helper properies though.
  //
  // These include:
  // * typeName: A human readable version of the Raptor type. E.g. STREAM instead of 102
  // * actionName: A human readable version of the Raptor action. E.g. CREATE instead of 101
  // * signature: typeName and actionName combined. This is mainly for debugging. E.g. A type
  //    of 102 and an action of 101 would result in a signature of "STREAM:CREATE"
  //
  OT.Raptor.deserializeMessage = function (msg) {
    if (msg.length === 0) return {};

    var message = JSON.parse(msg),
        bits = message.uri.substr(1).split('/');

    // Remove the Raptor protocol version
    bits.shift();
    if (bits[bits.length-1] === '') bits.pop();

    message.params = {};
    for (var i=0, numBits=bits.length ; i<numBits-1; i+=2) {
      message.params[bits[i]] = bits[i+1];
    }

    // extract the resource name. We special case 'channel' slightly, as
    // 'subscriber_channel' or 'stream_channel' is more useful for us
    // than 'channel' alone.
    if (bits.length % 2 === 0) {
      if (bits[bits.length-2] === 'channel' && bits.length > 6) {
        message.resource = bits[bits.length-4] + '_' + bits[bits.length-2];
      } else {
        message.resource = bits[bits.length-2];
      }
    }
    else {
      if (bits[bits.length-1] === 'channel' && bits.length > 5) {
        message.resource = bits[bits.length-3] + '_' + bits[bits.length-1];
      } else {
        message.resource = bits[bits.length-1];
      }
    }

    message.signature = message.resource + '#' + message.method;
    return message;
  };

  OT.Raptor.unboxFromRumorMessage = function (rumorMessage) {
    var message = OT.Raptor.deserializeMessage(rumorMessage.data);
    message.transactionId = rumorMessage.transactionId;
    message.fromAddress = rumorMessage.headers['X-TB-FROM-ADDRESS'];

    return message;
  };
  
  OT.Raptor.parseIceServers = function (message) {
    try {
      return JSON.parse(message.data).content.iceServers;
    } catch (e) {
      return [];
    }
  };

  OT.Raptor.Message = {};


  OT.Raptor.Message.connections = {};

  OT.Raptor.Message.connections.create = function (apiKey, sessionId, connectionId) {
    return OT.Raptor.serializeMessage({
      method: 'create',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/connection/' + connectionId,
      content: {
        userAgent: navigator.userAgent
      }
    });
  };

  OT.Raptor.Message.connections.destroy = function (apiKey, sessionId, connectionId) {
    return OT.Raptor.serializeMessage({
      method: 'delete',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/connection/' + connectionId,
      content: {}
    });
  };


  OT.Raptor.Message.sessions = {};

  OT.Raptor.Message.sessions.get = function (apiKey, sessionId) {
    return OT.Raptor.serializeMessage({
      method: 'read',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId,
      content: {}
    });
  };


  OT.Raptor.Message.streams = {};

  OT.Raptor.Message.streams.get = function (apiKey, sessionId, streamId) {
    return OT.Raptor.serializeMessage({
      method: 'read',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
      content: {}
    });
  };

  OT.Raptor.Message.streams.create = function (apiKey, sessionId, streamId, name, videoOrientation,
    videoWidth, videoHeight, hasAudio, hasVideo, frameRate, minBitrate, maxBitrate) {
    var channels = [];

    if (hasAudio !== void 0) {
      channels.push({
        id: 'audio1',
        type: 'audio',
        active: hasAudio
      });
    }

    if (hasVideo !== void 0) {
      var channel = {
        id: 'video1',
        type: 'video',
        active: hasVideo,
        width: videoWidth,
        height: videoHeight,
        orientation: videoOrientation
      };
      if (frameRate) channel.frameRate = frameRate;
      channels.push(channel);
    }
    
    var messageContent = {
      id: streamId,
      name: name,
      channel: channels
    };
    
    if (minBitrate) messageContent.minBitrate = minBitrate;
    if (maxBitrate) messageContent.maxBitrate = maxBitrate;

    return OT.Raptor.serializeMessage({
      method: 'create',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
      content: messageContent
    });
  };

  OT.Raptor.Message.streams.destroy = function (apiKey, sessionId, streamId) {
    return OT.Raptor.serializeMessage({
      method: 'delete',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
      content: {}
    });
  };

  OT.Raptor.Message.streams.offer = function (apiKey, sessionId, streamId, offerSdp) {
    return OT.Raptor.serializeMessage({
      method: 'offer',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
      content: {
        sdp: offerSdp
      }
    });
  };

  OT.Raptor.Message.streams.answer = function (apiKey, sessionId, streamId, answerSdp) {
    return OT.Raptor.serializeMessage({
      method: 'answer',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
      content: {
        sdp: answerSdp
      }
    });
  };

  OT.Raptor.Message.streams.candidate = function (apiKey, sessionId, streamId, candidate) {
    return OT.Raptor.serializeMessage({
      method: 'candidate',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' + streamId,
      content: candidate
    });
  };

  OT.Raptor.Message.streamChannels = {};
  OT.Raptor.Message.streamChannels.update =
    function (apiKey, sessionId, streamId, channelId, attributes) {
    return OT.Raptor.serializeMessage({
      method: 'update',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId + '/stream/' +
        streamId + '/channel/' + channelId,
      content: attributes
    });
  };


  OT.Raptor.Message.subscribers = {};

  OT.Raptor.Message.subscribers.create =
    function (apiKey, sessionId, streamId, subscriberId, connectionId, channelsToSubscribeTo) {
    var content = {
      id: subscriberId,
      connection: connectionId,
      keyManagementMethod: OT.$.supportedCryptoScheme(),
      bundleSupport: OT.$.supportsBundle(),
      rtcpMuxSupport: OT.$.supportsRtcpMux()
    };
    if (channelsToSubscribeTo) content.channel = channelsToSubscribeTo;

    return OT.Raptor.serializeMessage({
      method: 'create',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
        '/stream/' + streamId + '/subscriber/' + subscriberId,
      content: content
    });
  };

  OT.Raptor.Message.subscribers.destroy = function (apiKey, sessionId, streamId, subscriberId) {
    return OT.Raptor.serializeMessage({
      method: 'delete',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
        '/stream/' + streamId + '/subscriber/' + subscriberId,
      content: {}
    });
  };

  OT.Raptor.Message.subscribers.update =
    function (apiKey, sessionId, streamId, subscriberId, attributes) {
    return OT.Raptor.serializeMessage({
      method: 'update',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
      '/stream/' + streamId + '/subscriber/' + subscriberId,
      content: attributes
    });
  };


  OT.Raptor.Message.subscribers.candidate =
    function (apiKey, sessionId, streamId, subscriberId, candidate) {
    return OT.Raptor.serializeMessage({
      method: 'candidate',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
        '/stream/' + streamId + '/subscriber/' + subscriberId,
      content: candidate
    });
  };

  OT.Raptor.Message.subscribers.offer =
    function (apiKey, sessionId, streamId, subscriberId, offerSdp) {
    return OT.Raptor.serializeMessage({
      method: 'offer',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
        '/stream/' + streamId + '/subscriber/' + subscriberId,
      content: {
        sdp: offerSdp
      }
    });
  };

  OT.Raptor.Message.subscribers.answer =
    function (apiKey, sessionId, streamId, subscriberId, answerSdp) {
    return OT.Raptor.serializeMessage({
      method: 'answer',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
      '/stream/' + streamId + '/subscriber/' + subscriberId,
      content: {
        sdp: answerSdp
      }
    });
  };


  OT.Raptor.Message.subscriberChannels = {};

  OT.Raptor.Message.subscriberChannels.update =
    function (apiKey, sessionId, streamId, subscriberId, channelId, attributes) {
    return OT.Raptor.serializeMessage({
      method: 'update',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
      '/stream/' + streamId + '/subscriber/' + subscriberId + '/channel/' + channelId,
      content: attributes
    });
  };


  OT.Raptor.Message.signals = {};

  OT.Raptor.Message.signals.create = function (apiKey, sessionId, toAddress, type, data) {
    var content = {};
    if (type !== void 0) content.type = type;
    if (data !== void 0) content.data = data;

    return OT.Raptor.serializeMessage({
      method: 'signal',
      uri: '/v2/partner/' + apiKey + '/session/' + sessionId +
        (toAddress !== void 0 ? '/connection/' + toAddress : '') + '/signal/' + OT.$.uuid(),
      content: content
    });
  };

}(this));
!(function() {

  var MAX_SIGNAL_DATA_LENGTH = 8192,
      MAX_SIGNAL_TYPE_LENGTH = 128;

  //
  // Error Codes:
  // 413 - Type too long
  // 400 - Type is invalid
  // 413 - Data too long
  // 400 - Data is invalid (can't be parsed as JSON)
  // 429 - Rate limit exceeded
  // 500 - Websocket connection is down
  // 404 - To connection does not exist
  // 400 - To is invalid
  //
  OT.Signal = function(sessionId, fromConnectionId, options) {
    var isInvalidType = function(type) {
        // Our format matches the unreserved characters from the URI RFC:
        // http://www.ietf.org/rfc/rfc3986
        return !/^[a-zA-Z0-9\-\._~]+$/.exec(type);
      },

      validateTo = function(toAddress) {
        if (!toAddress) {
          return {
            code: 400,
            reason: 'The signal type was null or an empty String. Either set it to a non-empty ' +
              'String value or omit it'
          };
        }

        if ( !(toAddress instanceof OT.Connection || toAddress instanceof OT.Session) ) {
          return {
            code: 400,
            reason: 'The To field was invalid'
          };
        }

        return null;
      },

      validateType = function(type) {
        var error = null;

        if (type === null || type === void 0) {
          error = {
            code: 400,
            reason: 'The signal type was null or undefined. Either set it to a String value or ' +
              'omit it'
          };
        }
        else if (type.length > MAX_SIGNAL_TYPE_LENGTH) {
          error = {
            code: 413,
            reason: 'The signal type was too long, the maximum length of it is ' +
              MAX_SIGNAL_TYPE_LENGTH + ' characters'
          };
        }
        else if ( isInvalidType(type) ) {
          error = {
            code: 400,
            reason: 'The signal type was invalid, it can only contain letters, ' +
              'numbers, \'-\', \'_\', and \'~\'.'
          };
        }

        return error;
      },

      validateData = function(data) {
        var error = null;
        if (data === null || data === void 0) {
          error = {
            code: 400,
            reason: 'The signal data was null or undefined. Either set it to a String value or ' +
              'omit it'
          };
        }
        else {
          try {
            if (JSON.stringify(data).length > MAX_SIGNAL_DATA_LENGTH) {
              error = {
                code: 413,
                reason: 'The data field was too long, the maximum size of it is ' +
                  MAX_SIGNAL_DATA_LENGTH + ' characters'
              };
            }
          }
          catch(e) {
            error = {code: 400, reason: 'The data field was not valid JSON'};
          }
        }

        return error;
      };


    this.toRaptorMessage = function() {
      var to = this.to;

      if (to && typeof(to) !== 'string') {
        to = to.id;
      }

      return OT.Raptor.Message.signals.create(OT.APIKEY, sessionId, to, this.type, this.data);
    };

    this.toHash = function() {
      return options;
    };


    this.error = null;

    if (options) {
      if (options.hasOwnProperty('data')) {
        this.data = OT.$.clone(options.data);
        this.error = validateData(this.data);
      }

      if (options.hasOwnProperty('to')) {
        this.to = options.to;

        if (!this.error) {
          this.error = validateTo(this.to);
        }
      }

      if (options.hasOwnProperty('type')) {
        if (!this.error) {
          this.error = validateType(options.type);
        }
        this.type = options.type;
      }
    }

    this.valid = this.error === null;
  };

}(this));
!(function() {

  function SignalError(code, reason) {
    this.code = code;
    this.reason = reason;
  }

  // The Dispatcher bit is purely to make testing simpler, it defaults to a new OT.Raptor.Dispatcher
  // so in normal operation you would omit it.
  OT.Raptor.Socket = function(widgetId, messagingSocketUrl, symphonyUrl, dispatcher) {
    var _states = ['disconnected', 'connecting', 'connected', 'error', 'disconnecting'],
        _sessionId,
        _token,
        _rumor,
        _dispatcher,
        _completion;


    //// Private API
    var setState = OT.$.statable(this, _states, 'disconnected'),

        onConnectComplete = function onConnectComplete (error) {
          if (error) {
            setState('error');
          }
          else {
            setState('connected');
          }

          _completion.apply(null, arguments);
        },

        onClose = function onClose (err) {
          var reason = this.is('disconnecting') ? 'clientDisconnected' : 'networkDisconnected';

          if(err && err.code === 4001) {
            reason = 'networkTimedout';
          }

          setState('disconnected');

          _dispatcher.onClose(reason);

        }.bind(this),

        onError = function onError () {};
        // @todo what does having an error mean? Are they always fatal? Are we disconnected now?


    //// Public API

    this.connect = function (token, sessionInfo, completion) {
      if (!this.is('disconnected', 'error')) {
        OT.warn('Cannot connect the Raptor Socket as it is currently connected. You should ' +
          'disconnect first.');
        return;
      }

      setState('connecting');
      _sessionId = sessionInfo.sessionId;
      _token = token;
      _completion = completion;

      var connectionId = OT.$.uuid(),
          rumorChannel = '/v2/partner/' + OT.APIKEY + '/session/' + _sessionId;

      _rumor = new OT.Rumor.Socket(messagingSocketUrl, symphonyUrl);
      _rumor.onClose = onClose;
      _rumor.onMessage = _dispatcher.dispatch.bind(_dispatcher);

      _rumor.connect(connectionId, function(error) {
        if (error) {
          error.message = 'WebSocketConnection:' + error.code + ':' + error.message;
          onConnectComplete(error);
          return;
        }

        // we do this here to avoid getting connect errors twice
        _rumor.onError = onError;

        OT.debug('Raptor Socket connected. Subscribing to ' +
          rumorChannel + ' on ' + messagingSocketUrl);

        _rumor.subscribe([rumorChannel]);

        //connect to session
        var connectMessage = OT.Raptor.Message.connections.create(OT.APIKEY, _sessionId, _rumor.id);
        this.publish(connectMessage, {'X-TB-TOKEN-AUTH': _token}, function(error) {
          if (error) {
            error.message = 'ConnectToSession:' + error.code +
                ':Received error response to connection create message.';
            onConnectComplete(error);
            return;
          }

          this.publish( OT.Raptor.Message.sessions.get(OT.APIKEY, _sessionId),
            function (error) {
            if (error) error.message = 'GetSessionState:' + error.code +
                      ':Received error response to session read';
            onConnectComplete.apply(null, arguments);
          });
        }.bind(this));

      }.bind(this));
    };


    this.disconnect = function () {
      if (this.is('disconnected')) return;

      setState('disconnecting');
      _rumor.disconnect();
    };

    // Publishs +message+ to the Symphony app server.
    //
    // The completion handler is optional, as is the headers
    // dict, but if you provide the completion handler it must
    // be the last argument.
    //
    this.publish = function (message, headers, completion) {
      if (_rumor.isNot('connected')) {
        OT.error('OT.Raptor.Socket: cannot publish until the socket is connected.' + message);
        return;
      }

      var transactionId = OT.$.uuid(),
          _headers = {},
          _completion;

      // Work out if which of the optional arguments (headers, completion)
      // have been provided.
      if (headers) {
        if (OT.$.isFunction(headers)) {
          _headers = {};
          _completion = headers;
        }
        else {
          _headers = headers;
        }
      }
      if (!_completion && completion && OT.$.isFunction(completion)) _completion = completion;


      if (_completion) _dispatcher.registerCallback(transactionId, _completion);

      OT.debug('OT.Raptor.Socket Publish (ID:' + transactionId + ') ');
      OT.debug(message);

      _rumor.publish([symphonyUrl], message, OT.$.extend(_headers, {
        'Content-Type': 'application/x-raptor+v2',
        'TRANSACTION-ID': transactionId,
        'X-TB-FROM-ADDRESS': _rumor.id
      }));

      return transactionId;
    };

    // Register a new stream against _sessionId
    this.streamCreate = function(name, orientation, encodedWidth, encodedHeight,
      hasAudio, hasVideo, frameRate, minBitrate, maxBitrate, completion) {
      var streamId = OT.$.uuid(),
          message = OT.Raptor.Message.streams.create( OT.APIKEY,
                                                      _sessionId,
                                                      streamId,
                                                      name,
                                                      orientation,
                                                      encodedWidth,
                                                      encodedHeight,
                                                      hasAudio,
                                                      hasVideo,
                                                      frameRate,
                                                      minBitrate,
                                                      maxBitrate);

      this.publish(message, function(error, message) {
        completion(error, streamId, message);
      });
    };

    this.streamDestroy = function(streamId) {
      this.publish( OT.Raptor.Message.streams.destroy(OT.APIKEY, _sessionId, streamId) );
    };

    this.streamChannelUpdate = function(streamId, channelId, attributes) {
      this.publish( OT.Raptor.Message.streamChannels.update(OT.APIKEY, _sessionId,
        streamId, channelId, attributes) );
    };

    this.subscriberCreate = function(streamId, subscriberId, channelsToSubscribeTo, completion) {
      this.publish( OT.Raptor.Message.subscribers.create(OT.APIKEY, _sessionId,
        streamId, subscriberId, _rumor.id, channelsToSubscribeTo), completion );
    };

    this.subscriberDestroy = function(streamId, subscriberId) {
      this.publish( OT.Raptor.Message.subscribers.destroy(OT.APIKEY, _sessionId,
        streamId, subscriberId) );
    };

    this.subscriberUpdate = function(streamId, subscriberId, attributes) {
      this.publish( OT.Raptor.Message.subscribers.update(OT.APIKEY, _sessionId,
        streamId, subscriberId, attributes) );
    };

    this.subscriberChannelUpdate = function(streamId, subscriberId, channelId, attributes) {
      this.publish( OT.Raptor.Message.subscriberChannels.update(OT.APIKEY, _sessionId,
        streamId, subscriberId, channelId, attributes) );
    };

    this.forceDisconnect = function(connectionIdToDisconnect, completion) {
      this.publish( OT.Raptor.Message.connections.destroy(OT.APIKEY, _sessionId,
        connectionIdToDisconnect), completion );
    };

    this.forceUnpublish = function(streamIdToUnpublish, completion) {
      this.publish( OT.Raptor.Message.streams.destroy(OT.APIKEY, _sessionId,
        streamIdToUnpublish), completion );
    };

    this.jsepCandidate = function(streamId, candidate) {
      this.publish(
        OT.Raptor.Message.streams.candidate(OT.APIKEY, _sessionId, streamId, candidate)
      );
    };

    this.jsepCandidateP2p = function(streamId, subscriberId, candidate) {
      this.publish(
        OT.Raptor.Message.subscribers.candidate(OT.APIKEY, _sessionId, streamId,
          subscriberId, candidate)
      );
    };

    this.jsepOffer = function(streamId, offerSdp) {
      this.publish( OT.Raptor.Message.streams.offer(OT.APIKEY, _sessionId, streamId, offerSdp) );
    };

    this.jsepOfferP2p = function(streamId, subscriberId, offerSdp) {
      this.publish( OT.Raptor.Message.subscribers.offer(OT.APIKEY, _sessionId, streamId,
        subscriberId, offerSdp) );
    };

    this.jsepAnswer = function(streamId, answerSdp) {
      this.publish( OT.Raptor.Message.streams.answer(OT.APIKEY, _sessionId, streamId, answerSdp) );
    };

    this.jsepAnswerP2p = function(streamId, subscriberId, answerSdp) {
      this.publish( OT.Raptor.Message.subscribers.answer(OT.APIKEY, _sessionId, streamId,
        subscriberId, answerSdp) );
    };

    this.signal = function(options, completion) {
      var signal = new OT.Signal(_sessionId, _rumor.id, options || {});

      if (!signal.valid) {
        if (completion && OT.$.isFunction(completion)) {
          completion( new SignalError(signal.error.code, signal.error.reason), signal.toHash() );
        }

        return;
      }

      this.publish( signal.toRaptorMessage(), function(err) {
        var error;
        if (err) error = new SignalError(err.code, err.message);

        if (completion && OT.$.isFunction(completion)) completion(error, signal.toHash());
      });
    };

    OT.$.defineGetters(this, {
      id: function() {
        return _rumor && _rumor.id;
      },
      sessionId: function() {
        return _sessionId;
      },
      dispatcher: function() {
        return _dispatcher;
      }
    });

    if(dispatcher == null) {
      dispatcher = new OT.Raptor.Dispatcher();
    }
    _dispatcher = dispatcher;
  };

}(this));
!(function() {
  /*global EventEmitter, util*/

  // Connect error codes and reasons that Raptor can return.
  var connectErrorReasons;

  connectErrorReasons = {
    409: 'This P2P session already has 2 participants.',
    410: 'The session already has four participants.',
    1004: 'The token passed is invalid.'
  };


  OT.Raptor.Dispatcher = function () {

    if(typeof EventEmitter !== 'undefined') {
      EventEmitter.call(this);
    } else {
      OT.$.eventing(this, true);
      this.emit = this.trigger;
    }

    this.callbacks = {};
  };

  if(typeof EventEmitter !== 'undefined') {
    util.inherits(OT.Raptor.Dispatcher, EventEmitter);
  }

  OT.Raptor.Dispatcher.prototype.registerCallback = function (transactionId, completion) {
    this.callbacks[transactionId] = completion;
  };

  OT.Raptor.Dispatcher.prototype.triggerCallback = function (transactionId) {
    /*, arg1, arg2, argN-1, argN*/
    if (!transactionId) return;

    var completion = this.callbacks[transactionId];

    if (completion && OT.$.isFunction(completion)) {
      var args = Array.prototype.slice.call(arguments);
      args.shift();

      completion.apply(null, args);
    }

    delete this.callbacks[transactionId];
  };

  OT.Raptor.Dispatcher.prototype.onClose = function(reason) {
    this.emit('close', reason);
  };


  OT.Raptor.Dispatcher.prototype.dispatch = function(rumorMessage) {
    // The special casing of STATUS messages is ugly. Need to think about
    // how to better integrate this.

    if (rumorMessage.type === OT.Rumor.MessageType.STATUS) {
      OT.debug('OT.Raptor.dispatch: STATUS');
      OT.debug(rumorMessage);

      var error;

      if (rumorMessage.isError) {
        error = new OT.Error(rumorMessage.status);
      }

      this.triggerCallback(rumorMessage.transactionId, error, rumorMessage);

      return;
    }

    var message = OT.Raptor.unboxFromRumorMessage(rumorMessage);
    OT.debug('OT.Raptor.dispatch ' + message.signature);
    OT.debug(message);

    switch(message.resource) {
      case 'session':
        this.dispatchSession(message);
        break;

      case 'connection':
        this.dispatchConnection(message);
        break;

      case 'stream':
        this.dispatchStream(message);
        break;

      case 'stream_channel':
        this.dispatchStreamChannel(message);
        break;

      case 'subscriber':
        this.dispatchSubscriber(message);
        break;

      case 'subscriber_channel':
        this.dispatchSubscriberChannel(message);
        break;

      case 'signal':
        this.dispatchSignal(message);
        break;

      case 'archive':
        this.dispatchArchive(message);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: Type ' + message.resource + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchSession = function (message) {
    switch (message.method) {
      case 'read':

        this.emit('session#read', message.content, message.transactionId);
        break;


      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchConnection = function (message) {

    switch (message.method) {
      case 'created':
        this.emit('connection#created', message.content);
        break;


      case 'deleted':
        this.emit('connection#deleted', message.params.connection, message.reason);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchStream = function (message) {

    switch (message.method) {
      case 'created':
        this.emit('stream#created', message.content, message.transactionId);
        break;

      case 'deleted':
        this.emit('stream#deleted', message.params.stream,
          message.reason);
        break;


      case 'updated':
        this.emit('stream#updated', message.params.stream,
          message.content);
        break;


      // The JSEP process
      case 'generateoffer':
      case 'answer':
      case 'pranswer':
      case 'offer':
      case 'candidate':
        this.dispatchJsep(message.method, message);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchStreamChannel = function (message) {
    switch (message.method) {
      case 'updated':
        this.emit('streamChannel#updated', message.params.stream,
          message.params.channel, message.content);
        break;

      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  // Dispatch JSEP messages
  //
  // generateoffer:
  // Request to generate a offer for another Peer (or Prism). This kicks
  // off the JSEP process.
  //
  // answer:
  // generate a response to another peers offer, this contains our constraints
  // and requirements.
  //
  // pranswer:
  // a provisional answer, i.e. not the final one.
  //
  // candidate
  //
  //
  OT.Raptor.Dispatcher.prototype.dispatchJsep = function (method, message) {
    this.emit('jsep#' + method, message.params.stream, message.fromAddress, message);
  };


  OT.Raptor.Dispatcher.prototype.dispatchSubscriberChannel = function (message) {
    switch (message.method) {
      case 'updated':
        this.emit('subscriberChannel#updated', message.params.stream,
          message.params.channel, message.content);
        break;


      case 'update': // subscriberId, streamId, content
        this.emit('subscriberChannel#update', message.params.subscriber,
          message.params.stream, message.content);
        break;


      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchSubscriber = function (message) {

    switch (message.method) {
      case 'created':
        this.emit('subscriber#created', message.params.stream, message.fromAddress,
          message.content.id);
        break;


      case 'deleted':
        this.dispatchJsep('unsubscribe', message);
        this.emit('subscriber#deleted', message.params.stream,
          message.fromAddress);
        break;


      // The JSEP process
      case 'generateoffer':
      case 'answer':
      case 'pranswer':
      case 'offer':
      case 'candidate':
        this.dispatchJsep(message.method, message);
        break;


      default:
        OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
    }
  };

  OT.Raptor.Dispatcher.prototype.dispatchSignal = function (message) {
    if (message.method !== 'signal') {
      OT.warn('OT.Raptor.dispatch: ' + message.signature + ' is not currently implemented');
      return;
    }
    this.emit('signal', message.fromAddress, message.content.type,
      message.content.data);
  };

  OT.Raptor.Dispatcher.prototype.dispatchArchive = function (message) {
    switch (message.method) {
      case 'created':
        this.emit('archive#created', message.content);
        break;
      
      case 'updated':
        this.emit('archive#updated', message.params.archive, message.content);
        break;
    }
  };

}(this));
(function(window) {

  // @todo hide these
  OT.publishers = new OT.Collection('guid');          // Publishers are id'd by their guid
  OT.subscribers = new OT.Collection('widgetId');     // Subscribers are id'd by their widgetId
  OT.sessions = new OT.Collection();

  function parseStream(dict, session) {
    var channel = dict.channel.map(function(channel) {
      return new OT.StreamChannel(channel);
    });

    return  new OT.Stream(  dict.id,
                            dict.name,
                            dict.creationTime,
                            session.connections.get(dict.connection.id),
                            session,
                            channel );
  }

  function parseAndAddStreamToSession(dict, session) {
    if (session.streams.has(dict.id)) return;

    var stream = parseStream(dict, session);
    session.streams.add( stream );

    return stream;
  }

  function parseArchive(dict) {
    return new OT.Archive( dict.id,
                           dict.name,
                           dict.status );
  }

  function parseAndAddArchiveToSession(dict, session) {
    if (session.archives.has(dict.id)) return;

    var archive = parseArchive(dict);
    session.archives.add(archive);

    return archive;
  }

  window.OT.SessionDispatcher = function(session) {

    var dispatcher = new OT.Raptor.Dispatcher();

    dispatcher.on('close', function(reason) {

      var connection = session.connection;

      if (!connection) {
        return;
      }

      if (connection.destroyedReason) {
        OT.debug('OT.Raptor.Socket: Socket was closed but the connection had already ' +
          'been destroyed. Reason: ' + connection.destroyedReason);
        return;
      }

      connection.destroy( reason );

    });

    dispatcher.on('session#read', function(content, transactionId) {

      var state = {},
          connection;

      state.streams = [];
      state.connections = [];
      state.archives = [];

      content.connection.forEach(function(connectionParams) {
        connection = OT.Connection.fromHash(connectionParams);
        state.connections.push(connection);
        session.connections.add(connection);
      });

      content.stream.forEach(function(streamParams) {
        state.streams.push( parseAndAddStreamToSession(streamParams, session) );
      });
      
      (content.archive || content.archives).forEach(function(archiveParams) {
        state.archives.push( parseAndAddArchiveToSession(archiveParams, session) );
      });

      session._.subscriberMap = {};

      dispatcher.triggerCallback(transactionId, null, state);
    });

    dispatcher.on('connection#created', function(connection) {
      connection = OT.Connection.fromHash(connection);
      if (session.connection && connection.id !== session.connection.id) {
        session.connections.add( connection );
      }
    });

    dispatcher.on('connection#deleted', function(connection, reason) {
      connection = session.connections.get(connection);
      connection.destroy(reason);
    });

    dispatcher.on('stream#created', function(stream, transactionId) {
      stream = parseAndAddStreamToSession(stream, session);

      if (stream.publisher) {
        stream.publisher.stream = stream;
      }

      dispatcher.triggerCallback(transactionId, null, stream);
    });

    dispatcher.on('stream#deleted', function(streamId, reason) {
      var stream = session.streams.get(streamId);

      if (!stream) {
        OT.error('OT.Raptor.dispatch: A stream does not exist with the id of ' +
          streamId + ', for stream#deleted message!');
        // @todo error
        return;
      }

      stream.destroy(reason);
    });

    dispatcher.on('stream#updated', function(streamId, content) {
      var stream = session.streams.get(streamId);

      if (!stream) {
        OT.error('OT.Raptor.dispatch: A stream does not exist with the id of ' +
          streamId + ', for stream#updated message!');
        // @todo error
        return;
      }

      stream._.update(content);

    });

    dispatcher.on('streamChannel#updated', function(streamId, channelId, content) {
      var stream;
      if (!(streamId && (stream = session.streams.get(streamId)))) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does not ' +
          'exist, for streamChannel message!');
        // @todo error
        return;
      }
      stream._.updateChannel(channelId, content);
    });

    // Dispatch JSEP messages
    //
    // generateoffer:
    // Request to generate a offer for another Peer (or Prism). This kicks
    // off the JSEP process.
    //
    // answer:
    // generate a response to another peers offer, this contains our constraints
    // and requirements.
    //
    // pranswer:
    // a provisional answer, i.e. not the final one.
    //
    // candidate
    //
    //
    var jsepHandler = function(method, streamId, fromAddress, message) {

      var fromConnection,
          actors;

      switch (method) {
        // Messages for Subscribers
        case 'offer':
          actors = [];
          var subscriber = OT.subscribers.find({streamId: streamId});
          if (subscriber) actors.push(subscriber);
          break;


        // Messages for Publishers
        case 'answer':
        case 'pranswer':
        case 'generateoffer':
        case 'unsubscribe':
          console.warn('generateoffer maybe?');
          actors = OT.publishers.where({streamId: streamId});
          break;


        // Messages for Publishers and Subscribers
        case 'candidate':
          // send to whichever of your publisher or subscribers are
          // subscribing/publishing that stream
          actors = OT.publishers.where({streamId: streamId})
            .concat(OT.subscribers.where({streamId: streamId}));
          break;


        default:
          OT.warn('OT.Raptor.dispatch: jsep#' + method +
            ' is not currently implemented');
          return;
      }

      if (actors.length === 0) return;

      // This is a bit hacky. We don't have the session in the message so we iterate
      // until we find the actor that the message relates to this stream, and then
      // we grab the session from it.
      fromConnection = actors[0].session.connections.get(fromAddress);
      if(!fromConnection && fromAddress.match(/^symphony\./)) {
        fromConnection = OT.Connection.fromHash({
          id: fromAddress,
          creationTime: Date.now()
        });

        actors[0].session.connections.add(fromConnection);
      } else if(!fromConnection) {
        OT.warn('OT.Raptor.dispatch: Messsage comes from a connection (' +
          fromAddress + ') that we do not know about. The message was ignored.');
        return;
      }

      actors.forEach(function(actor) {
        actor.processMessage(method, fromConnection, message);
      });
    };

    dispatcher.on('jsep#offer', jsepHandler.bind(null, 'offer'));
    dispatcher.on('jsep#answer', jsepHandler.bind(null, 'answer'));
    dispatcher.on('jsep#pranswer', jsepHandler.bind(null, 'pranswer'));
    dispatcher.on('jsep#generateoffer', jsepHandler.bind(null, 'generateoffer'));
    dispatcher.on('jsep#unsubscribe', jsepHandler.bind(null, 'unsubscribe'));
    dispatcher.on('jsep#candidate', jsepHandler.bind(null, 'candidate'));

    dispatcher.on('subscriberChannel#updated', function(streamId, channelId, content) {

      if (!streamId || !session.streams.has(streamId)) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does not ' +
          'exist, for subscriberChannel#updated message!');
        // @todo error
        return;
      }

      session.streams.get(streamId)._
        .updateChannel(channelId, content);

    });

    dispatcher.on('subscriberChannel#update', function(subscriberId, streamId, content) {

      if (!streamId || !session.streams.has(streamId)) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does not ' +
          'exist, for subscriberChannel#update message!');
        // @todo error
        return;
      }

      // Hint to update for congestion control from the Media Server
      if (!OT.subscribers.has(subscriberId)) {
        OT.error('OT.Raptor.dispatch: Unable to determine subscriberId, or the subscriber ' +
          'does not exist, for subscriberChannel#update message!');
        // @todo error
        return;
      }

      // We assume that an update on a Subscriber channel is to disableVideo
      // we may need to be more specific in the future
      OT.subscribers.get(subscriberId).disableVideo(content.active);

    });

    dispatcher.on('subscriber#created', function(streamId, fromAddress, subscriberId) {

      var stream = streamId ? session.streams.get(streamId) : null;

      if (!stream) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does ' +
          'not exist, for subscriber#created message!');
        // @todo error
        return;
      }

      session._.subscriberMap[fromAddress + '_' + stream.id] = subscriberId;
    });

    dispatcher.on('subscriber#deleted', function(streamId, fromAddress) {
      var stream = streamId ? session.streams.get(streamId) : null;

      if (!stream) {
        OT.error('OT.Raptor.dispatch: Unable to determine streamId, or the stream does ' +
          'not exist, for subscriber#created message!');
        // @todo error
        return;
      }

      delete session._.subscriberMap[fromAddress + '_' + stream.id];
    });

    dispatcher.on('signal', function(fromAddress, signalType, data) {
      session._.dispatchSignal(session.connections.get(fromAddress),
                               signalType, data);
    });

    dispatcher.on('archive#created', function(archive) {
      parseAndAddArchiveToSession(archive, session);
    });

    dispatcher.on('archive#updated', function(archiveId, update) {
      var archive = session.archives.get(archiveId);

      if (!archive) {
        OT.error('OT.Raptor.dispatch: An archive does not exist with the id of ' +
          archiveId + ', for archive#updated message!');
        // @todo error
        return;
      }

      archive._.update(update);
    });

    return dispatcher;

  };

})(window);
!(function(window) {

  // Helper to synchronise several startup tasks and then dispatch a unified
  // 'envLoaded' event.
  //
  // This depends on:
  // * OT
  // * OT.Config
  //
  function EnvironmentLoader() {
    var _configReady = false,
        _domReady = false,

        isReady = function() {
          return _domReady && _configReady;
        },

        onLoaded = function() {
          if (isReady()) {
            OT.dispatchEvent(new OT.EnvLoadedEvent(OT.Event.names.ENV_LOADED));
          }
        },

        onDomReady = function() {
          _domReady = true;

          // This is making an assumption about there being only one "window"
          // that we care about.
          OT.$.on(window, 'unload', function() {
            OT.publishers.destroy();
            OT.subscribers.destroy();
            OT.sessions.destroy();
          });

          // The Dynamic Config won't load until the DOM is ready
          OT.Config.load(OT.properties.configURL);

          onLoaded();
        },

        configLoaded = function() {
          _configReady = true;
          OT.Config.off('dynamicConfigChanged', configLoaded);
          OT.Config.off('dynamicConfigLoadFailed', configLoadFailed);

          onLoaded();
        },

        configLoadFailed = function() {
          configLoaded();
        };

    OT.Config.on('dynamicConfigChanged', configLoaded);
    OT.Config.on('dynamicConfigLoadFailed', configLoadFailed);
    if (document.readyState === 'complete' ||
      (document.readyState === 'interactive' && document.body)) {
      onDomReady();
    } else {
      if (document.addEventListener) {
        document.addEventListener('DOMContentLoaded', onDomReady, false);
      } else if (document.attachEvent) {
        // This is so onLoad works in IE, primarily so we can show the upgrade to Chrome popup
        document.attachEvent('onreadystatechange', function() {
          if (document.readyState === 'complete') onDomReady();
        });
      }
    }

    this.onLoad = function(cb) {
      if (isReady()) {
        cb();
        return;
      }

      OT.on(OT.Event.names.ENV_LOADED, cb);
    };
  }

  var EnvLoader = new EnvironmentLoader();

  OT.onLoad = function(cb, context) {
    if (!context) {
      EnvLoader.onLoad(cb);
    } else {
      EnvLoader.onLoad(
        cb.bind(context)
      );
    }
  };

})(window);
!(function() {

  /**
   * The Error class is used to define the error object passed into completion handlers.
   * Each of the following methods, which execute asynchronously, includes a
   * <code>completionHandler</code> parameter:
   *
   * <ul>
   *   <li><a href="Session.html#connect">Session.connect()</a></li>
   *   <li><a href="Session.html#forceDisconnect">Session.forceDisconnect()</a></li>
   *   <li><a href="Session.html#forceUnpublish">Session.forceUnpublish()</a></li>
   *   <li><a href="Session.html#publish">Session.publish()</a></li>
   *   <li><a href="Session.html#subscribe">Session.subscribe()</a></li>
   *   <li><a href="OT.html#initPublisher">OT.initPublisher()</a></li>
   * </ul>
   *
   * <p>
   * The <code>completionHandler</code> parameter is a function that is called when the call to
   * the asynchronous method succeeds or fails. If the asynchronous call fails, the completion
   * handler function is passes an error object (defined by the Error class). The <code>code</code>
   * and <code>message</code> properties of the error object provide details about the error.
   *
   * @property {Number} code The error code, defining the error.
   *
   *   <p>
   *   In the event of an error, the <code>code</code> value of the <code>error</code> parameter can
   *   have one of the following values:
   * </p>
   *
   * <p>Errors when calling <code>Session.connect()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1004</td>
   *   <td>Authentication error. Check the error message for details. This error can result if you
   *     in an expired token when trying to connect to a session. It can also occur if you pass
   *     in an invalid token or API key. Make sure that you are generating the token using the
   *     current version of one of the
   *     <a href="(http://tokbox.com/opentok/libraries/server">OpenTok server SDKs</a>.</td>
   * </tr>
   * <tr>
   *   <td>1005</td>
   *   <td>Invalid Session ID. Make sure you generate the session ID using the current version of
   *     one of the <a href="(http://tokbox.com/opentok/libraries/server">OpenTok server
   *     SDKs</a>.</td>
   * </tr>
   * <tr>
   *   <td>1006</td>
   *   <td>Connect Failed. Unable to connect to the session. You may want to have the client check
   *     the network connection.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.forceDisconnect()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b>
   *       <code>code</code>
   *     </b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1010</td>
   *   <td>The client is not connected to the OpenTok session. Check that client connects
   *     successfully and has not disconnected before calling forceDisconnect().</td>
   * </tr>
   * <tr>
   *   <td>1520</td>
   *   <td>Unable to force disconnect. The client's token does not have the role set to moderator.
   *     Once the client has connected to the session, the <code>capabilities</code> property of
   *     the Session object lists the client's capabilities.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.forceUnpublish()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1010</td>
   *   <td>The client is not connected to the OpenTok session. Check that client connects
   *     successfully and has not disconnected before calling forceUnpublish().</td>
   * </tr>
   * <tr>
   *   <td>1530</td>
   *   <td>Unable to force unpublish. The client's token does not have the role set to moderator.
   *     Once the client has connected to the session, the <code>capabilities</code> property of
   *     the Session object lists the client's capabilities.</td>
   * </tr>
   * <tr>
   *   <td>1535</td>
   *   <td>Force Unpublish on an invalid stream. Make sure that the stream has not left the
   *     session before you call the <code>forceUnpublish()</code> method.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.publish()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1010</td>
   *   <td>The client is not connected to the OpenTok session. Check that the client connects
   *     successfully before trying to publish. And check that the client has not disconnected
   *     before trying to publish.</td>
   * </tr>
   * <tr>
   *   <td>1500</td>
   *   <td>Unable to Publish. The client's token does not have the role set to to publish or
   *     moderator. Once the client has connected to the session, the <code>capabilities</code>
   *     property of the Session object lists the client's capabilities.</td>
   * </tr>
   * <tr>
   *   <td>1601</td>
   *   <td>Internal error -- WebRTC publisher error. Try republishing or reconnecting to the
   *     session.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.signal()</code>:</p>
   *
   * <table class="docs_table" style="width:100%"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *     <td>400</td>
   *     <td>One of the signal properties &mdash; data, type, or to &mdash;
   *                 is invalid. Or the data cannot be parsed as JSON.</td>
   * </tr>
   * <tr>
   *   <td>404</td> <td>The to connection does not exist.</td>
   * </tr>
   * <tr>
   *   <td>413</td> <td>The type string exceeds the maximum length (128 bytes),
   *     or the data string exceeds the maximum size (8 kB).</td>
   * </tr>
   * <tr>
   *   <td>500</td>
   *   <td>The client is not connected to the OpenTok session. Check that the client connects
   *     successfully before trying to signal. And check that the client has not disconnected before
   *     trying to publish.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>Session.subscribe()</code>:</p>
   *
   * <table class="docs_table" style="width:100%"><tbody>
   * <tr>
   *   <td><b>
   *       Errors when calling <code>Session.subscribe()</code>:
   *     </b></td>
   * </tr>
   * <tr>
   *   <td><b>
   *       <code>code</code>
   *     </b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1600</td>
   *   <td>Internal error -- WebRTC subscriber error. Try resubscribing to the stream or
   *     reconnecting to the session.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>Errors when calling <code>TB.initPublisher()</code>:</p>
   *
   * <table class="docs_table"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1004</td>
   *   <td>Authentication error. Check the error message for details. This error can result if you
   *     pass in an expired token when trying to connect to a session. It can also occur if you
   *     pass in an invalid token or API key. Make sure that you are generating the token using
   *     the current version of one of the
   *     <a href="(http://tokbox.com/opentok/libraries/server">OpenTok server SDKs</a>.</td>
   * </tr>
   * </tbody></table>
   *
   * <p>General errors that can occur when calling any method:</p>
   *
   * <table class="docs_table" style="width:100%"><tbody>
   * <tr>
   *   <td><b><code>code</code></b></td>
   *   <td><b>Description</b></td>
   * </tr>
   * <tr>
   *   <td>1011</td>
   *   <td>Invalid Parameter. Check that you have passed valid parameter values into the method
   *     call.</td>
   * </tr>
   * <tr>
   *   <td>2000</td>
   *   <td>Internal Error. Try reconnecting to the OpenTok session and trying the action again.</td>
   * </tr>
   * </tbody></table>
   *
   * @property {String} message The message string provides details about the error.
   *
   * @class Error
   * @augments Event
   */
  OT.Error = function(code, message) {
    this.code = code;
    this.message = message;
  };

  var errorsCodesToTitle = {
    1004: 'Authentication error',
    1005: 'Invalid Session ID',
    1006: 'Connect Failed',
    1007: 'Connect Rejected',
    1008: 'Connect Time-out',
    1009: 'Security Error',
    1010: 'Not Connected',
    1011: 'Invalid Parameter',
    1012: 'Peer-to-peer Stream Play Failed',
    1013: 'Connection Failed',
    1014: 'API Response Failure',
    1500: 'Unable to Publish',
    1520: 'Unable to Force Disconnect',
    1530: 'Unable to Force Unpublish',
    2000: 'Internal Error',
    2001: 'Embed Failed',
    4000: 'WebSocket Connection Failed',
    4001: 'WebSocket Network Disconnected'
  };

  var analytics;

  function _exceptionHandler(component, msg, errorCode, context) {
    var title = errorsCodesToTitle[errorCode],
        contextCopy = context ? OT.$.clone(context) : {};

    OT.error('OT.exception :: title: ' + title + ' (' + errorCode + ') msg: ' + msg);

    if (!contextCopy.partnerId) contextCopy.partnerId = OT.APIKEY;

    try {
      if (!analytics) analytics = new OT.Analytics();
      analytics.logError(errorCode, 'tb.exception', title, {details:msg}, contextCopy);

      OT.dispatchEvent(
        new OT.ExceptionEvent(OT.Event.names.EXCEPTION, msg, title, errorCode, component, component)
      );
    } catch(err) {
      OT.error('OT.exception :: Failed to dispatch exception - ' + err.toString());
      // Don't throw an error because this is asynchronous
      // don't do an exceptionHandler because that would be recursive
    }
  }

// @todo redo this when we have time to tidy up
//
// @example
//
//  OT.handleJsException("Descriptive error message", 2000, {
//      session: session,
//      target: stream|publisher|subscriber|session|etc
//  });
//
  OT.handleJsException = function(errorMsg, code, options) {
    options = options || {};

    var context,
        session = options.session;

    if (session) {
      context = {
        sessionId: session.sessionId
      };

      if (session.connected) context.connectionId = session.connection.connectionId;
      if (!options.target) options.target = session;

    } else if (options.sessionId) {
      context = {
        sessionId: options.sessionId
      };

      if (!options.target) options.target = null;
    }

    _exceptionHandler(options.target, errorMsg, code, context);
  };


// @todo redo this when we have time to tidy up
//
// Public callback for exceptions from Flash.
//
// Called from Flash like:
//  OT.exceptionHandler('publisher_1234,1234',
//  "Descriptive Error Message", "Error Title", 2000, contextObj)
//
  OT.exceptionHandler = function(componentId, msg, errorTitle, errorCode, context) {
    var target;

    if (componentId) {
      target = OT.components[componentId];

      if (!target) {
        OT.warn('Could not find the component with component ID ' + componentId);
      }
    }

    _exceptionHandler(target, msg, errorCode, context);
  };


  // This is a placeholder until error handling can be rewritten
  OT.dispatchError = function (code, message, completionHandler, session) {
    OT.error(code, message);

    if (completionHandler && OT.$.isFunction(completionHandler)) {
      completionHandler.call(null, new OT.Error(code, message));
    }

    OT.handleJsException(message, code, {
      session: session
    });
  };

})(window);
!(function() {

  OT.ConnectionCapabilities = function(capabilitiesHash) {
    // Private helper methods
    var castCapabilities = function(capabilitiesHash) {
      capabilitiesHash.supportsWebRTC = OT.$.castToBoolean(capabilitiesHash.supportsWebRTC);
      return capabilitiesHash;
    };

    // Private data
    var _caps = castCapabilities(capabilitiesHash);
    this.supportsWebRTC = _caps.supportsWebRTC;
  };

})(window);
!(function() {

  /**
   * The Connection object represents a connection to an OpenTok session. Each client that connects
   * to a session has a unique connection, with a unique connection ID (represented by the
   * <code>id</code> property of the Connection object for the client).
   * <p>
   * The Session object has a <code>connection</code> property that is a Connection object.
   * It represents the local client's connection. (A client only has a connection once the
   * client has successfully called the <code>connect()</code> method of the {@link Session}
   * object.)
   * <p>
   * The Session object dispatches a <code>connectionCreated</code> event when each client (other
   * than your own) connects to a session (and for clients that are present in the session when you
   * connect). The <code>connectionCreated</code> event object has a <code>connection</code>
   * property, which is a Connection object corresponding to the client the event pertains to.
   * <p>
   * The Stream object has a <code>connection</code> property that is a Connection object.
   * It represents the connection of the client that is publishing the stream.
   *
   * @class Connection
   * @property {String} connectionId The ID of this connection.
   * @property {Number} creationTime The timestamp for the creation of the connection. This
   * value is calculated in milliseconds.
   * You can convert this value to a Date object by calling <code>new Date(creationTime)</code>,
   * where <code>creationTime</code>
   * is the <code>creationTime</code> property of the Connection object.
   * @property {String} data A string containing metadata describing the
   * connection. When you generate a user token string pass the connection data string to the
   * <code>generate_token()</code> method of our
   * <a href="/opentok/libraries/server/">server-side libraries</a>. You can also generate a token
   * and define connection data on the
   * <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page.
   */
  OT.Connection = function(id, creationTime, data, capabilitiesHash, permissionsHash) {
    var destroyedReason;

    this.id = this.connectionId = id;
    this.creationTime = creationTime ? Number(creationTime) : null;
    this.data = data;
    this.capabilities = new OT.ConnectionCapabilities(capabilitiesHash);
    this.permissions = new OT.Capabilities(permissionsHash);
    this.quality = null;

    OT.$.eventing(this);

    this.destroy = function(reason, quiet) {
      destroyedReason = reason || 'clientDisconnected';

      if (quiet !== true) {
        this.dispatchEvent(
          new OT.DestroyedEvent(
            'destroyed',      // This should be OT.Event.names.CONNECTION_DESTROYED, but
                              // the value of that is currently shared with Session
            this,
            destroyedReason
          )
        );
      }
    }.bind(this);

    Object.defineProperties(this, {
      destroyed: {
        get: function() { return destroyedReason !== void 0; },
        enumerable: true
      },

      destroyedReason: {
        get: function() { return destroyedReason; },
        enumerable: true
      }
    });
  };

  OT.Connection.fromHash = function(hash) {
    return new OT.Connection(hash.id,
                             hash.creationTime,
                             hash.data,
                             OT.$.extend(hash.capablities || {}, { supportsWebRTC: true }),
                             hash.permissions || [] );
  };

})(window);
!(function() {

  // id: String                           | mandatory | immutable
  // type: String {video/audio/data/...}  | mandatory | immutable
  // active: Boolean                      | mandatory | mutable
  // orientation: Integer?                | optional  | mutable
  // frameRate: Float                     | optional  | mutable
  // height: Integer                      | optional  | mutable
  // width: Integer                       | optional  | mutable
  OT.StreamChannel = function(options) {
    this.id = options.id;
    this.type = options.type;
    this.active = OT.$.castToBoolean(options.active);
    this.orientation = options.orientation || OT.VideoOrientation.ROTATED_NORMAL;
    if (options.frameRate) this.frameRate = parseFloat(options.frameRate, 10);
    this.width = parseInt(options.width, 10);
    this.height = parseInt(options.height, 10);

    OT.$.eventing(this, true);

    // Returns true if a property was updated.
    this.update = function(attributes) {
      var videoDimensions = {},
          oldVideoDimensions = {};

      for (var key in attributes) {
        // we shouldn't really read this before we know the key is valid
        var oldValue = this[key];

        switch(key) {
          case 'active':
            this.active = OT.$.castToBoolean(attributes[key]);
            break;

          case 'frameRate':
            this.frameRate = parseFloat(attributes[key], 10);
            break;

          case 'width':
          case 'height':
            this[key] = parseInt(attributes[key], 10);

            videoDimensions[key] = this[key];
            oldVideoDimensions[key] = oldValue;
            break;

          case 'orientation':
            this[key] = attributes[key];

            videoDimensions[key] = this[key];
            oldVideoDimensions[key] = oldValue;
            break;

          default:
            OT.warn('Tried to update unknown key ' + key + ' on ' + this.type +
              ' channel ' + this.id);
            return;
        }

        this.trigger('update', this, key, oldValue, this[key]);
      }

      if (Object.keys(videoDimensions).length) {
        // To make things easier for the public API, we broadcast videoDimensions changes,
        // which is an aggregate of width, height, and orientation changes.
        this.trigger('update', this, 'videoDimensions', oldVideoDimensions, videoDimensions);
      }

      return true;
    };
  };

})(window);
!(function() {

  var validPropertyNames = ['name', 'archiving'];

/**
 * Specifies a stream. A stream is a representation of a published stream in a session. When a
 * client calls the <a href="Session.html#publish">Session.publish() method</a>, a new stream is
 * created. Properties of the Stream object provide information about the stream.
 *
 *  <p>When a stream is added to a session, the Session object dispatches a
 * <code>streamCreatedEvent</code>. When a stream is destroyed, the Session object dispatches a
 * <code>streamDestroyed</code> event. The StreamEvent object, which defines these event objects,
 * has a <code>stream</code> property, which is an array of Stream object. For details and a code
 * example, see {@link StreamEvent}.</p>
 *
 *  <p>When a connection to a session is made, the Session object dispatches a
 * <code>sessionConnected</code> event, defined by the SessionConnectEvent object. The
 * SessionConnectEvent object has a <code>streams</code> property, which is an array of Stream
 * objects pertaining to the streams in the session at that time. For details and a code example,
 * see {@link SessionConnectEvent}.</p>
 *
 * @class Stream
 * @property {Connection} connection The Connection object corresponding
 * to the connection that is publishing the stream. You can compare this to to the
 * <code>connection</code> property of the Session object to see if the stream is being published
 * by the local web page.
 *
 * @property {Number} creationTime The timestamp for the creation
 * of the stream. This value is calculated in milliseconds. You can convert this value to a
 * Date object by calling <code>new Date(creationTime)</code>, where <code>creationTime</code> is
 * the <code>creationTime</code> property of the Stream object.
 *
 * @property {Number} fps The frame rate of the video stream.
 *
 * @property {Boolean} hasAudio Whether the stream has audio. This property can change if the
 * publisher turns on or off audio (by calling
 * <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a>). When this occurs, the
 * {@link Session} object dispatches a <code>streamPropertyChanged</code> event (see
 * {@link StreamPropertyChangedEvent}.)
 *
 * @property {Boolean} hasVideo Whether the stream has video. This property can change if the
 * publisher turns on or off video (by calling
 * <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a>). When this occurs, the
 * {@link Session} object dispatches a <code>streamPropertyChanged</code> event (see
 * {@link StreamPropertyChangedEvent}.)
 *
 * @property {Object} videoDimensions This object has two properties: <code>width</code> and
 * <code>height</code>. Both are numbers. The <code>width</code> property is the width of the
 * encoded stream; the <code>height</code> property is the height of the encoded stream. (These
 * are independent of the actual width of Publisher and Subscriber objects corresponding to the
 * stream.) This property can change if a stream
 * published from an iOS device resizes, based on a change in the device orientation. When this
 * occurs, the {@link Session} object dispatches a <code>streamPropertyChanged</code> event (see
 * {@link StreamPropertyChangedEvent}.)
 *
 * @property {String} name The name of the stream. Publishers can specify a name when publishing
 * a stream (using the <code>publish()</code> method of the publisher's Session object).
 *
 * @property {String} streamId The unique ID of the stream.
 */


  OT.Stream = function(id, name, creationTime, connection, session, channel) {
    var destroyedReason;

    this.id = this.streamId = id;
    this.name = name;
    this.creationTime = Number(creationTime);

    this.connection = connection;
    this.channel = channel;
    this.publisher = OT.publishers.find({streamId: this.id});
    this.publisherId = this.publisher ? this.publisher.id : null;

    OT.$.eventing(this);

    var onChannelUpdate = function(channel, key, oldValue, newValue) {
      var _key = key;

      switch(_key) {
        case 'active':
          _key = channel.type === 'audio' ? 'hasAudio' : 'hasVideo';
          this[_key] = newValue;
          break;

        case 'orientation':
        case 'width':
        case 'height':
          this.videoDimensions = {
            width: channel.width,
            height: channel.height,
            orientation: channel.orientation
          };

          // We dispatch this via the videoDimensions key instead
          return;
      }

      this.dispatchEvent( new OT.StreamUpdatedEvent(this, _key, oldValue, newValue) );
    }.bind(this);

    var associatedWidget = function() {
      if(this.publisher) {
        return this.publisher;
      } else {
        return OT.subscribers.find(function(subscriber) {
          return subscriber.streamId === this.id &&
            subscriber.session.id === session.id;
        });
      }
    }.bind(this);

    // Returns all channels that have a type of +type+.
    this.getChannelsOfType = function (type) {
      return this.channel.filter(function(channel) {
        return channel.type === type;
      });
    };

    this.getChannel = function (id) {
      for (var i=0; i<this.channel.length; ++i) {
        if (this.channel[i].id === id) return this.channel[i];
      }

      return null;
    };


    //// implement the following using the channels
    // hasAudio
    // hasVideo
    // videoDimensions

    var audioChannel = this.getChannelsOfType('audio')[0],
        videoChannel = this.getChannelsOfType('video')[0];

    // @todo this should really be: "has at least one video/audio track" instead of 
    // "the first video/audio track"
    this.hasAudio = audioChannel != null && audioChannel.active;
    this.hasVideo = videoChannel != null && videoChannel.active;

    this.videoDimensions = {};
    if (videoChannel) {
      this.videoDimensions.width = videoChannel.width;
      this.videoDimensions.height = videoChannel.height;
      this.videoDimensions.orientation = videoChannel.orientation;

      videoChannel.on('update', onChannelUpdate);
    }

    if (audioChannel) {
      audioChannel.on('update', onChannelUpdate);
    }

    this.setChannelActiveState = function(channelType, activeState, activeReason) {
      var attributes = {
        active: activeState
      };
      if (activeReason) {
        attributes.activeReason = activeReason;
      }
      updateChannelsOfType(channelType, attributes);
    };

    this.setRestrictFrameRate = function(restrict) {
      updateChannelsOfType('video', {
        restrictFrameRate: restrict
      });
    };

    var updateChannelsOfType = function(channelType, attributes) {
      var setChannelActiveState;
      if (!this.publisher) {
        var subscriber = OT.subscribers.find(function(subscriber) {
          return subscriber.streamId === this.id &&
            subscriber.session.id === session.id;
        }, this);

        setChannelActiveState = function(channel) {
          session._.subscriberChannelUpdate(this, subscriber, channel, attributes);
        };
      } else {
        setChannelActiveState = function(channel) {
          session._.streamChannelUpdate(this, channel, attributes);
        };
      }

      this.getChannelsOfType(channelType).forEach(setChannelActiveState.bind(this));
    }.bind(this);

    this.destroy = function(reason, quiet) {
      destroyedReason = reason || 'clientDisconnected';

      if (quiet !== true) {
        this.dispatchEvent(
          new OT.DestroyedEvent(
            'destroyed',      // This should be OT.Event.names.STREAM_DESTROYED, but
                              // the value of that is currently shared with Session
            this,
            destroyedReason
          )
        );
      }
    };

    Object.defineProperties(this, {
      destroyed: {
        get: function() { return destroyedReason !== void 0; },
        enumerable: true
      },

      destroyedReason: {
        get: function() { return destroyedReason; },
        enumerable: true
      },

      frameRate: {
        get: function() { return this.getChannelsOfType('video')[0].frameRate; },
        enumerable: true
      }
    });

    /// PRIVATE STUFF CALLED BY Raptor.Dispatcher

    // Confusingly, this should not be called when you want to change
    // the stream properties. This is used by Raptor dispatch to notify
    // the stream that it's properies have been successfully updated
    //
    // @todo make this sane. Perhaps use setters for the properties that can
    // send the appropriate Raptor message. This would require that Streams
    // have access to their session.
    //
    this._ = {};
    this._.updateProperty = function (key, value) {
      if (validPropertyNames.indexOf(key) === -1) {
        OT.warn('Unknown stream property "' + key + '" was modified to "' + value + '".');
        return;
      }

      var oldValue = this[key],
          newValue = value;

      switch(key) {
        case 'name':
          this[key] = newValue;
          break;

        case 'archiving':
          var widget = associatedWidget();
          if(widget) {
            widget._.archivingStatus(newValue);
          }
          this[key] = newValue;
          break;
      }

      var event = new OT.StreamUpdatedEvent(this, key, oldValue, newValue);
      this.dispatchEvent(event);
    }.bind(this);

    // Mass update, called by Raptor.Dispatcher
    this._.update = function (attributes) {
      for (var key in attributes) {
        this._.updateProperty(key, attributes[key]);
      }
    }.bind(this);

    this._.updateChannel = function (channelId, attributes) {
      this.getChannel(channelId).update(attributes);
    }.bind(this);
  };

})(window);
!(function() {
  

  OT.Archive = function(id, name, status) {
    
    this.id = id;
    this.name = name;
    this.status = status;
    
    this._ = {};

    OT.$.eventing(this);
    
    // Mass update, called by Raptor.Dispatcher
    this._.update = function (attributes) {
      for (var key in attributes) {
        var oldValue = this[key];
        this[key] = attributes[key];
        
        var event = new OT.ArchiveUpdatedEvent(this, key, oldValue, this[key]);
        this.dispatchEvent(event);
        
      }
    }.bind(this);

    this.destroy = function() {};

  };

})(window);
!(function(window) {

  // order is very important: "RTCSessionDescription" defined in Firefox Nighly but useless
  var NativeRTCSessionDescription = (window.mozRTCSessionDescription ||
    window.RTCSessionDescription);
  var NativeRTCIceCandidate = (window.mozRTCIceCandidate || window.RTCIceCandidate);

  // Helper function to forward Ice Candidates via +messageDelegate+
  var iceCandidateForwarder = function(messageDelegate) {
    return function(event) {
      if (event.candidate) {
        messageDelegate(OT.Raptor.Actions.CANDIDATE, event.candidate);
      } else {
        OT.debug('IceCandidateForwarder: No more ICE candidates.');
      }
    };
  };


  // Process incoming Ice Candidates from a remote connection (which have been
  // forwarded via iceCandidateForwarder). The Ice Candidates cannot be processed
  // until a PeerConnection is available. Once a PeerConnection becomes available
  // the pending PeerConnections can be processed by calling processPending.
  //
  // @example
  //
  //  var iceProcessor = new IceCandidateProcessor();
  //  iceProcessor.process(iceMessage1);
  //  iceProcessor.process(iceMessage2);
  //  iceProcessor.process(iceMessage3);
  //
  //  iceProcessor.peerConnection = peerConnection;
  //  iceProcessor.processPending();
  //
  var IceCandidateProcessor = function() {
    var _pendingIceCandidates = [],
        _peerConnection = null;


    Object.defineProperty(this, 'peerConnection', {
      set: function(peerConnection) {
        _peerConnection = peerConnection;
      }
    });

    this.process = function(message) {
      var iceCandidate = new NativeRTCIceCandidate(message.content);

      if (_peerConnection) {
        _peerConnection.addIceCandidate(iceCandidate);
      } else {
        _pendingIceCandidates.push(iceCandidate);
      }
    };

    this.processPending = function() {
      while(_pendingIceCandidates.length) {
        _peerConnection.addIceCandidate(_pendingIceCandidates.shift());
      }
    };
  };

  // Removes all Confort Noise from +sdp+.
  //
  // See https://jira.tokbox.com/browse/OPENTOK-7176
  //
  var removeComfortNoise = function removeComfortNoise (sdp) {
    // a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
    var matcher = /a=rtpmap:(\d+) CN\/\d+/i,
        payloadTypes = [],
        audioMediaLineIndex,
        sdpLines,
        match;

    // Icky code. This filter operation has two side effects in addition
    // to doing the actual filtering:
    //   1. extract all the payload types from the rtpmap CN lines
    //   2. find the index of the audio media line
    //
    sdpLines = sdp.split('\r\n').filter(function(line, index) {
      if (line.indexOf('m=audio') !== -1) audioMediaLineIndex = index;

      match = line.match(matcher);
      if (match !== null) {
        payloadTypes.push(match[1]);

        // remove this line as it contains CN
        return false;
      }

      return true;
    });

    if (payloadTypes.length && audioMediaLineIndex) {
      // Remove all CN payload types from the audio media line.
      sdpLines[audioMediaLineIndex] = sdpLines[audioMediaLineIndex].replace(
        new RegExp(payloadTypes.join('|'), 'ig') , '').replace(/\s+/g, ' ');
    }

    return sdpLines.join('\r\n');
  };

  // Attempt to completely process +offer+. This will:
  // * set the offer as the remote description
  // * create an answer and
  // * set the new answer as the location description
  //
  // If there are no issues, the +success+ callback will be executed on completion.
  // Errors during any step will result in the +failure+ callback being executed.
  //
  var offerProcessor = function(peerConnection, offer, success, failure) {
    var generateErrorCallback,
        setLocalDescription,
        createAnswer;

    generateErrorCallback = function(message, prefix) {
      return function(errorReason) {
        OT.error(message);
        OT.error(errorReason);

        if (failure) failure(message, errorReason, prefix);
      };
    };

    setLocalDescription = function(answer) {
      answer.sdp = removeComfortNoise(answer.sdp);

      peerConnection.setLocalDescription(
        answer,

        // Success
        function() {
          success(answer);
        },

        // Failure
        generateErrorCallback('Error while setting LocalDescription', 'SetLocalDescription')
      );
    };

    createAnswer = function() {
      peerConnection.createAnswer(
        // Success
        setLocalDescription,

        // Failure
        generateErrorCallback('Error while setting createAnswer', 'CreateAnswer'),

        null, // MediaConstraints
        false // createProvisionalAnswer
      );
    };

    // Workaround for a Chrome issue. Add in the SDES crypto line into offers
    // from Firefox
    if (offer.sdp.indexOf('a=crypto') === -1) {
      var cryptoLine = 'a=crypto:1 AES_CM_128_HMAC_SHA1_80 ' +
        'inline:FakeFakeFakeFakeFakeFakeFakeFakeFakeFake\\r\\n';

        // insert the fake crypto line for every M line
      offer.sdp = offer.sdp.replace(/^c=IN(.*)$/gmi, 'c=IN$1\r\n'+cryptoLine);
    }

    if (offer.sdp.indexOf('a=rtcp-fb') === -1) {
      var rtcpFbLine = 'a=rtcp-fb:* ccm fir\r\na=rtcp-fb:* nack ';

      // insert the fake crypto line for every M line
      offer.sdp = offer.sdp.replace(/^m=video(.*)$/gmi, 'm=video$1\r\n'+rtcpFbLine);
    }

    peerConnection.setRemoteDescription(
      offer,

      // Success
      createAnswer,

      // Failure
      generateErrorCallback('Error while setting RemoteDescription', 'SetRemoteDescription')
    );

  };

  // Attempt to completely process a subscribe message. This will:
  // * create an Offer
  // * set the new offer as the location description
  //
  // If there are no issues, the +success+ callback will be executed on completion.
  // Errors during any step will result in the +failure+ callback being executed.
  //
  var suscribeProcessor = function(peerConnection, success, failure) {
    var constraints,
        generateErrorCallback,
        setLocalDescription;

    constraints = {
      mandatory: {},
      optional: []
    },

    generateErrorCallback = function(message, prefix) {
      return function(errorReason) {
        OT.error(message);
        OT.error(errorReason);

        if (failure) failure(message, errorReason, prefix);
      };
    };

    setLocalDescription = function(offer) {
      offer.sdp = removeComfortNoise(offer.sdp);

      peerConnection.setLocalDescription(
        offer,

        // Success
        function() {
          success(offer);
        },

        // Failure
        generateErrorCallback('Error while setting LocalDescription', 'SetLocalDescription')
      );
    };

    // For interop with FireFox. Disable Data Channel in createOffer.
    if (navigator.mozGetUserMedia) {
      constraints.mandatory.MozDontOfferDataChannel = true;
    }

    peerConnection.createOffer(
      // Success
      setLocalDescription,

      // Failure
      generateErrorCallback('Error while creating Offer', 'CreateOffer'),

      constraints
    );
  };

  /*
   * Negotiates a WebRTC PeerConnection.
   *
   * Responsible for:
   * * offer-answer exchange
   * * iceCandidates
   * * notification of remote streams being added/removed
   *
   */
  OT.PeerConnection = function(config) {
    var _peerConnection,
        _iceProcessor = new IceCandidateProcessor(),
        _offer,
        _answer,
        _state = 'new',
        _messageDelegates = [],
        _gettingStats,
        _createTime = OT.$.now();

    OT.$.eventing(this);

    // if ice servers doesn't exist Firefox will throw an exception. Chrome
    // interprets this as "Use my default STUN servers" whereas FF reads it
    // as "Don't use STUN at all". *Grumble*
    if (!config.iceServers) config.iceServers = [];

    // Private methods
    var delegateMessage = function(type, messagePayload) {
          if (_messageDelegates.length) {
            // We actually only ever send to the first delegate. This is because
            // each delegate actually represents a Publisher/Subscriber that
            // shares a single PeerConnection. If we sent to all delegates it
            // would result in each message being processed multiple times by
            // each PeerConnection.
            _messageDelegates[0](type, messagePayload);
          }
        }.bind(this),

        setupPeerConnection = function() {
          if (!_peerConnection) {
            try {
              OT.debug('Creating peer connection config "' + JSON.stringify(config) + '".');
              if (!config.iceServers || config.iceServers.length === 0) {
                // This should never happen unless something is misconfigured
                OT.error('No ice servers present');
              }
              _peerConnection = OT.$.createPeerConnection(config, {
                optional: [
                  { DtlsSrtpKeyAgreement: true }
                ]
              });
            } catch(e) {
              triggerError('Failed to create PeerConnection, exception: ' +
                  e.message, 'NewPeerConnection');
              return null;
            }

            _peerConnection.onicecandidate = iceCandidateForwarder(delegateMessage);
            _peerConnection.onaddstream = onRemoteStreamAdded.bind(this);
            _peerConnection.onremovestream = onRemoteStreamRemoved.bind(this);

            if (_peerConnection.onsignalingstatechange !== undefined) {
              _peerConnection.onsignalingstatechange = routeStateChanged.bind(this);
            } else if (_peerConnection.onstatechange !== undefined) {
              _peerConnection.onstatechange = routeStateChanged.bind(this);
            }
            
            if (_peerConnection.oniceconnectionstatechange !== undefined) {
              var failedStateTimer;
              _peerConnection.oniceconnectionstatechange = function (event) {
                if (event.target.iceConnectionState === 'failed') {
                  if (failedStateTimer) {
                    clearTimeout(failedStateTimer);
                  }
                  // We wait 5 seconds and make sure that it's still in the failed state
                  // before we trigger the error. This is because we sometimes see
                  // 'failed' and then 'connected' afterwards.
                  setTimeout(function () {
                    if (event.target.iceConnectionState === 'failed') {
                      triggerError('The stream was unable to connect due to a network error.' +
                       ' Make sure your connection isn\'t blocked by a firewall.', 'ICEWorkflow');
                    }
                  }, 5000);
                }
              };
            }
          }

          return _peerConnection;
        }.bind(this),

        // Clean up the Peer Connection and trigger the close event.
        // This function can be called safely multiple times, it will
        // only trigger the close event once (per PeerConnection object)
        tearDownPeerConnection = function() {
          // Our connection is dead, stop processing ICE candidates
          if (_iceProcessor) _iceProcessor.peerConnection = null;

          if (_peerConnection !== null) {
            _peerConnection = null;
            this.trigger('close');
          }
        },

        routeStateChanged = function(event) {
          var newState;

          if (typeof(event) === 'string') {
            // The newest version of the API
            newState = event;

          } else if (event.target && event.target.signalingState) {
            // The slightly older version
            newState = event.target.signalingState;

          } else {
            // At least six months old version. Positively ancient, yeah?
            newState = event.target.readyState;
          }

          OT.debug('PeerConnection.stateChange: ' + newState);
          if (newState && newState.toLowerCase() !== _state) {
            _state = newState.toLowerCase();
            OT.debug('PeerConnection.stateChange: ' + _state);

            switch(_state) {
              case 'closed':
                tearDownPeerConnection.call(this);
                break;
            }
          }
        },

        getRemoteStreams = function() {
          var streams;

          if (_peerConnection.getRemoteStreams) {
            streams = _peerConnection.getRemoteStreams();
          } else if (_peerConnection.remoteStreams) {
            streams = _peerConnection.remoteStreams;
          } else {
            throw new Error('Invalid Peer Connection object implements no ' +
              'method for retrieving remote streams');
          }

          // Force streams to be an Array, rather than a "Sequence" object,
          // which is browser dependent and does not behaviour like an Array
          // in every case.
          return Array.prototype.slice.call(streams);
        },

        /// PeerConnection signaling
        onRemoteStreamAdded = function(event) {
          this.trigger('streamAdded', event.stream);
        },

        onRemoteStreamRemoved = function(event) {
          this.trigger('streamRemoved', event.stream);
        },

        // ICE Negotiation messages


        // Relays a SDP payload (+sdp+), that is part of a message of type +messageType+
        // via the registered message delegators
        relaySDP = function(messageType, sdp) {
          delegateMessage(messageType, sdp);
        },

        // Process an offer that
        processOffer = function(message) {
          var offer = new NativeRTCSessionDescription({type: 'offer', sdp: message.content.sdp}),

              // Relays +answer+ Answer
              relayAnswer = function(answer) {
                _iceProcessor.peerConnection = _peerConnection;
                _iceProcessor.processPending();
                relaySDP(OT.Raptor.Actions.ANSWER, answer);
              },

              reportError = function(message, errorReason, prefix) {
                triggerError('PeerConnection.offerProcessor ' + message + ': ' +
                  errorReason, prefix);
              };

          setupPeerConnection();

          offerProcessor(
            _peerConnection,
            offer,
            relayAnswer,
            reportError
          );
        },

        processAnswer = function(message) {
          if (!message.content.sdp) {
            OT.error('PeerConnection.processMessage: Weird answer message, no SDP.');
            return;
          }

          _answer = new NativeRTCSessionDescription({type: 'answer', sdp: message.content.sdp});

          _peerConnection.setRemoteDescription(_answer,
              function () {
                OT.debug('setRemoteDescription Success');
              }, function (errorReason) {
                triggerError('Error while setting RemoteDescription ' + errorReason,
                  'SetRemoteDescription');
              });

          _iceProcessor.peerConnection = _peerConnection;
          _iceProcessor.processPending();
        },

        processSubscribe = function() {
          OT.debug('PeerConnection.processSubscribe: Sending offer to subscriber.');

          setupPeerConnection();

          suscribeProcessor(
            _peerConnection,

            // Success: Relay Offer
            function(offer) {
              _offer = offer;
              relaySDP(OT.Raptor.Actions.OFFER, _offer);
            },

            // Failure
            function(message, errorReason, prefix) {
              triggerError('PeerConnection.suscribeProcessor ' + message + ': ' +
                errorReason, prefix);
            }
          );
        },

        triggerError = function(errorReason, prefix) {
          OT.error(errorReason);
          this.trigger('error', errorReason, prefix);
        }.bind(this);

    this.addLocalStream = function(webRTCStream) {
      setupPeerConnection();
      _peerConnection.addStream(webRTCStream);
    };

    this.disconnect = function() {
      _iceProcessor = null;

      if (_peerConnection) {
        var currentState = (_peerConnection.signalingState || _peerConnection.readyState);
        if (currentState && currentState.toLowerCase() !== 'closed') _peerConnection.close();

          // In theory, calling close on the PeerConnection should trigger a statechange
          // event with 'close'. For some reason I'm not seeing this in FF, hence we're
          // calling it manually below
        tearDownPeerConnection.call(this);
      }

      this.off();
    };

    this.processMessage = function(type, message) {
      OT.debug('PeerConnection.processMessage: Received ' +
        type + ' from ' + message.fromAddress);
      OT.debug(message);

      switch(type) {
        case 'generateoffer':
          processSubscribe.call(this, message);
          break;

        case 'offer':
          processOffer.call(this, message);
          break;

        case 'answer':
        case 'pranswer':
          processAnswer.call(this, message);
          break;

        case 'candidate':
          _iceProcessor.process(message);
          break;

        default:
          OT.debug('PeerConnection.processMessage: Received an unexpected message of type ' +
            type + ' from ' + message.fromAddress + ': ' + JSON.stringify(message));
      }

      return this;
    };
    
    this.setIceServers = function (iceServers) {
      if (iceServers) {
        config.iceServers = iceServers;
      }
    };

    this.registerMessageDelegate = function(delegateFn) {
      return _messageDelegates.push(delegateFn);
    };

    this.unregisterMessageDelegate = function(delegateFn) {
      var index = _messageDelegates.indexOf(delegateFn);

      if ( index !== -1 ) {
        _messageDelegates.splice(index, 1);
      }
      return _messageDelegates.length;
    };

    /**
     * Retrieves the PeerConnection stats.
     *
     * TODO document what the format of the final reports that +callback+ gets is
     *
     * @ignore
     * @private
     * @memberof PeerConnection
     * @param callback {Function} this will be triggered once the stats a are ready.
     * It takes a single argument which is the stats report, which may be undefined
     * if there is presently no stats.
     */
    this.getStats = function(prevStats, callback) {
      var parsedStats = {},
          now,
          timeDifference,
          parseAvgVideoBitrate,
          parseAvgAudioBitrate,
          parseFrameRate,
          parseStatsReports,
          parseStats,
          needsNewGetStats;

      // need to make sure that this isn't called again when in the middle of processing
      if (_gettingStats === true) {
        OT.warn('PeerConnection.getStats: Already getting the stats!');
        return;
      }

      // locking this function
      _gettingStats = true;

      // get the previous timestamp (seconds)
      now = OT.$.now();
      timeDifference = (now - prevStats.timeStamp) / 1000; // how many seconds has passed

      // now update the date
      prevStats.timeStamp = now;

      /* this parses a result if there it contains the video bitrate */
      parseAvgVideoBitrate = function(result) {
        var lastBytesSent = prevStats.videoBytesTransferred || 0;

        if (result.bytesSent || result.bytesReceived) {
          prevStats.videoBytesTransferred = result.bytesSent || result.bytesReceived;
          return Math.round((prevStats.videoBytesTransferred - lastBytesSent) * 8 / timeDifference);

        } else if (result.stat('googFrameHeightSent')) {
          prevStats.videoBytesTransferred = result.stat('bytesSent');
          return Math.round((prevStats.videoBytesTransferred - lastBytesSent) * 8 / timeDifference);

        } else if (result.stat('googFrameHeightReceived')) {
          prevStats.videoBytesTransferred = result.stat('bytesReceived');
          return Math.round((prevStats.videoBytesTransferred - lastBytesSent) * 8 / timeDifference);

        } else {
          return NaN;
        }

      };

        /* this parses a result if there it contains the audio bitrate */
      parseAvgAudioBitrate = function(result) {
        var lastBytesSent = prevStats.audioBytesTransferred || 0;

        if (result.bytesSent || result.bytesReceived) {
          prevStats.audioBytesTransferred = result.bytesSent || result.bytesReceived;
          return Math.round((prevStats.audioBytesTransferred - lastBytesSent) * 8 / timeDifference);

        } else if (result.stat('audioInputLevel')) {
          prevStats.audioBytesTransferred = result.stat('bytesSent');
          return Math.round((prevStats.audioBytesTransferred - lastBytesSent) * 8 / timeDifference);

        } else if (result.stat('audioOutputLevel')) {
          prevStats.audioBytesTransferred = result.stat('bytesReceived');
          return Math.round((prevStats.audioBytesTransferred - lastBytesSent) * 8 / timeDifference);

        } else {
          return NaN;
        }

      };
        
      parseFrameRate = function(result) {
        if (result.stat('googFrameRateSent')) {
          return result.stat('googFrameRateSent');
        } else if (result.stat('googFrameRateReceived')) {
          return result.stat('googFrameRateReceived');
        }
        return null;
      };

      parseStatsReports = function(stats) {
        if (stats.result) {
          var resultList = stats.result();
          for (var resultIndex = 0; resultIndex < resultList.length; resultIndex++) {
            var result = resultList[resultIndex];
            if (result.stat) {

              if(result.stat('googActiveConnection') === 'true') {
                parsedStats.localCandidateType = result.stat('googLocalCandidateType');
                parsedStats.remoteCandidateType = result.stat('googRemoteCandidateType');
                parsedStats.transportType = result.stat('googTransportType');
              }

              var avgVideoBitrate = parseAvgVideoBitrate(result);
              if (!isNaN(avgVideoBitrate)) {
                parsedStats.avgVideoBitrate = avgVideoBitrate;
              }

              var avgAudioBitrate = parseAvgAudioBitrate(result);
              if (!isNaN(avgAudioBitrate)) {
                parsedStats.avgAudioBitrate = avgAudioBitrate;
              }
              var frameRate = parseFrameRate(result);
              if (frameRate != null) {
                parsedStats.frameRate = frameRate;
              }
            }
          }
        }

        _gettingStats = false;
        callback(parsedStats);
      };
      parsedStats.duration = Math.round(now - _createTime);

      parseStats = function(stats) {
        for (var key in stats) {
          if (stats.hasOwnProperty(key) &&
            (stats[key].type === 'outboundrtp' || stats[key].type === 'inboundrtp')) {
            var res = stats[key];
            // Find the bandwidth info for video
            if (res.id.indexOf('video') !== -1) {

              var avgVideoBitrate = parseAvgVideoBitrate(res);
              if(!isNaN(avgVideoBitrate)) {
                parsedStats.avgVideoBitrate = avgVideoBitrate;
              }

            } else if (res.id.indexOf('audio') !== -1) {

              var avgAudioBitrate = parseAvgAudioBitrate(res);
              if(!isNaN(avgAudioBitrate)) {
                parsedStats.avgAudioBitrate = avgAudioBitrate;
              }

            }
          }
        }

        _gettingStats = false;
        callback(parsedStats);
      };

      needsNewGetStats = function() {
        var firefoxVersion = window.navigator.userAgent.toLowerCase()
        .match(/Firefox\/([0-9\.]+)/i);
        var needs = (firefoxVersion !== null && parseFloat(firefoxVersion[1], 10) >= 27.0);
        needsNewGetStats = function() { return needs; };
        return needs;
      };

      if (_peerConnection && _peerConnection.getStats) {
        if(needsNewGetStats()) {
          _peerConnection.getStats(null, parseStats, function(err) {
            OT.warn('Error collecting stats', err);
            _gettingStats = false;
          });
        } else {
          _peerConnection.getStats(parseStatsReports);
        }
      } else {
        // there was no peer connection yet or getStats isn't implemented in this enviroment
        _gettingStats = false;
        callback(parsedStats);
      }
    };

    Object.defineProperty(this, 'remoteStreams', {
      get: function() {
        return _peerConnection ? getRemoteStreams() : [];
      }
    });
  };

})(window);
!(function() {

  var _peerConnections = {};

  OT.PeerConnections = {
    add: function(remoteConnection, streamId, config) {
      var key = remoteConnection.id + '_' + streamId,
          ref = _peerConnections[key];

      if (!ref) {
        ref = _peerConnections[key] = {
          count: 0,
          pc: new OT.PeerConnection(config)
        };
      }

      // increase the PCs ref count by 1
      ref.count += 1;

      return ref.pc;
    },

    remove: function(remoteConnection, streamId) {
      var key = remoteConnection.id + '_' + streamId,
          ref = _peerConnections[key];

      if (ref) {
        ref.count -= 1;

        if (ref.count === 0) {
          ref.pc.disconnect();
          delete _peerConnections[key];
        }
      }
    }
  };

})(window);
!(function() {

  /*
   * Abstracts PeerConnection related stuff away from OT.Publisher.
   *
   * Responsible for:
   * * setting up the underlying PeerConnection (delegates to OT.PeerConnections)
   * * triggering a connected event when the Peer connection is opened
   * * triggering a disconnected event when the Peer connection is closed
   * * providing a destroy method
   * * providing a processMessage method
   *
   * Once the PeerConnection is connected and the video element playing it triggers 
   * the connected event
   *
   * Triggers the following events
   * * connected
   * * disconnected
   */
  OT.PublisherPeerConnection = function(remoteConnection, session, streamId, webRTCStream) {
    var _peerConnection,
        _hasRelayCandidates = false,
        _subscriberId = session._.subscriberMap[remoteConnection.id + '_' + streamId],
        _onPeerClosed,
        _onPeerError,
        _relayMessageToPeer;

    // Private
    _onPeerClosed = function() {
      this.destroy();
      this.trigger('disconnected', this);
    };

    // Note: All Peer errors are fatal right now.
    _onPeerError = function(errorReason, prefix) {
      this.trigger('error', null, errorReason, this, prefix);
      this.destroy();
    };

    _relayMessageToPeer = function(type, payload) {
      if (!_hasRelayCandidates){
        var extractCandidates = type === OT.Raptor.Actions.CANDIDATE ||
                                type === OT.Raptor.Actions.OFFER ||
                                type === OT.Raptor.Actions.ANSWER ||
                                type === OT.Raptor.Actions.PRANSWER ;

        if (extractCandidates) {
          var message = (type === OT.Raptor.Actions.CANDIDATE) ? payload.candidate : payload.sdp;
          _hasRelayCandidates = message.indexOf('typ relay') !== -1;
        }
      }

      switch(type) {
        case OT.Raptor.Actions.ANSWER:
        case OT.Raptor.Actions.PRANSWER:
          if (session.sessionInfo.p2pEnabled) {
            session._.jsepAnswerP2p(streamId, _subscriberId, payload.sdp);
          } else {
            session._.jsepAnswer(streamId, payload.sdp);
          }

          break;

        case OT.Raptor.Actions.OFFER:
          this.trigger('connected');

          if (session.sessionInfo.p2pEnabled) {
            session._.jsepOfferP2p(streamId, _subscriberId, payload.sdp);

          } else {
            session._.jsepOffer(streamId, payload.sdp);
          }

          break;

        case OT.Raptor.Actions.CANDIDATE:
          if (session.sessionInfo.p2pEnabled) {
            session._.jsepCandidateP2p(streamId, _subscriberId, payload);

          } else {
            session._.jsepCandidate(streamId, payload);
          }
      }
    }.bind(this);

    OT.$.eventing(this);

    // Public
    this.destroy = function() {
      // Clean up our PeerConnection
      if (_peerConnection) {
        OT.PeerConnections.remove(remoteConnection, streamId);
      }

      _peerConnection.off();
      _peerConnection = null;
    };

    this.processMessage = function(type, message) {
      _peerConnection.processMessage(type, message);
    };

    this.getStats = function(prevStats, callback) {
      _peerConnection.getStats(prevStats, callback);
    };

    // Init
    this.init = function(iceServers) {
      _peerConnection = OT.PeerConnections.add(remoteConnection, streamId, {
        iceServers: iceServers
      });

      _peerConnection.on({
        close: _onPeerClosed,
        error: _onPeerError
      }, this);

      _peerConnection.registerMessageDelegate(_relayMessageToPeer);
      _peerConnection.addLocalStream(webRTCStream);

      Object.defineProperty(this, 'remoteConnection', {
        value: remoteConnection
      });

      Object.defineProperty(this, 'hasRelayCandidates', {
        get: function() { return _hasRelayCandidates; }
      });
    };
  };

})(window);
!(function() {

  /*
   * Abstracts PeerConnection related stuff away from OT.Subscriber.
   *
   * Responsible for:
   * * setting up the underlying PeerConnection (delegates to OT.PeerConnections)
   * * triggering a connected event when the Peer connection is opened
   * * triggering a disconnected event when the Peer connection is closed
   * * creating a video element when a stream is added
   * * responding to stream removed intelligently
   * * providing a destroy method
   * * providing a processMessage method
   *
   * Once the PeerConnection is connected and the video element playing it
   * triggers the connected event
   *
   * Triggers the following events
   * * connected
   * * disconnected
   * * remoteStreamAdded
   * * remoteStreamRemoved
   * * error
   *
   */

  OT.SubscriberPeerConnection = function(remoteConnection, session, stream,
    subscriber, properties) {
    var _peerConnection,
        _hasRelayCandidates = false,
        _onPeerClosed,
        _onRemoteStreamAdded,
        _onRemoteStreamRemoved,
        _onPeerError,
        _relayMessageToPeer,
        _setEnabledOnStreamTracksCurry;

    // Private
    _onPeerClosed = function() {
      this.destroy();
      this.trigger('disconnected', this);
    };

    _onRemoteStreamAdded = function(remoteRTCStream) {
      this.trigger('remoteStreamAdded', remoteRTCStream, this);
    };

    _onRemoteStreamRemoved = function(remoteRTCStream) {
      this.trigger('remoteStreamRemoved', remoteRTCStream, this);
    };

    // Note: All Peer errors are fatal right now.
    _onPeerError = function(errorReason, prefix) {
      this.trigger('error', null, errorReason, this, prefix);
    };

    _relayMessageToPeer = function(type, payload) {
      if (!_hasRelayCandidates){
        var extractCandidates = type === OT.Raptor.Actions.CANDIDATE ||
                                type === OT.Raptor.Actions.OFFER ||
                                type === OT.Raptor.Actions.ANSWER ||
                                type === OT.Raptor.Actions.PRANSWER ;

        if (extractCandidates) {
          var message = (type === OT.Raptor.Actions.CANDIDATE) ? payload.candidate : payload.sdp;
          _hasRelayCandidates = message.indexOf('typ relay') !== -1;
        }
      }

      switch(type) {
        case OT.Raptor.Actions.ANSWER:
        case OT.Raptor.Actions.PRANSWER:
          this.trigger('connected');

          session._.jsepAnswerP2p(stream.id, subscriber.widgetId, payload.sdp);
          break;

        case OT.Raptor.Actions.OFFER:
          session._.jsepOfferP2p(stream.id, subscriber.widgetId, payload.sdp);
          break;

        case OT.Raptor.Actions.CANDIDATE:
          session._.jsepCandidateP2p(stream.id, subscriber.widgetId, payload);
          break;
      }
    }.bind(this);

    // Helper method used by subscribeToAudio/subscribeToVideo
    _setEnabledOnStreamTracksCurry = function(isVideo) {
      var method = 'get' + (isVideo ? 'Video' : 'Audio') + 'Tracks';

      return function(enabled) {
        var remoteStreams = _peerConnection.remoteStreams,
            tracks,
            stream;

        if (remoteStreams.length === 0 || !remoteStreams[0][method]) {
          // either there is no remote stream or we are in a browser that doesn't
          // expose the media tracks (Firefox)
          return;
        }

        for (var i=0, num=remoteStreams.length; i<num; ++i) {
          stream = remoteStreams[i];
          tracks = stream[method]();

          for (var k=0, numTracks=tracks.length; k < numTracks; ++k){
            // Only change the enabled property if it's different
            // otherwise we get flickering of the video
            if (tracks[k].enabled !== enabled) tracks[k].enabled=enabled;
          }
        }
      };
    };


    OT.$.eventing(this);

    // Public
    this.destroy = function() {
      if (_peerConnection) {
        var numDelegates = _peerConnection.unregisterMessageDelegate(_relayMessageToPeer);
        
        // Only clean up the PeerConnection if there isn't another Subscriber using it
        if (numDelegates === 0) {
          // Unsubscribe us from the stream, if it hasn't already been destroyed
          if (session && session.connected && stream && !stream.destroyed) {
              // Notify the server components
            session._.subscriberDestroy(stream, subscriber);
          }
          
          // Ref: OPENTOK-2458 disable all audio tracks before removing it.
          this.subscribeToAudio(false);
        }
        OT.PeerConnections.remove(remoteConnection, stream.streamId);
      }
      _peerConnection = null;
      this.off();
    };

    this.processMessage = function(type, message) {
      _peerConnection.processMessage(type, message);
    };

    this.getStats = function(prevStats, callback) {
      _peerConnection.getStats(prevStats, callback);
    };

    this.subscribeToAudio = _setEnabledOnStreamTracksCurry(false);
    this.subscribeToVideo = _setEnabledOnStreamTracksCurry(true);

    Object.defineProperty(this, 'hasRelayCandidates', {
      get: function() { return _hasRelayCandidates; }
    });

    // Init
    this.init = function() {
      _peerConnection = OT.PeerConnections.add(remoteConnection, stream.streamId, {});

      _peerConnection.on({
        close: _onPeerClosed,
        streamAdded: _onRemoteStreamAdded,
        streamRemoved: _onRemoteStreamRemoved,
        error: _onPeerError
      }, this);

      var numDelegates = _peerConnection.registerMessageDelegate(_relayMessageToPeer);

        // If there are already remoteStreams, add them immediately
      if (_peerConnection.remoteStreams.length > 0) {
        _peerConnection.remoteStreams.forEach(_onRemoteStreamAdded, this);
      } else if (numDelegates === 1) {
        // We only bother with the PeerConnection negotiation if we don't already
        // have a remote stream.

        var channelsToSubscribeTo;

        if (properties.subscribeToVideo || properties.subscribeToAudio) {
          var audio = stream.getChannelsOfType('audio'),
              video = stream.getChannelsOfType('video');

          channelsToSubscribeTo = audio.map(function(channel) {
            return {
              id: channel.id,
              type: channel.type,
              active: properties.subscribeToAudio
            };
          }).concat(video.map(function(channel) {
            return {
              id: channel.id,
              type: channel.type,
              active: properties.subscribeToVideo,
              restrictFrameRate: properties.restrictFrameRate !== void 0 ?
                properties.restrictFrameRate : false
            };
          }));
        }

        session._.subscriberCreate(stream, subscriber, channelsToSubscribeTo,
          function(err, message) {
            if (err) {
              this.trigger('error', null, err.message, this, 'Subscribe');
            }
            _peerConnection.setIceServers(OT.Raptor.parseIceServers(message));
          }.bind(this));
      }
    };
  };

})(window);
!(function() {

// Manages N Chrome elements
  OT.Chrome = function(properties) {
    var _visible = false,
        _widgets = {},

        // Private helper function
        _set = function(name, widget) {
          widget.parent = this;
          widget.appendTo(properties.parent);

          _widgets[name] = widget;

          Object.defineProperty(this, name, {
            get: function() { return _widgets[name]; }
          });
        };

    if (!properties.parent) {
      // @todo raise an exception
      return;
    }

    OT.$.eventing(this);

    this.destroy = function() {
      this.off();
      this.hide();

      for (var name in _widgets) {
        _widgets[name].destroy();
      }
    };

    this.show = function() {
      _visible = true;

      for (var name in _widgets) {
        _widgets[name].show();
      }
    };

    this.hide = function() {
      _visible = false;

      for (var name in _widgets) {
        _widgets[name].hide();
      }
    };


    // Adds the widget to the chrome and to the DOM. Also creates a accessor
    // property for it on the chrome.
    //
    // @example
    //  chrome.set('foo', new FooWidget());
    //  chrome.foo.setDisplayMode('on');
    //
    // @example
    //  chrome.set({
    //      foo: new FooWidget(),
    //      bar: new BarWidget()
    //  });
    //  chrome.foo.setDisplayMode('on');
    //
    this.set = function(widgetName, widget) {
      if (typeof(widgetName) === 'string' && widget) {
        _set.call(this, widgetName, widget);

      } else {
        for (var name in widgetName) {
          if (widgetName.hasOwnProperty(name)) {
            _set.call(this, name, widgetName[name]);
          }
        }
      }
      return this;
    };

  };

})(window);
!(function() {

  if (!OT.Chrome.Behaviour) OT.Chrome.Behaviour = {};

  // A mixin to encapsulate the basic widget behaviour. This needs a better name,
  // it's not actually a widget. It's actually "Behaviour that can be applied to
  // an object to make it support the basic Chrome widget workflow"...but that would
  // probably been too long a name.
  OT.Chrome.Behaviour.Widget = function(widget, options) {
    var _options = options || {},
        _mode,
        _previousMode;

    //
    // @param [String] mode
    //      'on', 'off', or 'auto'
    //
    widget.setDisplayMode = function(mode) {
      var newMode = mode || 'auto';
      if (_mode === newMode) return;

      OT.$.removeClass(this.domElement, 'OT_mode-' + _mode);
      OT.$.addClass(this.domElement, 'OT_mode-' + newMode);

      _previousMode = _mode;
      _mode = newMode;
    };

    widget.show = function() {
      this.setDisplayMode(_previousMode);
      if (_options.onShow) _options.onShow();

      return this;
    };

    widget.hide = function() {
      this.setDisplayMode('off');
      if (_options.onHide) _options.onHide();

      return this;
    };

    widget.destroy = function() {
      if (_options.onDestroy) _options.onDestroy(this.domElement);
      if (this.domElement) OT.$.removeElement(this.domElement);

      return widget;
    };

    widget.appendTo = function(parent) {
      // create the element under parent
      this.domElement = OT.$.createElement(_options.nodeName || 'div',
                                          _options.htmlAttributes,
                                          _options.htmlContent);

      if (_options.onCreate) _options.onCreate(this.domElement);

      // if the mode isn't auto, then we can directly set it
      if (_options.mode !== 'auto') {
        widget.setDisplayMode(_options.mode);
      } else {
        // we set it to on at first, and then apply the desired mode
        // this will let the proper widgets nicely fade away
        widget.setDisplayMode('on');
        setTimeout(function() {
          widget.setDisplayMode(_options.mode);
        }, 2000);
      }

      // add the widget to the parent
      parent.appendChild(this.domElement);

      return widget;
    };
  };

})(window);
!(function() {

  // BackingBar Chrome Widget
  //
  // nameMode (String)
  // Whether or not the name panel is being displayed
  // Possible values are: "auto" (the name is displayed
  // when the stream is first displayed and when the user mouses over the display),
  // "off" (the name is not displayed), and "on" (the name is displayed).
  //
  // muteMode (String)
  // Whether or not the mute button is being displayed
  // Possible values are: "auto" (the mute button is displayed
  // when the stream is first displayed and when the user mouses over the display),
  // "off" (the mute button is not displayed), and "on" (the mute button is displayed).
  //
  // displays a backing bar
  // can be shown/hidden
  // can be destroyed
  OT.Chrome.BackingBar = function(options) {
    var _nameMode = options.nameMode,
        _muteMode = options.muteMode,
        _domElement;

    // This behaviour must be implemented to make the widget behaviour work.
    // @fixme This is a nasty code smell
    Object.defineProperty(this, 'domElement', {
      get: function() { return _domElement; },
      set: function(domElement) {
        _domElement = domElement;
      }
    });

    function getDisplayMode() {
      if(_nameMode === 'on' || _muteMode === 'on') {
        return 'on';
      } else if(_nameMode === 'mini' || _muteMode === 'mini') {
        return 'mini';
      } else if(_nameMode === 'mini-auto' || _muteMode === 'mini-auto') {
        return 'mini-auto';
      } else if(_nameMode === 'auto' || _muteMode === 'auto') {
        return 'auto';
      } else {
        return 'off';
      }
    }

    // Mixin common widget behaviour
    OT.Chrome.Behaviour.Widget(this, {
      mode: getDisplayMode(),
      nodeName: 'div',
      htmlContent: '',
      htmlAttributes: {
        className: 'OT_bar OT_edge-bar-item'
      }
    });

    this.setNameMode = function(nameMode) {
      _nameMode = nameMode;
      this.setDisplayMode(getDisplayMode());
    };

    this.setMuteMode = function(muteMode) {
      _muteMode = muteMode;
      this.setDisplayMode(getDisplayMode());
    };

  };

})(window);
!(function() {

  // NamePanel Chrome Widget
  //
  // mode (String)
  // Whether to display the name. Possible values are: "auto" (the name is displayed
  // when the stream is first displayed and when the user mouses over the display),
  // "off" (the name is not displayed), and "on" (the name is displayed).
  //
  // displays a name
  // can be shown/hidden
  // can be destroyed
  OT.Chrome.NamePanel = function(options) {
    var _name = options.name,
        _bugMode = options.bugMode,
        _domElement;

    if (!_name || _name.trim().length === '') {
      _name = null;

      // THere's no name, just flip the mode off
      options.mode = 'off';
    }

    // This behaviour must be implemented to make the widget behaviour work.
    // @fixme This is a nasty code smell
    Object.defineProperty(this, 'domElement', {
      get: function() { return _domElement; },
      set: function(domElement) {
        _domElement = domElement;
      }
    });

    Object.defineProperty(this, 'name', {
      set: function(name) {
        if (!_name) this.setDisplayMode('auto');
        _name = name;
        _domElement.innerHTML = _name;
      }.bind(this)
    });

    this.setBugMode = function(bugMode) {
      _bugMode = bugMode;
      if(bugMode === 'off') {
        OT.$.addClass(_domElement, 'OT_name-no-bug');
      } else {
        OT.$.removeClass(_domElement, 'OT_name-no-bug');
      }
    };

    // Mixin common widget behaviour
    OT.Chrome.Behaviour.Widget(this, {
      mode: options.mode,
      nodeName: 'h1',
      htmlContent: _name,
      htmlAttributes: {
        className: 'OT_name OT_edge-bar-item'
      },
      onCreate: function() {
        this.setBugMode(_bugMode);
      }.bind(this)
    });

  };

})(window);
!(function() {

  OT.Chrome.MuteButton = function(options) {
    var _onClickCb,
        _muted = options.muted || false,
        _domElement,
        updateClasses,
        attachEvents,
        detachEvents,
        onClick;

    // This behaviour must be implemented to make the widget behaviour work.
    // @fixme This is a nasty code smell
    Object.defineProperty(this, 'domElement', {
      get: function() { return _domElement; },
      set: function(domElement) {
        _domElement = domElement;
      }
    });

    updateClasses = function() {
      if (_muted) {
        OT.$.addClass(_domElement, 'OT_active');
      } else {
        OT.$.removeClass(_domElement, 'OT_active ');
      }
    };

    // Private Event Callbacks
    attachEvents = function(elem) {
      _onClickCb = onClick.bind(this);
      elem.addEventListener('click', _onClickCb, false);
    };

    detachEvents = function(elem) {
      _onClickCb = null;
      elem.removeEventListener('click', _onClickCb, false);
    };

    onClick = function() {
      _muted = !_muted;

      updateClasses();

      if (_muted) {
        this.parent.trigger('muted', this);
      } else {
        this.parent.trigger('unmuted', this);
      }

      return false;
    };

    Object.defineProperty(this, 'muted', {
      get: function() { return _muted; },
      set: function(muted) {
        _muted = muted;
        updateClasses();
      }
    });

    // Mixin common widget behaviour
    var classNames = _muted ? 'OT_edge-bar-item OT_mute OT_active' : 'OT_edge-bar-item OT_mute';
    OT.Chrome.Behaviour.Widget(this, {
      mode: options.mode,
      nodeName: 'button',
      htmlContent: 'Mute',
      htmlAttributes: {
        className: classNames
      },
      onCreate: attachEvents.bind(this),
      onDestroy: detachEvents.bind(this)
    });
  };


})(window);
!(function() {

  OT.Chrome.OpenTokButton = function(options) {

    // This behaviour must be implemented to make the widget behaviour work.
    // @fixme This is a nasty code smell
    var _domElement;
    Object.defineProperty(this, 'domElement', {
      get: function() { return _domElement; },
      set: function(domElement) {
        _domElement = domElement;
      }
    });

    // Mixin common widget behaviour
    OT.Chrome.Behaviour.Widget(this, {
      mode: options ? options.mode : null,
      nodeName: 'span',
      htmlContent: 'OpenTok',
      htmlAttributes: {
        className: 'OT_opentok OT_edge-bar-item'
      }
    });

  };

})(window);
!(function() {

  // Archving Chrome Widget
  //
  // mode (String)
  // Whether to display the archving widget. Possible values are: "on" (the status is displayed
  // when archiving and briefly when archving ends) and "off" (the status is not displayed)

  // Whether to display the archving widget. Possible values are: "auto" (the name is displayed
  // when the status is first displayed and when the user mouses over the display),
  // "off" (the name is not displayed), and "on" (the name is displayed).
  //
  // displays a name
  // can be shown/hidden
  // can be destroyed
  OT.Chrome.Archiving = function(options) {
    var _archiving = options.archiving,
        _archivingStarted = options.archivingStarted || 'Archiving on',
        _archivingEnded = options.archivingEnded || 'Archiving off',
        _initialState = true,
        _lightBox,
        _light,
        _text,
        _domElement,
        renderStageDelayedAction,
        renderText,
        renderStage;

    // This behaviour must be implemented to make the widget behaviour work.
    // @fixme This is a nasty code smell
    Object.defineProperty(this, 'domElement', {
      get: function() { return _domElement; },
      set: function(domElement) {
        _domElement = domElement;
      }
    });

    renderText = function(text) {
      _text.innerText = text;
      _lightBox.setAttribute('title', text);
    };

    renderStage = function() {
      if(renderStageDelayedAction) {
        clearTimeout(renderStageDelayedAction);
        renderStageDelayedAction = null;
      }

      if(_archiving) {
        OT.$.addClass(_light, 'OT_active');
      } else {
        OT.$.removeClass(_light, 'OT_active');
      }

      OT.$.removeClass(_domElement, 'OT_archiving-' + (!_archiving ? 'on' : 'off'));
      OT.$.addClass(_domElement, 'OT_archiving-' + (_archiving ? 'on' : 'off'));
      if(options.show && _archiving) {
        renderText(_archivingStarted);
        OT.$.addClass(_text, 'OT_mode-on');
        OT.$.removeClass(_text, 'OT_mode-auto');
        this.setDisplayMode('on');
        renderStageDelayedAction = setTimeout(function() {
          OT.$.addClass(_text, 'OT_mode-auto');
          OT.$.removeClass(_text, 'OT_mode-on');
        }.bind(this), 5000);
      } else if(options.show && !_initialState) {
        OT.$.addClass(_text, 'OT_mode-on');
        OT.$.removeClass(_text, 'OT_mode-auto');
        this.setDisplayMode('on');
        renderText(_archivingEnded);
        renderStageDelayedAction = setTimeout(function() {
          this.setDisplayMode('off');
        }.bind(this), 5000);
      } else {
        this.setDisplayMode('off');
      }
    }.bind(this);

    // Mixin common widget behaviour
    OT.Chrome.Behaviour.Widget(this, {
      mode: _archiving && options.show && 'on' || 'off',
      nodeName: 'h1',
      htmlAttributes: {className: 'OT_archiving OT_edge-bar-item OT_edge-bottom'},
      onCreate: function() {
        _lightBox = OT.$.createElement('div', {
          className: 'OT_archiving-light-box'
        }, '');
        _light = OT.$.createElement('div', {
          className: 'OT_archiving-light'
        }, '');
        _lightBox.appendChild(_light);
        _text = OT.$.createElement('div', {
          className: 'OT_archiving-status OT_mode-on OT_edge-bar-item OT_edge-bottom'
        }, '');
        _domElement.appendChild(_lightBox);
        _domElement.appendChild(_text);
        renderStage();
      }
    });

    this.setShowArchiveStatus = function(show) {
      options.show = show;
      if(_domElement) {
        renderStage.call(this);
      }
    };

    this.setArchiving = function(status) {
      _archiving = status;
      _initialState = false;
      if(_domElement) {
        renderStage.call(this);
      }
    };

  };

})(window);
(function() {
/* Stylable Notes
 * RTC doesn't need to wait until anything is loaded
 * Some bits are controlled by multiple flags, i.e. buttonDisplayMode and nameDisplayMode.
 * When there are multiple flags how is the final setting chosen?
 * When some style bits are set updates will need to be pushed through to the Chrome
 */

  // Mixes the StylableComponent behaviour into the +self+ object. It will
  // also set the default styles to +initialStyles+.
  //
  // @note This Mixin is dependent on OT.Eventing.
  //
  //
  // @example
  //
  //  function SomeObject {
  //      OT.StylableComponent(this, {
  //          name: 'SomeObject',
  //          foo: 'bar'
  //      });
  //  }
  //
  //  var obj = new SomeObject();
  //  obj.getStyle('foo');        // => 'bar'
  //  obj.setStyle('foo', 'baz')
  //  obj.getStyle('foo');        // => 'baz'
  //  obj.getStyle();             // => {name: 'SomeObject', foo: 'baz'}
  //
  OT.StylableComponent = function(self, initalStyles) {
    if (!self.trigger) {
      throw new Error('OT.StylableComponent is dependent on the mixin OT.$.eventing. ' +
        'Ensure that this is included in the object before StylableComponent.');
    }

    // Broadcast style changes as the styleValueChanged event
    var onStyleChange = function(key, value, oldValue) {
      if (oldValue) {
        self.trigger('styleValueChanged', key, value, oldValue);
      } else {
        self.trigger('styleValueChanged', key, value);
      }
    };

    var _style = new Style(initalStyles, onStyleChange);

  /**
   * Returns an object that has the properties that define the current user interface controls of 
   * the Publisher. You can modify the properties of this object and pass the object to the 
   * <code>setStyle()</code> method of thePublisher object. (See the documentation for 
   * <a href="#setStyle">setStyle()</a> to see the styles that define this object.)
   * @return {Object} The object that defines the styles of the Publisher.
   * @see <a href="#setStyle">setStyle()</a>
   * @method #getStyle
   * @memberOf Publisher
   */

	/**
	 * Returns an object that has the properties that define the current user interface controls of 
   * the Subscriber. You can modify the properties of this object and pass the object to the 
   * <code>setStyle()</code> method of the Subscriber object. (See the documentation for 
   * <a href="#setStyle">setStyle()</a> to see the styles that define this object.)
	 * @return {Object} The object that defines the styles of the Subscriber.
	 * @see <a href="#setStyle">setStyle()</a>
	 * @method #getStyle
	 * @memberOf Subscriber
	 */
    // If +key+ is falsly then all styles will be returned.
    self.getStyle = function(key) {
      return _style.get(key);
    };

  /**
   * Sets properties that define the appearance of some user interface controls of the Publisher.
   *
   * <p>You can either pass one parameter or two parameters to this method.</p>
   *
   * <p>If you pass one parameter, <code>style</code>, it is an object that has the following
   * properties:
   *
   *     <ul>
   *       <li><code>backgroundImageURI</code> (String) &mdash; A URI for an image to display as
   *       the background image when a video is not displayed. (A video may not be displayed if
   *       you call <code>publishVideo(false)</code> on the Publisher object). You can pass an http
   *       or https URI to a PNG, JPEG, or non-animated GIF file location. You can also use the
   *       <code>data</code> URI scheme (instead of http or https) and pass in base-64-encrypted
   *       PNG data, such as that obtained from the
   *       <a href="Publisher.html#getImgData">Publisher.getImgData()</a> method. For example,
   *       you could set the property to <code>"data:VBORw0KGgoAA..."</code>, where the portion of
   *       the string after <code>"data:"</code> is the result of a call to
   *       <code>Publisher.getImgData()</code>. If the URL or the image data is invalid, the
   *       property is ignored (the attempt to set the image fails silently).</li>
   *
   *       <li><code>buttonDisplayMode</code> (String) &mdash; How to display the microphone
   *       controls. Possible values are: <code>"auto"</code> (controls are displayed when the
   *       stream is first displayed and when the user mouses over the display), <code>"off"</code>
   *       (controls are not displayed), and <code>"on"</code> (controls are always displayed).</li>
   *
   *       <li><code>nameDisplayMode</code> (String) &#151; Whether to display the stream name.
   *       Possible values are: <code>"auto"</code> (the name is displayed when the stream is first
   *       displayed and when the user mouses over the display), <code>"off"</code> (the name is not
   *       displayed), and <code>"on"</code> (the name is always displayed).</li>
   *   </ul>
   * </p>
   *
   * <p>For example, the following code passes one parameter to the method:</p>
   *
   * <pre>myPublisher.setStyle({nameDisplayMode: "off"});</pre>
   *
   * <p>If you pass two parameters, <code>style</code> and <code>value</code>, they are 
   * key-value pair that define one property of the display style. For example, the following 
   * code passes two parameter values to the method:</p>
   *
   * <pre>myPublisher.setStyle("nameDisplayMode", "off");</pre>
   *
   * <p>You can set the initial settings when you call the <code>Session.publish()</code>
   * or <code>OT.initPublisher()</code> method. Pass a <code>style</code> property as part of the
   * <code>properties</code> parameter of the method.</p>
   *
   * <p>The OT object dispatches an <code>exception</code> event if you pass in an invalid style 
   * to the method. The <code>code</code> property of the ExceptionEvent object is set to 1011.</p>
   *
   * @param {Object} style Either an object containing properties that define the style, or a 
   * String defining this single style property to set.
   * @param {String} value The value to set for the <code>style</code> passed in. Pass a value 
   * for this parameter only if the value of the <code>style</code> parameter is a String.</p>
   *
   * @see <a href="#getStyle">getStyle()</a>
   * @return {Publisher} The Publisher object
   * @see <a href="#setStyle">setStyle()</a>
   *
   * @see <a href="Session.html#subscribe">Session.publish()</a>
   * @see <a href="OT.html#initPublisher">OT.initPublisher()</a>
   * @method #setStyle
   * @memberOf Publisher
   */

  /**
   * Sets properties that define the appearance of some user interface controls of the Subscriber.
   *
   * <p>You can either pass one parameter or two parameters to this method.</p>
   *
   * <p>If you pass one parameter, <code>style</code>, it is an object that has the following
   * properties:
   *
   *     <ul>
   *       <li><code>backgroundImageURI</code> (String) &mdash; A URI for an image to display as
   *       the background image when a video is not displayed. (A video may not be displayed if
   *       you call <code>subscribeToVideo(false)</code> on the Publisher object). You can pass an
   *       http or https URI to a PNG, JPEG, or non-animated GIF file location. You can also use the
   *       <code>data</code> URI scheme (instead of http or https) and pass in base-64-encrypted
   *       PNG data, such as that obtained from the
   *       <a href="Subscriber.html#getImgData">Subscriber.getImgData()</a> method. For example,
   *       you could set the property to <code>"data:VBORw0KGgoAA..."</code>, where the portion of
   *       the string after <code>"data:"</code> is the result of a call to
   *       <code>Publisher.getImgData()</code>. If the URL or the image data is invalid, the
   *       property is ignored (the attempt to set the image fails silently).</li>
   *
   *       <li><code>buttonDisplayMode</code> (String) &mdash; How to display the speaker
   *       controls. Possible values are: <code>"auto"</code> (controls are displayed when the
   *       stream is first displayed and when the user mouses over the display), <code>"off"</code>
   *       (controls are not displayed), and <code>"on"</code> (controls are always displayed).</li>
   *
   *       <li><code>nameDisplayMode</code> (String) &#151; Whether to display the stream name.
   *       Possible values are: <code>"auto"</code> (the name is displayed when the stream is first
   *       displayed and when the user mouses over the display), <code>"off"</code> (the name is not
   *       displayed), and <code>"on"</code> (the name is always displayed).</li>
   *   </ul>
   * </p>
   *
   * <p>For example, the following code passes one parameter to the method:</p>
   *
   * <pre>mySubscriber.setStyle({nameDisplayMode: "off"});</pre>
   *
   * <p>If you pass two parameters, <code>style</code> and <code>value</code>, they are key-value 
   * pair that define one property of the display style. For example, the following code passes 
   * two parameter values to the method:</p>
   *
   * <pre>mySubscriber.setStyle("nameDisplayMode", "off");</pre>
   *
   * <p>You can set the initial settings when you call the <code>Session.subscribe()</code> method.
   * Pass a <code>style</code> property as part of the <code>properties</code> parameter of the 
   * method.</p>
   *
   * <p>The OT object dispatches an <code>exception</code> event if you pass in an invalid style 
   * to the method. The <code>code</code> property of the ExceptionEvent object is set to 1011.</p>
   *
   * @param {Object} style Either an object containing properties that define the style, or a 
   * String defining this single style property to set.
   * @param {String} value The value to set for the <code>style</code> passed in. Pass a value 
   * for this parameter only if the value of the <code>style</code> parameter is a String.</p>
   *
   * @returns {Subscriber} The Subscriber object.
   *
   * @see <a href="#getStyle">getStyle()</a>
   * @see <a href="#setStyle">setStyle()</a>
   *
   * @see <a href="Session.html#subscribe">Session.subscribe()</a>
   * @method #setStyle
   * @memberOf Subscriber
   */
    self.setStyle = function(keyOrStyleHash, value, silent) {
      if (typeof(keyOrStyleHash) !== 'string') {
        _style.setAll(keyOrStyleHash, silent);
      } else {
        _style.set(keyOrStyleHash, value);
      }
      return this;
    };
  };

  var Style = function(initalStyles, onStyleChange) {
    var _style = {},
        _COMPONENT_STYLES,
        _validStyleValues,
        isValidStyle,
        castValue;

    _COMPONENT_STYLES = [
      'showMicButton',
      'showSpeakerButton',
      'nameDisplayMode',
      'buttonDisplayMode',
      'backgroundImageURI',
      'bugDisplayMode'
    ];

    _validStyleValues = {
      buttonDisplayMode: ['auto', 'mini', 'mini-auto', 'off', 'on'],
      nameDisplayMode: ['auto', 'off', 'on'],
      bugDisplayMode: ['auto', 'off', 'on'],
      showSettingsButton: [true, false],
      showMicButton: [true, false],
      backgroundImageURI: null,
      showControlBar: [true, false],
      showArchiveStatus: [true, false]
    };


    // Validates the style +key+ and also whether +value+ is valid for +key+
    isValidStyle = function(key, value) {
      return key === 'backgroundImageURI' ||
        (   _validStyleValues.hasOwnProperty(key) &&
        _validStyleValues[key].indexOf(value) !== -1 );
    };

    castValue = function(value) {
      switch(value) {
        case 'true':
          return true;
        case 'false':
          return false;
        default:
          return value;
      }
    };

    // Returns a shallow copy of the styles.
    this.getAll = function() {
      var style = OT.$.clone(_style);

      for (var i in style) {
        if (_COMPONENT_STYLES.indexOf(i) < 0) {
          // Strip unnecessary properties out, should this happen on Set?
          delete style[i];
        }
      }

      return style;
    };

    this.get = function(key) {
      if (key) {
        return _style[key];
      }

      // We haven't been asked for any specific key, just return the lot
      return this.getAll();
    };

    // *note:* this will not trigger onStyleChange if +silent+ is truthy
    this.setAll = function(newStyles, silent) {
      var oldValue, newValue;

      for (var key in newStyles) {
        newValue = castValue(newStyles[key]);

        if (isValidStyle(key, newValue)) {
          oldValue = _style[key];

          if (newValue !== oldValue) {
            _style[key] = newValue;
            if (!silent) onStyleChange(key, newValue, oldValue);
          }

        } else {
          OT.warn('Style.setAll::Invalid style property passed ' + key + ' : ' + newValue);
        }
      }

      return this;
    };

    this.set = function(key, value) {
      OT.debug('Publisher.setStyle: ' + key.toString());

      var newValue = castValue(value),
          oldValue;

      if (!isValidStyle(key, newValue)) {
        OT.warn('Style.set::Invalid style property passed ' + key + ' : ' + newValue);
        return this;
      }

      oldValue = _style[key];
      if (newValue !== oldValue) {
        _style[key] = newValue;

        onStyleChange(key, value, oldValue);
      }

      return this;
    };


    if (initalStyles) this.setAll(initalStyles, true);
  };

})(window);
!(function() {

/*
 * A Publishers Microphone.
 *
 * TODO
 * * bind to changes in mute/unmute/volume/etc and respond to them
 */
  OT.Microphone = function(webRTCStream, muted) {
    var _muted,
        _gain = 50;


    Object.defineProperty(this, 'muted', {
      get: function() { return _muted; },
      set: function(muted) {
        if (_muted === muted) return;

        _muted = muted;

        var audioTracks = webRTCStream.getAudioTracks();

        for (var i=0, num=audioTracks.length; i<num; ++i) {
          audioTracks[i].enabled = !_muted;
        }
      }
    });

    Object.defineProperty(this, 'gain', {
      get: function() { return _gain; },
      set: function(gain) {
        OT.warn('OT.Microphone.gain IS NOT YET IMPLEMENTED');
        _gain = gain;
      }
    });

    // Set the initial value
    if (muted !== undefined) {
      this.muted = muted === true;

    } else if (webRTCStream.getAudioTracks().length) {
      this.muted = !webRTCStream.getAudioTracks()[0].enabled;

    } else {
      this.muted = false;
    }

  };

})(window);
!(function() {

  // A Factory method for generating simple state machine classes.
  //
  // @usage
  //    var StateMachine = OT.generateSimpleStateMachine('start', ['start', 'middle', 'end', {
  //      start: ['middle'],
  //      middle: ['end'],
  //      end: ['start']
  //    }]);
  //
  //    var states = new StateMachine();
  //    state.current;            // <-- start
  //    state.set('middle');
  //
  OT.generateSimpleStateMachine = function(initialState, states, transitions) {
    var validStates = states.slice(),
        validTransitions = OT.$.clone(transitions);

    var isValidState = function (state) {
      return validStates.indexOf(state) !== -1;
    };

    var isValidTransition = function(fromState, toState) {
      return validTransitions[fromState] && validTransitions[fromState].indexOf(toState) !== -1;
    };

    return function(stateChangeFailed) {
      var currentState = initialState,
          previousState = null;

      function signalChangeFailed(message, newState) {
        stateChangeFailed({
          message: message,
          newState: newState,
          currentState: currentState,
          previousState: previousState
        });
      }

      // Validates +newState+. If it's invalid it triggers stateChangeFailed and returns false.
      function handleInvalidStateChanges(newState) {
        if (!isValidState(newState)) {
          signalChangeFailed('\'' + newState + '\' is not a valid state', newState);

          return false;
        }

        if (!isValidTransition(currentState, newState)) {
          signalChangeFailed('\'' + currentState + '\' cannot transition to \'' +
            newState + '\'', newState);

          return false;
        }

        return true;
      }


      this.set = function(newState) {
        if (!handleInvalidStateChanges(newState)) return;

        previousState = currentState;
        currentState = newState;
      };

      Object.defineProperties(this, {
        current: {
          get: function() { return currentState; }
        },

        subscribing: {
          get: function() { return currentState === 'Subscribing'; }
        }
      });
    };
  };

})(window);
!(function() {

// Models a Subscriber's subscribing State
//
// Valid States:
//     NotSubscribing            (the initial state
//     Init                      (basic setup of DOM
//     ConnectingToPeer          (Failure Cases -> No Route, Bad Offer, Bad Answer
//     BindingRemoteStream       (Failure Cases -> Anything to do with the media being 
//                               (invalid, the media never plays
//     Subscribing               (this is 'onLoad'
//     Failed                    (terminal state, with a reason that maps to one of the 
//                               (failure cases above
//
//
// Valid Transitions:
//     NotSubscribing ->
//         Init
//
//     Init ->
//             ConnectingToPeer
//           | BindingRemoteStream         (if we are subscribing to ourselves and we alreay 
//                                         (have a stream
//           | NotSubscribing              (destroy()
//
//     ConnectingToPeer ->
//             BindingRemoteStream
//           | NotSubscribing
//           | Failed
//           | NotSubscribing              (destroy()
//
//     BindingRemoteStream ->
//             Subscribing
//           | Failed
//           | NotSubscribing              (destroy()
//
//     Subscribing ->
//             NotSubscribing              (unsubscribe
//           | Failed                      (probably a peer connection failure after we began 
//                                         (subscribing
//
//     Failed ->                           (terminal error state)
//
//
// @example
//     var state = new SubscribingState(function(change) {
//       console.log(change.message);
//     });
//
//     state.set('Init');
//     state.current;                 -> 'Init'
//
//     state.set('Subscribing');      -> triggers stateChangeFailed and logs out the error message
//
//
  var validStates,
      validTransitions,
      initialState = 'NotSubscribing';

  validStates = [
    'NotSubscribing', 'Init', 'ConnectingToPeer', 'BindingRemoteStream', 'Subscribing', 'Failed'
  ];

  validTransitions = {
    NotSubscribing: ['NotSubscribing', 'Init'],
    Init: ['NotSubscribing', 'ConnectingToPeer', 'BindingRemoteStream'],
    ConnectingToPeer: ['NotSubscribing', 'BindingRemoteStream', 'Failed'],
    BindingRemoteStream: ['NotSubscribing', 'Subscribing', 'Failed'],
    Subscribing: ['NotSubscribing', 'Failed'],
    Failed: []
  };

  OT.SubscribingState = OT.generateSimpleStateMachine(initialState, validStates, validTransitions);

  Object.defineProperty(OT.SubscribingState.prototype, 'attemptingToSubscribe', {
    get: function() {
      return [ 'Init', 'ConnectingToPeer', 'BindingRemoteStream' ]
        .indexOf(this.current) !== -1;
    }
  });

})(window);
!(function() {

// Models a Publisher's publishing State
//
// Valid States:
//    NotPublishing
//    GetUserMedia
//    BindingMedia
//    MediaBound
//    PublishingToSession
//    Publishing
//    Failed
//
//
// Valid Transitions:
//    NotPublishing ->
//        GetUserMedia
//
//    GetUserMedia ->
//        BindingMedia
//      | Failed                      (Failure Reasons -> stream error, constraints,
//                                    (permission denied
//      | NotPublishing               (destroy()
//
//
//    BindingMedia ->
//        MediaBound
//      | Failed                      (Failure Reasons -> Anything to do with the media
//                                    (being invalid, the media never plays
//      | NotPublishing               (destroy()
//
//    MediaBound ->
//        PublishingToSession         (MediaBound could transition to PublishingToSession
//                                    (if a stand-alone publish is bound to a session
//      | Failed                      (Failure Reasons -> media issues with a stand-alone publisher
//      | NotPublishing               (destroy()
//
//    PublishingToSession
//        Publishing
//      | Failed                      (Failure Reasons -> timeout while waiting for ack of
//                                    (stream registered. We do not do this right now
//      | NotPublishing               (destroy()
//
//
//    Publishing ->
//        NotPublishing               (Unpublish
//      | Failed                      (Failure Reasons -> loss of network, media error, anything
//                                    (that causes *all* Peer Connections to fail (less than all
//                                    (failing is just an error, all is failure)
//      | NotPublishing               (destroy()
//
//    Failed ->                       (Terminal state
//
//

  var validStates = [
      'NotPublishing', 'GetUserMedia', 'BindingMedia', 'MediaBound',
      'PublishingToSession', 'Publishing', 'Failed'
    ],

    validTransitions = {
      NotPublishing: ['NotPublishing', 'GetUserMedia'],
      GetUserMedia: ['BindingMedia', 'Failed', 'NotPublishing'],
      BindingMedia: ['MediaBound', 'Failed', 'NotPublishing'],
      MediaBound: ['NotPublishing', 'PublishingToSession', 'Failed'],
      PublishingToSession: ['NotPublishing', 'Publishing', 'Failed'],
      Publishing: ['NotPublishing', 'MediaBound', 'Failed'],
      Failed: []
    },

    initialState = 'NotPublishing';

  OT.PublishingState = OT.generateSimpleStateMachine(initialState, validStates, validTransitions);

  Object.defineProperties(OT.PublishingState.prototype, {
    attemptingToPublish: {
      get: function() {
        return [ 'GetUserMedia', 'BindingMedia', 'MediaBound', 'PublishingToSession' ]
          .indexOf(this.current) !== -1;
      }
    },

    publishing: {
      get: function() { return this.current === 'Publishing'; }
    }
  });


})(window);
!(function() {

  // The default constraints
  var defaultConstraints = {
    audio: true,
    video: true
  };

/**
 * The Publisher object  provides the mechanism through which control of the
 * published stream is accomplished. Calling the <code>OT.initPublisher()</code> method
 * creates a Publisher object. </p>
 *
 *  <p>The following code instantiates a session, and publishes an audio-video stream
 *  upon connection to the session: </p>
 *
 *  <pre>
 *  var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
 *  var sessionID = ""; // Replace with your own session ID.
 *                      // See https://dashboard.tokbox.com/projects
 *  var token = ""; // Replace with a generated token that has been assigned the moderator role.
 *                  // See https://dashboard.tokbox.com/projects
 *
 *  var session = OT.initSession(apiKey, sessionID);
 *  session.on("sessionConnected", function(event) {
 *    // This example assumes that a DOM element with the ID 'publisherElement' exists
 *    var publisherProperties = {width: 400, height:300, name:"Bob's stream"};
 *    publisher = OT.initPublisher('publisherElement', publisherProperties);
 *    session.publish(publisher);
 *  });
 *  session.connect(token);
 *  </pre>
 *
 *      <p>This example creates a Publisher object and adds its video to a DOM element
 *      with the ID <code>publisherElement</code> by calling the <code>OT.initPublisher()</code>
 *      method. It then publishes a stream to the session by calling
 *      the <code>publish()</code> method of the Session object.</p>
 *
 * @property {Boolean} accessAllowed Whether the user has granted access to the camera
 * and microphone. The Publisher object dispatches an <code>accessAllowed</code> event when
 * the user grants access. The Publisher object dispatches an <code>accessDenied</code> event
 * when the user denies access.
 * @property {Element} element The HTML DOM element containing the Publisher.
 * @property {String} id The DOM ID of the Publisher.
 * @property {Stream} stream The {@link Stream} object corresponding the the stream of
 * the Publisher.
 * @property {Session} session The {@link Session} to which the Publisher belongs.
 *
 * @see <a href="OT.html#initPublisher">OT.initPublisher</a>
 * @see <a href="Session.html#publish">Session.publish()</a>
 *
 * @class Publisher
 * @augments EventDispatcher
 */
  OT.Publisher = function() {
    // Check that the client meets the minimum requirements, if they don't the upgrade
    // flow will be triggered.
    if (!OT.checkSystemRequirements()) {
      OT.upgradeSystemRequirements();
      return;
    }

    var _guid = OT.Publisher.nextId(),
        _domId,
        _container,
        _targetElement,
        _stream,
        _streamId,
        _webRTCStream,
        _session,
        _peerConnections = {},
        _loaded = false,
        _publishProperties,
        _publishStartTime,
        _microphone,
        _chrome,
        _analytics = new OT.Analytics(),
        _validResolutions,
        _validFrameRates = [ 1, 7, 15, 30 ],
        _qosIntervals = {},
        _prevStats,
        _state,
        _iceServers;

    _validResolutions = {
      '320x240': {width: 320, height: 240},
      '640x480': {width: 640, height: 480},
      '1280x720': {width: 1280, height: 720}
    };

    _prevStats = {
      'timeStamp' : OT.$.now()
    };

    OT.$.eventing(this);

    OT.StylableComponent(this, {
      showMicButton: true,
      showArchiveStatus: true,
      nameDisplayMode: 'auto',
      buttonDisplayMode: 'auto',
      bugDisplayMode: 'auto',
      backgroundImageURI: null
    });

        /// Private Methods
    var logAnalyticsEvent = function(action, variation, payloadType, payload) {
          _analytics.logEvent({
            action: action,
            variation: variation,
            'payload_type': payloadType,
            payload: payload,
            'session_id': _session ? _session.sessionId : null,
            'connection_id': _session &&
              _session.connected ? _session.connection.connectionId : null,
            'partner_id': _session ? _session.apiKey : OT.APIKEY,
            streamId: _stream ? _stream.id : null,
            'widget_id': _guid,
            'widget_type': 'Publisher'
          });
        },

        recordQOS = function(connectionId) {
          var QoSBlob = {
            'widget_type': 'Publisher',
            'stream_type': 'WebRTC',
            sessionId: _session ? _session.sessionId : null,
            connectionId: _session && _session.connected ? _session.connection.connectionId : null,
            partnerId: _session ? _session.apiKey : OT.APIKEY,
            streamId: _stream ? _stream.id : null,
            widgetId: _guid,
            version: OT.properties.version,
            'media_server_name': _session ? _session.sessionInfo.messagingServer : null,
            p2pFlag: _session ? _session.sessionInfo.p2pEnabled : false,
            duration: _publishStartTime ? new Date().getTime() - _publishStartTime.getTime() : 0,
            'remote_connection_id': connectionId
          };

          // get stats for each connection id
          _peerConnections[connectionId].getStats(_prevStats, function(stats) {
            var statIndex;
            if (stats) {
              for (statIndex in stats) {
                QoSBlob[statIndex] = stats[statIndex];
              }
            }
            _analytics.logQOS(QoSBlob);
          });
        },

        /// Private Events

        stateChangeFailed = function(changeFailed) {
          OT.error('Publisher State Change Failed: ', changeFailed.message);
          OT.debug(changeFailed);
        },

        onLoaded = function() {
          OT.debug('OT.Publisher.onLoaded');

          _state.set('MediaBound');
          _container.loading = false;
          _loaded = true;

          _createChrome.call(this);

          this.trigger('initSuccess');
          this.trigger('loaded', this);
        },

        onLoadFailure = function(reason) {
          logAnalyticsEvent('publish', 'Failure', 'reason',
            'Publisher PeerConnection Error: ' + reason);

          _state.set('Failed');
          this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.P2P_CONNECTION_FAILED,
            'Publisher PeerConnection Error: ' + reason));

          OT.handleJsException('Publisher PeerConnection Error: ' + reason,
            OT.ExceptionCodes.P2P_CONNECTION_FAILED, {
            session: _session,
            target: this
          });
        },

        onStreamAvailable = function(webOTStream) {
          OT.debug('OT.Publisher.onStreamAvailable');

          _state.set('BindingMedia');

          cleanupLocalStream();
          _webRTCStream = webOTStream;

          _microphone = new OT.Microphone(_webRTCStream, !_publishProperties.publishAudio);
          this.publishVideo(_publishProperties.publishVideo &&
            _webRTCStream.getVideoTracks().length > 0);

          this.dispatchEvent(
            new OT.Event(OT.Event.names.ACCESS_ALLOWED, false)
          );

          _targetElement = new OT.VideoElement({
            attributes: {muted:true}
          });

          _targetElement.on({
            streamBound: onLoaded,
            loadError: onLoadFailure,
            error: onVideoError
          }, this)
            .bindToStream(_webRTCStream);

          _container.video = _targetElement;
        },

        onStreamAvailableError = function(error) {
          OT.error('OT.Publisher.onStreamAvailableError ' + error.name + ': ' + error.message);

          _state.set('Failed');
          this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
              error.message));

          if (_container) _container.destroy();

          logAnalyticsEvent('publish', 'Failure', 'reason',
            'GetUserMedia:Publisher failed to access camera/mic: ' + error.message);

          OT.handleJsException('Publisher failed to access camera/mic: ' + error.message,
          OT.ExceptionCodes.UNABLE_TO_PUBLISH, {
            session: _session,
            target: this
          });
        },

        // The user has clicked the 'deny' button the the allow access dialog
        // (or it's set to always deny)
        onAccessDenied = function(error) {
          OT.error('OT.Publisher.onStreamAvailableError Permission Denied');

          _state.set('Failed');
          this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
              'Publisher Access Denied: Permission Denied' +
                  (error.message ? ': ' + error.message : '')));

          logAnalyticsEvent('publish', 'Failure', 'reason',
            'GetUserMedia:Publisher Access Denied: Permission Denied');

          var browser = OT.$.browserVersion();

          var event = new OT.Event(OT.Event.names.ACCESS_DENIED),
            defaultAction = function() {
              if(!event.isDefaultPrevented()) {
                if(browser.browser === 'Chrome') {
                  if (_container) {
                    _container.addError('', null, 'OT_publisher-denied-chrome');
                  }
                  if(!accessDialogWasOpened) {
                    OT.Dialogs.AllowDeny.Chrome.previouslyDenied(window.location.hostname);
                  } else {
                    OT.Dialogs.AllowDeny.Chrome.deniedNow();
                  }
                } else if(browser.browser === 'Firefox') {
                  if(_container) {
                    _container.addError('', 'Click the reload button in the URL bar to change ' +
                      'camera & mic settings.', 'OT_publisher-denied-firefox');
                  }
                  OT.Dialogs.AllowDeny.Firefox.denied().on({
                    refresh: function() {
                      window.location.reload();
                    }
                  });
                }
              }
            };

          this.dispatchEvent(event, defaultAction);
        },

        accessDialogPrompt,
        accessDialogFirefoxTimeout,
        accessDialogWasOpened = false,

        onAccessDialogOpened = function() {

          accessDialogWasOpened = true;

          logAnalyticsEvent('accessDialog', 'Opened', '', '');

          var browser = OT.$.browserVersion();

          this.dispatchEvent(
            new OT.Event(OT.Event.names.ACCESS_DIALOG_OPENED, true),
            function(event) {
              if(!event.isDefaultPrevented()) {
                if(browser.browser === 'Chrome') {
                  accessDialogPrompt = OT.Dialogs.AllowDeny.Chrome.initialPrompt();
                } else if(browser.browser === 'Firefox') {
                  accessDialogFirefoxTimeout = setTimeout(function() {
                    accessDialogPrompt = OT.Dialogs.AllowDeny.Firefox.maybeDenied();
                  }, 7000);
                }
              }
            }
          );
        },

        onAccessDialogClosed = function() {
          logAnalyticsEvent('accessDialog', 'Closed', '', '');

          if(accessDialogFirefoxTimeout) {
            clearTimeout(accessDialogFirefoxTimeout);
            accessDialogFirefoxTimeout = null;
          }

          if(accessDialogPrompt) {
            accessDialogPrompt.close();
            accessDialogPrompt = null;
          }

          this.dispatchEvent(
            new OT.Event(OT.Event.names.ACCESS_DIALOG_CLOSED, false)
          );
        },

        onVideoError = function(errorCode, errorReason) {
          OT.error('OT.Publisher.onVideoError');

          var message = errorReason + (errorCode ? ' (' + errorCode + ')' : '');
          logAnalyticsEvent('stream', null, 'reason',
            'Publisher while playing stream: ' + message);

          _state.set('Failed');

          if (_state.attemptingToPublish) {
            this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
                message));
          } else {
            this.trigger('error', message);
          }

          OT.handleJsException('Publisher error playing stream: ' + message,
          OT.ExceptionCodes.UNABLE_TO_PUBLISH, {
            session: _session,
            target: this
          });
        },

        onPeerDisconnected = function(peerConnection) {
          OT.debug('OT.Subscriber has been disconnected from the Publisher\'s PeerConnection');

          this.cleanupSubscriber(peerConnection.remoteConnection.id);
        },

        onPeerConnectionFailure = function(code, reason, peerConnection, prefix) {
          logAnalyticsEvent('publish', 'Failure', 'reason|hasRelayCandidates',
            (prefix ? prefix : '') + [':Publisher PeerConnection with connection ' +
              (peerConnection && peerConnection.remoteConnection &&
              peerConnection.remoteConnection.id)  + ' failed: ' +
              reason, peerConnection.hasRelayCandidates
          ].join('|'));

          OT.handleJsException('Publisher PeerConnection Error: ' + reason,
          OT.ExceptionCodes.UNABLE_TO_PUBLISH, {
            session: _session,
            target: this
          });

          // We don't call cleanupSubscriber as it also logs a
          // disconnected analytics event, which we don't want in this
          // instance. The duplication is crufty though and should
          // be tidied up.
          clearInterval(_qosIntervals[peerConnection.remoteConnection.id]);
          delete _qosIntervals[peerConnection.remoteConnection.id];

          delete _peerConnections[peerConnection.remoteConnection.id];
        },

        /// Private Helpers

        // Assigns +stream+ to this publisher. The publisher listens
        // for a bunch of events on the stream so it can respond to
        // changes.
        assignStream = function(stream) {
          _stream = stream;
          _stream.on('destroyed', this.disconnect, this);

          _state.set('Publishing');
          _publishStartTime = new Date();

          this.trigger('publishComplete', null, this);
      
          this.dispatchEvent(new OT.StreamEvent('streamCreated', stream, null, false));
          
          logAnalyticsEvent('publish', 'Success', 'streamType:streamId', 'WebRTC:' + _streamId);
        }.bind(this),

        // Clean up our LocalMediaStream
        cleanupLocalStream = function() {
          if (_webRTCStream) {
            // Stop revokes our access cam and mic access for this instance
            // of localMediaStream.
            _webRTCStream.stop();
            _webRTCStream = null;
          }
        },

        createPeerConnectionForRemote = function(remoteConnection) {
          var peerConnection = _peerConnections[remoteConnection.id];

          if (!peerConnection) {
            var startConnectingTime = OT.$.now();

            logAnalyticsEvent('createPeerConnection', 'Attempt', '', '');

            // Cleanup our subscriber when they disconnect
            remoteConnection.on('destroyed',
              this.cleanupSubscriber.bind(this, remoteConnection.id));

            peerConnection = _peerConnections[remoteConnection.id] = new OT.PublisherPeerConnection(
              remoteConnection,
              _session,
              _streamId,
              _webRTCStream
            );

            peerConnection.on({
              connected: function() {
                logAnalyticsEvent('createPeerConnection', 'Success', 'pcc|hasRelayCandidates', [
                  parseInt(OT.$.now() - startConnectingTime, 10),
                  peerConnection.hasRelayCandidates
                ].join('|'));

                // start recording the QoS for this peer connection
                _qosIntervals[remoteConnection.id] = setInterval(function() {
                  recordQOS(remoteConnection.id);
                }, 30000);
              },
              disconnected: onPeerDisconnected,
              error: onPeerConnectionFailure
            }, this);

            peerConnection.init(_iceServers);
          }

          return peerConnection;
        },

        /// Chrome

        // If mode is false, then that is the mode. If mode is true then we'll
        // definitely display  the button, but we'll defer the model to the
        // Publishers buttonDisplayMode style property.
        chromeButtonMode = function(mode) {
          if (mode === false) return 'off';

          var defaultMode = this.getStyle('buttonDisplayMode');

          // The default model is false, but it's overridden by +mode+ being true
          if (defaultMode === false) return 'on';

          // defaultMode is either true or auto.
          return defaultMode;
        },

        updateChromeForStyleChange = function(key, value) {
          if (!_chrome) return;

          switch(key) {
            case 'nameDisplayMode':
              _chrome.name.setDisplayMode(value);
              _chrome.backingBar.nameMode = value;
              break;

            case 'showArchiveStatus':
              logAnalyticsEvent('showArchiveStatus', 'styleChange', 'mode', value ? 'on': 'off');
              _chrome.archive.setShowArchiveStatus(value);
              break;

            case 'buttonDisplayMode':
            case 'showMicButton':
              // _chrome.muteButton.setDisplayMode(
              //     chromeButtonMode.call(this, this.getStyle('showMicButton'))
              // );
            case 'bugDisplayMode':
              // _chrome.name.bugMode = value;
          }
        },

        _createChrome = function() {
          if(this.getStyle('bugDisplayMode') === 'off') {
            logAnalyticsEvent('bugDisplayMode', 'createChrome', 'mode', 'off');
          }
          if(!this.getStyle('showArchiveStatus')) {
            logAnalyticsEvent('showArchiveStatus', 'createChrome', 'mode', 'off');
          }
          _chrome = new OT.Chrome({
            parent: _container.domElement
          }).set({

            backingBar: new OT.Chrome.BackingBar({
              nameMode: this.getStyle('nameDisplayMode'),
              muteMode: chromeButtonMode.call(this, this.getStyle('showMicButton'))
            }),

            name: new OT.Chrome.NamePanel({
              name: _publishProperties.name,
              mode: this.getStyle('nameDisplayMode'),
              bugMode: this.getStyle('bugDisplayMode')
            }),

            muteButton: new OT.Chrome.MuteButton({
              muted: _publishProperties.publishAudio === false,
              mode: chromeButtonMode.call(this, this.getStyle('showMicButton'))
            }),

            opentokButton: new OT.Chrome.OpenTokButton({
              mode: this.getStyle('bugDisplayMode')
            }),

            archive: new OT.Chrome.Archiving({
              show: this.getStyle('showArchiveStatus'),
              archiving: false
            })

          }).on({
            muted: this.publishAudio.bind(this, false),
            unmuted: this.publishAudio.bind(this, true)
          });
        },

        reset = function() {
          if (_chrome) {
            _chrome.destroy();
            _chrome = null;
          }

          this.disconnect();

          _microphone = null;

          if (_targetElement) {
            _targetElement.destroy();
            _targetElement = null;
          }

          cleanupLocalStream();

          if (_container) {
            _container.destroy();
            _container = null;
          }

          if (this.session) {
            this._.unpublishFromSession(this.session, 'reset');
          }

          // clear all the intervals
          for (var connId in _qosIntervals) {
            clearInterval(_qosIntervals[connId]);
            delete _qosIntervals[connId];
          }

          _domId = null;
          _stream = null;
          _loaded = false;

          _session = null;
          
          _state.set('NotPublishing');
        }.bind(this);

    this.publish = function(targetElement, properties) {
      OT.debug('OT.Publisher: publish');

      if ( _state.attemptingToPublish || _state.publishing ) reset();
      _state.set('GetUserMedia');

      _publishProperties = OT.$.defaults(properties || {}, {
        publishAudio : true,
        publishVideo : true,
        mirror: true
      });
        
      if (!_publishProperties.constraints) {
        _publishProperties.constraints = OT.$.clone(defaultConstraints);
        if (_publishProperties.resolution) {
          if (_publishProperties.resolution !== void 0 &&
            !_validResolutions.hasOwnProperty(_publishProperties.resolution)) {
            OT.warn('Invalid resolution passed to the Publisher. Got: ' +
              _publishProperties.resolution + ' expecting one of "' +
              Object.keys(_validResolutions).join('","') + '"');
          } else {
            _publishProperties.videoDimensions = _validResolutions[_publishProperties.resolution];
            _publishProperties.constraints.video = {
              mandatory: {},
              optional: [
                {minWidth: _publishProperties.videoDimensions.width},
                {maxWidth: _publishProperties.videoDimensions.width},
                {minHeight: _publishProperties.videoDimensions.height},
                {maxHeight: _publishProperties.videoDimensions.height}
              ]
            };
          }
        }
    
        if (_publishProperties.frameRate !== void 0 &&
          _validFrameRates.indexOf(_publishProperties.frameRate) === -1) {
          OT.warn('Invalid frameRate passed to the publisher got: ' +
            _publishProperties.frameRate + ' expecting one of ' + _validFrameRates.join(','));
          delete _publishProperties.frameRate;
        } else if (_publishProperties.frameRate) {
          if (!_publishProperties.constraints.video.optional) {
            if (typeof _publishProperties.constraints.video !== 'object') {
              _publishProperties.constraints.video = {};
            }
            _publishProperties.constraints.video.optional = [];
          }
          _publishProperties.constraints.video.optional.push({
            minFrameRate: _publishProperties.frameRate
          });
          _publishProperties.constraints.video.optional.push({
            maxFrameRate: _publishProperties.frameRate
          });
        }
      } else {
        OT.warn('You have passed your own constraints not using ours');
      }

      if (_publishProperties.style) {
        this.setStyle(_publishProperties.style, null, true);
      }

      if (_publishProperties.name) {
        _publishProperties.name = _publishProperties.name.toString();
      }

      _publishProperties.classNames = 'OT_root OT_publisher';

      // Defer actually creating the publisher DOM nodes until we know
      // the DOM is actually loaded.
      OT.onLoad(function() {
        _container = new OT.WidgetView(targetElement, _publishProperties);
        _domId = _container.domId;

        OT.$.getUserMedia(
          _publishProperties.constraints,
          onStreamAvailable.bind(this),
          onStreamAvailableError.bind(this),
          onAccessDialogOpened.bind(this),
          onAccessDialogClosed.bind(this),
          onAccessDenied.bind(this)
        );
      }, this);

      return this;
    };

 /**
  * Starts publishing audio (if it is currently not being published)
  * when the <code>value</code> is <code>true</code>; stops publishing audio
  * (if it is currently being published) when the <code>value</code> is <code>false</code>.
  *
  * @param {Boolean} value Whether to start publishing audio (<code>true</code>)
  * or not (<code>false</code>).
  *
  * @see <a href="OT.html#initPublisher">OT.initPublisher()</a>
  * @see <a href="Stream.html#hasAudio">Stream.hasAudio</a>
  * @see StreamPropertyChangedEvent
  * @method #publishAudio
  * @memberOf Publisher
  */
    this.publishAudio = function(value) {
      _publishProperties.publishAudio = value;

      if (_microphone) {
        _microphone.muted = !value;
      }
      
      if (_chrome) {
        _chrome.muteButton.muted = !value;
      }

      if (_session && _stream) {
        _stream.setChannelActiveState('audio', value);
      }

      return this;
    };


 /**
  * Starts publishing video (if it is currently not being published)
  * when the <code>value</code> is <code>true</code>; stops publishing video
  * (if it is currently being published) when the <code>value</code> is <code>false</code>.
  *
  * @param {Boolean} value Whether to start publishing video (<code>true</code>)
  * or not (<code>false</code>).
  *
  * @see <a href="OT.html#initPublisher">OT.initPublisher()</a>
  * @see <a href="Stream.html#hasVideo">Stream.hasVideo</a>
  * @see StreamPropertyChangedEvent
  * @method #publishVideo
  * @memberOf Publisher
  */
    this.publishVideo = function(value) {
      var oldValue = _publishProperties.publishVideo;
      _publishProperties.publishVideo = value;

      if (_session && _stream && _publishProperties.publishVideo !== oldValue) {
        _stream.setChannelActiveState('video', value);
      }

      // We currently do this event if the value of publishVideo has not changed
      // This is because the state of the video tracks enabled flag may not match
      // the value of publishVideo at this point. This will be tidied up shortly.
      if (_webRTCStream) {
        var videoTracks = _webRTCStream.getVideoTracks();
        for (var i=0, num=videoTracks.length; i<num; ++i) {
          videoTracks[i].enabled = value;
        }
      }

      if(_container) {
        _container.showPoster = !value;
      }

      return this;
    };

    this.recordQOS = function() {
      // have to record QoS for every peer connection
      for (var connId in _peerConnections) {
        recordQOS(connId);
      }
    };

    /**
    * Deletes the Publisher object and removes it from the HTML DOM.
    * <p>
    * The Publisher object dispatches a <code>destroyed</code> event when the DOM
    * element is removed.
    * </p>
    * @method #destroy
    * @memberOf Publisher
    * @return {Publisher} The Publisher.
    */
    this.destroy = function(/* unused */ reason, quiet) {
      reset();

      if (quiet !== true) {
        this.dispatchEvent(
          new OT.DestroyedEvent(
            OT.Event.names.PUBLISHER_DESTROYED,
            this,
            reason
          ),
          this.off.bind(this)
        );
      }

      return this;
    };

    /**
    * @methodOf Publisher
    * @private
    */
    this.disconnect = function() {
      // Close the connection to each of our subscribers
      for (var fromConnectionId in _peerConnections) {
        this.cleanupSubscriber(fromConnectionId);
      }
    };

    this.cleanupSubscriber = function(fromConnectionId) {
      var pc = _peerConnections[fromConnectionId];

      clearInterval(_qosIntervals[fromConnectionId]);
      delete _qosIntervals[fromConnectionId];

      if (pc) {
        pc.destroy();
        delete _peerConnections[fromConnectionId];

        logAnalyticsEvent('disconnect', 'PeerConnection',
          'subscriberConnection', fromConnectionId);
      }
    };


    this.processMessage = function(type, fromConnection, message) {
      OT.debug('OT.Publisher.processMessage: Received ' + type + ' from ' + fromConnection.id);
      OT.debug(message);

      switch (type) {
        case 'unsubscribe':
          this.cleanupSubscriber(message.content.connection.id);
          break;

        default:
          var peerConnection = createPeerConnectionForRemote.call(this, fromConnection);
          peerConnection.processMessage(type, message);
      }
    };

    /**
    * Returns the base-64-encoded string of PNG data representing the Publisher video.
    *
    *   <p>You can use the string as the value for a data URL scheme passed to the src parameter of
    *   an image file, as in the following:</p>
    *
    * <pre>
    *  var imgData = publisher.getImgData();
    *
    *  var img = document.createElement("img");
    *  img.setAttribute("src", "data:image/png;base64," + imgData);
    *  var imgWin = window.open("about:blank", "Screenshot");
    *  imgWin.document.write("&lt;body&gt;&lt;/body&gt;");
    *  imgWin.document.body.appendChild(img);
    * </pre>
    *
    * @method #getImgData
    * @memberOf Publisher
    * @return {String} The base-64 encoded string. Returns an empty string if there is no video.
    */

    this.getImgData = function() {
      if (!_loaded) {
        OT.error('OT.Publisher.getImgData: Cannot getImgData before the Publisher is publishing.');

        return null;
      }

      return _targetElement.imgData;
    };


    // API Compatibility layer for Flash Publisher, this could do with some tidyup.
    this._ = {
      publishToSession: function(session) {
        // Add session property to Publisher
        this.session = session;

        var createStream = function() {

          var streamWidth,
              streamHeight;

          // Bail if this.session is gone, it means we were unpublished
          // before createStream could finish.
          if (!this.session) return;

          _state.set('PublishingToSession');

          var onStreamRegistered = function(err, streamId, message) {
            if (err) {
              // @todo we should respect err.code here and translate it to the local
              // client equivalent.
              logAnalyticsEvent('publish', 'Failure', 'reason',
                'Publish:' + OT.ExceptionCodes.UNABLE_TO_PUBLISH + ':' + err.message);
              if (_state.attemptingToPublish) {
                this.trigger('publishComplete', new OT.Error(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
                  err.message));
              }
              return;
            }

            _streamId = streamId;
            
            _iceServers = OT.Raptor.parseIceServers(message);
          }.bind(this);
              
          // We set the streamWidth and streamHeight to be the minimum of the requested
          // resolution and the actual resolution.
          if (_publishProperties.videoDimensions) {
            streamWidth = Math.min(_publishProperties.videoDimensions.width,
              _targetElement.videoWidth || 640);
            streamHeight = Math.min(_publishProperties.videoDimensions.height,
              _targetElement.videoHeight || 480);
          } else {
            streamWidth = _targetElement.videoWidth || 640;
            streamHeight = _targetElement.videoHeight || 480;
          }

          session._.streamCreate(
            _publishProperties && _publishProperties.name ? _publishProperties.name : '',
            OT.VideoOrientation.ROTATED_NORMAL,
            streamWidth,
            streamHeight,
            _publishProperties.publishAudio,
            _publishProperties.publishVideo,
            _publishProperties.frameRate,
            onStreamRegistered
          );
        };

        if (_loaded) createStream.call(this);
        else this.on('initSuccess', createStream, this);

        logAnalyticsEvent('publish', 'Attempt', 'streamType', 'WebRTC');

        return this;
      }.bind(this),

      unpublishFromSession: function(session, reason) {
        if (!this.session || session.id !== this.session.id) {
          OT.warn('The publisher ' + this.guid + ' is trying to unpublish from a session ' +
            session.id + ' it is not attached to (' +
            (this.session && this.session.id || 'no this.session') + ')');
          return this;
        }

        if (session.connected && this.stream) {
          session._.streamDestroy(this.stream.id);
        }

        // Disconnect immediately, rather than wait for the WebSocket to
        // reply to our destroyStream message.
        this.disconnect();
        this.session = null;

        // We're back to being a stand-alone publisher again.
        _state.set('MediaBound');

        logAnalyticsEvent('unpublish', 'Success', 'sessionId', session.id);

        this._.streamDestroyed(reason);

        return this;
      }.bind(this),
      
      streamDestroyed: function(reason) {
        if(['reset'].indexOf(reason) < 0) {
          var event = new OT.StreamEvent('streamDestroyed', _stream, reason, true);
          var defaultAction = function() {
            if(!event.isDefaultPrevented()) {
              this.destroy();
            }
          }.bind(this);
          this.dispatchEvent(event, defaultAction);
        }
      }.bind(this),

      archivingStatus: function(status) {
        if(_chrome) {
          _chrome.archive.setArchiving(status);
        }
        return status;
      }.bind(this)
    };

    this.detectDevices = function() {
      OT.warn('Fixme: Haven\'t implemented detectDevices');
    };

    this.detectMicActivity = function() {
      OT.warn('Fixme: Haven\'t implemented detectMicActivity');
    };

    this.getEchoCancellationMode = function() {
      OT.warn('Fixme: Haven\'t implemented getEchoCancellationMode');
      return 'fullDuplex';
    };

    this.setMicrophoneGain = function() {
      OT.warn('Fixme: Haven\'t implemented setMicrophoneGain');
    };

    this.getMicrophoneGain = function() {
      OT.warn('Fixme: Haven\'t implemented getMicrophoneGain');
      return 0.5;
    };

    this.setCamera = function() {
      OT.warn('Fixme: Haven\'t implemented setCamera');
    };

    this.setMicrophone = function() {
      OT.warn('Fixme: Haven\'t implemented setMicrophone');
    };


    Object.defineProperties(this, {
      id: {
        get: function() { return _domId; },
        enumerable: true
      },
        
      element: {
        get: function() { return _container.domElement; },
        enumerable: true
      },

      guid: {
        get: function() { return _guid; },
        enumerable: true
      },

      stream: {
        get: function() { return _stream; },
        set: function(stream) {
          assignStream(stream);
        },
        enumerable: true
      },

      streamId: {
        get: function() { return _streamId; },
        enumerable: true
      },

      targetElement: {
        get: function() { return _targetElement.domElement; }
      },

      domId: {
        get: function() { return _domId; }
      },

      session: {
        get: function() { return _session; },
        set: function(session) {
          _session = session;
        },
        enumerable: true
      },

      isWebRTC: {
        get: function() { return true; }
      },

      loading: {
        get: function(){
          return _container && _container.loading;
        }
      },
        
      videoWidth: {
        get: function() { return _targetElement.videoWidth; },
        enumerable: true
      },
        
      videoHeight: {
        get: function() { return _targetElement.videoHeight; },
        enumerable: true
      }
    });

    Object.defineProperty(this._, 'webRtcStream', {
      get: function() { return _webRTCStream; }
    });

    this.on('styleValueChanged', updateChromeForStyleChange, this);
    _state = new OT.PublishingState(stateChangeFailed);


	/**
	* Dispatched when the user has clicked the Allow button, granting the
	* app access to the camera and microphone. The Publisher object has an
	* <code>accessAllowed</code> property which indicates whether the user
	* has granted access to the camera and microphone.
	* @see Event
	* @name accessAllowed
	* @event
	* @memberof Publisher
	*/

	/**
	* Dispatched when the user has clicked the Deny button, preventing the
	* app from having access to the camera and microphone.
	* <p>
	* The default behavior of this event is to display a user interface element
	* in the Publisher object, indicating that that user has denied access to
	* the camera and microphone. Call the <code>preventDefault()</code> method
	* method of the event object in the event listener to prevent this message
	* from being displayed.
	* @see Event
	* @name accessDenied
	* @event
	* @memberof Publisher
	*/

	/**
	* Dispatched when the Allow/Deny dialog box is opened. (This is the dialog box in which
	* the user can grant the app access to the camera and microphone.)
	* <p>
	* The default behavior of this event is to display a message in the browser that instructs
	* the user how to enable the camera and microphone. Call the <code>preventDefault()</code>
	* method of the event object in the event listener to prevent this message from being displayed.
	* @see Event
	* @name accessDialogOpened
	* @event
	* @memberof Publisher
	*/

	/**
	* Dispatched when the Allow/Deny box is closed. (This is the dialog box in which the
	* user can grant the app access to the camera and microphone.)
	* @see Event
	* @name accessDialogClosed
	* @event
	* @memberof Publisher
	*/

	/**
	 * The publisher has started streaming to the session.
	 * @name streamCreated
	 * @event
	 * @memberof Publisher
	 * @see StreamEvent
	 * @see <a href="Session.html#publish">Session.publish()</a>
	 */

	/**
	 * The publisher has stopped streaming to the session. The default behavior is that
	 * the Publisher object is removed from the HTML DOM). The Publisher object dispatches a 
	 * <code>destroyed</code> event when the element is removed from the HTML DOM. If you call the 
	 * <code>preventDefault()</code> method of the event object in the event listener, the default
	 * behavior is prevented, and you can, optionally, retain the Publisher for reuse or clean it up
	 * using your own code.
	 * @name streamDestroyed
	 * @event
	 * @memberof Publisher
	 * @see StreamEvent
	 */

	/**
	* Dispatched when the Publisher element is removed from the HTML DOM. When this event
  * is dispatched, you may choose to adjust or remove HTML DOM elements related to the publisher.
	* @name destroyed
	* @event
	* @memberof Publisher
	*/
  };

  // Helper function to generate unique publisher ids
  OT.Publisher.nextId = OT.$.uuid;

})(window);
!(function(window) {

/**
 * The Subscriber object is a representation of the local video element that is playing back
 * a remote stream. The Subscriber object includes methods that let you disable and enable
 * local audio playback for the subscribed stream. The <code>subscribe()</code> method of the
 * {@link Session} object returns a Subscriber object.
 *
 * @property {Element} element The HTML DOM element containing the Subscriber.
 * @property {String} id The DOM ID of the Subscriber.
 * @property {Stream} stream The stream to which you are subscribing.
 *
 * @class Subscriber
 * @augments EventDispatcher
 */
  OT.Subscriber = function(targetElement, options) {
    var _widgetId = OT.$.uuid(),
        _domId = targetElement || _widgetId,
        _container,
        _streamContainer,
        _chrome,
        _stream,
        _fromConnectionId,
        _peerConnection,
        _session = options.session,
        _subscribeStartTime,
        _startConnectingTime,
        _qosInterval,
        _properties = OT.$.clone(options),
        _analytics = new OT.Analytics(),
        _audioVolume = 50,
        _state,
        _subscribeAudioFalseWorkaround, // OPENTOK-6844
        _prevStats,
        _lastSubscribeToVideoReason;

    _prevStats = {
      timeStamp: OT.$.now()
    };

    if (!_session) {
      OT.handleJsException('Subscriber must be passed a session option', 2000, {
        session: _session,
        target: this
      });

      return;
    }

    OT.$.eventing(this);

    OT.StylableComponent(this, {
      nameDisplayMode: 'auto',
      buttonDisplayMode: 'auto',
      backgroundImageURI: null,
      showArchiveStatus: true,
      showMicButton: true
    });

    var logAnalyticsEvent = function(action, variation, payloadType, payload) {
          /* jshint camelcase:false*/
          _analytics.logEvent({
            action: action,
            variation: variation,
            payload_type: payloadType,
            payload: payload,
            stream_id: _stream ? _stream.id : null,
            session_id: _session ? _session.sessionId : null,
            connection_id: _session && _session.connected ? _session.connection.connectionId : null,
            partner_id: _session && _session.connected ? _session.sessionInfo.partnerId : null,
            widget_id: _widgetId,
            widget_type: 'Subscriber'
          });
        },

        recordQOS = function() {
          if(_state.subscribing && _session && _session.connected) {
            /*jshint camelcase:false */
            var QoSBlob = {
              widget_type: 'Subscriber',
              stream_type : 'WebRTC',
              session_id: _session ? _session.sessionId : null,
              connectionId: _session ? _session.connection.connectionId : null,
              media_server_name: _session ? _session.sessionInfo.messagingServer : null,
              p2pFlag: _session ? _session.sessionInfo.p2pEnabled : false,
              partner_id: _session ? _session.apiKey : null,
              stream_id: _stream.id,
              widget_id: _widgetId,
              version: OT.properties.version,
              duration: parseInt(OT.$.now() - _subscribeStartTime, 10),
              remote_connection_id: _stream.connection.connectionId
            };

            // get stats for each connection id
            _peerConnection.getStats(_prevStats, function(stats) {
              var statIndex;
              if (stats) {
                for (statIndex in stats) {
                  QoSBlob[statIndex] = stats[statIndex];
                }
              }
              _analytics.logQOS(QoSBlob);
            });
          }
        },

        stateChangeFailed = function(changeFailed) {
          OT.error('Subscriber State Change Failed: ', changeFailed.message);
          OT.debug(changeFailed);
        },

        onLoaded = function() {
          if (_state.subscribing || !_streamContainer) return;

          OT.debug('OT.Subscriber.onLoaded');

          _state.set('Subscribing');
          _subscribeStartTime = OT.$.now();

          logAnalyticsEvent('createPeerConnection', 'Success', 'pcc|hasRelayCandidates', [
            parseInt(_subscribeStartTime - _startConnectingTime, 10),
            _peerConnection && _peerConnection.hasRelayCandidates
          ].join('|'));

          _qosInterval = setInterval(recordQOS, 30000);

          if(_subscribeAudioFalseWorkaround) {
            _subscribeAudioFalseWorkaround = null;
            this.subscribeToVideo(false);
          }

          _container.loading = false;

          _createChrome.call(this);

          this.trigger('subscribeComplete', null, this);
          this.trigger('loaded', this);

          logAnalyticsEvent('subscribe', 'Success', 'streamId', _stream.id);
        },

        onDisconnected = function() {
          OT.debug('OT.Subscriber has been disconnected from the Publisher\'s PeerConnection');

          if (_state.attemptingToSubscribe) {
            // subscribing error
            _state.set('Failed');
            this.trigger('subscribeComplete', new OT.Error(null, 'ClientDisconnected'));

          } else if (_state.subscribing) {
            _state.set('Failed');

            // we were disconnected after we were already subscribing
            // probably do nothing?
          }

          this.disconnect();
        },

        onPeerConnectionFailure = function(code, reason, peerConnection, prefix) {
          if (_state.attemptingToSubscribe) {
            // We weren't subscribing yet so this was a failure in setting
            // up the PeerConnection or receiving the initial stream.
            logAnalyticsEvent('createPeerConnection', 'Failure', 'reason|hasRelayCandidates', [
              'Subscriber PeerConnection Error: ' + reason,
              _peerConnection && _peerConnection.hasRelayCandidates
            ].join('|'));

            _state.set('Failed');
            this.trigger('subscribeComplete', new OT.Error(null, reason));

          } else if (_state.subscribing) {
            // we were disconnected after we were already subscribing
            _state.set('Failed');
            this.trigger('error', reason);
          }

          this.disconnect();

          logAnalyticsEvent('subscribe', 'Failure', 'reason',
            (prefix ? prefix : '') + ':Subscriber PeerConnection Error: ' + reason);

          OT.handleJsException('Subscriber PeerConnection Error: ' + reason,
            OT.ExceptionCodes.P2P_CONNECTION_FAILED, {
              session: _session,
              target: this
            }
          );
          _showError.call(this, reason);
        },

        onRemoteStreamAdded = function(webOTStream) {
          OT.debug('OT.Subscriber.onRemoteStreamAdded');

          _state.set('BindingRemoteStream');

          // Disable the audio/video, if needed
          this.subscribeToAudio(_properties.subscribeToAudio);

          var preserver = _subscribeAudioFalseWorkaround;
          this.subscribeToVideo(_properties.subscribeToVideo);
          _subscribeAudioFalseWorkaround = preserver;

          var streamElement = new OT.VideoElement();

          // Initialize the audio volume
          streamElement.setAudioVolume(_audioVolume);
          streamElement.on({
            streamBound: onLoaded,
            loadError: onPeerConnectionFailure,
            error: onPeerConnectionFailure
          }, this);

          streamElement.bindToStream(webOTStream);
          _container.video = streamElement;

          _streamContainer = streamElement;

          _streamContainer.orientation = {
            width: _stream.videoDimensions.width,
            height: _stream.videoDimensions.height,
            videoOrientation: _stream.videoDimensions.orientation
          };

          logAnalyticsEvent('createPeerConnection', 'StreamAdded', '', '');
          this.trigger('streamAdded', this);
        },

        onRemoteStreamRemoved = function(webOTStream) {
          OT.debug('OT.Subscriber.onStreamRemoved');

          if (_streamContainer.stream === webOTStream) {
            _streamContainer.destroy();
            _streamContainer = null;
          }


          this.trigger('streamRemoved', this);
        },

        streamDestroyed = function () {
          this.disconnect();
        },

        streamUpdated = function(event) {

          switch(event.changedProperty) {
            case 'videoDimensions':
              _streamContainer.orientation = {
                width: event.newValue.width,
                height: event.newValue.height,
                videoOrientation: event.newValue.orientation
              };
              break;

            case 'hasVideo':
              if(_container) {
                _container.showPoster = !(_stream.hasVideo && _properties.subscribeToVideo);
              }
              break;

            case 'hasAudio':
              // noop
          }
        },

        /// Chrome

        // If mode is false, then that is the mode. If mode is true then we'll
        // definitely display  the button, but we'll defer the model to the
        // Publishers buttonDisplayMode style property.
        chromeButtonMode = function(mode) {
          if (mode === false) return 'off';

          var defaultMode = this.getStyle('buttonDisplayMode');

          // The default model is false, but it's overridden by +mode+ being true
          if (defaultMode === false) return 'on';

          // defaultMode is either true or auto.
          return defaultMode;
        },

        updateChromeForStyleChange = function(key, value/*, oldValue*/) {
          if (!_chrome) return;

          switch(key) {
            case 'nameDisplayMode':
              _chrome.name.setDisplayMode(value);
              break;

            case 'showArchiveStatus':
              _chrome.archive.setShowArchiveStatus(value);
              break;

            case 'buttonDisplayMode':
              // _chrome.muteButton.setDisplayMode(value);

            case 'bugDisplayMode':
              // _chrome.name.bugMode = value;
          }
        },

        _createChrome = function() {
          if(this.getStyle('bugDisplayMode') === 'off') {
            logAnalyticsEvent('bugDisplayMode', 'createChrome', 'mode', 'off');
          }
          _chrome = new OT.Chrome({
            parent: _container.domElement
          }).set({

            backingBar: new OT.Chrome.BackingBar({
              nameMode: this.getStyle('nameDisplayMode'),
              muteMode: chromeButtonMode.call(this, this.getStyle('showMuteButton'))
            }),

            name: new OT.Chrome.NamePanel({
              name: _properties.name,
              mode: this.getStyle('nameDisplayMode'),
              bugMode: this.getStyle('bugDisplayMode')
            }),

            muteButton: new OT.Chrome.MuteButton({
              muted: _properties.muted,
              mode: chromeButtonMode.call(this, this.getStyle('showMuteButton'))
            }),

            opentokButton: new OT.Chrome.OpenTokButton({
              mode: this.getStyle('bugDisplayMode')
            }),

            archive: new OT.Chrome.Archiving({
              show: this.getStyle('showArchiveStatus'),
              archiving: false
            })

          }).on({
            muted: function() {
              muteAudio.call(this, true);
            },

            unmuted: function() {
              muteAudio.call(this, false);
            }
          }, this);
        },

        _showError = function() {
          // Display the error message inside the container, assuming it's
          // been created by now.
          if (_container) {
            _container.addError(
              'The stream was unable to connect due to a network error.',
              'Make sure your connection isn\'t blocked by a firewall.'
            );
          }
        };


    this.recordQOS = function() {
      recordQOS();
    };


    this.subscribe = function(stream) {
      OT.debug('OT.Subscriber: subscribe to ' + stream.id);

      if (_state.subscribing) {
        // @todo error
        OT.error('OT.Subscriber.Subscribe: Cannot subscribe, already subscribing.');
        return false;
      }

      _state.set('Init');

      if (!stream) {
        // @todo error
        OT.error('OT.Subscriber: No stream parameter.');
        return false;
      }

      if (_stream) {
        // @todo error
        OT.error('OT.Subscriber: Already subscribed');
        return false;
      }

      _stream = stream;
      _stream.on({
        updated: streamUpdated,
        destroyed: streamDestroyed
      }, this);

      _fromConnectionId = stream.connection.id;
      _properties.name = _properties.name || _stream.name;
      _properties.classNames = 'OT_root OT_subscriber';

      if (_properties.style) {
        this.setStyle(_properties.style, null, true);
      }
      if (_properties.audioVolume) {
        this.setAudioVolume(_properties.audioVolume);
      }

      _properties.subscribeToAudio = OT.$.castToBoolean(_properties.subscribeToAudio, true);
      _properties.subscribeToVideo = OT.$.castToBoolean(_properties.subscribeToVideo, true);

      _container = new OT.WidgetView(targetElement, _properties);
      _domId = _container.domId;

      if(!_properties.subscribeToVideo && OT.$.browser() === 'Chrome') {
        _subscribeAudioFalseWorkaround = true;
        _properties.subscribeToVideo = true;
      }

      _startConnectingTime = OT.$.now();

      if (_stream.connection.id !== _session.connection.id) {
        logAnalyticsEvent('createPeerConnection', 'Attempt', '', '');

        _state.set('ConnectingToPeer');

        _peerConnection = new OT.SubscriberPeerConnection(_stream.connection, _session,
          _stream, this, _properties);

        _peerConnection.on({
          disconnected: onDisconnected,
          error: onPeerConnectionFailure,
          remoteStreamAdded: onRemoteStreamAdded,
          remoteStreamRemoved: onRemoteStreamRemoved
        }, this);

        // initialize the peer connection AFTER we've added the event listeners
        _peerConnection.init();
      } else {
        logAnalyticsEvent('createPeerConnection', 'Attempt', '', '');

        var publisher = _session.getPublisherForStream(_stream);
        if(!(publisher && publisher._.webRtcStream)) {
          this.trigger('subscribeComplete', new OT.Error(null, 'InvalidStreamID'));
          return this;
        }

        // Subscribe to yourself edge-case
        onRemoteStreamAdded.call(this, _session.getPublisherForStream(_stream)._.webRtcStream);
      }

      logAnalyticsEvent('subscribe', 'Attempt', 'streamId', _stream.id);

      return this;
    };

    this.destroy = function(reason, quiet) {

      if(reason === 'streamDestroyed') {
        if (_state.attemptingToSubscribe) {
          // We weren't subscribing yet so the stream was destroyed before we setup
          // the PeerConnection or receiving the initial stream.
          this.trigger('subscribeComplete', new OT.Error(null, 'InvalidStreamID'));
        }
      }

      clearInterval(_qosInterval);
      _qosInterval = null;

      this.disconnect();

      if (_chrome) {
        _chrome.destroy();
        _chrome = null;
      }

      if (_container) {
        _container.destroy();
        _container = null;
      }

      if (_stream && !_stream.destroyed) {
        logAnalyticsEvent('unsubscribe', null, 'streamId', _stream.id);
      }

      _domId = null;
      _stream = null;

      _session = null;
      _properties = null;

      if (quiet !== true) {
        this.dispatchEvent(
          new OT.DestroyedEvent(
            OT.Event.names.SUBSCRIBER_DESTROYED,
            this,
            reason
          ),
          this.off.bind(this)
        );
      }

      return this;
    };

    this.disconnect = function() {
      _state.set('NotSubscribing');

      if (_streamContainer) {
        _streamContainer.destroy();
        _streamContainer = null;
      }

      if (_peerConnection) {
        _peerConnection.destroy();
        _peerConnection = null;

        logAnalyticsEvent('disconnect', 'PeerConnection', 'streamId', _stream.id);
      }
    };

    this.processMessage = function(type, fromConnection, message) {
      OT.debug('OT.Subscriber.processMessage: Received ' + type + ' message from ' +
        fromConnection.id);
      OT.debug(message);

      if (_fromConnectionId !== fromConnection.id) {
        _fromConnectionId = fromConnection.id;
      }

      if (_peerConnection) {
        _peerConnection.processMessage(type, message);
      }
    };

    this.disableVideo = function(active) {
      if (!active) {
        OT.warn('Due to high packet loss and low bandwidth, video has been disabled');
      } else {
        if (_lastSubscribeToVideoReason === 'auto') {
          OT.info('Video has been re-enabled');
        } else {
          OT.info('Video was not re-enabled because it was manually disabled');
          return;
        }
      }
      this.subscribeToVideo(active, 'auto');
      this.dispatchEvent(new OT.Event(active ? 'videoEnabled' : 'videoDisabled'));
      logAnalyticsEvent('updateQuality', 'video', active ? 'videoEnabled' : 'videoDisabled', true);
    };

    /**
     * Return the base-64-encoded string of PNG data representing the Subscriber video.
     *
     *  <p>You can use the string as the value for a data URL scheme passed to the src parameter of
     *  an image file, as in the following:</p>
     *
     *  <pre>
     *  var imgData = subscriber.getImgData();
     *
     *  var img = document.createElement("img");
     *  img.setAttribute("src", "data:image/png;base64," + imgData);
     *  var imgWin = window.open("about:blank", "Screenshot");
     *  imgWin.document.write("&lt;body&gt;&lt;/body&gt;");
     *  imgWin.document.body.appendChild(img);
     *  </pre>
     * @method #getImgData
     * @memberOf Subscriber
     * @return {String} The base-64 encoded string. Returns an empty string if there is no video.
     */
    this.getImgData = function() {
      if (!this.subscribing) {
        OT.error('OT.Subscriber.getImgData: Cannot getImgData before the Subscriber ' +
          'is subscribing.');
        return null;
      }

      return _streamContainer.imgData;
    };

    /**
     * Sets the audio volume, between 0 and 100, of the Subscriber.
     *
     * <p>You can set the initial volume when you call the <code>Session.subscribe()</code>
     * method. Pass a <code>audioVolume</code> property of the <code>properties</code> parameter
     * of the method.</p>
     *
     * @param {Number} value The audio volume, between 0 and 100.
     *
     * @return {Subscriber} The Subscriber object. This lets you chain method calls, as in the 
     * following:
     *
     * <pre>mySubscriber.setAudioVolume(50).setStyle(newStyle);</pre>
     *
     * @see <a href="#getAudioVolume">getAudioVolume()</a>
     * @see <a href="Session.html#subscribe">Session.subscribe()</a>
     * @method #setAudioVolume
     * @memberOf Subscriber
     */
    this.setAudioVolume = function(value) {
      value = parseInt(value, 10);
      if (isNaN(value)) {
        OT.error('OT.Subscriber.setAudioVolume: value should be an integer between 0 and 100');
        return this;
      }
      _audioVolume = Math.max(0, Math.min(100, value));
      if (_audioVolume !== value) {
        OT.warn('OT.Subscriber.setAudioVolume: value should be an integer between 0 and 100');
      }
      if(_properties.muted && _audioVolume > 0) {
        _properties.premuteVolume = value;
        muteAudio.call(this, false);
      }
      if (_streamContainer) {
        _streamContainer.setAudioVolume(_audioVolume);
      }
      return this;
    };

    /**
    * Returns the audio volume, between 0 and 100, of the Subscriber.
    *
    * <p>Generally you use this method in conjunction with the <code>setAudioVolume()</code> 
    * method.</p>
    *
    * @return {Number} The audio volume, between 0 and 100, of the Subscriber.
    * @see <a href="#setAudioVolume">setAudioVolume()</a>
    * @method #getAudioVolume
    * @memberOf Subscriber
    */
    this.getAudioVolume = function() {
      if(_properties.muted) {
        return 0;
      }
      if (_streamContainer) return _streamContainer.getAudioVolume();
      else return _audioVolume;
    };

    /**
    * Toggles audio on and off. Starts subscribing to audio (if it is available and currently 
    * not being subscribed to) when the <code>value</code> is <code>true</code>; stops 
    * subscribing to audio (if it is currently being subscribed to) when the <code>value</code> 
    * is <code>false</code>.
    * <p>
    * <i>Note:</i> This method only affects the local playback of audio. It has no impact on the 
    * audio for other connections subscribing to the same stream. If the Publsher is not 
    * publishing audio, enabling the Subscriber audio will have no practical effect.
    * </p>
    *
    * @param {Boolean} value Whether to start subscribing to audio (<code>true</code>) or not 
    * (<code>false</code>).
    *
    * @return {Subscriber} The Subscriber object. This lets you chain method calls, as in the 
    * following:
    *
    * <pre>mySubscriber.subscribeToAudio(true).subscribeToVideo(false);</pre>
    *
    * @see <a href="#subscribeToVideo">subscribeToVideo()</a>
    * @see <a href="Session.html#subscribe">Session.subscribe()</a>
    * @see <a href="StreamPropertyChangedEvent.html">StreamPropertyChangedEvent</a>
    *
    * @method #subscribeToAudio
    * @memberOf Subscriber
    */
    this.subscribeToAudio = function(pValue) {
      var value = OT.$.castToBoolean(pValue, true);

      if (_peerConnection) {
        _peerConnection.subscribeToAudio(value && !_properties.subscribeMute);

        if (_session && _stream && value !== _properties.subscribeToAudio) {
          _stream.setChannelActiveState('audio', value && !_properties.subscribeMute);
        }
      }

      _properties.subscribeToAudio = value;

      return this;
    };

    var muteAudio = function(_mute) {
      _chrome.muteButton.muted = _mute;

      if(_mute === _properties.mute) {
        return;
      }
      if (window.navigator.userAgent.toLowerCase().indexOf('chrome') >= 0) {
        _properties.subscribeMute = _properties.muted = _mute;
        this.subscribeToAudio(_properties.subscribeToAudio);
      } else {
        if(_mute) {
          _properties.premuteVolume = this.getAudioVolume();
          _properties.muted = true;
          this.setAudioVolume(0);
        } else if(_properties.premuteVolume || _properties.audioVolume) {
          _properties.muted = false;
          this.setAudioVolume(_properties.premuteVolume || _properties.audioVolume);
        }
      }
      _properties.mute = _properties.mute;
    };


    /**
    * Toggles video on and off. Starts subscribing to video (if it is available and 
    * currently not being subscribed to) when the <code>value</code> is <code>true</code>; 
    * stops subscribing to video (if it is currently being subscribed to) when the 
    * <code>value</code> is <code>false</code>.
    * <p>
    * <i>Note:</i> This method only affects the local playback of video. It has no impact on 
    * the video for other connections subscribing to the same stream. If the Publsher is not 
    * publishing video, enabling the Subscriber video will have no practical video.
    * </p>
    *
    * @param {Boolean} value Whether to start subscribing to video (<code>true</code>) or not 
    * (<code>false</code>).
    *
    * @return {Subscriber} The Subscriber object. This lets you chain method calls, as in the 
    * following:
    *
    * <pre>mySubscriber.subscribeToVideo(true).subscribeToAudio(false);</pre>
    *
    * @see <a href="#subscribeToAudio">subscribeToAudio()</a>
    * @see <a href="Session.html#subscribe">Session.subscribe()</a>
    * @see <a href="StreamPropertyChangedEvent.html">StreamPropertyChangedEvent</a>
    *
    * @method #subscribeToVideo
    * @memberOf Subscriber
    */
    this.subscribeToVideo = function(pValue, reason) {
      if(_subscribeAudioFalseWorkaround && pValue === true) {
        // Turn off the workaround if they enable the video
        _subscribeAudioFalseWorkaround = false;
        return;
      }

      var value = OT.$.castToBoolean(pValue, true);

      if(_container) {
        _container.showPoster = !(value && _stream.hasVideo);
        if(value && _container.video) {
          _container.loading = value;
          _container.video.whenTimeIncrements(function(){
            _container.loading = false;
          }, this);
        }
      }

      if (_peerConnection) {
        _peerConnection.subscribeToVideo(value);

        if (_session && _stream && (value !== _properties.subscribeToVideo ||
            reason !== _lastSubscribeToVideoReason)) {
          _stream.setChannelActiveState('video', value, reason);
        }
      }

      _properties.subscribeToVideo = value;
      _lastSubscribeToVideoReason = reason;

      return this;
    };

    Object.defineProperties(this, {
      id: {
        get: function() { return _domId; },
        enumerable: true
      },
        
      element: {
        get: function() { return _container.domElement; },
        enumerable: true
      },

      widgetId: {
        get: function() { return _widgetId; }
      },

      stream: {
        get: function() { return _stream; },
        enumerable: true
      },

      streamId: {
        get: function() {
          if (!_stream) return null;
          return _stream.id;
        },
        enumerable: true
      },

      targetElement: {
        get: function() { return _streamContainer ? _streamContainer.domElement : null; }
      },

      subscribing: {
        get: function() { return _state.subscribing; },
        enumerable: true
      },

      isWebRTC: {
        get: function() { return true; }
      },

      loading: {
        get: function(){ return _container && _container.loading; }
      },

      session: {
        get: function() { return _session; }
      },

      videoWidth: {
        get: function() { return _streamContainer.videoWidth; },
        enumerable: true
      },

      videoHeight: {
        get: function() { return _streamContainer.videoHeight; },
        enumerable: true
      }
    });

    /**
    * Restricts the frame rate of the Subscriber's video stream, when you pass in 
    * <code>true</code>. When you pass in <code>false</code>, the frame rate of the video stream 
    * is not restricted. 
    * <p>
    * When the frame rate is restricted, the Subscriber video frame will update once or less per 
    * second.
    * <p>
    * This feature is only available in sessions that use the OpenTok media server, not in 
    * peer-to-peer sessions. In peer-to-peer sessions, calling this method has no effect.
    * <p>
    * Restricting the subscriber frame rate has the following benefits:
    * <ul>
    *    <li>It reduces CPU usage.</li>
    *    <li>It reduces the network bandwidth consumed.</li>
    *    <li>It lets you subscribe to more streams simultaneously.</li>
    * </ul>
    * <p>
    * Reducing a subscriber's frame rate has no effect on the frame rate of the video in
    * other clients.
    *
    * @param {Boolean} value Whether to restrict the Subscriber's video frame rate 
    * (<code>true</code>) or not (<code>false</code>).
    *
    * @return {Subscriber} The Subscriber object. This lets you chain method calls, as in the 
    * following:
    *
    * <pre>mySubscriber.restrictFrameRate(false).subscribeToAudio(true);</pre>
    *
    * @method #restrictFrameRate
    * @memberOf Subscriber
    */
    this.restrictFrameRate = function(val) {
      OT.debug('OT.Subscriber.restrictFrameRate(' + val + ')');

      logAnalyticsEvent('restrictFrameRate', val.toString(), 'streamId', _stream.id);

      if (_session.sessionInfo.p2pEnabled) {
        OT.warn('OT.Subscriber.restrictFrameRate: Cannot restrictFrameRate on a P2P session');
      }

      if (typeof val !== 'boolean') {
        OT.error('OT.Subscriber.restrictFrameRate: expected a boolean value got a ' + typeof val);
      } else {
        _stream.setRestrictFrameRate(val);
      }
      return this;
    };

    this.on('styleValueChanged', updateChromeForStyleChange, this);

    this._ = {
      archivingStatus: function(status) {
        if(_chrome) {
          _chrome.archive.setArchiving(status);
        }
      }
    };

    _state = new OT.SubscribingState(stateChangeFailed);

	/**
	* Dispatched when the OpenTok media server stops sending video to the subscriber.
	* This feature of the OpenTok media server has a subscriber drop the video stream
	* when connectivity degrades. The subscriber continues to receive the audio stream,
	* if there is one.
	* <p>
	* If connectivity improves to support video again, the Subscriber object dispatches
	* a videoEnabled event, and the Subscriber resumes receiving video.
	* <p>
	* This feature is only available in sessions that use the OpenTok media server,
	* not in peer-to-peer sessions.
	*
	* @see Event
	* @see event:videoEnabled
	* @name videoDisabled
	* @event
	* @memberof Subscriber
	*/

	/**
	* Dispatched when the OpenTok media server resumes sending video to the subscriber
	* after video was previously disabled.
	* <p>
	* The OpenTok media server has a subscriber drop the video stream when connectivity
	* degrades (and the Subscriber dispatches a videoDisabled event). When the connectivity
	* improves to support video the Subscriber dispatches the videoEnabled event and
	* video resumes.
	* <p>
	* To prevent video from resuming, in the <code>videoEnabled</code> event listener,
	* call <code>subscribeToVideo(false)</code> on the Subscriber object.
	* <p>
	* This feature is only available in sessions that use the OpenTok media server,
	* not in peer-to-peer sessions.
	*
	* @see Event
	* @see event:videoDisabled
	* @name videoEnabled
	* @event
	* @memberof Subscriber
	*/

	/**
	* Dispatched when the Subscriber element is removed from the HTML DOM. When this event is
	* dispatched, you may choose to adjust or remove HTML DOM elements related to the subscriber.
	* @see Event
	* @name destroyed
	* @event
	* @memberof Subscriber
	*/
  };

})(window);
!(function() {

  var parseErrorFromJSONDocument,
      onGetResponseCallback,
      onGetErrorCallback;

  OT.SessionInfo = function(jsonDocument) {
    var sessionJSON = jsonDocument[0];

    OT.log('SessionInfo Response:');
    OT.log(jsonDocument);

    /*jshint camelcase:false*/

    this.sessionId = sessionJSON.session_id;
    this.partnerId = sessionJSON.partner_id;
    this.sessionStatus = sessionJSON.session_status;

    this.messagingServer = sessionJSON.messaging_server_url;

    this.messagingURL = sessionJSON.messaging_url;
    this.symphonyAddress = sessionJSON.symphony_address;

    this.p2pEnabled = sessionJSON.properties &&
      sessionJSON.properties.p2p &&
      sessionJSON.properties.p2p.preference &&
      sessionJSON.properties.p2p.preference.value === 'enabled';
  };

  // Retrieves Session Info for +session+. The SessionInfo object will be passed
  // to the +onSuccess+ callback. The +onFailure+ callback will be passed an error
  // object and the DOMEvent that relates to the error.
  OT.SessionInfo.get = function(session, onSuccess, onFailure) {
    var sessionInfoURL = OT.properties.apiURLSSL + '/session/' + session.id + '?extended=true',

        startTime = OT.$.now(),

        validateRawSessionInfo = function(sessionInfo) {
          session.logEvent('Instrumentation', null, 'gsi', OT.$.now() - startTime);
          var error = parseErrorFromJSONDocument(sessionInfo);
          if (error === false) {
            onGetResponseCallback(session, onSuccess, sessionInfo);
          } else {
            onGetErrorCallback(session, onFailure, error, JSON.stringify(sessionInfo));
          }
        };

    session.logEvent('getSessionInfo', 'Attempt', 'api_url', OT.properties.apiURLSSL);

    OT.$.getJSON(sessionInfoURL, {
      headers: {
        'X-TB-TOKEN-AUTH': session.token,
        'X-TB-VERSION': 1
      }
    }, function(error, sessionInfo) {
      if(error) {
        var responseText = sessionInfo;
        console.log('getJSON said error:', error);
        onGetErrorCallback(session, onFailure,
          new OT.Error(error.target && error.target.status || error.code, error.message ||
            'Could not connect to the OpenTok API Server.'), responseText);
      } else {
        validateRawSessionInfo(sessionInfo);
      }
    });
  };

  var messageServerToClientErrorCodes = {};
  messageServerToClientErrorCodes['404'] = OT.ExceptionCodes.INVALID_SESSION_ID;
  messageServerToClientErrorCodes['409'] = OT.ExceptionCodes.INVALID_SESSION_ID;
  messageServerToClientErrorCodes['400'] = OT.ExceptionCodes.INVALID_SESSION_ID;
  messageServerToClientErrorCodes['403'] = OT.ExceptionCodes.AUTHENTICATION_ERROR;

  // Return the error in +jsonDocument+, if there is one. Otherwise it will return
  // false.
  parseErrorFromJSONDocument = function(jsonDocument) {
    if(Array.isArray(jsonDocument)) {
      var errors = jsonDocument.filter(function(node) {
        return node.error != null;
      });

      var numErrorNodes = errors.length;
      if(numErrorNodes === 0) {
        return false;
      }

      var errorCode = errors[0].error.code;
      if (messageServerToClientErrorCodes[errorCode.toString()]) {
        errorCode = messageServerToClientErrorCodes[errorCode];
      }

      return {
        code: errorCode,
        message: errors[0].error.errorMessage && errors[0].error.errorMessage.message
      };
    } else {
      return {
        code: null,
        message: 'Unknown error: getSessionInfo JSON response was badly formed'
      };
    }
  };

  onGetResponseCallback = function(session, onSuccess, rawSessionInfo) {
    session.logEvent('getSessionInfo', 'Success', 'api_url', OT.properties.apiURLSSL);

    onSuccess( new OT.SessionInfo(rawSessionInfo) );
  };

  onGetErrorCallback = function(session, onFailure, error, responseText) {
    session.logEvent('Connect', 'Failure', 'errorMessage',
      'GetSessionInfo:' + error.code + ':' + error.message + ':' + responseText);

    onFailure(error, session);
  };

})(window);
!(function() {
	/**
	 * A class defining properties of the <code>capabilities</code> property of a
   * Session object. See <a href="Session.html#properties">Session.capabilities</a>.
	 * <p>
	 * All Capabilities properties are undefined until you have connected to a session
	 * and the Session object has dispatched the <code>sessionConnected</code> event.
	 * <p>
	 * For more information on token roles, see the
   * <a href="server_side_libraries.html#generate_token">generate_token()</a>
   * method of the OpenTok server-side libraries.
	 *
	 * @class Capabilities
	 *
	 * @property {Number} forceDisconnect Specifies whether you can call
   * the <code>Session.forceDisconnect()</code> method (1) or not (0). To call the
   * <code>Session.forceDisconnect()</code> method,
   * the user must have a token that is assigned the role of moderator.
	 * @property {Number} forceUnpublish Specifies whether you can call
   * the <code>Session.forceUnpublish()</code> method (1) or not (0). To call the
   * <code>Session.forceUnpublish()</code> method, the user must have a token that
   * is assigned the role of moderator.
	 * @property {Number} publish Specifies whether you can publish to the session (1) or not (0).
   * The ability to publish is based on a few factors. To publish, the user must have a token that
   * is assigned a role that supports publishing. There must be a connected camera and microphone.
	 * @property {Number} subscribe Specifies whether you can subscribe to streams
   * in the session (1) or not (0). Currently, this capability is available for all users on all
   * platforms.
   * @property {Number} supportsWebRTC Whether the client supports WebRTC (1) or not (0).
	 */
	OT.Capabilities = function(permissions) {
	    this.publish = permissions.indexOf('publish') !== -1 ? 1 : 0;
	    this.subscribe = permissions.indexOf('subscribe') !== -1 ? 1 : 0;
	    this.forceUnpublish = permissions.indexOf('forceunpublish') !== -1 ? 1 : 0;
	    this.forceDisconnect = permissions.indexOf('forcedisconnect') !== -1 ? 1 : 0;
	    this.supportsWebRTC = OT.$.supportsWebRTC() ? 1 : 0;

      this.permittedTo = function(action) {
        return this.hasOwnProperty(action) && this[action] === 1;
      };
    };

})(window);
!(function(window) {


/**
 * The Session object returned by the <code>OT.initSession()</code> method provides access to
 * much of the OpenTok functionality.
 *
 * @class Session
 * @augments EventDispatcher
 *
 * @property {Capabilities} capabilities A {@link Capabilities} object that includes information
 * about the capabilities of the client. All properties of the <code>capabilities</code> object
 * are undefined until you have connected to a session and the Session object has dispatched the
 * <code>sessionConnected</code> event.
 * @property {Connection} connection The {@link Connection} object for this session. The
 * connection property is only available once the Session object dispatches the sessionConnected
 * event. The Session object asynchronously dispatches a sessionConnected event in response
 * to a successful call to the connect() method. See: <a href="Session#connect">connect</a> and
 * {@link Connection}.
 * @property {String} sessionId The session ID for this session. You pass this value into the
 * <code>OT.initSession()</code> method when you create the Session object. (Note: a Session
 * object is not connected to the OpenTok server until you call the connect() method of the
 * object and the object dispatches a connected event. See {@link OT.initSession} and
 * {@link connect}).
 *  For more information on sessions and session IDs, see
 * <a href="/opentok/tutorials/create-session/">Session creation</a>.
 */
  OT.Session = function(apiKey, sessionId) {
    // Check that the client meets the minimum requirements, if they don't the upgrade
    // flow will be triggered.
    if (!OT.checkSystemRequirements()) {
      OT.upgradeSystemRequirements();
      return;
    }

    if(sessionId == null) {
      sessionId = apiKey;
      apiKey = null;
    }

    var _initialConnection = true,
        _apiKey = apiKey,
        _token,
        _sessionId = sessionId,
        _socket,
        _widgetId = OT.$.uuid(),
        _connectionId,
        _analytics = new OT.Analytics(),
        sessionConnectFailed,
        sessionDisconnectedHandler,
        connectionCreatedHandler,
        connectionDestroyedHandler,
        streamCreatedHandler,
        streamPropertyModifiedHandler,
        streamDestroyedHandler,
        archiveCreatedHandler,
        archiveDestroyedHandler,
        archiveUpdatedHandler,
        reset,
        disconnectComponents,
        destroyPublishers,
        destroySubscribers,
        connectMessenger,
        getSessionInfo,
        onSessionInfoResponse,
        permittedTo,
        dispatchError;


    OT.$.eventing(this);
    var setState = OT.$.statable(this, [
      'disconnected', 'connecting', 'connected', 'disconnecting'
    ], 'disconnected');

    this.connections = new OT.Collection();
    this.streams = new OT.Collection();
    this.archives = new OT.Collection();


  //--------------------------------------
  //  MESSAGE HANDLERS
  //--------------------------------------

  // The duplication of this and sessionConnectionFailed will go away when
  // session and messenger are refactored
    sessionConnectFailed = function(reason, code) {
      setState('disconnected');

      OT.error(reason);

      this.trigger('sessionConnectFailed',
        new OT.Error(code || OT.ExceptionCodes.CONNECT_FAILED, reason));

      OT.handleJsException(reason, code || OT.ExceptionCodes.CONNECT_FAILED, {
        session: this
      });
    };

    sessionDisconnectedHandler = function(event) {
      var reason = event.reason;
      if(reason === 'networkTimedout') {
        reason = 'networkDisconnected';
        this.logEvent('Connect', 'TimeOutDisconnect', 'reason', event.reason);
      } else {
        this.logEvent('Connect', 'Disconnected', 'reason', event.reason);
      }

      var publicEvent = new OT.SessionDisconnectEvent('sessionDisconnected', reason);

      reset.call(this);
      disconnectComponents.call(this, reason);

      var defaultAction = function() {
        // Publishers handle preventDefault'ing themselves
        destroyPublishers.call(this, publicEvent.reason);
        // Subscriers don't, destroy 'em if needed
        if (!publicEvent.isDefaultPrevented()) destroySubscribers.call(this, publicEvent.reason);
      }.bind(this);

      this.dispatchEvent(publicEvent, defaultAction);
    };

    connectionCreatedHandler = function(connection) {
      // We don't broadcast events for the symphony connection
      if (connection.id.match(/^symphony\./)) return;

      this.dispatchEvent(new OT.ConnectionEvent(
        OT.Event.names.CONNECTION_CREATED,
        connection
      ));
    };

    connectionDestroyedHandler = function(connection, reason) {
      // We don't broadcast events for the symphony connection
      if (connection.id.match(/^symphony\./)) return;

      // Don't delete the connection if it's ours. This only happens when
      // we're about to receive a session disconnected and session disconnected
      // will also clean up our connection.
      if (connection.id === _socket.id) return;

      this.dispatchEvent(
        new OT.ConnectionEvent(
          OT.Event.names.CONNECTION_DESTROYED,
          connection,
          reason
        )
      );
    };

    streamCreatedHandler = function(stream) {
      if(stream.connection.id !== this.connection.id) {
        this.dispatchEvent(new OT.StreamEvent(
          OT.Event.names.STREAM_CREATED,
          stream,
          null,
          false
        ));
      }
    };

    streamPropertyModifiedHandler = function(event) {
      var stream = event.target,
          propertyName = event.changedProperty,
          newValue = event.newValue;

      if (propertyName === 'orientation') {
        propertyName = 'videoDimensions';
        newValue = {width: newValue.width, height: newValue.height};
      }

      this.dispatchEvent(new OT.StreamPropertyChangedEvent(
        OT.Event.names.STREAM_PROPERTY_CHANGED,
        stream,
        propertyName,
        event.oldValue,
        newValue
      ));
    };

    streamDestroyedHandler = function(stream, reason) {

      // if the stream is one of ours we delegate handling
      // to the publisher itself.
      if(stream.connection.id === this.connection.id) {
        OT.publishers.where({ streamId: stream.id }).forEach(function(publisher) {
          publisher._.unpublishFromSession(this, reason);
        }.bind(this));
        return;
      }

      var event = new OT.StreamEvent('streamDestroyed', stream, reason, true);

      var defaultAction = function() {
        if (!event.isDefaultPrevented()) {
          // If we are subscribed to any of the streams we should unsubscribe
          OT.subscribers.where({streamId: stream.id}).forEach(function(subscriber) {
            if (subscriber.session.id === this.id) {
              if(subscriber.stream) {
                subscriber.destroy('streamDestroyed');
              }
            }
          }, this);
        } else {
          // @TODO Add a one time warning that this no longer cleans up the publisher
        }
      }.bind(this);

      this.dispatchEvent(event, defaultAction);
    };

    archiveCreatedHandler = function(archive) {
      this.dispatchEvent(new OT.ArchiveEvent('archiveStarted', archive));
    };

    archiveDestroyedHandler = function(archive) {
      this.dispatchEvent(new OT.ArchiveEvent('archiveDestroyed', archive));
    };

    archiveUpdatedHandler = function(event) {
      var archive = event.target,
        propertyName = event.changedProperty,
        newValue = event.newValue;

      if(propertyName === 'status' && newValue === 'stopped') {
        this.dispatchEvent(new OT.ArchiveEvent('archiveStopped', archive));
      } else {
        this.dispatchEvent(new OT.ArchiveEvent('archiveUpdated', archive));
      }
    };

    // Put ourselves into a pristine state
    reset = function() {
      _token = null;
      setState('disconnected');

      this.connections.destroy();
      this.streams.destroy();
      this.archives.destroy();
    };

    disconnectComponents = function(reason) {
      OT.publishers.where({session: this}).forEach(function(publisher) {
        publisher.disconnect(reason);
      });

      OT.subscribers.where({session: this}).forEach(function(subscriber) {
        subscriber.disconnect();
      });
    };

    destroyPublishers = function(reason) {
      OT.publishers.where({session: this}).forEach(function(publisher) {
        publisher._.streamDestroyed(reason);
      });
    };

    destroySubscribers = function(reason) {
      OT.subscribers.where({session: this}).forEach(function(subscriber) {
        subscriber.destroy(reason);
      });
    };

    connectMessenger = function() {
      OT.debug('OT.Session: connecting to Raptor');

      var socketUrl = OT.properties.messagingProtocol + '://' + this.sessionInfo.messagingServer +
            ':' + OT.properties.messagingPort + '/rumorwebsocketsv2',
          symphonyUrl = OT.properties.symphonyAddresss || 'symphony.' +
            this.sessionInfo.messagingServer;

      _socket = new OT.Raptor.Socket(_widgetId, socketUrl, symphonyUrl,
        OT.SessionDispatcher(this));

      var analyticsPayload = [
        socketUrl, navigator.userAgent, OT.properties.version,
        window.externalHost ? 'yes' : 'no'
      ];

      _socket.connect(_token, this.sessionInfo, function(error, sessionState) {
        if (error) {
          analyticsPayload.splice(0,0,error.message);
          this.logEvent('Connect', 'Failure',
            'reason|webSocketServerUrl|userAgent|sdkVersion|chromeFrame',
            analyticsPayload.map(function(e) { return e.replace('|', '\\|'); }).join('|'));

          sessionConnectFailed.call(this, error.message, error.code);
          return;
        }

        OT.debug('OT.Session: Received session state from Raptor', sessionState);

        setState('connected');

        this.logEvent('Connect', 'Success',
          'webSocketServerUrl|userAgent|sdkVersion|chromeFrame',
          analyticsPayload.map(function(e) {
            return e.replace('|', '\\|');
          }).join('|'), {connectionId: this.connection.id});

        // Listen for our own connection's destroyed event so we know when we've been disconnected.
        this.connection.on('destroyed', sessionDisconnectedHandler, this);

        // Listen for connection updates
        this.connections.on({
          add: connectionCreatedHandler,
          remove: connectionDestroyedHandler
        }, this);

        // Listen for stream updates
        this.streams.on({
          add: streamCreatedHandler,
          remove: streamDestroyedHandler,
          update: streamPropertyModifiedHandler
        }, this);

        this.archives.on({
          add: archiveCreatedHandler,
          remove: archiveDestroyedHandler,
          update: archiveUpdatedHandler
        }, this);

        this.dispatchEvent(
          new OT.SessionConnectEvent(OT.Event.names.SESSION_CONNECTED), function() {
            this.connections._triggerAddEvents(); // { id: this.connection.id }
            this.streams._triggerAddEvents(); // { id: this.stream.id }
            this.archives._triggerAddEvents();
          }.bind(this)
        );

      }.bind(this));
    };

    getSessionInfo = function() {
      if (this.is('connecting')) {
        OT.SessionInfo.get(
          this,
          onSessionInfoResponse.bind(this),
          function(error) {
            sessionConnectFailed.call(this, error.message +
              (error.code ? ' (' + error.code + ')' : ''), error.code);
          }.bind(this)
        );
      }
    };

    onSessionInfoResponse = function(sessionInfo) {
      if (this.is('connecting')) {
        this.sessionInfo = sessionInfo;
        if (this.sessionInfo.partnerId && this.sessionInfo.partnerId !== _apiKey) {
          _apiKey = this.sessionInfo.partnerId;

          var reason = 'Authentication Error: The API key does not match the token or session.';

          this.logEvent('Connect', 'Failure', 'reason', 'GetSessionInfo:' +
            OT.ExceptionCodes.AUTHENTICATION_ERROR + ':' + reason);

          sessionConnectFailed.call(this, reason, OT.ExceptionCodes.AUTHENTICATION_ERROR);
        } else {
          connectMessenger.call(this);
        }
      }
    };

    // Check whether we have permissions to perform the action.
    permittedTo = function(action) {
      return this.capabilities.permittedTo(action);
    }.bind(this);

    dispatchError = function(code, message, completionHandler) {
      OT.dispatchError(code, message, completionHandler, this);
    }.bind(this);

    this.logEvent = function(action, variation, payloadType, payload, options) {
      /* jshint camelcase:false */
      var event = {
        action: action,
        variation: variation,
        payload_type: payloadType,
        payload: payload,
        session_id: _sessionId,
        partner_id: _apiKey,
        widget_id: _widgetId,
        widget_type: 'Controller'
      };
      if (this.connection && this.connection.id) _connectionId = event.connection_id =
        this.connection.id;
      else if (_connectionId) event.connection_id = _connectionId;

      if (options) event = OT.$.extend(options, event);
      _analytics.logEvent(event);
    };

 /**
 * Connects to an OpenTok session. Pass your API key as the <code>apiKey</code> parameter.
 * You get an API key when you <a href="https://dashboard.tokbox.com/users/sign_in">sign up</a>
 * for an OpenTok account. Pass a token string as the <code>token</code> parameter. You generate
 * a token using the
 * <a href="/opentok/api/tools/documentation/api/server_side_libraries.html">OpenTok server-side
 * libraries</a> or the <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page. For
 * more information, see <a href="/opentok/tutorials/create-token/">Connection token creation</a>.
 * <p>
 * Upon a successful connection, the Session object dispatches a <code>sessionConnected</code>
 * event. Call the <code>on()</code> method to set up an event handler to process this event before
 * calling other methods of the Session object.
 *  </p>
 *  <p>
 *    The Session object dispatches a <code>connectionCreated</code> event when other clients
 *    create connections to the session.
 *  </p>
 *  <p>
 *    The OT object dispatches an <code>exception</code> event if the session ID,
 *    API key, or token string are invalid. See <a href="ExceptionEvent.html">ExceptionEvent</a>
 *    and <a href="OT.html#on">OT.on()</a>.
 *  </p>
 *  <p>
 *    The application throws an error if the system requirements are not met
 *    (see <a href="OT.html#checkSystemRequirements">OT.checkSystemRequirements()</a>).
 *    The application also throws an error if the session has peer-to-peer streaming enabled
 *    and two clients are already connected to the session (see our
 *    <a href="/opentok/libraries/server/">server-side libraries</a>).
 *  </p>
 *
 * <p>
 *  With the peer-to-peer option enabled for a session, the session supports two connections.
 *  Without the peer-to-peer option enabled, the session supports up to four connections. On
 *  clients that attempt to a session that already has the maximum number of connections, the
 *  OT object dispatches an `exception` event (with the `code` property set to 1007). See
 *  <a href="/opentok/tutorials/create-session/">Session Creation documentation</a>.
 * </p>
 *
 *
 *  <h5>
 *  Example
 *  </h5>
 *  <p>
 *  The following code initializes a session and sets up an event listener for when the session
 *  connects:
 *  </p>
 *  <pre>
 *  var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
 *  var sessionID = ""; // Replace with your own session ID.
 *                      // See https://dashboard.tokbox.com/projects
 *  var token = ""; // Replace with a generated token that has been assigned the moderator role.
 *                  // See https://dashboard.tokbox.com/projects
 *
 *  var session = OT.initSession(apiKey, sessionID);
 *  session.on("sessionConnected", function(sessionConnectEvent) {
 *      //
 *  });
 *  session.connect(token);
 *  </pre>
 *  <p>
 *  <p>
 *    In this example, the <code>sessionConnectHandler()</code> function is passed an event
 *    object of type {@link SessionConnectEvent}.
 *  </p>
 *
 *  <h5>
 *  Events dispatched:
 *  </h5>
 *
 *  <p>
 *    <code>exception</code> (<a href="ExceptionEvent.html">ExceptionEvent</a>) &#151; Dispatched
 *    by the OT class locally in the event of an error.
 *  </p>
 *  <p>
 *    <code>connectionCreated</code> (<a href="ConnectionEvent.html">ConnectionEvent</a>) &#151;
 *      Dispatched by the Session object on all clients connected to the session.
 *  </p>
 *  <p>
 *    <code>sessionConnected</code> (<a href="SessionConnectEvent.html">SessionConnectEvent</a>)
 *      &#151; Dispatched locally by the Session object when the connection is established.
 *  </p>
 *
  * @param {String} token The session token. You generate a session token using our
  * <a href="/opentok/libraries/server/">server-side libraries</a> or the
  * <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page. For more information, see
  * <a href="/opentok/tutorials/create-token/">Connection token creation</a>.
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>connect()</code> method succeeds or fails. This function takes one parameter &mdash;
  * <code>error</code>. On success, the <code>completionHandler</code> function is not passed any
  * arguments. On error, the function is passed an <code>error</code> object parameter. The
  * <code>error</code> object has two properties: <code>code</code> (an integer) and
  * <code>message</code> (a string), which identify the cause of the failure. The following
  * code adds a <code>completionHandler</code> when calling the <code>connect()</code> method:
  * <pre>
  * session.connect(token, function (error) {
  *   if (error) {
  *       console.log(error.message);
  *   } else {
  *     console.log("Connected to session.");
  *   }
  * });
  * </pre>
  * <p>
  * Note that upon connecting to the session, the Session object dispatches a
  * <code>sessionConnected</code> event in addition to calling the <code>completionHandler</code>.
  * The SessionConnectEvent object, which defines the <code>sessionConnected</code> event,
  * includes <code>connections</code> and <code>streams</code> properties, which
  * list the connections and streams in the session when you connect.
  * </p>
  *
  * @see SessionConnectEvent
  * @method #connect
  * @memberOf Session
  */
    this.connect = function(token) {

      if(apiKey == null && arguments.length > 1 &&
        (typeof arguments[0] === 'string' || typeof arguments[0] === 'number') &&
        typeof arguments[1] === 'string') {
        _apiKey = token.toString();
        token = arguments[1];
      }

      // The completion handler is always the last argument.
      var completionHandler = arguments[arguments.length - 1];

      if (this.is('connecting', 'connected')) {
        OT.warn('OT.Session: Cannot connect, the session is already ' + this.state);
        return this;
      }

      reset.call(this);
      setState('connecting');
      _token = !OT.$.isFunction(token) && token;

      // Get a new widget ID when reconnecting.
      if (_initialConnection) {
        _initialConnection = false;
      } else {
        _widgetId = OT.$.uuid();
      }

      if (completionHandler && OT.$.isFunction(completionHandler)) {
        this.once('sessionConnected', completionHandler.bind(null, null));
        this.once('sessionConnectFailed', completionHandler);
      }

      if(_apiKey == null || OT.$.isFunction(_apiKey)) {
        setTimeout(sessionConnectFailed.bind(this,
          'API Key is undefined. You must pass an API Key to initSession.',
          OT.ExceptionCodes.AUTHENTICATION_ERROR));
        return this;
      }
      
      if (!_sessionId) {
        setTimeout(sessionConnectFailed.bind(this,
            'SessionID is undefined. You must pass a sessionID to initSession.',
          OT.ExceptionCodes.INVALID_SESSION_ID));
        return this;
      }

      _apiKey = _apiKey.toString();

      // Ugly hack, make sure OT.APIKEY is set
      if (OT.APIKEY.length === 0) {
        OT.APIKEY = _apiKey;
      }

      var analyticsPayload = [
        navigator.userAgent, OT.properties.version,
        window.externalHost ? 'yes' : 'no'
      ];
      this.logEvent( 'Connect', 'Attempt',
        'userAgent|sdkVersion|chromeFrame',
        analyticsPayload.map(function(e) { return e.replace('|', '\\|'); }).join('|')
      );

      getSessionInfo.call(this);
      return this;
    };

 /**
  * Disconnects from the OpenTok session.
  *
  * <p>
  * Calling the <code>disconnect()</code> method ends your connection with the session. In the
  * course of terminating your connection, it also ceases publishing any stream(s) you were
  * publishing.
  * </p>
  * <p>
  * Session objects on remote clients dispatch <code>streamDestroyed</code> events for any
  * stream you were publishing. The Session object dispatches a <code>sessionDisconnected</code>
  * event locally. The Session objects on remote clients dispatch <code>connectionDestroyed</code>
  * events, letting other connections know you have left the session. The
  * {@link SessionDisconnectEvent} and {@link StreamEvent} objects that define the
  * <code>sessionDisconnect</code> and <code>connectionDestroyed</code> events each have a
  * <code>reason</code> property. The <code>reason</code> property lets the developer determine
  * whether the connection is being terminated voluntarily and whether any streams are being
  * destroyed as a byproduct of the underlying connection's voluntary destruction.
  * </p>
  * <p>
  * If the session is not currently connected, calling this method causes a warning to be logged.
  * See <a "href=OT.html#setLogLevel">OT.setLogLevel()</a>.
  * </p>
  *
  * <p>
  * <i>Note:</i> If you intend to reuse a Publisher object created using
  * <code>OT.initPublisher()</code> to publish to different sessions sequentially, call either
  * <code>Session.disconnect()</code> or <code>Session.unpublish()</code>. Do not call both.
  * Then call the <code>preventDefault()</code> method of the <code>streamDestroyed</code> or
  * <code>sessionDisconnected</code> event object to prevent the Publisher object from being
  * removed from the page. Be sure to call <code>preventDefault()</code> only if the
  * <code>connection.connectionId</code> property of the Stream object in the event matches the
  * <code>connection.connectionId</code> property of your Session object (to ensure that you are
  * preventing the default behavior for your published streams, not for other streams that you
  * subscribe to).
  * </p>
  *
  * <h5>
  * Events dispatched:
  * </h5>
  * <p>
  * <code>sessionDisconnected</code>
  * (<a href="SessionDisconnectEvent.html">SessionDisconnectEvent</a>)
  * &#151; Dispatched locally when the connection is disconnected.
  * </p>
  * <p>
  * <code>connectionDestroyed</code> (<a href="ConnectionEvent.html">ConnectionEvent</a>) &#151;
  * Dispatched on other clients, along with the <code>streamDestroyed</code> event (as warranted).
  * </p>
  *
  * <p>
  * <code>streamDestroyed</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  * Dispatched on other clients if streams are lost as a result of the session disconnecting.
  * </p>
  *
  * @method #disconnect
  * @memberOf Session
  */
    this.disconnect = function() {
      if (_socket && _socket.isNot('disconnected')) {
        setState('disconnecting');
        _socket.disconnect();
      }
      else {
        reset.call(this);
      }
    };

    this.destroy = function(/*reason, quiet*/) {
      this.streams.destroy();
      this.connections.destroy();
      this.archives.destroy();
      this.disconnect();
    };

 /**
  * The <code>publish()</code> method starts publishing an audio-video stream to the session.
  * The audio-video stream is captured from a local microphone and webcam. Upon successful
  * publishing, the Session objects on all connected clients dispatch the
  * <code>streamCreated</code> event.
  * </p>
  *
  * <!--JS-ONLY-->
  * <p>You pass a Publisher object as the one parameter of the method. You can initialize a
  * Publisher object by calling the <a href="OT.html#initPublisher">OT.initPublisher()</a>
  * method. Before calling <code>Session.publish()</code>.
  * </p>
  *
  * <p>This method takes an alternate form: <code>publish([targetElement:String,
  * properties:Object]):Publisher</code> &#151; In this form, you do <i>not</i> pass a Publisher
  * object into the function. Instead, you pass in a <code>targetElement</code> (the ID of the
  * DOM element that the Publisher will replace) and a <code>properties</code> object that
  * defines options for the Publisher (see <a href="OT.html#initPublisher">OT.initPublisher()</a>.)
  * The method returns a new Publisher object, which starts sending an audio-video stream to the
  * session. The remainder of this documentation describes the form that takes a single Publisher
  * object as a parameter.
  *
  * <p>
  *   A local display of the published stream is created on the web page by replacing
  *         the specified element in the DOM with a streaming video display. The video stream
  *         is automatically mirrored horizontally so that users see themselves and movement
  *         in their stream in a natural way. If the width and height of the display do not match
  *         the 4:3 aspect ratio of the video signal, the video stream is cropped to fit the
  *         display.
  * </p>
  *
  * <p>
  *   If calling this method creates a new Publisher object and the OpenTok library does not
  *   have access to the camera or microphone, the web page alerts the user to grant access
  *   to the camera and microphone.
  * </p>
  *
  * <p>
  * The OT object dispatches an <code>exception</code> event if the user's role does not
  * include permissions required to publish. For example, if the user's role is set to subscriber,
  * then they cannot publish. You define a user's role when you create the user token using the
  * <code>generate_token()</code> method of the
  * <a href="/opentok/api/tools/documentation/api/server_side_libraries.html">OpenTok server-side
  * libraries</a> or the <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page.
  * You pass the token string as a parameter of the <code>connect()</code> method of the Session
  * object. See <a href="ExceptionEvent.html">ExceptionEvent</a> and
  * <a href="OT.html#on">OT.on()</a>.
  * </p>
  *     <p>
  *     The application throws an error if the session is not connected.
  *     </p>
  *
  * <h5>Events dispatched:</h5>
  * <p>
  * <code>exception</code> (<a href="ExceptionEvent.html">ExceptionEvent</a>) &#151; Dispatched
  * by the OT object. This can occur when user's role does not allow publishing (the
  * <code>code</code> property of event object is set to 1500); it can also occur if the c
  * onnection fails to connect (the <code>code</code> property of event object is set to 1013).
  * WebRTC is a peer-to-peer protocol, and it is possible that connections will fail to connect.
  * The most common cause for failure is a firewall that the protocol cannot traverse.</li>
  * </p>
  * <p>
  * <code>streamCreated</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  * The stream has been published. The Session object dispatches this on all clients
  * subscribed to the stream, as well as on the publisher's client.
  * </p>
  *
  * <h5>Example</h5>
  *
  * <p>
  *   The following example publishes a video once the session connects:
  * </p>
  * <pre>
  * var sessionId = ""; // Replace with your own session ID.
  *                     // See https://dashboard.tokbox.com/projects
  * var token = ""; // Replace with a generated token that has been assigned the moderator role.
  *                 // See https://dashboard.tokbox.com/projects
  * var session = OT.initSession(apiKey, sessionID);
  * session.on("sessionConnected", function (event) {
  *     var publisherOptions = {width: 400, height:300, name:"Bob's stream"};
  *     // This assumes that there is a DOM element with the ID 'publisher':
  *     publisher = OT.initPublisher('publisher', publisherOptions);
  *     session.publish(publisher);
  * });
  * session.connect(token);
  * </pre>
  *
  * @param {Publisher} publisher A Publisher object, which you initialize by calling the
  * <a href="OT.html#initPublisher">OT.initPublisher()</a> method.
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>publish()</code> method succeeds or fails. This function takes one parameter &mdash;
  * <code>error</code>. On success, the <code>completionHandler</code> function is not passed any
  * arguments. On error, the function is passed an <code>error</code> object parameter. The
  * <code>error</code> object has two properties: <code>code</code> (an integer) and
  * <code>message</code> (a string), which identify the cause of the failure. Calling
  * <code>publish()</code> fails if the role assigned to your token is not "publisher" or
  * "moderator"; in this case <code>error.code</code> is set to 1500. Calling
  * <code>publish()</code> also fails the client fails to connect; in this case
  * <code>error.code</code> is set to 1013. The following code adds a
  * <code>completionHandler</code> when calling the <code>publish()</code> method:
  * <pre>
  * session.publish(publisher, null, function (error) {
  *   if (error) {
  *     console.log(error.message);
  *   } else {
  *     console.log("Publishing a stream.");
  *   }
  * });
  * </pre>
  *
  * @returns The Publisher object for this stream.
  *
  * @method #publish
  * @memberOf Session
  */
    this.publish = function(publisher, properties, completionHandler) {
      if(typeof publisher === 'function') {
        completionHandler = publisher;
        publisher = undefined;
      }
      if(typeof properties === 'function') {
        completionHandler = properties;
        properties = undefined;
      }
      if (this.isNot('connected')) {
        /*jshint camelcase:false*/
        _analytics.logError(1010, 'OT.exception',
          'We need to be connected before you can publish', null, {
          action: 'publish',
          variation: 'Failure',
          payload_type: 'reason',
          payload: 'We need to be connected before you can publish',
          session_id: _sessionId,
          partner_id: _apiKey,
          widgetId: _widgetId,
          widget_type: 'Controller'
        });

        if (completionHandler && OT.$.isFunction(completionHandler)) {
          dispatchError(OT.ExceptionCodes.NOT_CONNECTED,
            'We need to be connected before you can publish', completionHandler);
        }

        return null;
      }

      if (!permittedTo('publish')) {
        this.logEvent('publish', 'Failure', 'reason',
          'This token does not allow publishing. The role must be at least `publisher` ' +
          'to enable this functionality');
        dispatchError(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
          'This token does not allow publishing. The role must be at least `publisher` ' +
          'to enable this functionality', completionHandler);
        return null;
      }

      // If the user has passed in an ID of a element then we create a new publisher.
      if (!publisher || typeof(publisher)==='string' || publisher.nodeType === Node.ELEMENT_NODE) {
        // Initiate a new Publisher with the new session credentials
        publisher = OT.initPublisher(publisher, properties);

      } else if (publisher instanceof OT.Publisher){

        // If the publisher already has a session attached to it we can
        if ('session' in publisher && publisher.session && 'sessionId' in publisher.session) {
          // send a warning message that we can't publish again.
          if( publisher.session.sessionId === this.sessionId){
            OT.warn('Cannot publish ' + publisher.guid + ' again to ' +
              this.sessionId + '. Please call session.unpublish(publisher) first.');
          } else {
            OT.warn('Cannot publish ' + publisher.guid + ' publisher already attached to ' +
              publisher.session.sessionId+ '. Please call session.unpublish(publisher) first.');
          }
        }

      } else {
        dispatchError(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
          'Session.publish :: First parameter passed in is neither a ' +
          'string nor an instance of the Publisher',
          completionHandler);
        return;
      }

      publisher.once('publishComplete', function(err) {
        if (err) {
          dispatchError(OT.ExceptionCodes.UNABLE_TO_PUBLISH,
            'Session.publish :: ' + err.message,
            completionHandler);
          return;
        }

        if (completionHandler && OT.$.isFunction(completionHandler)) {
          completionHandler.apply(null, arguments);
        }
      });

      // Add publisher reference to the session
      publisher._.publishToSession(this);

      // return the embed publisher
      return publisher;
    };

 /**
  * Ceases publishing the specified publisher's audio-video stream
  * to the session. By default, the local representation of the audio-video stream is
  * removed from the web page. Upon successful termination, the Session object on every
  * connected web page dispatches
  * a <code>streamDestroyed</code> event.
  * </p>
  *
  * <p>
  * To prevent the Publisher from being removed from the DOM, add an event listener for the
  * <code>streamDestroyed</code> event dispatched by the Publisher object and call the
  * <code>preventDefault()</code> method of the event object.
  * </p>
  *
  * <p>
  * <i>Note:</i> If you intend to reuse a Publisher object created using
  * <code>OT.initPublisher()</code> to publish to different sessions sequentially, call
  * either <code>Session.disconnect()</code> or <code>Session.unpublish()</code>. Do not call
  * both. Then call the <code>preventDefault()</code> method of the <code>streamDestroyed</code>
  * or <code>sessionDisconnected</code> event object to prevent the Publisher object from being
  * removed from the page. Be sure to call <code>preventDefault()</code> only if the
  * <code>connection.connectionId</code> property of the Stream object in the event matches the
  * <code>connection.connectionId</code> property of your Session object (to ensure that you are
  * preventing the default behavior for your published streams, not for other streams that you
  * subscribe to).
  * </p>
  *
  * <h5>Events dispatched:</h5>
  *
  * <p>
  * <code>streamDestroyed</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  * The stream associated with the Publisher has been destroyed. Dispatched on by the
  * Publisher on on the Publisher's browser. Dispatched by the Session object on
  * all other connections subscribing to the publisher's stream.
  * </p>
  *
  * <h5>Example</h5>
  *
  * The following example publishes a stream to a session and adds a Disconnect link to the
  * web page. Clicking this link causes the stream to stop being published.
  *
  * <pre>
  * &lt;script&gt;
  *     var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
  *     var sessionID = ""; // Replace with your own session ID.
  *                      // See https://dashboard.tokbox.com/projects
  *     var token = "Replace with the TokBox token string provided to you."
  *     var session = OT.initSession(apiKey, sessionID);
  *     session.on("sessionConnected", function sessionConnectHandler(event) {
  *         // This assumes that there is a DOM element with the ID 'publisher':
  *         publisher = OT.initPublisher('publisher');
  *         session.publish(publisher);
  *     });
  *     session.connect(token);
  *     var publisher;
  *
  *     function unpublish() {
  *         session.unpublish(publisher);
  *     }
  * &lt;/script&gt;
  *
  * &lt;body&gt;
  *
  *     &lt;div id="publisherContainer/&gt;
  *     &lt;br/&gt;
  *
  *     &lt;a href="javascript:unpublish()"&gt;Stop Publishing&lt;/a&gt;
  *
  * &lt;/body&gt;
  *
  * </pre>
  *
  * @see <a href="#publish">publish()</a>
  *
  * @see <a href="StreamEvent.html">streamDestroyed event</a>
  *
  * @param {Publisher} publisher</span> The Publisher object to stop streaming.
  *
  * @method #unpublish
  * @memberOf Session
  */
    this.unpublish = function(publisher) {
      if (!publisher) {
        OT.error('OT.Session.unpublish: publisher parameter missing.');
        return;
      }

      // Unpublish the localMedia publisher
      publisher._.unpublishFromSession(this, 'unpublished');
    };


 /**
  * Subscribes to a stream that is available to the session. You can get an array of
  * available streams from the <code>streams</code> property of the <code>sessionConnected</code>
  * and <code>streamCreated</code> events (see
  * <a href="SessionConnectEvent.html">SessionConnectEvent</a> and
  * <a href="StreamEvent.html">StreamEvent</a>).
  * </p>
  * <p>
  * The subscribed stream is displayed on the local web page by replacing the specified element
  * in the DOM with a streaming video display. If the width and height of the display do not
  * match the 4:3 aspect ratio of the video signal, the video stream is cropped to fit
  * the display. If the stream lacks a video component, a blank screen with an audio indicator
  * is displayed in place of the video stream.
  * </p>
  *
  * <p>
  * The application throws an error if the session is not connected<!--JS-ONLY--> or if the
  * <code>targetElement</code> does not exist in the HTML DOM<!--/JS-ONLY-->.
  * </p>
  *
  * <h5>Example</h5>
  *
  * The following code subscribes to other clients' streams:
  *
  * <pre>
  * var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
  * var sessionID = ""; // Replace with your own session ID.
  *                     // See https://dashboard.tokbox.com/projects
  *
  * var session = OT.initSession(apiKey, sessionID);
  * session.on("streamCreated", function(event) {
  *   subscriber = session.subscribe(event.stream, targetElement);
  * });
  * session.connect(token);
  * </pre>
  *
  * @param {Stream} stream The Stream object representing the stream to which we are trying to
  * subscribe.
  *
  * @param {Object} targetElement (Optional) The DOM element or the <code>id</code> attribute of
  * the existing DOM element used to determine the location of the Subscriber video in the HTML
  * DOM. See the <code>insertMode</code> property of the <code>properties</code> parameter. If
  * you do not specify a <code>targetElement</code>, the application appends a new DOM element
  * to the HTML <code>body</code>.
  *
  * @param {Object} properties This is an object that contains the following properties:
  *    <ul>
  *       <li><code>audioVolume</code> (Number) &#151; The desired audio volume, between 0 and
  *       100, when the Subscriber is first opened (default: 50). After you subscribe to the
  *       stream, you can adjust the volume by calling the
  *       <a href="Subscriber.html#setAudioVolume"><code>setAudioVolume()</code> method</a> of the
  *       Subscriber object. This volume setting affects local playback only; it does not affect
  *       the stream's volume on other clients.</li>
  *
  *       <li><code>height</code> (Number) &#151; The desired height, in pixels, of the
  *        displayed Subscriber video stream (default: 198). <i>Note:</i> Use the
  *       <code>height</code> and <code>width</code> properties to set the dimensions
  *       of the Subscriber video; do not set the height and width of the DOM element
  *       (using CSS).</li>
  *
  *       <li>
  *         <code>insertMode</code> (String) &#151; Specifies how the Subscriber object will
  *         be inserted in the HTML DOM. See the <code>targetElement</code> parameter. This
  *         string can have the following values:
  *         <ul>
  *           <li><code>"replace"</code> &#151; The Subscriber object replaces contents of the
  *             targetElement. This is the default.</li>
  *           <li><code>"after"</code> &#151; The Subscriber object is a new element inserted
  *             after the targetElement in the HTML DOM. (Both the Subscriber and targetElement
  *             have the same parent element.)</li>
  *           <li><code>"before"</code> &#151; The Subscriber object is a new element inserted
  *             before the targetElement in the HTML DOM. (Both the Subsciber and targetElement
  *             have the same parent element.)</li>
  *           <li><code>"append"</code> &#151; The Subscriber object is a new element added as a
  *             child of the targetElement. If there are other child elements, the Subscriber is
  *             appended as the last child element of the targetElement.</li>
  *         </ul>
  *       </li>
  *
  *   <li>
  *   <code>style</code> (Object) &#151; An object containing properties that define the initial
  *   appearance of user interface controls of the Subscriber. The <code>style</code> object
  *   includes the following properties:
  *     <ul>
  *       <li><code>backgroundImageURI</code> (String) &mdash; A URI for an image to display as
  *       the background image when a video is not displayed. (A video may not be displayed if
  *       you call <code>subscribeToVideo(false)</code> on the Subscriber object). You can pass an
  *       http or https URI to a PNG, JPEG, or non-animated GIF file location. You can also use the
  *       <code>data</code> URI scheme (instead of http or https) and pass in base-64-encrypted
  *       PNG data, such as that obtained from the
  *       <a href="Subscriber.html#getImgData">Subscriber.getImgData()</a> method. For example,
  *       you could set the property to <code>"data:VBORw0KGgoAA..."</code>, where the portion of
  *       the string after <code>"data:"</code> is the result of a call to
  *       <code>Subscriber.getImgData()</code>. If the URL or the image data is invalid, the
  *       property is ignored (the attempt to set the image fails silently).</li>
  *
  *       <li><code>buttonDisplayMode</code> (String) &mdash; How to display the speaker controls
  *       Possible values are: <code>"auto"</code> (controls are displayed when the stream is first
  *       displayed and when the user mouses over the display), <code>"off"</code> (controls are not
  *       displayed), and <code>"on"</code> (controls are always displayed).</li>
  *
  *       <li><code>nameDisplayMode</code> (String) &#151; Whether to display the stream name.
  *       Possible values are: <code>"auto"</code> (the name is displayed when the stream is first
  *       displayed and when the user mouses over the display), <code>"off"</code> (the name is not
  *       displayed), and <code>"on"</code> (the name is always displayed).</li>
  *   </ul>
  *   </li>
  *
  *       <li><code>subscribeToAudio</code> (Boolean) &#151; Whether to initially subscribe to audio
  *       (if available) for the stream (default: <code>true</code>).</li>
  *
  *       <li><code>subscribeToVideo</code> (Boolean) &#151; Whether to initially subscribe to video
  *       (if available) for the stream (default: <code>true</code>).</li>
  *
  *       <li><code>width</code> (Number) &#151; The desired width, in pixels, of the
  *       displayed Subscriber video stream (default: 264). <i>Note:</i> Use the
  *       <code>height</code> and <code>width</code> properties to set the dimensions
  *        of the Subscriber video; do not set the height and width of the DOM element
  *       (using CSS).</li>
  *
  *    </ul>
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>subscribe()</code> method succeeds or fails. This function takes one parameter &mdash;
  * <code>error</code>. On success, the <code>completionHandler</code> function is not passed any
  * arguments. On error, the function is passed an <code>error</code> object, defined by the
  * <a href="Error.html">Error</a> class, has two properties: <code>code</code> (an integer) and
  * <code>message</code> (a string), which identify the cause of the failure. The following
  * code adds a <code>completionHandler</code> when calling the <code>subscribe()</code> method:
  * <pre>
  * session.subscribe(stream, "subscriber", null, function (error) {
  *   if (error) {
  *     console.log(error.message);
  *   } else {
  *     console.log("Subscribed to stream: " + stream.id);
  *   }
  * });
  * </pre>
  *
  * @signature subscribe(stream, targetElement, properties, completionHandler)
  * @returns {Subscriber} The Subscriber object for this stream. Stream control functions
  * are exposed through the Subscriber object.
  * @method #subscribe
  * @memberOf Session
  */
    this.subscribe = function(stream, targetElement, properties, completionHandler) {

      if (!this.connection || !this.connection.connectionId) {
        dispatchError(OT.ExceptionCodes.UNABLE_TO_SUBSCRIBE,
                      'Session.subscribe :: Connection required to subscribe',
                      completionHandler);
        return;
      }

      if (!stream) {
        dispatchError(OT.ExceptionCodes.UNABLE_TO_SUBSCRIBE,
                      'Session.subscribe :: stream cannot be null',
                      completionHandler);
        return;
      }

      if (!stream.hasOwnProperty('streamId')) {
        dispatchError(OT.ExceptionCodes.UNABLE_TO_SUBSCRIBE,
                      'Session.subscribe :: invalid stream object',
                      completionHandler);
        return;
      }

      if(typeof targetElement === 'function') {
        completionHandler = targetElement;
        targetElement = undefined;
      }

      if(typeof properties === 'function') {
        completionHandler = properties;
        properties = undefined;
      }

      var subscriber = new OT.Subscriber(targetElement, OT.$.extend(properties || {}, {
        session: this
      }));

      subscriber.once('subscribeComplete', function(err) {
        if (err) {
          dispatchError(OT.ExceptionCodes.UNABLE_TO_SUBSCRIBE,
                'Session.subscribe :: ' + err.message,
                completionHandler);

          return;
        }

        if (completionHandler && OT.$.isFunction(completionHandler)) {
          completionHandler.apply(null, arguments);
        }
      });

      OT.subscribers.add(subscriber);
      subscriber.subscribe(stream);

      return subscriber;
    };

 /**
  * Stops subscribing to a stream in the session. the display of the audio-video stream is
  * removed from the local web page.
  *
  * <h5>Example</h5>
  * <p>
  * The following code subscribes to other clients' streams. For each stream, the code also
  * adds an Unsubscribe link.
  * </p>
  * <pre>
  * var apiKey = ""; // Replace with your API key. See https://dashboard.tokbox.com/projects
  * var sessionID = ""; // Replace with your own session ID.
  *                     // See https://dashboard.tokbox.com/projects
  * var streams = [];
  *
  * var session = OT.initSession(apiKey, sessionID);
  * session.on("streamCreated", function(event) {
  *     var stream = event.stream;
  *     displayStream(stream);
  * });
  * session.connect(token);
  *
  * function displayStream(stream) {
  *     var div = document.createElement('div');
  *     div.setAttribute('id', 'stream' + stream.streamId);
  *
  *     var subscriber = session.subscribe(stream, div);
  *     subscribers.push(subscriber);
  *
  *     var aLink = document.createElement('a');
  *     aLink.setAttribute('href', 'javascript: unsubscribe("' + subscriber.id + '")');
  *     aLink.innerHTML = "Unsubscribe";
  *
  *     var streamsContainer = document.getElementById('streamsContainer');
  *     streamsContainer.appendChild(div);
  *     streamsContainer.appendChild(aLink);
  *
  *     streams = event.streams;
  * }
  *
  * function unsubscribe(subscriberId) {
  *     console.log("unsubscribe called");
  *     for (var i = 0; i &lt; subscribers.length; i++) {
  *         var subscriber = subscribers[i];
  *         if (subscriber.id == subscriberId) {
  *             session.unsubscribe(subscriber);
  *         }
  *     }
  * }
  * </pre>
  *
  * @param {Subscriber} subscriber The Subscriber object to unsubcribe.
  *
  * @see <a href="#subscribe">subscribe()</a>
  *
  * @method #unsubscribe
  * @memberOf Session
  */
    this.unsubscribe = function(subscriber) {
      if (!subscriber) {
        var errorMsg = 'OT.Session.unsubscribe: subscriber cannot be null';
        OT.error(errorMsg);
        throw new Error(errorMsg);
      }

      if (!subscriber.stream) {
        OT.warn('OT.Session.unsubscribe:: tried to unsubscribe a subscriber that had no stream');
        return false;
      }

      OT.debug('OT.Session.unsubscribe: subscriber ' + subscriber.id);

      subscriber.destroy();

      return true;
    };

 /**
  * Returns an array of local Subscriber objects for a given stream.
  *
  * @param {Stream} stream The stream for which you want to find subscribers.
  *
  * @returns {Array} An array of {@link Subscriber} objects for the specified stream.
  *
  * @see <a href="#unsubscribe">unsubscribe()</a>
  * @see <a href="Subscriber.html">Subscriber</a>
  * @see <a href="StreamEvent.html">StreamEvent</a>
  * @method #getSubscribersForStream
  * @memberOf Session
  */
    this.getSubscribersForStream = function(stream) {
      return OT.subscribers.where({streamId: stream.id});
    };

 /**
  * Returns the local Publisher object for a given stream.
  *
  * @param {Stream} stream The stream for which you want to find the Publisher.
  *
  * @returns {Publisher} A Publisher object for the specified stream. Returns
  * <code>null</code> if there is no local Publisher object
  * for the specified stream.
  *
  * @see <a href="#forceUnpublish">forceUnpublish()</a>
  * @see <a href="Subscriber.html">Subscriber</a>
  * @see <a href="StreamEvent.html">StreamEvent</a>
  *
  * @method #getPublisherForStream
  * @memberOf Session
  */
    this.getPublisherForStream = function(stream) {
      var streamId,
          errorMsg;

      if (typeof stream === 'string') {
        streamId = stream;
      } else if (typeof stream === 'object' && stream && stream.hasOwnProperty('id')) {
        streamId = stream.id;
      } else {
        errorMsg = 'Session.getPublisherForStream :: Invalid stream type';
        OT.error(errorMsg);
        throw new Error(errorMsg);
      }

      return OT.publishers.where({streamId: streamId})[0];
    };


    // Private Session API: for internal OT use only
    this._ = {
      jsepCandidateP2p: function(streamId, subscriberId, candidate) {
        return _socket.jsepCandidateP2p(streamId, subscriberId, candidate);
      }.bind(this),

      jsepCandidate: function(streamId, candidate) {
        return _socket.jsepCandidate(streamId, candidate);
      }.bind(this),

      jsepOffer: function(streamId, offerSdp) {
        return _socket.jsepOffer(streamId, offerSdp);
      }.bind(this),

      jsepOfferP2p: function(streamId, subscriberId, offerSdp) {
        return _socket.jsepOfferP2p(streamId, subscriberId, offerSdp);
      }.bind(this),

      jsepAnswer: function(streamId, answerSdp) {
        return _socket.jsepAnswer(streamId, answerSdp);
      }.bind(this),

      jsepAnswerP2p: function(streamId, subscriberId, answerSdp) {
        return _socket.jsepAnswerP2p(streamId, subscriberId, answerSdp);
      }.bind(this),

      // session.on("signal", function(SignalEvent))
      // session.on("signal:{type}", function(SignalEvent))
      dispatchSignal: function(fromConnection, type, data) {
        var event = new OT.SignalEvent(type, data, fromConnection);
        event.target = this;

        // signal a "signal" event
        // NOTE: trigger doesn't support defaultAction, and therefore preventDefault.
        this.trigger(OT.Event.names.SIGNAL, event);

        // signal an "signal:{type}" event" if there was a custom type
        if (type) this.dispatchEvent(event);
      }.bind(this),

      subscriberCreate: function(stream, subscriber, channelsToSubscribeTo, completion) {
        return _socket.subscriberCreate(stream.id, subscriber.widgetId,
          channelsToSubscribeTo, completion);
      }.bind(this),

      subscriberDestroy: function(stream, subscriber) {
        return _socket.subscriberDestroy(stream.id, subscriber.widgetId);
      }.bind(this),

      subscriberUpdate: function(stream, subscriber, attributes) {
        return _socket.subscriberUpdate(stream.id, subscriber.widgetId, attributes);
      }.bind(this),

      subscriberChannelUpdate: function(stream, subscriber, channel, attributes) {
        return _socket.subscriberChannelUpdate(stream.id, subscriber.widgetId, channel.id,
          attributes);
      }.bind(this),

      streamCreate: function(name, orientation, encodedWidth, encodedHeight, hasAudio, hasVideo,
        frameRate, completion) {
        _socket.streamCreate(name, orientation, encodedWidth, encodedHeight, hasAudio, hasVideo,
          frameRate, OT.Config.get('bitrates', 'min', OT.APIKEY),
          OT.Config.get('bitrates', 'max', OT.APIKEY), completion);
      }.bind(this),

      streamDestroy: function(streamId) {
        _socket.streamDestroy(streamId);
      }.bind(this),

      streamChannelUpdate: function(stream, channel, attributes) {
        _socket.streamChannelUpdate(stream.id, channel.id, attributes);
      }.bind(this)
    };


 /**
  * Sends a signal to each client or specified clients in the session. Specify a
  * <code>connections</code> property of the <code>signal</code> parameter to limit the
  * recipients of the signal; otherwise the signal is sent to each client connected to
  * the session.
  * <p>
  * The following example sends a signal of type "foo" with a specified data payload to all
  * clients connected to the session:
  * <pre>
  * session.signal({
  *     type: "foo",
  *     data: "hello"
  *   },
  *   function(error) {
  *     if (error) {
  *       console.log("signal error: " + error.reason);
  *     } else {
  *       console.log("signal sent");
  *     }
  *   }
  * );
  * </pre>
  * <p>
  * Calling this method without limiting the set of recipient clients will result in
  * multiple signals sent (one to each client in the session). For information on charges
  * for signaling, see the <a href="http://tokbox.com/pricing">OpenTok
  * pricing</a> page.
  * <p>
  * The following example sends a signal of type "foo" with a specified data payload to two
  * specific clients connected to the session:
  * <pre>
  * session.signal({
  *     type: "foo",
  *     to: [connection1, connection2]; // connection1 and 2 are Connection objects
  *     data: "hello"
  *   },
  *   function(error) {
  *     if (error) {
  *       console.log("signal error: " + error.reason);
  *     } else {
  *       console.log("signal sent");
  *     }
  *   }
  * );
  * </pre>
  * <p>
  * Add an event handler for the <code>signal</code> event to listen for all signals sent in
  * the session. Add an event handler for the <code>signal:type</code> event to listen for
  * signals of a specified type only (replace <code>type</code>, in <code>signal:type</code>,
  * with the type of signal to listen for). The Session object dispatches these events. (See
  * <a href="#events">events</a>.)
  *
  * @param {Object} signal An object that contains the following properties defining the signal:
  * <ul>
  *   <li><code>to</code> &mdash; (Array) An array of <a href="Connection.html">Connection</a>
  *      objects, corresponding to clients that the message is to be sent to. If you do not
  *      specify this property, the signal is sent to all clients connected to the session.</li>
  *   <li><code>data</code> &mdash; (String) The data to send. The limit to the length of data
  *     string is 8kB. Do not set the data string to <code>null</code> or
  *     <code>undefined</code>.</li>
  *   <li><code>type</code> &mdash; (String) The type of the signal. You can use the type to
  *     filter signals when setting an event handler for the <code>signal:type</code> event
  *     (where you replace <code>type</code> with the type string). The maximum length of the
  *     <code>type</code> string is 128 characters, and it must contain only letters (A-Z and a-z),
  *     numbers (0-9), '-', '_', and '~'.</li>
  *   </li>
  * </ul>
  *
  * <p>Each property is optional. If you set none of the properties, you will send a signal
  * with no data or type to each client connected to the session.</p>
  *
  * @param {Function} completionHandler A function that is called when sending the signal
  * succeeds or fails. This function takes one parameter &mdash; <code>error</code>.
  * On success, the <code>completionHandler</code> function is not passed any
  * arguments. On error, the function is passed an <code>error</code> object, defined by the
  * <a href="Error.html">Error</a> class. The <code>error</code> object has the following
  * properties:
  *
  * <ul>
  *   <li><code>code</code> &mdash; (Number) An error code, which can be one of the following:
  *     <table style="width:100%">
  *         <tr>
  *           <td>400</td> <td>One of the signal properties &mdash; data, type, or to &mdash;
  *                         is invalid. Or the data cannot be parsed as JSON.</td>
  *         </tr>
  *         <tr>
  *           <td>404</td> <td>The to connection does not exist.</td>
  *         </tr>
  *         <tr>
  *           <td>413</td> <td>The type string exceeds the maximum length (128 bytes),
  *                        or the data string exceeds the maximum size (8 kB).</td>
  *         </tr>
  *         <tr>
  *           <td>500</td> <td>You are not connected to the OpenTok session.</td>
  *         </tr>
  *      </table>
  *   </li>
  *   <li><code>reason</code> &mdash; (String) A description of the error.</li>
  *   <li><code>signal</code> &mdash; (Object) An object with properties corresponding to the
  *     values passed into the <code>signal()</code> method &mdash; <code>data</code>,
  *     <code>to</code>, and <code>type</code>.
  *   </li>
  * </ul>
  *
  * <p>Note that the <code>completionHandler</code> success result (<code>error == null</code>)
  * indicates that the options passed into the <code>Session.signal()</code> method are valid
  * and the signal was sent. It does <i>not</i> indicate that the signal was sucessfully
  * received by any of the intended recipients.
  *
  * @method #signal
  * @memberOf Session
  * @see <a href="#event:signal">signal</a> and <a href="#event:signal:type">signal:type</a> events
  */
    this.signal = function(options, completion) {
      var _options = options,
          _completion = completion;

      if (OT.$.isFunction(_options)) {
        _completion = _options;
        _options = null;
      }

      if (this.isNot('connected')) {
        var notConnectedErrorMsg = 'Unable to send signal - you are not connected to the session.';
        dispatchError(500, notConnectedErrorMsg, _completion);
        return;
      }

      _socket.signal(_options, _completion);
      if (options && options.data && (typeof(options.data) !== 'string')) {
        OT.warn('Signaling of anything other than Strings is deprecated. ' +
                'Please update the data property to be a string.');
      }
      this.logEvent('signal', 'send', 'type',
        (options && options.data) ? typeof(options.data) : 'null');
    };

 /**
  *   Forces a remote connection to leave the session.
  *
  * <p>
  *   The <code>forceDisconnect()</code> method is normally used as a moderation tool
  *        to remove users from an ongoing session.
  * </p>
  * <p>
  *   When a connection is terminated using the <code>forceDisconnect()</code>,
  *        <code>sessionDisconnected</code>, <code>connectionDestroyed</code> and
  *        <code>streamDestroyed</code> events are dispatched in the same way as they
  *        would be if the connection had terminated itself using the <code>disconnect()</code>
  *        method. However, the <code>reason</code> property of a {@link ConnectionEvent} or
  *        {@link StreamEvent} object specifies <code>"forceDisconnected"</code> as the reason
  *        for the destruction of the connection and stream(s).
  * </p>
  * <p>
  *   While you can use the <code>forceDisconnect()</code> method to terminate your own connection,
  *        calling the <code>disconnect()</code> method is simpler.
  * </p>
  * <p>
  *   The OT object dispatches an <code>exception</code> event if the user's role
  *   does not include permissions required to force other users to disconnect.
  *   You define a user's role when you create the user token using the
  *   <code>generate_token()</code> method using
  *   <a href="/opentok/api/tools/documentation/api/server_side_libraries.html">OpenTok
  *   server-side libraries</a> or the
  *   <a href="https://dashboard.tokbox.com/projects">Dashboard</a> page.
  *   See <a href="ExceptionEvent.html">ExceptionEvent</a> and <a href="OT.html#on">OT.on()</a>.
  * </p>
  * <p>
  *   The application throws an error if the session is not connected.
  * </p>
  *
  * <h5>Events dispatched:</h5>
  *
  * <p>
  *   <code>connectionDestroyed</code> (<a href="ConnectionEvent.html">ConnectionEvent</a>) &#151;
  *     On clients other than which had the connection terminated.
  * </p>
  * <p>
  *   <code>exception</code> (<a href="ExceptionEvent.html">ExceptionEvent</a>) &#151;
  *     The user's role does not allow forcing other user's to disconnect (<code>event.code =
  *     1530</code>),
  *   or the specified stream is not publishing to the session (<code>event.code = 1535</code>).
  * </p>
  * <p>
  *   <code>sessionDisconnected</code>
  *   (<a href="SessionDisconnectEvent.html">SessionDisconnectEvent</a>) &#151;
  *     On the client which has the connection terminated.
  * </p>
  * <p>
  *   <code>streamDestroyed</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  *     If streams are stopped as a result of the connection ending.
  * </p>
  *
  * @param {Connection} connection The connection to be disconnected from the session.
  * This value can either be a <a href="Connection.html">Connection</a> object or a connection
  * ID (which can be obtained from the <code>connectionId</code> property of the Connection object).
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>forceDiscononnect()</code> method succeeds or fails. This function takes one parameter
  * &mdash; <code>error</code>. On success, the <code>completionHandler</code> function is
  * not passed any arguments. On error, the function is passed an <code>error</code> object
  * parameter. The <code>error</code> object, defined by the <a href="Error.html">Error</a>
  * class, has two properties: <code>code</code> (an integer)
  * and <code>message</code> (a string), which identify the cause of the failure.
  * Calling <code>forceDisconnect()</code> fails if the role assigned to your
  * token is not "moderator"; in this case <code>error.code</code> is set to 1520. The following
  * code adds a <code>completionHandler</code> when calling the <code>forceDisconnect()</code>
  * method:
  * <pre>
  * session.forceDisconnect(connection, function (error) {
  *   if (error) {
  *       console.log(error);
  *     } else {
  *       console.log("Connection forced to disconnect: " + connection.id);
  *     }
  *   });
  * </pre>
  *
  * @method #forceDisconnect
  * @memberOf Session
  */

    this.forceDisconnect = function(connectionOrConnectionId, completionHandler) {
      if (this.isNot('connected')) {
        var notConnectedErrorMsg = 'Cannot call forceDisconnect(). You are not ' +
                                   'connected to the session.';
        dispatchError(OT.ExceptionCodes.NOT_CONNECTED, notConnectedErrorMsg, completionHandler);
        return;
      }

      var notPermittedErrorMsg = 'This token does not allow forceDisconnect. ' +
        'The role must be at least `moderator` to enable this functionality';

      if (permittedTo('forceDisconnect')) {
        var connectionId = typeof connectionOrConnectionId === 'string' ?
          connectionOrConnectionId : connectionOrConnectionId.id;

        _socket.forceDisconnect(connectionId, function(err) {
          if (err) {
            dispatchError(OT.ExceptionCodes.UNABLE_TO_FORCE_DISCONNECT,
              notPermittedErrorMsg, completionHandler);

          } else if (completionHandler && OT.$.isFunction(completionHandler)) {
            completionHandler.apply(null, arguments);
          }
        });
      } else {
        // if this throws an error the handleJsException won't occur
        dispatchError(OT.ExceptionCodes.UNABLE_TO_FORCE_DISCONNECT,
          notPermittedErrorMsg, completionHandler);
      }
    };

 /**
  * Forces the publisher of the specified stream to stop publishing the stream.
  *
  * <p>
  * Calling this method causes the Session object to dispatch a <code>streamDestroyed</code>
  * event on all clients that are subscribed to the stream (including the client that is
  * publishing the stream). The <code>reason</code> property of the StreamEvent object is
  * set to <code>"forceUnpublished"</code>.
  * </p>
  * <p>
  * The OT object dispatches an <code>exception</code> event if the user's role
  * does not include permissions required to force other users to unpublish.
  * You define a user's role when you create the user token using the <code>generate_token()</code>
  * method using the
  * <a href="/opentok/api/tools/documentation/api/server_side_libraries.html">OpenTok
  * server-side libraries</a> or the <a href="https://dashboard.tokbox.com/projects">Dashboard</a>
  * page.
  * You pass the token string as a parameter of the <code>connect()</code> method of the Session
  * object. See <a href="ExceptionEvent.html">ExceptionEvent</a> and
  * <a href="OT.html#on">OT.on()</a>.
  * </p>
  *
  * <h5>Events dispatched:</h5>
  *
  * <p>
  *   <code>exception</code> (<a href="ExceptionEvent.html">ExceptionEvent</a>) &#151;
  *     The user's role does not allow forcing other users to unpublish.
  * </p>
  * <p>
  *   <code>streamDestroyed</code> (<a href="StreamEvent.html">StreamEvent</a>) &#151;
  *     The stream has been unpublished. The Session object dispatches this on all clients
  *     subscribed to the stream, as well as on the publisher's client.
  * </p>
  *
  * @param {Stream} stream The stream to be unpublished.
  *
  * @param {Function} completionHandler (Optional) A function to be called when the call to the
  * <code>forceUnpublish()</code> method succeeds or fails. This function takes one parameter
  * &mdash; <code>error</code>. On success, the <code>completionHandler</code> function is
  * not passed any arguments. On error, the function is passed an <code>error</code> object
  * parameter. The <code>error</code> object, defined by the <a href="Error.html">Error</a>
  * class, has two properties: <code>code</code> (an integer)
  * and <code>message</code> (a string), which identify the cause of the failure. Calling
  * <code>forceUnpublish()</code> fails if the role assigned to your token is not "moderator";
  * in this case <code>error.code</code> is set to 1530. The following code adds a
  * <code>completionHandler</code> when calling the <code>forceUnpublish()</code> method:
  * <pre>
  * session.forceUnpublish(stream, function (error) {
  *   if (error) {
  *       console.log(error);
  *     } else {
  *       console.log("Connection forced to disconnect: " + connection.id);
  *     }
  *   });
  * </pre>
  *
  * @method #forceUnpublish
  * @memberOf Session
  */
    this.forceUnpublish = function(streamOrStreamId, completionHandler) {
      if (this.isNot('connected')) {
        var notConnectedErrorMsg = 'Cannot call forceUnpublish(). You are not ' +
                                   'connected to the session.';
        dispatchError(OT.ExceptionCodes.NOT_CONNECTED, notConnectedErrorMsg, completionHandler);
        return;
      }

      var notPermittedErrorMsg = 'This token does not allow forceUnpublish. ' +
        'The role must be at least `moderator` to enable this functionality';

      if (permittedTo('forceUnpublish')) {
        var stream = typeof streamOrStreamId === 'string' ?
          this.streams.get(streamOrStreamId) : streamOrStreamId;

        _socket.forceUnpublish(stream.id, function(err) {
          if (err) {
            dispatchError(OT.ExceptionCodes.UNABLE_TO_FORCE_UNPUBLISH,
              notPermittedErrorMsg, completionHandler);
          } else if (completionHandler && OT.$.isFunction(completionHandler)) {
            completionHandler.apply(null, arguments);
          }
        });
      } else {
        // if this throws an error the handleJsException won't occur
        dispatchError(OT.ExceptionCodes.UNABLE_TO_FORCE_UNPUBLISH,
          notPermittedErrorMsg, completionHandler);
      }
    };

    this.getStateManager = function() {
      OT.warn('Fixme: Have not implemented session.getStateManager');
    };

    OT.$.defineGetters(this, {
      apiKey: function() { return _apiKey; },
      token: function() { return _token; },
      connected: function() { return this.is('connected'); },
      connection: function() {
        return _socket && _socket.id ? this.connections.get(_socket.id) : null;
      },
      sessionId: function() { return _sessionId; },
      id: function() { return _sessionId; },
      capabilities: function() {
        var connection =  this.connection;
        return connection ? connection.permissions : new OT.Capabilities([]);
      }.bind(this)
    }, true);


  /**
   * A new client, other than your own, has connected to the session.
   * @name connectionCreated
   * @event
   * @memberof Session
   * @see ConnectionEvent
   * @see <a href="OT.html#initSession">OT.initSession()</a>
   */

  /**
   * A client, other than your own, has disconnected from the session.
   * @name connectionDestroyed
   * @event
   * @memberof Session
   * @see ConnectionEvent
   */

  /**
   * The page has connected to an OpenTok session. This event is dispatched asynchronously
   * in response to a successful call to the <code>connect()</code> method of a Session
   * object. Before calling the <code>connect()</code> method, initialize the session by
   * calling the <code>OT.initSession()</code> method. For a code example and more details,
   * see <a href="#connect">Session.connect()</a>.
   * @name sessionConnected
   * @event
   * @memberof Session
   * @see SessionConnectEvent
   * @see <a href="#connect">Session.connect()</a>
   * @see <a href="OT.html#initSession">OT.initSession()</a>
   */

  /**
   * The client has disconnected from the session. This event may be dispatched asynchronously
   * in response to a successful call to the <code>disconnect()</code> method of the Session object.
   * The event may also be disptached if a session connection is lost inadvertantly, as in the case
   * of a lost network connection.
   * <p>
   * The default behavior is that all Subscriber objects are unsubscribed and removed from the
   * HTML DOM. Each Subscriber object dispatches a <code>destroyed</code> event when the element is
   * removed from the HTML DOM. If you call the <code>preventDefault()</code> method in the event
   * listener for the <code>sessionDisconnect</code> event, the default behavior is prevented, and
   * you can, optionally, clean up Subscriber objects using your own code.
*
   * @name sessionDisconnected
   * @event
   * @memberof Session
   * @see <a href="#disconnect">Session.disconnect()</a>
   * @see <a href="#disconnect">Session.forceDisconnect()</a>
   * @see SessionDisconnectEvent
   */

  /**
   * A new stream, published by another client, has been created on this session. For streams
   * published by your own client, the Publisher object dispatches a <code>streamCreated</code>
   * event. For a code example and more details, see {@link StreamEvent}.
   * @name streamCreated
   * @event
   * @memberof Session
   * @see StreamEvent
   * @see <a href="Session.html#publish">Session.publish()</a>
   */

	/**
	 * A stream from another client has stopped publishing to the session.
	 * <p>
	 * The default behavior is that all Subscriber objects that are subscribed to the stream are
	 * unsubscribed and removed from the HTML DOM. Each Subscriber object dispatches a
	 * <code>destroyed</code> event when the element is removed from the HTML DOM. If you call the
	 * <code>preventDefault()</code> method in the event listener for the
	 * <code>streamDestroyed</code> event, the default behavior is prevented and you can clean up
	 * Subscriber objects using your own code. See
	 * <a href="Session.html#getSubscribersForStream">Session.getSubscribersForStream()</a>.
	 * <p>
	 * For streams published by your own client, the Publisher object dispatches a
	 * <code>streamDestroyed</code> event.
	 * <p>
	 * For a code example and more details, see {@link StreamEvent}.
	 * @name streamDestroyed
	 * @event
	 * @memberof Session
	 * @see StreamEvent
	 */

	/**
	 * A stream has started or stopped publishing audio or video (see
	 * <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a> and
	 * <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a>); or the
	 * <code>videoDimensions</code> property of the Stream
	 * object has changed (see <a href="Stream.html#"videoDimensions>Stream.videoDimensions</a>).
	 * @name streamPropertyChanged
	 * @event
	 * @memberof Session
	 * @see StreamPropertyChangedEvent
	 * @see <a href="Publisher.html#publishAudio">Publisher.publishAudio()</a>
	 * @see <a href="Publisher.html#publishVideo">Publisher.publishVideo()</a>
	 * @see <a href="Stream.html#"hasAudio>Stream.hasAudio</a>
	 * @see <a href="Stream.html#"hasVideo>Stream.hasVideo</a>
	 * @see <a href="Stream.html#"videoDimensions>Stream.videoDimensions</a>
	 */

	/**
	 * A signal was received from the session. The <a href="SignalEvent.html">SignalEvent</a>
	 * class defines this event object. It includes the following properties:
	 * <ul>
	 *   <li><code>data</code> &mdash; (String) The data string sent with the signal (if there
	 *       is one).</li>
	 *   <li><code>from</code> &mdash; (<a href="Connection.html">Connection</a>) The Connection
	 *       corresponding to the client that sent with the signal.</li>
	 *   <li><code>type</code> &mdash; (String) The type assigned to the signal (if there is
	 *       one).</li>
	 * </ul>
	 * <p>
	 * You can register to receive all signals sent in the session, by adding an event handler
	 * for the <code>signal</code> event. For example, the following code adds an event handler
	 * to process all signals sent in the session:
	 * <pre>
	 * session.on("signal", function(event) {
	 *   console.log("Signal sent from connection: " + event.from.id);
	 *   console.log("Signal data: " + event.data);
	 * });
	 * </pre>
	 * <p>You can register for signals of a specfied type by adding an event handler for the
	 * <code>signal:type</code> event (replacing <code>type</code> with the actual type string
	 * to filter on).
	 *
	 * @name signal
	 * @event
	 * @memberof Session
	 * @see <a href="Session.html#signal">Session.signal()</a>
	 * @see SignalEvent
	 * @see <a href="#event:signal:type">signal:type</a> event
	 */

	/**
	 * A signal of the specified type was received from the session. The
	 * <a href="SignalEvent.html">SignalEvent</a> class defines this event object.
	 * It includes the following properties:
	 * <ul>
	 *   <li><code>data</code> &mdash; (String) The data string sent with the signal.</li>
	 *   <li><code>from</code> &mdash; (<a href="Connection.html">Connection</a>) The Connection
	 *   corresponding to the client that sent with the signal.</li>
	 *   <li><code>type</code> &mdash; (String) The type assigned to the signal (if there is one).
	 *   </li>
	 * </ul>
	 * <p>
	 * You can register for signals of a specfied type by adding an event handler for the
	 * <code>signal:type</code> event (replacing <code>type</code> with the actual type string
	 * to filter on). For example, the following code adds an event handler for signals of
	 * type "foo":
	 * <pre>
	 * session.on("signal:foo", function(event) {
	 *   console.log("foo signal sent from connection " + event.from.id);
	 *   console.log("Signal data: " + event.data);
	 * });
	 * </pre>
	 * <p>
	 * You can register to receive <i>all</i> signals sent in the session, by adding an event
	 * handler for the <code>signal</code> event.
	 *
	 * @name signal:type
	 * @event
	 * @memberof Session
	 * @see <a href="Session.html#signal">Session.signal()</a>
	 * @see SignalEvent
	 * @see <a href="#event:signal">signal</a> event
	 */
  };

})(window);
!(function() {
  var style = document.createElement('link');
  style.type = 'text/css';
  style.media = 'screen';
  style.rel = 'stylesheet';
  style.href = OT.properties.cssURL;
  var head = document.head || document.getElementsByTagName('head')[0];
  head.appendChild(style);
})(window);
!(function(){
/*global define*/

// Register as a named AMD module, since TokBox could be concatenated with other
// files that may use define, but not via a proper concatenation script that
// understands anonymous AMD modules. A named AMD is safest and most robust
// way to register. Uppercase TB is used because AMD module names are
// derived from file names, and OpenTok is normally delivered in an uppercase
// file name.
  if (typeof define === 'function' && define.amd) {
    define( 'TB', [], function () { return TB; } );
  }

})(window);
