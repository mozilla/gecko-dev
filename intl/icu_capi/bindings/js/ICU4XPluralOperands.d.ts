import { FFIError } from "./diplomat-runtime"
import { ICU4XError } from "./ICU4XError";
import { ICU4XFixedDecimal } from "./ICU4XFixedDecimal";

/**

 * See the {@link https://docs.rs/icu/latest/icu/plurals/struct.PluralOperands.html Rust documentation for `PluralOperands`} for more information.
 */
export class ICU4XPluralOperands {

  /**

   * Construct for a given string representing a number

   * See the {@link https://docs.rs/icu/latest/icu/plurals/struct.PluralOperands.html#method.from_str Rust documentation for `from_str`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create_from_string(s: string): ICU4XPluralOperands | never;

  /**

   * Construct from a FixedDecimal

   * Retains at most 18 digits each from the integer and fraction parts.
   */
  static create_from_fixed_decimal(x: ICU4XFixedDecimal): ICU4XPluralOperands;
}
