use arraystring::{prelude::*, typenum::U20};
use inlinable_string::{InlinableString, StringExt};
use smallstring::SmallString as SmallVecString;
use criterion::{criterion_group, criterion_main, Criterion};

fn string_clone_benchmark(c: &mut Criterion) {
    let string = String::from("abcdefghijklmnopqrst");
    c.bench_function("string clone", move |b| b.iter(|| string.clone()));
}

fn string_from_benchmark(c: &mut Criterion) {
    let string = String::from("uvwxyzaabbccddeeffgg");
    c.bench_function("string from", move |b| {
        b.iter(|| String::from(string.as_str()))
    });
}

fn string_push_str_benchmark(c: &mut Criterion) {
    let mut string = String::default();
    c.bench_function("string push str", move |b| {
        b.iter(|| {
            string.push_str("0123456789123456789");
            string.clear();
            string.shrink_to_fit();
        })
    });
}

fn inlinable_clone_benchmark(c: &mut Criterion) {
    let string = InlinableString::from("hcuahdaidshdaisuhda");
    c.bench_function("inlinable clone", move |b| b.iter(|| string.clone()));
}

fn inlinable_from_benchmark(c: &mut Criterion) {
    let string = "edauhefhiaw na na  ";
    c.bench_function("inlinable from", move |b| {
        b.iter(|| InlinableString::from(string))
    });
}

fn inlinable_push_str_benchmark(c: &mut Criterion) {
    let mut string = InlinableString::default();
    c.bench_function("inlinable push str", move |b| {
        b.iter(|| {
            string.push_str("ddauhifnaoe jaowijd");
            string.clear();
            string.shrink_to_fit();
        })
    });
}

fn smallvecstring_clone_benchmark(c: &mut Criterion) {
    let string = SmallVecString::<<U20 as Capacity>::Array>::from("xhduibabicemlatdhue");
    c.bench_function("smallvecstring clone", move |b| b.iter(|| string.clone()));
}

fn smallvecstring_from_benchmark(c: &mut Criterion) {
    let string = "audshaisdhaisduo8";
    c.bench_function("smallvecstring from", move |b| {
        b.iter(|| SmallVecString::<<U20 as Capacity>::Array>::from(string))
    });
}

fn small_clone_benchmark(c: &mut Criterion) {
    let string = SmallString::from_str_truncate("hhiijjkkllmmneeeepqq");
    c.bench_function("small clone", move |b| b.iter(|| string.clone()));
}

fn small_from_unchecked_benchmark(c: &mut Criterion) {
    let string = "rrssttuuvvwwxxyyzza";
    c.bench_function("small from unchecked", move |b| {
        b.iter(|| unsafe { SmallString::from_str_unchecked(&string) })
    });
}

fn small_from_truncate_benchmark(c: &mut Criterion) {
    let string = "bbbcccdddeeefffgggh";
    c.bench_function("small from truncate", move |b| {
        b.iter(|| SmallString::from_str_truncate(&string))
    });
}

fn small_try_from_benchmark(c: &mut Criterion) {
    let string = "iiijjjkkklllmmmnnnoo";
    c.bench_function("small try from", move |b| {
        b.iter(|| SmallString::try_from_str(&string))
    });
}

fn small_push_str_unchecked_benchmark(c: &mut Criterion) {
    let mut string = SmallString::default();
    c.bench_function("small push str unchecked", move |b| {
        b.iter(|| unsafe {
            string.push_str_unchecked("1413121110987654321");
            string.clear();
        })
    });
}

fn small_push_str_benchmark(c: &mut Criterion) {
    let mut string = SmallString::default();
    c.bench_function("small push str truncate", move |b| {
        b.iter(|| {
            string.push_str("1413121110987654321");
            string.clear();
        })
    });
}

fn small_try_push_str_benchmark(c: &mut Criterion) {
    let mut string = SmallString::default();
    c.bench_function("small try push str", move |b| {
        b.iter(|| {
            string.try_push_str("9897969594939291908").unwrap();
            string.clear();
        })
    });
}

fn cache_clone_benchmark(c: &mut Criterion) {
    let string = CacheString::from_str_truncate("opppqqqrrrssstttuuuv");
    c.bench_function("cache clone", move |b| b.iter(|| string.clone()));
}

fn cache_from_unchecked_benchmark(c: &mut Criterion) {
    let string = "wwwxxxyyyzzzaaaabbbb";
    c.bench_function("cache from unchecked", move |b| {
        b.iter(|| unsafe { CacheString::from_str_unchecked(&string) })
    });
}

