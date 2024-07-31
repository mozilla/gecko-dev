// SPDX-License-Identifier: Apache-2.0 OR MIT

// default pin_project! is completely safe.

::pin_project_lite::pin_project! {
    /// Testing default struct.
    #[derive(Debug)]
    pub struct DefaultStruct<T, U> {
        #[pin]
        pub pinned: T,
        pub unpinned: U,
    }
}

::pin_project_lite::pin_project! {
    /// Testing named struct.
    #[project = DefaultStructProj]
    #[project_ref = DefaultStructProjRef]
    #[derive(Debug)]
    pub struct DefaultStructNamed<T, U> {
        #[pin]
        pub pinned: T,
        pub unpinned: U,
    }
}

::pin_project_lite::pin_project! {
    /// Testing enum.
    #[project = DefaultEnumProj]
    #[project_ref = DefaultEnumProjRef]
    #[derive(Debug)]
    pub enum DefaultEnum<T, U> {
        /// Struct variant.
        Struct {
            #[pin]
            pinned: T,
            unpinned: U,
        },
        /// Unit variant.
        Unit,
    }
}
