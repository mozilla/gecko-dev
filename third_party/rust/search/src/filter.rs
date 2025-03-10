/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines the functions for managing the filtering of the configuration.

use crate::configuration_overrides_types::JSONOverridesRecord;
use crate::environment_matching::matches_user_environment;
use crate::{
    error::Error, JSONDefaultEnginesRecord, JSONEngineBase, JSONEngineRecord, JSONEngineUrl,
    JSONEngineUrls, JSONEngineVariant, JSONSearchConfigurationRecords, RefinedSearchConfig,
    SearchEngineDefinition, SearchEngineUrl, SearchEngineUrls, SearchUserEnvironment,
};
use crate::{sort_helpers, JSONEngineOrdersRecord};
use remote_settings::RemoteSettingsRecord;

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

impl SearchEngineUrl {
    fn merge(&mut self, preferred: &JSONEngineUrl) {
        if let Some(base) = &preferred.base {
            self.base = base.clone();
        }
        if let Some(method) = &preferred.method {
            self.method = method.as_str().to_string();
        }
        if let Some(params) = &preferred.params {
            self.params = params.clone();
        }
        if let Some(search_term_param_name) = &preferred.search_term_param_name {
            self.search_term_param_name = Some(search_term_param_name.clone());
        }
    }
}

impl From<JSONEngineUrls> for SearchEngineUrls {
    fn from(urls: JSONEngineUrls) -> Self {
        Self {
            search: urls.search.unwrap_or_default().into(),
            suggestions: urls.suggestions.map(|suggestions| suggestions.into()),
            trending: urls.trending.map(|trending| trending.into()),
            search_form: urls.search_form.map(|search_form| search_form.into()),
        }
    }
}

impl SearchEngineUrls {
    fn merge(&mut self, preferred: &JSONEngineUrls) {
        if let Some(search_url) = &preferred.search {
            self.search.merge(search_url);
        }
        if let Some(suggestions_url) = &preferred.suggestions {
            match &mut self.suggestions {
                Some(suggestion) => suggestion.merge(suggestions_url),
                None => self.suggestions = Some(suggestions_url.clone().into()),
            };
        }
        if let Some(trending_url) = &preferred.trending {
            match &mut self.trending {
                Some(trend) => trend.merge(trending_url),
                None => self.trending = Some(trending_url.clone().into()),
            };
        }
        if let Some(search_form_url) = &preferred.search_form {
            match &mut self.search_form {
                Some(search_form) => search_form.merge(search_form_url),
                None => self.search_form = Some(search_form_url.clone().into()),
            };
        }
    }
}

impl SearchEngineDefinition {
    fn merge_variant(&mut self, variant: &JSONEngineVariant) {
        if !self.optional {
            self.optional = variant.optional;
        }
        if let Some(partner_code) = &variant.partner_code {
            self.partner_code = partner_code.clone();
        }
        if let Some(telemetry_suffix) = &variant.telemetry_suffix {
            self.telemetry_suffix = telemetry_suffix.clone();
        }
        if let Some(urls) = &variant.urls {
            self.urls.merge(urls);
        }
    }

    fn merge_override(&mut self, override_record: &JSONOverridesRecord) {
        self.partner_code = override_record.partner_code.clone();
        self.urls.merge(&override_record.urls);
        self.click_url = Some(override_record.click_url.clone());

        if let Some(telemetry_suffix) = &override_record.telemetry_suffix {
            self.telemetry_suffix = telemetry_suffix.clone();
        }
    }

    pub(crate) fn from_configuration_details(
        identifier: &str,
        base: JSONEngineBase,
        variant: &JSONEngineVariant,
        sub_variant: &Option<JSONEngineVariant>,
    ) -> SearchEngineDefinition {
        let mut engine_definition = SearchEngineDefinition {
            aliases: base.aliases.unwrap_or_default(),
            charset: base.charset.unwrap_or_else(|| "UTF-8".to_string()),
            classification: base.classification,
            identifier: identifier.to_string(),
            name: base.name,
            optional: variant.optional,
            order_hint: None,
            partner_code: base.partner_code.unwrap_or_default(),
            telemetry_suffix: String::new(),
            urls: base.urls.into(),
            click_url: None,
        };

        engine_definition.merge_variant(variant);
        if let Some(sub_variant) = sub_variant {
            engine_definition.merge_variant(sub_variant);
        }

        engine_definition
    }
}

