/* Copyright 2021 Mozilla Foundation
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
 */

// ./test/core/binary.wast

// ./test/core/binary.wast:1
let $0 = instantiate(`(module binary "\\00asm\\01\\00\\00\\00")`);

// ./test/core/binary.wast:2
let $1 = instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00")`);

// ./test/core/binary.wast:3
let $2 = instantiate(`(module $$M1 binary "\\00asm\\01\\00\\00\\00")`);
let $M1 = $2;

// ./test/core/binary.wast:4
let $3 = instantiate(`(module $$M2 binary "\\00asm" "\\01\\00\\00\\00")`);
let $M2 = $3;

// ./test/core/binary.wast:6
assert_malformed(() => instantiate(`(module binary "")`), `unexpected end`);

// ./test/core/binary.wast:7
assert_malformed(() => instantiate(`(module binary "\\01")`), `unexpected end`);

// ./test/core/binary.wast:8
assert_malformed(() => instantiate(`(module binary "\\00as")`), `unexpected end`);

// ./test/core/binary.wast:9
assert_malformed(
  () => instantiate(`(module binary "asm\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:10
assert_malformed(
  () => instantiate(`(module binary "msa\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:11
assert_malformed(
  () => instantiate(`(module binary "msa\\00\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:12
assert_malformed(
  () => instantiate(`(module binary "msa\\00\\00\\00\\00\\01")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:13
assert_malformed(
  () => instantiate(`(module binary "asm\\01\\00\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:14
assert_malformed(
  () => instantiate(`(module binary "wasm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:15
assert_malformed(
  () => instantiate(`(module binary "\\7fasm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:16
assert_malformed(
  () => instantiate(`(module binary "\\80asm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:17
assert_malformed(
  () => instantiate(`(module binary "\\82asm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:18
assert_malformed(
  () => instantiate(`(module binary "\\ffasm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:21
assert_malformed(
  () => instantiate(`(module binary "\\00\\00\\00\\01msa\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:24
assert_malformed(
  () => instantiate(`(module binary "a\\00ms\\00\\01\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:25
assert_malformed(
  () => instantiate(`(module binary "sm\\00a\\00\\00\\01\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:28
assert_malformed(
  () => instantiate(`(module binary "\\00ASM\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:31
assert_malformed(
  () => instantiate(`(module binary "\\00\\81\\a2\\94\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:34
assert_malformed(
  () => instantiate(`(module binary "\\ef\\bb\\bf\\00asm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:37
assert_malformed(() => instantiate(`(module binary "\\00asm")`), `unexpected end`);

// ./test/core/binary.wast:38
assert_malformed(() => instantiate(`(module binary "\\00asm\\01")`), `unexpected end`);

// ./test/core/binary.wast:39
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\01\\00\\00")`),
  `unexpected end`,
);

// ./test/core/binary.wast:40
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\00\\00\\00\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:41
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\0d\\00\\00\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:42
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\0e\\00\\00\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:43
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\00\\01\\00\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:44
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\00\\00\\01\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:45
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\00\\00\\00\\01")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:48
assert_malformed(
  () => instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00" "\\0e\\01\\00")`),
  `malformed section id`,
);

// ./test/core/binary.wast:49
assert_malformed(
  () => instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00" "\\7f\\01\\00")`),
  `malformed section id`,
);

// ./test/core/binary.wast:50
assert_malformed(
  () => instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00" "\\80\\01\\00\\01\\01\\00")`),
  `malformed section id`,
);

// ./test/core/binary.wast:51
assert_malformed(
  () => instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00" "\\81\\01\\00\\01\\01\\00")`),
  `malformed section id`,
);

// ./test/core/binary.wast:52
assert_malformed(
  () => instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00" "\\ff\\01\\00\\01\\01\\00")`),
  `malformed section id`,
);

// ./test/core/binary.wast:55
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\03\\02\\00\\00"          ;; Function section: 2 functions
    "\\0a\\0c\\02"                ;; Code section: 2 functions
    ;; function 0
    "\\04\\00"                   ;; Function size and local type count
    "\\41\\01"                   ;; i32.const 1
    "\\1a"                      ;; drop
    ;; Missing end marker here
    ;; function 1
    "\\05\\00"                   ;; Function size and local type count
    "\\41\\01"                   ;; i32.const 1
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`),
  `END opcode expected`,
);

// ./test/core/binary.wast:76
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\0a\\06\\01"                ;; Code section: 1 function
    ;; function 0
    "\\04\\00"                   ;; Function size and local type count
    "\\41\\01"                   ;; i32.const 1
    "\\1a"                      ;; drop
    ;; Missing end marker here
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:92
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\0a\\06\\01"                ;; Code section: 1 function
    ;; function 0
    "\\04\\00"                   ;; Function size and local type count
    "\\41\\01"                   ;; i32.const 1
    "\\1a"                      ;; drop
    ;; Missing end marker here
    "\\0b\\03\\01\\01\\00"          ;; Data section
  )`),
  `section size mismatch`,
);

// ./test/core/binary.wast:112
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\06\\05\\01\\7f\\00\\41\\00"    ;; Global section: 1 entry with missing end marker
    ;; Missing end marker here
    "\\0a\\04\\01\\02\\00\\0b"       ;; Code section: 1 function
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:125
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\0a\\0c\\01"                ;; Code section

    ;; function 0
    "\\0a\\02"
    "\\80\\80\\80\\80\\10\\7f"       ;; 0x100000000 i32
    "\\02\\7e"                   ;; 0x00000002 i64
    "\\0b"                      ;; end
  )`),
  `integer too large`,
);

// ./test/core/binary.wast:142
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\0a\\0c\\01"                ;; Code section

    ;; function 0
    "\\0a\\02"
    "\\80\\80\\80\\80\\10\\7f"       ;; 0x100000000 i32
    "\\02\\7e"                   ;; 0x00000002 i64
    "\\0b"                      ;; end
  )`),
  `integer too large`,
);

// ./test/core/binary.wast:159
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\0a\\0c\\01"                ;; Code section

    ;; function 0
    "\\0a\\02"
    "\\ff\\ff\\ff\\ff\\0f\\7f"       ;; 0xFFFFFFFF i32
    "\\02\\7e"                   ;; 0x00000002 i64
    "\\0b"                      ;; end
  )`),
  `too many locals`,
);

// ./test/core/binary.wast:175
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\06\\01\\60\\02\\7f\\7f\\00" ;; Type section: (param i32 i32)
    "\\03\\02\\01\\00"             ;; Function section
    "\\0a\\1c\\01"                ;; Code section

    ;; function 0
    "\\1a\\04"
    "\\80\\80\\80\\80\\04\\7f"       ;; 0x40000000 i32
    "\\80\\80\\80\\80\\04\\7e"       ;; 0x40000000 i64
    "\\80\\80\\80\\80\\04\\7d"       ;; 0x40000000 f32
    "\\80\\80\\80\\80\\04\\7c"       ;; 0x40000000 f64
    "\\0b"                      ;; end
  )`),
  `too many locals`,
);

// ./test/core/binary.wast:194
let $4 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01\\60\\00\\00"     ;; Type section
  "\\03\\02\\01\\00"           ;; Function section
  "\\0a\\0a\\01"              ;; Code section

  ;; function 0
  "\\08\\03"
  "\\00\\7f"                 ;; 0 i32
  "\\00\\7e"                 ;; 0 i64
  "\\02\\7d"                 ;; 2 f32
  "\\0b"                    ;; end
)`);

// ./test/core/binary.wast:209
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"  ;; Type section
    "\\03\\03\\02\\00\\00"     ;; Function section with 2 functions
  )`),
  `function and code section have inconsistent lengths`,
);

// ./test/core/binary.wast:219
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0a\\04\\01\\02\\00\\0b"  ;; Code section with 1 empty function
  )`),
  `function and code section have inconsistent lengths`,
);

// ./test/core/binary.wast:228
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"  ;; Type section
    "\\03\\03\\02\\00\\00"     ;; Function section with 2 functions
    "\\0a\\04\\01\\02\\00\\0b"  ;; Code section with 1 empty function
  )`),
  `function and code section have inconsistent lengths`,
);

// ./test/core/binary.wast:239
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"           ;; Type section
    "\\03\\02\\01\\00"                 ;; Function section with 1 function
    "\\0a\\07\\02\\02\\00\\0b\\02\\00\\0b"  ;; Code section with 2 empty functions
  )`),
  `function and code section have inconsistent lengths`,
);

// ./test/core/binary.wast:250
let $5 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\03\\01\\00"  ;; Function section with 0 functions
)`);

// ./test/core/binary.wast:256
let $6 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\0a\\01\\00"  ;; Code section with 0 functions
)`);

// ./test/core/binary.wast:262
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0c\\01\\03"                ;; Data count section with value 3
    "\\0b\\05\\02"                ;; Data section with two entries
    "\\01\\00"                   ;; Passive data section
    "\\01\\00"                   ;; Passive data section
  )`),
  `data count and data section have inconsistent lengths`,
);

// ./test/core/binary.wast:274
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0c\\01\\01"                ;; Data count section with value 1
    "\\0b\\05\\02"                ;; Data section with two entries
    "\\01\\00"                   ;; Passive data section
    "\\01\\00"                   ;; Passive data section
  )`),
  `data count and data section have inconsistent lengths`,
);

// ./test/core/binary.wast:286
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01\\00\\01"          ;; Memory section with one entry
    "\\0c\\01\\01"                ;; Data count section with value 1
  )`),
  `data count and data section have inconsistent lengths`,
);

