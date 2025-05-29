// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![expect(
    clippy::unwrap_used,
    clippy::iter_over_hash_type,
    reason = "OK in a build script."
)]

use std::{
    collections::HashMap,
    env, fs,
    path::{Path, PathBuf},
    process::Command,
};

use bindgen::Builder;
use semver::{Version, VersionReq};
use serde_derive::Deserialize;

#[path = "src/min_version.rs"]
mod min_version;
use min_version::MINIMUM_NSS_VERSION;

const BINDINGS_DIR: &str = "bindings";
const BINDINGS_CONFIG: &str = "bindings.toml";

// This is the format of a single section of the configuration file.
#[derive(Deserialize)]
struct Bindings {
    /// types that are explicitly included
    #[serde(default)]
    types: Vec<String>,
    /// functions that are explicitly included
    #[serde(default)]
    functions: Vec<String>,
    /// variables (and `#define`s) that are explicitly included
    #[serde(default)]
    variables: Vec<String>,
    /// types that should be explicitly marked as opaque
    #[serde(default)]
    opaque: Vec<String>,
    /// enumerations that are turned into a module (without this, the enum is
    /// mapped using the default, which means that the individual values are
    /// formed with an underscore as <`enum_type`>_<`enum_value_name`>).
    #[serde(default)]
    enums: Vec<String>,

    /// Any item that is specifically excluded; if none of the types, functions,
    /// or variables fields are specified, everything defined will be mapped,
    /// so this can be used to limit that.
    #[serde(default)]
    exclude: Vec<String>,

    /// Whether the file is to be interpreted as C++
    #[serde(default)]
    cplusplus: bool,
}

// bindgen needs access to libclang.
// On windows, this doesn't just work, you have to set LIBCLANG_PATH.
// Rather than download the 400Mb+ files, like gecko does, let's just reuse their work.
fn setup_clang() {
    // If this isn't Windows, or we're in CI, then we don't need to do anything.
    if env::consts::OS != "windows" || env::var("GITHUB_WORKFLOW").unwrap_or_default() == "CI" {
        return;
    }
    println!("rerun-if-env-changed=LIBCLANG_PATH");
    println!("rerun-if-env-changed=MOZBUILD_STATE_PATH");
    if env::var("LIBCLANG_PATH").is_ok() {
        return;
    }
    let mozbuild_root = if let Ok(dir) = env::var("MOZBUILD_STATE_PATH") {
        PathBuf::from(dir.trim())
    } else {
        eprintln!("warning: Building without a gecko setup is not likely to work.");
        eprintln!("         A working libclang is needed to build neqo.");
        eprintln!("         Either LIBCLANG_PATH or MOZBUILD_STATE_PATH needs to be set.");
        eprintln!();
        eprintln!("    We recommend checking out https://github.com/mozilla/gecko-dev");
        eprintln!("    Then run `./mach bootstrap` which will retrieve clang.");
        eprintln!("    Make sure to export MOZBUILD_STATE_PATH when building.");
        return;
    };
    let libclang_dir = mozbuild_root.join("clang").join("lib");
    if libclang_dir.is_dir() {
        env::set_var("LIBCLANG_PATH", libclang_dir.to_str().unwrap());
        println!("rustc-env:LIBCLANG_PATH={}", libclang_dir.to_str().unwrap());
    } else {
        println!("warning: LIBCLANG_PATH isn't set; maybe run ./mach bootstrap with gecko");
    }
}

fn get_bash() -> PathBuf {
    // If BASH is set, use that.
    if let Ok(bash) = env::var("BASH") {
        return PathBuf::from(bash);
    }

    // When running under MOZILLABUILD, we need to make sure not to invoke
    // another instance of bash that might be sitting around (like WSL).
    env::var("MOZILLABUILD").map_or_else(
        |_| PathBuf::from("bash"),
        |d| PathBuf::from(d).join("msys").join("bin").join("bash.exe"),
    )
}

