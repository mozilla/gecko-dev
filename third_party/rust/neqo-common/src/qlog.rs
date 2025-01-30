// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    fmt::{self, Display},
    fs::OpenOptions,
    io::BufWriter,
    path::PathBuf,
    rc::Rc,
    time::{Instant, SystemTime},
};

use qlog::{
    streamer::QlogStreamer, CommonFields, Configuration, TraceSeq, VantagePoint, VantagePointType,
};

use crate::Role;

#[allow(clippy::module_name_repetitions)]
#[derive(Debug, Clone, Default)]
pub struct NeqoQlog {
    inner: Rc<RefCell<Option<NeqoQlogShared>>>,
}

pub struct NeqoQlogShared {
    qlog_path: PathBuf,
    streamer: QlogStreamer,
}

impl NeqoQlog {
    /// Create an enabled `NeqoQlog` configuration backed by a file.
    ///
    /// # Errors
    ///
    /// Will return `qlog::Error` if it cannot write to the new file.
    pub fn enabled_with_file(
        mut qlog_path: PathBuf,
        role: Role,
        title: Option<String>,
        description: Option<String>,
        file_prefix: impl Display,
    ) -> Result<Self, qlog::Error> {
        qlog_path.push(format!("{file_prefix}.sqlog"));

        let file = OpenOptions::new()
            .write(true)
            // As a server, the original DCID is chosen by the client. Using
            // create_new() prevents attackers from overwriting existing logs.
            .create_new(true)
            .open(&qlog_path)
            .map_err(qlog::Error::IoError)?;

        let streamer = QlogStreamer::new(
            qlog::QLOG_VERSION.to_string(),
            title,
            description,
            None,
            std::time::Instant::now(),
            new_trace(role),
            qlog::events::EventImportance::Base,
            Box::new(BufWriter::new(file)),
        );
        Self::enabled(streamer, qlog_path)
    }

    /// Create an enabled `NeqoQlog` configuration.
    ///
    /// # Errors
    ///
    /// Will return `qlog::Error` if it cannot write to the new log.
    pub fn enabled(mut streamer: QlogStreamer, qlog_path: PathBuf) -> Result<Self, qlog::Error> {
        streamer.start_log()?;

        Ok(Self {
            inner: Rc::new(RefCell::new(Some(NeqoQlogShared {
                qlog_path,
                streamer,
            }))),
        })
    }

    #[must_use]
    pub fn inner(&self) -> Rc<RefCell<Option<NeqoQlogShared>>> {
        Rc::clone(&self.inner)
    }

    /// Create a disabled `NeqoQlog` configuration.
    #[must_use]
    pub fn disabled() -> Self {
        Self::default()
    }

    /// If logging enabled, closure may generate an event to be logged.
    pub fn add_event_with_instant<F>(&self, f: F, now: Instant)
    where
        F: FnOnce() -> Option<qlog::events::Event>,
    {
        self.add_event_with_stream(|s| {
            if let Some(evt) = f() {
                s.add_event_with_instant(evt, now)?;
            }
            Ok(())
        });
    }

    /// If logging enabled, closure may generate an event to be logged.
    pub fn add_event_data_with_instant<F>(&self, f: F, now: Instant)
    where
        F: FnOnce() -> Option<qlog::events::EventData>,
    {
        self.add_event_with_stream(|s| {
            if let Some(ev_data) = f() {
                s.add_event_data_with_instant(ev_data, now)?;
            }
            Ok(())
        });
    }

    /// If logging enabled, closure may generate an event to be logged.
    ///
    /// This function is similar to [`NeqoQlog::add_event_data_with_instant`],
    /// but it does not take `now: Instant` as an input parameter. Instead, it
    /// internally calls [`std::time::Instant::now`]. Prefer calling
    /// [`NeqoQlog::add_event_data_with_instant`] when `now` is available, as it
    /// ensures consistency with the current time, which might differ from
    /// [`std::time::Instant::now`] (e.g., when using simulated time instead of
    /// real time).
    pub fn add_event_data_now<F>(&self, f: F)
    where
        F: FnOnce() -> Option<qlog::events::EventData>,
    {
        self.add_event_with_stream(|s| {
            if let Some(ev_data) = f() {
                s.add_event_data_now(ev_data)?;
            }
            Ok(())
        });
    }

    /// If logging enabled, closure is given the Qlog stream to write events and
    /// frames to.
    pub fn add_event_with_stream<F>(&self, f: F)
    where
        F: FnOnce(&mut QlogStreamer) -> Result<(), qlog::Error>,
    {
        if let Some(inner) = self.inner.borrow_mut().as_mut() {
            if let Err(e) = f(&mut inner.streamer) {
                crate::do_log!(
                    ::log::Level::Error,
                    "Qlog event generation failed with error {}; closing qlog.",
                    e
                );
                *self.inner.borrow_mut() = None;
            }
        }
    }
}

impl fmt::Debug for NeqoQlogShared {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "NeqoQlog writing to {}", self.qlog_path.display())
    }
}

impl Drop for NeqoQlogShared {
    fn drop(&mut self) {
        if let Err(e) = self.streamer.finish_log() {
            crate::do_log!(::log::Level::Error, "Error dropping NeqoQlog: {}", e);
        }
    }
}

#[must_use]
pub fn new_trace(role: Role) -> qlog::TraceSeq {
    TraceSeq {
        vantage_point: VantagePoint {
            name: Some(format!("neqo-{role}")),
            ty: match role {
                Role::Client => VantagePointType::Client,
                Role::Server => VantagePointType::Server,
            },
            flow: None,
        },
        title: Some(format!("neqo-{role} trace")),
        description: Some("Example qlog trace description".to_string()),
        configuration: Some(Configuration {
            time_offset: Some(0.0),
            original_uris: None,
        }),
        common_fields: Some(CommonFields {
            group_id: None,
            protocol_type: None,
            reference_time: SystemTime::now()
                .duration_since(SystemTime::UNIX_EPOCH)
                .map(|d| d.as_secs_f64() * 1_000.0)
                .ok(),
            time_format: Some("relative".to_string()),
        }),
    }
}

#[cfg(test)]
mod test {
    use std::time::Instant;

    use qlog::events::Event;
    use regex::Regex;
    use test_fixture::EXPECTED_LOG_HEADER;

    const EV_DATA: qlog::events::EventData =
        qlog::events::EventData::SpinBitUpdated(qlog::events::connectivity::SpinBitUpdated {
            state: true,
        });

    const EXPECTED_LOG_EVENT: &str = concat!(
        "\u{1e}",
        r#"{"time":0.0,"name":"connectivity:spin_bit_updated","data":{"state":true}}"#,
        "\n"
    );

    #[test]
    fn new_neqo_qlog() {
        let (_log, contents) = test_fixture::new_neqo_qlog();
        assert_eq!(contents.to_string(), EXPECTED_LOG_HEADER);
    }

    #[test]
    fn add_event_with_instant() {
        let (log, contents) = test_fixture::new_neqo_qlog();
        log.add_event_with_instant(|| Some(Event::with_time(0.0, EV_DATA)), Instant::now());
        assert_eq!(
            Regex::new("\"time\":[0-9].[0-9]*,")
                .unwrap()
                .replace(&contents.to_string(), "\"time\":0.0,"),
            format!("{EXPECTED_LOG_HEADER}{EXPECTED_LOG_EVENT}"),
        );
    }
}
