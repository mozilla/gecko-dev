// |jit-test| error: ReferenceError
p = Proxy.create({
  has: function() {},
  set: function() {}
})
Object.prototype.__proto__ = p
n = [];
(function() {
  var a = [];
  if (b) t = a.s()
})()
