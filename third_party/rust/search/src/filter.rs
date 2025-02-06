/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines the functions for managing the filtering of the configuration.

use crate::environment_matching::matches_user_environment;
use crate::{
    error::Error, JSONDefaultEnginesRecord, JSONEngineBase, JSONEngineRecord, JSONEngineUrl,
    JSONEngineUrls, JSONEngineVariant, JSONSearchConfigurationRecords, RefinedSearchConfig,
    SearchEngineDefinition, SearchEngineUrl, SearchEngineUrls, SearchUserEnvironment,
};

impl From<JSONEngineUrl> for SearchEngineUrl {
    fn from(url: JSONEngineUrl) -> Self {
        Self {
            base: url.base.unwrap_or_default(),
            method: url.method.unwrap_or_default().as_str().to_string(),
            params: url.params.unwrap_or_default(),
            search_term_param_name: url.search_term_param_name,
        }
    }
}

impl JSONEngineUrl {
    /// Merges two `JSONEngineUrl` objects, preferring the values from the
    /// `preferred` object.
    fn merge(original: Self, preferred: Self) -> Self {
        Self {
            base: preferred.base.or(original.base),
            method: preferred.method.or(original.method),
            params: preferred.params.or(original.params),
            search_term_param_name: preferred
                .search_term_param_name
                .or(original.search_term_param_name),
        }
    }
}

impl From<JSONEngineUrls> for SearchEngineUrls {
    fn from(urls: JSONEngineUrls) -> Self {
        Self {
            search: urls.search.into(),
            suggestions: urls.suggestions.map(|suggestions| suggestions.into()),
            trending: urls.trending.map(|trending| trending.into()),
        }
    }
}

impl JSONEngineUrls {
    fn maybe_merge_urls(
        original_url: Option<JSONEngineUrl>,
        preferred_url: Option<JSONEngineUrl>,
    ) -> Option<JSONEngineUrl> {
        match (&original_url, &preferred_url) {
            (Some(original), Some(preferred)) => {
                Some(JSONEngineUrl::merge(original.clone(), preferred.clone()))
            }
            (None, Some(preferred)) => Some(preferred.clone()),
            _ => original_url.clone(),
        }
    }

    /// Merges two `JSONEngineUrl` objects, preferring the values from the
    /// `preferred` object.
    fn merge(original: Self, preferred: JSONEngineUrls) -> Self {
        Self {
            search: JSONEngineUrl::merge(original.search, preferred.search),
            suggestions: JSONEngineUrls::maybe_merge_urls(
                original.suggestions,
                preferred.suggestions,
            ),
            trending: JSONEngineUrls::maybe_merge_urls(original.trending, preferred.trending),
        }
    }
}

impl SearchEngineDefinition {
    pub(crate) fn from_configuration_details(
        identifier: &str,
        base: JSONEngineBase,
        variant: JSONEngineVariant,
    ) -> SearchEngineDefinition {
        let urls: JSONEngineUrls = match variant.urls {
            Some(urls) => JSONEngineUrls::merge(base.urls, urls),
            None => base.urls,
        };

        SearchEngineDefinition {
            aliases: base.aliases.unwrap_or_default(),
            charset: base.charset.unwrap_or_else(|| "UTF-8".to_string()),
            classification: base.classification,
            identifier: identifier.to_string(),
            name: base.name,
            optional: variant.optional,
            order_hint: None,
            partner_code: variant
                .partner_code
                .or(base.partner_code)
                .unwrap_or_default(),
            telemetry_suffix: variant.telemetry_suffix,
            urls: urls.into(),
        }
    }
}

pub(crate) fn filter_engine_configuration(
    user_environment: SearchUserEnvironment,
    configuration: Vec<JSONSearchConfigurationRecords>,
) -> Result<RefinedSearchConfig, Error> {
    let mut engines = Vec::new();

    let mut user_environment = user_environment.clone();
    user_environment.locale = user_environment.locale.to_lowercase();
    user_environment.region = user_environment.region.to_lowercase();
    user_environment.version = user_environment.version.to_lowercase();

    let mut default_engines_record = None;

    for record in configuration {
        match record {
            JSONSearchConfigurationRecords::Engine(engine) => {
                let result = maybe_extract_engine_config(&user_environment, engine);
                engines.extend(result);
            }
            JSONSearchConfigurationRecords::DefaultEngines(default_engines) => {
                default_engines_record = Some(default_engines);
            }
            JSONSearchConfigurationRecords::EngineOrders(_engine_orders) => {
                // TODO: Implementation.
            }
            JSONSearchConfigurationRecords::Unknown => {
                // Prevents panics if a new record type is added in future.
            }
        }
    }

    let (default_engine_id, default_private_engine_id) =
        determine_default_engines(&engines, default_engines_record, &user_environment);

    Ok(RefinedSearchConfig {
        engines,
        app_default_engine_id: default_engine_id,
        app_private_default_engine_id: default_private_engine_id,
    })
}

