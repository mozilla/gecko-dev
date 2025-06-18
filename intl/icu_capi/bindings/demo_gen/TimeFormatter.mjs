import { Locale } from "icu4x"
import { Time } from "icu4x"
import { TimeFormatter } from "icu4x"
export function format(timeFormatterLocaleName, timeFormatterLength, timeFormatterTimePrecision, timeFormatterAlignment, timeHour, timeMinute, timeSecond, timeSubsecond) {
    
    let timeFormatterLocale = Locale.fromString(timeFormatterLocaleName);
    
    let timeFormatter = new TimeFormatter(timeFormatterLocale,timeFormatterLength,timeFormatterTimePrecision,timeFormatterAlignment);
    
    let time = new Time(timeHour,timeMinute,timeSecond,timeSubsecond);
    
    let out = timeFormatter.format(time);
    

    return out;
}
