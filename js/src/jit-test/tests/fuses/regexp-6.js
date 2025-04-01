// Tests for String.prototype builtins with custom regexp symbols on
// RegExp.prototype.
// Use fresh globals because this pops realm fuses.

// Custom RegExp.prototype[@@match].
newGlobal().evaluate(`(function() {
  var s = "foobar";
  var re = /abc.+/;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.match(re);
    if (i === 150) {
      RegExp.prototype[Symbol.match] = function() {
        count++;
        return null;
      };
    }
  }
  assertEq(count, 49);
})();`);

// Custom RegExp.prototype[@@matchAll].
newGlobal().evaluate(`(function() {
  var s = "foobar";
  var re = /abc.+/g;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.matchAll(re);
    if (i === 150) {
      RegExp.prototype[Symbol.matchAll] = function() {
        count++;
        return null;
      };
    }
  }
  assertEq(count, 49);
})();`);

// Custom RegExp.prototype[@@replace] for replace.
newGlobal().evaluate(`(function() {
  var s = "foobar";
  var re = /abc.+/;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.replace(re, "");
    if (i === 150) {
      RegExp.prototype[Symbol.replace] = function() {
        count++;
        return "";
      };
    }
  }
  assertEq(count, 49);
})();`);

// Custom RegExp.prototype[@@replace] for replaceAll.
newGlobal().evaluate(`(function() {
  var s = "foobar";
  var re = /abc.+/g;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.replaceAll(re, "");
    if (i === 150) {
      RegExp.prototype[Symbol.replace] = function() {
        count++;
        return "";
      };
    }
  }
  assertEq(count, 49);
})();`);

// Custom RegExp.prototype[@@search].
newGlobal().evaluate(`(function() {
  var s = "foobar";
  var re = /abc.+/g;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.search(re);
    if (i === 150) {
      RegExp.prototype[Symbol.search] = function() {
        count++;
        return -1;
      };
    }
  }
  assertEq(count, 49);
})();`);

// Custom RegExp.prototype[@@split].
newGlobal().evaluate(`(function() {
  var s = "foobar";
  var re = /abc.+/;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.split(re);
    if (i === 150) {
      RegExp.prototype[Symbol.split] = function() {
        count++;
        return [];
      };
    }
  }
  assertEq(count, 49);
})();`);
