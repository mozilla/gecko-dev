//! Contains HTTP symbol retrieval specific functionality

use crate::*;
use reqwest::{redirect, Client, Url};
use std::io::{self, Write};
use std::path::Path;
use std::str::FromStr;
use std::time::Duration;
use tempfile::NamedTempFile;
use tracing::{debug, trace, warn};

/// A key that uniquely identifies a File associated with a module
type FileKey = (ModuleKey, FileKind);

/// An implementation of `SymbolSupplier` that loads Breakpad text-format
/// symbols from HTTP URLs.
///
/// See [`crate::breakpad_sym_lookup`] for details on how paths are searched.
pub struct HttpSymbolSupplier {
    /// File paths that are known to be in the cache
    #[allow(clippy::type_complexity)]
    cached_file_paths: CacheMap<FileKey, CachedAsyncResult<(PathBuf, Option<Url>), FileError>>,
    /// HTTP Client to use for fetching symbols.
    client: Client,
    /// URLs to search for symbols.
    urls: Vec<Url>,
    /// A `SimpleSymbolSupplier` to use for local symbol paths.
    local: SimpleSymbolSupplier,
    /// A path at which to cache downloaded symbols.
    ///
    /// We recommend using a subdirectory of `std::env::temp_dir()`, as this
    /// will be your OS's intended location for tempory files. This should
    /// give you free garbage collection of the cache while still allowing it
    /// to function between runs.
    cache: PathBuf,
    /// A path to a temporary location where downloaded symbols can be written
    /// before being atomically swapped into the cache.
    ///
    /// We recommend using `std::env::temp_dir()`, as this will be your OS's
    /// intended location for temporary files.
    tmp: PathBuf,
}

impl HttpSymbolSupplier {
    /// Create a new `HttpSymbolSupplier`.
    ///
    /// Symbols will be searched for in each of `local_paths` and `cache` first,
    /// then via HTTP at each of `urls`. If a symbol file is found via HTTP it
    /// will be saved under `cache`.
    pub fn new(
        urls: Vec<String>,
        cache: PathBuf,
        tmp: PathBuf,
        mut local_paths: Vec<PathBuf>,
        timeout: Duration,
    ) -> HttpSymbolSupplier {
        let client = Client::builder().timeout(timeout).build().unwrap();
        let urls = urls
            .into_iter()
            .filter_map(|mut u| {
                if !u.ends_with('/') {
                    u.push('/');
                }
                Url::parse(&u).ok()
            })
            .collect();
        local_paths.push(cache.clone());
        let local = SimpleSymbolSupplier::new(local_paths);
        let cached_file_paths = Default::default();
        HttpSymbolSupplier {
            client,
            cached_file_paths,
            urls,
            local,
            cache,
            tmp,
        }
    }

