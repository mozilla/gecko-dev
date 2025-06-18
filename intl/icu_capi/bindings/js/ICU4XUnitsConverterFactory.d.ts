import { FFIError } from "./diplomat-runtime"
import { ICU4XDataProvider } from "./ICU4XDataProvider";
import { ICU4XError } from "./ICU4XError";
import { ICU4XMeasureUnit } from "./ICU4XMeasureUnit";
import { ICU4XMeasureUnitParser } from "./ICU4XMeasureUnitParser";
import { ICU4XUnitsConverter } from "./ICU4XUnitsConverter";

/**

 * An ICU4X Units Converter Factory object, capable of creating converters a {@link ICU4XUnitsConverter `ICU4XUnitsConverter`} for converting between two {@link ICU4XMeasureUnit `ICU4XMeasureUnit`}s. Also, it can parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the {@link ICU4XMeasureUnit `ICU4XMeasureUnit`}.

 * See the {@link https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html Rust documentation for `ConverterFactory`} for more information.
 */
export class ICU4XUnitsConverterFactory {

  /**

   * Construct a new {@link ICU4XUnitsConverterFactory `ICU4XUnitsConverterFactory`} instance.

   * See the {@link https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.new Rust documentation for `new`} for more information.
   * @throws {@link FFIError}<{@link ICU4XError}>
   */
  static create(provider: ICU4XDataProvider): ICU4XUnitsConverterFactory | never;

  /**

   * Creates a new {@link ICU4XUnitsConverter `ICU4XUnitsConverter`} from the input and output {@link ICU4XMeasureUnit `ICU4XMeasureUnit`}s. Returns nothing if the conversion between the two units is not possible. For example, conversion between `meter` and `second` is not possible.

   * See the {@link https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.converter Rust documentation for `converter`} for more information.
   */
  converter(from: ICU4XMeasureUnit, to: ICU4XMeasureUnit): ICU4XUnitsConverter | undefined;

  /**

   * Creates a parser to parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the {@link ICU4XMeasureUnit `ICU4XMeasureUnit`}.

   * See the {@link https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.parser Rust documentation for `parser`} for more information.
   */
  parser(): ICU4XMeasureUnitParser;
}
