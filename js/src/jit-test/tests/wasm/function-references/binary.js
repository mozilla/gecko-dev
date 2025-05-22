load(libdir + "wasm-binary.js");

const v2vSig = {args:[], ret:VoidCode};
const v2vSigSection = sigSection([v2vSig]);

function checkInvalid(binary, errorMessage) {
    assertErrorMessage(() => new WebAssembly.Module(binary),
        WebAssembly.CompileError,
        errorMessage);
}

// The immediate of ref.null is a heap type, not a general reference type

const invalidRefNullHeapBody = moduleWithSections([
    v2vSigSection,
    declSection([0]),
    bodySection([
        funcBody({locals:[], body:[
            RefNullCode,
            OptRefCode,
            FuncRefCode,
            DropCode,
        ]})
    ])
]);
checkInvalid(invalidRefNullHeapBody, /invalid heap type/);

const invalidRefNullHeapElem = moduleWithSections([
    generalElemSection([
        {
            flag: PassiveElemExpr,
            typeCode: FuncRefCode,
            elems: [
                [RefNullCode, OptRefCode, FuncRefCode, EndCode]
            ]
        }
    ])
]);
checkInvalid(invalidRefNullHeapElem, /invalid heap type/);

const invalidRefNullHeapGlobal = moduleWithSections([
    globalSection([
        {
            valType: FuncRefCode,
            flag: 0,
            initExpr: [RefNullCode, OptRefCode, FuncRefCode, EndCode]
        }
    ])
]);
checkInvalid(invalidRefNullHeapGlobal, /invalid heap type/);

// Test the encoding edge case where an init expression is provided for an
// imported table using the same byte pattern as the table section.
const invalidImportedTableInit = moduleWithSections([
    importSection([
        {
            module: "", item: "",
            // Hand-encode a table type with an init expression, which should
            // only appear in the table section proper:
            // https://wasm-dsl.github.io/spectec/core/binary/modules.html#table-section
            tableType: [
                0x40, 0x00, ...tableType(FuncRefCode, limits({ min: 0 })),
                RefFuncCode, ...varS32(123), EndCode,
            ],
        },
    ]),
]);
checkInvalid(invalidImportedTableInit, /imported tables cannot have initializer expressions/);
