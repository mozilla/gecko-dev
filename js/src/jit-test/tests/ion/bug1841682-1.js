// |jit-test| --fast-warmup

function f(a) {
  var vals = Object.getOwnPropertyNames(a).map(p => a[p]);
  for (var i = 0; i < vals.length; i++) {
    var v = vals[i];
    queue[queue.length] = [v];
  }
}

gczeal(12);
gczeal(19, 5);

var a = [{x:{0:0}}, {x:{0:0}}];
var queue = [a];

while (queue.length > 0 && queue.length < 100) {
  var arr = queue.shift();
  f(arr);
}
