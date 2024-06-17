import { FFIError } from "./diplomat-runtime"
import { ICU4XError } from "./ICU4XError";
import { ICU4XMeasureUnit } from "./ICU4XMeasureUnit";

/**

 * An ICU4X Measurement Unit parser object which is capable of parsing the CLDR unit identifier (e.g. `meter-per-square-second`) and get the {@link ICU4XMeasureUnit `ICU4XMeasureUnit`}.

 * See the {@link https://docs.rs/icu/latest/icu/experimental/units/measureunit/struct.MeasureUnitParser.html Rust documentation for `MeasureUnitParser`} for more information.
 */
export class ICU4XMeasureUnitParser {

  /**

   * Parses the CLDR unit identifier (e.g. `meter-per-square-second`) and returns the corresponding {@link ICU4XMeasureUnit `ICU4XMeasureUnit`}. Returns an error if the unit identifier is not valid.

   * See the {@link https://docs.rs/icu/latest/icu/experimental/units/measureunit/struct.MeasureUnitParser.html#method.parse Rust documentation for `parse`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  parse(unit_id: string): ICU4XMeasureUnit | never;
}
