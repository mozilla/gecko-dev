# CacheMap

CacheMap is a data structure for concurrently caching values.

The `cache` function will look up a value in the map, or generate and store a new one using the
provided function.

This is a updated and maintained fork of [hclarke/cachemap](https://github.com/hclarke/cachemap).

## Example

```
use cachemap::CacheMap;
	
let m = CacheMap::new();

let fst = m.cache("key", || 5u32);
let snd = m.cache("key", || 7u32);

assert_eq!(*fst, *snd);
assert_eq!(*fst, 5u32);
```

## Features

- Can cache values concurrently (using `&CacheMap<K,V>` rather than `&mut CacheMap<K,V>`).
- Returned references use the map's lifetime, so clients can avoid smart pointers.
- Clients can optionally enable the `dashmap` feature, which uses `dashmap` internally and allows:
  - getting `Arc<V>` pointers, in case values need to outlive the map, and
  - adding `Arc<V>` directly, allowing unsized values, and re-using `Arc<V>`s from elsewhere.
- Clients can optionally enable the `abi_stable` feature which will derive `abi_stable::StableAbi`
  on the type.

## AntiFeatures

- There is no cache invalidation: the only way to remove things from a CacheMap is to drop it.