fn build_nss(dir: PathBuf, nsstarget: &str) {
    let mut build_nss = vec![
        String::from("./build.sh"),
        String::from("-Ddisable_tests=1"),
        // Generate static libraries in addition to shared libraries.
        String::from("--static"),
    ];
    if nsstarget == "Release" {
        build_nss.push(String::from("-o"));
    }
    if let Ok(d) = env::var("NSS_JOBS") {
        build_nss.push(String::from("-j"));
        build_nss.push(d);
    }
    let target = env::var("TARGET").unwrap();
    if target.strip_prefix("aarch64-").is_some() {
        build_nss.push(String::from("--target=arm64"));
    }
    let status = Command::new(get_bash())
        .args(build_nss)
        .current_dir(dir)
        .status()
        .expect("couldn't start NSS build");
    assert!(status.success(), "NSS build failed");
}

fn dynamic_link() {
    let dynamic_libs = if env::consts::OS == "windows" {
        [
            "nssutil3.dll",
            "nss3.dll",
            "ssl3.dll",
            "libplds4.dll",
            "libplc4.dll",
            "libnspr4.dll",
        ]
    } else {
        ["nssutil3", "nss3", "ssl3", "plds4", "plc4", "nspr4"]
    };
    for lib in dynamic_libs {
        println!("cargo:rustc-link-lib=dylib={lib}");
    }
}

fn static_link() {
    let mut static_libs = vec![
        "certdb",
        "certhi",
        "cryptohi",
        "freebl_static",
        if env::consts::OS == "windows" {
            "libnspr4"
        } else {
            "nspr4"
        },
        "nss_static",
        "nssb",
        "nssdev",
        "nsspki",
        "nssutil",
        "pk11wrap_static",
        if env::consts::OS == "windows" {
            "libplc4"
        } else {
            "plc4"
        },
        if env::consts::OS == "windows" {
            "libplds4"
        } else {
            "plds4"
        },
        "softokn_static",
        "ssl",
    ];
    // macOS always dynamically links against the system sqlite library.
    // See https://github.com/nss-dev/nss/blob/a8c22d8fc0458db3e261acc5e19b436ab573a961/coreconf/Darwin.mk#L130-L135
    if env::consts::OS == "macos" {
        println!("cargo:rustc-link-lib=dylib=sqlite3");
    } else {
        static_libs.push("sqlite");
    }
    // Hardware specific libs.
    // See https://github.com/mozilla/application-services/blob/0a2dac76f979b8bcfb6bacb5424b50f58520b8fe/components/support/rc_crypto/nss/nss_build_common/src/lib.rs#L127-L157
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    // https://searchfox.org/nss/rev/0d5696b3edce5124353f03159d2aa15549db8306/lib/freebl/freebl.gyp#508-542
    if target_arch == "arm" || target_arch == "aarch64" {
        static_libs.push("armv8_c_lib");
    }
    if target_arch == "x86_64" || target_arch == "x86" {
        static_libs.push("gcm-aes-x86_c_lib");
        static_libs.push("sha-x86_c_lib");
    }
    if target_arch == "arm" {
        static_libs.push("gcm-aes-arm32-neon_c_lib");
    }
    if target_arch == "aarch64" {
        static_libs.push("gcm-aes-aarch64_c_lib");
    }
    if target_arch == "x86_64" {
        static_libs.push("hw-acc-crypto-avx");
        static_libs.push("hw-acc-crypto-avx2");
    }
    // https://searchfox.org/nss/rev/08c4d05078d00089f8d7540651b0717a9d66f87e/lib/freebl/freebl.gyp#315-324
    if (target_os == "android" || target_os == "linux") && target_arch == "x86_64" {
        static_libs.push("intel-gcm-wrap_c_lib");
        // https://searchfox.org/nss/rev/08c4d05078d00089f8d7540651b0717a9d66f87e/lib/freebl/freebl.gyp#43-47
        if (target_os == "android" || target_os == "linux") && target_arch == "x86_64" {
            static_libs.push("intel-gcm-s_lib");
        }
    }
    for lib in static_libs {
        println!("cargo:rustc-link-lib=static={lib}");
    }
}

fn get_includes(nsstarget: &Path, nssdist: &Path) -> Vec<PathBuf> {
    let nsprinclude = nsstarget.join("include").join("nspr");
    let nssinclude = nssdist.join("public").join("nss");
    let includes = vec![nsprinclude, nssinclude];
    for i in &includes {
        println!("cargo:include={}", i.to_str().unwrap());
    }
    includes
}

