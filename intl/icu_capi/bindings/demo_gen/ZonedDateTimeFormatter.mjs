import { DateTimeFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
import { Time } from "icu4x"
import { TimeZone } from "icu4x"
import { TimeZoneInfo } from "icu4x"
import { UtcOffset } from "icu4x"
import { ZonedDateTimeFormatter } from "icu4x"
export function formatIso(zonedDateTimeFormatterLocaleName, zonedDateTimeFormatterFormatterLocaleName, zonedDateTimeFormatterFormatterLength, zonedDateTimeFormatterFormatterTimePrecision, zonedDateTimeFormatterFormatterAlignment, zonedDateTimeFormatterFormatterYearStyle, dateYear, dateMonth, dateDay, timeHour, timeMinute, timeSecond, timeSubsecond, zoneIdId, zoneOffsetOffset, zoneVariant) {
    
    let zonedDateTimeFormatterLocale = Locale.fromString(zonedDateTimeFormatterLocaleName);
    
    let zonedDateTimeFormatterFormatterLocale = Locale.fromString(zonedDateTimeFormatterFormatterLocaleName);
    
    let zonedDateTimeFormatterFormatter = DateTimeFormatter.createYmdt(zonedDateTimeFormatterFormatterLocale,zonedDateTimeFormatterFormatterLength,zonedDateTimeFormatterFormatterTimePrecision,zonedDateTimeFormatterFormatterAlignment,zonedDateTimeFormatterFormatterYearStyle);
    
    let zonedDateTimeFormatter = ZonedDateTimeFormatter.createGenericShort(zonedDateTimeFormatterLocale,zonedDateTimeFormatterFormatter);
    
    let date = new IsoDate(dateYear,dateMonth,dateDay);
    
    let time = new Time(timeHour,timeMinute,timeSecond,timeSubsecond);
    
    let zoneId = TimeZone.createFromBcp47(zoneIdId);
    
    let zoneOffset = UtcOffset.fromString(zoneOffsetOffset);
    
    let zone = new TimeZoneInfo(zoneId,zoneOffset,zoneVariant);
    
    let out = zonedDateTimeFormatter.formatIso(date,time,zone);
    

    return out;
}
