// |jit-test| skip-if: !wasmGcEnabled()
// Ensure that if gc types aren't enabled, test cases properly fail.

// Dummy constructor.
function Baguette(calories) {
    this.calories = calories;
}

// Type checking.

const { validate, CompileError } = WebAssembly;

assertErrorMessage(() => wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (func (result anyref)
        i32.const 42
    )
)`), CompileError, mismatchError('i32', 'anyref'));

assertErrorMessage(() => wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (func (result anyref)
        i32.const 0
        ref.null
        i32.const 42
        select
    )
)`), CompileError, /select operand types/);

assertErrorMessage(() => wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (func (result i32)
        ref.null
        if
            i32.const 42
        end
    )
)`), CompileError, mismatchError('nullref', 'i32'));


// Basic compilation tests.

let simpleTests = [
    "(module (gc_feature_opt_in 2) (func (drop (ref.null))))",
    "(module (gc_feature_opt_in 2) (func $test (local anyref)))",
    "(module (gc_feature_opt_in 2) (func $test (param anyref)))",
    "(module (gc_feature_opt_in 2) (func $test (result anyref) (ref.null)))",
    "(module (gc_feature_opt_in 2) (func $test (block anyref (unreachable)) unreachable))",
    "(module (gc_feature_opt_in 2) (func $test (local anyref) (result i32) (ref.is_null (get_local 0))))",
    `(module (gc_feature_opt_in 2) (import "a" "b" (param anyref)))`,
    `(module (gc_feature_opt_in 2) (import "a" "b" (result anyref)))`,
    `(module (gc_feature_opt_in 2) (global anyref (ref.null)))`,
    `(module (gc_feature_opt_in 2) (global (mut anyref) (ref.null)))`,
];

for (let src of simpleTests) {
    wasmEvalText(src, {a:{b(){}}});
    assertEq(validate(wasmTextToBinary(src)), true);
}

// Basic behavioral tests.

let { exports } = wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (func (export "is_null") (result i32)
        ref.null
        ref.is_null
    )

    (func $sum (result i32) (param i32)
        get_local 0
        i32.const 42
        i32.add
    )

    (func (export "is_null_spill") (result i32)
        ref.null
        i32.const 58
        call $sum
        drop
        ref.is_null
    )

    (func (export "is_null_local") (result i32) (local anyref)
        ref.null
        set_local 0
        i32.const 58
        call $sum
        drop
        get_local 0
        ref.is_null
    )

    (func (export "ref_eq") (param $a anyref) (param $b anyref) (result i32)
	  (ref.eq (get_local $a) (get_local $b)))

    (func (export "ref_eq_for_control") (param $a anyref) (param $b anyref) (result f64)
	  (if f64 (ref.eq (get_local $a) (get_local $b))
	      (f64.const 5.0)
	      (f64.const 3.0)))
    )`);

assertEq(exports.is_null(), 1);
assertEq(exports.is_null_spill(), 1);
assertEq(exports.is_null_local(), 1);
assertEq(exports.ref_eq(null, null), 1);
assertEq(exports.ref_eq(null, {}), 0);
assertEq(exports.ref_eq(this, this), 1);
assertEq(exports.ref_eq_for_control(null, null), 5);
assertEq(exports.ref_eq_for_control(null, {}), 3);
assertEq(exports.ref_eq_for_control(this, this), 5);

// Anyref param and result in wasm functions.

exports = wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (func (export "is_null") (result i32) (param $ref anyref)
        get_local $ref
        ref.is_null
    )

    (func (export "ref_or_null") (result anyref) (param $ref anyref) (param $selector i32)
        get_local $ref
        ref.null
        get_local $selector
        select
    )

    (func $recursive (export "nested") (result anyref) (param $ref anyref) (param $i i32)
        ;; i == 10 => ret $ref
        get_local $i
        i32.const 10
        i32.eq
        if
            get_local $ref
            return
        end

        get_local $ref

        get_local $i
        i32.const 1
        i32.add

        call $recursive
    )
)`).exports;

assertErrorMessage(() => exports.is_null(undefined), TypeError, "can't convert undefined to object");
assertEq(exports.is_null(null), 1);
assertEq(exports.is_null({}), 0);
assertEq(exports.is_null("hi"), 0);
assertEq(exports.is_null(3), 0);
assertEq(exports.is_null(3.5), 0);
assertEq(exports.is_null(true), 0);
assertEq(exports.is_null(Symbol("croissant")), 0);
assertEq(exports.is_null(new Baguette(100)), 0);

let baguette = new Baguette(42);
assertEq(exports.ref_or_null(null, 0), null);
assertEq(exports.ref_or_null(baguette, 0), null);

let ref = exports.ref_or_null(baguette, 1);
assertEq(ref, baguette);
assertEq(ref.calories, baguette.calories);

ref = exports.nested(baguette, 0);
assertEq(ref, baguette);
assertEq(ref.calories, baguette.calories);

// Make sure grow-memory isn't blocked by the lack of gc.
(function() {
    assertEq(wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (memory 0 64)
    (func (export "f") (param anyref) (result i32)
        i32.const 10
        grow_memory
        drop
        current_memory
    )
)`).exports.f({}), 10);
})();

