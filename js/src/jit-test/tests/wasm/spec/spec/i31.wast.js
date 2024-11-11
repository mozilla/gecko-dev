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

// ./test/core/gc/i31.wast

// ./test/core/gc/i31.wast:1
let $0 = instantiate(`(module
  (func (export "new") (param $$i i32) (result (ref i31))
    (ref.i31 (local.get $$i))
  )

  (func (export "get_u") (param $$i i32) (result i32)
    (i31.get_u (ref.i31 (local.get $$i)))
  )
  (func (export "get_s") (param $$i i32) (result i32)
    (i31.get_s (ref.i31 (local.get $$i)))
  )

  (func (export "get_u-null") (result i32)
    (i31.get_u (ref.null i31))
  )
  (func (export "get_s-null") (result i32)
    (i31.get_u (ref.null i31))
  )

  (global $$i (ref i31) (ref.i31 (i32.const 2)))
  (global $$m (mut (ref i31)) (ref.i31 (i32.const 3)))

  (func (export "get_globals") (result i32 i32)
    (i31.get_u (global.get $$i))
    (i31.get_u (global.get $$m))
  )

  (func (export "set_global") (param i32)
    (global.set $$m (ref.i31 (local.get 0)))
  )
)`);

// ./test/core/gc/i31.wast:33
assert_return(() => invoke($0, `new`, [1]), [new RefWithType('i31ref')]);

// ./test/core/gc/i31.wast:35
assert_return(() => invoke($0, `get_u`, [0]), [value("i32", 0)]);

// ./test/core/gc/i31.wast:36
assert_return(() => invoke($0, `get_u`, [100]), [value("i32", 100)]);

// ./test/core/gc/i31.wast:37
assert_return(() => invoke($0, `get_u`, [-1]), [value("i32", 2147483647)]);

// ./test/core/gc/i31.wast:38
assert_return(() => invoke($0, `get_u`, [1073741823]), [value("i32", 1073741823)]);

// ./test/core/gc/i31.wast:39
assert_return(() => invoke($0, `get_u`, [1073741824]), [value("i32", 1073741824)]);

// ./test/core/gc/i31.wast:40
assert_return(() => invoke($0, `get_u`, [2147483647]), [value("i32", 2147483647)]);

// ./test/core/gc/i31.wast:41
assert_return(() => invoke($0, `get_u`, [-1431655766]), [value("i32", 715827882)]);

// ./test/core/gc/i31.wast:42
assert_return(() => invoke($0, `get_u`, [-894784854]), [value("i32", 1252698794)]);

// ./test/core/gc/i31.wast:44
assert_return(() => invoke($0, `get_s`, [0]), [value("i32", 0)]);

// ./test/core/gc/i31.wast:45
assert_return(() => invoke($0, `get_s`, [100]), [value("i32", 100)]);

// ./test/core/gc/i31.wast:46
assert_return(() => invoke($0, `get_s`, [-1]), [value("i32", -1)]);

// ./test/core/gc/i31.wast:47
assert_return(() => invoke($0, `get_s`, [1073741823]), [value("i32", 1073741823)]);

// ./test/core/gc/i31.wast:48
assert_return(() => invoke($0, `get_s`, [1073741824]), [value("i32", -1073741824)]);

// ./test/core/gc/i31.wast:49
assert_return(() => invoke($0, `get_s`, [2147483647]), [value("i32", -1)]);

// ./test/core/gc/i31.wast:50
assert_return(() => invoke($0, `get_s`, [-1431655766]), [value("i32", 715827882)]);

// ./test/core/gc/i31.wast:51
assert_return(() => invoke($0, `get_s`, [-894784854]), [value("i32", -894784854)]);

// ./test/core/gc/i31.wast:53
assert_trap(() => invoke($0, `get_u-null`, []), `null i31 reference`);

// ./test/core/gc/i31.wast:54
assert_trap(() => invoke($0, `get_s-null`, []), `null i31 reference`);

// ./test/core/gc/i31.wast:56
assert_return(() => invoke($0, `get_globals`, []), [value("i32", 2), value("i32", 3)]);

// ./test/core/gc/i31.wast:58
invoke($0, `set_global`, [1234]);

// ./test/core/gc/i31.wast:59
assert_return(() => invoke($0, `get_globals`, []), [value("i32", 2), value("i32", 1234)]);

