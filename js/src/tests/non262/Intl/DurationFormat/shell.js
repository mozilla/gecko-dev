function GenericPartCreator(type) {
  return str => ({ type, value: str });
}

const NumberFormatParts = {
  Nan: GenericPartCreator("nan"),
  Inf: GenericPartCreator("infinity"),
  Integer: GenericPartCreator("integer"),
  Group: GenericPartCreator("group"),
  Decimal: GenericPartCreator("decimal"),
  Fraction: GenericPartCreator("fraction"),
  MinusSign: GenericPartCreator("minusSign"),
  PlusSign: GenericPartCreator("plusSign"),
  PercentSign: GenericPartCreator("percentSign"),
  Currency: GenericPartCreator("currency"),
  Literal: GenericPartCreator("literal"),
  ExponentSeparator: GenericPartCreator("exponentSeparator"),
  ExponentMinusSign: GenericPartCreator("exponentMinusSign"),
  ExponentInteger: GenericPartCreator("exponentInteger"),
  Compact: GenericPartCreator("compact"),
  Unit: GenericPartCreator("unit"),
};

function GenericPartsCreator(unit) {
  return (...values) => values.map(value => ({...value, unit}));
}

const DurationFormatParts = {
  Year: GenericPartsCreator("year"),
  Month: GenericPartsCreator("month"),
  Week: GenericPartsCreator("week"),
  Day: GenericPartsCreator("day"),
  Hour: GenericPartsCreator("hour"),
  Minute: GenericPartsCreator("minute"),
  Second: GenericPartsCreator("second"),
  Millisecond: GenericPartsCreator("millisecond"),
  Microsecond: GenericPartsCreator("microsecond"),
  Nanosecond: GenericPartsCreator("nanosecond"),
};

function PartsToString(parts) {
  return parts.reduce((acc, {value}) => acc + value, "");
}

const units = [
  "years", "months", "weeks", "days",
  "hours", "minutes", "seconds",
  "milliseconds", "microseconds", "nanoseconds",
];
