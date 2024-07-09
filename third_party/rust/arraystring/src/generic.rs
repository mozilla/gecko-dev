//! Implements actual generic array abstraction for all supported types from `typenum` (1 to 255)

use typenum::*;

macro_rules! impl_generic_array {
    ($($type: ty),*) => {
        $(
            impl private::Sealed for $type {}
            impl Capacity for $type {
                type Array = [u8; Self::USIZE];
            }

            impl private::Sealed for <$type as Capacity>::Array {}
            impl ArraySlice for <$type as Capacity>::Array {
                const CAPACITY: usize = <$type as Unsigned>::USIZE;

                #[inline]
                fn as_slice(&self) -> &[u8] {
                    self
                }

                #[inline]
                unsafe fn as_mut_slice(&mut self) -> &mut [u8] {
                    self
                }

                #[inline]
                fn zeroed() -> Self {
                    [0; Self::CAPACITY]
                }
            }
        )*
    }
}

/// Private module to hide access to sealed trait
mod private {
    /// Trait impossible to be implemented outside of this crate, seals other traits
    pub trait Sealed {}
}

/// Implements needed types for all types of arrays (bigger than 32 don't have the default traits)
#[doc(hidden)]
pub trait ArraySlice: private::Sealed {
    /// Capacity represented by type
    const CAPACITY: usize;

    /// Returns slice of the entire array
    fn as_slice(&self) -> &[u8];
    /// Returns mutable slice of the entire array
    unsafe fn as_mut_slice(&mut self) -> &mut [u8];
    /// Returns array filled with zeroes
    fn zeroed() -> Self;
}

/// Converts between `typenum` types and its corresponding array
#[doc(hidden)]
pub trait Capacity: Unsigned + private::Sealed {
    /// Array with specified capacity
    type Array: ArraySlice + Copy;
}

impl_generic_array!(
    U1, U2, U3, U4, U5, U6, U7, U8, U9, U10, U11, U12, U13, U14, U15, U16, U17, U18, U19, U20, U21,
    U22, U23, U24, U25, U26, U27, U28, U29, U30, U31, U32, U33, U34, U35, U36, U37, U38, U39, U40,
    U41, U42, U43, U44, U45, U46, U47, U48, U49, U50, U51, U52, U53, U54, U55, U56, U57, U58, U59,
    U60, U61, U62, U63, U64, U65, U66, U67, U68, U69, U70, U71, U72, U73, U74, U75, U76, U77, U78,
    U79, U80, U81, U82, U83, U84, U85, U86, U87, U88, U89, U90, U91, U92, U93, U94, U95, U96, U97,
    U98, U99, U100, U101, U102, U103, U104, U105, U106, U107, U108, U109, U110, U111, U112, U113,
    U114, U115, U116, U117, U118, U119, U120, U121, U122, U123, U124, U125, U126, U127, U128, U129,
    U130, U131, U132, U133, U134, U135, U136, U137, U138, U139, U140, U141, U142, U143, U144, U145,
    U146, U147, U148, U149, U150, U151, U152, U153, U154, U155, U156, U157, U158, U159, U160, U161,
    U162, U163, U164, U165, U166, U167, U168, U169, U170, U171, U172, U173, U174, U178, U179, U180,
    U181, U182, U183, U184, U185, U186, U187, U188, U189, U190, U191, U192, U193, U194, U195, U196,
    U197, U198, U199, U200, U201, U202, U203, U204, U205, U206, U207, U208, U209, U210, U211, U212,
    U213, U214, U215, U216, U217, U218, U219, U220, U221, U222, U223, U224, U225, U226, U227, U228,
    U229, U230, U231, U232, U233, U234, U235, U236, U237, U238, U239, U240, U241, U242, U243, U244,
    U245, U246, U247, U248, U249, U250, U251, U252, U253, U254, U255
);
