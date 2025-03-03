/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines functions for sorting search engines based on priority
//! and order hints, falling back to alphabetical sorting when neither is provided.

use crate::SearchEngineDefinition;

pub(crate) fn set_engine_order(engines: &mut [SearchEngineDefinition], ordered_engines: &[String]) {
    let mut order_number = ordered_engines.len();

    for engine_id in ordered_engines {
        if let Some(found_engine) = find_engine_with_match_mut(engines, engine_id) {
            found_engine.order_hint = Some(order_number as u32);
            order_number -= 1;
        }
    }
}

pub(crate) fn sort(
    default_engine_id: Option<&String>,
    default_private_engine_id: Option<&String>,
    a: &SearchEngineDefinition,
    b: &SearchEngineDefinition,
) -> std::cmp::Ordering {
    let b_index = get_priority(b, default_engine_id, default_private_engine_id);
    let a_index = get_priority(a, default_engine_id, default_private_engine_id);
    let order = b_index.cmp(&a_index);

    // TODO: update the comparison to use ICU4X when it's available.
    // See Bug 1945295: https://bugzilla.mozilla.org/show_bug.cgi?id=1945295
    // If order is equal and order_hint is None for both, fall back to alphabetical sorting
    if order == std::cmp::Ordering::Equal {
        return a.identifier.cmp(&b.identifier);
    }

    order
}

fn find_engine_with_match_mut<'a>(
    engines: &'a mut [SearchEngineDefinition],
    engine_id_match: &String,
) -> Option<&'a mut SearchEngineDefinition> {
    if engine_id_match.is_empty() {
        return None;
    }
    if let Some(match_no_star) = engine_id_match.strip_suffix('*') {
        return engines
            .iter_mut()
            .find(|e| e.identifier.starts_with(match_no_star));
    }

    engines
        .iter_mut()
        .find(|e| e.identifier == *engine_id_match)
}