fn build_bindings(base: &str, bindings: &Bindings, flags: &[String], gecko: bool) {
    let suffix = if bindings.cplusplus { ".hpp" } else { ".h" };
    let header_path = PathBuf::from(BINDINGS_DIR).join(String::from(base) + suffix);
    let header = header_path.to_str().unwrap();
    let out = PathBuf::from(env::var("OUT_DIR").unwrap()).join(String::from(base) + ".rs");

    println!("cargo:rerun-if-changed={header}");

    let mut builder = Builder::default().header(header);
    builder = builder.generate_comments(false);
    builder = builder.size_t_is_usize(true);

    builder = builder.clang_arg("-v");

    if !gecko {
        builder = builder.clang_arg("-DNO_NSPR_10_SUPPORT");
        if env::consts::OS == "windows" {
            builder = builder.clang_arg("-DWIN");
        } else if env::consts::OS == "macos" {
            builder = builder.clang_arg("-DDARWIN");
        } else if env::consts::OS == "linux" {
            builder = builder.clang_arg("-DLINUX");
        } else if env::consts::OS == "android" {
            builder = builder.clang_arg("-DLINUX");
            builder = builder.clang_arg("-DANDROID");
        }
        if bindings.cplusplus {
            builder = builder.clang_args(&["-x", "c++", "-std=c++14"]);
        }
    }

    builder = builder.clang_args(flags);

    // Apply the configuration.
    for v in &bindings.types {
        builder = builder.allowlist_type(v);
    }
    for v in &bindings.functions {
        builder = builder.allowlist_function(v);
    }
    for v in &bindings.variables {
        builder = builder.allowlist_var(v);
    }
    for v in &bindings.exclude {
        builder = builder.blocklist_item(v);
    }
    for v in &bindings.opaque {
        builder = builder.opaque_type(v);
    }
    for v in &bindings.enums {
        builder = builder.constified_enum_module(v);
    }

    let bindings = builder.generate().expect("unable to generate bindings");
    bindings
        .write_to_file(out)
        .expect("couldn't write bindings");
}

fn pkg_config() -> Vec<String> {
    let modversion = Command::new("pkg-config")
        .args(["--modversion", "nss"])
        .output()
        .expect("pkg-config reports NSS as absent")
        .stdout;
    let modversion = String::from_utf8(modversion).expect("non-UTF8 from pkg-config");
    let modversion = modversion.trim();
    // The NSS version number does not follow semver numbering, because it omits the patch version
    // when that's 0. Deal with that.
    let modversion_for_cmp = if modversion.chars().filter(|c| *c == '.').count() == 1 {
        modversion.to_owned() + ".0"
    } else {
        modversion.to_owned()
    };
    let modversion_for_cmp =
        Version::parse(&modversion_for_cmp).expect("NSS version not in semver format");
    let version_req = VersionReq::parse(&format!(">={}", MINIMUM_NSS_VERSION.trim())).unwrap();
    assert!(
        version_req.matches(&modversion_for_cmp),
        "neqo has NSS version requirement {version_req}, found {modversion}"
    );

    let cfg = Command::new("pkg-config")
        .args(["--cflags", "--libs", "nss"])
        .output()
        .expect("NSS flags not returned by pkg-config")
        .stdout;
    let cfg_str = String::from_utf8(cfg).expect("non-UTF8 from pkg-config");

    let mut flags: Vec<String> = Vec::new();
    for f in cfg_str.split(' ') {
        if let Some(include) = f.strip_prefix("-I") {
            flags.push(String::from(f));
            println!("cargo:include={include}");
        } else if let Some(path) = f.strip_prefix("-L") {
            println!("cargo:rustc-link-search=native={path}");
        } else if let Some(lib) = f.strip_prefix("-l") {
            println!("cargo:rustc-link-lib=dylib={lib}");
        } else {
            println!("Warning: Unknown flag from pkg-config: {f}");
        }
    }

    flags
}

