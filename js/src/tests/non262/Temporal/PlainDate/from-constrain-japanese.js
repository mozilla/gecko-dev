// |reftest| skip-if(!this.hasOwnProperty("Temporal"))

const eras = [
  {
    name: "meiji",
    start: "1868-09-08",
    end: "1912-07-29",
  },
  {
    name: "taisho",
    start: "1912-07-30",
    end: "1926-12-24",
  },
  {
    name: "showa",
    start: "1926-12-25",
    end: "1989-01-07",
  },
  {
    name: "heisei",
    start: "1989-01-08",
    end: "2019-04-30",
  },
  {
    name: "reiwa",
    start: "2019-05-01",
    end: undefined,
  },
];

for (let {name: era, start, end} of eras) {
  let eraStart = Temporal.PlainDate.from(start);
  let eraEnd = end && Temporal.PlainDate.from(end);

  // Months before start of era.
  for (let month = 1; month < eraStart.month; ++month) {
    for (let day = 1; day <= 32; ++day) {
      let monthCode = "M" + String(month).padStart(2, "0");

      let dateWithMonth = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 1,
        month,
        day,
      });

      let dateWithMonthCode = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 1,
        monthCode,
        day,
      });

      assertEq(dateWithMonth.toString({calendarName: "never"}), start);
      assertEq(dateWithMonthCode.toString({calendarName: "never"}), start);
    }
  }

  // Days before start of era.
  for (let day = 1; day < eraStart.day; ++day) {
    let dateWithMonth = Temporal.PlainDate.from({
      calendar: "japanese",
      era,
      eraYear: 1,
      month: eraStart.month,
      day,
    });

    let dateWithMonthCode = Temporal.PlainDate.from({
      calendar: "japanese",
      era,
      eraYear: 1,
      monthCode: eraStart.monthCode,
      day,
    });

    assertEq(dateWithMonth.toString({calendarName: "never"}), start);
    assertEq(dateWithMonthCode.toString({calendarName: "never"}), start);
  }

  // After start of era.
  for (let day = eraStart.day; day <= 32; ++day) {
    let dateWithMonth = Temporal.PlainDate.from({
      calendar: "japanese",
      era,
      eraYear: 1,
      month: eraStart.month,
      day,
    });

    let dateWithMonthCode = Temporal.PlainDate.from({
      calendar: "japanese",
      era,
      eraYear: 1,
      monthCode: eraStart.monthCode,
      day,
    });

    let expected = Temporal.PlainDate.from({
      year: eraStart.year,
      monthCode: eraStart.monthCode,
      day,
    }).toString();

    assertEq(dateWithMonth.toString({calendarName: "never"}), expected);
    assertEq(dateWithMonthCode.toString({calendarName: "never"}), expected);
  }
  
  // Months after start of era.
  for (let month = eraStart.month + 1; month <= 12; ++month) {
    for (let day = 1; day <= 32; ++day) {
      let monthCode = "M" + String(month).padStart(2, "0");

      let dateWithMonth = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 1,
        month,
        day,
      });

      let dateWithMonthCode = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 1,
        monthCode,
        day,
      });

      let expected = Temporal.PlainDate.from({
        year: eraStart.year,
        monthCode: monthCode,
        day,
      }).toString();

      assertEq(dateWithMonth.toString({calendarName: "never"}), expected);
      assertEq(dateWithMonthCode.toString({calendarName: "never"}), expected);
    }
  }

  if (end) {
    let lastEraYear = (eraEnd.year - eraStart.year) + 1;
    
    // After end of era.
    for (let day = 31; day <= 32; ++day) {
      let date = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: 100,
        monthCode: "M12",
        day,
      });
      assertEq(date.toString({calendarName: "never"}), end);
    }

    // Days after end of era.
    for (let day = eraEnd.day + 1; day <= 32; ++day) {
      let dateWithMonth = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: lastEraYear,
        month: eraEnd.month,
        day,
      });
    
      let dateWithMonthCode = Temporal.PlainDate.from({
        calendar: "japanese",
        era,
        eraYear: lastEraYear,
        monthCode: eraEnd.monthCode,
        day,
      });
    
      assertEq(dateWithMonth.toString({calendarName: "never"}), end);
      assertEq(dateWithMonthCode.toString({calendarName: "never"}), end);
    }
  
    // Months after end of era.
    for (let month = eraEnd.month + 1; month <= 12; ++month) {
      for (let day = 1; day <= 32; ++day) {
        let monthCode = "M" + String(month).padStart(2, "0");

        let dateWithMonth = Temporal.PlainDate.from({
          calendar: "japanese",
          era,
          eraYear: lastEraYear,
          month,
          day,
        });
    
        let dateWithMonthCode = Temporal.PlainDate.from({
          calendar: "japanese",
          era,
          eraYear: lastEraYear,
          monthCode,
          day,
        });
        
        assertEq(dateWithMonth.toString({calendarName: "never"}), end);
        assertEq(dateWithMonthCode.toString({calendarName: "never"}), end);
      }
    }
    
    // Year after end of era.
    let yearAfterLastEraYear = lastEraYear + 1;
    for (let month = 1; month <= 12; ++month) {
      for (let day = 1; day <= 31; ++day) {
        let monthCode = "M" + String(month).padStart(2, "0");

        let dateWithMonth = Temporal.PlainDate.from({
          calendar: "japanese",
          era,
          eraYear: yearAfterLastEraYear,
          month,
          day,
        });

        let dateWithMonthCode = Temporal.PlainDate.from({
          calendar: "japanese",
          era,
          eraYear: yearAfterLastEraYear,
          monthCode,
          day,
        });

        assertEq(dateWithMonth.toString({calendarName: "never"}), end, `era=${era}, eraYear=${yearAfterLastEraYear}, month=${month}, day=${day}`);
        assertEq(dateWithMonthCode.toString({calendarName: "never"}), end, `era=${era}, eraYear=${yearAfterLastEraYear}, monthCode=${monthCode}, day=${day}`);
      }
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
