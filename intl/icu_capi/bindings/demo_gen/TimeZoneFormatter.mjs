import { Locale } from "icu4x"
import { TimeZone } from "icu4x"
import { TimeZoneFormatter } from "icu4x"
import { TimeZoneInfo } from "icu4x"
import { UtcOffset } from "icu4x"
export function format(timeZoneFormatterLocaleName, zoneIdId, zoneOffsetOffset, zoneVariant) {
    
    let timeZoneFormatterLocale = Locale.fromString(timeZoneFormatterLocaleName);
    
    let timeZoneFormatter = TimeZoneFormatter.createGenericShort(timeZoneFormatterLocale);
    
    let zoneId = TimeZone.createFromBcp47(zoneIdId);
    
    let zoneOffset = UtcOffset.fromString(zoneOffsetOffset);
    
    let zone = new TimeZoneInfo(zoneId,zoneOffset,zoneVariant);
    
    let out = timeZoneFormatter.format(zone);
    

    return out;
}
