import { FFIError } from "./diplomat-runtime"
import { ICU4XCollatorOptionsV1 } from "./ICU4XCollatorOptionsV1";
import { ICU4XCollatorResolvedOptionsV1 } from "./ICU4XCollatorResolvedOptionsV1";
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XLocale } from "./ICU4XLocale";
import { ICU4XOrdering } from "./ICU4XOrdering";

/**

 * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html Rust documentation for `Collator`} for more information.
 */
export class ICU4XCollator {

  /**

   * Construct a new Collator instance.

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.try_new Rust documentation for `try_new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_v1(provider: ICU4XDataProvider, locale: ICU4XLocale, options: ICU4XCollatorOptionsV1): ICU4XCollator | never;

  /**

   * Compare two strings.

   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according to the WHATWG Encoding Standard.

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare_utf8 Rust documentation for `compare_utf8`} for more information.
   */
  compare(left: string, right: string): ICU4XOrdering;

  /**

   * Compare two strings.

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare Rust documentation for `compare`} for more information.
   */
  compare_valid_utf8(left: string, right: string): ICU4XOrdering;

  /**

   * Compare two strings.

   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according to the WHATWG Encoding Standard.

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare_utf16 Rust documentation for `compare_utf16`} for more information.
   */
  compare_utf16(left: string, right: string): ICU4XOrdering;

  /**

   * The resolved options showing how the default options, the requested options, and the options from locale data were combined. None of the struct fields will have `Auto` as the value.

   * See the {@link https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.resolved_options Rust documentation for `resolved_options`} for more information.
   */
  resolved_options(): ICU4XCollatorResolvedOptionsV1;
}