fn maybe_extract_engine_config(
    user_environment: &SearchUserEnvironment,
    record: Box<JSONEngineRecord>,
) -> Option<SearchEngineDefinition> {
    let JSONEngineRecord {
        identifier,
        variants,
        base,
    } = *record;
    let matching_variant = variants
        .into_iter()
        .rev()
        .find(|r| matches_user_environment(&r.environment, user_environment));

    matching_variant.map(|variant| {
        SearchEngineDefinition::from_configuration_details(&identifier, base, variant)
    })
}

fn determine_default_engines(
    engines: &[SearchEngineDefinition],
    default_engines_record: Option<JSONDefaultEnginesRecord>,
    user_environment: &SearchUserEnvironment,
) -> (Option<String>, Option<String>) {
    match default_engines_record {
        None => (None, None),
        Some(record) => {
            let mut default_engine_id = None;
            let mut default_engine_private_id = None;

            let specific_default = record
                .specific_defaults
                .into_iter()
                .rev()
                .find(|r| matches_user_environment(&r.environment, user_environment));

            if let Some(specific_default) = specific_default {
                // Check the engine is present in the list of engines before
                // we return it as default.
                if let Some(engine_id) = find_engine_with_match(engines, specific_default.default) {
                    default_engine_id.replace(engine_id);
                }
                if let Some(private_engine_id) =
                    find_engine_with_match(engines, specific_default.default_private)
                {
                    default_engine_private_id.replace(private_engine_id);
                }
            }

            (
                // If we haven't found a default engine in a specific default,
                // then fall back to the global default engine - but only if that
                // exists in the engine list.
                //
                // For the normal mode engine (`default_engine_id`), this would
                // effectively be considered an error. However, we can't do anything
                // sensible here, so we will return `None` to the application, and
                // that can handle it.
                default_engine_id.or_else(|| find_engine_id(engines, record.global_default)),
                default_engine_private_id
                    .or_else(|| find_engine_id(engines, record.global_default_private)),
            )
        }
    }
}

fn find_engine_id(engines: &[SearchEngineDefinition], engine_id: String) -> Option<String> {
    if engine_id.is_empty() {
        return None;
    }
    match engines.iter().any(|e| e.identifier == engine_id) {
        true => Some(engine_id.clone()),
        false => None,
    }
}

fn find_engine_with_match(
    engines: &[SearchEngineDefinition],
    engine_id_match: String,
) -> Option<String> {
    if engine_id_match.is_empty() {
        return None;
    }
    if let Some(match_no_star) = engine_id_match.strip_suffix('*') {
        return engines
            .iter()
            .find(|e| e.identifier.starts_with(match_no_star))
            .map(|e| e.identifier.clone());
    }

    engines
        .iter()
        .find(|e| e.identifier == engine_id_match)
        .map(|e| e.identifier.clone())
}

#[cfg(test)]
mod tests {
    use std::vec;

    use super::*;
    use crate::*;
    use once_cell::sync::Lazy;
    use pretty_assertions::assert_eq;

    #[test]
    fn test_from_configuration_details_fallsback_to_defaults() {
        // This test doesn't use `..Default::default()` as we want to
        // be explicit about `JSONEngineBase` and handling `None`
        // options/default values.
        let result = SearchEngineDefinition::from_configuration_details(
            "test",
            JSONEngineBase {
                aliases: None,
                charset: None,
                classification: SearchEngineClassification::General,
                name: "Test".to_string(),
                partner_code: None,
                urls: JSONEngineUrls {
                    search: JSONEngineUrl {
                        base: Some("https://example.com".to_string()),
                        method: None,
                        params: None,
                        search_term_param_name: None,
                    },
                    suggestions: None,
                    trending: None,
                },
            },
            JSONEngineVariant {
                environment: JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    ..Default::default()
                },
                optional: false,
                partner_code: None,
                telemetry_suffix: None,
                urls: None,
            },
        );

