load(libdir + "asm.js");
load(libdir + "asserts.js");
var heap = new ArrayBuffer(0x10000);

// Set to true to see more JS debugging spew
const DEBUG = false;

if (!isSimdAvailable() || typeof SIMD === 'undefined') {
    DEBUG && print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

const I32 = 'var i4 = glob.SIMD.int32x4;'
const CI32 = 'var ci4 = i4.check;'
const I32A = 'var i4a = i4.add;'
const I32S = 'var i4s = i4.sub;'
const I32M = 'var i4m = i4.mul;'
const F32 = 'var f4 = glob.SIMD.float32x4;'
const CF32 = 'var cf4 = f4.check;'
const F32A = 'var f4a = f4.add;'
const F32S = 'var f4s = f4.sub;'
const F32M = 'var f4m = f4.mul;'
const F32D = 'var f4d = f4.div;'
const FROUND = 'var f32=glob.Math.fround;'

const INT32_MAX = Math.pow(2, 31) - 1;
const INT32_MIN = INT32_MAX + 1 | 0;

const assertEqFFI = {assertEq:assertEq};

function assertEqX4(real, expected, assertFunc) {
    if (typeof assertFunc === 'undefined')
        assertFunc = assertEq;

    assertFunc(real.x, expected[0]);
    assertFunc(real.y, expected[1]);
    assertFunc(real.z, expected[2]);
    assertFunc(real.w, expected[3]);
}

function CheckI4(header, code, expected) {
    // code needs to contain a local called x
    header = USE_ASM + I32 + CI32 + F32 + header;
    var observed = asmLink(asmCompile('glob', header + ';function f() {' + code + ';return ci4(x)} return f'), this)();
    assertEqX4(observed, expected);
}

function CheckF4(header, code, expected) {
    // code needs to contain a local called x
    header = USE_ASM + F32 + CF32 + header;
    var observed = asmLink(asmCompile('glob', header + ';function f() {' + code + ';return cf4(x)} return f'), this)();
    assertEqX4(observed, expected.map(Math.fround));
}

try {

// 1. Constructors

// 1.1 Compilation
assertAsmTypeFail('glob', USE_ASM + "var i4 = int32x4               ; return {}") ;
assertAsmTypeFail('glob', USE_ASM + "var i4 = glob.int32x4          ; return {}") ;
assertAsmTypeFail('glob', USE_ASM + "var i4 = glob.globglob.int32x4 ; return {}") ;
assertAsmTypeFail('glob', USE_ASM + "var i4 = glob.Math.int32x4     ; return {}") ;
assertAsmTypeFail('glob', USE_ASM + "var herd = glob.SIMD.ponyX4    ; return {}") ;

// 1.2 Linking
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: 42});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: Math.fround});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {int32x4: 42}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {int32x4: Math.fround}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {int32x4: new Array}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {int32x4: SIMD.float32x4}});

[Type, int32] = [TypedObject.StructType, TypedObject.int32];
var MyStruct = new Type({'x': int32, 'y': int32, 'z': int32, 'w': int32});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {int32x4: MyStruct}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {int32x4: new MyStruct}});

assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {} return f"), {SIMD:{int32x4: SIMD.int32x4}})(), undefined);

assertAsmLinkFail(asmCompile('glob', USE_ASM + F32 + "return {}"), {SIMD: {float32x4: 42}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + F32 + "return {}"), {SIMD: {float32x4: Math.fround}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + F32 + "return {}"), {SIMD: {float32x4: new Array}});
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {} return f"), {SIMD:{float32x4: SIMD.float32x4}})(), undefined);

// 1.3 Correctness
// 1.3.1 Local variables declarations
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=Int32x4(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1, 2);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1, 2, 3);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1, 2, 3, 4.0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1, 2.0, 3, 4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4a(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,2+2|0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3," + (INT32_MIN - 1) + ");} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(i4(1,2,3,4));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,4);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3," + (INT32_MAX + 1) + ");} return f"), this)(), undefined);

assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4();} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1.,2.,3.);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1.,2.,f32(3.),4.);} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {var x=f4(1.,2.,3.,4.);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3," + (INT32_MIN - 1) + ");} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3," + (INT32_MAX + 1) + ");} return f"), this)(), undefined);

// Places where NumLit can creep in
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(i) {i=i|0; var z=0; switch(i|0) {case i4(1,2,3,4): z=1; break; default: z=2; break;}} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(i) {i=i|0; var z=0; return i * i4(1,2,3,4) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(i) {var x=i4(1,2,3,i4(4,5,6,7))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "function f(i) {var x=i4(1,2,3,f4(4,5,6,7))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "function f(i) {var x=f4(1,2,3,i4(4,5,6,7))} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {return +i4(1,2,3,4)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {return i4(1,2,3,4)|0} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + FROUND + "function f() {return f32(i4(1,2,3,4))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + "function f() {return cf4(i4(1,2,3,4))} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {return +f4(1,2,3,4)} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {return f4(1,2,3,4)|0} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + FROUND + "function f() {return f32(f4(1,2,3,4))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + F32 + "function f() {return ci4(f4(1,2,3,4))} return f");

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {return i4(1,2,3,4);} return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f() {return ci4(i4(1,2,3,4));} return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {return f4(1,2,3,4);} return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f() {return cf4(f4(1,2,3,4));} return f"), this)(), [1, 2, 3, 4]);

assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {i4(1,2,3,4);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {f4(1,2,3,4);} return f"), this)(), undefined);

// Int32x4 ctor should accept int?
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + I32 + CI32 + "var i32=new glob.Int32Array(heap); function f(i) {i=i|0; return ci4(i4(i32[i>>2], 2, 3, 4))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [0, 2, 3, 4]);
// Float32x4 ctor should accept floatish (i.e. float || float? || floatish) and doublit
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + F32 + CF32 + FROUND + "var h=new glob.Float32Array(heap); function f(i) {i=i|0; return cf4(f4(h[i>>2], f32(2), f32(3), f32(4)))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [NaN, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + FROUND + "function f(i) {i=i|0; return cf4(f4(f32(1) + f32(2), f32(2), f32(3), f32(4)))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [3, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + FROUND + "function f(i) {i=i|0; return cf4(f4(f32(1) + f32(2), 2.0, 3.0, 4.0))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [3, 2, 3, 4]);

// 1.3.2 Reading values out of lanes
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=1; return x.y | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=1; return (x + x).y | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=1.; return x.y | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + "var f32=glob.Math.fround;" + I32 + "function f() {var x=f32(1); return x.y | 0;} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,4); return x.length|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,4).y; return x|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,4); return (x.x > (1>>>0)) | 0;} return f");

// signMask
function CheckSignMask(innerBody, coerceBefore, coerceAfter, expected) {
    var lanes = ['x', 'y', 'z', 'w'];
    for (var i = 0; i < lanes.length; i++) {
        var lane = lanes[i];
        var laneCheckCode = `"use asm"; var i4=glob.SIMD.int32x4; var f4=glob.SIMD.float32x4; function f() {${innerBody}; return ${coerceBefore}x.${lane}${coerceAfter} } return f;`;
        assertEq(asmLink(asmCompile('glob', laneCheckCode), this)(), expected[i]);
    }
}
function CheckSignMaskI4(innerBody, expected) { return CheckSignMask(innerBody, '', '|0', expected); }
function CheckSignMaskF4(innerBody, expected) { return CheckSignMask(innerBody, '+', '', expected.map(Math.fround)); }

CheckSignMaskI4('var x=i4(0,0,0,0);', [0,0,0,0]);
CheckSignMaskI4('var x=i4(1,2,3,4);', [1,2,3,4]);
CheckSignMaskI4('var x=i4(' + INT32_MIN + ',2,3,' + INT32_MAX + ')', [INT32_MIN,2,3,INT32_MAX]);
CheckSignMaskI4('var x=i4(1,2,3,4); var y=i4(5,6,7,8)', [1,2,3,4]);
CheckSignMaskI4('var a=1; var b=i4(9,8,7,6); var c=13.37; var x=i4(1,2,3,4); var y=i4(5,6,7,8)', [1,2,3,4]);
CheckSignMaskI4('var y=i4(5,6,7,8); var x=i4(1,2,3,4)', [1,2,3,4]);

CheckSignMaskF4('var x=f4(' + INT32_MAX + ', 2, 3, ' + INT32_MIN + ')', [INT32_MAX, 2, 3, INT32_MIN]);
CheckSignMaskF4('var x=f4(' + (INT32_MAX + 1) + ', 2, 3, 4)', [INT32_MAX + 1, 2, 3, 4]);
CheckSignMaskF4('var x=f4(1.3, 2.4, 3.5, 98.76)', [1.3, 2.4, 3.5, 98.76]);
CheckSignMaskF4('var x=f4(13.37, 2., 3., -0)', [13.37, 2, 3, -0]);

assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,4); var y=0.0; y=x.signMask;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,4); return (x.signMask > (1>>>0)) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + FROUND + "function f() {var x=i4(1,2,3,4); var y=f32(0.0); y=x.signMask;} return f");

assertAsmTypeFail('glob', USE_ASM + "function f() {var x=42; return x.signMask;} return f");
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=42.; return x.signMask;} return f");
assertAsmTypeFail('glob', USE_ASM + FROUND + "function f() {var x=f32(42.); return x.signMask;} return f");

assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + 'function f() { var x=i4(1,2,3,4); return x.signMask | 0 } return f'), this)(), 0b0000);
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + 'function f() { var x=i4(0,-1, ' + INT32_MAX + ',' + INT32_MIN  + '); return x.signMask | 0 } return f'), this)(), 0b1010);

assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + FROUND + 'var Infinity = glob.Infinity; function f() { var x=f4(0,0,0,0); x=f4(f32(1), f32(-13.37), f32(42), f32(-Infinity)); return x.signMask | 0 } return f'), this)(), 0b1010);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + FROUND + 'var Infinity = glob.Infinity; function f() { var x=f4(0,0,0,0); x=f4(f32(-1), f32(0), f32(-0.000001), f32(Infinity)); return x.signMask | 0 } return f'), this)(), 0b0101);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + FROUND + 'var NaN = glob.NaN; function f() { var x=f4(0,0,0,0); x=f4(f32(-1), f32(NaN), f32(-0), f32(0)); return x.signMask | 0 } return f'), this)(), 0b0101);

// 1.3.3. Variable assignments
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2, 3);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1.0, 2, 3, 4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2.0, 3, 4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2, 3.0, 4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2, 3, 4.0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2, 3, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); var c=4.0; x=i4(1, 2, 3, +c);} return f");

assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + I32 + "var i32=new glob.Int32Array(heap); function f() {var x=i4(1,2,3,4); i32[0] = x;} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + I32 + "var i32=new glob.Int32Array(heap); function f() {var x=i4(1,2,3,4); x = i32[0];} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + F32 + "var f32=new glob.Float32Array(heap); function f() {var x=f4(1,2,3,4); f32[0] = x;} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + F32 + "var f32=new glob.Int32Array(heap); function f() {var x=f4(1,2,3,4); x = f32[0];} return f");

CheckI4('', 'var x=i4(1,2,3,4); x=i4(5,6,7,8)', [5, 6, 7, 8]);
CheckI4('', 'var x=i4(1,2,3,4); var c=6; x=i4(5,c|0,7,8)', [5, 6, 7, 8]);
CheckI4('', 'var x=i4(8,7,6,5); x=i4(x.w|0,x.z|0,x.y|0,x.x|0)', [5, 6, 7, 8]);

assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; x=f4(1,2,3,c);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; x=f4(1.,2.,3.,c);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4.; x=f4(1,2,3,c);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4.; x=f4(1.,2.,3.,c);} return f");

CheckF4(FROUND, 'var x=f4(1,2,3,4); var y=f32(7.); x=f4(f32(5),f32(6),y,f32(8))', [5, 6, 7, 8]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); x=f4(f32(5),f32(6),f32(7),f32(8))', [5, 6, 7, 8]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); x=f4(f32(5.),f32(6.),f32(7.),f32(8.))', [5, 6, 7, 8]);
CheckF4('', 'var x=f4(1.,2.,3.,4.); x=f4(5.,6.,7.,8.)', [5, 6, 7, 8]);
CheckF4('', 'var x=f4(1.,2.,3.,4.); x=f4(1,2,3,4)', [1, 2, 3, 4]);
CheckF4(FROUND, 'var x=f4(1.,2.,3.,4.); var y=f32(7.); x=f4(9, 4, 2, 1)', [9, 4, 2, 1]);
CheckF4('', 'var x=f4(8.,7.,6.,5.); x=f4(x.w, x.z, x.y, x.x)', [5, 6, 7, 8]);

// Optimization for all lanes from the same definition.
CheckI4('', 'var x=i4(1,2,3,4); var c=6; x=i4(c|0,c|0,c|0,c|0)', [6, 6, 6, 6]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); var y=f32(7.); x=f4(y,y,y,y)', [7, 7, 7, 7]);
CheckI4('', 'var x=i4(1,2,3,4); var c=0; c=x.w|0; x=i4(c,c,c,c)', [4, 4, 4, 4]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); var y=f32(0); y=x.z; x=f4(y,y,y,y)', [3, 3, 3, 3]);
CheckI4('', 'var x=i4(1,2,3,4); var c=0; var d=0; c=x.w|0; d=x.w|0; x=i4(c,d,d,c)', [4, 4, 4, 4]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); var y=f32(0); var z=f32(0); y=x.z; z=x.z; x=f4(y,z,y,z)', [3, 3, 3, 3]);

// Uses in ternary conditionals
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; c=x?c:c;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; x=1?x:c;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; x=1?c:x;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + I32 + "function f() {var x=f4(1,2,3,4); var y=i4(1,2,3,4); x=1?x:y;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + I32 + "function f() {var x=f4(1,2,3,4); var y=i4(1,2,3,4); x=1?y:y;} return f");

CheckF4('', 'var x=f4(1,2,3,4); var y=f4(4,3,2,1); x=3?y:x', [4, 3, 2, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(x) {x=x|0; var v=f4(1,2,3,4); var w=f4(5,6,7,8); return cf4(x?w:v);} return f"), this)(1), [5,6,7,8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(v) {v=cf4(v); var w=f4(5,6,7,8); return cf4(4?w:v);} return f"), this)(SIMD.float32x4(1,2,3,4)), [5,6,7,8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(v, x) {v=cf4(v); x=x|0; var w=f4(5,6,7,8); return cf4(x?w:v);} return f"), this)(SIMD.float32x4(1,2,3,4), 0), [1,2,3,4]);

CheckI4('', 'var x=i4(1,2,3,4); var y=i4(4,3,2,1); x=(x.x|0)?y:x', [4, 3, 2, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(x) {x=x|0; var v=i4(1,2,3,4); var w=i4(5,6,7,8); return ci4(x?w:v);} return f"), this)(1), [5,6,7,8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(v) {v=ci4(v); var w=i4(5,6,7,8); return ci4(4?w:v);} return f"), this)(SIMD.int32x4(1,2,3,4)), [5,6,7,8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(v, x) {v=ci4(v); x=x|0; var w=i4(5,6,7,8); return ci4(x?w:v);} return f"), this)(SIMD.int32x4(1,2,3,4), 0), [1,2,3,4]);

// 1.3.4 Return values
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {var x=1; return ci4(x)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {var x=1; return ci4(x + x)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {var x=1.; return ci4(x)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + FROUND + "function f() {var x=f32(1.); return ci4(x)} return f");

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f() {var x=i4(1,2,3,4); return ci4(x)} return f"), this)(), [1,2,3,4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f() {var x=f4(1,2,3,4); return cf4(x)} return f"), this)(), [1,2,3,4]);

// 1.3.5 Coerce and pass arguments
// Via check
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {ci4();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); ci4(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {ci4(1);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {ci4(1.);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + FROUND + "function f() {ci4(f32(1.));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + F32 + CF32 + "function f(x) {x=cf4(x); ci4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); return 1 + ci4(x) | 0;} return f");

var i32x4 = SIMD.int32x4(1, 3, 3, 7);
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x)} return f"), this)(i32x4), undefined);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); return ci4(x);} return f"), this)(i32x4), [1,3,3,7]);

assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f() {cf4();} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); cf4(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f() {cf4(1);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f() {cf4(1.);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + FROUND + "function f() {cf4(f32(1.));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + F32 + CF32 + "function f(x) {x=cf4(x); cf4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); return 1 + cf4(x) | 0;} return f");

var f32x4 = SIMD.float32x4(13.37, 42.42, -0, NaN);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x)} return f"), this)(f32x4), undefined);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); return cf4(x);} return f"), this)(f32x4), [13.37, 42.42, -0, NaN].map(Math.fround));

// Legacy coercions
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {x=i4();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {x=i4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {x=i4(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x,y) {x=i4(y);y=+y} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); return i4(x);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {return +i4(1,2,3,4)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {return 0|i4(1,2,3,4)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + FROUND + "function f(x) {return f32(i4(1,2,3,4))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "function f(x) {return f4(i4(1,2,3,4))} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); return f4(x);} return f");


function assertCaught(f) {
    var caught = false;
    try {
        f.apply(null, Array.prototype.slice.call(arguments, 1));
    } catch (e) {
        DEBUG && print('Assert caught: ', e, '\n', e.stack);
        assertEq(e instanceof TypeError, true);
        caught = true;
    }
    assertEq(caught, true);
}

var f = asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); return cf4(x);} return f"), this);
assertCaught(f);
assertCaught(f, 1);
assertCaught(f, {});
assertCaught(f, "I sincerely am a SIMD typed object.");
assertCaught(f, SIMD.int32x4(1,2,3,4));

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); return ci4(x);} return f"), this);
assertCaught(f);
assertCaught(f, 1);
assertCaught(f, {});
assertCaught(f, "I sincerely am a SIMD typed object.");
assertCaught(f, SIMD.float32x4(4,3,2,1));