fn cache_from_truncate_benchmark(c: &mut Criterion) {
    let string = "ccccddddeeeeffffggggh";
    c.bench_function("cache from truncate", move |b| {
        b.iter(|| CacheString::from_str_truncate(&string))
    });
}

fn cache_try_from_benchmark(c: &mut Criterion) {
    let string = "iiiijjjjkkkkllllmmmmn";
    c.bench_function("cache try from", move |b| {
        b.iter(|| CacheString::try_from_str(&string))
    });
}

fn cache_push_str_unchecked_benchmark(c: &mut Criterion) {
    let mut string = CacheString::default();
    c.bench_function("cache push str unchecked", move |b| {
        b.iter(|| unsafe {
            string.push_str_unchecked("1413121110987654321");
            string.clear();
        })
    });
}

fn cache_push_str_benchmark(c: &mut Criterion) {
    let mut string = CacheString::default();
    c.bench_function("cache push str truncate", move |b| {
        b.iter(|| {
            string.push_str("1413121110987654321");
            string.clear();
        })
    });
}

fn cache_try_push_str_benchmark(c: &mut Criterion) {
    let mut string = CacheString::default();
    c.bench_function("cache try push str", move |b| {
        b.iter(|| {
            string.try_push_str("9897969594939291908").unwrap();
            string.clear();
        })
    });
}

fn max_clone_benchmark(c: &mut Criterion) {
    let string = MaxString::from_str_truncate("ooopppqqqrrrssstttuu");
    c.bench_function("max clone", move |b| b.iter(|| string.clone()));
}

fn max_from_unchecked_benchmark(c: &mut Criterion) {
    let string = "vvvvwwwwxxxxyyyzzzza";
    c.bench_function("max from unchecked", move |b| {
        b.iter(|| unsafe { MaxString::from_str_unchecked(&string) })
    });
}

fn max_from_truncate_benchmark(c: &mut Criterion) {
    let string = "bbbbccccddddeeeeffff";
    c.bench_function("max from truncate", move |b| {
        b.iter(|| MaxString::from_str_truncate(&string))
    });
}

fn max_try_from_benchmark(c: &mut Criterion) {
    let string = "gggghhhhiiiijjjjkkkk";
    c.bench_function("max try from", move |b| {
        b.iter(|| MaxString::try_from_str(&string).unwrap())
    });
}

fn max_push_str_unchecked_benchmark(c: &mut Criterion) {
    let mut string = MaxString::default();
    c.bench_function("max push str unchecked", move |b| {
        b.iter(|| unsafe {
            string.push_str_unchecked("1413121110987654321");
            string.clear();
        })
    });
}

fn max_push_str_benchmark(c: &mut Criterion) {
    let mut string = MaxString::default();
    c.bench_function("max push str truncate", move |b| {
        b.iter(|| {
            string.push_str("1413121110987654321");
            string.clear();
        })
    });
}

fn max_try_push_str_benchmark(c: &mut Criterion) {
    let mut string = MaxString::default();
    c.bench_function("max try push str", move |b| {
        b.iter(|| {
            string.try_push_str("9897969594939291908").unwrap();
            string.clear();
        })
    });
}

criterion_group!(
    string,
    string_clone_benchmark,
    string_from_benchmark,
    string_push_str_benchmark
);
criterion_group!(
    inlinable,
    inlinable_clone_benchmark,
    inlinable_from_benchmark,
    inlinable_push_str_benchmark
);
criterion_group!(
    smallvecstring,
    smallvecstring_clone_benchmark,
    smallvecstring_from_benchmark,
);
criterion_group!(
    small,
    small_clone_benchmark,
    small_try_from_benchmark,
    small_from_unchecked_benchmark,
    small_from_truncate_benchmark,
    small_try_push_str_benchmark,
    small_push_str_unchecked_benchmark,
    small_push_str_benchmark,
);
criterion_group!(
    cache,
    cache_clone_benchmark,
    cache_try_from_benchmark,
    cache_from_unchecked_benchmark,
    cache_from_truncate_benchmark,
    cache_try_push_str_benchmark,
    cache_push_str_unchecked_benchmark,
    cache_push_str_benchmark,
);
criterion_group!(
    max,
    max_clone_benchmark,
    max_try_from_benchmark,
    max_from_unchecked_benchmark,
    max_from_truncate_benchmark,
    max_try_push_str_benchmark,
    max_push_str_unchecked_benchmark,
    max_push_str_benchmark,
);
criterion_main!(string, inlinable, smallvecstring, small, cache, max);
