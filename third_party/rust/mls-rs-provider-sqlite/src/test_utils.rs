// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use rand::RngCore;
pub fn gen_rand_bytes(size: usize) -> Vec<u8> {
    let mut bytes: Vec<u8> = vec![0; size];
    rand::thread_rng().fill_bytes(&mut bytes);
    bytes
}