// ./test/core/binary.wast:296
let $7 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\0c\\01\\00"                  ;; Data count section with value 0
)`);

// ./test/core/binary.wast:302
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"

    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\0e\\01"                ;; Code section

    ;; function 0
    "\\0c\\00"
    "\\41\\00"                   ;; zero args
    "\\41\\00"
    "\\41\\00"
    "\\fc\\08\\00\\00"             ;; memory.init
    "\\0b"

    "\\0b\\03\\01\\01\\00"          ;; Data section
  )`),
  `data count section required`,
);

// ./test/core/binary.wast:325
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"

    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\07\\01"                ;; Code section

    ;; function 0
    "\\05\\00"
    "\\fc\\09\\00"                ;; data.drop
    "\\0b"

    "\\0b\\03\\01\\01\\00"          ;; Data section
  )`),
  `data count section required`,
);

// ./test/core/binary.wast:345
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"

    "\\01\\04\\01\\60\\00\\00"       ;; Type section

    "\\03\\02\\01\\00"             ;; Function section

    "\\04\\04\\01"                ;; Table section with 1 entry
    "\\70\\00\\00"                ;; no max, minimum 0, funcref

    "\\05\\03\\01\\00\\00"          ;; Memory section

    "\\09\\07\\01"                ;; Element section with one segment
    "\\05\\70"                   ;; Passive, funcref
    "\\01"                      ;; 1 element
    "\\f3\\00\\0b"                ;; bad opcode, index 0, end

    "\\0a\\04\\01"                ;; Code section

    ;; function 0
    "\\02\\00"
    "\\0b"                      ;; end
  )`),
  `illegal opcode`,
);

