/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines the main `SearchEngineSelector`.

use crate::filter::filter_engine_configuration;
use crate::{
    error::Error, JSONSearchConfiguration, RefinedSearchConfig, SearchApiResult,
    SearchUserEnvironment,
};
use error_support::handle_error;
use parking_lot::Mutex;
use std::sync::Arc;

#[derive(Default)]
pub(crate) struct SearchEngineSelectorInner {
    configuration: Option<JSONSearchConfiguration>,
}

/// SearchEngineSelector parses the JSON configuration for
/// search engines and returns the applicable engines depending
/// on their region + locale.
#[derive(Default, uniffi::Object)]
pub struct SearchEngineSelector(Mutex<SearchEngineSelectorInner>);

#[uniffi::export]
impl SearchEngineSelector {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self(Mutex::default())
    }

    /// Sets the search configuration from the given string. If the configuration
    /// string is unchanged since the last update, the cached configuration is
    /// reused to avoid unnecessary reprocessing. This helps optimize performance,
    /// particularly during test runs where the same configuration may be used
    /// repeatedly.
    #[handle_error(Error)]
    pub fn set_search_config(self: Arc<Self>, configuration: String) -> SearchApiResult<()> {
        if configuration.is_empty() {
            return Err(Error::SearchConfigNotSpecified);
        }
        self.0.lock().configuration = serde_json::from_str(&configuration)?;
        Ok(())
    }

    /// Clears the search configuration from memory if it is known that it is
    /// not required for a time, e.g. if the configuration will only be re-filtered
    /// after an app/environment update.
    pub fn clear_search_config(self: Arc<Self>) {}

    /// Filters the search configuration with the user's given environment,
    /// and returns the set of engines and parameters that should be presented
    /// to the user.
    #[handle_error(Error)]
    pub fn filter_engine_configuration(
        self: Arc<Self>,
        user_environment: SearchUserEnvironment,
    ) -> SearchApiResult<RefinedSearchConfig> {
        let data = match &self.0.lock().configuration {
            None => return Err(Error::SearchConfigNotSpecified),
            Some(configuration) => configuration.data.clone(),
        };
        filter_engine_configuration(user_environment, data)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::*;
    use pretty_assertions::assert_eq;
    use serde_json::json;

    #[test]
    fn test_set_config_should_allow_basic_config() {
        let selector = Arc::new(SearchEngineSelector::new());

        let config_result = Arc::clone(&selector).set_search_config(
            json!({
              "data": [
                {
                  "recordType": "engine",
                  "identifier": "test",
                  "base": {
                    "name": "Test",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com",
                        "method": "GET"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true,
                      "excludedRegions": []
                    }
                  }],
                },
                {
                  "recordType": "defaultEngines",
                  "globalDefault": "test"
                }
              ]
            })
            .to_string(),
        );
        assert!(
            config_result.is_ok(),
            "Should have set the configuration successfully. {:?}",
            config_result
        );
    }

    #[test]
    fn test_set_config_should_allow_extra_fields() {
        let selector = Arc::new(SearchEngineSelector::new());

        let config_result = Arc::clone(&selector).set_search_config(
            json!({
              "data": [
                {
                  "recordType": "engine",
                  "identifier": "test",
                  "base": {
                    "name": "Test",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com",
                        "method": "GET",
                        "extraField1": true
                      }
                    },
                    "extraField2": "123"
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    }
                  }],
                  "extraField3": ["foo"]
                },
                {
                  "recordType": "defaultEngines",
                  "globalDefault": "test",
                  "extraField4": {
                    "subField1": true
                  }
                }
              ]
            })
            .to_string(),
        );
        assert!(
            config_result.is_ok(),
            "Should have set the configuration successfully with extra fields. {:?}",
            config_result
        );
    }

    #[test]
    fn test_set_config_should_ignore_unknown_record_types() {
        let selector = Arc::new(SearchEngineSelector::new());

        let config_result = Arc::clone(&selector).set_search_config(
            json!({
              "data": [
                {
                  "recordType": "engine",
                  "identifier": "test",
                  "base": {
                    "name": "Test",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com",
                        "method": "GET"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    }
                  }],
                },
                {
                  "recordType": "defaultEngines",
                  "globalDefault": "test"
                },
                {
                  "recordType": "unknown"
                }
              ]
            })
            .to_string(),
        );
        assert!(
            config_result.is_ok(),
            "Should have set the configuration successfully with unknown record types. {:?}",
            config_result
        );
    }

    #[test]
    fn test_filter_engine_configuration_throws_without_config() {
        let selector = Arc::new(SearchEngineSelector::new());

        let result = selector.filter_engine_configuration(SearchUserEnvironment {
            ..Default::default()
        });

        assert!(
            result.is_err(),
            "Should throw an error when a configuration has not been specified before filtering"
        );
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Search configuration not specified"))
    }

    #[test]
    fn test_filter_engine_configuration_returns_basic_engines() {
        let selector = Arc::new(SearchEngineSelector::new());

        let config_result = Arc::clone(&selector).set_search_config(
            json!({
              "data": [
                {
                  "recordType": "engine",
                  "identifier": "test1",
                  "base": {
                    "name": "Test 1",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com/1",
                        "method": "GET",
                        "searchTermParamName": "q"
                      },
                      "suggestions": {
                        "base": "https://example.com/suggestions",
                        "method": "POST",
                        "params": [{
                          "name": "suggestion-name",
                          "value": "suggestion-value",
                        }],
                        "searchTermParamName": "suggest"
                      },
                      "trending": {
                        "base": "https://example.com/trending",
                        "method": "GET",
                        "params": [{
                          "name": "trending-name",
                          "experimentConfig": "trending-experiment-value",
                        }]
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    }
                  }],
                },
                {
                  "recordType": "engine",
                  "identifier": "test2",
                  "base": {
                    "name": "Test 2",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com/2",
                        "method": "GET",
                        "searchTermParamName": "search"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    }
                  }],
                },
                {
                  "recordType": "defaultEngines",
                  "globalDefault": "test1",
                  "globalDefaultPrivate": "test2"
                }
              ]
            })
            .to_string(),
        );
        assert!(
            config_result.is_ok(),
            "Should have set the configuration successfully. {:?}",
            config_result
        );

        let result = selector.filter_engine_configuration(SearchUserEnvironment {
            ..Default::default()
        });

        assert!(
            result.is_ok(),
            "Should have filtered the configuration without error. {:?}",
            result
        );
        assert_eq!(
            result.unwrap(),
            RefinedSearchConfig {
                engines: vec!(
                    SearchEngineDefinition {
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test1".to_string(),
                        name: "Test 1".to_string(),
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/1".to_string(),
                                method: "GET".to_string(),
                                params: Vec::new(),
                                search_term_param_name: Some("q".to_string())
                            },
                            suggestions: Some(SearchEngineUrl {
                                base: "https://example.com/suggestions".to_string(),
                                method: "POST".to_string(),
                                params: vec![SearchUrlParam {
                                    name: "suggestion-name".to_string(),
                                    value: Some("suggestion-value".to_string()),
                                    experiment_config: None
                                }],
                                search_term_param_name: Some("suggest".to_string())
                            }),
                            trending: Some(SearchEngineUrl {
                                base: "https://example.com/trending".to_string(),
                                method: "GET".to_string(),
                                params: vec![SearchUrlParam {
                                    name: "trending-name".to_string(),
                                    value: None,
                                    experiment_config: Some(
                                        "trending-experiment-value".to_string()
                                    )
                                }],
                                search_term_param_name: None
                            })
                        },
                        ..Default::default()
                    },
                    SearchEngineDefinition {
                        aliases: Vec::new(),
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test2".to_string(),
                        name: "Test 2".to_string(),
                        optional: false,
                        order_hint: None,
                        partner_code: String::new(),
                        telemetry_suffix: None,
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/2".to_string(),
                                method: "GET".to_string(),
                                params: Vec::new(),
                                search_term_param_name: Some("search".to_string())
                            },
                            suggestions: None,
                            trending: None
                        }
                    }
                ),
                app_default_engine_id: Some("test1".to_string()),
                app_private_default_engine_id: Some("test2".to_string())
            }
        )
    }

    #[test]
    fn test_filter_engine_configuration_handles_basic_variants() {
        let selector = Arc::new(SearchEngineSelector::new());

        let config_result = Arc::clone(&selector).set_search_config(
            json!({
              "data": [
                {
                  "recordType": "engine",
                  "identifier": "test1",
                  "partnerCode": "star",
                  "base": {
                    "name": "Test 1",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com/1",
                        "method": "GET",
                        "searchTermParamName": "q"
                      },
                      "suggestions": {
                        "base": "https://example.com/suggestions",
                        "method": "POST",
                        "params": [{
                          "name": "type",
                          "value": "space",
                        }],
                        "searchTermParamName": "suggest"
                      },
                      "trending": {
                        "base": "https://example.com/trending",
                        "method": "GET",
                        "params": [{
                          "name": "area",
                          "experimentConfig": "area-param",
                        }]
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    },
                  },
                  {
                    "environment": {
                      "regions": ["FR"]
                    },
                    "urls": {
                      "search": {
                        "method": "POST",
                        "params": [{
                          "name": "mission",
                          "value": "ongoing"
                        }]
                      }
                    }
                  }],
                },
                {
                  "recordType": "engine",
                  "identifier": "test2",
                  "base": {
                    "name": "Test 2",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com/2",
                        "method": "GET",
                        "searchTermParamName": "search"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    },
                    "partnerCode": "ship",
                    "telemetrySuffix": "E",
                    "optional": true
                  }],
                },
                {
                  "recordType": "defaultEngines",
                  "globalDefault": "test1",
                  "globalDefaultPrivate": "test2"
                }
              ]
            })
            .to_string(),
        );
        assert!(
            config_result.is_ok(),
            "Should have set the configuration successfully. {:?}",
            config_result
        );

        let result = selector.filter_engine_configuration(SearchUserEnvironment {
            region: "FR".into(),
            ..Default::default()
        });

        assert!(
            result.is_ok(),
            "Should have filtered the configuration without error. {:?}",
            result
        );
        assert_eq!(
            result.unwrap(),
            RefinedSearchConfig {
                engines: vec!(
                    SearchEngineDefinition {
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test1".to_string(),
                        name: "Test 1".to_string(),
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/1".to_string(),
                                method: "POST".to_string(),
                                params: vec![SearchUrlParam {
                                    name: "mission".to_string(),
                                    value: Some("ongoing".to_string()),
                                    experiment_config: None
                                }],
                                search_term_param_name: Some("q".to_string())
                            },
                            suggestions: Some(SearchEngineUrl {
                                base: "https://example.com/suggestions".to_string(),
                                method: "POST".to_string(),
                                params: vec![SearchUrlParam {
                                    name: "type".to_string(),
                                    value: Some("space".to_string()),
                                    experiment_config: None
                                }],
                                search_term_param_name: Some("suggest".to_string())
                            }),
                            trending: Some(SearchEngineUrl {
                                base: "https://example.com/trending".to_string(),
                                method: "GET".to_string(),
                                params: vec![SearchUrlParam {
                                    name: "area".to_string(),
                                    value: None,
                                    experiment_config: Some("area-param".to_string())
                                }],
                                search_term_param_name: None
                            })
                        },
                        ..Default::default()
                    },
                    SearchEngineDefinition {
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test2".to_string(),
                        name: "Test 2".to_string(),
                        optional: true,
                        partner_code: "ship".to_string(),
                        telemetry_suffix: Some("E".to_string()),
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/2".to_string(),
                                method: "GET".to_string(),
                                params: Vec::new(),
                                search_term_param_name: Some("search".to_string())
                            },
                            ..Default::default()
                        },
                        ..Default::default()
                    }
                ),
                app_default_engine_id: Some("test1".to_string()),
                app_private_default_engine_id: Some("test2".to_string())
            }
        )
    }

    #[test]
    fn test_filter_engine_configuration_handles_environments() {
        let selector = Arc::new(SearchEngineSelector::new());

        let config_result = Arc::clone(&selector).set_search_config(
            json!({
              "data": [
                {
                  "recordType": "engine",
                  "identifier": "test1",
                  "base": {
                    "name": "Test 1",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com/1",
                        "method": "GET",
                        "searchTermParamName": "q"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    }
                  }],
                },
                {
                  "recordType": "engine",
                  "identifier": "test2",
                  "base": {
                    "name": "Test 2",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com/2",
                        "method": "GET",
                        "searchTermParamName": "search"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "applications": ["firefox-android", "focus-ios"]
                    }
                  }],
                },
                {
                  "recordType": "engine",
                  "identifier": "test3",
                  "base": {
                    "name": "Test 3",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com/3",
                        "method": "GET",
                        "searchTermParamName": "trek"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "distributions": ["starship"]
                    }
                  }],
                },
                {
                  "recordType": "defaultEngines",
                  "globalDefault": "test1",
                }
              ]
            })
            .to_string(),
        );
        assert!(
            config_result.is_ok(),
            "Should have set the configuration successfully. {:?}",
            config_result
        );

        let mut result = Arc::clone(&selector).filter_engine_configuration(SearchUserEnvironment {
            distribution_id: String::new(),
            app_name: SearchApplicationName::Firefox,
            ..Default::default()
        });

        assert!(
            result.is_ok(),
            "Should have filtered the configuration without error. {:?}",
            result
        );
        assert_eq!(
            result.unwrap(),
            RefinedSearchConfig {
                engines: vec!(
                    SearchEngineDefinition {
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test1".to_string(),
                        name: "Test 1".to_string(),
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/1".to_string(),
                                method: "GET".to_string(),
                                params: Vec::new(),
                                search_term_param_name: Some("q".to_string())
                            },
                            ..Default::default()
                        },
                        ..Default::default()
                      },
            ),
                app_default_engine_id: Some("test1".to_string()),
                app_private_default_engine_id: None
            }, "Should have selected test1 for all matching locales, as the environments do not match for the other two"
        );

        result = Arc::clone(&selector).filter_engine_configuration(SearchUserEnvironment {
            distribution_id: String::new(),
            app_name: SearchApplicationName::FocusIos,
            ..Default::default()
        });

        assert!(
            result.is_ok(),
            "Should have filtered the configuration without error. {:?}",
            result
        );
        assert_eq!(
            result.unwrap(),
            RefinedSearchConfig {
                engines: vec!(
                    SearchEngineDefinition {
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test1".to_string(),
                        name: "Test 1".to_string(),
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/1".to_string(),
                                method: "GET".to_string(),
                                params: Vec::new(),
                                search_term_param_name: Some("q".to_string())
                            },
                            ..Default::default()
                        },
                        ..Default::default()
                    },
                    SearchEngineDefinition {
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test2".to_string(),
                        name: "Test 2".to_string(),
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/2".to_string(),
                                method: "GET".to_string(),
                                params: Vec::new(),
                                search_term_param_name: Some("search".to_string())
                            },
                            ..Default::default()
                        },
                        ..Default::default()
                    },
                ),
                app_default_engine_id: Some("test1".to_string()),
                app_private_default_engine_id: None
            },
            "Should have selected test1 for all matching locales and test2 for matching Focus IOS"
        );

        result = Arc::clone(&selector).filter_engine_configuration(SearchUserEnvironment {
            distribution_id: "starship".to_string(),
            app_name: SearchApplicationName::Firefox,
            ..Default::default()
        });

        assert!(
            result.is_ok(),
            "Should have filtered the configuration without error. {:?}",
            result
        );
        assert_eq!(
            result.unwrap(),
            RefinedSearchConfig {
                engines: vec!(
                    SearchEngineDefinition {
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test1".to_string(),
                        name: "Test 1".to_string(),
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/1".to_string(),
                                method: "GET".to_string(),
                                params: Vec::new(),
                                search_term_param_name: Some("q".to_string())
                            },
                            ..Default::default()
                        },
                        ..Default::default()
                    },
                    SearchEngineDefinition {
                        charset: "UTF-8".to_string(),
                        classification: SearchEngineClassification::General,
                        identifier: "test3".to_string(),
                        name: "Test 3".to_string(),
                        urls: SearchEngineUrls {
                            search: SearchEngineUrl {
                                base: "https://example.com/3".to_string(),
                                method: "GET".to_string(),
                                params: Vec::new(),
                                search_term_param_name: Some("trek".to_string())
                            },
                            ..Default::default()
                        },
                        ..Default::default()
                      },
                ),
                app_default_engine_id: Some("test1".to_string()),
                app_private_default_engine_id: None
            }, "Should have selected test1 for all matching locales and test3 for matching the distribution id"
        );
    }

    #[test]
    fn test_set_config_should_handle_default_engines() {
        let selector = Arc::new(SearchEngineSelector::new());

        let config_result = Arc::clone(&selector).set_search_config(
            json!({
              "data": [
                {
                  "recordType": "engine",
                  "identifier": "test",
                  "base": {
                    "name": "Test",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com",
                        "method": "GET",
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    }
                  }],
                },
                {
                  "recordType": "engine",
                  "identifier": "distro-default",
                  "base": {
                    "name": "Distribution Default",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com",
                        "method": "GET"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true
                    }
                  }],
                },
                {
                  "recordType": "engine",
                  "identifier": "private-default-FR",
                  "base": {
                    "name": "Private default FR",
                    "classification": "general",
                    "urls": {
                      "search": {
                        "base": "https://example.com",
                        "method": "GET"
                      }
                    }
                  },
                  "variants": [{
                    "environment": {
                      "allRegionsAndLocales": true,
                    }
                  }],
                },
                {
                  "recordType": "defaultEngines",
                  "globalDefault": "test",
                  "specificDefaults": [{
                    "environment": {
                      "distributions": ["test-distro"],
                    },
                    "default": "distro-default"
                  }, {
                    "environment": {
                      "regions": ["fr"]
                    },
                    "defaultPrivate": "private-default-FR"
                  }]
                }
              ]
            })
            .to_string(),
        );
        assert!(
            config_result.is_ok(),
            "Should have set the configuration successfully. {:?}",
            config_result
        );
        let expected_engines = vec![
            SearchEngineDefinition {
                charset: "UTF-8".to_string(),
                classification: SearchEngineClassification::General,
                identifier: "test".to_string(),
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
                charset: "UTF-8".to_string(),
                classification: SearchEngineClassification::General,
                identifier: "distro-default".to_string(),
                name: "Distribution Default".to_string(),
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
                charset: "UTF-8".to_string(),
                classification: SearchEngineClassification::General,
                identifier: "private-default-FR".to_string(),
                name: "Private default FR".to_string(),
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
        ];

        let result = Arc::clone(&selector).filter_engine_configuration(SearchUserEnvironment {
            distribution_id: "test-distro".to_string(),
            ..Default::default()
        });
        assert!(
            result.is_ok(),
            "Should have filtered the configuration without error. {:?}",
            result
        );
        assert_eq!(
            result.unwrap(),
            RefinedSearchConfig {
                engines: expected_engines.clone(),
                app_default_engine_id: Some("distro-default".to_string()),
                app_private_default_engine_id: None
            },
            "Should have selected the default engine for the matching specific default"
        );

        let result = Arc::clone(&selector).filter_engine_configuration(SearchUserEnvironment {
            region: "fr".into(),
            distribution_id: String::new(),
            ..Default::default()
        });
        assert!(
            result.is_ok(),
            "Should have filtered the configuration without error. {:?}",
            result
        );
        assert_eq!(
            result.unwrap(),
            RefinedSearchConfig {
                engines: expected_engines,
                app_default_engine_id: Some("test".to_string()),
                app_private_default_engine_id: Some("private-default-FR".to_string())
            },
            "Should have selected the private default engine for the matching specific default"
        );
    }
}