pub(crate) struct FilterRecordsResult {
    engines: Vec<SearchEngineDefinition>,
    default_engines_record: Option<JSONDefaultEnginesRecord>,
    engine_orders_record: Option<JSONEngineOrdersRecord>,
}

pub(crate) trait Filter {
    fn filter_records(
        &self,
        user_environment: &SearchUserEnvironment,
        overrides: Option<Vec<JSONOverridesRecord>>,
    ) -> Result<FilterRecordsResult, Error>;
}

fn apply_overrides(engines: &mut [SearchEngineDefinition], overrides: &[JSONOverridesRecord]) {
    for override_record in overrides {
        for engine in engines.iter_mut() {
            if engine.identifier == override_record.identifier {
                engine.merge_override(override_record);
            }
        }
    }
}

impl Filter for Vec<RemoteSettingsRecord> {
    fn filter_records(
        &self,
        user_environment: &SearchUserEnvironment,
        overrides: Option<Vec<JSONOverridesRecord>>,
    ) -> Result<FilterRecordsResult, Error> {
        let mut engines = Vec::new();
        let mut default_engines_record = None;
        let mut engine_orders_record = None;

        for record in self {
            // TODO: Bug 1947241 - Find a way to avoid having to serialise the records
            // back to strings and then deserialise them into the records that we want.
            let stringified = serde_json::to_string(&record.fields)?;
            match record.fields.get("recordType") {
                Some(val) if *val == "engine" => {
                    let engine_config: Option<JSONEngineRecord> =
                        serde_json::from_str(&stringified)?;
                    if let Some(engine_config) = engine_config {
                        let result =
                            maybe_extract_engine_config(user_environment, Box::new(engine_config));
                        engines.extend(result);
                    }
                }
                Some(val) if *val == "defaultEngines" => {
                    default_engines_record = serde_json::from_str(&stringified)?;
                }
                Some(val) if *val == "engineOrders" => {
                    engine_orders_record = serde_json::from_str(&stringified)?;
                }
                // These cases are acceptable - we expect the potential for new
                // record types/options so that we can be flexible.
                Some(_val) => {}
                None => {}
            }
        }

        if let Some(overrides_data) = &overrides {
            apply_overrides(&mut engines, overrides_data);
        }

        Ok(FilterRecordsResult {
            engines,
            default_engines_record,
            engine_orders_record,
        })
    }
}