// 1.3.6 Globals
// 1.3.6.1 Local globals
// Read
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); function f() {var x=4; x=g|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); function f() {var x=4.; x=+g;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); var f32=glob.Math.fround; function f() {var x=f32(4.); x=f32(g);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); function f() {var x=4; x=g|0;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); function f() {var x=4.; x=+g;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); var f32=glob.Math.fround; function f() {var x=f32(4.); x=f32(g);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + I32 + CI32 + "var g=f4(1., 2., 3., 4.); function f() {var x=i4(1,2,3,4); x=ci4(g);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + "var g=i4(1,2,3,4); function f() {var x=f4(1.,2.,3.,4.); x=cf4(g);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "var g=0; function f() {var x=i4(1,2,3,4); x=g|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=0.; function f() {var x=i4(1,2,3,4); x=+g;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var f32=glob.Math.fround; var g=f32(0.); function f() {var x=i4(1,2,3,4); x=f32(g);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + "var g=0; function f() {var x=f4(0.,0.,0.,0.); x=g|0;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=0.; function f() {var x=f4(0.,0.,0.,0.); x=+g;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var f32=glob.Math.fround; var g=f32(0.); function f() {var x=f4(0.,0.,0.,0.); x=f32(g);} return f");

CheckI4('var x=i4(1,2,3,4)', '', [1, 2, 3, 4]);
CheckI4('var _=42; var h=i4(5,5,5,5); var __=13.37; var x=i4(4,7,9,2);', '', [4,7,9,2]);

CheckF4('var x=f4(1.,2.,3.,4.)', '', [1, 2, 3, 4]);
CheckF4('var _=42; var h=f4(5.,5.,5.,5.); var __=13.37; var x=f4(4.,13.37,9.,-0.);', '', [4, 13.37, 9, -0]);
CheckF4('var x=f4(1,2,3,4)', '', [1, 2, 3, 4]);

// Write
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); function f() {var x=4; g=x|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); function f() {var x=4.; g=+x;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); var f32=glob.Math.fround; function f() {var x=f32(4.); g=f32(x);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); function f() {var x=4; g=x|0;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); function f() {var x=4.; g=+x;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); var f32=glob.Math.fround; function f() {var x=f32(4.); g=f32(x);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + I32 + CI32 + "var g=f4(1., 2., 3., 4.); function f() {var x=i4(1,2,3,4); g=ci4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + I32 + CF32 + "var g=f4(1., 2., 3., 4.); function f() {var x=i4(1,2,3,4); g=cf4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + "var g=i4(1,2,3,4); function f() {var x=f4(1.,2.,3.,4.); g=cf4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + "var g=i4(1,2,3,4); function f() {var x=f4(1.,2.,3.,4.); g=ci4(x);} return f");

CheckI4('var x=i4(0,0,0,0);', 'x=i4(1,2,3,4)', [1,2,3,4]);
CheckF4('var x=f4(0.,0.,0.,0.);', 'x=f4(5.,3.,4.,2.)', [5,3,4,2]);

CheckI4('var x=i4(0,0,0,0); var y=42; var z=3.9; var w=13.37', 'x=i4(1,2,3,4); y=24; z=4.9; w=23.10;', [1,2,3,4]);
CheckF4('var x=f4(0,0,0,0); var y=42; var z=3.9; var w=13.37', 'x=f4(1,2,3,4); y=24; z=4.9; w=23.10;', [1,2,3,4]);

// 1.3.6.2 Imported globals
// Read
var int32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + I32 + CI32 + "var g=ci4(ffi.g); function f() {return ci4(g)} return f"), this, {g: SIMD.int32x4(1,2,3,4)})();
assertEq(int32x4.x, 1);
assertEq(int32x4.y, 2);
assertEq(int32x4.z, 3);
assertEq(int32x4.w, 4);

for (var v of [1, {}, "totally legit SIMD variable", SIMD.float32x4(1,2,3,4)])
    assertCaught(asmCompile('glob', 'ffi', USE_ASM + I32 + CI32 + "var g=ci4(ffi.g); function f() {return ci4(g)} return f"), this, {g: v});

var float32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + F32 + CF32 + "var g=cf4(ffi.g); function f() {return cf4(g)} return f"), this, {g: SIMD.float32x4(1,2,3,4)})();
assertEq(float32x4.x, 1);
assertEq(float32x4.y, 2);
assertEq(float32x4.z, 3);
assertEq(float32x4.w, 4);

for (var v of [1, {}, "totally legit SIMD variable", SIMD.int32x4(1,2,3,4)])
    assertCaught(asmCompile('glob', 'ffi', USE_ASM + F32 + CF32 + "var g=cf4(ffi.g); function f() {return cf4(g)} return f"), this, {g: v});

// Write
var int32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + I32 + CI32 + "var g=ci4(ffi.g); function f() {g=i4(4,5,6,7); return ci4(g)} return f"), this, {g: SIMD.int32x4(1,2,3,4)})();
assertEq(int32x4.x, 4);
assertEq(int32x4.y, 5);
assertEq(int32x4.z, 6);
assertEq(int32x4.w, 7);

var float32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + F32 + CF32 + "var g=cf4(ffi.g); function f() {g=f4(4.,5.,6.,7.); return cf4(g)} return f"), this, {g: SIMD.float32x4(1,2,3,4)})();
assertEq(float32x4.x, 4);
assertEq(float32x4.y, 5);
assertEq(float32x4.z, 6);
assertEq(float32x4.w, 7);

// 2. SIMD operations
// 2.1 Compilation
assertAsmTypeFail('glob', USE_ASM + "var add = int32x4.add; return {}");
assertAsmTypeFail('glob', USE_ASM + I32A + I32 + "return {}");
assertAsmTypeFail('glob', USE_ASM + "var g = 3; var add = g.add; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + "var func = i4.doTheHarlemShake; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + "var div = i4.div; return {}");
assertAsmTypeFail('glob', USE_ASM + "var f32 = glob.Math.fround; var i4a = f32.add; return {}");

// 2.2 Linking
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + I32 + I32A + "function f() {} return f"), {});
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + I32 + I32A + "function f() {} return f"), {SIMD: Math.fround});

var oldInt32x4Add = SIMD.int32x4.add;
var code = asmCompile('glob', USE_ASM + I32 + I32A + "return {}");
for (var v of [42, Math.fround, SIMD.float32x4.add, function(){}, SIMD.int32x4.mul]) {
    SIMD.int32x4.add = v;
    assertAsmLinkFail(code, {SIMD: {int32x4: SIMD.int32x4}});
}
SIMD.int32x4.add = oldInt32x4Add; // finally replace the add function with the original one
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + I32A + "function f() {} return f"), {SIMD: {int32x4: SIMD.int32x4}})(), undefined);

// 2.3. Binary arithmetic operations
// 2.3.1 Additions
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(x, x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(13, 37);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(23.10, 19.89);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(x, 42);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(x, 13.37);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); var y=4; x=i4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(0,0,0,0); var y=4; x=i4a(y, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(0,0,0,0); var y=4; y=i4a(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + I32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); x=i4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + I32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); y=i4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + I32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); y=i4a(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + F32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); y=f4a(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + F32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); y=f4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + F32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); x=f4a(y, y);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + I32A + 'function f() {var x=i4(1,2,3,4); var y=0; y=i4a(x,x)|0} return f');
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + 'function f() {var x=i4(1,2,3,4); var y=0.; y=+i4a(x,x)} return f');

CheckI4(I32A, 'var z=i4(1,2,3,4); var y=i4(0,1,0,3); var x=i4(0,0,0,0); x=i4a(z,y)', [1,3,3,7]);
CheckI4(I32A, 'var x=i4(2,3,4,5); var y=i4(0,1,0,3); x=i4a(x,y)', [2,4,4,8]);
CheckI4(I32A, 'var x=i4(1,2,3,4); x=i4a(x,x)', [2,4,6,8]);
CheckI4(I32A, 'var x=i4(' + INT32_MAX + ',2,3,4); var y=i4(1,1,0,3); x=i4a(x,y)', [INT32_MIN,3,3,7]);
CheckI4(I32A, 'var x=i4(' + INT32_MAX + ',2,3,4); var y=i4(1,1,0,3); x=ci4(i4a(x,y))', [INT32_MIN,3,3,7]);

CheckF4(F32A, 'var x=f4(1,2,3,4); x=f4a(x,x)', [2,4,6,8]);
CheckF4(F32A, 'var x=f4(1,2,3,4); var y=f4(4,3,5,2); x=f4a(x,y)', [5,5,8,6]);
CheckF4(F32A, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=f4a(x,y)', [Math.fround(13.37) + 4,5,8,6]);
CheckF4(F32A, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=cf4(f4a(x,y))', [Math.fround(13.37) + 4,5,8,6]);

// 2.3.2. Subtracts
CheckI4(I32S, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=i4s(x,y)', [2,1,3,2]);
CheckI4(I32S, 'var x=i4(5,4,3,2); var y=i4(1,2,3,4); x=i4s(x,y)', [4,2,0,-2]);
CheckI4(I32S, 'var x=i4(1,2,3,4); x=i4s(x,x)', [0,0,0,0]);
CheckI4(I32S, 'var x=i4(' + INT32_MIN + ',2,3,4); var y=i4(1,1,0,3); x=i4s(x,y)', [INT32_MAX,1,3,1]);
CheckI4(I32S, 'var x=i4(' + INT32_MIN + ',2,3,4); var y=i4(1,1,0,3); x=ci4(i4s(x,y))', [INT32_MAX,1,3,1]);

CheckF4(F32S, 'var x=f4(1,2,3,4); x=f4s(x,x)', [0,0,0,0]);
CheckF4(F32S, 'var x=f4(1,2,3,4); var y=f4(4,3,5,2); x=f4s(x,y)', [-3,-1,-2,2]);
CheckF4(F32S, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=f4s(x,y)', [Math.fround(13.37) - 4,-1,-2,2]);
CheckF4(F32S, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=cf4(f4s(x,y))', [Math.fround(13.37) - 4,-1,-2,2]);

// 2.3.3. Multiplications / Divisions
assertAsmTypeFail('glob', USE_ASM + I32 + "var f4d=i4.div; function f() {} return f");

CheckI4(I32M, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=i4m(x,y)', [-1,2,0,8]);
CheckI4(I32M, 'var x=i4(5,4,3,2); var y=i4(1,2,3,4); x=i4m(x,y)', [5,8,9,8]);
CheckI4(I32M, 'var x=i4(1,2,3,4); x=i4m(x,x)', [1,4,9,16]);
(function() {
    var m = INT32_MIN, M = INT32_MAX, imul = Math.imul;
    CheckI4(I32M, `var x=i4(${m},${m}, ${M}, ${M}); var y=i4(2,-3,4,-5); x=i4m(x,y)`,
            [imul(m, 2), imul(m, -3), imul(M, 4), imul(M, -5)]);
    CheckI4(I32M, `var x=i4(${m},${m}, ${M}, ${M}); var y=i4(${m}, ${M}, ${m}, ${M}); x=i4m(x,y)`,
            [imul(m, m), imul(m, M), imul(M, m), imul(M, M)]);
})();

CheckF4(F32M, 'var x=f4(1,2,3,4); x=f4m(x,x)', [1,4,9,16]);
CheckF4(F32M, 'var x=f4(1,2,3,4); var y=f4(4,3,5,2); x=f4m(x,y)', [4,6,15,8]);
CheckF4(F32M, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=f4m(x,y)', [Math.fround(13.37) * 4,6,15,8]);
CheckF4(F32M, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=cf4(f4m(x,y))', [Math.fround(13.37) * 4,6,15,8]);

var f32x4 = SIMD.float32x4(0, NaN, -0, NaN);
var another = SIMD.float32x4(NaN, -1, -0, NaN);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + F32M + CF32 + "function f(x, y) {x=cf4(x); y=cf4(y); x=f4m(x,y); return cf4(x);} return f"), this)(f32x4, another), [NaN, NaN, 0, NaN]);

CheckF4(F32D, 'var x=f4(1,2,3,4); x=f4d(x,x)', [1,1,1,1]);
CheckF4(F32D, 'var x=f4(1,2,3,4); var y=f4(4,3,5,2); x=f4d(x,y)', [1/4,2/3,3/5,2]);
CheckF4(F32D, 'var x=f4(13.37,1,1,4); var y=f4(4,0,-0.,2); x=f4d(x,y)', [Math.fround(13.37) / 4,+Infinity,-Infinity,2]);

var f32x4 = SIMD.float32x4(0, 0, -0, NaN);
var another = SIMD.float32x4(0, -0, 0, 0);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + F32D + CF32 + "function f(x,y) {x=cf4(x); y=cf4(y); x=f4d(x,y); return cf4(x);} return f"), this)(f32x4, another), [NaN, NaN, NaN, NaN]);

// Unary arithmetic operators
function CheckUnaryF4(op, checkFunc, assertFunc) {
    var _ = asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + 'var op=f4.' + op + '; function f(x){x=cf4(x); return cf4(op(x)); } return f'), this);
    return function(input) {
        var simd = SIMD.float32x4(input[0], input[1], input[2], input[3]);

        var exp = input.map(Math.fround).map(checkFunc).map(Math.fround);
        var obs = _(simd);
        assertEqX4(obs, exp, assertFunc);
    }
}

