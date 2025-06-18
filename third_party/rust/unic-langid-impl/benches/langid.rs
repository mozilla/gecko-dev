use criterion::{black_box, criterion_group, criterion_main, Criterion};

use unic_langid_impl::subtags;
use unic_langid_impl::LanguageIdentifier;

static STRINGS: &[&str] = &[
    "en-US",
    "en-GB",
    "es-AR",
    "it",
    "zh-Hans-CN",
    "de-AT",
    "pl",
    "fr-FR",
    "de-AT",
    "sr-Cyrl-SR",
    "nb-NO",
    "fr-FR",
    "mk",
    "uk",
    "en-US",
    "en-GB",
    "es-AR",
    "th",
    "de",
    "zh-Cyrl-HN",
    "en-Latn-US",
];

fn language_identifier_construct_bench(c: &mut Criterion) {
    let mut group = c.benchmark_group("language_identifier_construct");

    let slices: Vec<&[u8]> = STRINGS.iter().map(|s| s.as_bytes()).collect();
    let langids: Vec<LanguageIdentifier> = STRINGS.iter().map(|s| s.parse().unwrap()).collect();
    let entries: Vec<_> = langids
        .iter()
        .cloned()
        .map(|langid| langid.into_parts())
        .collect();

    group.bench_function("from_str", |b| {
        b.iter(|| {
            for s in STRINGS {
                let _: Result<LanguageIdentifier, _> = black_box(s).parse();
            }
        })
    });

    group.bench_function("from_bytes", |b| {
        b.iter(|| {
            for s in &slices {
                let _ = LanguageIdentifier::from_bytes(black_box(s));
            }
        })
    });

    group.bench_function("from_parts", |b| {
        b.iter(|| {
            for (language, script, region, variants) in &entries {
                let _ = LanguageIdentifier::from_parts(*language, *script, *region, variants);
            }
        })
    });

    group.finish();
}

criterion_group!(benches, language_identifier_construct_bench,);
criterion_main!(benches);
