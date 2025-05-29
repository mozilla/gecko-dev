// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    fmt::{self, Display},
    net::{SocketAddr, ToSocketAddrs as _},
    path::PathBuf,
    time::Duration,
};

use clap::Parser;
use neqo_transport::{
    tparams::PreferredAddress, CongestionControlAlgorithm, ConnectionParameters, StreamType,
    Version,
};

pub mod client;
mod send_data;
pub mod server;
pub mod udp;

/// Firefox default value
///
/// See `network.buffer.cache.size` pref <https://searchfox.org/mozilla-central/rev/f6e3b81aac49e602f06c204f9278da30993cdc8a/modules/libpref/init/all.js#3212>
const STREAM_IO_BUFFER_SIZE: usize = 32 * 1024;

#[derive(Debug, Parser)]
pub struct SharedArgs {
    #[command(flatten)]
    verbose: Option<clap_verbosity_flag::Verbosity>,

    #[arg(short = 'a', long, default_value = "h3")]
    /// ALPN labels to negotiate.
    ///
    /// This client still only does HTTP/3 no matter what the ALPN says.
    alpn: String,

    #[arg(name = "qlog-dir", long, value_parser=clap::value_parser!(PathBuf))]
    /// Enable QLOG logging and QLOG traces to this directory
    qlog_dir: Option<PathBuf>,

    #[arg(name = "encoder-table-size", long, default_value = "16384")]
    max_table_size_encoder: u64,

    #[arg(name = "decoder-table-size", long, default_value = "16384")]
    max_table_size_decoder: u64,

    #[arg(name = "max-blocked-streams", short = 'b', long, default_value = "10")]
    max_blocked_streams: u16,

    #[arg(short = 'c', long, number_of_values = 1)]
    /// The set of TLS cipher suites to enable.
    /// From: `TLS_AES_128_GCM_SHA256`, `TLS_AES_256_GCM_SHA384`, `TLS_CHACHA20_POLY1305_SHA256`.
    ciphers: Vec<String>,

    #[arg(name = "qns-test", long)]
    /// Enable special behavior for use with QUIC Network Simulator
    qns_test: Option<String>,

    #[command(flatten)]
    quic_parameters: QuicParameters,
}

#[cfg(any(test, feature = "bench"))]
impl Default for SharedArgs {
    fn default() -> Self {
        Self {
            verbose: None,
            alpn: "h3".into(),
            qlog_dir: None,
            max_table_size_encoder: 16384,
            max_table_size_decoder: 16384,
            max_blocked_streams: 10,
            ciphers: vec![],
            qns_test: None,
            quic_parameters: QuicParameters::default(),
        }
    }
}

#[derive(Debug, Parser)]
pub struct QuicParameters {
    #[arg(
        short = 'Q',
        long,
        num_args = 1..,
        value_delimiter = ' ',
        number_of_values = 1,
        value_parser = from_str)]
    /// A list of versions to support, in hex.
    /// The first is the version to attempt.
    /// Adding multiple values adds versions in order of preference.
    /// If the first listed version appears in the list twice, the position
    /// of the second entry determines the preference order of that version.
    pub quic_version: Vec<Version>,

    #[arg(long, default_value = "16")]
    /// Set the `MAX_STREAMS_BIDI` limit.
    pub max_streams_bidi: u64,

    #[arg(long, default_value = "16")]
    /// Set the `MAX_STREAMS_UNI` limit.
    pub max_streams_uni: u64,

    #[arg(long = "idle", default_value = "30")]
    /// The idle timeout for connections, in seconds.
    pub idle_timeout: u64,

    #[arg(long = "cc", default_value = "cubic")]
    /// The congestion controller to use.
    pub congestion_control: CongestionControlAlgorithm,

    #[arg(long = "no-pacing")]
    /// Whether to disable pacing.
    pub no_pacing: bool,

    #[arg(long)]
    /// Whether to disable path MTU discovery.
    pub no_pmtud: bool,

    #[arg(long)]
    /// Whether to slice the SNI.
    pub no_sni_slicing: bool,

    #[arg(name = "preferred-address-v4", long)]
    /// An IPv4 address for the server preferred address.
    pub preferred_address_v4: Option<String>,

    #[arg(name = "preferred-address-v6", long)]
    /// An IPv6 address for the server preferred address.
    pub preferred_address_v6: Option<String>,
}

#[cfg(any(test, feature = "bench"))]
impl Default for QuicParameters {
    fn default() -> Self {
        Self {
            quic_version: vec![],
            max_streams_bidi: 16,
            max_streams_uni: 16,
            idle_timeout: 30,
            congestion_control: CongestionControlAlgorithm::Cubic,
            no_pacing: false,
            no_pmtud: false,
            preferred_address_v4: None,
            preferred_address_v6: None,
            no_sni_slicing: false,
        }
    }
}

