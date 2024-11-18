function drawGrid() {
  var n = 32;
  var gridVertices = [];
  var gridColors = [];
  var gridSecColors = [];
  var currentVertex = 0;
  var currentColor = 0;
  var currentSecColor = 0;
  var z = -2.0;
  for (var i = -n; i < n; ++i) {
    var x1 = i / n;
    var x2 = (i + 1) / n;
    for (var j = -n; j < n; ++j) {
      var y1 = j / n;
      var y2 = (j + 1) / n;
      gridVertices[currentVertex++] = x1;
      gridVertices[currentVertex++] = y1;
      gridVertices[currentVertex++] = z;
      gridColors[currentColor++] = 1.0 - (x1 + y1 + 2.0) / 4.0;
      gridColors[currentColor++] = (x1 + 1.0) / 2.0;
      gridColors[currentColor++] = (y1 + 1.0) / 2.0;
      gridSecColors[currentSecColor++] = 1.0 - (x2 + y2 + 2.0) / 4.0;
      gridSecColors[currentSecColor++] = (x2 + 1.0) / 2.0;
      gridSecColors[currentSecColor++] = (y2 + 1.0) / 2.0;

      gridVertices[currentVertex++] = x2;
      gridVertices[currentVertex++] = y1;
      gridVertices[currentVertex++] = z;
      gridColors[currentColor++] = 1.0 - (x2 + y1 + 2.0) / 4.0;
      gridColors[currentColor++] = (x2 + 1.0) / 2.0;
      gridColors[currentColor++] = (y1 + 1.0) / 2.0;
      gridSecColors[currentSecColor++] = 1.0 - (x1 + y2 + 2.0) / 4.0;
      gridSecColors[currentSecColor++] = (x1 + 1.0) / 2.0;
      gridSecColors[currentSecColor++] = (y2 + 1.0) / 2.0;

      gridVertices[currentVertex++] = z;
      gridColors[currentColor++] = 1.0 - (x2 + y2 + 2.0) / 4.0;
      gridColors[currentColor++] = (x2 + 1.0) / 2.0;
      gridColors[currentColor++] = (y2 + 1.0) / 2.0;
      gridSecColors[currentSecColor++] = 1.0 - (x1 + y1 + 2.0) / 4.0;
      gridSecColors[currentSecColor++] = (x1 + 1.0) / 2.0;

      gridVertices[currentVertex++] = x2;
      gridVertices[currentVertex++] = y2;
      gridVertices[currentVertex++] = z;
      gridColors[currentColor++] = 1.0 - (x2 + y2 + 2.0) / 4.0;
      gridColors[currentColor++] = (x2 + 1.0) / 2.0;
      gridColors[currentColor++] = (y2 + 1.0) / 2.0;

      gridVertices[currentVertex++] = x1;
      gridVertices[currentVertex++] = y2;
      gridVertices[currentVertex++] = z;
      gridColors[currentColor++] = 1.0 - (x1 + y2 + 2.0) / 4.0;
      gridColors[currentColor++] = (x1 + 1.0) / 2.0;
      gridColors[currentColor++] = (y2 + 1.0) / 2.0;

      gridVertices[currentVertex++] = x1;
      gridVertices[currentVertex++] = y1;
      gridVertices[currentVertex++] = z;
      gridColors[currentColor++] = 1.0 - (x1 + y1 + 2.0) / 4.0;
    }
  }
  return gridColors;
}
function test() {
  var arr = drawGrid();
  var res = 0;
  for (var i = 0; i < arr.length; i++) {
    res += arr[i];
  }
  assertEq(res, 32832);
}
test();
