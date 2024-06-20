// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use crate::fixtures::structs::DateFixture;
use criterion::{
    black_box, criterion_group, criterion_main, measurement::WallTime, BenchmarkGroup, Criterion,
};
use icu_calendar::{AsCalendar, Calendar, Date, DateDuration};

fn bench_date<A: AsCalendar>(date: &mut Date<A>) {
    // black_box used to avoid compiler optimization.
    // Arithmetic
    date.add(DateDuration::new(
        black_box(1),
        black_box(2),
        black_box(3),
        black_box(4),
    ));

    // Retrieving vals
    let _ = black_box(date.year().number);
    let _ = black_box(date.month().ordinal);
    let _ = black_box(date.day_of_month().0);

    // Conversion to ISO.
    let _ = black_box(date.to_iso());
}

fn bench_calendar<C: Clone + Calendar>(
    group: &mut BenchmarkGroup<WallTime>,
    name: &str,
    fxs: &DateFixture,
    calendar: C,
    calendar_date_init: impl Fn(i32, u8, u8) -> Date<C>,
) {
    group.bench_function(name, |b| {
        b.iter(|| {
            for fx in &fxs.0 {
                // Instantion from int
                let mut instantiated_date_calendar = calendar_date_init(fx.year, fx.month, fx.day);

                // Conversion from ISO
                let date_iso = Date::try_new_iso_date(fx.year, fx.month, fx.day).unwrap();
                let mut converted_date_calendar = Date::new_from_iso(date_iso, calendar.clone());

                bench_date(&mut instantiated_date_calendar);
                bench_date(&mut converted_date_calendar);
            }
        })
    });
}

fn date_benches(c: &mut Criterion) {
    let mut group = c.benchmark_group("date");
    let fxs = fixtures::get_dates_fixture().unwrap();

    bench_calendar(
        &mut group,
        "calendar/overview",
        &fxs,
        icu::calendar::iso::Iso,
        |y, m, d| Date::try_new_iso_date(y, m, d).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/buddhist",
        &fxs,
        icu::calendar::buddhist::Buddhist,
        |y, m, d| Date::try_new_buddhist_date(y, m, d).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/coptic",
        &fxs,
        icu::calendar::coptic::Coptic,
        |y, m, d| Date::try_new_coptic_date(y, m, d).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/ethiopic",
        &fxs,
        icu::calendar::ethiopian::Ethiopian::new(),
        |y, m, d| {
            Date::try_new_ethiopian_date(
                icu::calendar::ethiopian::EthiopianEraStyle::AmeteMihret,
                y,
                m,
                d,
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/indian",
        &fxs,
        icu::calendar::indian::Indian,
        |y, m, d| Date::try_new_indian_date(y, m, d).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/persian",
        &fxs,
        icu::calendar::persian::Persian,
        |y, m, d| Date::try_new_persian_date(y, m, d).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/roc",
        &fxs,
        icu::calendar::roc::Roc,
        |y, m, d| Date::try_new_roc_date(y, m, d).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/julian",
        &fxs,
        icu::calendar::julian::Julian,
        |y, m, d| Date::try_new_julian_date(y, m, d).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/chinese",
        &fxs,
        icu::calendar::chinese::Chinese::new_always_calculating(),
        |y, m, d| {
            Date::try_new_chinese_date_with_calendar(
                y,
                m,
                d,
                icu::calendar::chinese::Chinese::new_always_calculating(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/dangi",
        &fxs,
        icu::calendar::dangi::Dangi::new_always_calculating(),
        |y, m, d| {
            Date::try_new_dangi_date_with_calendar(
                y,
                m,
                d,
                icu::calendar::dangi::Dangi::new_always_calculating(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/hebrew",
        &fxs,
        icu::calendar::hebrew::Hebrew::new_always_calculating(),
        |y, m, d| {
            Date::try_new_hebrew_date_with_calendar(
                y,
                m,
                d,
                icu::calendar::hebrew::Hebrew::new_always_calculating(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/gregorian",
        &fxs,
        icu::calendar::gregorian::Gregorian,
        |y, m, d| Date::try_new_gregorian_date(y, m, d).unwrap(),
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/islamic/civil",
        &fxs,
        icu::calendar::islamic::IslamicCivil::new_always_calculating(),
        |y, m, d| {
            Date::try_new_islamic_civil_date_with_calendar(
                y,
                m,
                d,
                icu::calendar::islamic::IslamicCivil::new_always_calculating(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/islamic/tabular",
        &fxs,
        icu::calendar::islamic::IslamicTabular::new_always_calculating(),
        |y, m, d| {
            Date::try_new_islamic_tabular_date_with_calendar(
                y,
                m,
                d,
                icu::calendar::islamic::IslamicTabular::new_always_calculating(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/islamic/ummalqura",
        &fxs,
        icu::calendar::islamic::IslamicUmmAlQura::new_always_calculating(),
        |y, m, d| {
            Date::try_new_ummalqura_date(
                y,
                m,
                d,
                icu::calendar::islamic::IslamicUmmAlQura::new_always_calculating(),
            )
            .unwrap()
        },
    );

    #[cfg(feature = "bench")]
    bench_calendar(
        &mut group,
        "calendar/islamic/observational",
        &fxs,
        icu::calendar::islamic::IslamicObservational::new_always_calculating(),
        |y, m, d| {
            Date::try_new_observational_islamic_date(
                y,
                m,
                d,
                icu::calendar::islamic::IslamicObservational::new_always_calculating(),
            )
            .unwrap()
        },
    );

    group.finish();
}

criterion_group!(benches, date_benches);
criterion_main!(benches);
