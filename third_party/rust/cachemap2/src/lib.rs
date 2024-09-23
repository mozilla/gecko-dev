#[cfg(feature = "dashmap")]
mod dashmap_impl;

#[cfg(feature = "dashmap")]
pub use dashmap_impl::*;

#[cfg(all(feature = "dashmap", feature = "abi_stable"))]
compile_error!("abi_stable and dashmap features cannot be used together");

#[cfg(not(feature = "dashmap"))]
mod hashmap_impl;

#[cfg(not(feature = "dashmap"))]
pub use hashmap_impl::*;
