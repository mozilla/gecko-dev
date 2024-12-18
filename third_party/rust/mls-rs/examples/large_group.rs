// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs::{
    client_builder::MlsConfig,
    error::MlsError,
    identity::{
        basic::{BasicCredential, BasicIdentityProvider},
        SigningIdentity,
    },
    mls_rules::{CommitOptions, DefaultMlsRules},
    CipherSuite, CipherSuiteProvider, Client, CryptoProvider, Group,
};

const CIPHERSUITE: CipherSuite = CipherSuite::CURVE25519_AES128;
const GROUP_SIZES: [usize; 8] = [2, 3, 5, 9, 17, 33, 65, 129];

enum Case {
    Best,
    Worst,
}

fn bench_commit_size<P: CryptoProvider + Clone>(
    case_group: Case,
    crypto_provider: &P,
) -> Result<(Vec<usize>, Vec<usize>), MlsError> {
    let mut small_bench = vec![];
    let mut large_bench = vec![];

    for num_groups in GROUP_SIZES.iter().copied() {
        let (small_commit, large_commit) = match case_group {
            Case::Best => {
                let mut groups = make_groups_best_case(num_groups, crypto_provider)?;
                let small_commit = groups[num_groups - 1].commit(vec![])?.commit_message;
                let large_commit = groups[0].commit(vec![])?.commit_message;
                (small_commit, large_commit)
            }
            Case::Worst => {
                let mut groups = make_groups_worst_case(num_groups, crypto_provider)?;
                let small_commit = groups[num_groups - 1].commit(vec![])?.commit_message;
                let large_commit = groups[0].commit(vec![])?.commit_message;
                (small_commit, large_commit)
            }
        };

        small_bench.push(small_commit.to_bytes()?.len());
        large_bench.push(large_commit.to_bytes()?.len());
    }

    Ok((small_bench, large_bench))
}

// Bob[0] crates a group. Repeat for `i=0` to `num_groups - 1` times : Bob[i] adds Bob[i+1]
fn make_groups_best_case<P: CryptoProvider + Clone>(
    num_groups: usize,
    crypto_provider: &P,
) -> Result<Vec<Group<impl MlsConfig>>, MlsError> {
    let bob_client = make_client(crypto_provider.clone(), &make_name(0))?;

    let bob_group = bob_client.create_group(Default::default())?;

    let mut groups = vec![bob_group];

    for i in 0..(num_groups - 1) {
        let bob_client = make_client(crypto_provider.clone(), &make_name(i + 1))?;

        // The new client generates a key package.
        let bob_kpkg = bob_client.generate_key_package_message()?;

        // Last group sends a commit adding the new client to the group.
        let commit = groups
            .last_mut()
            .unwrap()
            .commit_builder()
            .add_member(bob_kpkg)?
            .build()?;

        // All other groups process the commit.
        for group in groups.iter_mut().rev().skip(1) {
            group.process_incoming_message(commit.commit_message.clone())?;
        }

        // The last group applies the generated commit.
        groups.last_mut().unwrap().apply_pending_commit()?;

        // The new member joins.
        let (bob_group, _info) = bob_client.join_group(None, &commit.welcome_messages[0])?;

        groups.push(bob_group);
    }

    Ok(groups)
}

// Alice creates a group by adding `num_groups - 1` clients in one commit.
fn make_groups_worst_case<P: CryptoProvider + Clone>(
    num_groups: usize,
    crypto_provider: &P,
) -> Result<Vec<Group<impl MlsConfig>>, MlsError> {
    let alice_client = make_client(crypto_provider.clone(), &make_name(0))?;

    let mut alice_group = alice_client.create_group(Default::default())?;

    let bob_clients = (0..(num_groups - 1))
        .map(|i| make_client(crypto_provider.clone(), &make_name(i + 1)))
        .collect::<Result<Vec<_>, _>>()?;

    // Alice adds all Bob's clients in a single commit.
    let mut commit_builder = alice_group.commit_builder();

    for bob_client in &bob_clients {
        let bob_kpkg = bob_client.generate_key_package_message()?;
        commit_builder = commit_builder.add_member(bob_kpkg)?;
    }

    let welcome_message = &commit_builder.build()?.welcome_messages[0];

    alice_group.apply_pending_commit()?;

    // Bob's clients join the group.
    let mut groups = vec![alice_group];

    for bob_client in &bob_clients {
        let (bob_group, _info) = bob_client.join_group(None, welcome_message)?;
        groups.push(bob_group);
    }

    Ok(groups)
}

fn make_client<P: CryptoProvider + Clone>(
    crypto_provider: P,
    name: &str,
) -> Result<Client<impl MlsConfig>, MlsError> {
    let cipher_suite = crypto_provider.cipher_suite_provider(CIPHERSUITE).unwrap();

    // Generate a signature key pair.
    let (secret, public) = cipher_suite.signature_key_generate().unwrap();

    // Create a basic credential for the session.
    // NOTE: BasicCredential is for demonstration purposes and not recommended for production.
    // X.509 credentials are recommended.
    let basic_identity = BasicCredential::new(name.as_bytes().to_vec());
    let signing_identity = SigningIdentity::new(basic_identity.into_credential(), public);

    Ok(Client::builder()
        .identity_provider(BasicIdentityProvider)
        .crypto_provider(crypto_provider)
        .mls_rules(
            DefaultMlsRules::new()
                .with_commit_options(CommitOptions::new().with_path_required(true)),
        )
        .signing_identity(signing_identity, secret, CIPHERSUITE)
        .build())
}

fn make_name(i: usize) -> String {
    format!("bob {i:08}")
}

fn main() -> Result<(), MlsError> {
    let crypto_provider = mls_rs_crypto_openssl::OpensslCryptoProvider::default();

    println!("Demonstrate that performance depends on a) group evolution and b) a members position in the tree.\n");

    let (small_bench_bc, large_bench_bc) = bench_commit_size(Case::Best, &crypto_provider)?;
    let (small_bench_wc, large_bench_wc) = bench_commit_size(Case::Worst, &crypto_provider)?;

    println!("\nBest case a), worst case b) : commit size is θ(log(n)) bytes.");
    println!("group sizes n :\n{GROUP_SIZES:?}\ncommit sizes :\n{large_bench_bc:?}");

    println!("\nWorst case a), worst case b) : commit size is θ(n) bytes.");
    println!("group sizes n :\n{GROUP_SIZES:?}\ncommit sizes :\n{large_bench_wc:?}");

    println!(
        "\nBest case b) : if n-1 is a power of 2, commit size is θ(1) bytes, independent of a)."
    );
    println!("group sizes n :\n{GROUP_SIZES:?}\ncommit sizes, best case a) :\n{small_bench_bc:?}");
    println!("commit sizes, worst case a) :\n{small_bench_wc:?}");

    Ok(())
}