// ./test/core/binary.wast:373
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"

    "\\01\\04\\01\\60\\00\\00"       ;; Type section

    "\\03\\02\\01\\00"             ;; Function section

    "\\04\\04\\01"                ;; Table section with 1 entry
    "\\70\\00\\00"                ;; no max, minimum 0, funcref

    "\\05\\03\\01\\00\\00"          ;; Memory section

    "\\09\\07\\01"                ;; Element section with one segment
    "\\05\\7f"                   ;; Passive, i32
    "\\01"                      ;; 1 element
    "\\d2\\00\\0b"                ;; ref.func, index 0, end

    "\\0a\\04\\01"                ;; Code section

    ;; function 0
    "\\02\\00"
    "\\0b"                      ;; end
  )`),
  `malformed reference type`,
);

// ./test/core/binary.wast:401
let $8 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"

  "\\01\\04\\01\\60\\00\\00"       ;; Type section

  "\\03\\02\\01\\00"             ;; Function section

  "\\04\\04\\01"                ;; Table section with 1 entry
  "\\70\\00\\00"                ;; no max, minimum 0, funcref

  "\\05\\03\\01\\00\\00"          ;; Memory section

  "\\09\\07\\01"                ;; Element section with one segment
  "\\05\\70"                   ;; Passive, funcref
  "\\01"                      ;; 1 element
  "\\d2\\00\\0b"                ;; ref.func, index 0, end

  "\\0a\\04\\01"                ;; Code section

  ;; function 0
  "\\02\\00"
  "\\0b"                      ;; end
)`);

