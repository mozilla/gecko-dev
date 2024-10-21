/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Context;
use clap::Parser;
use futures_executor::{block_on, ThreadPool};
use minidump::{
    system_info::Cpu, Minidump, MinidumpException, MinidumpMemoryList, MinidumpMiscInfo,
    MinidumpModule, MinidumpModuleList, MinidumpSystemInfo, MinidumpThread, MinidumpThreadList,
    MinidumpUnloadedModule, MinidumpUnloadedModuleList, Module, UnifiedMemoryList,
};
use minidump_unwind::{
    symbols::debuginfo::DebugInfoSymbolProvider, symbols::SymbolProvider, walk_stack, CallStack,
    CallStackInfo, SystemInfo,
};
use std::fmt::Debug;
use std::fs::File;
use std::io::Read;
use std::path::PathBuf;
use std::sync::Arc;

use serde_json::{json, Value as JsonValue};

#[cfg(windows)]
mod windows;

/// Analyze a minidump file to augment a corresponding .extra file with stack trace information.
#[derive(Parser, Debug)]
#[clap(version)]
struct Args {
    /// Generate all stacks, rather than just those of the crashing thread.
    #[clap(long = "full")]
    all_stacks: bool,

    /// The minidump file to analyze.
    minidump: PathBuf,
}

impl Args {
    /// Get the extra file.
    ///
    /// This file is derived from the minidump file path.
    pub fn extra_file(&self) -> PathBuf {
        let mut ret = self.minidump.clone();
        ret.set_extension("extra");
        ret
    }
}

mod processor {
    use super::*;

