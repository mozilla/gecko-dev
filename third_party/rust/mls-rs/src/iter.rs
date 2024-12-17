// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(all(not(mls_build_async), feature = "rayon"))]
mod sync_rayon {
    use rayon::{
        iter::IterBridge,
        prelude::{FromParallelIterator, IntoParallelIterator, ParallelBridge, ParallelIterator},
    };

    pub fn wrap_iter<I>(it: I) -> I::Iter
    where
        I: IntoParallelIterator,
    {
        it.into_par_iter()
    }

    pub fn wrap_impl_iter<I>(it: I) -> IterBridge<I::IntoIter>
    where
        I: IntoIterator,
        I::IntoIter: Send,
        I::Item: Send,
    {
        it.into_iter().par_bridge()
    }

    pub trait ParallelIteratorExt {
        type Ok: Send;
        type Error: Send;

        fn try_collect<A>(self) -> Result<A, Self::Error>
        where
            A: FromParallelIterator<Self::Ok>;
    }

    impl<I, T, E> ParallelIteratorExt for I
    where
        I: ParallelIterator<Item = Result<T, E>>,
        T: Send,
        E: Send,
    {
        type Ok = T;
        type Error = E;

        fn try_collect<A>(self) -> Result<A, Self::Error>
        where
            A: FromParallelIterator<Self::Ok>,
        {
            self.collect()
        }
    }
}

#[cfg(all(not(mls_build_async), feature = "rayon"))]
pub use sync_rayon::{wrap_impl_iter, wrap_iter, ParallelIteratorExt};

#[cfg(not(any(mls_build_async, feature = "rayon")))]
mod sync {
    pub fn wrap_iter<I>(it: I) -> I::IntoIter
    where
        I: IntoIterator,
    {
        it.into_iter()
    }

    pub fn wrap_impl_iter<I>(it: I) -> I::IntoIter
    where
        I: IntoIterator,
    {
        it.into_iter()
    }
}

#[cfg(not(any(mls_build_async, feature = "rayon")))]
pub use sync::{wrap_impl_iter, wrap_iter};

#[cfg(mls_build_async)]
mod async_ {
    pub fn wrap_iter<I>(it: I) -> futures::stream::Iter<I::IntoIter>
    where
        I: IntoIterator,
    {
        futures::stream::iter(it)
    }

    pub fn wrap_impl_iter<I>(it: I) -> futures::stream::Iter<I::IntoIter>
    where
        I: IntoIterator,
    {
        futures::stream::iter(it)
    }
}

#[cfg(mls_build_async)]
pub use async_::{wrap_impl_iter, wrap_iter};
