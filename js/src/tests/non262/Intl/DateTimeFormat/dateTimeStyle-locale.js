// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const msPerHour = 60 * 60 * 1000;
const tzOffset = 8 * msPerHour;

const date = new Date(Date.UTC(2024, 0, 1, 12, 30, 45) + tzOffset);

const timeZone = "America/Los_Angeles";

const dateStyles = {
  en: {
    full: "Monday, January 1, 2024",
    long: "January 1, 2024",
    medium: "Jan 1, 2024",
    short: "1/1/24",
  },
  fr: {
    full: "lundi 1 janvier 2024",
    long: "1 janvier 2024",
    medium: "1 janv. 2024",
    short: "01/01/2024",
  },
  de: {
    full: "Montag, 1. Januar 2024",
    long: "1. Januar 2024",
    medium: "01.01.2024",
    short: "01.01.24",
  },
  es: {
    full: "lunes, 1 de enero de 2024",
    long: "1 de enero de 2024",
    medium: "1 ene 2024",
    short: "1/1/24",
  },
  ja: {
    full: "2024年1月1日月曜日",
    long: "2024年1月1日",
    medium: "2024/01/01",
    short: "2024/01/01",
  },
  "ar-EG": {
    full: "الاثنين، ١ يناير ٢٠٢٤",
    long: "١ يناير ٢٠٢٤",
    medium: "٠١‏/٠١‏/٢٠٢٤",
    short: "١‏/١‏/٢٠٢٤",
  },
  zh: {
    full: "2024年1月1日星期一",
    long: "2024年1月1日",
    medium: "2024年1月1日",
    short: "2024/1/1",
  },
  "zh-u-ca-chinese": {
    full: "2023癸卯年十一月二十星期一",
    long: "2023癸卯年十一月二十",
    medium: "2023年十一月二十",
    short: "2023/11/20",
  },
};

const timeStyles = {
  en: {
    full: "12:30:45 PM Pacific Standard Time",
    long: "12:30:45 PM PST",
    medium: "12:30:45 PM",
    short: "12:30 PM",
  },
  fr: {
    full: "12:30:45 heure normale du Pacifique nord-américain",
    long: "12:30:45 UTC−8",
    medium: "12:30:45",
    short: "12:30",
  },
  de: {
    full: "12:30:45 Nordamerikanische Westküsten-Normalzeit",
    long: "12:30:45 GMT-8",
    medium: "12:30:45",
    short: "12:30",
  },
  es: {
    full: "12:30:45 (hora estándar del Pacífico)",
    long: "12:30:45 GMT-8",
    medium: "12:30:45",
    short: "12:30",
  },
  ja: {
    full: "12時30分45秒 アメリカ太平洋標準時",
    long: "12:30:45 GMT-8",
    medium: "12:30:45",
    short: "12:30",
  },
  "ar-EG": {
    full: "١٢:٣٠:٤٥ م توقيت المحيط الهادي الرسمي",
    long: "١٢:٣٠:٤٥ م غرينتش-٨",
    medium: "١٢:٣٠:٤٥ م",
    short: "١٢:٣٠ م",
  },
  zh: {
    full: "北美太平洋标准时间 12:30:45",
    long: "GMT-8 12:30:45",
    medium: "12:30:45",
    short: "12:30",
  },
};

for (let [locale, styles] of Object.entries(dateStyles)) {
  for (let [dateStyle, expected] of Object.entries(styles)) {
    let df = new Intl.DateTimeFormat(locale, {dateStyle, timeZone});
    assertEq(df.format(date), expected);
  }
}

for (let [locale, styles] of Object.entries(timeStyles)) {
  for (let [timeStyle, expected] of Object.entries(styles)) {
    let df = new Intl.DateTimeFormat(locale, {timeStyle, timeZone});
    assertEq(df.format(date), expected);
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, 'ok');
