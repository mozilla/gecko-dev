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

// ./test/core/token.wast

// ./test/core/token.wast:3
assert_malformed(() => instantiate(`(func (drop (i32.const0))) `), `unknown operator`);

// ./test/core/token.wast:7
assert_malformed(() => instantiate(`(func br 0drop) `), `unknown operator`);

// ./test/core/token.wast:15
let $0 = instantiate(`(module
  (func(nop))
)`);

// ./test/core/token.wast:18
let $1 = instantiate(`(module
  (func (nop)nop)
)`);

// ./test/core/token.wast:21
let $2 = instantiate(`(module
  (func nop(nop))
)`);

// ./test/core/token.wast:24
let $3 = instantiate(`(module
  (func(nop)(nop))
)`);

// ./test/core/token.wast:27
let $4 = instantiate(`(module
  (func $$f(nop))
)`);

// ./test/core/token.wast:30
let $5 = instantiate(`(module
  (func br 0(nop))
)`);

// ./test/core/token.wast:33
let $6 = instantiate(`(module
  (table 1 funcref)
  (func)
  (elem (i32.const 0)0)
)`);

// ./test/core/token.wast:38
let $7 = instantiate(`(module
  (table 1 funcref)
  (func $$f)
  (elem (i32.const 0)$$f)
)`);

// ./test/core/token.wast:43
let $8 = instantiate(`(module
  (memory 1)
  (data (i32.const 0)"a")
)`);

// ./test/core/token.wast:47
let $9 = instantiate(`(module
  (import "spectest" "print"(func))
)`);

// ./test/core/token.wast:54
let $10 = instantiate(`(module
  (func;;bla
  )
)`);

// ./test/core/token.wast:58
let $11 = instantiate(`(module
  (func (nop);;bla
  )
)`);

// ./test/core/token.wast:62
let $12 = instantiate(`(module
  (func nop;;bla
  )
)`);

// ./test/core/token.wast:66
let $13 = instantiate(`(module
  (func $$f;;bla
  )
)`);

// ./test/core/token.wast:70
let $14 = instantiate(`(module
  (func br 0;;bla
  )
)`);

// ./test/core/token.wast:74
let $15 = instantiate(`(module
  (data "a";;bla
  )
)`);

// ./test/core/token.wast:82
let $16 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table 0 $$l)))
)`);

// ./test/core/token.wast:85
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table 0$$l))) `),
  `unknown operator`,
);

// ./test/core/token.wast:91
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table 0$$"l"))) `),
  `unknown operator`,
);

// ./test/core/token.wast:98
let $17 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table $$l 0)))
)`);

// ./test/core/token.wast:101
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table $$l0))) `),
  `unknown label`,
);

// ./test/core/token.wast:107
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table $$"l"0))) `),
  `unknown operator`,
);

// ./test/core/token.wast:114
let $18 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table $$l $$l)))
)`);

// ./test/core/token.wast:117
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table $$l$$l))) `),
  `unknown label`,
);

// ./test/core/token.wast:123
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table $$"l"$$l))) `),
  `unknown operator`,
);

// ./test/core/token.wast:130
let $19 = instantiate(`(module
  (func (block $$l0 (i32.const 0) (br_table $$l0)))
)`);

// ./test/core/token.wast:133
let $20 = instantiate(`(module
  (func (block $$l$$l (i32.const 0) (br_table $$l$$l)))
)`);

// ./test/core/token.wast:140
let $21 = instantiate(`(module
  (data "a")
)`);

// ./test/core/token.wast:143
assert_malformed(() => instantiate(`(data"a") `), `unknown operator`);

// ./test/core/token.wast:150
let $22 = instantiate(`(module
  (data $$l "a")
)`);

// ./test/core/token.wast:153
assert_malformed(() => instantiate(`(data $$l"a") `), `unknown operator`);

// ./test/core/token.wast:160
let $23 = instantiate(`(module
  (data $$l " a")
)`);

// ./test/core/token.wast:163
assert_malformed(() => instantiate(`(data $$l" a") `), `unknown operator`);

// ./test/core/token.wast:170
let $24 = instantiate(`(module
  (data $$l "a ")
)`);

// ./test/core/token.wast:173
assert_malformed(() => instantiate(`(data $$l"a ") `), `unknown operator`);

// ./test/core/token.wast:180
let $25 = instantiate(`(module
  (data $$l "a " "b")
)`);

// ./test/core/token.wast:183
assert_malformed(() => instantiate(`(data $$l"a ""b") `), `unknown operator`);

// ./test/core/token.wast:190
let $26 = instantiate(`(module
  (data $$l "\u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:193
assert_malformed(() => instantiate(`(data $$l"\u{f61a}\u{f4a9}") `), `unknown operator`);

// ./test/core/token.wast:200
let $27 = instantiate(`(module
  (data $$l " \u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:203
assert_malformed(() => instantiate(`(data $$l" \u{f61a}\u{f4a9}") `), `unknown operator`);

// ./test/core/token.wast:210
let $28 = instantiate(`(module
  (data $$l "\u{f61a}\u{f4a9} ")
)`);

// ./test/core/token.wast:213
assert_malformed(() => instantiate(`(data $$l"\u{f61a}\u{f4a9} ") `), `unknown operator`);

// ./test/core/token.wast:220
let $29 = instantiate(`(module
  (data "a" "b")
)`);

// ./test/core/token.wast:223
assert_malformed(() => instantiate(`(data "a""b") `), `unknown operator`);

// ./test/core/token.wast:230
let $30 = instantiate(`(module
  (data "a" " b")
)`);

// ./test/core/token.wast:233
assert_malformed(() => instantiate(`(data "a"" b") `), `unknown operator`);

// ./test/core/token.wast:240
let $31 = instantiate(`(module
  (data "a " "b")
)`);

// ./test/core/token.wast:243
assert_malformed(() => instantiate(`(data "a ""b") `), `unknown operator`);

// ./test/core/token.wast:250
let $32 = instantiate(`(module
  (data "\u{f61a}\u{f4a9}" "\u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:253
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9}""\u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/token.wast:260
let $33 = instantiate(`(module
  (data "\u{f61a}\u{f4a9}" " \u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:263
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9}"" \u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/token.wast:270
let $34 = instantiate(`(module
  (data "\u{f61a}\u{f4a9} " "\u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:273
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9} ""\u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/token.wast:281
assert_malformed(() => instantiate(`(func "a"x) `), `unknown operator`);

// ./test/core/token.wast:287
assert_malformed(() => instantiate(`(func "a"0) `), `unknown operator`);

// ./test/core/token.wast:293
assert_malformed(() => instantiate(`(func 0"a") `), `unknown operator`);

// ./test/core/token.wast:299
assert_malformed(() => instantiate(`(func "a"$$x) `), `unknown operator`);
