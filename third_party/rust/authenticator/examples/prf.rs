/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use authenticator::{
    authenticatorservice::{AuthenticatorService, RegisterArgs, SignArgs},
    crypto::COSEAlgorithm,
    ctap2::server::{
        AuthenticationExtensionsClientInputs, AuthenticationExtensionsPRFInputs,
        AuthenticationExtensionsPRFValues, HMACGetSecretInput, PublicKeyCredentialDescriptor,
        PublicKeyCredentialParameters, PublicKeyCredentialUserEntity, RelyingParty,
        ResidentKeyRequirement, Transport, UserVerificationRequirement,
    },
    statecallback::StateCallback,
    Pin, StatusPinUv, StatusUpdate,
};
use getopts::Options;
use rand::{thread_rng, RngCore};
use std::sync::mpsc::{channel, RecvError};
use std::{env, thread};

fn print_usage(program: &str, opts: Options) {
    let brief = format!("Usage: {program} [options]");
    print!("{}", opts.usage(&brief));
}

fn main() {
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    let program = args[0].clone();

    let rp_id = "example.com".to_string();

    let mut opts = Options::new();
    opts.optflag("h", "help", "print this help menu").optopt(
        "t",
        "timeout",
        "timeout in seconds",
        "SEC",
    );
    opts.optflag("h", "help", "print this help menu");
    opts.optflag(
        "",
        "hmac-secret",
        "Return hmac-secret outputs instead of prf outputs (i.e., do not prefix and hash the inputs)",
    );
    let matches = match opts.parse(&args[1..]) {
        Ok(m) => m,
        Err(f) => panic!("{}", f.to_string()),
    };
    if matches.opt_present("help") {
        print_usage(&program, opts);
        return;
    }

    let mut manager =
        AuthenticatorService::new().expect("The auth service should initialize safely");
    manager.add_u2f_usb_hid_platform_transports();

    let timeout_ms = match matches.opt_get_default::<u64>("timeout", 25) {
        Ok(timeout_s) => {
            println!("Using {}s as the timeout", &timeout_s);
            timeout_s * 1_000
        }
        Err(e) => {
            println!("{e}");
            print_usage(&program, opts);
            return;
        }
    };

    let (register_hmac_secret, sign_hmac_secret, register_prf, sign_prf) =
        if matches.opt_present("hmac-secret") {
            let register_hmac_secret = Some(true);
            let sign_hmac_secret = Some(HMACGetSecretInput {
                salt1: [0x07; 32],
                salt2: Some([0x07; 32]),
            });
            (register_hmac_secret, sign_hmac_secret, None, None)
        } else {
            let register_prf = Some(AuthenticationExtensionsPRFInputs::default());
            let sign_prf = Some(AuthenticationExtensionsPRFInputs {
                eval: Some(AuthenticationExtensionsPRFValues {
                    first: vec![1, 2, 3, 4],
                    second: Some(vec![1, 2, 3, 4]),
                }),
                eval_by_credential: None,
            });
            (None, None, register_prf, sign_prf)
        };

    println!("Asking a security key to register now...");
    let mut chall_bytes = [0u8; 32];
    thread_rng().fill_bytes(&mut chall_bytes);

    let (status_tx, status_rx) = channel::<StatusUpdate>();
    thread::spawn(move || loop {
        match status_rx.recv() {
            Ok(StatusUpdate::InteractiveManagement(..)) => {
                panic!("STATUS: This can't happen when doing non-interactive usage");
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                println!("STATUS: Please select a device by touching one of them.");
            }
            Ok(StatusUpdate::PresenceRequired) => {
                println!("STATUS: waiting for user presence");
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinRequired(sender))) => {
                let raw_pin =
                    rpassword::prompt_password_stderr("Enter PIN: ").expect("Failed to read PIN");
                sender.send(Pin::new(&raw_pin)).expect("Failed to send PIN");
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::InvalidPin(sender, attempts))) => {
                println!(
                    "Wrong PIN! {}",
                    attempts.map_or("Try again.".to_string(), |a| format!(
                        "You have {a} attempts left."
                    ))
                );
                let raw_pin =
                    rpassword::prompt_password_stderr("Enter PIN: ").expect("Failed to read PIN");
                sender.send(Pin::new(&raw_pin)).expect("Failed to send PIN");
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinAuthBlocked)) => {
                panic!("Too many failed attempts in one row. Your device has been temporarily blocked. Please unplug it and plug in again.")
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinBlocked)) => {
                panic!("Too many failed attempts. Your device has been blocked. Reset it.")
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::InvalidUv(attempts))) => {
                println!(
                    "Wrong UV! {}",
                    attempts.map_or("Try again.".to_string(), |a| format!(
                        "You have {a} attempts left."
                    ))
                );
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::UvBlocked)) => {
                println!("Too many failed UV-attempts.");
                continue;
            }
            Ok(StatusUpdate::PinUvError(e)) => {
                panic!("Unexpected error: {:?}", e)
            }
            Ok(StatusUpdate::SelectResultNotice(_, _)) => {
                panic!("Unexpected select device notice")
            }
            Err(RecvError) => {
                println!("STATUS: end");
                return;
            }
        }
    });

    let user = PublicKeyCredentialUserEntity {
        id: "user_id".as_bytes().to_vec(),
        name: Some("A. User".to_string()),
        display_name: None,
    };
    let relying_party = RelyingParty {
        id: rp_id.clone(),
        name: None,
    };
    let ctap_args = RegisterArgs {
        client_data_hash: chall_bytes,
        relying_party,
        origin: format!("https://{rp_id}"),
        user,
        pub_cred_params: vec![
            PublicKeyCredentialParameters {
                alg: COSEAlgorithm::ES256,
            },
            PublicKeyCredentialParameters {
                alg: COSEAlgorithm::RS256,
            },
        ],
        exclude_list: vec![],
        user_verification_req: UserVerificationRequirement::Required,
        resident_key_req: ResidentKeyRequirement::Discouraged,
        extensions: AuthenticationExtensionsClientInputs {
            hmac_create_secret: register_hmac_secret,
            prf: register_prf,
            ..Default::default()
        },
        pin: None,
        use_ctap1_fallback: false,
    };

    let attestation_object;
    let (register_tx, register_rx) = channel();
    let callback = StateCallback::new(Box::new(move |rv| {
        register_tx.send(rv).unwrap();
    }));

    if let Err(e) = manager.register(timeout_ms, ctap_args, status_tx.clone(), callback) {
        panic!("Couldn't register: {:?}", e);
    };

    let register_result = register_rx
        .recv()
        .expect("Problem receiving, unable to continue");
    match register_result {
        Ok(a) => {
            println!("Ok!");
            attestation_object = a;
        }
        Err(e) => panic!("Registration failed: {:?}", e),
    };

    println!("Register result: {:?}", &attestation_object);

    println!();
    println!("*********************************************************************");
    println!("Asking a security key to sign now, with the data from the register...");
    println!("*********************************************************************");

    let allow_list;
    if let Some(cred_data) = attestation_object.att_obj.auth_data.credential_data {
        allow_list = vec![PublicKeyCredentialDescriptor {
            id: cred_data.credential_id,
            transports: vec![Transport::USB],
        }];
    } else {
        allow_list = Vec::new();
    }

    let ctap_args = SignArgs {
        client_data_hash: chall_bytes,
        origin: format!("https://{rp_id}"),
        relying_party_id: rp_id,
        allow_list,
        user_verification_req: UserVerificationRequirement::Required,
        user_presence_req: true,
        extensions: AuthenticationExtensionsClientInputs {
            hmac_get_secret: sign_hmac_secret.clone(),
            prf: sign_prf.clone(),
            ..Default::default()
        },
        pin: None,
        use_ctap1_fallback: false,
    };

    let (sign_tx, sign_rx) = channel();
    let callback = StateCallback::new(Box::new(move |rv| {
        sign_tx.send(rv).unwrap();
    }));

    if let Err(e) = manager.sign(timeout_ms, ctap_args, status_tx, callback) {
        panic!("Couldn't sign: {:?}", e);
    }

    let sign_result = sign_rx
        .recv()
        .expect("Problem receiving, unable to continue");

    match sign_result {
        Ok(assertion_object) => {
            println!("Assertion Object: {assertion_object:?}");
            println!("Done.");

            if sign_hmac_secret.is_some() {
                let hmac_secret_outputs = assertion_object
                    .extensions
                    .hmac_get_secret
                    .as_ref()
                    .expect("Expected hmac-secret output");

                assert_eq!(
                    Some(hmac_secret_outputs.output1),
                    hmac_secret_outputs.output2,
                    "Expected hmac-secret outputs to be equal for equal input"
                );

                assert_eq!(
                    assertion_object.extensions.prf, None,
                    "Expected no PRF outputs when hmacGetSecret input was present"
                );
            }

            if sign_prf.is_some() {
                let prf_results = assertion_object
                    .extensions
                    .prf
                    .expect("Expected PRF output")
                    .results
                    .expect("Expected PRF output to contain results");

                assert_eq!(
                    Some(prf_results.first),
                    prf_results.second,
                    "Expected PRF results to be equal for equal input"
                );

                assert_eq!(
                    assertion_object.extensions.hmac_get_secret, None,
                    "Expected no hmacGetSecret output when PRF input was present"
                );
            }
        }

        Err(e) => panic!("Signing failed: {:?}", e),
    }
}
