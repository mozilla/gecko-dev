import { Calendar } from "icu4x"
import { Date } from "icu4x"
import { DateFormatter } from "icu4x"
import { IsoDate } from "icu4x"
import { Locale } from "icu4x"
export function formatIso(dateFormatterLocaleName: string, dateFormatterLength: DateTimeLength, dateFormatterAlignment: DateTimeAlignment, dateFormatterYearStyle: YearStyle, dateYear: number, dateMonth: number, dateDay: number);
export function formatSameCalendar(dateFormatterLocaleName: string, dateFormatterLength: DateTimeLength, dateFormatterAlignment: DateTimeAlignment, dateFormatterYearStyle: YearStyle, dateYear: number, dateMonth: number, dateDay: number, dateCalendarKind: CalendarKind);
