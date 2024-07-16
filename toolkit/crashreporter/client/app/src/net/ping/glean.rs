/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Glean crash ping support. This mainly sets glean metrics which will be sent later.

use crate::glean;
use anyhow::Context;

/// Set glean metrics to be sent in the crash ping.
pub fn set_crash_ping_metrics(
    extra: &serde_json::Value,
    minidump_hash: Option<&str>,
) -> anyhow::Result<()> {
    let now: time::OffsetDateTime = crate::std::time::SystemTime::now().into();
    glean::crash::process_type.set("main".into());
    glean::crash::time.set(Some(glean_datetime(now)));
    if let Some(hash) = minidump_hash {
        glean::crash::minidump_sha256_hash.set(hash.into());
    }

    macro_rules! set_metrics_from_extra {
        ( ) => {};
        ( $category:ident { $($inner:tt)+ } $($rest:tt)* ) => {
            set_metrics_from_extra!(@metrics $category, $($inner)+);
            set_metrics_from_extra!($($rest)*);
        };
        ( @metrics $category:ident, $metric:ident : $type:tt = $key:literal $($next:tt $($rest:tt)*)? ) => {
            if let Some(value) = extra.get($key) {
                (|| -> anyhow::Result<()> {
                    set_metrics_from_extra!(@set glean::$category::$metric , $type , value);
                    Ok(())
                })().context(concat!("while trying to set glean::", stringify!($category), "::", stringify!($metric), " from extra data key ", $key))?;
            }
            $(set_metrics_from_extra!(@metrics $category, $next $($rest)*);)?
        };
        ( @set $metric:expr , bool , $val:expr ) => {
            $metric.set(
                $val.as_str()
                    .map(|s| s == "1")
                    .context("expected a string")?
            );
        };
        ( @set $metric:expr , str , $val:expr ) => {
            $metric.set(
                $val.as_str()
                    .context("expected a string")?
                    .to_owned()
            );
        };
        ( @set $metric:expr , quantity , $val:expr ) => {
            $metric.set(
                $val.as_i64()
                    .context("expected a number")?
            );
        };
        ( @set $metric:expr , seconds , $val:expr ) => {
            $metric.set_raw(
                crate::std::time::Duration::from_secs_f32(
                    $val.as_str().context("expected a string")?
                        .parse().context("couldn't parse floating point value")?
                )
            );
        };
        ( @set $metric:expr , (object $f:ident) , $val:expr ) => {
            $metric.set_string(
                serde_json::to_string(&$f($val)?).context("failed to serialize data")?
            );
        };
        ( @set $metric:expr , (string_list $separator:literal) , $val:expr ) => {
            $metric.set(
                $val.as_str().context("expected a string")?
                    .split($separator)
                    .filter(|s| !s.is_empty())
                    .map(|s| s.to_owned())
                    .collect()
            );
        };
    }

    set_metrics_from_extra! {
        crash {
            app_channel: str = "ReleaseChannel"
            app_display_version: str = "Version"
            app_build: str = "BuildID"
            async_shutdown_timeout: (object convert_async_shutdown_timeout) = "AsyncShutdownTimeout"
            background_task_name: str = "BackgroundTaskName"
            event_loop_nesting_level: quantity = "EventLoopNestingLevel"
            font_name: str = "FontName"
            gpu_process_launch: quantity = "GPUProcessLaunchCount"
            ipc_channel_error: str = "ipc_channel_error"
            is_garbage_collecting: bool = "IsGarbageCollecting"
            main_thread_runnable_name: str = "MainThreadRunnableName"
            moz_crash_reason: str = "MozCrashReason"
            profiler_child_shutdown_phase: str = "ProfilerChildShutdownPhase"
            quota_manager_shutdown_timeout: (object convert_quota_manager_shutdown_timeout) = "QuotaManagerShutdownTimeout"
            remote_type: str = "RemoteType"
            shutdown_progress: str = "ShutdownProgress"
            stack_traces: (object convert_stack_traces) = "StackTraces"
            startup: bool = "StartupCrash"
        }
        crash_windows {
            error_reporting: bool = "WindowsErrorReporting"
            file_dialog_error_code: str = "WindowsFileDialogErrorCode"
        }
        dll_blocklist {
            list: (string_list ';') = "BlockedDllList"
            init_failed: bool = "BlocklistInitFailed"
            user32_loaded_before: bool = "User32BeforeBlocklist"
        }
        environment {
            experimental_features: (string_list ',') = "ExperimentalFeatures"
            headless_mode: bool = "HeadlessMode"
            uptime: seconds = "UptimeTS"
        }
        memory {
            available_commit: quantity = "AvailablePageFile"
            available_physical: quantity = "AvailablePhysicalMemory"
            available_swap: quantity = "AvailableSwapMemory"
            available_virtual: quantity = "AvailableVirtualMemory"
            low_physical: quantity = "LowPhysicalMemoryEvents"
            oom_allocation_size: quantity = "OOMAllocationSize"
            purgeable_physical: quantity = "PurgeablePhysicalMemory"
            system_use_percentage: quantity = "SystemMemoryUsePercentage"
            texture: quantity = "TextureUsage"
            total_page_file: quantity = "TotalPageFile"
            total_physical: quantity = "TotalPhysicalMemory"
            total_virtual: quantity = "TotalVirtualMemory"
        }
        windows {
            package_family_name: str = "WindowsPackageFamilyName"
        }
    }

    Ok(())
}

