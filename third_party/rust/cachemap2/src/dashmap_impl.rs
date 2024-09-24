use dashmap::DashMap;
use std::ops::Deref;
use std::sync::Arc;
use std::{collections::hash_map::RandomState, hash::Hash};
use std::{hash::BuildHasher, marker::PhantomData};

/// An insert-only map for caching the result of functions
pub struct CacheMap<K: Hash + Eq, V: ?Sized, S = RandomState> {
    inner: DashMap<K, Arc<V>, S>,
}

/// A handle that can be converted to a &T or an Arc<T>
pub struct ArcRef<'a, T: ?Sized> {
    // this pointer never gets dereferenced, but it has to be T, so that Ref is the right size for wide pointers
    #[allow(dead_code)]
    fake_ptr: *const T,
    phantom: PhantomData<&'a T>,
}

impl<'a, T: ?Sized> Clone for ArcRef<'a, T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<'a, T: ?Sized> Copy for ArcRef<'a, T> {}

impl<T: ?Sized> Deref for ArcRef<'_, T> {
    type Target = Arc<T>;
    fn deref(&self) -> &Self::Target {
        unsafe { std::mem::transmute(self) }
    }
}

impl<'a, T: ?Sized> ArcRef<'a, T> {
    /// Converts the ArcRef into an Arc<T>
    pub fn to_arc(self) -> Arc<T> {
        self.deref().clone()
    }

    /// Converts the ArcRef into a &T
    pub fn as_ref(self) -> &'a T {
        let ptr = &**self as *const T;
        unsafe { &*ptr }
    }
}

impl<K: Hash + Eq, V: ?Sized, S: BuildHasher + Default + Clone> Default for CacheMap<K, V, S> {
    fn default() -> Self {
        CacheMap {
            inner: Default::default(),
        }
    }
}

impl<K: Hash + Eq, V, S: BuildHasher + Default + Clone> std::iter::FromIterator<(K, V)>
    for CacheMap<K, V, S>
{
    fn from_iter<T>(iter: T) -> Self
    where
        T: IntoIterator<Item = (K, V)>,
    {
        CacheMap {
            inner: iter.into_iter().map(|(k, v)| (k, Arc::new(v))).collect(),
        }
    }
}

pub struct IntoIter<K, V, S>(dashmap::iter::OwningIter<K, Arc<V>, S>);

impl<K: Eq + Hash, V, S: BuildHasher + Clone> Iterator for IntoIter<K, V, S> {
    type Item = (K, Arc<V>);

    fn next(&mut self) -> Option<Self::Item> {
        self.0.next()
    }
}

impl<K: Hash + Eq, V, S: BuildHasher + Clone> IntoIterator for CacheMap<K, V, S> {
    type Item = (K, Arc<V>);
    type IntoIter = IntoIter<K, V, S>;

    fn into_iter(self) -> Self::IntoIter {
        IntoIter(self.inner.into_iter())
    }
}

impl<K: Hash + Eq, V, S: BuildHasher + Clone> CacheMap<K, V, S> {
    /// Fetch the value associated with the key, or run the provided function to insert one.
    ///
    /// # Example
    ///
    /// ```
    /// use cachemap2::CacheMap;
    ///
    /// let m = CacheMap::new();
    ///
    /// let fst = m.cache("key", || 5u32).as_ref();
    /// let snd = m.cache("key", || 7u32).as_ref();
    ///
    /// assert_eq!(*fst, *snd);
    /// assert_eq!(*fst, 5u32);
    /// ```
    pub fn cache<F: FnOnce() -> V>(&self, key: K, f: F) -> ArcRef<'_, V> {
        self.cache_arc(key, || Arc::new(f()))
    }

    /// Fetch the value associated with the key, or insert a default value.
    pub fn cache_default(&self, key: K) -> ArcRef<'_, V>
    where
        V: Default,
    {
        self.cache(key, || Default::default())
    }

    /// Return whether the map contains the given key.
    pub fn contains_key<Q: ?Sized>(&self, key: &Q) -> bool
    where
        K: std::borrow::Borrow<Q>,
        Q: Hash + Eq,
    {
        self.inner.contains_key(key)
    }
}

impl<K: Hash + Eq, V: ?Sized> CacheMap<K, V, RandomState> {
    /// Creates a new CacheMap
    pub fn new() -> Self {
        CacheMap {
            inner: DashMap::new(),
        }
    }
}

impl<K: Hash + Eq, V: ?Sized, S: BuildHasher + Clone> CacheMap<K, V, S> {
    /// Creates a new CacheMap with the provided hasher
    pub fn with_hasher(hash_builder: S) -> Self {
        Self {
            inner: DashMap::with_hasher(hash_builder),
        }
    }

    /// Fetch the value associated with the key, or run the provided function to insert one.
    /// With this version, the function returns an Arc<V>, whch allows caching unsized types.
    ///
    /// # Example
    ///
    /// ```
    /// use cachemap2::CacheMap;
    ///
    /// let m: CacheMap<_, [usize]> = CacheMap::new();
    ///
    /// let a = m.cache_arc("a", || {
    ///		let a = &[1,2,3][..];
    ///     a.into()
    /// }).as_ref();
    ///
    /// let b = m.cache_arc("b", || {
    ///		let b = &[9,9][..];
    ///     b.into()
    /// }).as_ref();
    ///
    /// assert_eq!(a, &[1,2,3]);
    /// assert_eq!(b, &[9,9]);
    /// ```
    pub fn cache_arc<F: FnOnce() -> Arc<V>>(&self, key: K, f: F) -> ArcRef<'_, V> {
        let val = self.inner.entry(key).or_insert_with(f);
        let arc: &Arc<V> = &*val;
        let arc_ref: &ArcRef<'_, V> = unsafe { std::mem::transmute(arc) };
        *arc_ref
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_insert() {
        let m = CacheMap::new();

        let a = m.cache("key", || 21u32).as_ref();
        assert_eq!(21, *a);
    }

    #[test]
    fn contains_key() {
        let m = CacheMap::new();

        m.cache("key", || 21u32);
        assert!(m.contains_key("key"));
        assert!(!m.contains_key("other"));
    }

    #[test]
    fn double_insert() {
        let m = CacheMap::new();

        let a = m.cache("key", || 5u32).as_ref();
        let b = m.cache("key", || 7u32).as_ref();

        assert_eq!(*a, *b);
        assert_eq!(5, *a);
    }

    #[test]
    fn insert_two() {
        let m = CacheMap::new();

        let a = m.cache("a", || 5u32).as_ref();
        let b = m.cache("b", || 7u32).as_ref();

        assert_eq!(5, *a);
        assert_eq!(7, *b);

        let c = m.cache("a", || 9u32).as_ref();
        let d = m.cache("b", || 11u32).as_ref();

        assert_eq!(*a, *c);
        assert_eq!(*b, *d);

        assert_eq!(5, *a);
        assert_eq!(7, *b);
    }

    #[test]
    fn use_after_drop() {
        #[derive(Clone)]
        struct Foo(usize);
        impl Drop for Foo {
            fn drop(&mut self) {
                assert_eq!(33, self.0);
            }
        }

        {
            let mut arc = {
                let m = CacheMap::new();
                let a = m.cache("key", || Foo(99)).to_arc();
                assert_eq!(99, (*a).0);
                a
            };

            Arc::make_mut(&mut arc).0 = 33;
        }

        assert!(true);
    }
}
