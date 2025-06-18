
/**

 * Documents which days of the week are considered to be a part of the weekend

 * See the {@link https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#method.weekend Rust documentation for `weekend`} for more information.
 */
export class ICU4XWeekendContainsDay {
  monday: boolean;
  tuesday: boolean;
  wednesday: boolean;
  thursday: boolean;
  friday: boolean;
  saturday: boolean;
  sunday: boolean;
}
