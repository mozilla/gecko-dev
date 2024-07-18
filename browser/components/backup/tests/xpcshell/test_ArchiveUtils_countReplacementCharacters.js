/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ArchiveUtils } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveUtils.sys.mjs"
);

const string = "🌞🌞🌞🌞";
// F0 9F 8C 9E F0 9F 8C 9E F0 9F 8C 9E F0 9F 8C 9E
const buffer = new TextEncoder().encode(string);

const decoder = new TextDecoder();

add_task(async function test_no_issues_decoding_back() {
  let decoded = decoder.decode(buffer);

  let count = ArchiveUtils.countReplacementCharacters(decoded);

  Assert.equal(
    decoded,
    string,
    "The string should decode back to the original"
  );
  Assert.equal(count, 0, "There should have been no decoding issues");
});

add_task(async function test_3_replacements_if_cut_off_1() {
  let decoded = decoder.decode(buffer.slice(1));

  let count = ArchiveUtils.countReplacementCharacters(decoded);

  Assert.equal(
    count,
    3,
    "The three bytes of the first character should have been replaced with �"
  );
  Assert.equal(
    decoded,
    "\uFFFD\uFFFD\uFFFD🌞🌞🌞",
    "The string should retain the characters that could be decoded"
  );
});

add_task(async function test_2_replacements_if_cut_off_2() {
  let decoded = decoder.decode(buffer.slice(2));

  let count = ArchiveUtils.countReplacementCharacters(decoded);

  Assert.equal(
    count,
    2,
    "The two bytes of the first character should have been replaced with �"
  );
  Assert.equal(
    decoded,
    "\uFFFD\uFFFD🌞🌞🌞",
    "The string should retain the characters that could be decoded"
  );
});

add_task(async function test_1_replacement_if_cut_off_3() {
  let decoded = decoder.decode(buffer.slice(3));

  let count = ArchiveUtils.countReplacementCharacters(decoded);

  Assert.equal(
    count,
    1,
    "The last byte of the first character should have been replaced with �"
  );
  Assert.equal(
    decoded,
    "\uFFFD🌞🌞🌞",
    "The string should retain the characters that could be decoded"
  );
});

add_task(async function test_no_issues_cutting_off_whole_character() {
  let decoded = decoder.decode(buffer.slice(4));

  let count = ArchiveUtils.countReplacementCharacters(decoded);

  Assert.equal(count, 0, "There should be no replacement characters");
  Assert.equal(
    decoded,
    "🌞🌞🌞",
    "The string should retain the characters that could be decoded"
  );
});

add_task(async function test_robust_to_empty_string() {
  let count = ArchiveUtils.countReplacementCharacters("");

  Assert.equal(count, 0, "There should be no replacement characters");
});
