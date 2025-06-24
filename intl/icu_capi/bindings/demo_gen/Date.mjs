import { Calendar } from "icu4x"
import { Date } from "icu4x"
export function monthCode(dateYear, dateMonth, dateDay, dateCalendarKind) {
    
    let dateCalendar = new Calendar(dateCalendarKind);
    
    let date = Date.fromIsoInCalendar(dateYear,dateMonth,dateDay,dateCalendar);
    
    let out = date.monthCode;
    

    return out;
}
export function era(dateYear, dateMonth, dateDay, dateCalendarKind) {
    
    let dateCalendar = new Calendar(dateCalendarKind);
    
    let date = Date.fromIsoInCalendar(dateYear,dateMonth,dateDay,dateCalendar);
    
    let out = date.era;
    

    return out;
}
