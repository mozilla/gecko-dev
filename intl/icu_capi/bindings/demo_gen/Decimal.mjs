import { Decimal } from "icu4x"
export function toString(decimalF) {
    
    let decimal = Decimal.fromNumberWithRoundTripPrecision(decimalF);
    
    let out = decimal.toString();
    

    return out;
}