fn get_priority(
    engine: &SearchEngineDefinition,
    default_engine_id: Option<&String>,
    default_private_engine_id: Option<&String>,
) -> u32 {
    if Some(&engine.identifier) == default_engine_id {
        return u32::MAX;
    }
    if Some(&engine.identifier) == default_private_engine_id {
        return u32::MAX - 1;
    }
    engine.order_hint.unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::*;
    use pretty_assertions::assert_eq;

    fn create_engine(engine_id: &str, order_hint: Option<u32>) -> SearchEngineDefinition {
        SearchEngineDefinition {
            identifier: engine_id.to_string(),
            order_hint,
            ..Default::default()
        }
    }

    #[test]
    fn test_find_engine_with_match_mut_starts_with() {
        let mut engines = vec![
            create_engine("wiki-ca", None),
            create_engine("wiki-uk", None),
            create_engine("test-engine", None),
        ];
        let found_engine = find_engine_with_match_mut(&mut engines, &"wiki*".to_string());

        assert_eq!(
            found_engine.unwrap().identifier,
            "wiki-ca",
            "Should match the first engine that starts with 'wiki'."
        );
    }

    #[test]
    fn test_set_engine_order_full_list() {
        let mut engines = vec![
            create_engine("last-engine", None),
            create_engine("secondary-engine", None),
            create_engine("primary-engine", None),
        ];
        let ordered_engines_list = vec![
            "primary-engine".to_string(),
            "secondary-engine".to_string(),
            "last-engine".to_string(),
        ];
        set_engine_order(&mut engines, &ordered_engines_list);

        let expected_order_hints = vec![
            ("last-engine", Some(1)),
            ("secondary-engine", Some(2)),
            ("primary-engine", Some(3)),
        ];
        let actual_order_hints: Vec<(&str, Option<u32>)> = engines
            .iter()
            .map(|e| (e.identifier.as_str(), e.order_hint))
            .collect();

        assert_eq!(
            actual_order_hints, expected_order_hints,
            "Should assign correct order hints when all engines are in the ordered engines list, the first engine with the highest and decreasing for each next engine."
        )
    }

    #[test]
    fn test_set_engine_order_partial_list() {
        let mut engines = vec![
            create_engine("secondary-engine", None),
            create_engine("primary-engine", None),
            create_engine("no-order-hint-engine", None),
        ];
        let ordered_engines_list =
            vec!["primary-engine".to_string(), "secondary-engine".to_string()];
        set_engine_order(&mut engines, &ordered_engines_list);

        let expected_order_hints = vec![
            ("secondary-engine", Some(1)),
            ("primary-engine", Some(2)),
            ("no-order-hint-engine", None),
        ];
        let actual_order_hints: Vec<(&str, Option<u32>)> = engines
            .iter()
            .map(|e| (e.identifier.as_str(), e.order_hint))
            .collect();
        assert_eq!(
            actual_order_hints, expected_order_hints,
            "Should assign correct order hints when some of the engines are in the ordered engines list, the first engine with the highest and decreasing for each next engine."
        )
    }

    #[test]
    fn test_sort_engines_by_order_hint() {
        let default_engine_id = None;
        let default_private_engine_id = None;
        let mut engines = vec![
            create_engine("c-engine", Some(3)),
            create_engine("b-engine", Some(2)),
            create_engine("a-engine", Some(1)),
        ];
        engines.sort_by(|a, b| {
            sort(
                default_engine_id.as_ref(),
                default_private_engine_id.as_ref(),
                a,
                b,
            )
        });

        let actual_order: Vec<&str> = engines.iter().map(|e| e.identifier.as_str()).collect();
        let expected_order = vec!["c-engine", "b-engine", "a-engine"];
        assert_eq!(
            actual_order, expected_order,
            "Should sort engines by descending order hint, with the highest order hint appearing first."
        )
    }

    #[test]
    fn test_sort_engines_alphabetically_without_order_hint() {
        let default_engine_id = None;
        let default_private_engine_id = None;
        let mut engines = vec![
            create_engine("c-engine", None),
            create_engine("b-engine", None),
            create_engine("a-engine", None),
        ];
        engines.sort_by(|a, b| {
            sort(
                default_engine_id.as_ref(),
                default_private_engine_id.as_ref(),
                a,
                b,
            )
        });

        let actual_order: Vec<&str> = engines.iter().map(|e| e.identifier.as_str()).collect();
        let expected_order = vec!["a-engine", "b-engine", "c-engine"];
        assert_eq!(
            actual_order, expected_order,
            "Should sort engines alphabetically when there are no order hints."
        )
    }

    #[test]
    fn test_sort_engines_by_order_hint_and_alphabetically() {
        let default_engine_id = None;
        let default_private_engine_id = None;
        let mut engines = vec![
            create_engine("f-engine", None),
            create_engine("e-engine", None),
            create_engine("d-engine", None),
            create_engine("c-engine", Some(4)),
            create_engine("b-engine", Some(5)),
            create_engine("a-engine", Some(6)),
        ];
        engines.sort_by(|a, b| {
            sort(
                default_engine_id.as_ref(),
                default_private_engine_id.as_ref(),
                a,
                b,
            )
        });

        let actual_order: Vec<&str> = engines.iter().map(|e| e.identifier.as_str()).collect();
        let expected_order = vec![
            "a-engine", "b-engine", "c-engine", "d-engine", "e-engine", "f-engine",
        ];
        assert_eq!(
            actual_order, expected_order,
            "Should sort engines by order hint before sorting alphabetically."
        )
    }

    #[test]
    fn test_sort_engines_with_defaults() {
        let default_engine_id = Some("a-engine".to_string());
        let default_private_engine_id = Some("b-engine".to_string());
        let mut engines = vec![
            create_engine("c-engine", Some(3)),
            create_engine("a-engine", Some(1)), // Default engine should be first
            create_engine("b-engine", Some(2)), // Default private engine should be second
        ];
        engines.sort_by(|a, b| {
            sort(
                default_engine_id.as_ref(),
                default_private_engine_id.as_ref(),
                a,
                b,
            )
        });

        let actual_order: Vec<&str> = engines.iter().map(|e| e.identifier.as_str()).collect();
        let expected_order = vec!["a-engine", "b-engine", "c-engine"];
        assert_eq!(
            actual_order, expected_order,
            "Should have sorted the default and private default to have the highest priority."
        )
    }

    #[test]
    fn test_sort_engines_non_ascii_without_order_hint() {
        // TODO: update the comparison to use ICU4X when it's available.
        // See Bug 1945295: https://bugzilla.mozilla.org/show_bug.cgi?id=1945295
    }
}