impl Filter for Vec<JSONSearchConfigurationRecords> {
    fn filter_records(
        &self,
        user_environment: &SearchUserEnvironment,
        overrides: Option<Vec<JSONOverridesRecord>>,
    ) -> Result<FilterRecordsResult, Error> {
        let mut engines = Vec::new();
        let mut default_engines_record = None;
        let mut engine_orders_record = None;

        for record in self {
            match record {
                JSONSearchConfigurationRecords::Engine(engine) => {
                    let result = maybe_extract_engine_config(user_environment, engine.clone());
                    engines.extend(result);
                }
                JSONSearchConfigurationRecords::DefaultEngines(default_engines) => {
                    default_engines_record = Some(default_engines);
                }
                JSONSearchConfigurationRecords::EngineOrders(engine_orders) => {
                    engine_orders_record = Some(engine_orders)
                }
                JSONSearchConfigurationRecords::Unknown => {
                    // Prevents panics if a new record type is added in future.
                }
            }
        }

        if let Some(overrides_data) = &overrides {
            apply_overrides(&mut engines, overrides_data);
        }

        Ok(FilterRecordsResult {
            engines,
            default_engines_record: default_engines_record.cloned(),
            engine_orders_record: engine_orders_record.cloned(),
        })
    }
}
pub(crate) fn filter_engine_configuration_impl(
    user_environment: SearchUserEnvironment,
    configuration: &impl Filter,
    overrides: Option<Vec<JSONOverridesRecord>>,
) -> Result<RefinedSearchConfig, Error> {
    let mut user_environment = user_environment.clone();
    user_environment.locale = user_environment.locale.to_lowercase();
    user_environment.region = user_environment.region.to_lowercase();
    user_environment.version = user_environment.version.to_lowercase();

    let filtered_result = configuration.filter_records(&user_environment, overrides);

    filtered_result.map(|result| {
        let (default_engine_id, default_private_engine_id) = determine_default_engines(
            &result.engines,
            result.default_engines_record,
            &user_environment,
        );

        let mut engines = result.engines.clone();

        if let Some(orders_record) = result.engine_orders_record {
            for order_data in &orders_record.orders {
                if matches_user_environment(&order_data.environment, &user_environment) {
                    sort_helpers::set_engine_order(&mut engines, &order_data.order);
                }
            }
        }

        engines.sort_by(|a, b| {
            sort_helpers::sort(
                default_engine_id.as_ref(),
                default_private_engine_id.as_ref(),
                a,
                b,
            )
        });

        RefinedSearchConfig {
            engines,
            app_default_engine_id: default_engine_id,
            app_private_default_engine_id: default_private_engine_id,
        }
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

    let mut matching_sub_variant = None;
    if let Some(variant) = &matching_variant {
        matching_sub_variant = variant
            .sub_variants
            .iter()
            .rev()
            .find(|r| matches_user_environment(&r.environment, user_environment))
            .cloned();
    }

    matching_variant.map(|variant| {
        SearchEngineDefinition::from_configuration_details(
            &identifier,
            base,
            &variant,
            &matching_sub_variant,
        )
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
                if let Some(engine_id) =
                    find_engine_id_with_match(engines, specific_default.default)
                {
                    default_engine_id.replace(engine_id);
                }
                if let Some(private_engine_id) =
                    find_engine_id_with_match(engines, specific_default.default_private)
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

fn find_engine_id_with_match(
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
    fn test_merge_override() {
        let mut test_engine = SearchEngineDefinition {
            identifier: "test".to_string(),
            partner_code: "partner-code".to_string(),
            telemetry_suffix: "original-telemetry-suffix".to_string(),
            ..Default::default()
        };

        let override_record = JSONOverridesRecord {
            identifier: "test".to_string(),
            partner_code: "override-partner-code".to_string(),
            click_url: "https://example.com/click-url".to_string(),
            telemetry_suffix: None,
            urls: JSONEngineUrls {
                search: Some(JSONEngineUrl {
                    base: Some("https://example.com/override-search".to_string()),
                    method: None,
                    params: None,
                    search_term_param_name: None,
                }),
                ..Default::default()
            },
        };

        test_engine.merge_override(&override_record);

        assert_eq!(
            test_engine.partner_code, "override-partner-code",
            "Should override the partner code"
        );
        assert_eq!(
            test_engine.click_url,
            Some("https://example.com/click-url".to_string()),
            "Should override the click url"
        );
        assert_eq!(
            test_engine.urls.search.base, "https://example.com/override-search",
            "Should override search url"
        );
        assert_eq!(
            test_engine.telemetry_suffix, "original-telemetry-suffix",
            "Should not override telemetry suffix when telemetry suffix is supplied as None"
        );
    }

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
                    search: Some(JSONEngineUrl {
                        base: Some("https://example.com".to_string()),
                        method: None,
                        params: None,
                        search_term_param_name: None,
                    }),
                    suggestions: None,
                    trending: None,
                    search_form: None,
                },
            },
            &JSONEngineVariant {
                environment: JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    ..Default::default()
                },
                optional: false,
                partner_code: None,
                telemetry_suffix: None,
                urls: None,
                sub_variants: vec![],
            },
            &None,
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
                telemetry_suffix: String::new(),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com".to_string(),
                        method: "GET".to_string(),
                        params: Vec::new(),
                        search_term_param_name: None,
                    },
                    suggestions: None,
                    trending: None,
                    search_form: None
                },
                click_url: None
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
            search: Some(JSONEngineUrl {
                base: Some("https://example.com".to_string()),
                method: Some(crate::JSONEngineMethod::Post),
                params: Some(vec![
                    SearchUrlParam {
                        name: "param".to_string(),
                        value: Some("test param".to_string()),
                        enterprise_value: None,
                        experiment_config: None,
                    },
                    SearchUrlParam {
                        name: "enterprise-name".to_string(),
                        value: None,
                        enterprise_value: Some("enterprise-value".to_string()),
                        experiment_config: None,
                    },
                ]),
                search_term_param_name: Some("baz".to_string()),
            }),
            suggestions: Some(JSONEngineUrl {
                base: Some("https://example.com/suggestions".to_string()),
                method: Some(crate::JSONEngineMethod::Get),
                params: Some(vec![SearchUrlParam {
                    name: "suggest-name".to_string(),
                    value: None,
                    enterprise_value: None,
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
                    enterprise_value: None,
                    experiment_config: None,
                }]),
                search_term_param_name: None,
            }),
            search_form: Some(JSONEngineUrl {
                base: Some("https://example.com/search_form".to_string()),
                method: Some(crate::JSONEngineMethod::Get),
                params: Some(vec![SearchUrlParam {
                    name: "search-form-name".to_string(),
                    value: Some("search-form-value".to_string()),
                    enterprise_value: None,
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
            &JSONEngineVariant {
                environment: JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    ..Default::default()
                },
                optional: false,
                partner_code: None,
                telemetry_suffix: None,
                urls: None,
                sub_variants: vec![],
            },
            &None,
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
                telemetry_suffix: String::new(),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com".to_string(),
                        method: "POST".to_string(),
                        params: vec![
                            SearchUrlParam {
                                name: "param".to_string(),
                                value: Some("test param".to_string()),
                                enterprise_value: None,
                                experiment_config: None,
                            },
                            SearchUrlParam {
                                name: "enterprise-name".to_string(),
                                value: None,
                                enterprise_value: Some("enterprise-value".to_string()),
                                experiment_config: None,
                            },
                        ],
                        search_term_param_name: Some("baz".to_string()),
                    },
                    suggestions: Some(SearchEngineUrl {
                        base: "https://example.com/suggestions".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "suggest-name".to_string(),
                            value: None,
                            enterprise_value: None,
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
                            enterprise_value: None,
                            experiment_config: None,
                        }],
                        search_term_param_name: None,
                    }),
                    search_form: Some(SearchEngineUrl {
                        base: "https://example.com/search_form".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "search-form-name".to_string(),
                            value: Some("search-form-value".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }],
                        search_term_param_name: None,
                    }),
                },
                click_url: None
            }
        )
    }

    #[test]
    fn test_from_configuration_details_merges_variants() {
        let result = SearchEngineDefinition::from_configuration_details(
            "test",
            Lazy::force(&ENGINE_BASE).clone(),
            &JSONEngineVariant {
                environment: JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    ..Default::default()
                },
                optional: true,
                partner_code: Some("trek".to_string()),
                telemetry_suffix: Some("star".to_string()),
                urls: Some(JSONEngineUrls {
                    search: Some(JSONEngineUrl {
                        base: Some("https://example.com/variant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "variant".to_string(),
                            value: Some("test variant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("ship".to_string()),
                    }),
                    suggestions: Some(JSONEngineUrl {
                        base: Some("https://example.com/suggestions-variant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "suggest-variant".to_string(),
                            value: Some("sugg test variant".to_string()),
                            enterprise_value: None,
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
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("trend".to_string()),
                    }),
                    search_form: Some(JSONEngineUrl {
                        base: Some("https://example.com/search_form".to_string()),
                        method: Some(crate::JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "search-form-name".to_string(),
                            value: Some("search-form-value".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: None,
                    }),
                }),
                sub_variants: vec![],
            },
            &None,
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
                telemetry_suffix: "star".to_string(),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com/variant".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "variant".to_string(),
                            value: Some("test variant".to_string()),
                            enterprise_value: None,
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
                            enterprise_value: None,
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
                            enterprise_value: None,
                            experiment_config: None,
                        }],
                        search_term_param_name: Some("trend".to_string()),
                    }),
                    search_form: Some(SearchEngineUrl {
                        base: "https://example.com/search_form".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "search-form-name".to_string(),
                            value: Some("search-form-value".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }],
                        search_term_param_name: None,
                    }),
                },
                click_url: None
            }
        )
    }

    #[test]
    fn test_from_configuration_details_merges_sub_variants() {
        let result = SearchEngineDefinition::from_configuration_details(
            "test",
            Lazy::force(&ENGINE_BASE).clone(),
            &JSONEngineVariant {
                environment: JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    ..Default::default()
                },
                optional: true,
                partner_code: Some("trek".to_string()),
                telemetry_suffix: Some("star".to_string()),
                urls: Some(JSONEngineUrls {
                    search: Some(JSONEngineUrl {
                        base: Some("https://example.com/variant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "variant".to_string(),
                            value: Some("test variant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("ship".to_string()),
                    }),
                    suggestions: Some(JSONEngineUrl {
                        base: Some("https://example.com/suggestions-variant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "suggest-variant".to_string(),
                            value: Some("sugg test variant".to_string()),
                            enterprise_value: None,
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
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("trend".to_string()),
                    }),
                    search_form: Some(JSONEngineUrl {
                        base: Some("https://example.com/search-form-variant".to_string()),
                        method: Some(crate::JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "search-form-variant".to_string(),
                            value: Some("search form variant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: None,
                    }),
                }),
                // This would be the list of sub-variants for this part of the
                // configuration, however it is not used as the actual sub-variant
                // to be merged is passed as the third argument to
                // `from_configuration_details`.
                sub_variants: vec![],
            },
            &Some(JSONEngineVariant {
                environment: JSONVariantEnvironment {
                    all_regions_and_locales: true,
                    ..Default::default()
                },
                optional: true,
                partner_code: Some("trek2".to_string()),
                telemetry_suffix: Some("star2".to_string()),
                urls: Some(JSONEngineUrls {
                    search: Some(JSONEngineUrl {
                        base: Some("https://example.com/subvariant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "subvariant".to_string(),
                            value: Some("test subvariant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("shuttle".to_string()),
                    }),
                    suggestions: Some(JSONEngineUrl {
                        base: Some("https://example.com/suggestions-subvariant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "suggest-subvariant".to_string(),
                            value: Some("sugg test subvariant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("subvariant".to_string()),
                    }),
                    trending: Some(JSONEngineUrl {
                        base: Some("https://example.com/trending-subvariant".to_string()),
                        method: Some(JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "trend-subvariant".to_string(),
                            value: Some("trend test subvariant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: Some("subtrend".to_string()),
                    }),
                    search_form: Some(JSONEngineUrl {
                        base: Some("https://example.com/search-form-subvariant".to_string()),
                        method: Some(crate::JSONEngineMethod::Get),
                        params: Some(vec![SearchUrlParam {
                            name: "search-form-subvariant".to_string(),
                            value: Some("search form subvariant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }]),
                        search_term_param_name: None,
                    }),
                }),
                sub_variants: vec![],
            }),
        );

        assert_eq!(
            result,
            SearchEngineDefinition {
                aliases: vec!["foo".to_string(), "bar".to_string()],
                charset: "ISO-8859-15".to_string(),
                classification: SearchEngineClassification::Unknown,
                identifier: "test".to_string(),
                partner_code: "trek2".to_string(),
                name: "Test".to_string(),
                optional: true,
                order_hint: None,
                telemetry_suffix: "star2".to_string(),
                urls: SearchEngineUrls {
                    search: SearchEngineUrl {
                        base: "https://example.com/subvariant".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "subvariant".to_string(),
                            value: Some("test subvariant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }],
                        search_term_param_name: Some("shuttle".to_string()),
                    },
                    suggestions: Some(SearchEngineUrl {
                        base: "https://example.com/suggestions-subvariant".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "suggest-subvariant".to_string(),
                            value: Some("sugg test subvariant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }],
                        search_term_param_name: Some("subvariant".to_string()),
                    }),
                    trending: Some(SearchEngineUrl {
                        base: "https://example.com/trending-subvariant".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "trend-subvariant".to_string(),
                            value: Some("trend test subvariant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }],
                        search_term_param_name: Some("subtrend".to_string()),
                    }),
                    search_form: Some(SearchEngineUrl {
                        base: "https://example.com/search-form-subvariant".to_string(),
                        method: "GET".to_string(),
                        params: vec![SearchUrlParam {
                            name: "search-form-subvariant".to_string(),
                            value: Some("search form subvariant".to_string()),
                            enterprise_value: None,
                            experiment_config: None,
                        }],
                        search_term_param_name: None,
                    }),
                },
                click_url: None
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