// ./test/core/binary.wast:426
let $9 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"

  "\\01\\04\\01\\60\\00\\00"       ;; Type section

  "\\03\\02\\01\\00"             ;; Function section

  "\\04\\04\\01"                ;; Table section with 1 entry
  "\\70\\00\\00"                ;; no max, minimum 0, funcref

  "\\05\\03\\01\\00\\00"          ;; Memory section

  "\\09\\07\\01"                ;; Element section with one segment
  "\\05\\70"                   ;; Passive, funcref
  "\\01"                      ;; 1 element
  "\\d0\\70\\0b"                ;; ref.null, end

  "\\0a\\04\\01"                ;; Code section

  ;; function 0
  "\\02\\00"
  "\\0b"                      ;; end
)`);

// ./test/core/binary.wast:452
let $10 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\01\\00"                               ;; type count can be zero
)`);

// ./test/core/binary.wast:458
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\07\\02"                             ;; type section with inconsistent count (2 declared, 1 given)
    "\\60\\00\\00"                             ;; 1st type
    ;; "\\60\\00\\00"                          ;; 2nd type (missed)
  )`),
  `length out of bounds`,
);

// ./test/core/binary.wast:469
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\07\\01"                             ;; type section with inconsistent count (1 declared, 2 given)
    "\\60\\00\\00"                             ;; 1st type
    "\\60\\00\\00"                             ;; 2nd type (redundant)
  )`),
  `section size mismatch`,
);

// ./test/core/binary.wast:480
let $11 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\05\\01"                             ;; type section
    "\\60\\01\\7f\\00"                          ;; type 0
    "\\02\\01\\00"                             ;; import count can be zero
)`);

// ./test/core/binary.wast:488
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\04\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\05"                                 ;; malformed import kind
  )`),
  `malformed import kind`,
);

// ./test/core/binary.wast:498
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\05\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\05"                                 ;; malformed import kind
      "\\00"                                 ;; dummy byte
  )`),
  `malformed import kind`,
);

// ./test/core/binary.wast:509
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\04\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\05"                                 ;; malformed import kind
  )`),
  `malformed import kind`,
);

// ./test/core/binary.wast:519
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\05\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\05"                                 ;; malformed import kind
      "\\00"                                 ;; dummy byte
  )`),
  `malformed import kind`,
);

// ./test/core/binary.wast:530
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\04\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\80"                                 ;; malformed import kind
  )`),
  `malformed import kind`,
);

