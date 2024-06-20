// @generated
/// Marks a type as a data provider. You can then use macros like
/// `impl_core_helloworld_v1` to add implementations.
///
/// ```ignore
/// struct MyProvider;
/// const _: () = {
///     include!("path/to/generated/macros.rs");
///     make_provider!(MyProvider);
///     impl_core_helloworld_v1!(MyProvider);
/// }
/// ```
#[doc(hidden)]
#[macro_export]
macro_rules! __make_provider {
    ($ name : ty) => {
        #[clippy::msrv = "1.67"]
        impl $name {
            #[doc(hidden)]
            #[allow(dead_code)]
            pub const MUST_USE_MAKE_PROVIDER_MACRO: () = ();
        }
    };
}
#[doc(inline)]
pub use __make_provider as make_provider;
#[macro_use]
#[path = "macros/calendar_japanese_v1.rs.data"]
mod calendar_japanese_v1;
#[doc(inline)]
pub use __impl_calendar_japanese_v1 as impl_calendar_japanese_v1;
#[macro_use]
#[path = "macros/calendar_japanext_v1.rs.data"]
mod calendar_japanext_v1;
#[doc(inline)]
pub use __impl_calendar_japanext_v1 as impl_calendar_japanext_v1;
#[macro_use]
#[path = "macros/datetime_week_data_v1.rs.data"]
mod datetime_week_data_v1;
#[doc(inline)]
pub use __impl_datetime_week_data_v1 as impl_datetime_week_data_v1;