impl QuicParameters {
    fn get_sock_addr<F>(opt: Option<&String>, v: &str, f: F) -> Option<SocketAddr>
    where
        F: FnMut(&SocketAddr) -> bool,
    {
        let addr = opt
            .iter()
            .filter_map(|spa| spa.to_socket_addrs().ok())
            .flatten()
            .find(f);
        assert_eq!(
            opt.is_some(),
            addr.is_some(),
            "unable to resolve '{:?}' to an {v} address",
            opt.as_ref()?,
        );
        addr
    }

    #[must_use]
    pub fn preferred_address_v4(&self) -> Option<SocketAddr> {
        Self::get_sock_addr(
            self.preferred_address_v4.as_ref(),
            "IPv4",
            SocketAddr::is_ipv4,
        )
    }

    #[must_use]
    pub fn preferred_address_v6(&self) -> Option<SocketAddr> {
        Self::get_sock_addr(
            self.preferred_address_v6.as_ref(),
            "IPv6",
            SocketAddr::is_ipv6,
        )
    }

    #[must_use]
    pub fn preferred_address(&self) -> Option<PreferredAddress> {
        let v4 = self.preferred_address_v4();
        let v6 = self.preferred_address_v6();
        if v4.is_none() && v6.is_none() {
            None
        } else {
            let v4 = v4.map(|v4| {
                let SocketAddr::V4(v4) = v4 else {
                    unreachable!();
                };
                v4
            });
            let v6 = v6.map(|v6| {
                let SocketAddr::V6(v6) = v6 else {
                    unreachable!();
                };
                v6
            });
            Some(PreferredAddress::new(v4, v6))
        }
    }

    #[must_use]
    pub fn get(&self, alpn: &str) -> ConnectionParameters {
        let mut params = ConnectionParameters::default()
            .max_streams(StreamType::BiDi, self.max_streams_bidi)
            .max_streams(StreamType::UniDi, self.max_streams_uni)
            .idle_timeout(Duration::from_secs(self.idle_timeout))
            .cc_algorithm(self.congestion_control)
            .pacing(!self.no_pacing)
            .pmtud(!self.no_pmtud)
            .sni_slicing(!self.no_sni_slicing);
        params = if let Some(pa) = self.preferred_address() {
            params.preferred_address(pa)
        } else {
            params
        };
        if let Some(&first) = self.quic_version.first() {
            let all = if self.quic_version[1..].contains(&first) {
                &self.quic_version[1..]
            } else {
                &self.quic_version
            };
            params.versions(first, all.to_vec())
        } else {
            let version = match alpn {
                "h3" | "hq-interop" => Version::Version1,
                #[cfg(feature = "draft-29")]
                "h3-29" | "hq-29" => Version::Draft29,
                _ => Version::default(),
            };
            params.versions(version, Version::all())
        }
    }
}

fn from_str(s: &str) -> Result<Version, Error> {
    let v = u32::from_str_radix(s, 16)
        .map_err(|_| Error::Argument("versions need to be specified in hex"))?;
    Version::try_from(v).map_err(|_| Error::Argument("unknown version"))
}

#[derive(Debug)]
pub enum Error {
    Argument(&'static str),
}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Error: {self:?}")?;
        Ok(())
    }
}

impl std::error::Error for Error {}

#[cfg(test)]
mod tests {
    use std::{fs, path::PathBuf, str::FromStr as _, time::SystemTime};

    use crate::{client, server};

    struct TempDir {
        path: PathBuf,
    }

    impl TempDir {
        fn new() -> Self {
            let dir = std::env::temp_dir().join(format!(
                "neqo-bin-test-{}",
                SystemTime::now()
                    .duration_since(SystemTime::UNIX_EPOCH)
                    .unwrap()
                    .as_secs()
            ));
            fs::create_dir_all(&dir).unwrap();
            Self { path: dir }
        }

        fn path(&self) -> PathBuf {
            self.path.clone()
        }
    }

    impl Drop for TempDir {
        fn drop(&mut self) {
            if self.path.exists() {
                fs::remove_dir_all(&self.path).unwrap();
            }
        }
    }

    #[tokio::test]
    async fn write_qlog_file() {
        neqo_crypto::init_db(PathBuf::from_str("../test-fixture/db").unwrap()).unwrap();

        let temp_dir = TempDir::new();

        let mut client_args = client::Args::new(&[1], false);
        client_args.set_qlog_dir(temp_dir.path());
        let mut server_args = server::Args::default();
        server_args.set_qlog_dir(temp_dir.path());

        let client = client::client(client_args);
        let server = Box::pin(server::server(server_args));
        tokio::select! {
            _ = client => {}
            res = server  => panic!("expect server not to terminate: {res:?}"),
        };

        // Verify that the directory contains two non-empty files
        let entries: Vec<_> = fs::read_dir(temp_dir.path())
            .unwrap()
            .filter_map(Result::ok)
            .collect();
        assert_eq!(entries.len(), 2, "expect 2 files in the directory");

        for entry in entries {
            let metadata = entry.metadata().unwrap();
            assert!(metadata.is_file(), "expect a file, found something else");
            assert!(metadata.len() > 0, "expect file not be empty");
        }
    }
}
