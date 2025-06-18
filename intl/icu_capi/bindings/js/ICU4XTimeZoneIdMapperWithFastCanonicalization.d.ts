import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";

/**

 * A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.

 * This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47. It also supports normalizing and canonicalizing the IANA strings.

 * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalization.html Rust documentation for `TimeZoneIdMapperWithFastCanonicalization`} for more information.
 */
export class ICU4XTimeZoneIdMapperWithFastCanonicalization {

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalization.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XTimeZoneIdMapperWithFastCanonicalization | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalizationBorrowed.html#method.canonicalize_iana Rust documentation for `canonicalize_iana`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  canonicalize_iana(value: string): string | never;

  /**

   * See the {@link https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalizationBorrowed.html#method.canonical_iana_from_bcp47 Rust documentation for `canonical_iana_from_bcp47`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  canonical_iana_from_bcp47(value: string): string | never;
}
