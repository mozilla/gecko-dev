import { f64 } from "./diplomat-runtime"

/**

 * An ICU4X Units Converter object, capable of converting between two {@link ICU4XMeasureUnit `ICU4XMeasureUnit`}s.

 * You can create an instance of this object using {@link ICU4XUnitsConverterFactory `ICU4XUnitsConverterFactory`} by calling the `converter` method.

 * See the {@link https://docs.rs/icu/latest/icu/experimental/units/converter/struct.UnitsConverter.html Rust documentation for `UnitsConverter`} for more information.
 */
export class ICU4XUnitsConverter {

  /**

   * Converts the input value in float from the input unit to the output unit (that have been used to create this converter). NOTE: The conversion using floating-point operations is not as accurate as the conversion using ratios.

   * See the {@link https://docs.rs/icu/latest/icu/experimental/units/converter/struct.UnitsConverter.html#method.convert Rust documentation for `convert`} for more information.
   */
  convert_f64(value: f64): f64;

  /**

   * Clones the current {@link ICU4XUnitsConverter `ICU4XUnitsConverter`} object.

   * See the {@link https://docs.rs/icu/latest/icu/experimental/units/converter/struct.UnitsConverter.html#method.clone Rust documentation for `clone`} for more information.
   */
  clone(): ICU4XUnitsConverter;
}