// ./test/core/gc/i31.wast:61
let $1 = instantiate(`(module $$tables_of_i31ref
  (table $$table 3 10 i31ref)
  (elem (table $$table) (i32.const 0) i31ref (item (ref.i31 (i32.const 999)))
                                            (item (ref.i31 (i32.const 888)))
                                            (item (ref.i31 (i32.const 777))))

  (func (export "size") (result i32)
    table.size $$table
  )

  (func (export "get") (param i32) (result i32)
    (i31.get_u (table.get $$table (local.get 0)))
  )

  (func (export "grow") (param i32 i32) (result i32)
    (table.grow $$table (ref.i31 (local.get 1)) (local.get 0))
  )

  (func (export "fill") (param i32 i32 i32)
    (table.fill $$table (local.get 0) (ref.i31 (local.get 1)) (local.get 2))
  )

  (func (export "copy") (param i32 i32 i32)
    (table.copy $$table $$table (local.get 0) (local.get 1) (local.get 2))
  )

  (elem $$elem i31ref (item (ref.i31 (i32.const 123)))
                     (item (ref.i31 (i32.const 456)))
                     (item (ref.i31 (i32.const 789))))
  (func (export "init") (param i32 i32 i32)
    (table.init $$table $$elem (local.get 0) (local.get 1) (local.get 2))
  )
)`);
let $tables_of_i31ref = $1;

// ./test/core/gc/i31.wast:96
assert_return(() => invoke($1, `size`, []), [value("i32", 3)]);

// ./test/core/gc/i31.wast:97
assert_return(() => invoke($1, `get`, [0]), [value("i32", 999)]);

// ./test/core/gc/i31.wast:98
assert_return(() => invoke($1, `get`, [1]), [value("i32", 888)]);

// ./test/core/gc/i31.wast:99
assert_return(() => invoke($1, `get`, [2]), [value("i32", 777)]);

// ./test/core/gc/i31.wast:102
assert_return(() => invoke($1, `grow`, [2, 333]), [value("i32", 3)]);

// ./test/core/gc/i31.wast:103
assert_return(() => invoke($1, `size`, []), [value("i32", 5)]);

// ./test/core/gc/i31.wast:104
assert_return(() => invoke($1, `get`, [3]), [value("i32", 333)]);

// ./test/core/gc/i31.wast:105
assert_return(() => invoke($1, `get`, [4]), [value("i32", 333)]);

// ./test/core/gc/i31.wast:108
invoke($1, `fill`, [2, 111, 2]);

// ./test/core/gc/i31.wast:109
assert_return(() => invoke($1, `get`, [2]), [value("i32", 111)]);

// ./test/core/gc/i31.wast:110
assert_return(() => invoke($1, `get`, [3]), [value("i32", 111)]);

// ./test/core/gc/i31.wast:113
invoke($1, `copy`, [3, 0, 2]);

// ./test/core/gc/i31.wast:114
assert_return(() => invoke($1, `get`, [3]), [value("i32", 999)]);

// ./test/core/gc/i31.wast:115
assert_return(() => invoke($1, `get`, [4]), [value("i32", 888)]);

// ./test/core/gc/i31.wast:118
invoke($1, `init`, [1, 0, 3]);

// ./test/core/gc/i31.wast:119
assert_return(() => invoke($1, `get`, [1]), [value("i32", 123)]);

// ./test/core/gc/i31.wast:120
assert_return(() => invoke($1, `get`, [2]), [value("i32", 456)]);

// ./test/core/gc/i31.wast:121
assert_return(() => invoke($1, `get`, [3]), [value("i32", 789)]);

// ./test/core/gc/i31.wast:123
let $2 = instantiate(`(module $$env
  (global (export "g") i32 (i32.const 42))
)`);
let $env = $2;

// ./test/core/gc/i31.wast:126
register($2, `env`);

// ./test/core/gc/i31.wast:128
let $3 = instantiate(`(module $$i31ref_of_global_table_initializer
  (global $$g (import "env" "g") i32)
  (table $$t 3 3 (ref i31) (ref.i31 (global.get $$g)))
  (func (export "get") (param i32) (result i32)
    (i31.get_u (local.get 0) (table.get $$t))
  )
)`);
let $i31ref_of_global_table_initializer = $3;

// ./test/core/gc/i31.wast:136
assert_return(() => invoke($3, `get`, [0]), [value("i32", 42)]);

// ./test/core/gc/i31.wast:137
assert_return(() => invoke($3, `get`, [1]), [value("i32", 42)]);

// ./test/core/gc/i31.wast:138
assert_return(() => invoke($3, `get`, [2]), [value("i32", 42)]);

// ./test/core/gc/i31.wast:140
let $4 = instantiate(`(module $$i31ref_of_global_global_initializer
  (global $$g0 (import "env" "g") i32)
  (global $$g1 i31ref (ref.i31 (global.get $$g0)))
  (func (export "get") (result i32)
    (i31.get_u (global.get $$g1))
  )
)`);
let $i31ref_of_global_global_initializer = $4;