// ./test/core/binary.wast:540
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\05\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\80"                                 ;; malformed import kind
      "\\00"                                 ;; dummy byte
  )`),
  `malformed import kind`,
);

// ./test/core/binary.wast:553
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\01\\05\\01"                           ;; type section
      "\\60\\01\\7f\\00"                        ;; type 0
      "\\02\\16\\02"                           ;; import section with inconsistent count (2 declared, 1 given)
      ;; 1st import
      "\\08"                                 ;; string length
      "\\73\\70\\65\\63\\74\\65\\73\\74"            ;; spectest
      "\\09"                                 ;; string length
      "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"         ;; print_i32
      "\\00\\00"                              ;; import kind, import signature index
      ;; 2nd import
      ;; (missed)
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:572
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\01\\09\\02"                           ;; type section
      "\\60\\01\\7f\\00"                        ;; type 0
      "\\60\\01\\7d\\00"                        ;; type 1
      "\\02\\2b\\01"                           ;; import section with inconsistent count (1 declared, 2 given)
      ;; 1st import
      "\\08"                                 ;; string length
      "\\73\\70\\65\\63\\74\\65\\73\\74"            ;; spectest
      "\\09"                                 ;; string length
      "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"         ;; print_i32
      "\\00\\00"                              ;; import kind, import signature index
      ;; 2nd import
      ;; (redundant)
      "\\08"                                 ;; string length
      "\\73\\70\\65\\63\\74\\65\\73\\74"            ;; spectest
      "\\09"                                 ;; string length
      "\\70\\72\\69\\6e\\74\\5f\\66\\33\\32"         ;; print_f32
      "\\00\\01"                              ;; import kind, import signature index
  )`),
  `section size mismatch`,
);

// ./test/core/binary.wast:597
let $12 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\04\\01\\00"                             ;; table count can be zero
)`);

// ./test/core/binary.wast:603
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\04\\01\\01"                           ;; table section with inconsistent count (1 declared, 0 given)
      ;; "\\70\\01\\00\\00"                     ;; table entity
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:613
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\04\\03\\01"                           ;; table section with one entry
      "\\70"                                 ;; anyfunc
      "\\08"                                 ;; malformed table limits flag
  )`),
  `malformed limits flags`,
);

// ./test/core/binary.wast:622
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\04\\04\\01"                           ;; table section with one entry
      "\\70"                                 ;; anyfunc
      "\\08"                                 ;; malformed table limits flag
      "\\00"                                 ;; dummy byte
  )`),
  `malformed limits flags`,
);

// ./test/core/binary.wast:632
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\04\\06\\01"                           ;; table section with one entry
      "\\70"                                 ;; anyfunc
      "\\81\\00"                              ;; malformed table limits flag as LEB128
      "\\00\\00"                              ;; dummy bytes
  )`),
  `malformed limits flags`,
);

// ./test/core/binary.wast:644
let $13 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\01\\00"                             ;; memory count can be zero
)`);

// ./test/core/binary.wast:650
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\01\\01"                           ;; memory section with inconsistent count (1 declared, 0 given)
      ;; "\\00\\00"                           ;; memory 0 (missed)
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:660
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\02\\01"                           ;; memory section with one entry
      "\\08"                                 ;; malformed memory limits flag
  )`),
  `malformed limits flags`,
);

// ./test/core/binary.wast:668
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\03\\01"                           ;; memory section with one entry
      "\\08"                                 ;; malformed memory limits flag
      "\\00"                                 ;; dummy byte
  )`),
  `malformed limits flags`,
);

// ./test/core/binary.wast:677
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\05\\01"                           ;; memory section with one entry
      "\\81\\00"                              ;; malformed memory limits flag as LEB128
      "\\00\\00"                              ;; dummy bytes
  )`),
  `malformed limits flags`,
);

// ./test/core/binary.wast:686
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\05\\01"                           ;; memory section with one entry
      "\\81\\01"                              ;; malformed memory limits flag as LEB128
      "\\00\\00"                              ;; dummy bytes
  )`),
  `malformed limits flags`,
);

// ./test/core/binary.wast:697
let $14 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\01\\00"                               ;; global count can be zero
)`);

// ./test/core/binary.wast:703
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\06\\02"                             ;; global section with inconsistent count (2 declared, 1 given)
    "\\7f\\00\\41\\00\\0b"                       ;; global 0
    ;; "\\7f\\00\\41\\00\\0b"                    ;; global 1 (missed)
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:714
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                             ;; global section with inconsistent count (1 declared, 2 given)
    "\\7f\\00\\41\\00\\0b"                       ;; global 0
    "\\7f\\00\\41\\00\\0b"                       ;; global 1 (redundant)
  )`),
  `section size mismatch`,
);

// ./test/core/binary.wast:725
let $15 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                               ;; type section
  "\\60\\00\\00"                               ;; type 0
  "\\03\\03\\02\\00\\00"                         ;; func section
  "\\07\\01\\00"                               ;; export count can be zero
  "\\0a\\07\\02"                               ;; code section
  "\\02\\00\\0b"                               ;; function body 0
  "\\02\\00\\0b"                               ;; function body 1
)`);

