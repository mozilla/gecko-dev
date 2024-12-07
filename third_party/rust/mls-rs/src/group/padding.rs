// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// Padding used when sending an encrypted group message.
#[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum PaddingMode {
    /// Step function based on the size of the message being sent.
    /// The amount of padding used will increase with the size of the original
    /// message.
    #[default]
    StepFunction,
    /// No padding.
    None,
}

impl PaddingMode {
    pub(super) fn padded_size(&self, content_size: usize) -> usize {
        match self {
            PaddingMode::StepFunction => {
                // The padding hides all but 2 most significant bits of `length`. The hidden bits are replaced
                // by zeros and then the next number is taken to make sure the message fits.
                let blind = 1
                    << ((content_size + 1)
                        .next_power_of_two()
                        .max(256)
                        .trailing_zeros()
                        - 3);

                (content_size | (blind - 1)) + 1
            }
            PaddingMode::None => content_size,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::PaddingMode;

    use alloc::vec;
    use alloc::vec::Vec;
    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[derive(serde::Deserialize, serde::Serialize)]
    struct TestCase {
        input: usize,
        output: usize,
    }

    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_message_padding_test_vector() -> Vec<TestCase> {
        let mut test_cases = vec![];
        for x in 1..1024 {
            test_cases.push(TestCase {
                input: x,
                output: PaddingMode::StepFunction.padded_size(x),
            });
        }
        test_cases
    }

    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(
            message_padding_test_vector,
            generate_message_padding_test_vector()
        )
    }

    #[test]
    fn test_no_padding() {
        for i in [0, 100, 1000, 10000] {
            assert_eq!(PaddingMode::None.padded_size(i), i)
        }
    }

    #[test]
    fn test_padding_length() {
        assert_eq!(PaddingMode::StepFunction.padded_size(0), 32);

        // Short
        assert_eq!(PaddingMode::StepFunction.padded_size(63), 64);
        assert_eq!(PaddingMode::StepFunction.padded_size(64), 96);
        assert_eq!(PaddingMode::StepFunction.padded_size(65), 96);

        // Almost long and almost short
        assert_eq!(PaddingMode::StepFunction.padded_size(127), 128);
        assert_eq!(PaddingMode::StepFunction.padded_size(128), 160);
        assert_eq!(PaddingMode::StepFunction.padded_size(129), 160);

        // One length from each of the 4 buckets between 256 and 512
        assert_eq!(PaddingMode::StepFunction.padded_size(260), 320);
        assert_eq!(PaddingMode::StepFunction.padded_size(330), 384);
        assert_eq!(PaddingMode::StepFunction.padded_size(390), 448);
        assert_eq!(PaddingMode::StepFunction.padded_size(490), 512);

        // All test cases
        let test_cases: Vec<TestCase> = load_test_cases();
        for test_case in test_cases {
            assert_eq!(
                test_case.output,
                PaddingMode::StepFunction.padded_size(test_case.input)
            );
        }
    }
}