function CheckUnaryI4(op, checkFunc) {
    var _ = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + 'var op=i4.' + op + '; function f(x){x=ci4(x); return ci4(op(x)); } return f'), this);
    return function(input) {
        var simd = SIMD.int32x4(input[0], input[1], input[2], input[3]);
        assertEqX4(_(simd), input.map(checkFunc).map(function(x) { return x | 0}));
    }
}

CheckUnaryI4('neg', function(x) { return -x })([1, -2, INT32_MIN, INT32_MAX]);
CheckUnaryI4('not', function(x) { return ~x })([1, -2, INT32_MIN, INT32_MAX]);

var CheckAbs = CheckUnaryF4('abs', Math.abs);
CheckAbs([1, 42.42, 0.63, 13.37]);
CheckAbs([NaN, -Infinity, Infinity, 0]);

var CheckNegF = CheckUnaryF4('neg', function(x) { return -x });
CheckNegF([1, 42.42, 0.63, 13.37]);
CheckNegF([NaN, -Infinity, Infinity, 0]);

var CheckNotF = CheckUnaryF4('not', (function() {
    var f32 = new Float32Array(1);
    var i32 = new Int32Array(f32.buffer);
    return function(x) {
        f32[0] = x;
        i32[0] = ~i32[0];
        return f32[0];
    }
})());
CheckNotF([1, 42.42, 0.63, 13.37]);
CheckNotF([NaN, -Infinity, Infinity, 0]);

var CheckSqrt = CheckUnaryF4('sqrt', function(x) { return Math.sqrt(x); });
CheckSqrt([1, 42.42, 0.63, 13.37]);
CheckSqrt([NaN, -Infinity, Infinity, 0]);

// ReciprocalApproximation and reciprocalSqrtApproximation give approximate results
function assertNear(a, b) {
    if (a !== a && b === b)
        throw 'Observed NaN, expected ' + b;
    if (Math.abs(a - b) > 1e-3)
        throw 'More than 1e-3 between ' + a + ' and ' + b;
}
var CheckRecp = CheckUnaryF4('reciprocalApproximation', function(x) { return 1 / x; }, assertNear);
CheckRecp([1, 42.42, 0.63, 13.37]);
CheckRecp([NaN, -Infinity, Infinity, 0]);

var CheckRecp = CheckUnaryF4('reciprocalSqrtApproximation', function(x) { return 1 / Math.sqrt(x); }, assertNear);
CheckRecp([1, 42.42, 0.63, 13.37]);
CheckRecp([NaN, -Infinity, Infinity, 0]);

// Min/Max
assertAsmTypeFail('glob', USE_ASM + I32 + "var f4m=i4.min; function f() {} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var f4d=i4.max; function f() {} return f");

const F32MIN = 'var min = f4.min;'
const F32MAX = 'var max = f4.max;'