// ./test/core/gc/i31.wast:148
assert_return(() => invoke($4, `get`, []), [value("i32", 42)]);

// ./test/core/gc/i31.wast:150
let $5 = instantiate(`(module $$anyref_global_of_i31ref
  (global $$c anyref (ref.i31 (i32.const 1234)))
  (global $$m (mut anyref) (ref.i31 (i32.const 5678)))

  (func (export "get_globals") (result i32 i32)
    (i31.get_u (ref.cast i31ref (global.get $$c)))
    (i31.get_u (ref.cast i31ref (global.get $$m)))
  )

  (func (export "set_global") (param i32)
    (global.set $$m (ref.i31 (local.get 0)))
  )
)`);
let $anyref_global_of_i31ref = $5;

// ./test/core/gc/i31.wast:164
assert_return(() => invoke($5, `get_globals`, []), [value("i32", 1234), value("i32", 5678)]);

// ./test/core/gc/i31.wast:165
invoke($5, `set_global`, [0]);

// ./test/core/gc/i31.wast:166
assert_return(() => invoke($5, `get_globals`, []), [value("i32", 1234), value("i32", 0)]);

// ./test/core/gc/i31.wast:168
let $6 = instantiate(`(module $$anyref_table_of_i31ref
  (table $$table 3 10 anyref)
  (elem (table $$table) (i32.const 0) i31ref (item (ref.i31 (i32.const 999)))
                                            (item (ref.i31 (i32.const 888)))
                                            (item (ref.i31 (i32.const 777))))

  (func (export "size") (result i32)
    table.size $$table
  )

  (func (export "get") (param i32) (result i32)
    (i31.get_u (ref.cast i31ref (table.get $$table (local.get 0))))
  )

  (func (export "grow") (param i32 i32) (result i32)
    (table.grow $$table (ref.i31 (local.get 1)) (local.get 0))
  )

  (func (export "fill") (param i32 i32 i32)
    (table.fill $$table (local.get 0) (ref.i31 (local.get 1)) (local.get 2))
  )

  (func (export "copy") (param i32 i32 i32)
    (table.copy $$table $$table (local.get 0) (local.get 1) (local.get 2))
  )

  (elem $$elem i31ref (item (ref.i31 (i32.const 123)))
                     (item (ref.i31 (i32.const 456)))
                     (item (ref.i31 (i32.const 789))))
  (func (export "init") (param i32 i32 i32)
    (table.init $$table $$elem (local.get 0) (local.get 1) (local.get 2))
  )
)`);
let $anyref_table_of_i31ref = $6;

// ./test/core/gc/i31.wast:203
assert_return(() => invoke($6, `size`, []), [value("i32", 3)]);

// ./test/core/gc/i31.wast:204
assert_return(() => invoke($6, `get`, [0]), [value("i32", 999)]);

// ./test/core/gc/i31.wast:205
assert_return(() => invoke($6, `get`, [1]), [value("i32", 888)]);

// ./test/core/gc/i31.wast:206
assert_return(() => invoke($6, `get`, [2]), [value("i32", 777)]);

// ./test/core/gc/i31.wast:209
assert_return(() => invoke($6, `grow`, [2, 333]), [value("i32", 3)]);

// ./test/core/gc/i31.wast:210
assert_return(() => invoke($6, `size`, []), [value("i32", 5)]);

// ./test/core/gc/i31.wast:211
assert_return(() => invoke($6, `get`, [3]), [value("i32", 333)]);

// ./test/core/gc/i31.wast:212
assert_return(() => invoke($6, `get`, [4]), [value("i32", 333)]);

// ./test/core/gc/i31.wast:215
invoke($6, `fill`, [2, 111, 2]);

// ./test/core/gc/i31.wast:216
assert_return(() => invoke($6, `get`, [2]), [value("i32", 111)]);

// ./test/core/gc/i31.wast:217
assert_return(() => invoke($6, `get`, [3]), [value("i32", 111)]);

// ./test/core/gc/i31.wast:220
invoke($6, `copy`, [3, 0, 2]);

// ./test/core/gc/i31.wast:221
assert_return(() => invoke($6, `get`, [3]), [value("i32", 999)]);

// ./test/core/gc/i31.wast:222
assert_return(() => invoke($6, `get`, [4]), [value("i32", 888)]);

// ./test/core/gc/i31.wast:225
invoke($6, `init`, [1, 0, 3]);

// ./test/core/gc/i31.wast:226
assert_return(() => invoke($6, `get`, [1]), [value("i32", 123)]);

// ./test/core/gc/i31.wast:227
assert_return(() => invoke($6, `get`, [2]), [value("i32", 456)]);

// ./test/core/gc/i31.wast:228
assert_return(() => invoke($6, `get`, [3]), [value("i32", 789)]);
