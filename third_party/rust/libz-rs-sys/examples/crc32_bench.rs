//! a binary just so we can look at the optimized assembly

pub fn main() {
    let mut it = std::env::args();

    let _ = it.next().unwrap();

    match it.next().unwrap().as_str() {
        "sse" => {
            let path = it.next().unwrap();
            let input = std::fs::read(path).unwrap();

            let mut state = zlib_rs::crc32::Crc32Fold::new();
            state.fold(&input, 0);
            println!("{:#x}", state.finish());
        }

        "crc32fast" => {
            let path = it.next().unwrap();
            let input = std::fs::read(path).unwrap();

            let mut h = crc32fast::Hasher::new_with_initial(0);
            h.update(&input[..]);
            println!("{:#x}", h.finalize());
        }

        "sse-chunked" => {
            let path = it.next().unwrap();
            let input = std::fs::read(path).unwrap();

            let mut state = zlib_rs::crc32::Crc32Fold::new();

            for c in input.chunks(32) {
                state.fold(c, 0);
            }
            println!("{:#x}", state.finish());
        }

        "crc32fast-chunked" => {
            let path = it.next().unwrap();
            let input = std::fs::read(path).unwrap();

            let mut h = crc32fast::Hasher::new_with_initial(0);

            for c in input.chunks(32) {
                h.update(c);
            }
            println!("{:#x}", h.finalize());
        }

        "adler32" => {
            let path = it.next().unwrap();
            let input = std::fs::read(path).unwrap();

            let h = zlib_rs::adler32(42, &input);
            println!("{:#x}", h);
        }

        other => panic!("invalid option '{other}', expected one of 'rs' or 'ng'"),
    }
}