CheckF4(F32MIN, 'var x=f4(1,2,3,4); x=min(x,x)', [1,2,3,4]);
CheckF4(F32MIN, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=min(x,y)', [4,2,3,2]);
CheckF4(F32MIN + FROUND + 'var Infinity = glob.Infinity;', 'var x=f4(0,0,0,0); var y=f4(2310,3,5,0); x=f4(f32(+Infinity),f32(-Infinity),f32(3),f32(-0.)); x=min(x,y)', [2310,-Infinity,3,-0]);

CheckF4(F32MIN, 'var x=f4(0,0,-0,-0); var y=f4(0,-0,0,-0); x=min(x,y)', [0,-0,-0,-0]);
CheckF4(F32MIN + FROUND + 'var NaN = glob.NaN;', 'var x=f4(0,0,0,0); var y=f4(0,0,0,0); var n=f32(0); n=f32(NaN); x=f4(n,0.,n,0.); y=f4(n,n,0.,0.); x=min(x,y)', [NaN, NaN, NaN, 0]);

CheckF4(F32MAX, 'var x=f4(1,2,3,4); x=max(x,x)', [1,2,3,4]);
CheckF4(F32MAX, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=max(x,y)', [13.37, 3, 5, 4]);
CheckF4(F32MAX + FROUND + 'var Infinity = glob.Infinity;', 'var x=f4(0,0,0,0); var y=f4(2310,3,5,0); x=f4(f32(+Infinity),f32(-Infinity),f32(3),f32(-0.)); x=max(x,y)', [+Infinity,3,5,0]);

CheckF4(F32MAX, 'var x=f4(0,0,-0,-0); var y=f4(0,-0,0,-0); x=max(x,y)', [0,0,0,-0]);
CheckF4(F32MAX + FROUND + 'var NaN = glob.NaN;', 'var x=f4(0,0,0,0); var y=f4(0,0,0,0); var n=f32(0); n=f32(NaN); x=f4(n,0.,n,0.); y=f4(n,n,0.,0.); x=max(x,y)', [NaN, NaN, NaN, 0]);

const F32MINNUM = 'var min = f4.minNum;'
const F32MAXNUM = 'var max = f4.maxNum;'

CheckF4(F32MINNUM, 'var x=f4(1,2,3,4); x=min(x,x)', [1,2,3,4]);
CheckF4(F32MINNUM, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=min(x,y)', [4,2,3,2]);
CheckF4(F32MINNUM + FROUND + 'var Infinity = glob.Infinity;', 'var x=f4(0,0,0,0); var y=f4(2310,3,5,0); x=f4(f32(+Infinity),f32(-Infinity),f32(3),f32(-0.)); x=min(x,y)', [2310,-Infinity,3,-0]);

CheckF4(F32MINNUM, 'var x=f4(0,0,-0,-0); var y=f4(0,-0,0,-0); x=min(x,y)', [0,-0,-0,-0]);
CheckF4(F32MINNUM + FROUND + 'var NaN = glob.NaN;', 'var x=f4(0,0,0,0); var y=f4(0,0,0,0); var n=f32(0); n=f32(NaN); x=f4(n,0.,n,0.); y=f4(n,n,0.,0.); x=min(x,y)', [NaN, 0, 0, 0]);

CheckF4(F32MAXNUM, 'var x=f4(1,2,3,4); x=max(x,x)', [1,2,3,4]);
CheckF4(F32MAXNUM, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=max(x,y)', [13.37, 3, 5, 4]);
CheckF4(F32MAXNUM + FROUND + 'var Infinity = glob.Infinity;', 'var x=f4(0,0,0,0); var y=f4(2310,3,5,0); x=f4(f32(+Infinity),f32(-Infinity),f32(3),f32(-0.)); x=max(x,y)', [+Infinity,3,5,0]);

CheckF4(F32MAXNUM, 'var x=f4(0,0,-0,-0); var y=f4(0,-0,0,-0); x=max(x,y)', [0,0,0,-0]);
CheckF4(F32MAXNUM + FROUND + 'var NaN = glob.NaN;', 'var x=f4(0,0,0,0); var y=f4(0,0,0,0); var n=f32(0); n=f32(NaN); x=f4(n,0.,n,0.); y=f4(n,n,0.,0.); x=max(x,y)', [NaN, 0, 0, 0]);

// With
const RLF = 'var r = f4.replaceLane;';

assertAsmTypeFail('glob', USE_ASM + F32 + RLF + "function f() {var x = f4(1,2,3,4); x = r(x, 0, 1);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + "function f() {var x = f4(1,2,3,4); x = r(x, 0, x);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(x, 4, f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(x, f32(0), f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(1, 0, f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(1, 0., f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(f32(1), 0, f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); var l = 0; x = r(x, l, f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); var y = i4(1,2,3,4); x = r(y, 0, f32(1));} return f");

CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 0, f32(13.37));', [Math.fround(13.37), 2, 3, 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 1, f32(13.37));', [1, Math.fround(13.37), 3, 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 2, f32(13.37));', [1, 2, Math.fround(13.37), 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 3, f32(13.37));', [1, 2, 3, Math.fround(13.37)]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 3, f32(13.37) + f32(6.63));', [1, 2, 3, Math.fround(Math.fround(13.37) + Math.fround(6.63))]);

CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 0, 13.37);', [Math.fround(13.37), 2, 3, 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 1, 13.37);', [1, Math.fround(13.37), 3, 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 2, 13.37);', [1, 2, Math.fround(13.37), 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 3, 13.37);', [1, 2, 3, Math.fround(13.37)]);

const RLI = 'var r = i4.replaceLane;';
CheckI4(RLI, 'var x = i4(1,2,3,4); x = r(x, 0, 42);', [42, 2, 3, 4]);
CheckI4(RLI, 'var x = i4(1,2,3,4); x = r(x, 1, 42);', [1, 42, 3, 4]);
CheckI4(RLI, 'var x = i4(1,2,3,4); x = r(x, 2, 42);', [1, 2, 42, 4]);
CheckI4(RLI, 'var x = i4(1,2,3,4); x = r(x, 3, 42);', [1, 2, 3, 42]);

// Comparisons
// True yields all bits set to 1 (i.e as an int32, 0xFFFFFFFF === -1), false
// yields all bits set to 0 (i.e 0).
const T = -1;
const F = 0;

const EQI32 = 'var eq = i4.equal';
const NEI32 = 'var ne = i4.notEqual';
const LTI32 = 'var lt = i4.lessThan;';
const LEI32 = 'var le = i4.lessThanOrEqual';
const GTI32 = 'var gt = i4.greaterThan;';
const GEI32 = 'var ge = i4.greaterThanOrEqual';

CheckI4(EQI32, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=eq(x,y)', [F, F, F, F]);
CheckI4(EQI32, 'var x=i4(-1,1,0,2); var y=i4(1,2,3,4); x=eq(x,y)', [F, F, F, F]);
CheckI4(EQI32, 'var x=i4(1,0,3,4); var y=i4(1,1,7,0); x=eq(x,y)', [T, F, F, F]);

CheckI4(NEI32, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=ne(x,y)', [T, T, T, T]);
CheckI4(NEI32, 'var x=i4(-1,1,0,2); var y=i4(1,2,3,4); x=ne(x,y)', [T, T, T, T]);
CheckI4(NEI32, 'var x=i4(1,0,3,4); var y=i4(1,1,7,0); x=ne(x,y)', [F, T, T, T]);

CheckI4(LTI32, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=lt(x,y)', [F, F, F, F]);
CheckI4(LTI32, 'var x=i4(-1,1,0,2); var y=i4(1,2,3,4); x=lt(x,y)', [T, T, T, T]);
CheckI4(LTI32, 'var x=i4(1,0,3,4); var y=i4(1,1,7,0); x=lt(x,y)', [F, T, T, F]);

CheckI4(LEI32, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=le(x,y)', [F, F, F, F]);
CheckI4(LEI32, 'var x=i4(-1,1,0,2); var y=i4(1,2,3,4); x=le(x,y)', [T, T, T, T]);
CheckI4(LEI32, 'var x=i4(1,0,3,4); var y=i4(1,1,7,0); x=le(x,y)', [T, T, T, F]);

CheckI4(GTI32, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=gt(x,y)', [T, T, T, T]);
CheckI4(GTI32, 'var x=i4(-1,1,0,2); var y=i4(1,2,3,4); x=gt(x,y)', [F, F, F, F]);
CheckI4(GTI32, 'var x=i4(1,0,3,4); var y=i4(1,1,7,0); x=gt(x,y)', [F, F, F, T]);

CheckI4(GEI32, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=ge(x,y)', [T, T, T, T]);
CheckI4(GEI32, 'var x=i4(-1,1,0,2); var y=i4(1,2,3,4); x=ge(x,y)', [F, F, F, F]);
CheckI4(GEI32, 'var x=i4(1,0,3,4); var y=i4(1,1,7,0); x=ge(x,y)', [T, F, F, T]);

const LTF32 = 'var lt=f4.lessThan;';
const LEF32 = 'var le=f4.lessThanOrEqual;';
const GTF32 = 'var gt=f4.greaterThan;';
const GEF32 = 'var ge=f4.greaterThanOrEqual;';
const EQF32 = 'var eq=f4.equal;';
const NEF32 = 'var ne=f4.notEqual;';

assertAsmTypeFail('glob', USE_ASM + F32 + "var lt=f4.lessThan; function f() {var x=f4(1,2,3,4); var y=f4(5,6,7,8); x=lt(x,y);} return f");

CheckI4(LTF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=i4(0,0,0,0); x=lt(y,z)', [F, F, F, F]);
CheckI4(LTF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=i4(0,0,0,0); x=lt(y,z)', [T, T, T, T]);
CheckI4(LTF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=i4(0,0,0,0); x=lt(y,z)', [F, T, T, F]);
CheckI4(LTF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=i4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=lt(y,z);', [F, F, F, F]);

CheckI4(LEF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=i4(0,0,0,0); x=le(y,z)', [F, F, F, F]);
CheckI4(LEF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=i4(0,0,0,0); x=le(y,z)', [T, T, T, T]);
CheckI4(LEF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=i4(0,0,0,0); x=le(y,z)', [T, T, T, F]);
CheckI4(LEF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=i4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=le(y,z);', [T, T, F, F]);

CheckI4(EQF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=i4(0,0,0,0); x=eq(y,z)', [F, F, F, F]);
CheckI4(EQF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=i4(0,0,0,0); x=eq(y,z)', [F, F, F, F]);
CheckI4(EQF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=i4(0,0,0,0); x=eq(y,z)', [T, F, F, F]);
CheckI4(EQF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=i4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=eq(y,z);', [T, T, F, F]);

CheckI4(NEF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=i4(0,0,0,0); x=ne(y,z)', [T, T, T, T]);
CheckI4(NEF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=i4(0,0,0,0); x=ne(y,z)', [T, T, T, T]);
CheckI4(NEF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=i4(0,0,0,0); x=ne(y,z)', [F, T, T, T]);
CheckI4(NEF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=i4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=ne(y,z);', [F, F, T, T]);

CheckI4(GTF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=i4(0,0,0,0); x=gt(y,z)', [T, T, T, T]);
CheckI4(GTF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=i4(0,0,0,0); x=gt(y,z)', [F, F, F, F]);
CheckI4(GTF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=i4(0,0,0,0); x=gt(y,z)', [F, F, F, T]);
CheckI4(GTF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=i4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=gt(y,z);', [F, F, F, F]);

CheckI4(GEF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=i4(0,0,0,0); x=ge(y,z)', [T, T, T, T]);
CheckI4(GEF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=i4(0,0,0,0); x=ge(y,z)', [F, F, F, F]);
CheckI4(GEF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=i4(0,0,0,0); x=ge(y,z)', [T, F, F, T]);
CheckI4(GEF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=i4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=ge(y,z);', [T, T, F, F]);

// Conversions operators
const CVTIF = 'var cvt=f4.fromInt32x4;';
const CVTFI = 'var cvt=i4.fromFloat32x4;';

assertAsmTypeFail('glob', USE_ASM + I32 + "var cvt=i4.fromInt32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + F32 + "var cvt=f4.fromFloat32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CVTIF + "function f() {var x=i4(1,2,3,4); x=cvt(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CVTIF + "function f() {var x=f4(1,2,3,4); x=cvt(x);} return f");

var f = asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + CI32 + CVTIF + 'function f(x){x=ci4(x); var y=f4(0,0,0,0); y=cvt(x); return cf4(y);} return f'), this);
assertEqX4(f(SIMD.int32x4(1,2,3,4)), [1, 2, 3, 4]);
assertEqX4(f(SIMD.int32x4(0,INT32_MIN,INT32_MAX,-1)), [0, Math.fround(INT32_MIN), Math.fround(INT32_MAX), -1]);

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + F32 + CF32 + CVTFI + 'function f(x){x=cf4(x); var y=i4(0,0,0,0); y=cvt(x); return ci4(y);} return f'), this);
assertEqX4(f(SIMD.float32x4(1,2,3,4)), [1, 2, 3, 4]);
// Test that INT32_MIN (exactly representable as an float32) and the first
// integer representable as an float32 can be converted.
assertEqX4(f(SIMD.float32x4(INT32_MIN, INT32_MAX - 64, -0, 0)), [INT32_MIN, INT32_MAX - 64, 0, 0].map(Math.fround));
// Test boundaries: first integer less than INT32_MIN and representable as a float32
assertThrowsInstanceOf(() => f(SIMD.float32x4(INT32_MIN - 129, 0, 0, 0)), RangeError);
// INT_MAX + 1
assertThrowsInstanceOf(() => f(SIMD.float32x4(Math.pow(2, 31), 0, 0, 0)), RangeError);
// Special values
assertThrowsInstanceOf(() => f(SIMD.float32x4(NaN, 0, 0, 0)), RangeError);
assertThrowsInstanceOf(() => f(SIMD.float32x4(Infinity, 0, 0, 0)), RangeError);
assertThrowsInstanceOf(() => f(SIMD.float32x4(-Infinity, 0, 0, 0)), RangeError);

// Cast operators
const CVTIFB = 'var cvt=f4.fromInt32x4Bits;';
const CVTFIB = 'var cvt=i4.fromFloat32x4Bits;';

var cast = (function() {
    var i32 = new Int32Array(1);
    var f32 = new Float32Array(i32.buffer);

    function fromInt32Bits(x) {
        i32[0] = x;
        return f32[0];
    }

    function fromFloat32Bits(x) {
        f32[0] = x;
        return i32[0];
    }

    return {
        fromInt32Bits,
        fromFloat32Bits
    }
})();

assertAsmTypeFail('glob', USE_ASM + I32 + "var cvt=i4.fromInt32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + F32 + "var cvt=f4.fromFloat32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CVTIFB + "function f() {var x=i4(1,2,3,4); x=cvt(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CVTIFB + "function f() {var x=f4(1,2,3,4); x=cvt(x);} return f");

var f = asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CVTIFB + CF32 + CI32 + 'function f(x){x=ci4(x); var y=f4(0,0,0,0); y=cvt(x); return cf4(y);} return f'), this);
assertEqX4(f(SIMD.int32x4(1,2,3,4)), [1, 2, 3, 4].map(cast.fromInt32Bits));
assertEqX4(f(SIMD.int32x4(0,INT32_MIN,INT32_MAX,-1)), [0, INT32_MIN, INT32_MAX, -1].map(cast.fromInt32Bits));

var f = asmLink(asmCompile('glob', USE_ASM + I32 + F32 + F32A + CVTIFB + CF32 + CI32 + 'function f(x){x=ci4(x); var y=f4(0,0,0,0); var z=f4(1,1,1,1); y=cvt(x); y=f4a(y, z); return cf4(y)} return f'), this);
assertEqX4(f(SIMD.int32x4(1,2,3,4)), [1, 2, 3, 4].map(cast.fromInt32Bits).map((x) => x+1));
assertEqX4(f(SIMD.int32x4(0,INT32_MIN,INT32_MAX,-1)), [0, INT32_MIN, INT32_MAX, -1].map(cast.fromInt32Bits).map((x) => x+1));

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + F32 + CF32 + CVTFIB + 'function f(x){x=cf4(x); var y=i4(0,0,0,0); y=cvt(x); return ci4(y);} return f'), this);
assertEqX4(f(SIMD.float32x4(1,2,3,4)), [1, 2, 3, 4].map(cast.fromFloat32Bits));
assertEqX4(f(SIMD.float32x4(-0,NaN,+Infinity,-Infinity)), [-0, NaN, +Infinity, -Infinity].map(cast.fromFloat32Bits));

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + F32 + CF32 + I32A + CVTFIB + 'function f(x){x=cf4(x); var y=i4(0,0,0,0); var z=i4(1,1,1,1); y=cvt(x); y=i4a(y,z); return ci4(y);} return f'), this);
assertEqX4(f(SIMD.float32x4(1,2,3,4)), [1, 2, 3, 4].map(cast.fromFloat32Bits).map((x) => x+1));
assertEqX4(f(SIMD.float32x4(-0,NaN,+Infinity,-Infinity)), [-0, NaN, +Infinity, -Infinity].map(cast.fromFloat32Bits).map((x) => x+1));

// Bitwise ops
const ANDI32 = 'var andd=i4.and;';
const ORI32 = 'var orr=i4.or;';
const XORI32 = 'var xorr=i4.xor;';

CheckI4(ANDI32, 'var x=i4(42,1337,-1,13); var y=i4(2, 4, 7, 15); x=andd(x,y)', [42 & 2, 1337 & 4, -1 & 7, 13 & 15]);
CheckI4(ORI32, ' var x=i4(42,1337,-1,13); var y=i4(2, 4, 7, 15); x=orr(x,y)',  [42 | 2, 1337 | 4, -1 | 7, 13 | 15]);
CheckI4(XORI32, 'var x=i4(42,1337,-1,13); var y=i4(2, 4, 7, 15); x=xorr(x,y)', [42 ^ 2, 1337 ^ 4, -1 ^ 7, 13 ^ 15]);

const ANDF32 = 'var andd=f4.and;';
const ORF32 = 'var orr=f4.or;';
const XORF32 = 'var xorr=f4.xor;';

var bitwise = (function() {
    var asf32 = new Float32Array(2);
    var asi32 = new Int32Array(asf32.buffer);
    function andd(x, y) { asf32[0] = x; asf32[1] = y; asi32[0] = asi32[0] & asi32[1]; return asf32[0]; }
    function orr(x, y)  { asf32[0] = x; asf32[1] = y; asi32[0] = asi32[0] | asi32[1]; return asf32[0]; }
    function xorr(x, y) { asf32[0] = x; asf32[1] = y; asi32[0] = asi32[0] ^ asi32[1]; return asf32[0]; }

    function andx4(x, y) { var res = []; for (var i = 0; i < 4; i++) res[i] = andd(x[i], y[i]); return res; }
    function orx4(x, y) { var res = []; for (var i = 0; i < 4; i++) res[i] = orr(x[i], y[i]); return res; }
    function xorx4(x, y) { var res = []; for (var i = 0; i < 4; i++) res[i] = xorr(x[i], y[i]); return res; }
    return {
        and: andx4,
        or: orx4,
        xor: xorx4
    }
})();

CheckF4(ANDF32, 'var x=f4(42, 13.37,-1.42, 23.10); var y=f4(19.89, 2.4, 8.15, 16.36); x=andd(x,y)', bitwise.and([42, 13.37, -1.42, 23.10], [19.89, 2.4, 8.15, 16.36]));
CheckF4(ORF32,  'var x=f4(42, 13.37,-1.42, 23.10); var y=f4(19.89, 2.4, 8.15, 16.36); x=orr(x,y)',  bitwise.or( [42, 13.37, -1.42, 23.10], [19.89, 2.4, 8.15, 16.36]));
CheckF4(XORF32, 'var x=f4(42, 13.37,-1.42, 23.10); var y=f4(19.89, 2.4, 8.15, 16.36); x=xorr(x,y)', bitwise.xor([42, 13.37, -1.42, 23.10], [19.89, 2.4, 8.15, 16.36]));

// Logical ops
const LSHI = 'var lsh=i4.shiftLeftByScalar;'
const RSHI = 'var rsh=i4.shiftRightArithmeticByScalar;'
const URSHI = 'var ursh=i4.shiftRightLogicalByScalar;'

assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + F32 + FROUND + LSHI + "function f() {var x=f4(1,2,3,4); return ci4(lsh(x,f32(42)));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + F32 + FROUND + LSHI + "function f() {var x=f4(1,2,3,4); return ci4(lsh(x,42));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + FROUND + LSHI + "function f() {var x=i4(1,2,3,4); return ci4(lsh(x,42.0));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + FROUND + LSHI + "function f() {var x=i4(1,2,3,4); return ci4(lsh(x,f32(42)));} return f");

var input = 'i4(0, 1, ' + INT32_MIN + ', ' + INT32_MAX + ')';
var vinput = [0, 1, INT32_MIN, INT32_MAX];

// TODO: What to do for masks > 31? Should we keep only the five low bits of
// the mask (JS) or not (x86)?
// Behave as x86 for now, fix when more broadly specified. See also bug 1068028
function Lsh(i) {  if (i > 31) return () => 0; return function(x) { return (x << i) | 0 } }
function Rsh(i) {  if (i > 31) return (x) => (x<0)?-1:0; return function(x) { return (x >> i) | 0 } }
function Ursh(i) { if (i > 31) return () => 0; return function(x) { return (x >>> i) | 0 } }

var asmLsh = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + LSHI + 'function f(x, y){x=x|0;y=y|0; var v=' + input + ';return ci4(lsh(v, x+y))} return f;'), this)
var asmRsh = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + RSHI + 'function f(x, y){x=x|0;y=y|0; var v=' + input + ';return ci4(rsh(v, x+y))} return f;'), this)
var asmUrsh = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + URSHI + 'function f(x, y){x=x|0;y=y|0; var v=' + input + ';return ci4(ursh(v, x+y))} return f;'), this)

for (var i = 1; i < 64; i++) {
    CheckI4(LSHI,  'var x=' + input + '; x=lsh(x, ' + i + ')',   vinput.map(Lsh(i)));
    CheckI4(RSHI,  'var x=' + input + '; x=rsh(x, ' + i + ')',   vinput.map(Rsh(i)));
    CheckI4(URSHI, 'var x=' + input + '; x=ursh(x, ' + i + ')',  vinput.map(Ursh(i)));

    assertEqX4(asmLsh(i, 3),  vinput.map(Lsh(i + 3)));
    assertEqX4(asmRsh(i, 3),  vinput.map(Rsh(i + 3)));
    assertEqX4(asmUrsh(i, 3), vinput.map(Ursh(i + 3)));
}

// Select
const I32SEL = 'var i4sel = i4.select;'
const F32SEL = 'var f4sel = f4.select;'
const I32BSEL = 'var i4sel = i4.bitselect;'
const F32BSEL = 'var f4sel = f4.bitselect;'

assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + I32SEL + "function f() {var x=f4(1,2,3,4); return ci4(i4sel(x,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + I32SEL + "function f() {var m=f4(1,2,3,4); var x=i4(1,2,3,4); return ci4(i4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + I32SEL + "function f() {var m=f4(1,2,3,4); var x=f4(1,2,3,4); return ci4(i4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + I32SEL + "function f() {var m=i4(1,2,3,4); var x=f4(1,2,3,4); return ci4(i4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + I32SEL + "function f() {var m=i4(1,2,3,4); var x=f4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + I32SEL + "function f() {var m=i4(1,2,3,4); var x=i4(1,2,3,4); var y=f4(5,6,7,8); return ci4(i4sel(m,x,y));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + I32SEL + "function f() {var m=i4(1,2,3,4); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return ci4(i4sel(m,x,y));} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + F32SEL + "function f() {var m=f4(1,2,3,4); return cf4(f4sel(x,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=f4(1,2,3,4); var x=i4(1,2,3,4); return cf4(f4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=f4(1,2,3,4); var x=f4(1,2,3,4); return cf4(f4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(1,2,3,4); var x=f4(1,2,3,4); var y=i4(5,6,7,8); return cf4(f4sel(m,x,y));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(1,2,3,4); var x=i4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y));} return f");

// These pass with select but not bitselect
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SEL + "function f() {var m=i4(0,0,0,0); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 6, 7, 8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SEL + "function f() {var m=i4(-1,-2,-3,-42); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SEL + "function f() {var m=i4(1,-1,2,-2); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 2, 7, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SEL + "function f() {var m=i4(42,45,-42,-47); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 6, 3, 4]);

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(0,0,0,0); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 6, 7, 8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(-1,-2,-3,-42); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(1,-1,2,-2); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 2, 7, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(42,45,-42,-47); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 6, 3, 4]);

// These pass for both select and bitselect
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SEL + "function f() {var m=i4(0,0,0,0); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 6, 7, 8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SEL + "function f() {var m=i4(0xffffffff,0xffffffff,0xffffffff,0xffffffff); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SEL + "function f() {var m=i4(0,0xffffffff,0,0xffffffff); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 2, 7, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SEL + "function f() {var m=i4(0,0,0xffffffff,0xffffffff); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 6, 3, 4]);

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(0,0,0,0); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 6, 7, 8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(0xffffffff,0xffffffff,0xffffffff,0xffffffff); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(0,0xffffffff,0,0xffffffff); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 2, 7, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(0,0,0xffffffff,0xffffffff); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 6, 3, 4]);

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32BSEL + "function f() {var m=i4(0,0,0,0); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 6, 7, 8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32BSEL + "function f() {var m=i4(0xffffffff,0xffffffff,0xffffffff,0xffffffff); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32BSEL + "function f() {var m=i4(0,0xffffffff,0,0xffffffff); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 2, 7, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32BSEL + "function f() {var m=i4(0,0,0xffffffff,0xffffffff); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 6, 3, 4]);

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32BSEL + "function f() {var m=i4(0,0,0,0); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 6, 7, 8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32BSEL + "function f() {var m=i4(0xffffffff,0xffffffff,0xffffffff,0xffffffff); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32BSEL + "function f() {var m=i4(0,0xffffffff,0,0xffffffff); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 2, 7, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + F32BSEL + "function f() {var m=i4(0,0,0xffffffff,0xffffffff); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 6, 3, 4]);

// Specific bitselect tests
var masks = [
    SIMD.int32x4(1337, 0x1337, 0x42, 42),
    SIMD.int32x4(0x00FF1CE, 0xBAADF00D, 0xDEADBEEF, 0xCAFED00D),
    SIMD.int32x4(0xD15EA5E, 0xDEADC0DE, 0xFACEB00C, 0x4B1D4B1D)
];

var simdToArray = (vec) => [vec.x, vec.y, vec.z, vec.w];

var inputs = [
    [SIMD.int32x4(0,4,9,16), SIMD.int32x4(1,2,3,4)],
    [SIMD.int32x4(-1, 2, INT32_MAX, INT32_MIN), SIMD.int32x4(INT32_MAX, -4, INT32_MIN, 42)]
];

var i32bsel = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32BSEL + "function f(mask, ifTrue, ifFalse) {mask=ci4(mask); ifTrue=ci4(ifTrue); ifFalse=ci4(ifFalse); return ci4(i4sel(mask,ifTrue,ifFalse)); } return f"), this)

for (var mask of masks) {
    for (var [x, y] of inputs) {
        assertEqX4(i32bsel(mask, x, y), simdToArray(SIMD.int32x4.bitselect(mask, x, y)));
    }
}

inputs = [
    [SIMD.float32x4(0.125,4.25,9.75,16.125), SIMD.float32x4(1.5,2.75,3.25,4.5)],
    [SIMD.float32x4(-1.5,-0,NaN,-Infinity),  SIMD.float32x4(1,-2,13.37,3.13)],
    [SIMD.float32x4(1.5,2.75,NaN,Infinity),  SIMD.float32x4(-NaN,-Infinity,9.75,16.125)]
];

var f32bsel = asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CI32 + CF32 + F32BSEL + "function f(mask, ifTrue, ifFalse) {mask=ci4(mask); ifTrue=cf4(ifTrue); ifFalse=cf4(ifFalse); return cf4(f4sel(mask,ifTrue,ifFalse)); } return f"), this)

for (var mask of masks)
    for (var [x, y] of inputs)
        assertEqX4(f32bsel(mask, x, y), simdToArray(SIMD.float32x4.bitselect(mask, x, y)));

// Splat
const I32SPLAT = 'var splat=i4.splat;'
const F32SPLAT = 'var splat=f4.splat;'

assertAsmTypeFail('glob', USE_ASM + I32 + F32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); var p=f4(1.,2.,3.,4.); p=splat(f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); m=splat(1, 2)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); m=splat()} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); m=splat(m);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); m=splat(1.0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + FROUND + "function f() {var m=i4(1,2,3,4); m=splat(f32(1.0));} return f");

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SPLAT + 'function f(){return ci4(splat(42));} return f'), this)(), [42, 42, 42, 42]);

const l33t = Math.fround(13.37);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(f32(1)));} return f'), this)(), [1, 1, 1, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(1.0));} return f'), this)(), [1, 1, 1, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(f32(1 >>> 0)));} return f'), this)(), [1, 1, 1, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(f32(13.37)));} return f'), this)(), [l33t, l33t, l33t, l33t]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(13.37));} return f'), this)(), [l33t, l33t, l33t, l33t]);

var i32view = new Int32Array(heap);
var f32view = new Float32Array(heap);
i32view[0] = 42;
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + I32 + CI32 + I32SPLAT + 'var i32=new glob.Int32Array(heap); function f(){return ci4(splat(i32[0]));} return f'), this, {}, heap)(), [42, 42, 42, 42]);
f32view[0] = 42;
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + F32 + CF32 + F32SPLAT + 'var f32=new glob.Float32Array(heap); function f(){return cf4(splat(f32[0]));} return f'), this, {}, heap)(), [42, 42, 42, 42]);
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(f32(1) + f32(2)));} return f'), this, {}, heap)(), [3, 3, 3, 3]);

