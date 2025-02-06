/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines functions for testing if an environment from the
//! configuration matches the user environment.

use crate::{JSONVariantEnvironment, SearchUserEnvironment};
use firefox_versioning::version::Version;

/// Matches the user's environment against the given environment from the
/// configuration.
///
/// This function expects the locale, region and app version in the environment
/// to be lower case.
pub(crate) fn matches_user_environment(
    environment: &JSONVariantEnvironment,
    user_environment: &SearchUserEnvironment,
) -> bool {
    if !environment.experiment.is_empty() && user_environment.experiment != environment.experiment {
        return false;
    }

    if !environment.excluded_distributions.is_empty()
        && environment
            .excluded_distributions
            .contains(&user_environment.distribution_id)
    {
        return false;
    }

    matches_region_and_locale(
        &user_environment.region,
        &user_environment.locale,
        environment,
    ) && is_empty_or_contains(
        &environment.distributions,
        &user_environment.distribution_id,
    ) && matches_version(
        &user_environment.version,
        &environment.min_version,
        &environment.max_version,
    ) && is_empty_or_contains(&environment.channels, &user_environment.update_channel)
        && is_empty_or_contains(&environment.applications, &user_environment.app_name)
        && is_empty_or_contains(&environment.device_type, &user_environment.device_type)
}

/// Determines whether the region and locale constraints in the supplied
/// environment apply to a user given the region and locale they are using.
fn matches_region_and_locale(
    user_region: &str,
    user_locale: &str,
    environment: &JSONVariantEnvironment,
) -> bool {
    if does_array_include(&environment.excluded_regions, user_region)
        || does_array_include(&environment.excluded_locales, user_locale)
    {
        return false;
    }

    // This is a special case, if all_regions_and_locales is false (default value)
    // and region and locales are not set, then we assume that for the purposes of
    // matching region & locale, we do match everywhere. This allows us to specify
    // none of these options but match against other items such as distribution or
    // application name.
    if !environment.all_regions_and_locales
        && environment.regions.is_empty()
        && environment.locales.is_empty()
    {
        return true;
    }

    if does_array_include(&environment.regions, user_region)
        && does_array_include(&environment.locales, user_locale)
    {
        return true;
    }

    if environment.regions.is_empty() && does_array_include(&environment.locales, user_locale) {
        return true;
    }

    if environment.locales.is_empty() && does_array_include(&environment.regions, user_region) {
        return true;
    }

    if environment.all_regions_and_locales {
        return true;
    }

    false
}

fn matches_version(
    user_version: &str,
    environment_min_version: &str,
    environment_max_version: &str,
) -> bool {
    use std::ops::{Bound, RangeBounds};

    let (min_version, max_version) = match (
        environment_min_version.is_empty(),
        environment_max_version.is_empty(),
    ) {
        (true, true) => return true,
        (true, false) => (
            Bound::Unbounded,
            Version::try_from(environment_max_version).map_or(Bound::Unbounded, Bound::Included),
        ),
        (false, true) => (
            Version::try_from(environment_min_version).map_or(Bound::Unbounded, Bound::Included),
            Bound::Unbounded,
        ),
        (false, false) => (
            Version::try_from(environment_min_version).map_or(Bound::Unbounded, Bound::Included),
            Version::try_from(environment_max_version).map_or(Bound::Unbounded, Bound::Included),
        ),
    };

    !user_version.is_empty()
        && Version::try_from(user_version).map_or(true, |user_version| {
            (min_version, max_version).contains(&user_version)
        })
}

fn is_empty_or_contains<T: std::cmp::PartialEq>(env_value: &[T], user_value: &T) -> bool {
    env_value.is_empty() || env_value.contains(user_value)
}

fn does_array_include(config_array: &[String], compare_item: &str) -> bool {
    !config_array.is_empty()
        && config_array
            .iter()
            .any(|x| x.to_lowercase() == compare_item)
}

#[cfg(test)]
mod tests {
    use std::vec;

    use super::*;
    use crate::*;

