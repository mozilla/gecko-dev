import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.

 * This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47. It also supports normalizing and canonicalizing the IANA strings.

 * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapper.html Rust documentation for `TimeZoneIdMapper`} for more information.
 */
export class ICU4XTimeZoneIdMapper {

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapper.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XTimeZoneIdMapper | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.iana_to_bcp47 Rust documentation for `iana_to_bcp47`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  iana_to_bcp47(value: string): string | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.normalize_iana Rust documentation for `normalize_iana`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  normalize_iana(value: string): string | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.canonicalize_iana Rust documentation for `canonicalize_iana`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  canonicalize_iana(value: string): string | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.find_canonical_iana_from_bcp47 Rust documentation for `find_canonical_iana_from_bcp47`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  find_canonical_iana_from_bcp47(value: string): string | never;
}