        assert_eq!(
            result,
            SearchEngineDefinition {
                aliases: Vec::new(),
                charset: "UTF-8".to_string(),
                classification: SearchEngineClassification::General,
                identifier: "test".to_string(),
                partner_code: String::new(),
                name: "Test".to_string(),
                optional: false,
                order_hint: None,
                telemetry_suffix: None,
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com".to_string(),
                        method: "GET".to_string(),
                        params: Vec::new(),
                        search_term_param_name: None,
                    },
                    suggestions: None,
                    trending: None
                }
            }
        )
    }

    static ENGINE_BASE: Lazy<JSONEngineBase> = Lazy::new(|| JSONEngineBase {
        aliases: Some(vec!["foo".to_string(), "bar".to_string()]),
        charset: Some("ISO-8859-15".to_string()),
        classification: SearchEngineClassification::Unknown,
        name: "Test".to_string(),
        partner_code: Some("firefox".to_string()),
        urls: JSONEngineUrls {
            search: JSONEngineUrl {
                base: Some("https://example.com".to_string()),
                method: Some(crate::JSONEngineMethod::Post),
                params: Some(vec![SearchUrlParam {
                    name: "param".to_string(),
                    value: Some("test param".to_string()),
                    experiment_config: None,
                }]),
                search_term_param_name: Some("baz".to_string()),
            },
            suggestions: Some(JSONEngineUrl {
                base: Some("https://example.com/suggestions".to_string()),
                method: Some(crate::JSONEngineMethod::Get),
                params: Some(vec![SearchUrlParam {
                    name: "suggest-name".to_string(),
                    value: None,
                    experiment_config: Some("suggest-experiment-value".to_string()),
                }]),
                search_term_param_name: Some("suggest".to_string()),
            }),
            trending: Some(JSONEngineUrl {
                base: Some("https://example.com/trending".to_string()),
                method: Some(crate::JSONEngineMethod::Get),
                params: Some(vec![SearchUrlParam {
                    name: "trend-name".to_string(),
                    value: Some("trend-value".to_string()),
                    experiment_config: None,
                }]),
                search_term_param_name: None,
            }),
        },
    });

    #[test]
    fn test_from_configuration_details_uses_values() {
        let result = SearchEngineDefinition::from_configuration_details(
            "test",
            Lazy::force(&ENGINE_BASE).clone(),
            JSONEngineVariant {
                environment: JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    ..Default::default()
                },
                optional: false,
                partner_code: None,
                telemetry_suffix: None,
                urls: None,
            },
        );

        assert_eq!(
            result,
            SearchEngineDefinition {
                aliases: vec!["foo".to_string(), "bar".to_string()],
                charset: "ISO-8859-15".to_string(),
                classification: SearchEngineClassification::Unknown,
                identifier: "test".to_string(),
                partner_code: "firefox".to_string(),
                name: "Test".to_string(),
                optional: false,
                order_hint: None,
                telemetry_suffix: None,
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com".to_string(),
                        method: "POST".to_string(),
                        params: vec![SearchUrlParam {
                            name: "param".to_string(),
                            value: Some("test param".to_string()),
                            experiment_config: None,
                        }],
                        search_term_param_name: Some("baz".to_string()),
                    },
                    suggestions: Some(SearchEngineUrl {
                        base: "https://example.com/suggestions".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "suggest-name".to_string(),
                            value: None,
                            experiment_config: Some("suggest-experiment-value".to_string()),
                        }],
                        search_term_param_name: Some("suggest".to_string()),
                    }),
                    trending: Some(SearchEngineUrl {
                        base: "https://example.com/trending".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "trend-name".to_string(),
                            value: Some("trend-value".to_string()),
                            experiment_config: None,
                        }],
                        search_term_param_name: None,
                    })
                }
            }
        )
    }

    #[test]
    fn test_from_configuration_details_merges_variants() {
        let result = SearchEngineDefinition::from_configuration_details(
            "test",
            Lazy::force(&ENGINE_BASE).clone(),
            JSONEngineVariant {
                environment: JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    ..Default::default()
                },
                optional: true,
                partner_code: Some("trek".to_string()),
                telemetry_suffix: Some("star".to_string()),
                urls: Some(JSONEngineUrls {
                    search: JSONEngineUrl {
                        base: Some("https://example.com/variant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "variant".to_string(),
                            value: Some("test variant".to_string()),
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("ship".to_string()),
                    },
                    suggestions: Some(JSONEngineUrl {
                        base: Some("https://example.com/suggestions-variant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "suggest-variant".to_string(),
                            value: Some("sugg test variant".to_string()),
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("variant".to_string()),
                    }),
                    trending: Some(JSONEngineUrl {
                        base: Some("https://example.com/trending-variant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "trend-variant".to_string(),
                            value: Some("trend test variant".to_string()),
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("trend".to_string()),
                    }),
                }),
            },
        );

        assert_eq!(
            result,
            SearchEngineDefinition {
                aliases: vec!["foo".to_string(), "bar".to_string()],
                charset: "ISO-8859-15".to_string(),
                classification: SearchEngineClassification::Unknown,
                identifier: "test".to_string(),
                partner_code: "trek".to_string(),
                name: "Test".to_string(),
                optional: true,
                order_hint: None,
                telemetry_suffix: Some("star".to_string()),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com/variant".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "variant".to_string(),
                            value: Some("test variant".to_string()),
                            experiment_config: None,
                        }],
                        search_term_param_name: Some("ship".to_string()),
                    },
                    suggestions: Some(SearchEngineUrl {
                        base: "https://example.com/suggestions-variant".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "suggest-variant".to_string(),
                            value: Some("sugg test variant".to_string()),
                            experiment_config: None,
                        }],
                        search_term_param_name: Some("variant".to_string()),
                    }),
                    trending: Some(SearchEngineUrl {
                        base: "https://example.com/trending-variant".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "trend-variant".to_string(),
                            value: Some("trend test variant".to_string()),
                            experiment_config: None,
                        }],
                        search_term_param_name: Some("trend".to_string()),
                    })
                }
            }
        )
    }

    static ENGINES_LIST: Lazy<Vec<SearchEngineDefinition>> = Lazy::new(|| {
        vec![
            SearchEngineDefinition {
                identifier: "engine1".to_string(),
                name: "Test".to_string(),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com".to_string(),
                        method: "GET".to_string(),
                        params: Vec::new(),
                        search_term_param_name: None,
                    },
                    ..Default::default()
                },
                ..Default::default()
            },
            SearchEngineDefinition {
                identifier: "engine2".to_string(),
                name: "Test 2".to_string(),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com/2".to_string(),
                        method: "GET".to_string(),
                        params: Vec::new(),
                        search_term_param_name: None,
                    },
                    ..Default::default()
                },
                ..Default::default()
            },
            SearchEngineDefinition {
                identifier: "engine3".to_string(),
                name: "Test 3".to_string(),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com/3".to_string(),
                        method: "GET".to_string(),
                        params: Vec::new(),
                        search_term_param_name: None,
                    },
                    ..Default::default()
                },
                ..Default::default()
            },
            SearchEngineDefinition {
                identifier: "engine4wildcardmatch".to_string(),
                name: "Test 4".to_string(),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com/4".to_string(),
                        method: "GET".to_string(),
                        params: Vec::new(),
                        search_term_param_name: None,
                    },
                    ..Default::default()
                },
                ..Default::default()
            },
        ]
    });

    #[test]
    fn test_determine_default_engines_returns_global_default() {
        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: String::new(),
                specific_defaults: Vec::new(),
            }),
            &SearchUserEnvironment {
                locale: "fi".into(),
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine2",
            "Should have returned the global default engine"
        );
        assert!(
            default_engine_private_id.is_none(),
            "Should not have returned an id for the private engine"
        );

        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: String::new(),
                specific_defaults: vec![JSONSpecificDefaultRecord {
                    default: "engine1".to_string(),
                    default_private: String::new(),
                    environment: JSONVariantEnvironment {
                        locales: vec!["en-GB".to_string()],
                        ..Default::default()
                    },
                }],
            }),
            &SearchUserEnvironment {
                locale: "fi".into(),
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine2",
            "Should have returned the global default engine when no specific defaults environments match"
        );
        assert!(
            default_engine_private_id.is_none(),
            "Should not have returned an id for the private engine"
        );

        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: String::new(),
                specific_defaults: vec![JSONSpecificDefaultRecord {
                    default: "engine1".to_string(),
                    default_private: String::new(),
                    environment: JSONVariantEnvironment {
                        locales: vec!["fi".to_string()],
                        ..Default::default()
                    },
                }],
            }),
            &SearchUserEnvironment {
                locale: "fi".into(),
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine1",
            "Should have returned the specific default when environments match"
        );
        assert!(
            default_engine_private_id.is_none(),
            "Should not have returned an id for the private engine"
        );

        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: String::new(),
                specific_defaults: vec![JSONSpecificDefaultRecord {
                    default: "engine4*".to_string(),
                    default_private: String::new(),
                    environment: JSONVariantEnvironment {
                        locales: vec!["fi".to_string()],
                        ..Default::default()
                    },
                }],
            }),
            &SearchUserEnvironment {
                locale: "fi".into(),
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine4wildcardmatch",
            "Should have returned the specific default when using a wildcard match"
        );
        assert!(
            default_engine_private_id.is_none(),
            "Should not have returned an id for the private engine"
        );

        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: String::new(),
                specific_defaults: vec![
                    JSONSpecificDefaultRecord {
                        default: "engine4*".to_string(),
                        default_private: String::new(),
                        environment: JSONVariantEnvironment {
                            locales: vec!["fi".to_string()],
                            ..Default::default()
                        },
                    },
                    JSONSpecificDefaultRecord {
                        default: "engine3".to_string(),
                        default_private: String::new(),
                        environment: JSONVariantEnvironment {
                            locales: vec!["fi".to_string()],
                            ..Default::default()
                        },
                    },
                ],
            }),
            &SearchUserEnvironment {
                locale: "fi".into(),
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine3",
            "Should have returned the last specific default when multiple environments match"
        );
        assert!(
            default_engine_private_id.is_none(),
            "Should not have returned an id for the private engine"
        );
    }

    #[test]
    fn test_determine_default_engines_returns_global_default_private() {
        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: "engine3".to_string(),
                specific_defaults: Vec::new(),
            }),
            &SearchUserEnvironment {
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine2",
            "Should have returned the global default engine"
        );
        assert_eq!(
            default_engine_private_id.unwrap(),
            "engine3",
            "Should have returned the global default engine for private mode"
        );

        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: "engine3".to_string(),
                specific_defaults: vec![JSONSpecificDefaultRecord {
                    default: String::new(),
                    default_private: "engine1".to_string(),
                    environment: JSONVariantEnvironment {
                        locales: vec!["en-GB".to_string()],
                        ..Default::default()
                    },
                }],
            }),
            &SearchUserEnvironment {
                locale: "fi".into(),
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine2",
            "Should have returned the global default engine when no specific defaults environments match"
        );
        assert_eq!(
            default_engine_private_id.unwrap(),
            "engine3",
            "Should have returned the global default engine for private mode when no specific defaults environments match"
        );

        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: "engine3".to_string(),
                specific_defaults: vec![JSONSpecificDefaultRecord {
                    default: String::new(),
                    default_private: "engine1".to_string(),
                    environment: JSONVariantEnvironment {
                        locales: vec!["fi".to_string()],
                        ..Default::default()
                    },
                }],
            }),
            &SearchUserEnvironment {
                locale: "fi".into(),
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine2",
            "Should have returned the global default engine when specific environments match which override the private global default (and not the global default)."
        );
        assert_eq!(
            default_engine_private_id.unwrap(),
            "engine1",
            "Should have returned the specific default engine for private mode when environments match"
        );

        let (default_engine_id, default_engine_private_id) = determine_default_engines(
            &ENGINES_LIST,
            Some(JSONDefaultEnginesRecord {
                global_default: "engine2".to_string(),
                global_default_private: String::new(),
                specific_defaults: vec![JSONSpecificDefaultRecord {
                    default: String::new(),
                    default_private: "engine4*".to_string(),
                    environment: JSONVariantEnvironment {
                        locales: vec!["fi".to_string()],
                        ..Default::default()
                    },
                }],
            }),
            &SearchUserEnvironment {
                locale: "fi".into(),
                ..Default::default()
            },
        );

        assert_eq!(
            default_engine_id.unwrap(),
            "engine2",
            "Should have returned the global default engine when specific environments match which override the private global default (and not the global default)"
        );
        assert_eq!(
            default_engine_private_id.unwrap(),
            "engine4wildcardmatch",
            "Should have returned the specific default for private mode when using a wildcard match"
        );
    }
}
