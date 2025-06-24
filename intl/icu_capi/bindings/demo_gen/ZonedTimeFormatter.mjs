import { Locale } from "icu4x"
import { Time } from "icu4x"
import { TimeZone } from "icu4x"
import { TimeZoneInfo } from "icu4x"
import { UtcOffset } from "icu4x"
import { ZonedTimeFormatter } from "icu4x"
export function format(zonedTimeFormatterLocaleName, zonedTimeFormatterLength, zonedTimeFormatterTimePrecision, zonedTimeFormatterAlignment, timeHour, timeMinute, timeSecond, timeSubsecond, zoneIdId, zoneOffsetOffset, zoneVariant) {
    
    let zonedTimeFormatterLocale = Locale.fromString(zonedTimeFormatterLocaleName);
    
    let zonedTimeFormatter = ZonedTimeFormatter.createGenericShort(zonedTimeFormatterLocale,zonedTimeFormatterLength,zonedTimeFormatterTimePrecision,zonedTimeFormatterAlignment);
    
    let time = new Time(timeHour,timeMinute,timeSecond,timeSubsecond);
    
    let zoneId = TimeZone.createFromBcp47(zoneIdId);
    
    let zoneOffset = UtcOffset.fromString(zoneOffsetOffset);
    
    let zone = new TimeZoneInfo(zoneId,zoneOffset,zoneVariant);
    
    let out = zonedTimeFormatter.format(time,zone);
    

    return out;
}
