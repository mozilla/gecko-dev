import { DateFormatterGregorian } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
export function formatIso(dateFormatterGregorianLocaleName, dateFormatterGregorianLength, dateFormatterGregorianAlignment, dateFormatterGregorianYearStyle, dateYear, dateMonth, dateDay) {
    
    let dateFormatterGregorianLocale = Locale.fromString(dateFormatterGregorianLocaleName);
    
    let dateFormatterGregorian = DateFormatterGregorian.createYmd(dateFormatterGregorianLocale,dateFormatterGregorianLength,dateFormatterGregorianAlignment,dateFormatterGregorianYearStyle);
    
    let date = new IsoDate(dateYear,dateMonth,dateDay);
    
    let out = dateFormatterGregorian.formatIso(date);
    

    return out;
}