// ./test/core/binary.wast:737
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\03\\02\\00\\00"                       ;; func section
    "\\07\\06\\02"                             ;; export section with inconsistent count (2 declared, 1 given)
    "\\02"                                   ;; export 0
    "\\66\\31"                                ;; export name
    "\\00\\00"                                ;; export kind, export func index
    ;; "\\02"                                ;; export 1 (missed)
    ;; "\\66\\32"                             ;; export name
    ;; "\\00\\01"                             ;; export kind, export func index
    "\\0a\\07\\02"                             ;; code section
    "\\02\\00\\0b"                             ;; function body 0
    "\\02\\00\\0b"                             ;; function body 1
  )`),
  `length out of bounds`,
);

// ./test/core/binary.wast:758
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\03\\02\\00\\00"                       ;; func section
    "\\07\\0b\\01"                             ;; export section with inconsistent count (1 declared, 2 given)
    "\\02"                                   ;; export 0
    "\\66\\31"                                ;; export name
    "\\00\\00"                                ;; export kind, export func index
    "\\02"                                   ;; export 1 (redundant)
    "\\66\\32"                                ;; export name
    "\\00\\01"                                ;; export kind, export func index
    "\\0a\\07\\02"                             ;; code section
    "\\02\\00\\0b"                             ;; function body 0
    "\\02\\00\\0b"                             ;; function body 1
  )`),
  `section size mismatch`,
);

// ./test/core/binary.wast:779
let $16 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                               ;; type section
  "\\60\\00\\00"                               ;; type 0
  "\\03\\02\\01\\00"                            ;; func section
  "\\04\\04\\01"                               ;; table section
  "\\70\\00\\01"                               ;; table 0
  "\\09\\01\\00"                               ;; elem segment count can be zero
  "\\0a\\04\\01"                               ;; code section
  "\\02\\00\\0b"                               ;; function body
)`);

// ./test/core/binary.wast:792
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\02\\01\\00"                          ;; func section
    "\\04\\04\\01"                             ;; table section
    "\\70\\00\\01"                             ;; table 0
    "\\09\\07\\02"                             ;; elem with inconsistent segment count (2 declared, 1 given)
    "\\00\\41\\00\\0b\\01\\00"                    ;; elem 0
    ;; "\\00\\41\\00\\0b\\01\\00"                 ;; elem 1 (missed)
  )`),
  `unexpected end`,
);

// ./test/core/binary.wast:808
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\02\\01\\00"                          ;; func section
    "\\04\\04\\01"                             ;; table section
    "\\70\\00\\01"                             ;; table 0
    "\\09\\0a\\02"                             ;; elem with inconsistent segment count (2 declared, 1 given)
    "\\00\\41\\00\\0b\\01\\00"                    ;; elem 0
    "\\00\\41\\00"                             ;; elem 1 (partial)
    ;; "\\0b\\01\\00"                          ;; elem 1 (missing part)
  )`),
  `unexpected end`,
);

// ./test/core/binary.wast:825
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\02\\01\\00"                          ;; func section
    "\\04\\04\\01"                             ;; table section
    "\\70\\00\\01"                             ;; table 0
    "\\09\\0d\\01"                             ;; elem with inconsistent segment count (1 declared, 2 given)
    "\\00\\41\\00\\0b\\01\\00"                    ;; elem 0
    "\\00\\41\\00\\0b\\01\\00"                    ;; elem 1 (redundant)
    "\\0a\\04\\01"                             ;; code section
    "\\02\\00\\0b"                             ;; function body
  )`),
  `section size mismatch`,
);

