/// Magically unpack up to 6 values from 10 bits.
///
/// Background:
///
/// Let's start with a simpler example of packing a list of numbers.
/// Let's say you want to store 2 values a and b, which can each be 0, 1, or 2.
/// You can store this as x = a * 3 + b. Then you can get out (a, b) by doing a
/// division by 3 with remainder, because this has the form of n * 3 + (something less than 3)
///
/// Similar, for four values, you can use:
///
/// ```text
/// x = a * 27 + b * 9 + c * 3 + d.
///              ^^^^^^^^^^^^^^^^^ == x % 27
///                      ^^^^^^^^^ == x % 9
///                              ^ == x % 3
/// x == 27 * a + rem27
/// rem27 == 9 * b + rem9
/// rem9 == 3 * c + rem3
/// rem3 = d
/// ```
///
/// Written differently:
/// `x = d + 3 * (c + 3 * (b + (3 * a)))`
///
/// So that was the case for when all digits have the same range (0..3 in this example).
///
/// In this function we want to decode a permutation. In a permutation of n items,
/// for the first digit we can choose one of n items, for the second digit we can
/// choose one of the remaining n - 1 items, for the third one of the remaining n - 2 etc.
///
/// We have the choice between 6 registers, so n = 6 in this function.
/// Each digit is stored zero-based. So a is in 0..6, b is in 0..5, c in 0..4 etc.
///
/// We encode as (a, b, c) as c + 4 * (b + 5 * a)
/// [...]
pub fn decode_permutation_6(count: u32, mut encoding: u32) -> std::result::Result<[u8; 6], ()> {
    if count > 6 {
        return Err(());
    }

    let mut compressed_regindexes = [0; 6];

    if count > 4 {
        compressed_regindexes[4] = encoding % 2;
        encoding /= 2;
    }
    if count > 3 {
        compressed_regindexes[3] = encoding % 3;
        encoding /= 3;
    }
    if count > 2 {
        compressed_regindexes[2] = encoding % 4;
        encoding /= 4;
    }
    if count > 1 {
        compressed_regindexes[1] = encoding % 5;
        encoding /= 5;
    }
    if count > 0 {
        compressed_regindexes[0] = encoding;
    }

    if compressed_regindexes[0] >= 6 {
        return Err(());
    }

    let mut registers = [0; 6];
    let mut used = [false; 6];
    for i in 0..count {
        let compressed_regindex = compressed_regindexes[i as usize];
        debug_assert!(compressed_regindex < 6 - i);
        let uncompressed_regindex = (0..6)
            .filter(|ri| !used[*ri])
            .nth(compressed_regindex as usize)
            .unwrap();
        used[uncompressed_regindex] = true;
        registers[i as usize] = (uncompressed_regindex + 1) as u8;
    }
    Ok(registers)
}