// Dead code
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + 'function f(){var x=i4(1,2,3,4); return ci4(x); x=i4(5,6,7,8); return ci4(x);} return f'), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + 'function f(){var x=i4(1,2,3,4); var c=0; return ci4(x); c=x.x|0; return ci4(x);} return f'), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32A + 'function f(){var x=i4(1,2,3,4); var c=0; return ci4(x); x=i4a(x,x); return ci4(x);} return f'), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32S + 'function f(){var x=i4(1,2,3,4); var c=0; return ci4(x); x=i4s(x,x); return ci4(x);} return f'), this)(), [1, 2, 3, 4]);

// Swizzle
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x, -1, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x, 4, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x, 0.0, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x, 0, 0, 0, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); var y=42; x=swizzle(x, 0, 0, 0, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "var swizzle=i4.swizzle; function f() {var x=f4(1,2,3,4); x=swizzle(x, 0, 0, 0, 0);} return f");

function swizzle(arr, lanes) {
    return [arr[lanes[0]], arr[lanes[1]], arr[lanes[2]], arr[lanes[3]]];
}

var before = Date.now();
for (var i = 0; i < Math.pow(4, 4); i++) {
    var lanes = [i & 3, (i >> 2) & 3, (i >> 4) & 3, (i >> 6) & 3];
    CheckI4('var swizzle=i4.swizzle;', 'var x=i4(1,2,3,4); x=swizzle(x, ' + lanes.join(',') + ')', swizzle([1,2,3,4], lanes));
    CheckF4('var swizzle=f4.swizzle;', 'var x=f4(1,2,3,4); x=swizzle(x, ' + lanes.join(',') + ')', swizzle([1,2,3,4], lanes));
}
DEBUG && print('time for checking all swizzles:', Date.now() - before);