// ./test/core/binary.wast:843
let $17 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\03\\01"                               ;; memory section
  "\\00\\01"                                  ;; memory 0
  "\\0b\\01\\00"                               ;; data segment count can be zero
)`);

// ./test/core/binary.wast:851
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\07\\02"                             ;; data with inconsistent segment count (2 declared, 1 given)
    "\\00\\41\\00\\0b\\01\\61"                    ;; data 0
    ;; "\\00\\41\\01\\0b\\01\\62"                 ;; data 1 (missed)
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:864
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\0d\\01"                             ;; data with inconsistent segment count (1 declared, 2 given)
    "\\00\\41\\00\\0b\\01\\61"                    ;; data 0
    "\\00\\41\\01\\0b\\01\\62"                    ;; data 1 (redundant)
  )`),
  `section size mismatch`,
);

// ./test/core/binary.wast:877
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\0c\\01"                             ;; data section
    "\\00\\41\\03\\0b"                          ;; data segment 0
    "\\07"                                   ;; data segment size with inconsistent lengths (7 declared, 6 given)
    "\\61\\62\\63\\64\\65\\66"                    ;; 6 bytes given
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:891
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\0c\\01"                             ;; data section
    "\\00\\41\\00\\0b"                          ;; data segment 0
    "\\05"                                   ;; data segment size with inconsistent lengths (5 declared, 6 given)
    "\\61\\62\\63\\64\\65\\66"                    ;; 6 bytes given
  )`),
  `section size mismatch`,
);

// ./test/core/binary.wast:905
let $18 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                               ;; type section
  "\\60\\00\\00"                               ;; type 0
  "\\03\\02\\01\\00"                            ;; func section
  "\\0a\\11\\01"                               ;; code section
  "\\0f\\00"                                  ;; func 0
  "\\02\\40"                                  ;; block 0
  "\\41\\01"                                  ;; condition of if 0
  "\\04\\40"                                  ;; if 0
  "\\41\\01"                                  ;; index of br_table element
  "\\0e\\00"                                  ;; br_table target count can be zero
  "\\02"                                     ;; break depth for default
  "\\0b\\0b\\0b"                               ;; end
)`);

// ./test/core/binary.wast:922
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\25\\0c"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\60\\00\\00"                             ;; type 1
    "\\60\\00\\00"                             ;; type 2
    "\\60\\00\\00"                             ;; type 3
    "\\60\\00\\00"                             ;; type 4
    "\\60\\00\\00"                             ;; type 5
    "\\60\\00\\00"                             ;; type 6
    "\\60\\00\\00"                             ;; type 7
    "\\60\\00\\00"                             ;; type 8
    "\\60\\00\\00"                             ;; type 9
    "\\60\\00\\00"                             ;; type 10
    "\\60\\00\\00"                             ;; type 11
    "\\03\\02\\01\\00"                          ;; func section
    "\\0a\\13\\01"                             ;; code section
    "\\11\\00"                                ;; func 0
    "\\02\\40"                                ;; block 0
    "\\41\\01"                                ;; condition of if 0
    "\\04\\40"                                ;; if 0
    "\\41\\01"                                ;; index of br_table element
    "\\0e\\01"                                ;; br_table with inconsistent target count (1 declared, 2 given)
    "\\00"                                   ;; break depth 0
    "\\01"                                   ;; break depth 1
    "\\02"                                   ;; break depth for default, interpreted as a block
    "\\0b"                                   ;; end, interpreted as type 11 for the block
    "\\0b\\0b"                                ;; end
  )`),
  `unexpected end of section or function`,
);

// ./test/core/binary.wast:956
let $19 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01\\60\\00\\00"       ;; Type section
  "\\03\\02\\01\\00"             ;; Function section
  "\\08\\01\\00"                ;; Start section: function 0

  "\\0a\\04\\01"                ;; Code section
  ;; function 0
  "\\02\\00"
  "\\0b"                      ;; end
)`);