    pub struct Processor<'a> {
        runtime: ThreadPool,
        // We create a Context abstraction to easily spawn tokio tasks (which must be 'static).
        context: Arc<Ctx<'a>>,
    }

    struct Ctx<'a> {
        module_list: MinidumpModuleList,
        unloaded_module_list: MinidumpUnloadedModuleList,
        memory_list: UnifiedMemoryList<'a>,
        system_info: MinidumpSystemInfo,
        processor_system_info: SystemInfo,
        exception: MinidumpException<'a>,
        misc_info: Option<MinidumpMiscInfo>,
        symbol_provider: BoxedSymbolProvider,
    }

    /// Concurrently execute the given futures, returning a Vec of the results.
    async fn concurrently<'a, I, Fut, R>(runtime: &R, iter: I) -> Vec<Fut::Output>
    where
        R: futures_util::task::Spawn,
        I: IntoIterator<Item = Fut>,
        Fut: std::future::Future + Send + 'a,
        // It's possible, though very obtuse, to support `'a` on the Output. We don't need it
        // though, so we keep it `'static` to simplify things.
        Fut::Output: Send + 'static,
    {
        use futures_util::{
            future::{join_all, BoxFuture, FutureExt},
            task::SpawnExt,
        };

        join_all(iter.into_iter().map(|f| {
            let fut: BoxFuture<'a, Fut::Output> = f.boxed();
            // Safety: It is safe to transmute to a static lifetime because we await the output of
            // the future while the `'a` lifetime is guaranteed to be valid (before exit from this
            // function).
            let fut: BoxFuture<'static, Fut::Output> = unsafe { std::mem::transmute(fut) };
            runtime.spawn_with_handle(fut).expect("spawn failed")
        }))
        .await
    }

    impl<'a> Processor<'a> {
        pub fn new<T>(minidump: &'a Minidump<T>) -> anyhow::Result<Self>
        where
            T: std::ops::Deref<Target = [u8]>,
        {
            let system_info = minidump.get_stream::<MinidumpSystemInfo>()?;
            let misc_info = minidump.get_stream::<MinidumpMiscInfo>().ok();
            let module_list = minidump
                .get_stream::<MinidumpModuleList>()
                .unwrap_or_default();
            let unloaded_module_list = minidump
                .get_stream::<MinidumpUnloadedModuleList>()
                .unwrap_or_default();
            let memory_list = minidump
                .get_stream::<MinidumpMemoryList>()
                .unwrap_or_default();
            let exception = minidump.get_stream::<MinidumpException>()?;

            // TODO Something like SystemInfo::current() to get the active system's info?
            let processor_system_info = SystemInfo {
                os: system_info.os,
                os_version: None,
                os_build: None,
                cpu: system_info.cpu,
                cpu_info: None,
                cpu_microcode_version: None,
                cpu_count: 1,
            };

            let symbol_provider = BoxedSymbolProvider(match system_info.cpu {
                // DebugInfoSymbolProvider only supports x86_64 and Arm64 right now
                Cpu::X86_64 | Cpu::Arm64 => Box::new(block_on(DebugInfoSymbolProvider::new(
                    &system_info,
                    &module_list,
                ))),
                _ => Box::new(breakpad_symbols::Symbolizer::new(
                    breakpad_symbols::SimpleSymbolSupplier::new(vec![]),
                )),
            });

            Ok(Processor {
                runtime: ThreadPool::new()?,
                context: Arc::new(Ctx {
                    module_list,
                    unloaded_module_list,
                    memory_list: UnifiedMemoryList::Memory(memory_list),
                    system_info,
                    processor_system_info,
                    exception,
                    misc_info,
                    symbol_provider,
                }),
            })
        }

        /// Get the minidump system info.
        pub fn system_info(&self) -> &MinidumpSystemInfo {
            &self.context.system_info
        }

        /// Get the minidump exception.
        pub fn exception(&self) -> &MinidumpException {
            &self.context.exception
        }

        /// Get call stacks for the given threads.
        ///
        /// Call stacks will be concurrently calculated.
        pub fn thread_call_stacks<'b>(
            &self,
            threads: impl IntoIterator<Item = &'b MinidumpThread<'b>>,
        ) -> anyhow::Result<Vec<CallStack>> {
            Ok(block_on(concurrently(
                &self.runtime,
                threads
                    .into_iter()
                    .map(|thread| self.context.thread_call_stack(thread)),
            ))
            .into_iter()
            .collect())
        }

        /// Get all modules, ordered by address.
        #[cfg(windows)]
        pub fn ordered_modules(&self) -> impl Iterator<Item = &MinidumpModule> {
            self.context.module_list.by_addr()
        }

        /// Get all unloaded modules, ordered by address.
        pub fn unloaded_modules(&self) -> impl Iterator<Item = &MinidumpUnloadedModule> {
            self.context.unloaded_module_list.by_addr()
        }

        /// Get the index of the main module.
        ///
        /// Returns `None` when no main module exists (only when there are modules).
        pub fn main_module(&self) -> Option<&MinidumpModule> {
            self.context.module_list.main_module()
        }

        /// Get the json representation of module signature information.
        #[cfg(windows)]
        pub fn module_signature_info(&self) -> JsonValue {
            // JSON with structure { <binary_org_name>: [<code_file filename>...], ... }
            let mut ret = json!({});
            for module in self
                .ordered_modules()
                .map(|m| m as &dyn Module)
                .chain(self.unloaded_modules().map(|m| m as &dyn Module))
            {
                let code_file = module.code_file();
                let code_file_path: &std::path::Path = code_file.as_ref().as_ref();
                if let Some(org_name) = windows::binary_org_name(code_file_path) {
                    let entry = &mut ret[org_name];

                    if entry.is_null() {
                        *entry = json!([]);
                    }
                    entry.as_array_mut().unwrap().push(
                        code_file_path
                            .file_name()
                            .map(|s| s.to_string_lossy())
                            .into(),
                    );
                } else {
                    log::warn!("couldn't get binary org name for {code_file}");
                }
            }

            ret
        }

        /// Get the json representation of module signature information.
        ///
        /// This is currently unimplemented and returns null.
        #[cfg(unix)]
        pub fn module_signature_info(&self) -> JsonValue {
            JsonValue::Null
        }
    }

    impl Ctx<'_> {
        /// Compute the call stack for a single thread.
        pub async fn thread_call_stack(&self, thread: &MinidumpThread<'_>) -> CallStack {
            let context = if thread.raw.thread_id == self.exception.get_crashing_thread_id() {
                self.exception
                    .context(&self.system_info, self.misc_info.as_ref())
            } else {
                thread.context(&self.system_info, self.misc_info.as_ref())
            }
            .map(|c| c.into_owned());
            let stack_memory = thread.stack_memory(&self.memory_list);
            let Some(mut call_stack) = context.map(CallStack::with_context) else {
                return CallStack::with_info(thread.raw.thread_id, CallStackInfo::MissingContext);
            };

            walk_stack(
                0,
                (),
                &mut call_stack,
                stack_memory,
                &self.module_list,
                &self.processor_system_info,
                &self.symbol_provider,
            )
            .await;

            call_stack
        }
    }
}

