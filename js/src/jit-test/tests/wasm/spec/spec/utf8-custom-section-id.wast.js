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

// ./test/core/utf8-custom-section-id.wast

// ./test/core/utf8-custom-section-id.wast:6
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\80"                       ;; "\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:16
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\8f"                       ;; "\\8f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:26
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\90"                       ;; "\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:36
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\9f"                       ;; "\\9f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:46
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\a0"                       ;; "\\a0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:56
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\bf"                       ;; "\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:68
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\c2\\80\\80"                 ;; "\\c2\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:78
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\c2"                       ;; "\\c2"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:88
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c2\\2e"                    ;; "\\c2."
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:100
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c0\\80"                    ;; "\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:110
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c0\\bf"                    ;; "\\c0\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:120
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c1\\80"                    ;; "\\c1\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:130
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c1\\bf"                    ;; "\\c1\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:140
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c2\\00"                    ;; "\\c2\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:150
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c2\\7f"                    ;; "\\c2\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:160
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c2\\c0"                    ;; "\\c2\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:170
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\c2\\fd"                    ;; "\\c2\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:180
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\df\\00"                    ;; "\\df\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:190
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\df\\7f"                    ;; "\\df\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:200
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\df\\c0"                    ;; "\\df\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:210
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\df\\fd"                    ;; "\\df\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:222
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\e1\\80\\80\\80"              ;; "\\e1\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:232
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\e1\\80"                    ;; "\\e1\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:242
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\80\\2e"                 ;; "\\e1\\80."
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:252
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\e1"                       ;; "\\e1"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:262
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\e1\\2e"                    ;; "\\e1."
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:274
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\00\\a0"                 ;; "\\e0\\00\\a0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:284
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\7f\\a0"                 ;; "\\e0\\7f\\a0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:294
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\80\\80"                 ;; "\\e0\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:304
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\80\\a0"                 ;; "\\e0\\80\\a0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:314
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\9f\\a0"                 ;; "\\e0\\9f\\a0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:324
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\9f\\bf"                 ;; "\\e0\\9f\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:334
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\c0\\a0"                 ;; "\\e0\\c0\\a0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:344
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\fd\\a0"                 ;; "\\e0\\fd\\a0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:354
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\00\\80"                 ;; "\\e1\\00\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:364
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\7f\\80"                 ;; "\\e1\\7f\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:374
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\c0\\80"                 ;; "\\e1\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:384
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\fd\\80"                 ;; "\\e1\\fd\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:394
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ec\\00\\80"                 ;; "\\ec\\00\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:404
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ec\\7f\\80"                 ;; "\\ec\\7f\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:414
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ec\\c0\\80"                 ;; "\\ec\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:424
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ec\\fd\\80"                 ;; "\\ec\\fd\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:434
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\00\\80"                 ;; "\\ed\\00\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:444
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\7f\\80"                 ;; "\\ed\\7f\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:454
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\a0\\80"                 ;; "\\ed\\a0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:464
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\a0\\bf"                 ;; "\\ed\\a0\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:474
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\bf\\80"                 ;; "\\ed\\bf\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:484
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\bf\\bf"                 ;; "\\ed\\bf\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:494
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\c0\\80"                 ;; "\\ed\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:504
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\fd\\80"                 ;; "\\ed\\fd\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:514
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ee\\00\\80"                 ;; "\\ee\\00\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:524
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ee\\7f\\80"                 ;; "\\ee\\7f\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:534
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ee\\c0\\80"                 ;; "\\ee\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:544
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ee\\fd\\80"                 ;; "\\ee\\fd\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:554
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ef\\00\\80"                 ;; "\\ef\\00\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:564
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ef\\7f\\80"                 ;; "\\ef\\7f\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:574
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ef\\c0\\80"                 ;; "\\ef\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:584
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ef\\fd\\80"                 ;; "\\ef\\fd\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:596
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\a0\\00"                 ;; "\\e0\\a0\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:606
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\a0\\7f"                 ;; "\\e0\\a0\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:616
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\a0\\c0"                 ;; "\\e0\\a0\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:626
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e0\\a0\\fd"                 ;; "\\e0\\a0\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:636
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\80\\00"                 ;; "\\e1\\80\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:646
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\80\\7f"                 ;; "\\e1\\80\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:656
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\80\\c0"                 ;; "\\e1\\80\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:666
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\e1\\80\\fd"                 ;; "\\e1\\80\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:676
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ec\\80\\00"                 ;; "\\ec\\80\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:686
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ec\\80\\7f"                 ;; "\\ec\\80\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:696
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ec\\80\\c0"                 ;; "\\ec\\80\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:706
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ec\\80\\fd"                 ;; "\\ec\\80\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:716
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\80\\00"                 ;; "\\ed\\80\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:726
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\80\\7f"                 ;; "\\ed\\80\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:736
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\80\\c0"                 ;; "\\ed\\80\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:746
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ed\\80\\fd"                 ;; "\\ed\\80\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:756
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ee\\80\\00"                 ;; "\\ee\\80\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:766
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ee\\80\\7f"                 ;; "\\ee\\80\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:776
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ee\\80\\c0"                 ;; "\\ee\\80\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:786
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ee\\80\\fd"                 ;; "\\ee\\80\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:796
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ef\\80\\00"                 ;; "\\ef\\80\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:806
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ef\\80\\7f"                 ;; "\\ef\\80\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:816
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ef\\80\\c0"                 ;; "\\ef\\80\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:826
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\ef\\80\\fd"                 ;; "\\ef\\80\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:838
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\06"                       ;; custom section
    "\\05\\f1\\80\\80\\80\\80"           ;; "\\f1\\80\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:848
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\f1\\80\\80"                 ;; "\\f1\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:858
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\80\\23"              ;; "\\f1\\80\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:868
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\f1\\80"                    ;; "\\f1\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:878
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\f1\\80\\23"                 ;; "\\f1\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:888
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\f1"                       ;; "\\f1"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:898
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\f1\\23"                    ;; "\\f1#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:910
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\00\\90\\90"              ;; "\\f0\\00\\90\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:920
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\7f\\90\\90"              ;; "\\f0\\7f\\90\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:930
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\80\\80\\80"              ;; "\\f0\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:940
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\80\\90\\90"              ;; "\\f0\\80\\90\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:950
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\8f\\90\\90"              ;; "\\f0\\8f\\90\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:960
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\8f\\bf\\bf"              ;; "\\f0\\8f\\bf\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:970
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\c0\\90\\90"              ;; "\\f0\\c0\\90\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:980
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\fd\\90\\90"              ;; "\\f0\\fd\\90\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:990
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\00\\80\\80"              ;; "\\f1\\00\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1000
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\7f\\80\\80"              ;; "\\f1\\7f\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1010
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\c0\\80\\80"              ;; "\\f1\\c0\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1020
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\fd\\80\\80"              ;; "\\f1\\fd\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1030
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\00\\80\\80"              ;; "\\f3\\00\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1040
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\7f\\80\\80"              ;; "\\f3\\7f\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1050
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\c0\\80\\80"              ;; "\\f3\\c0\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1060
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\fd\\80\\80"              ;; "\\f3\\fd\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1070
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\00\\80\\80"              ;; "\\f4\\00\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1080
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\7f\\80\\80"              ;; "\\f4\\7f\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1090
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\90\\80\\80"              ;; "\\f4\\90\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1100
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\bf\\80\\80"              ;; "\\f4\\bf\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1110
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\c0\\80\\80"              ;; "\\f4\\c0\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1120
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\fd\\80\\80"              ;; "\\f4\\fd\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1130
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f5\\80\\80\\80"              ;; "\\f5\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1140
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f7\\80\\80\\80"              ;; "\\f7\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1150
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f7\\bf\\bf\\bf"              ;; "\\f7\\bf\\bf\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1162
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\90\\00\\90"              ;; "\\f0\\90\\00\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1172
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\90\\7f\\90"              ;; "\\f0\\90\\7f\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1182
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\90\\c0\\90"              ;; "\\f0\\90\\c0\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1192
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\90\\fd\\90"              ;; "\\f0\\90\\fd\\90"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1202
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\00\\80"              ;; "\\f1\\80\\00\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1212
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\7f\\80"              ;; "\\f1\\80\\7f\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1222
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\c0\\80"              ;; "\\f1\\80\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1232
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\fd\\80"              ;; "\\f1\\80\\fd\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1242
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\80\\00\\80"              ;; "\\f3\\80\\00\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1252
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\80\\7f\\80"              ;; "\\f3\\80\\7f\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1262
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\80\\c0\\80"              ;; "\\f3\\80\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1272
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\80\\fd\\80"              ;; "\\f3\\80\\fd\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1282
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\80\\00\\80"              ;; "\\f4\\80\\00\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1292
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\80\\7f\\80"              ;; "\\f4\\80\\7f\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1302
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\80\\c0\\80"              ;; "\\f4\\80\\c0\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1312
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\80\\fd\\80"              ;; "\\f4\\80\\fd\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1324
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\90\\90\\00"              ;; "\\f0\\90\\90\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1334
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\90\\90\\7f"              ;; "\\f0\\90\\90\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1344
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\90\\90\\c0"              ;; "\\f0\\90\\90\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1354
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f0\\90\\90\\fd"              ;; "\\f0\\90\\90\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1364
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\80\\00"              ;; "\\f1\\80\\80\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1374
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\80\\7f"              ;; "\\f1\\80\\80\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1384
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\80\\c0"              ;; "\\f1\\80\\80\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1394
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f1\\80\\80\\fd"              ;; "\\f1\\80\\80\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1404
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\80\\80\\00"              ;; "\\f3\\80\\80\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1414
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\80\\80\\7f"              ;; "\\f3\\80\\80\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1424
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\80\\80\\c0"              ;; "\\f3\\80\\80\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1434
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f3\\80\\80\\fd"              ;; "\\f3\\80\\80\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1444
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\80\\80\\00"              ;; "\\f4\\80\\80\\00"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1454
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\80\\80\\7f"              ;; "\\f4\\80\\80\\7f"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1464
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\80\\80\\c0"              ;; "\\f4\\80\\80\\c0"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1474
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f4\\80\\80\\fd"              ;; "\\f4\\80\\80\\fd"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1486
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\07"                       ;; custom section
    "\\06\\f8\\80\\80\\80\\80\\80"        ;; "\\f8\\80\\80\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1496
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f8\\80\\80\\80"              ;; "\\f8\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1506
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\06"                       ;; custom section
    "\\05\\f8\\80\\80\\80\\23"           ;; "\\f8\\80\\80\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1516
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\f8\\80\\80"                 ;; "\\f8\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1526
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\f8\\80\\80\\23"              ;; "\\f8\\80\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1536
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\f8\\80"                    ;; "\\f8\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1546
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\f8\\80\\23"                 ;; "\\f8\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1556
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\f8"                       ;; "\\f8"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1566
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\f8\\23"                    ;; "\\f8#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1578
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\06"                       ;; custom section
    "\\05\\f8\\80\\80\\80\\80"           ;; "\\f8\\80\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1588
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\06"                       ;; custom section
    "\\05\\fb\\bf\\bf\\bf\\bf"           ;; "\\fb\\bf\\bf\\bf\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1600
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\08"                       ;; custom section
    "\\07\\fc\\80\\80\\80\\80\\80\\80"     ;; "\\fc\\80\\80\\80\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1610
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\06"                       ;; custom section
    "\\05\\fc\\80\\80\\80\\80"           ;; "\\fc\\80\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1620
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\07"                       ;; custom section
    "\\06\\fc\\80\\80\\80\\80\\23"        ;; "\\fc\\80\\80\\80\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1630
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\fc\\80\\80\\80"              ;; "\\fc\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1640
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\06"                       ;; custom section
    "\\05\\fc\\80\\80\\80\\23"           ;; "\\fc\\80\\80\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1650
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\fc\\80\\80"                 ;; "\\fc\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1660
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\fc\\80\\80\\23"              ;; "\\fc\\80\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1670
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\fc\\80"                    ;; "\\fc\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1680
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\04"                       ;; custom section
    "\\03\\fc\\80\\23"                 ;; "\\fc\\80#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1690
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\fc"                       ;; "\\fc"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1700
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\fc\\23"                    ;; "\\fc#"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1712
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\07"                       ;; custom section
    "\\06\\fc\\80\\80\\80\\80\\80"        ;; "\\fc\\80\\80\\80\\80\\80"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1722
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\07"                       ;; custom section
    "\\06\\fd\\bf\\bf\\bf\\bf\\bf"        ;; "\\fd\\bf\\bf\\bf\\bf\\bf"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1734
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\fe"                       ;; "\\fe"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1744
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\02"                       ;; custom section
    "\\01\\ff"                       ;; "\\ff"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1754
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\fe\\ff"                    ;; "\\fe\\ff"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1764
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\00\\00\\fe\\ff"              ;; "\\00\\00\\fe\\ff"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1774
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\03"                       ;; custom section
    "\\02\\ff\\fe"                    ;; "\\ff\\fe"
  )`),
  `malformed UTF-8 encoding`,
);

// ./test/core/utf8-custom-section-id.wast:1784
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\05"                       ;; custom section
    "\\04\\ff\\fe\\00\\00"              ;; "\\ff\\fe\\00\\00"
  )`),
  `malformed UTF-8 encoding`,
);