// More interesting use cases about control flow joins.

function assertJoin(body) {
    let val = { i: -1 };
    assertEq(wasmEvalText(`(module
        (gc_feature_opt_in 2)
        (func (export "test") (param $ref anyref) (param $i i32) (result anyref)
            ${body}
        )
    )`).exports.test(val), val);
    assertEq(val.i, -1);
}

assertJoin("(block anyref get_local $ref)");
assertJoin("(block $out anyref get_local $ref br $out)");
assertJoin("(loop anyref get_local $ref)");

assertJoin(`(block $out anyref (loop $top anyref
    get_local $i
    i32.const 1
    i32.add
    tee_local $i
    i32.const 10
    i32.eq
    if
        get_local $ref
        return
    end
    br $top))
`);

assertJoin(`(block $out (loop $top
    get_local $i
    i32.const 1
    i32.add
    tee_local $i
    i32.const 10
    i32.le_s
    if
        br $top
    else
        get_local $ref
        return
    end
    )) unreachable
`);

assertJoin(`(block $out anyref (loop $top
    get_local $ref
    get_local $i
    i32.const 1
    i32.add
    tee_local $i
    i32.const 10
    i32.eq
    br_if $out
    br $top
    ) unreachable)
`);

assertJoin(`(block $out anyref (block $unreachable anyref (loop $top
    get_local $ref
    get_local $i
    i32.const 1
    i32.add
    tee_local $i
    br_table $unreachable $out
    ) unreachable))
`);

let x = { i: 42 }, y = { f: 53 };
exports = wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (func (export "test") (param $lhs anyref) (param $rhs anyref) (param $i i32) (result anyref)
        get_local $lhs
        get_local $rhs
        get_local $i
        select
    )
)`).exports;

let result = exports.test(x, y, 0);
assertEq(result, y);
assertEq(result.i, undefined);
assertEq(result.f, 53);
assertEq(x.i, 42);

result = exports.test(x, y, 1);
assertEq(result, x);
assertEq(result.i, 42);
assertEq(result.f, undefined);
assertEq(y.f, 53);

// Anyref in params/result of imported functions.

let firstBaguette = new Baguette(13),
    secondBaguette = new Baguette(37);

let imports = {
    i: 0,
    myBaguette: null,
    funcs: {
        param(x) {
            if (this.i === 0) {
                assertEq(x, firstBaguette);
                assertEq(x.calories, 13);
                assertEq(secondBaguette !== null, true);
            } else if (this.i === 1 || this.i === 2) {
                assertEq(x, secondBaguette);
                assertEq(x.calories, 37);
                assertEq(firstBaguette !== null, true);
            } else if (this.i === 3) {
                assertEq(x, null);
            } else {
                firstBaguette = null;
                secondBaguette = null;
                gc(); // evil mode
            }
            this.i++;
        },
        ret() {
            return imports.myBaguette;
        }
    }
};

exports = wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (import $ret "funcs" "ret" (result anyref))
    (import $param "funcs" "param" (param anyref))

    (func (export "param") (param $x anyref) (param $y anyref)
        get_local $y
        get_local $x
        call $param
        call $param
    )

    (func (export "ret") (result anyref)
        call $ret
    )
)`, imports).exports;

exports.param(firstBaguette, secondBaguette);
exports.param(secondBaguette, null);
exports.param(firstBaguette, secondBaguette);

imports.myBaguette = null;
assertEq(exports.ret(), null);

imports.myBaguette = new Baguette(1337);
assertEq(exports.ret(), imports.myBaguette);

// Check lazy stubs generation.

