import { Calendar } from "icu4x"
import { Date } from "icu4x"
import { DateFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
export function formatIso(dateFormatterLocaleName, dateFormatterLength, dateFormatterAlignment, dateFormatterYearStyle, dateYear, dateMonth, dateDay) {
    
    let dateFormatterLocale = Locale.fromString(dateFormatterLocaleName);
    
    let dateFormatter = DateFormatter.createYmd(dateFormatterLocale,dateFormatterLength,dateFormatterAlignment,dateFormatterYearStyle);
    
    let date = new IsoDate(dateYear,dateMonth,dateDay);
    
    let out = dateFormatter.formatIso(date);
    

    return out;
}
export function formatSameCalendar(dateFormatterLocaleName, dateFormatterLength, dateFormatterAlignment, dateFormatterYearStyle, dateYear, dateMonth, dateDay, dateCalendarKind) {
    
    let dateFormatterLocale = Locale.fromString(dateFormatterLocaleName);
    
    let dateFormatter = DateFormatter.createYmd(dateFormatterLocale,dateFormatterLength,dateFormatterAlignment,dateFormatterYearStyle);
    
    let dateCalendar = new Calendar(dateCalendarKind);
    
    let date = Date.fromIsoInCalendar(dateYear,dateMonth,dateDay,dateCalendar);
    
    let out = dateFormatter.formatSameCalendar(date);
    

    return out;
}