struct BoxedSymbolProvider(Box<dyn SymbolProvider + Send + Sync>);

#[async_trait::async_trait]
impl SymbolProvider for BoxedSymbolProvider {
    async fn fill_symbol(
        &self,
        module: &(dyn Module + Sync),
        frame: &mut (dyn minidump_unwind::FrameSymbolizer + Send),
    ) -> Result<(), minidump_unwind::FillSymbolError> {
        self.0.fill_symbol(module, frame).await
    }

    async fn walk_frame(
        &self,
        module: &(dyn Module + Sync),
        walker: &mut (dyn minidump_unwind::FrameWalker + Send),
    ) -> Option<()> {
        self.0.walk_frame(module, walker).await
    }

    async fn get_file_path(
        &self,
        module: &(dyn Module + Sync),
        file_kind: minidump_unwind::FileKind,
    ) -> Result<PathBuf, minidump_unwind::FileError> {
        self.0.get_file_path(module, file_kind).await
    }

    fn stats(&self) -> std::collections::HashMap<String, minidump_unwind::SymbolStats> {
        self.0.stats()
    }

    fn pending_stats(&self) -> minidump_unwind::PendingSymbolStats {
        self.0.pending_stats()
    }
}

use processor::Processor;

pub fn main() {
    env_logger::init();

    if let Err(e) = try_main() {
        eprintln!("{e}");
        std::process::exit(1);
    }
}