// Shuffle
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, y, -1, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, y, 8, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, y, 0.0, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, 0, 0, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, y, 0, 0, 0, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); var z=42; x=shuffle(x, y, 0, 0, 0, z);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "var shuffle=i4.shuffle; function f() {var x=f4(1,2,3,4); x=shuffle(x, x, 0, 0, 0, 0);} return f");

function shuffle(lhs, rhs, lanes) {
    return [(lanes[0] < 4 ? lhs : rhs)[lanes[0] % 4],
            (lanes[1] < 4 ? lhs : rhs)[lanes[1] % 4],
            (lanes[2] < 4 ? lhs : rhs)[lanes[2] % 4],
            (lanes[3] < 4 ? lhs : rhs)[lanes[3] % 4]];
}

before = Date.now();

const LANE_SELECTORS = [
    // Four of lhs or four of rhs, equivalent to swizzle
    [0, 1, 2, 3],
    [4, 5, 6, 7],
    [0, 2, 3, 1],
    [4, 7, 4, 6],
    // One of lhs, three of rhs
    [0, 4, 5, 6],
    [4, 0, 5, 6],
    [4, 5, 0, 6],
    [4, 5, 6, 0],
    // Two of lhs, two of rhs
    //      in one shufps
    [1, 2, 4, 5],
    [4, 5, 1, 2],
    //      in two shufps
    [7, 0, 5, 2],
    [0, 7, 5, 2],
    [0, 7, 2, 5],
    [7, 0, 2, 5],
    // Three of lhs, one of rhs
    [7, 0, 1, 2],
    [0, 7, 1, 2],
    [0, 1, 7, 2],
    [0, 1, 2, 7],
    // Impl-specific special cases for swizzle
    [2, 3, 2, 3],
    [0, 1, 0, 1],
    [0, 0, 1, 1],
    [2, 2, 3, 3],
    // Impl-specific special cases for shuffle (case and swapped case)
    [2, 3, 6, 7], [6, 7, 2, 3],
    [0, 1, 4, 5], [4, 5, 0, 1],
    [0, 4, 1, 5], [4, 0, 5, 1],
    [2, 6, 3, 7], [6, 2, 7, 3],
    [4, 1, 2, 3], [0, 5, 6, 7],
    // Insert one element from rhs into lhs keeping other elements unchanged
    [7, 1, 2, 3],
    [0, 7, 2, 3],
    [0, 1, 7, 2],
    // These are effectively vector selects
    [0, 5, 2, 3],
    [0, 1, 6, 3],
    [4, 5, 2, 3],
    [4, 1, 6, 3]
];