// ./test/core/binary.wast:969
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\08\\01\\00"                ;; Start section: function 0
    "\\08\\01\\00"                ;; Start section: function 0

    "\\0a\\04\\01"                ;; Code section
    ;; function 0
    "\\02\\00"
    "\\0b"                      ;; end
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:986
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"           ;; Type section
    "\\03\\02\\01\\00"                 ;; Function section with 1 function
    "\\03\\02\\01\\00"                 ;; Function section with 1 function
    "\\0a\\07\\02\\02\\00\\0b\\02\\00\\0b"  ;; Code section with 2 empty functions
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:998
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"           ;; Type section
    "\\03\\03\\02\\00\\00"              ;; Function section with 2 functions
    "\\0a\\04\\01\\02\\00\\0b"           ;; Code section with 1 empty function
    "\\0a\\04\\01\\02\\00\\0b"           ;; Code section with 1 empty function
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1010
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0c\\01\\01"                   ;; Data count section with value "1"
    "\\0c\\01\\01"                   ;; Data count section with value "1"
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1020
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0b\\01\\00"                   ;; Data section with zero entries
    "\\0b\\01\\00"                   ;; Data section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1030
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\01\\00"                   ;; Global section with zero entries
    "\\06\\01\\00"                   ;; Global section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1040
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\07\\01\\00"                   ;; Export section with zero entries
    "\\07\\01\\00"                   ;; Export section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1050
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\04\\01\\00"                 ;; Table section with zero entries
      "\\04\\01\\00"                 ;; Table section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1060
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\09\\01\\00"                 ;; Element section with zero entries
      "\\09\\01\\00"                 ;; Element section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1070
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\01\\00"                 ;; Import section with zero entries
      "\\02\\01\\00"                 ;; Import section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1080
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\01\\00"                   ;; Type section with zero entries
    "\\01\\01\\00"                   ;; Type section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1090
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\01\\00"                   ;; Memory section with zero entries
    "\\05\\01\\00"                   ;; Memory section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1100
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\01\\00"                 ;; Import section with zero entries
      "\\01\\01\\00"                 ;; Type section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1110
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\03\\01\\00"                 ;; Function section with zero entries
      "\\02\\01\\00"                 ;; Import section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1120
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\04\\01\\00"                 ;; Table section with zero entries
      "\\03\\01\\00"                 ;; Function section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1130
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\01\\00"                 ;; Memory section with zero entries
      "\\04\\01\\00"                 ;; Table section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1140
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\06\\01\\00"                 ;; Global section with zero entries
      "\\05\\01\\00"                 ;; Memory section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1150
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\07\\01\\00"                 ;; Export section with zero entries
      "\\06\\01\\00"                 ;; Global section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1160
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\03\\02\\01\\00"              ;; Function section
      "\\08\\01\\00"                 ;; Start section: function 0
      "\\07\\01\\00"                 ;; Export section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1171
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\03\\02\\01\\00"              ;; Function section
      "\\09\\01\\00"                 ;; Element section with zero entries
      "\\08\\01\\00"                 ;; Start section: function 0
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1182
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\0c\\01\\01"                 ;; Data count section with value "1"
      "\\09\\01\\00"                 ;; Element section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1192
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\0a\\01\\00"                 ;; Code section with zero entries
      "\\0c\\01\\01"                 ;; Data count section with value "1"
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1202
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\0b\\01\\00"                 ;; Data section with zero entries
      "\\0a\\01\\00"                 ;; Code section with zero entries
  )`),
  `unexpected content after last section`,
);

// ./test/core/binary.wast:1216
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\0a\\08\\01"                ;; Code section: 1 function
    ;; function 0
    "\\06\\00"                   ;; Function size and local type count
    "\\00"                      ;; unreachable
    "\\ff"                      ;; 0xff
    "\\00"                      ;; might be interpreted as unreachable, or as the second byte
                               ;; of a multi-byte instruction
    "\\00"                      ;; unreachable
    "\\0b"                      ;; end
  )`),
  `illegal opcode ff`,
);