fn try_main() -> anyhow::Result<()> {
    let args = Args::parse();
    let extra_file = args.extra_file();

    log::info!("minidump file path: {}", args.minidump.display());
    log::info!("extra file path: {}", extra_file.display());

    let minidump = Minidump::read_path(&args.minidump).context("while reading minidump")?;

    let mut extra_json: JsonValue = {
        let mut extra_file_content = String::new();
        File::open(&extra_file)
            .context("while opening extra file")?
            .read_to_string(&mut extra_file_content)
            .context("while reading extra file")?;

        serde_json::from_str(&extra_file_content).context("while parsing extra file JSON")?
    };

    // Read relevant information from the minidump.
    let proc = Processor::new(&minidump)?;
    let thread_list = minidump.get_stream::<MinidumpThreadList>()?;

    // Derive additional arguments used in stack walking.
    let crashing_thread = thread_list
        .get_thread(proc.exception().get_crashing_thread_id())
        .ok_or(anyhow::anyhow!(
            "exception thread id missing in thread list"
        ))?;

    let (crashing_thread_idx, call_stacks) = if args.all_stacks {
        (
            thread_list
                .threads
                .iter()
                .position(|t| t.raw.thread_id == crashing_thread.raw.thread_id)
                .expect("get_thread() returned a thread that doesn't exist"),
            proc.thread_call_stacks(&thread_list.threads)?,
        )
    } else {
        (0, proc.thread_call_stacks([crashing_thread])?)
    };

    let crash_type = proc
        .exception()
        .get_crash_reason(proc.system_info().os, proc.system_info().cpu)
        .to_string();
    let crash_address = proc
        .exception()
        .get_crash_address(proc.system_info().os, proc.system_info().cpu);

    let used_modules = {
        let mut v = call_stacks
            .iter()
            .flat_map(|call_stack| call_stack.frames.iter())
            .filter_map(|frame| frame.module.as_ref())
            // Always include the main module.
            .chain(proc.main_module())
            .collect::<Vec<_>>();
        v.sort_by_key(|m| m.base_address());
        v.dedup_by_key(|m| m.base_address());
        v
    };

    extra_json["StackTraces"] = json!({
        "status": call_stack_status(&call_stacks),
        "crash_info": {
            "type": crash_type,
            "address": format!("{crash_address:#x}"),
            "crashing_thread": crashing_thread_idx
            // TODO: "assertion" when there's no crash indicator
        },
        "main_module": proc.main_module().and_then(|m| module_index(&used_modules, m)),
        "modules": used_modules.iter().map(|module| {
            let code_file = module.code_file();
            let code_file_path: &std::path::Path = code_file.as_ref().as_ref();
            json!({
                "base_addr": format!("{:#x}", module.base_address()),
                "end_addr": format!("{:#x}", module.base_address() + module.size()),
                "filename": code_file_path.file_name().map(|s| s.to_string_lossy()),
                "code_id": module.code_identifier().as_ref().map(|id| id.as_str()),
                "debug_file": module.debug_file().as_deref(),
                "debug_id": module.debug_identifier().map(|debug| debug.breakpad().to_string()),
                "version": module.version().as_deref()
            })
        }).collect::<Vec<_>>(),
        "unloaded_modules": proc.unloaded_modules().map(|module| {
            let code_file = module.code_file();
            let code_file_path: &std::path::Path = code_file.as_ref().as_ref();
            json!({
                "base_addr": format!("{:#x}", module.base_address()),
                "end_addr": format!("{:#x}", module.base_address() + module.size()),
                "filename": code_file_path.file_name().map(|s| s.to_string_lossy()),
                "code_id": module.code_identifier().as_ref().map(|id| id.as_str()),
            })
        }).collect::<Vec<_>>(),
        "threads": call_stacks.iter().map(|call_stack| call_stack_to_json(call_stack, &used_modules)).collect::<Vec<_>>()
    });

    // StackTraces should not have null values (upstream processing expects the values to be
    // omitted).
    remove_nulls(&mut extra_json["StackTraces"]);

    let module_signature_info = proc.module_signature_info();
    if !module_signature_info.is_null() {
        // ModuleSignatureInfo is sent as a crash annotation so must be string. This differs from
        // StackTraces which isn't actually sent (it's just read and removed by the crash
        // reporter client).
        extra_json["ModuleSignatureInfo"] = serde_json::to_string(&module_signature_info)
            .unwrap()
            .into();
    }

    std::fs::write(&extra_file, extra_json.to_string())
        .context("while writing modified extra file")?;

    Ok(())
}

/// Get the index of `needle` in `modules`.
fn module_index(modules: &[&MinidumpModule], needle: &MinidumpModule) -> Option<usize> {
    modules
        .iter()
        .position(|o| o.base_address() == needle.base_address())
}

/// Convert a call stack to json (in a form appropriate for the extra json file).
fn call_stack_to_json(call_stack: &CallStack, modules: &[&MinidumpModule]) -> JsonValue {
    json!({
        "frames": call_stack.frames.iter().map(|frame| {
            json!({
                "ip": format!("{:#x}", frame.instruction),
                "module_index": frame.module.as_ref().and_then(|m| module_index(modules, m)),
                "trust": frame.trust.as_str(),
            })
        }).collect::<Vec<_>>()
    })
}

fn call_stack_status(stacks: &[CallStack]) -> JsonValue {
    let mut error_string = String::new();

    for (_i, s) in stacks.iter().enumerate() {
        match s.info {
            CallStackInfo::Ok | CallStackInfo::DumpThreadSkipped => (),
            CallStackInfo::UnsupportedCpu => {
                // If the CPU is unsupported, it ought to be the same error for every thread.
                error_string = "unsupported cpu".into();
                break;
            }
            // We ignore these errors as they are permissible wrt the overall status.
            CallStackInfo::MissingContext | CallStackInfo::MissingMemory => (),
        }
    }
    if error_string.is_empty() {
        "OK".into()
    } else {
        error_string.into()
    }
}

/// Remove all object entries which have null values.
fn remove_nulls(value: &mut JsonValue) {
    match value {
        JsonValue::Array(vals) => {
            for v in vals {
                remove_nulls(v);
            }
        }
        JsonValue::Object(kvs) => {
            kvs.retain(|_, v| !v.is_null());
            for v in kvs.values_mut() {
                remove_nulls(v);
            }
        }
        _ => (),
    }
}
