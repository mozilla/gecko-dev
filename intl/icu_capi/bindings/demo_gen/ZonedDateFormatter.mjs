import { DateFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
import { TimeZone } from "icu4x"
import { TimeZoneInfo } from "icu4x"
import { UtcOffset } from "icu4x"
import { ZonedDateFormatter } from "icu4x"
export function formatIso(zonedDateFormatterLocaleName, zonedDateFormatterFormatterLocaleName, zonedDateFormatterFormatterLength, zonedDateFormatterFormatterAlignment, zonedDateFormatterFormatterYearStyle, dateYear, dateMonth, dateDay, zoneIdId, zoneOffsetOffset, zoneVariant) {
    
    let zonedDateFormatterLocale = Locale.fromString(zonedDateFormatterLocaleName);
    
    let zonedDateFormatterFormatterLocale = Locale.fromString(zonedDateFormatterFormatterLocaleName);
    
    let zonedDateFormatterFormatter = DateFormatter.createYmd(zonedDateFormatterFormatterLocale,zonedDateFormatterFormatterLength,zonedDateFormatterFormatterAlignment,zonedDateFormatterFormatterYearStyle);
    
    let zonedDateFormatter = ZonedDateFormatter.createGenericShort(zonedDateFormatterLocale,zonedDateFormatterFormatter);
    
    let date = new IsoDate(dateYear,dateMonth,dateDay);
    
    let zoneId = TimeZone.createFromBcp47(zoneIdId);
    
    let zoneOffset = UtcOffset.fromString(zoneOffsetOffset);
    
    let zone = new TimeZoneInfo(zoneId,zoneOffset,zoneVariant);
    
    let out = zonedDateFormatter.formatIso(date,zone);
    

    return out;
}
