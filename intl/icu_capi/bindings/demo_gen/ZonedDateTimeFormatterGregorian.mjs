import { DateTimeFormatterGregorian } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
import { Time } from "icu4x"
import { TimeZone } from "icu4x"
import { TimeZoneInfo } from "icu4x"
import { UtcOffset } from "icu4x"
import { ZonedDateTimeFormatterGregorian } from "icu4x"
export function formatIso(zonedDateTimeFormatterGregorianLocaleName, zonedDateTimeFormatterGregorianFormatterLocaleName, zonedDateTimeFormatterGregorianFormatterLength, zonedDateTimeFormatterGregorianFormatterTimePrecision, zonedDateTimeFormatterGregorianFormatterAlignment, zonedDateTimeFormatterGregorianFormatterYearStyle, dateYear, dateMonth, dateDay, timeHour, timeMinute, timeSecond, timeSubsecond, zoneIdId, zoneOffsetOffset, zoneVariant) {
    
    let zonedDateTimeFormatterGregorianLocale = Locale.fromString(zonedDateTimeFormatterGregorianLocaleName);
    
    let zonedDateTimeFormatterGregorianFormatterLocale = Locale.fromString(zonedDateTimeFormatterGregorianFormatterLocaleName);
    
    let zonedDateTimeFormatterGregorianFormatter = DateTimeFormatterGregorian.createYmdt(zonedDateTimeFormatterGregorianFormatterLocale,zonedDateTimeFormatterGregorianFormatterLength,zonedDateTimeFormatterGregorianFormatterTimePrecision,zonedDateTimeFormatterGregorianFormatterAlignment,zonedDateTimeFormatterGregorianFormatterYearStyle);
    
    let zonedDateTimeFormatterGregorian = ZonedDateTimeFormatterGregorian.createGenericShort(zonedDateTimeFormatterGregorianLocale,zonedDateTimeFormatterGregorianFormatter);
    
    let date = new IsoDate(dateYear,dateMonth,dateDay);
    
    let time = new Time(timeHour,timeMinute,timeSecond,timeSubsecond);
    
    let zoneId = TimeZone.createFromBcp47(zoneIdId);
    
    let zoneOffset = UtcOffset.fromString(zoneOffsetOffset);
    
    let zone = new TimeZoneInfo(zoneId,zoneOffset,zoneVariant);
    
    let out = zonedDateTimeFormatterGregorian.formatIso(date,time,zone);
    

    return out;
}
