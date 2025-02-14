// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    net::{IpAddr, Ipv4Addr},
    time::Duration,
};

mod cubic;
mod new_reno;

pub const IP_ADDR: IpAddr = IpAddr::V4(Ipv4Addr::UNSPECIFIED);
pub const MTU: Option<usize> = Some(1_500);
pub const RTT: Duration = Duration::from_millis(100);
