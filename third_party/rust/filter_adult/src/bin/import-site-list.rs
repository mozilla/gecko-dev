/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use base64::prelude::*;
use clap::Parser;
use regex::Regex;
use std::fs::{read_to_string, File};
use std::io::Write;
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(version, about = "A tool for generating the adult_set.bin from the pre-existing FilterAdult.sys.mjs file.", long_about = None)]
struct Cli {
    /// The path to the FilterAdult.sys.mjs file that contains MD5 hashes of
    /// sites to filter.
    #[arg(required = true)]
    input_file: Option<PathBuf>,

    /// The path to write the adult_set.bin file, which are the bytes for the
    /// hash set of adult sites.
    #[arg(required = true)]
    output_file: Option<PathBuf>,
}

fn main() {
    let cli = Cli::parse();
    let input_path = cli.input_file.as_deref().unwrap();

    let file_contents: String = read_to_string(input_path)
        .unwrap_or_else(|_| panic!("Unable to read {}", input_path.display()));
    let bytes = ingest_filteradult_mjs(&file_contents);

    let output_path = cli.output_file.as_deref().unwrap();
    let mut buffer = File::create(output_path)
        .unwrap_or_else(|_| panic!("Unable to write {}", output_path.display()));
    buffer.write_all(&bytes).expect("Writing bytes failed");
}

fn ingest_filteradult_mjs(filteradult_mjs_contents: &str) -> Vec<u8> {
    let mut result = Vec::new();
    let re = Regex::new(r#"\s\s"(.+==)","#).unwrap();

    for (_, [string_hash]) in re
        .captures_iter(filteradult_mjs_contents)
        .map(|c| c.extract())
    {
        let mut byte_hash = BASE64_STANDARD.decode(string_hash).unwrap();
        result.append(&mut byte_hash);
    }

    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::borrow::Borrow;
    use std::collections::HashSet;

    #[test]
    fn test_ingest_filteradult_mjs_simple() {
        let filteradult_contents = r#"
   * For tests, adds a domain to the adult list.
   */
  addDomainToList(url) {
    gAdultSet.add(
      md5Hash(Services.eTLD.getBaseDomain(Services.io.newURI(url)))
    );
  },

  /**
   * For tests, removes a domain to the adult list.
   */
  removeDomainFromList(url) {
    gAdultSet.delete(
      md5Hash(Services.eTLD.getBaseDomain(Services.io.newURI(url)))
    );
  },
};

// These are md5 hashes of base domains to be filtered out. Originally from:
// https://hg.mozilla.org/mozilla-central/log/default/browser/base/content/newtab/newTab.inadjacent.json
gAdultSet = new Set([
  "+P5q4YD1Rr5SX26Xr+tzlw==",
  "+PUVXkoTqHxJHO18z4KMfw==",
  "+Pl0bSMBAdXpRIA+zE02JA==",
  "+QosBAnSM2h4lsKuBlqEZw==",
  "+S+WXgVDSU1oGmCzGwuT3g==",
  "+SclwwY8R2RPrnX54Z+A6w==",
]);
"#;
        let expected_hashes = vec![
            "+P5q4YD1Rr5SX26Xr+tzlw==",
            "+PUVXkoTqHxJHO18z4KMfw==",
            "+Pl0bSMBAdXpRIA+zE02JA==",
            "+QosBAnSM2h4lsKuBlqEZw==",
            "+S+WXgVDSU1oGmCzGwuT3g==",
            "+SclwwY8R2RPrnX54Z+A6w==",
        ];
        let bytes = ingest_filteradult_mjs(filteradult_contents);

        let mut hashset = HashSet::new();
        for chunk in bytes.chunks_exact(16) {
            hashset.insert(chunk);
        }

        // Ensure we got the expected number of hashes out.
        assert_eq!(hashset.len(), 6);

        for hash in expected_hashes {
            // Compute the byte representation of the hash
            let hash_bytes = BASE64_STANDARD.decode(hash).unwrap();
            // We expect these bytes in the hashset.
            assert!(hashset.contains::<[u8]>(hash_bytes.borrow()));
        }
    }

    #[test]
    fn test_ingest_filteradult_mjs_empty() {
        let filteradult_contents = r#"
   * For tests, adds a domain to the adult list.
   */
  addDomainToList(url) {
    gAdultSet.add(
      md5Hash(Services.eTLD.getBaseDomain(Services.io.newURI(url)))
    );
  }
]);
"#;
        let bytes = ingest_filteradult_mjs(filteradult_contents);

        // We should not have found any hashes.
        assert_eq!(bytes.len(), 0);
    }
}