fn setup_standalone(nss: &str) -> Vec<String> {
    setup_clang();

    println!("cargo:rerun-if-env-changed=NSS_DIR");
    let nss = PathBuf::from(nss);
    assert!(
        !nss.is_relative(),
        "The NSS_DIR environment variable is expected to be an absolute path."
    );

    // $NSS_DIR/../dist/
    let nssdist = nss.parent().unwrap().join("dist");
    println!("cargo:rerun-if-env-changed=NSS_TARGET");
    let nsstarget = env::var("NSS_TARGET")
        .unwrap_or_else(|_| fs::read_to_string(nssdist.join("latest")).unwrap());

    // If NSS_PREBUILT is set, we assume that the NSS libraries are already built.
    if env::var("NSS_PREBUILT").is_err() {
        build_nss(nss, &nsstarget);
    }

    let nsstarget = nssdist.join(nsstarget.trim());
    let includes = get_includes(&nsstarget, &nssdist);

    let nsslibdir = nsstarget.join("lib");
    println!(
        "cargo:rustc-link-search=native={}",
        nsslibdir.to_str().unwrap()
    );
    if env::var("CARGO_CFG_FUZZING").is_ok()
        || env::var("PROFILE").unwrap_or_default() == "debug"
        // FIXME: NSPR doesn't build proper dynamic libraries on Windows.
        || env::consts::OS == "windows"
    {
        static_link();
    } else {
        dynamic_link();
    }

    let mut flags: Vec<String> = Vec::new();
    for i in includes {
        flags.push(String::from("-I") + i.to_str().unwrap());
    }

    flags
}

#[cfg(feature = "gecko")]
fn setup_for_gecko() -> Vec<String> {
    use mozbuild::{
        config::{BINDGEN_SYSTEM_FLAGS, NSPR_CFLAGS, NSS_CFLAGS},
        TOPOBJDIR,
    };

    let fold_libs = mozbuild::config::MOZ_FOLD_LIBS;
    let libs = if fold_libs {
        vec!["nss3"]
    } else {
        vec!["nssutil3", "nss3", "ssl3", "plds4", "plc4", "nspr4"]
    };

    for lib in &libs {
        println!("cargo:rustc-link-lib=dylib={}", lib);
    }

    if fold_libs {
        println!(
            "cargo:rustc-link-search=native={}",
            TOPOBJDIR.join("security").to_str().unwrap()
        );
    } else {
        println!(
            "cargo:rustc-link-search=native={}",
            TOPOBJDIR.join("dist").join("bin").to_str().unwrap()
        );
        let nsslib_path = TOPOBJDIR.join("security").join("nss").join("lib");
        println!(
            "cargo:rustc-link-search=native={}",
            nsslib_path.join("nss").join("nss_nss3").to_str().unwrap()
        );
        println!(
            "cargo:rustc-link-search=native={}",
            nsslib_path.join("ssl").join("ssl_ssl3").to_str().unwrap()
        );
        println!(
            "cargo:rustc-link-search=native={}",
            TOPOBJDIR
                .join("config")
                .join("external")
                .join("nspr")
                .join("pr")
                .to_str()
                .unwrap()
        );
    }

    let mut flags = BINDGEN_SYSTEM_FLAGS
        .iter()
        .chain(&NSPR_CFLAGS)
        .chain(&NSS_CFLAGS)
        .map(|s| s.to_string())
        .collect::<Vec<_>>();

    flags.push(String::from("-include"));
    flags.push(
        TOPOBJDIR
            .join("dist")
            .join("include")
            .join("mozilla-config.h")
            .to_str()
            .unwrap()
            .to_string(),
    );
    flags
}

#[cfg(not(feature = "gecko"))]
fn setup_for_gecko() -> Vec<String> {
    unreachable!()
}

fn main() {
    println!("cargo:rustc-check-cfg=cfg(nss_nodb)");
    let flags = if cfg!(feature = "gecko") {
        setup_for_gecko()
    } else if let Ok(nss_dir) = env::var("NSS_DIR") {
        setup_standalone(nss_dir.trim())
    } else {
        pkg_config()
    };

    let config_file = PathBuf::from(BINDINGS_DIR).join(BINDINGS_CONFIG);
    println!("cargo:rerun-if-changed={}", config_file.to_str().unwrap());
    let config = fs::read_to_string(config_file).expect("unable to read binding configuration");
    let config: HashMap<String, Bindings> = ::toml::from_str(&config).unwrap();

    for (k, v) in &config {
        build_bindings(k, v, &flags[..], cfg!(feature = "gecko"));
    }
}
