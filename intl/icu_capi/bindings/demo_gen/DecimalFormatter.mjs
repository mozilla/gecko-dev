import { Decimal } from "icu4x"
import { DecimalFormatter } from "icu4x"
import { Locale } from "icu4x"
export function format(decimalFormatterLocaleName, decimalFormatterGroupingStrategy, valueF) {
    
    let decimalFormatterLocale = Locale.fromString(decimalFormatterLocaleName);
    
    let decimalFormatter = DecimalFormatter.createWithGroupingStrategy(decimalFormatterLocale,decimalFormatterGroupingStrategy);
    
    let value = Decimal.fromNumberWithRoundTripPrecision(valueF);
    
    let out = decimalFormatter.format(value);
    

    return out;
}
