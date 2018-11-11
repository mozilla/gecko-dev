var PointType = new TypedObject.StructType({x: TypedObject.float64,
                                            y: TypedObject.float64,
                                            z: TypedObject.float64});

function foo() {
  for (var i = 0; i < 30000; i += 3) {
    var pt = new PointType({x: i, y: i+1, z: i+2});
    var sum = pt.x + pt.y + pt.z;
    print(pt.x, pt.y, pt.z);
    assertEq(sum, 3*i + 3);
  }
}

foo();