exports = wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (import $mirror "funcs" "mirror" (param anyref) (result anyref))
    (import $augment "funcs" "augment" (param anyref) (result anyref))

    (global $count_f (mut i32) (i32.const 0))
    (global $count_g (mut i32) (i32.const 0))

    (func $f (param $param anyref) (result anyref)
        i32.const 1
        get_global $count_f
        i32.add
        set_global $count_f

        get_local $param
        call $augment
    )

    (func $g (param $param anyref) (result anyref)
        i32.const 1
        get_global $count_g
        i32.add
        set_global $count_g

        get_local $param
        call $mirror
    )

    (table (export "table") 10 anyfunc)
    (elem (i32.const 0) $f $g $mirror $augment)
    (type $table_type (func (param anyref) (result anyref)))

    (func (export "call_indirect") (param $i i32) (param $ref anyref) (result anyref)
        get_local $ref
        get_local $i
        call_indirect $table_type
    )

    (func (export "count_f") (result i32) get_global $count_f)
    (func (export "count_g") (result i32) get_global $count_g)
)`, {
    funcs: {
        mirror(x) {
            return x;
        },
        augment(x) {
            x.i++;
            x.newProp = "hello";
            return x;
        }
    }
}).exports;

x = { i: 19 };
assertEq(exports.table.get(0)(x), x);
assertEq(x.i, 20);
assertEq(x.newProp, "hello");
assertEq(exports.count_f(), 1);
assertEq(exports.count_g(), 0);

x = { i: 21 };
assertEq(exports.table.get(1)(x), x);
assertEq(x.i, 21);
assertEq(typeof x.newProp, "undefined");
assertEq(exports.count_f(), 1);
assertEq(exports.count_g(), 1);

x = { i: 22 };
assertEq(exports.table.get(2)(x), x);
assertEq(x.i, 22);
assertEq(typeof x.newProp, "undefined");
assertEq(exports.count_f(), 1);
assertEq(exports.count_g(), 1);

x = { i: 23 };
assertEq(exports.table.get(3)(x), x);
assertEq(x.i, 24);
assertEq(x.newProp, "hello");
assertEq(exports.count_f(), 1);
assertEq(exports.count_g(), 1);

// Globals.

// Anyref globals in wasm modules.

assertErrorMessage(() => wasmEvalText(`(module (gc_feature_opt_in 2) (global (import "glob" "anyref") anyref))`, { glob: { anyref: 42 } }),
    WebAssembly.LinkError,
    /import object field 'anyref' is not a Object-or-null/);

assertErrorMessage(() => wasmEvalText(`(module (gc_feature_opt_in 2) (global (import "glob" "anyref") anyref))`, { glob: { anyref: new WebAssembly.Global({ value: 'i32' }, 42) } }),
    WebAssembly.LinkError,
    /imported global type mismatch/);

assertErrorMessage(() => wasmEvalText(`(module (gc_feature_opt_in 2) (global (import "glob" "i32") i32))`, { glob: { i32: {} } }),
    WebAssembly.LinkError,
    /import object field 'i32' is not a Number/);

imports = {
    constants: {
        imm_null: null,
        imm_bread: new Baguette(321),
        mut_null: new WebAssembly.Global({ value: "anyref", mutable: true }, null),
        mut_bread: new WebAssembly.Global({ value: "anyref", mutable: true }, new Baguette(123))
    }
};

exports = wasmEvalText(`(module
    (gc_feature_opt_in 2)
    (global $g_imp_imm_null  (import "constants" "imm_null") anyref)
    (global $g_imp_imm_bread (import "constants" "imm_bread") anyref)

    (global $g_imp_mut_null   (import "constants" "mut_null") (mut anyref))
    (global $g_imp_mut_bread  (import "constants" "mut_bread") (mut anyref))

    (global $g_imm_null     anyref (ref.null))
    (global $g_imm_getglob  anyref (get_global $g_imp_imm_bread))
    (global $g_mut         (mut anyref) (ref.null))

    (func (export "imm_null")      (result anyref) get_global $g_imm_null)
    (func (export "imm_getglob")   (result anyref) get_global $g_imm_getglob)

    (func (export "imp_imm_null")  (result anyref) get_global $g_imp_imm_null)
    (func (export "imp_imm_bread") (result anyref) get_global $g_imp_imm_bread)
    (func (export "imp_mut_null")  (result anyref) get_global $g_imp_mut_null)
    (func (export "imp_mut_bread") (result anyref) get_global $g_imp_mut_bread)

    (func (export "set_imp_null")  (param anyref) get_local 0 set_global $g_imp_mut_null)
    (func (export "set_imp_bread") (param anyref) get_local 0 set_global $g_imp_mut_bread)

    (func (export "set_mut") (param anyref) get_local 0 set_global $g_mut)
    (func (export "get_mut") (result anyref) get_global $g_mut)
)`, imports).exports;

assertEq(exports.imp_imm_null(), imports.constants.imm_null);
assertEq(exports.imp_imm_bread(), imports.constants.imm_bread);

assertEq(exports.imm_null(), null);
assertEq(exports.imm_getglob(), imports.constants.imm_bread);

assertEq(exports.imp_mut_null(), imports.constants.mut_null.value);
assertEq(exports.imp_mut_bread(), imports.constants.mut_bread.value);

let brandNewBaguette = new Baguette(1000);
exports.set_imp_null(brandNewBaguette);
assertEq(exports.imp_mut_null(), brandNewBaguette);
assertEq(exports.imp_mut_bread(), imports.constants.mut_bread.value);

exports.set_imp_bread(null);
assertEq(exports.imp_mut_null(), brandNewBaguette);
assertEq(exports.imp_mut_bread(), null);

assertEq(exports.get_mut(), null);
let glutenFreeBaguette = new Baguette("calories-free bread");
exports.set_mut(glutenFreeBaguette);
assertEq(exports.get_mut(), glutenFreeBaguette);
assertEq(exports.get_mut().calories, "calories-free bread");
