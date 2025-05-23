// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fmt;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use crate::ping::PingMaker;
use crate::upload::PingPayload;
use crate::Glean;

use malloc_size_of_derive::MallocSizeOf;
use uuid::Uuid;

/// Stores information about a ping.
///
/// This is required so that given metric data queued on disk we can send
/// pings with the correct settings, e.g. whether it has a client_id.
#[derive(Clone)]
pub struct PingType(Arc<InnerPing>);

#[derive(MallocSizeOf)]
struct InnerPing {
    /// The name of the ping.
    pub name: String,
    /// Whether the ping should include the client ID.
    pub include_client_id: bool,
    /// Whether the ping should be sent if it is empty
    pub send_if_empty: bool,
    /// Whether to use millisecond-precise start/end times.
    pub precise_timestamps: bool,
    /// Whether to include the {client|ping}_info sections on assembly.
    pub include_info_sections: bool,
    /// Whether this ping is enabled.
    pub enabled: AtomicBool,
    /// Other pings that should be scheduled when this ping is sent.
    pub schedules_pings: Vec<String>,
    /// The "reason" codes that this ping can send
    pub reason_codes: Vec<String>,

    /// True when it follows the `collection_enabled` flag (aka `upload_enabled`) flag.
    /// Otherwise it needs to be enabled through `enabled_pings`.
    follows_collection_enabled: AtomicBool,

    /// Ordered list of uploader capabilities required to upload this ping.
    uploader_capabilities: Vec<String>,
}

impl fmt::Debug for PingType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PingType")
            .field("name", &self.0.name)
            .field("include_client_id", &self.0.include_client_id)
            .field("send_if_empty", &self.0.send_if_empty)
            .field("precise_timestamps", &self.0.precise_timestamps)
            .field("include_info_sections", &self.0.include_info_sections)
            .field("enabled", &self.0.enabled.load(Ordering::Relaxed))
            .field("schedules_pings", &self.0.schedules_pings)
            .field("reason_codes", &self.0.reason_codes)
            .field(
                "follows_collection_enabled",
                &self.0.follows_collection_enabled.load(Ordering::Relaxed),
            )
            .field("uploader_capabilities", &self.0.uploader_capabilities)
            .finish()
    }
}