for (var lanes of LANE_SELECTORS) {
    CheckI4('var shuffle=i4.shuffle;', 'var x=i4(1,2,3,4); var y=i4(5,6,7,8); x=shuffle(x, y, ' + lanes.join(',') + ')', shuffle([1,2,3,4], [5,6,7,8], lanes));
    CheckF4('var shuffle=f4.shuffle;', 'var x=f4(1,2,3,4); var y=f4(5,6,7,8); x=shuffle(x, y, ' + lanes.join(',') + ')', shuffle([1,2,3,4], [5,6,7,8], lanes));
}
DEBUG && print('time for checking all shuffles:', Date.now() - before);

// 3. Function calls
// 3.1. No math builtins
assertAsmTypeFail('glob', USE_ASM + I32 + "var fround=glob.Math.fround; function f() {var x=i4(1,2,3,4); return +fround(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var sin=glob.Math.sin; function f() {var x=i4(1,2,3,4); return +sin(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var ceil=glob.Math.ceil; function f() {var x=i4(1,2,3,4); return +ceil(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var pow=glob.Math.pow; function f() {var x=i4(1,2,3,4); return +pow(1.0, x);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "var fround=glob.Math.fround; function f() {var x=i4(1,2,3,4); x=i4(fround(3));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var sin=glob.Math.sin; function f() {var x=i4(1,2,3,4); x=i4(sin(3.0));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var ceil=glob.Math.sin; function f() {var x=i4(1,2,3,4); x=i4(ceil(3.0));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var pow=glob.Math.pow; function f() {var x=i4(1,2,3,4); x=i4(pow(1.0, 2.0));} return f");

// 3.2. FFI calls
// Can't pass SIMD arguments to FFI
assertAsmTypeFail('glob', 'ffi', USE_ASM + I32 + "var func=ffi.func; function f() {var x=i4(1,2,3,4); func(x);} return f");
assertAsmTypeFail('glob', 'ffi', USE_ASM + F32 + "var func=ffi.func; function f() {var x=f4(1,2,3,4); func(x);} return f");

// Can't have FFI return SIMD values
assertAsmTypeFail('glob', 'ffi', USE_ASM + I32 + "var func=ffi.func; function f() {var x=i4(1,2,3,4); x=i4(func());} return f");
assertAsmTypeFail('glob', 'ffi', USE_ASM + F32 + "var func=ffi.func; function f() {var x=f4(1,2,3,4); x=f4(func());} return f");

// 3.3 Internal calls
// asm.js -> asm.js
// Retrieving values from asm.js
var code = USE_ASM + I32 + CI32 + I32A + `
    var check = ffi.check;

    function g() {
        var i = 0;
        var y = i4(0,0,0,0);
        var tmp = i4(0,0,0,0); var z = i4(1,1,1,1);
        var w = i4(5,5,5,5);
        for (; (i|0) < 30; i = i + 1 |0)
            y = i4a(z, y);
        y = i4a(w, y);
        check(y.x | 0, y.y | 0, y.z | 0, y.w | 0);
        return ci4(y);
    }

    function f(x) {
        x = ci4(x);
        var y = i4(0,0,0,0);
        y = ci4(g());
        check(y.x | 0, y.y | 0, y.z | 0, y.w | 0);
        return ci4(x);
    }
    return f;
`;

var v4 = SIMD.int32x4(1,2,3,4);
function check(x, y, z, w) {
    assertEq(x, 35);
    assertEq(y, 35);
    assertEq(z, 35);
    assertEq(w, 35);
}
var ffi = {check};
assertEqX4(asmLink(asmCompile('glob', 'ffi', code), this, ffi)(v4), [1,2,3,4]);

// Passing arguments from asm.js to asm.js
// TODO make this code look better with templatized strings
var code = USE_ASM + I32 + CI32 + I32A + `
    var assertEq = ffi.assertEq;

    function internal([args]) {
        [coerc]
        assertEq([last].x | 0, [i] | 0);
        assertEq([last].y | 0, [i] + 1 |0);
        assertEq([last].z | 0, [i] + 2 |0);
        assertEq([last].w | 0, [i] + 3 |0);
    }

    function external() {
        [decls]
        internal([args]);
    }
    return external;
`;

var ffi = {assertEq};
var args = '';
var decls = '';
var coerc = '';
for (var i = 1; i < 10; ++i) {
    var j = i;
    args += ((i > 1) ? ', ':'') + 'x' + i;
    decls += 'var x' + i + ' = i4(' + j++ + ', ' + j++ + ', ' + j++ + ', ' + j++ + ');\n';
    coerc += 'x' + i + ' = ci4(x' + i + ');\n';
    last = 'x' + i;
    var c = code.replace(/\[args\]/g, args)
                .replace(/\[last\]/g, last)
                .replace(/\[decls\]/i, decls)
                .replace(/\[coerc\]/i, coerc)
                .replace(/\[i\]/g, i);
    asmLink(asmCompile('glob', 'ffi', c), this, ffi)();
}

// Stress-test for register spilling code and stack depth checks
var code = `
    "use asm";
    var i4 = glob.SIMD.int32x4;
    var i4a = i4.add;
    var assertEq = ffi.assertEq;
    function g() {
        var x = i4(1,2,3,4);
        var y = i4(2,3,4,5);
        var z = i4(0,0,0,0);
        z = i4a(x, y);
        assertEq(z.x | 0, 3);
        assertEq(z.y | 0, 5);
        assertEq(z.z | 0, 7);
        assertEq(z.w | 0, 9);
    }
    return g
`
asmLink(asmCompile('glob', 'ffi', code), this, assertEqFFI)();

(function() {
    var code = `
        "use asm";
        var i4 = glob.SIMD.int32x4;
        var i4a = i4.add;
        var assertEq = ffi.assertEq;
        var one = ffi.one;

        // Function call with arguments on the stack (1 on x64, 3 on x86)
        function h(x1, x2, x3, x4, x5, x6, x7) {
            x1=x1|0
            x2=x2|0
            x3=x3|0
            x4=x4|0
            x5=x5|0
            x6=x6|0
            x7=x7|0
            return x1 + x2 |0
        }

        function g() {
            var x = i4(1,2,3,4);
            var y = i4(2,3,4,5);
            var z = i4(0,0,0,0);
            var w = 1;
            z = i4a(x, y);
            w = w + (one() | 0) | 0;
            assertEq(z.x | 0, 3);
            assertEq(z.y | 0, 5);
            assertEq(z.z | 0, 7);
            assertEq(z.w | 0, 9);
            h(1, 2, 3, 4, 42, 42, 42)|0
            return w | 0;
        }
        return g
    `;

    asmLink(asmCompile('glob', 'ffi', code), this, {assertEq: assertEq, one: () => 1})();
})();

// Function calls with mixed arguments on the stack (SIMD and scalar). In the
// worst case (x64), we have 6 int arg registers and 8 float registers.
(function() {
    var code = `
        "use asm";
        var i4 = glob.SIMD.int32x4;
        var ci4 = i4.check;
        function h(
            // In registers:
            gpr1, gpr2, gpr3, gpr4, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8,
            // On the stack:
            sint1, ssimd1, sdouble1, ssimd2, sint2, sint3, sint4, ssimd3, sdouble2
            )
        {
            gpr1=gpr1|0;
            gpr2=gpr2|0;
            gpr3=gpr3|0;
            gpr4=gpr4|0;

            xmm1=+xmm1;
            xmm2=+xmm2;
            xmm3=+xmm3;
            xmm4=+xmm4;
            xmm5=+xmm5;
            xmm6=+xmm6;
            xmm7=+xmm7;
            xmm8=+xmm8;

            sint1=sint1|0;
            ssimd1=ci4(ssimd1);
            sdouble1=+sdouble1;
            ssimd2=ci4(ssimd2);
            sint2=sint2|0;
            sint3=sint3|0;
            sint4=sint4|0;
            ssimd3=ci4(ssimd3);
            sdouble2=+sdouble2;

            return (ssimd1.x|0) + (ssimd2.y|0) + (ssimd3.z|0) + sint2 + gpr3 | 0;
        }

        function g() {
            var simd1 = i4(1,2,3,4);
            var simd2 = i4(5,6,7,8);
            var simd3 = i4(9,10,11,12);
            return h(1, 2, 3, 4,
                     1., 2., 3., 4., 5., 6., 7., 8.,
                     5, simd1, 9., simd2, 6, 7, 8, simd3, 10.) | 0;
        }
        return g
    `;

    assertEq(asmLink(asmCompile('glob', 'ffi', code), this)(), 1 + 6 + 11 + 6 + 3);
})();

// Check that the interrupt callback doesn't erase high components of simd
// registers:

// WARNING: must be the last test in this file
(function() {
    var iters = 2000000;
    var code = `
    "use asm";
    var i4 = glob.SIMD.int32x4;
    var i4a = i4.add;
    var ci4 = i4.check;
    function _() {
        var i = 0;
        var n = i4(0,0,0,0);
        var one = i4(1,1,1,1);
        for (; (i>>>0) < ` + iters + `; i=(i+1)>>>0) {
            n = i4a(n, one);
        }
        return ci4(n);
    }
    return _;`;
    // This test relies on the fact that setting the timeout will call the
    // interrupt callback at fixed intervals, even before the timeout.
    timeout(1000);
    var x4 = asmLink(asmCompile('glob', code), this)();
    assertEq(x4.x, iters);
    assertEq(x4.y, iters);
    assertEq(x4.z, iters);
    assertEq(x4.w, iters);
})();

} catch(e) {
    print('Stack:', e.stack)
    print('Error:', e)
    throw e;
}
