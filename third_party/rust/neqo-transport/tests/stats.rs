// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_transport::ecn;

#[test]
fn crate_exports_ecn_types() {
    let stats = neqo_transport::Stats::default();

    let _ = stats.ecn_path_validation[ecn::ValidationOutcome::Capable];
    let _ = stats.ecn_path_validation
        [ecn::ValidationOutcome::NotCapable(ecn::ValidationError::BlackHole)];
}