    #[tracing::instrument(level = "trace", skip(self, module), fields(module = crate::basename(&module.code_file())))]
    pub async fn locate_file_internal(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<(PathBuf, Option<Url>), FileError> {
        self.cached_file_paths
            .cache_default(file_key(module, file_kind))
            .get(|| async {
                // First look for the file in the cache
                if let Ok(path) = self.local.locate_file(module, file_kind).await {
                    return Ok((path, None));
                }

                // Then try to download the file
                // FIXME: if we try to parallelize this with `join` then if we have multiple hits
                // we'll end up downloading all of them at once and having them race to write into
                // the cache... is that ok? Maybe? Since only one will ever win the swap, and it's
                // unlikely to get multiple hits... this might actually be ok!
                if let Some(lookup) = lookup(module, file_kind) {
                    for url in &self.urls {
                        let fetch =
                            fetch_lookup(&self.client, url, &lookup, &self.cache, &self.tmp).await;

                        if let Ok((path, url)) = fetch {
                            return Ok((path, url));
                        }
                    }

                    // If we're allowed to look for mozilla's special CAB paths, do that
                    if cfg!(feature = "mozilla_cab_symbols") {
                        for url in &self.urls {
                            let fetch = fetch_cab_lookup(
                                &self.client,
                                url,
                                &lookup,
                                &self.cache,
                                &self.tmp,
                            )
                            .await;

                            if let Ok((path, url)) = fetch {
                                return Ok((path, url));
                            }
                        }
                    }
                }
                Err(FileError::NotFound)
            })
            .await
            .as_ref()
            .clone()
    }
}

fn file_key(module: &(dyn Module + Sync), file_kind: FileKind) -> FileKey {
    (module_key(module), file_kind)
}

fn create_cache_file(tmp_path: &Path, final_path: &Path) -> io::Result<NamedTempFile> {
    // Use tempfile to save things to our cache to ensure proper
    // atomicity of writes. We may want multiple instances of rust-minidump
    // to be sharing a cache, and we don't want one instance to see another
    // instance's partially written results.
    //
    // tempfile is designed explicitly for this purpose, and will handle all
    // the platform-specific details and do its best to cleanup if things
    // crash.

    // First ensure that the target directory in the cache exists
    let base = final_path.parent().ok_or_else(|| {
        io::Error::new(
            io::ErrorKind::Other,
            format!("Bad cache path: {final_path:?}"),
        )
    })?;
    fs::create_dir_all(base)?;

    NamedTempFile::new_in(tmp_path)
}

fn commit_cache_file(mut temp: NamedTempFile, final_path: &Path, url: &Url) -> io::Result<()> {
    // Append any extra metadata we also want to be cached as "INFO" lines,
    // because this is an established format that parsers will ignore the
    // contents of by default.

    // INFO URL allows us to properly report the url we retrieved a symbol file
    // from, even when the file is loaded from our on-disk cache.
    let cache_metadata = format!("INFO URL {url}\n");
    temp.write_all(cache_metadata.as_bytes())?;

    // TODO: don't do this
    if final_path.exists() {
        fs::remove_file(final_path)?;
    }

    // If another process already wrote this entry, prefer their value to
    // avoid needless file system churn.
    temp.persist_noclobber(final_path)?;

    Ok(())
}

/// Perform a code_file/code_identifier lookup for a specific symbol server.
async fn individual_lookup_debug_info_by_code_info(
    base_url: &Url,
    lookup_path: &str,
) -> Option<DebugInfoResult> {
    let url = base_url.join(lookup_path).ok()?;

    debug!("Trying code file / code identifier lookup: {}", url);

    // This should not follow redirects--we want the next url if there is one
    let no_redirects_client = Client::builder()
        .redirect(redirect::Policy::none())
        .build()
        .ok()?;

    let response = no_redirects_client.get(url.clone()).send().await;
    if let Ok(res) = response {
        let res_status = res.status();
        if res_status == reqwest::StatusCode::FOUND
            || res_status == reqwest::StatusCode::MOVED_PERMANENTLY
        {
            let location_header = res.headers().get("Location")?;
            let mut new_url = location_header.to_str().ok()?;
            if new_url.starts_with('/') {
                new_url = new_url.strip_prefix('/').unwrap_or(new_url);
            }

            // new_url looks like some/path/stuff/xul.pdb/somedebugid/xul.sym and we want the debug
            // file and debug id portions which are at fixed indexes from the end
            let mut parts = new_url.rsplit('/');
            let debug_identifier_part = parts.nth(1)?;
            let debug_identifier = DebugId::from_str(debug_identifier_part).ok()?;
            let debug_file_part = parts.next()?;
            let debug_file = String::from(debug_file_part);

            debug!("Found debug info {} {}", debug_file, debug_identifier);
            return Some(DebugInfoResult {
                debug_file,
                debug_identifier,
            });
        }
    }

    None
}

/// Given a vector of symbol urls and a module with a code_file and code_identifier,
/// this tries to request a symbol file using the code file and code identifier.
///
/// `<code file>/<code identifier>/<code file>.sym`
///
/// If the symbol server returns an HTTP 302 redirect, the Location header will
/// have the correct download API url with the debug file and debug identifier.
///
/// This is supported by tecken
///
/// This returns a DebugInfoResult with the new debug file and debug identifier
/// or None.
async fn lookup_debug_info_by_code_info(
    symbol_urls: &Vec<Url>,
    module: &(dyn Module + Sync),
) -> Option<DebugInfoResult> {
    let lookup_path = code_info_breakpad_sym_lookup(module)?;

    for base_url in symbol_urls {
        if let Some(result) =
            individual_lookup_debug_info_by_code_info(base_url, &lookup_path).await
        {
            return Some(result);
        }
    }

    debug!(
        "No debug file / debug id found with lookup path {}.",
        lookup_path
    );

    None
}

/// Fetch a symbol file from the URL made by combining `base_url` and `rel_path` using `client`,
/// save the file contents under `cache` + `rel_path` and also return them.
async fn fetch_symbol_file(
    client: &Client,
    base_url: &Url,
    module: &(dyn Module + Sync),
    cache: &Path,
    tmp: &Path,
) -> Result<SymbolFile, SymbolError> {
    trace!("HttpSymbolSupplier trying symbol server {}", base_url);
    // This function is a bit of a complicated mess because we want to write
    // the input to our symbol cache, but we're a streaming parser. So we
    // use the bare SymbolFile::parse to get access to the contents of
    // the input stream as it's downloaded+parsed to write it to disk.
    //
    // Note that caching is strictly "optional" because it's more important
    // to parse the symbols. So if at any point the caching i/o fails, we just
    // give up on caching but let the parse+download continue.

    // First try to GET the file from a server
    let sym_lookup = breakpad_sym_lookup(module).ok_or(SymbolError::MissingDebugFileOrId)?;
    let mut url = base_url
        .join(&sym_lookup.server_rel)
        .map_err(|_| SymbolError::NotFound)?;
    let code_id = module.code_identifier().unwrap_or_default();
    url.query_pairs_mut()
        .append_pair("code_file", crate::basename(&module.code_file()))
        .append_pair("code_id", code_id.as_str());
    debug!("Trying {}", url);
    let res = client
        .get(url.clone())
        .send()
        .await
        .and_then(|res| res.error_for_status())
        .map_err(|_| SymbolError::NotFound)?;

    // Now try to create the temp cache file (not yet in the cache)
    let final_cache_path = cache.join(sym_lookup.cache_rel);
    let mut temp = create_cache_file(tmp, &final_cache_path)
        .map_err(|e| {
            warn!("Failed to save symbol file in local disk cache: {}", e);
        })
        .ok();

    // Now stream parse the file as it downloads.
    let mut symbol_file = SymbolFile::parse_async(res, |data| {
        // While we're downloading+parsing, save this data to the the disk cache too
        if let Some(file) = temp.as_mut() {
            if let Err(e) = file.write_all(data) {
                // Give up on caching this.
                warn!("Failed to save symbol file in local disk cache: {}", e);
                temp = None;
            }
        }
    })
    .await?;
    // Make note of what URL this symbol file was downloaded from.
    symbol_file.url = Some(url.to_string());

    // Try to finish the cache file and atomically swap it into the cache.
    if let Some(temp) = temp {
        let _ = commit_cache_file(temp, &final_cache_path, &url).map_err(|e| {
            warn!("Failed to save symbol file in local disk cache: {}", e);
        });
    }

    Ok(symbol_file)
}

/// Like fetch_symbol_file but instead of parsing the file live, we just download it opaquely based
/// on the given Lookup.
///
/// The returned value is the path to the downloaded file and the url it was downloaded from.
async fn fetch_lookup(
    client: &Client,
    base_url: &Url,
    lookup: &FileLookup,
    cache: &Path,
    tmp: &Path,
) -> Result<(PathBuf, Option<Url>), SymbolError> {
    // First try to GET the file from a server
    let url = base_url
        .join(&lookup.server_rel)
        .map_err(|_| SymbolError::NotFound)?;
    debug!("Trying {}", url);
    let mut res = client
        .get(url.clone())
        .send()
        .await
        .and_then(|res| res.error_for_status())
        .map_err(|_| SymbolError::NotFound)?;

    // Now try to create the temp cache file (not yet in the cache)
    let final_cache_path = cache.join(&lookup.cache_rel);
    let mut temp = create_cache_file(tmp, &final_cache_path)?;

    // Now stream the contents to our file
    while let Some(chunk) = res
        .chunk()
        .await
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?
    {
        temp.write_all(&chunk[..])?;
    }

    // And swap it into the cache
    temp.persist_noclobber(&final_cache_path)
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?;

    trace!("symbols: fetched native binary: {}", lookup.cache_rel);

    Ok((final_cache_path, Some(url)))
}

#[cfg(feature = "mozilla_cab_symbols")]
async fn fetch_cab_lookup(
    client: &Client,
    base_url: &Url,
    lookup: &FileLookup,
    cache: &Path,
    tmp: &Path,
) -> Result<(PathBuf, Option<Url>), FileError> {
    let cab_lookup = moz_lookup(lookup.clone());
    // First try to GET the file from a server
    let url = base_url
        .join(&cab_lookup.server_rel)
        .map_err(|_| FileError::NotFound)?;
    debug!("Trying {}", url);
    let res = client
        .get(url.clone())
        .send()
        .await
        .and_then(|res| res.error_for_status())
        .map_err(|_| FileError::NotFound)?;

    let cab_bytes = res.bytes().await.map_err(|_| FileError::NotFound)?;
    let final_cache_path =
        unpack_cabinet_file(&cab_bytes, lookup, cache, tmp).map_err(|_| FileError::NotFound)?;

    trace!("symbols: fetched native binary: {}", lookup.cache_rel);

    Ok((final_cache_path, Some(url)))
}

#[cfg(not(feature = "mozilla_cab_symbols"))]
async fn fetch_cab_lookup(
    _client: &Client,
    _base_url: &Url,
    _lookup: &FileLookup,
    _cache: &Path,
    _tmp: &Path,
) -> Result<(PathBuf, Option<Url>), FileError> {
    Err(FileError::NotFound)
}

#[cfg(feature = "mozilla_cab_symbols")]
pub fn unpack_cabinet_file(
    buf: &[u8],
    lookup: &FileLookup,
    cache: &Path,
    tmp: &Path,
) -> Result<PathBuf, std::io::Error> {
    trace!("symbols: unpacking CAB file: {}", lookup.cache_rel);
    // try to find a file in a cabinet archive and unpack it to the destination
    use cab::Cabinet;
    use std::io::Cursor;
    fn get_cabinet_file(
        cab: &Cabinet<Cursor<&[u8]>>,
        file_name: &str,
    ) -> Result<String, std::io::Error> {
        for folder in cab.folder_entries() {
            for file in folder.file_entries() {
                let cab_file_name = file.name();
                if cab_file_name.ends_with(file_name) {
                    return Ok(cab_file_name.to_string());
                }
            }
        }
        Err(std::io::Error::from(std::io::ErrorKind::NotFound))
    }
    let final_cache_path = cache.join(&lookup.cache_rel);

    let cursor = Cursor::new(buf);
    let mut cab = Cabinet::new(cursor)?;
    let file_name = final_cache_path.file_name().unwrap().to_string_lossy();
    let cab_file = get_cabinet_file(&cab, &file_name)?;
    let mut reader = cab.read_file(&cab_file)?;

    // Now try to create the temp cache file (not yet in the cache)
    let mut temp = create_cache_file(tmp, &final_cache_path)?;
    std::io::copy(&mut reader, &mut temp)?;

    // And swap it into the cache
    temp.persist_noclobber(&final_cache_path)
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?;

    Ok(final_cache_path)
}

/// Try to lookup native binaries in the cache and by querying the symbol server

#[async_trait]
impl SymbolSupplier for HttpSymbolSupplier {
    #[tracing::instrument(name = "symbols", level = "trace", skip_all, fields(file = crate::basename(&module.code_file())))]
    async fn locate_symbols(
        &self,
        module: &(dyn Module + Sync),
    ) -> Result<LocateSymbolsResult, SymbolError> {
        // If we don't have a debug_file or debug_identifier, then try to get it
        // from a symbol server.
        let mut debug_file = module.debug_file().map(|name| name.into_owned());
        let mut debug_id = module.debug_identifier();
        let missing_debug_info = debug_file.is_none() || debug_id.is_none();

        let extra_debug_info;

        if missing_debug_info {
            debug!("Missing debug file or debug identifier--trying lookup with code info");
            extra_debug_info = lookup_debug_info_by_code_info(&self.urls, module).await;
            if let Some(debug_info_result) = &extra_debug_info {
                debug_file = Some(debug_info_result.debug_file.clone());
                debug_id = Some(debug_info_result.debug_identifier);
            }
        } else {
            extra_debug_info = None;
        }

        // Build a minimal module for lookups with the debug file and debug
        // identifier we need to use
        let lookup_module = SimpleModule::from_basic_info(
            debug_file,
            debug_id,
            Some(module.code_file().into_owned()),
            module.code_identifier(),
        );

        // First: try local paths for sym files
        let local_result = self.local.locate_symbols(&lookup_module).await;
        if !matches!(local_result, Err(SymbolError::NotFound)) {
            // Everything but NotFound prevents cascading
            return local_result.map(|r| LocateSymbolsResult {
                symbols: r.symbols,
                extra_debug_info: r.extra_debug_info.or(extra_debug_info),
            });
        }
        trace!("HttpSymbolSupplier search (SimpleSymbolSupplier found nothing)");

        // Second: try to directly download sym files
        for url in &self.urls {
            // First, try to get a breakpad .sym file from the symbol server
            let sym =
                fetch_symbol_file(&self.client, url, &lookup_module, &self.cache, &self.tmp).await;
            match sym {
                Ok(symbols) => {
                    trace!("HttpSymbolSupplier parsed file!");
                    return Ok(LocateSymbolsResult {
                        symbols,
                        extra_debug_info,
                    });
                }
                Err(e) => {
                    trace!("HttpSymbolSupplier failed: {}", e);
                }
            }
        }

        // If we get this far, we have failed to find anything
        Err(SymbolError::NotFound)
    }

    async fn locate_file(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError> {
        self.locate_file_internal(module, file_kind)
            .await
            .map(|(path, _url)| path)
    }
}
