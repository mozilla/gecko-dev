// |jit-test| skip-if: !('Function' in WebAssembly)

// Tests constants pools in the tail call trampoline

const fun1 = new WebAssembly.Function({ parameters: [], results: [] }, () => { });
const isFun1 = function (f) { return f == fun1; }
const ext1 = {};
const isExt1 = function (e) { return e == ext1; }

const imp = { gc, fun1, ext1, isFun1, isExt1, };

function main() {
    const a0 = ext1; const a1 = ext1; const a2 = 1.2; const a3 = ext1; const a4 = 1; const a5 = 1; const a6 = 1; const a7 = ext1; const a8 = 1.2; const a9 = 1.2; const a10 = 1.125; const a11 = fun1; const a12 = fun1; const a13 = 1; const a14 = 1; const a15 = 1; const a16 = 1; const a17 = 1; const a18 = 1; const a19 = 1; const a20 = 1; const a21 = 1; const a22 = 1; const a23 = 1; const a24 = 1; const a25 = 1; const a26 = 1; const a27 = 1; const a28 = fun1; const a29 = 1; const a30 = 1;
    ins2.exports.f0(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30);
}

const ins4 = wasmEvalText(`(module
    (func $f2 (export "f2"))
)`);

const ins3 = wasmEvalText(`(module
    (import "x" "f2" (func $f2 ))

    (func $f1 (export "f1")
      return_call $f2
    )
)`, {x: {
    "f2": ins4.exports.f2,
  }});

const ins2 = wasmEvalText(`(module
  (import "" "fun1" (global $fun1 funcref))
  (import "" "isFun1" (func $isFun1 (param funcref) (result i32)))
  (import "" "ext1" (global $ext1 externref))
  (import "" "isExt1" (func $isExt1 (param externref) (result i32)))
    (import "x" "f1" (func $f1))

    (func $f0 (export "f0") (param externref externref f64 externref i32 i32 i32 externref f64 f64 f32 funcref funcref i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 funcref i32 i32) (result )
      local.get 0
block (param externref)
call $isExt1
br_if 0
unreachable
end
local.get 1
block (param externref)
call $isExt1
br_if 0
unreachable
end
local.get 2
block (param f64)
f64.const 1.2
f64.eq
br_if 0
unreachable
end
local.get 3
block (param externref)
call $isExt1
br_if 0
unreachable
end
local.get 4
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 5
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 6
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 7
block (param externref)
call $isExt1
br_if 0
unreachable
end
local.get 8
block (param f64)
f64.const 1.2
f64.eq
br_if 0
unreachable
end
local.get 9
block (param f64)
f64.const 1.2
f64.eq
br_if 0
unreachable
end
local.get 10
block (param f32)
f32.const 1.125
f32.eq
br_if 0
unreachable
end
local.get 11
block (param funcref)
call $isFun1
br_if 0
unreachable
end
local.get 12
block (param funcref)
call $isFun1
br_if 0
unreachable
end
local.get 13
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 14
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 15
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 16
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 17
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 18
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 19
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 20
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 21
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 22
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 23
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 24
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 25
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 26
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 27
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 28
block (param funcref)
call $isFun1
br_if 0
unreachable
end
local.get 29
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end
local.get 30
block (param i32)
i32.const 1
i32.eq
br_if 0
unreachable
end

    return_call $f1
    )

)`, {"":imp, x: {
  "f1": ins3.exports.f1,
}});

main()