fn glean_datetime(datetime: time::OffsetDateTime) -> ::glean::Datetime {
    ::glean::Datetime {
        year: datetime.year(),
        month: datetime.month() as _,
        day: datetime.day() as _,
        hour: datetime.hour() as _,
        minute: datetime.minute() as _,
        second: datetime.second() as _,
        nanosecond: datetime.nanosecond(),
        offset_seconds: datetime.offset().whole_seconds(),
    }
}

fn convert_async_shutdown_timeout(value: &serde_json::Value) -> anyhow::Result<serde_json::Value> {
    let mut ret = value.as_object().context("expected object")?.clone();
    if let Some(conditions) = ret.get_mut("conditions") {
        if !conditions.is_string() {
            *conditions = serde_json::to_string(conditions)
                .context("failed to serialize conditions")?
                .into();
        }
    }
    if let Some(blockers) = ret.remove("brokenAddBlockers") {
        ret.insert("broken_add_blockers".into(), blockers);
    }
    Ok(ret.into())
}

fn convert_quota_manager_shutdown_timeout(
    value: &serde_json::Value,
) -> anyhow::Result<serde_json::Value> {
    // The Glean metric is an array of the lines.
    Ok(value
        .as_str()
        .context("expected string")?
        .lines()
        .collect::<Vec<_>>()
        .into())
}

fn convert_stack_traces(value: &serde_json::Value) -> anyhow::Result<serde_json::Value> {
    // glean stack_traces has a slightly different layout
    let mut st = value.as_object().context("expected object")?.clone();
    if let Some(v) = st.remove("status") {
        if v != "OK" {
            st.insert("error".into(), v.into());
        }
    }

    if let Some(mut v) = st.remove("crash_info").and_then(|v| match v {
        serde_json::Value::Object(m) => Some(m),
        _ => None,
    }) {
        if let Some(t) = v.remove("type") {
            st.insert("crash_type".into(), t);
        }
        if let Some(a) = v.remove("address") {
            st.insert("crash_address".into(), a);
        }
        if let Some(ct) = v.remove("crashing_thread") {
            st.insert("crash_thread".into(), ct);
        }
    }

    if let Some(modules) = st.get_mut("modules").and_then(|v| v.as_array_mut()) {
        for m in modules.iter_mut().filter_map(|m| m.as_object_mut()) {
            if let Some(v) = m.remove("base_addr") {
                m.insert("base_address".into(), v);
            }
            if let Some(v) = m.remove("end_addr") {
                m.insert("end_address".into(), v);
            }
        }
    }

    Ok(st.into())
}