impl ::malloc_size_of::MallocSizeOf for PingType {
    fn size_of(&self, ops: &mut malloc_size_of::MallocSizeOfOps) -> usize {
        // Note: This is behind an `Arc`.
        // `size_of` should only be called from a single thread to avoid double-counting.
        self.0.size_of(ops)
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl PingType {
    /// Creates a new ping type for the given name, whether to include the client ID and whether to
    /// send this ping empty.
    ///
    /// # Arguments
    ///
    /// * `name` - The name of the ping.
    /// * `include_client_id` - Whether to include the client ID in the assembled ping when submitting.
    /// * `send_if_empty` - Whether the ping should be sent empty or not.
    /// * `precise_timestamps` - Whether the ping should use precise timestamps for the start and end time.
    /// * `include_info_sections` - Whether the ping should include the client/ping_info sections.
    /// * `enabled` - Whether or not this ping is enabled. Note: Data that would be sent on a disabled
    ///   ping will still be collected but is discarded rather than being submitted.
    /// * `reason_codes` - The valid reason codes for this ping.
    /// * `uploader_capabilities` - The ordered list of capabilities this ping requires to be uploaded with.
    #[allow(clippy::too_many_arguments)]
    pub fn new<A: Into<String>>(
        name: A,
        include_client_id: bool,
        send_if_empty: bool,
        precise_timestamps: bool,
        include_info_sections: bool,
        enabled: bool,
        schedules_pings: Vec<String>,
        reason_codes: Vec<String>,
        follows_collection_enabled: bool,
        uploader_capabilities: Vec<String>,
    ) -> Self {
        Self::new_internal(
            name,
            include_client_id,
            send_if_empty,
            precise_timestamps,
            include_info_sections,
            enabled,
            schedules_pings,
            reason_codes,
            follows_collection_enabled,
            uploader_capabilities,
        )
    }

    #[allow(clippy::too_many_arguments)]
    pub(crate) fn new_internal<A: Into<String>>(
        name: A,
        include_client_id: bool,
        send_if_empty: bool,
        precise_timestamps: bool,
        include_info_sections: bool,
        enabled: bool,
        schedules_pings: Vec<String>,
        reason_codes: Vec<String>,
        follows_collection_enabled: bool,
        uploader_capabilities: Vec<String>,
    ) -> Self {
        let this = Self(Arc::new(InnerPing {
            name: name.into(),
            include_client_id,
            send_if_empty,
            precise_timestamps,
            include_info_sections,
            enabled: AtomicBool::new(enabled),
            schedules_pings,
            reason_codes,
            follows_collection_enabled: AtomicBool::new(follows_collection_enabled),
            uploader_capabilities,
        }));

        // Register this ping.
        // That will happen asynchronously and not block operation.
        crate::register_ping_type(&this);

        this
    }

    /// Get the name of this Ping
    pub fn name(&self) -> &str {
        &self.0.name
    }

    /// Whether the client ID will be included in the assembled ping when submitting.
    pub fn include_client_id(&self) -> bool {
        self.0.include_client_id
    }

    /// Whether the ping should be sent if empty.
    pub fn send_if_empty(&self) -> bool {
        self.0.send_if_empty
    }

    /// Whether the ping will include precise timestamps for the start/end time.
    pub fn precise_timestamps(&self) -> bool {
        self.0.precise_timestamps
    }

    /// Whether client/ping_info sections will be included in this ping.
    pub fn include_info_sections(&self) -> bool {
        self.0.include_info_sections
    }

    /// Enable or disable a ping.
    ///
    /// Disabling a ping causes all data for that ping to be removed from storage
    /// and all pending pings of that type to be deleted.
    pub fn set_enabled(&self, enabled: bool) {
        crate::set_ping_enabled(self, enabled)
    }

    /// Store whether this ping is enabled or not.
    ///
    /// **Note**: For internal use only. Only stores the flag. Does not touch any stored data.
    /// Use the public API `PingType::set_enabled` instead.
    pub(crate) fn store_enabled(&self, enabled: bool) {
        self.0.enabled.store(enabled, Ordering::Release);
    }

    pub(crate) fn enabled(&self, glean: &Glean) -> bool {
        if self.0.follows_collection_enabled.load(Ordering::Relaxed) {
            // if this follows collection_enabled:
            // 1. check that first. if disabled, we're done
            // 2. if enabled, check server-knobs
            // 3. If that is not set, fall-through checking the ping
            if !glean.is_upload_enabled() {
                return false;
            }

            let remote_settings_config = &glean.remote_settings_config.lock().unwrap();

            if !remote_settings_config.pings_enabled.is_empty() {
                if let Some(remote_enabled) = remote_settings_config.pings_enabled.get(self.name())
                {
                    return *remote_enabled;
                }
            }
        }

        self.0.enabled.load(Ordering::Relaxed)
    }

    /// Whether the `enabled` field of this ping is set. Note that there are
    /// multiple other reasons why a ping may or may not be enabled. See
    /// `PingType::new` and `PingType::enabled` for more details.
    pub fn naively_enabled(&self) -> bool {
        self.0.enabled.load(Ordering::Relaxed)
    }

    /// Whether this ping follows the `collection_enabled` flag
    /// See InnerPing member documentation for further details.
    pub fn follows_collection_enabled(&self) -> bool {
        self.0.follows_collection_enabled.load(Ordering::Relaxed)
    }

    /// Other pings that should be scheduled when this ping is sent.
    pub fn schedules_pings(&self) -> &[String] {
        &self.0.schedules_pings
    }

    /// Reason codes that this ping can send.
    pub fn reason_codes(&self) -> &[String] {
        &self.0.reason_codes
    }

    /// The capabilities this ping requires to be uploaded under.
    pub fn uploader_capabilities(&self) -> &[String] {
        &self.0.uploader_capabilities
    }

    /// Submits the ping for eventual uploading.
    ///
    /// The ping content is assembled as soon as possible, but upload is not
    /// guaranteed to happen immediately, as that depends on the upload policies.
    ///
    /// If the ping currently contains no content, it will not be sent,
    /// unless it is configured to be sent if empty.
    ///
    /// # Arguments
    ///
    /// * `reason` - the reason the ping was triggered. Included in the
    ///   `ping_info.reason` part of the payload.
    pub fn submit(&self, reason: Option<String>) {
        let ping = PingType(Arc::clone(&self.0));

        // Need to separate access to the Glean object from access to global state.
        // `trigger_upload` itself might lock the Glean object and we need to avoid that deadlock.
        crate::dispatcher::launch(|| {
            let sent =
                crate::core::with_glean(move |glean| ping.submit_sync(glean, reason.as_deref()));
            if sent {
                let state = crate::global_state().lock().unwrap();
                if let Err(e) = state.callbacks.trigger_upload() {
                    log::error!("Triggering upload failed. Error: {}", e);
                }
            }
        })
    }

    /// Collects and submits a ping for eventual uploading.
    ///
    /// # Returns
    ///
    /// Whether the ping was succesfully assembled and queued.
    #[doc(hidden)]
    pub fn submit_sync(&self, glean: &Glean, reason: Option<&str>) -> bool {
        if !self.enabled(glean) {
            log::info!(
                "The ping '{}' is disabled and will be discarded and not submitted",
                self.0.name
            );

            return false;
        }

        let ping = &self.0;

        // Allowing `clippy::manual_filter`.
        // This causes a false positive.
        // We have a side-effect in the `else` branch,
        // so shouldn't delete it.
        #[allow(unknown_lints)]
        #[allow(clippy::manual_filter)]
        let corrected_reason = match reason {
            Some(reason) => {
                if ping.reason_codes.contains(&reason.to_string()) {
                    Some(reason)
                } else {
                    log::error!("Invalid reason code {} for ping {}", reason, ping.name);
                    None
                }
            }
            None => None,
        };

        let ping_maker = PingMaker::new();
        let doc_id = Uuid::new_v4().to_string();
        let url_path = glean.make_path(&ping.name, &doc_id);
        match ping_maker.collect(glean, self, corrected_reason, &doc_id, &url_path) {
            None => {
                log::info!(
                    "No content for ping '{}', therefore no ping queued.",
                    ping.name
                );
                false
            }
            Some(ping) => {
                if !self.enabled(glean) {
                    log::info!(
                        "The ping '{}' is disabled and will be discarded and not submitted",
                        ping.name
                    );

                    return false;
                }

                const BUILTIN_PINGS: [&str; 4] =
                    ["baseline", "metrics", "events", "deletion-request"];

                // This metric is recorded *after* the ping is collected (since
                // that is the only way to know *if* it will be submitted). The
                // implication of this is that the count for a metrics ping will
                // be included in the *next* metrics ping.
                if BUILTIN_PINGS.contains(&ping.name) {
                    glean
                        .additional_metrics
                        .pings_submitted
                        .get(ping.name)
                        .add_sync(glean, 1);
                }

                if let Err(e) = ping_maker.store_ping(glean.get_data_path(), &ping) {
                    log::warn!(
                        "IO error while writing ping to file: {}. Enqueuing upload of what we have in memory.",
                        e
                    );
                    glean.additional_metrics.io_errors.add_sync(glean, 1);
                    // `serde_json::to_string` only fails if serialization of the content
                    // fails or it contains maps with non-string keys.
                    // However `ping.content` is already a `JsonValue`,
                    // so both scenarios should be impossible.
                    let content =
                        ::serde_json::to_string(&ping.content).expect("ping serialization failed");
                    // TODO: Shouldn't we consolidate on a single collected Ping representation?
                    let ping = PingPayload {
                        document_id: ping.doc_id.to_string(),
                        upload_path: ping.url_path.to_string(),
                        json_body: content,
                        headers: Some(ping.headers),
                        body_has_info_sections: self.0.include_info_sections,
                        ping_name: self.0.name.to_string(),
                        uploader_capabilities: self.0.uploader_capabilities.clone(),
                    };

                    glean.upload_manager.enqueue_ping(glean, ping);
                    return true;
                }

                glean.upload_manager.enqueue_ping_from_file(glean, &doc_id);

                log::info!(
                    "The ping '{}' was submitted and will be sent as soon as possible",
                    ping.name
                );

                if ping.schedules_pings.is_empty() {
                    let ping_schedule = glean
                        .ping_schedule
                        .get(ping.name)
                        .map(|v| &v[..])
                        .unwrap_or(&[]);

                    if !ping_schedule.is_empty() {
                        log::info!(
                            "The ping '{}' is being used to schedule other pings: {:?}",
                            ping.name,
                            ping_schedule
                        );

                        for scheduled_ping_name in ping_schedule {
                            glean.submit_ping_by_name(scheduled_ping_name, reason);
                        }
                    }
                } else {
                    log::info!(
                        "The ping '{}' is being used to schedule other pings: {:?}",
                        ping.name,
                        ping.schedules_pings
                    );
                    for scheduled_ping_name in &ping.schedules_pings {
                        glean.submit_ping_by_name(scheduled_ping_name, reason);
                    }
                }

                true
            }
        }
    }
}
