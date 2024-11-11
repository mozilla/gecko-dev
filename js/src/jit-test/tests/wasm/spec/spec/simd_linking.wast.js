// |jit-test| skip-if: !wasmSimdEnabled()

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

// ./test/core/simd/simd_linking.wast

// ./test/core/simd/simd_linking.wast:1
let $0 = instantiate(`(module
  (global (export "g-v128") v128 (v128.const i64x2 0 0))
  (global (export "mg-v128") (mut v128) (v128.const i64x2 0 0))
)`);

// ./test/core/simd/simd_linking.wast:5
register($0, `Mv128`);

// ./test/core/simd/simd_linking.wast:7
let $1 = instantiate(`(module
  ;; TODO: Reactivate once the fix for https://bugs.chromium.org/p/v8/issues/detail?id=13732
  ;; has made it to the downstream node.js that we use on CI.
  ;; (import "Mv128" "g-v128" (global v128))
  (import "Mv128" "mg-v128" (global (mut v128)))
)`);