    #[test]
    fn test_matches_user_environment_all_locales() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    excluded_locales: vec![],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "FR".into(),
                    ..Default::default()
                }
            ),
            "Should return true when all_regions_and_locales is true"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when all_regions_and_locales is false (default) and no regions/locales are specified"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    excluded_locales: vec!["fi".to_string()],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return false when all_regions_and_locales is true and the locale is excluded"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    excluded_locales: vec!["FI".to_string()],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return false when all_regions_and_locales is true and the excluded locale is a different case"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    excluded_locales: vec!["en-US".to_string()],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when all_regions_and_locales is true and the locale is not excluded"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    excluded_regions: vec!["us".to_string(), "fr".to_string()],
                    locales: vec![],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return false when all_regions_and_locales is true and the region is excluded"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    excluded_locales: vec![],
                    excluded_regions: vec!["US".to_string(), "FR".to_string()],
                    locales: vec![],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return false when all_regions_and_locales is true and the excluded region is a different case"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    excluded_locales: vec![],
                    excluded_regions: vec!["us".to_string()],
                    locales: vec![],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when all_regions_and_locales is true and the region is not excluded"
        );
    }

    #[test]
    fn test_matches_user_environment_locales() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec![],
                    locales: vec!["en-gb".to_string(), "fi".to_string()],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the user locale matches one from the config"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec![],
                    locales: vec!["en-GB".to_string(), "FI".to_string()],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the user locale matches one from the config and is a different case"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec![],
                    locales: vec!["en-gb".to_string(), "en-ca".to_string()],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return false when the user locale does not match one from the config"
        );
    }

    #[test]
    fn test_matches_user_environment_regions() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec!["gb".to_string(), "fr".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the user region matches one from the config"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec!["GB".to_string(), "FR".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the user region matches one from the config and is a different case"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec![],
                    locales: vec!["gb".to_string(), "ca".to_string()],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return false when the user region does not match one from the config"
        );
    }

    #[test]
    fn test_matches_user_environment_locales_with_excluded_regions() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec!["gb".to_string(), "ca".to_string()],
                    locales: vec!["en-gb".to_string(), "fi".to_string()],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the locale matches and the region is not excluded"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec![],
                    excluded_regions: vec!["gb".to_string(), "fr".to_string()],
                    locales: vec!["en-gb".to_string(), "fi".to_string()],
                    regions: vec![],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return false when the locale matches and the region is excluded"
        );
    }

    #[test]
    fn test_matches_user_environment_regions_with_excluded_locales() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec!["en-gb".to_string(), "de".to_string()],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec!["gb".to_string(), "fr".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the region matches and the locale is not excluded"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    all_regions_and_locales: false,
                    excluded_locales: vec!["en-gb".to_string(), "fi".to_string()],
                    excluded_regions: vec![],
                    locales: vec![],
                    regions: vec!["gb".to_string(), "fr".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    ..Default::default()
                }
            ),
            "Should return false when the region matches and the locale is excluded"
        );
    }

    #[test]
    fn test_matches_user_environment_distributions() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    distributions: vec!["distro-1".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    distribution_id: "distro-1".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the distribution matches one in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    distributions: vec!["distro-2".to_string(), "distro-3".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    distribution_id: "distro-3".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the distribution matches one in the environment when there are multiple"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    distributions: vec!["distro-2".to_string(), "distro-3".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    distribution_id: "distro-4".into(),
                    ..Default::default()
                }
            ),
            "Should return false when the distribution does not match any in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    regions: vec!["fr".to_string()],
                    distributions: vec!["distro-1".to_string(), "distro-2".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    distribution_id: "distro-2".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the distribution and region matches the environment"
        );
    }

    #[test]
    fn test_matches_user_environment_excluded_distributions() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    distributions: vec!["distro-1".to_string(), "distro-2".to_string()],
                    excluded_distributions: vec!["
                        distro-3".to_string(), "distro-4".to_string()
                    ],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    distribution_id: "distro-2".into(),
                    ..Default::default()
                }
            ),
            "Should return true when the distribution matches the distribution list but not the excluded distributions"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    distributions: vec!["distro-1".to_string(), "distro-2".to_string()],
                    excluded_distributions: vec!["distro-3".to_string(), "distro-4".to_string()],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    distribution_id: "distro-3".into(),
                    ..Default::default()
                }
            ),
            "Should return false when the distribution matches the the excluded distributions"
        );
    }

    #[test]
    fn test_matches_user_environment_application_name() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    applications: vec![SearchApplicationName::Firefox],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    app_name: SearchApplicationName::Firefox,
                    ..Default::default()
                }
            ),
            "Should return true when the application name matches the one in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    applications: vec![
                        SearchApplicationName::FirefoxAndroid,
                        SearchApplicationName::Firefox
                    ],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    app_name: SearchApplicationName::Firefox,
                    ..Default::default()
                }
            ),
            "Should return true when the application name matches one in the environment when there are multiple"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    applications: vec![
                        SearchApplicationName::FirefoxAndroid,
                        SearchApplicationName::Firefox
                    ],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    app_name: SearchApplicationName::FirefoxIos,
                    ..Default::default()
                }
            ),
            "Should return false when the applications do not match the one in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    regions: vec!["fr".to_string()],
                    applications: vec![
                        SearchApplicationName::FirefoxAndroid,
                        SearchApplicationName::Firefox
                    ],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    app_name: SearchApplicationName::Firefox,
                    ..Default::default()
                }
            ),
            "Should return true when the application name matches the one in the environment"
        );
    }

    #[test]
    fn test_matches_user_environment_channel() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    channels: vec![SearchUpdateChannel::Nightly],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    update_channel: SearchUpdateChannel::Nightly,
                    ..Default::default()
                }
            ),
            "Should return true when the channel matches one in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    channels: vec![SearchUpdateChannel::Nightly, SearchUpdateChannel::Release],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    update_channel: SearchUpdateChannel::Release,
                    ..Default::default()
                }
            ),
            "Should return true when the channel matches one in the environment when there are multiple"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    channels: vec![SearchUpdateChannel::Nightly],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    update_channel: SearchUpdateChannel::Default,
                    distribution_id: "distro-4".into(),
                    experiment: String::new(),
                    app_name: SearchApplicationName::Firefox,
                    version: String::new(),
                    device_type: SearchDeviceType::None,
                }
            ),
            "Should return false when the channel does not match any in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    regions: vec!["fr".to_string()],
                    channels: vec![SearchUpdateChannel::Default],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    update_channel: SearchUpdateChannel::Default,
                    ..Default::default()
                }
            ),
            "Should return true when the channel and region matches the environment"
        );
    }

    #[test]
    fn test_matches_user_environment_experiment() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    experiment: "warp-drive".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    experiment: "warp-drive".to_string(),
                    ..Default::default()
                }
            ),
            "Should return true when the experiment matches the one in the environment"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    experiment: "warp-drive".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    experiment: "cloak".to_string(),
                    ..Default::default()
                }
            ),
            "Should return false when the experiment does not match the one in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    regions: vec!["fr".to_string()],
                    experiment: "warp-drive".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    experiment: "warp-drive".to_string(),
                    ..Default::default()
                }
            ),
            "Should return true when the experiment and region matches the environment"
        );
    }

    #[test]
    fn test_matches_user_environment_versions() {
        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "43.0.0".to_string(),
                    max_version: "".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "42.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return false when the version is below the minimum"
        );
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "42.0.0".to_string(),
                    max_version: "".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "42.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return true when the version is equal to the minimum"
        );
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "41.0.0".to_string(),
                    max_version: "".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "42.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return true when the version is above the minimum"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "".to_string(),
                    max_version: "43.0.0".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "42.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return true when the version is below the maximum"
        );
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "".to_string(),
                    max_version: "42.0.0".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "42.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return true when the version is equal to the maximum"
        );
        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "".to_string(),
                    max_version: "41.0.0".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "42.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return false when the version is above the maximum"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "41.0.0".to_string(),
                    max_version: "43.0.0".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "2.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return false when the version is below the minimum and both are specified"
        );
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "41.0.0".to_string(),
                    max_version: "43.0.0".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "41.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return true when the version is equal to the minimum and both are specified"
        );
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "41.0.0".to_string(),
                    max_version: "43.0.0".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "42.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return true when the version is between the minimum and maximum"
        );
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "41.0.0".to_string(),
                    max_version: "43.0.0".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "43.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return true when the version is equal to the maximum and both are specified"
        );
        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    min_version: "41.0.0".to_string(),
                    max_version: "43.0.0".to_string(),
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    version: "44.0.0".to_string(),
                    ..Default::default()
                },
            ),
            "Should return false when the version is above the maximum and both are specified"
        );
    }

    #[test]
    fn test_matches_user_environment_device_type() {
        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    device_type: vec![SearchDeviceType::None],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    device_type: SearchDeviceType::None,
                    ..Default::default()
                }
            ),
            "Should return true when the device type matches one in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    device_type: vec![SearchDeviceType::Smartphone, SearchDeviceType::Tablet],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    device_type: SearchDeviceType::Tablet,
                    ..Default::default()
                }
            ),
            "Should return true when the device type matches one in the environment when there are multiple"
        );

        assert!(
            !matches_user_environment(
                &crate::JSONVariantEnvironment {
                    device_type: vec![SearchDeviceType::Smartphone],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    device_type: SearchDeviceType::None,
                    ..Default::default()
                }
            ),
            "Should return false when the device type does not match any in the environment"
        );

        assert!(
            matches_user_environment(
                &crate::JSONVariantEnvironment {
                    regions: vec!["fr".to_string()],
                    device_type: vec![SearchDeviceType::Tablet],
                    ..Default::default()
                },
                &SearchUserEnvironment {
                    locale: "fi".into(),
                    region: "fr".into(),
                    device_type: SearchDeviceType::Tablet,
                    ..Default::default()
                }
            ),
            "Should return true when the device type and region matches the environment"
        );
    }
}
