/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_export]
macro_rules! packaged_collections {
    ($(($bucket:expr, $collection:expr)),* $(,)?) => {
        fn get_packaged_data(collection_name: &str) -> Option<&'static str> {
            match collection_name {
                $($collection => Some(include_str!(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/dumps/",
                    $bucket,
                    "/",
                    $collection,
                    ".json"
                ))),)*
                _ => None,
            }
        }
    };
}

#[macro_export]
macro_rules! packaged_attachments {
    () => {
        fn get_packaged_attachment(collection_name: &str, filename: &str) -> Option<(&'static [u8], &'static str)> {
            None
        }
    };
    ($(($bucket:expr, $collection:expr) => [$($filename:expr),* $(,)?]),* $(,)?) => {
        fn get_packaged_attachment(collection_name: &str, filename: &str) -> Option<(&'static [u8], &'static str)> {
            match (collection_name, filename) {
                $($(
                    ($collection, $filename) => Some((
                        include_bytes!(concat!(
                            env!("CARGO_MANIFEST_DIR"),
                            "/dumps/",
                            $bucket,
                            "/attachments/",
                            $collection,
                            "/",
                            $filename
                        )),
                        include_str!(concat!(
                            env!("CARGO_MANIFEST_DIR"),
                            "/dumps/",
                            $bucket,
                            "/attachments/",
                            $collection,
                            "/",
                            $filename,
                            ".meta.json"
                        ))
                    )),
                )*)*
                _ => None,
            }
        }
    };
}
