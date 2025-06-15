/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Support for crash report creation and upload.
//!
//! Upload currently uses the system libcurl or curl binary rather than a rust network stack (as
//! curl is more mature, albeit the code to interact with it must be a bit more careful).

use super::http;
use crate::std::path::Path;

#[cfg(mock)]
use crate::std::mock::{mock_key, MockKey};

#[cfg(mock)]
mock_key! {
    /// The outer Result is for CrashReport::send(), the inner is for CrashReporterSender::finish().
    pub struct MockReport => Box<dyn Fn(&CrashReport) -> std::io::Result<std::io::Result<String>> + Send + Sync>
}

/// A crash report to upload.
///
/// Post a multipart form payload to the report URL.
///
/// The form data contains:
/// | name | filename | content | mime |
/// ====================================
/// | `extra` | `extra.json` | extra json object | `application/json`|
/// | `upload_file_minidump` | dump file name | dump file contents | derived (probably application/binary) |
/// if present:
/// | `memory_report` | memory file name | memory file contents | derived (probably gzipped json) |
pub struct CrashReport<'a> {
    pub extra: &'a serde_json::Value,
    pub dump_file: &'a Path,
    pub memory_file: Option<&'a Path>,
    pub url: &'a str,
}

impl CrashReport<'_> {
    /// Send the crash report.
    pub fn send(&self) -> std::io::Result<CrashReportSender> {
        #[cfg(mock)]
        if let Some(r) = MockReport.try_get(|f| f(self)) {
            return r.map(|inner| {
                CrashReportSender(http::Request::Mock {
                    response: inner.map(|s| s.into()),
                })
            });
        }

        let extra_json_data = serde_json::to_string(self.extra)?;

        let mut parts = vec![
            http::MimePart {
                name: "extra",
                content: http::MimePartContent::String(&extra_json_data),
                filename: Some("extra.json"),
                mime_type: Some("application/json"),
            },
            http::MimePart {
                name: "upload_file_minidump",
                content: http::MimePartContent::File(self.dump_file),
                filename: None,
                mime_type: None,
            },
        ];
        if let Some(path) = self.memory_file {
            parts.push(http::MimePart {
                name: "memory_report",
                content: http::MimePartContent::File(path),
                filename: None,
                mime_type: None,
            })
        }

        http::RequestBuilder::MimePost { parts }
            .build(self.url)
            .map(CrashReportSender)
    }
}

pub struct CrashReportSender(http::Request);

impl CrashReportSender {
    pub fn finish(self) -> anyhow::Result<Response> {
        let response = self.0.send()?;
        let response = String::from_utf8_lossy(&response).into_owned();
        log::debug!("received response from sending report: {:?}", &*response);
        Ok(Response::parse(response))
    }
}

/// A parsed response from submitting a crash report.
#[derive(Default, Debug)]
pub struct Response {
    pub crash_id: Option<String>,
    pub stop_sending_reports_for: Option<String>,
    pub view_url: Option<String>,
    pub discarded: bool,
}

impl Response {
    /// Parse a server response.
    ///
    /// The response should be newline-separated `<key>=<value>` pairs.
    fn parse<S: AsRef<str>>(response: S) -> Self {
        let mut ret = Self::default();
        // Fields may be omitted, and parsing is best-effort but will not produce any errors (just
        // a default Response struct).
        for line in response.as_ref().lines() {
            if let Some((key, value)) = line.split_once('=') {
                match key {
                    "StopSendingReportsFor" => {
                        ret.stop_sending_reports_for = Some(value.to_owned())
                    }
                    "Discarded" => ret.discarded = true,
                    "CrashID" => ret.crash_id = Some(value.to_owned()),
                    "ViewURL" => ret.view_url = Some(value.to_owned()),
                    _ => (),
                }
            }
        }
        ret
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::std::mock;

    #[test]
    fn report_http() {
        for memory_file in [None, Some(Path::new("minidump.memory.gz"))] {
            let report = CrashReport {
                extra: &serde_json::json! {{
                    "Foo": "Bar"
                }},
                dump_file: Path::new("minidump.dmp"),
                memory_file,
                url: "reports.example.com".as_ref(),
            };

            let checked = crate::test::Counter::new();

            mock::builder()
                .set(
                    http::MockHttp,
                    Box::new(cc!(
                        (checked)
                        move |request, url| {
                            checked.inc();
                            assert_eq!(url, "reports.example.com");
                            let mut parts = vec![
                                http::MimePart {
                                    name: "extra",
                                    content: http::MimePartContent::String(r#"{"Foo":"Bar"}"#),
                                    filename: Some("extra.json"),
                                    mime_type: Some("application/json"),
                                },
                                http::MimePart {
                                    name: "upload_file_minidump",
                                    content: http::MimePartContent::File(Path::new("minidump.dmp")),
                                    filename: None,
                                    mime_type: None,
                                },
                            ];
                            if let Some(name) = memory_file {
                                parts.push(http::MimePart {
                                    name: "memory_report",
                                    content: http::MimePartContent::File(name),
                                    filename: None,
                                    mime_type: None,
                                });
                            }
                            assert_eq!(request, &http::RequestBuilder::MimePost { parts });

                            Ok(Ok(vec![]))
                        }
                    )),
                )
                .run(|| report.send().unwrap());

            checked.assert_one();
        }
    }

    #[test]
    fn report_response() {
        let report = CrashReport {
            extra: &serde_json::json! {{}},
            dump_file: Path::new("minidump.dmp"),
            memory_file: None,
            url: "reports.example.com".as_ref(),
        };

        mock::builder()
                .set(
                    http::MockHttp,
                    Box::new(|_request, _url| {
                        Ok(Ok("CrashID=1234\nDiscarded=1\nStopSendingReportsFor=100\nViewURL=reports.example.com/foo".into()))
                    }),
                )
                .run(|| {
                    let response = report.send().unwrap().finish().unwrap();
                    assert_eq!(response.crash_id.as_deref(), Some("1234"));
                    assert!(response.discarded);
                    assert_eq!(response.stop_sending_reports_for.as_deref(), Some("100"));
                    assert_eq!(response.view_url.as_deref(), Some("reports.example.com/foo"));
                });
    }
}
