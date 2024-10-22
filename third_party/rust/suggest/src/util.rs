/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use crate::Result;

/// Given a list of keywords for a suggestion, returns a phrase that best
/// completes the user's query. This function uses two heuristics to pick the
/// best match:
///
/// 1. Find the first keyword in the list that has at least one more word than
///    the query, then trim the keyword up to the end of that word.
/// 2. If there isn't a keyword with more words, pick the keyword that forms the
///    longest suffix of the query. This might be the query itself.
pub fn full_keyword(query: &str, keywords: &[impl AsRef<str>]) -> String {
    let query_words_len = query.split_whitespace().count();
    let min_phrase_words_len = if query.ends_with(char::is_whitespace) {
        // If the query ends with a space, find a keyword with at least one more
        // word, so that the completed phrase can show a word after the space.
        query_words_len + 1
    } else {
        query_words_len
    };
    keywords
        .iter()
        .map(AsRef::as_ref)
        .filter(|phrase| phrase.starts_with(query))
        .map(|phrase| phrase.split_whitespace().collect::<Vec<_>>())
        .find(|phrase_words| phrase_words.len() > min_phrase_words_len)
        .map(|phrase_words| phrase_words[..min_phrase_words_len].join(" "))
        .unwrap_or_else(|| {
            keywords
                .iter()
                .map(AsRef::as_ref)
                .filter(|phrase| phrase.starts_with(query) && query.len() < phrase.len())
                .max_by_key(|phrase| phrase.trim().len())
                .unwrap_or(query)
                .to_owned()
        })
}

/// Performs a depth-first traversal over all possible chunk sequences in a
/// string, applies a filter-map function to each chunk in each sequence, and
/// collects the filter-mapped sequences in a `Vec`. A "chunk" is a slice of one
/// or more consecutive words in the string such that the slices do not overlap.
/// It's analogous to the concept of slice chunks described in [1] where the
/// elements in this case are words in a string.
///
/// IMPORTANT: This function potentially does an exponential amount of work! You
/// should always be careful to prune the traversal space by returning `None`
/// from your mappper function, as described further below, when a chunk does
/// not match what you are searching for.
///
/// `max_chunk_size` controls the maximum chunk size (in number of words), which
/// influences the branching factor at each step in the traversal.
///
/// At each traversal step, the filter-map function is passed the chunk at that
/// step and the chunk's word-based index in the `query` string. The function
/// can map the chunk to one or more values. Each value expands the branching
/// factor at the current step by `max_chunk_size`. In other words, the
/// branching factor at a given traversal step is `max_chunk_size` multiplied by
/// the number of values returned by the filter-map function at that step. The
/// traversed path of mapped values at that step is also passed to the
/// filter-map function. Each path is a sequence of chunks in the original
/// `query` string except the chunks have been replaced by mapped values from
/// the filter-map function.
///
/// The filter-map function can return `None` to halt traversal at the current
/// step. Returning `None` sets the branching factor at that step to zero,
/// pruning the subtree rooted at that step from the traversal space and
/// discarding the path from the output. This is important for keeping traversal
/// reasonably bounded.
///
/// Traversal ends and the function returns when all paths have been visited.
/// The returned `Vec` will contain all traversal paths that weren't pruned.
///
/// [1] https://doc.rust-lang.org/std/vec/struct.Vec.html#method.chunks
///
/// # Examples
///
/// Mapping chunks in "a b c" to uppercase, up to a max chunk size of `3`:
///
/// ```
/// # use suggest::util::filter_map_chunks;
/// let paths = filter_map_chunks("a b c", 3, |chunk, _, _| {
///     Ok(Some(vec![chunk.to_uppercase()]))
/// });
/// assert_eq!(paths.unwrap(), vec![
///     vec!["A", "B", "C"],
///     vec!["A", "B C"],
///     vec!["A B", "C"],
///     vec!["A B C"]
/// ]);
/// ```
///
/// Same as previous but using `chunk_index` in the filter-map function to prune
/// paths that don't start with `"a"`:
///
/// ```
/// # use suggest::util::filter_map_chunks;
/// let paths = filter_map_chunks("a b c", 3, |chunk, chunk_index, _| {
///     if chunk_index > 0 || chunk == "a" {
///         Ok(Some(vec![chunk.to_uppercase()]))
///     } else {
///         Ok(None)
///     }
/// });
/// assert_eq!(paths.unwrap(), vec![
///     vec!["A", "B", "C"],
///     vec!["A", "B C"],
/// ]);
/// ```
///
/// Same as the first example but using `path` in the filter-map function to
/// prune paths that include "A B":
///
/// ```
/// # use suggest::util::filter_map_chunks;
/// let paths = filter_map_chunks("a b c", 3, |chunk, _, path| {
///     if path.iter().any(|value| value == "A B") {
///         Ok(None)
///     } else {
///         Ok(Some(vec![chunk.to_uppercase()]))
///     }
/// });
/// assert_eq!(paths.unwrap(), vec![
///     vec!["A", "B", "C"],
///     vec!["A", "B C"],
///     vec!["A B C"],
/// ]);
/// ```
///
/// Mapping each chunk to multiple values:
///
/// ```
/// # use suggest::util::filter_map_chunks;
/// let paths = filter_map_chunks("a b c", 3, |chunk, _, _| {
///     Ok(Some(vec![format!("{chunk}0"), format!("{chunk}1")]))
/// });
/// assert_eq!(paths.unwrap(), vec![
///     vec!["a0", "b0", "c0"],
///     vec!["a0", "b0", "c1"],
///     vec!["a0", "b1", "c0"],
///     vec!["a0", "b1", "c1"],
///     vec!["a0", "b c0"],
///     vec!["a0", "b c1"],
///     vec!["a1", "b0", "c0"],
///     vec!["a1", "b0", "c1"],
///     vec!["a1", "b1", "c0"],
///     vec!["a1", "b1", "c1"],
///     vec!["a1", "b c0"],
///     vec!["a1", "b c1"],
///     vec!["a b0", "c0"],
///     vec!["a b0", "c1"],
///     vec!["a b1", "c0"],
///     vec!["a b1", "c1"],
///     vec!["a b c0"],
///     vec!["a b c1"]
/// ]);
/// ```
pub fn filter_map_chunks<T: Clone>(
    query: &str,
    max_chunk_size: usize,
    f: impl Fn(&str, usize, &[T]) -> Result<Option<Vec<T>>>,
) -> Result<Vec<Vec<T>>> {
    let words: Vec<_> = query.split_whitespace().collect();
    let normalized_query = words.join(" ");
    filter_map_chunks_recurse(
        &words,
        &normalized_query,
        &mut vec![],
        0,
        max_chunk_size,
        &f,
    )
}

/// `remaining_words` is the slice of remaining words in the query string at
/// this step. `remaining_query` is the remaining slice of the normalized query
/// string at this step.
///
/// `path` is the sequence of values returned by the filter-map function so far
/// at this step.
///
/// `chunk_index` is the word-based index in the query string at this step.
fn filter_map_chunks_recurse<T: Clone>(
    remaining_words: &[&str],
    remaining_query: &str,
    path: &mut Vec<T>,
    chunk_index: usize,
    max_chunk_size: usize,
    f: &impl Fn(&str, usize, &[T]) -> Result<Option<Vec<T>>>,
) -> Result<Vec<Vec<T>>> {
    // Filtered-in (non-pruned) paths that will be returned from this step of
    // the traversal.
    let mut this_step_paths: Vec<Vec<T>> = vec![];

    for chunk_size in 1..=max_chunk_size {
        if remaining_words.len() < chunk_size {
            // `chunk_size` and the later chunk sizes in this for-loop are too
            // big to visit the remaining words. We already visited them earlier
            // in the loop when the chunk size was small enough.
            break;
        }

        // Get the current chunk within the remaining query. Its char length is
        // the sum of the lengths of the words in the chunk + `chunk_size - 1`
        // spaces between the words. There will only be one space between each
        // word in `remaining_query` because `remaining_query` is normalized.
        let chunk_char_len = remaining_words[..chunk_size]
            .iter()
            .fold(chunk_size - 1, |memo, w| memo + w.len());
        let chunk = &remaining_query[..chunk_char_len];

        // Call the mapper function.
        if let Some(mapped_values) = f(chunk, chunk_index, &path[..])? {
            for value in mapped_values {
                if chunk_size == remaining_words.len() {
                    // This is the final chunk in the path. Stop recursing.
                    this_step_paths.push(vec![value.clone()]);
                } else {
                    // Recurse. Note that the new `remaining_words` slice won't
                    // be empty because if it were, `chunk_size` would equal the
                    // remaining word count, which is if-branch condition.
                    path.push(value.clone());
                    let subtree_paths = filter_map_chunks_recurse(
                        &remaining_words[chunk_size..],
                        &remaining_query[(chunk_char_len + 1)..],
                        path,
                        chunk_index + chunk_size,
                        max_chunk_size,
                        f,
                    )?;
                    path.pop();
                    for mut p in subtree_paths {
                        p.insert(0, value.clone());
                        this_step_paths.push(p);
                    }
                }
            }
        }
    }

    Ok(this_step_paths)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keywords_with_more_words() {
        assert_eq!(
            full_keyword(
                "moz",
                &[
                    "moz",
                    "mozi",
                    "mozil",
                    "mozill",
                    "mozilla",
                    "mozilla firefox"
                ]
            ),
            "mozilla".to_owned(),
        );
        assert_eq!(
            full_keyword(
                "mozilla",
                &[
                    "moz",
                    "mozi",
                    "mozil",
                    "mozill",
                    "mozilla",
                    "mozilla firefox"
                ]
            ),
            "mozilla".to_owned(),
        );
    }

    #[test]
    fn keywords_with_longer_phrase() {
        assert_eq!(
            full_keyword("moz", &["moz", "mozi", "mozil", "mozill", "mozilla"]),
            "mozilla".to_owned()
        );
        assert_eq!(
            full_keyword(
                "mozilla f",
                &["moz", "mozi", "mozil", "mozill", "mozilla firefox"]
            ),
            "mozilla firefox".to_owned()
        );
    }

    #[test]
    fn query_ends_with_space() {
        assert_eq!(
            full_keyword(
                "mozilla ",
                &["moz", "mozi", "mozil", "mozill", "mozilla firefox"]
            ),
            "mozilla firefox".to_owned()
        );
    }

    fn check_paths(actual: Vec<Vec<(String, usize)>>, expected: Vec<Vec<(&str, usize)>>) {
        assert_eq!(
            actual,
            expected
                .into_iter()
                .map(|p| p
                    .into_iter()
                    .map(|(w, i)| (w.to_string(), i))
                    .collect::<Vec<_>>())
                .collect::<Vec<Vec<_>>>()
        );
    }

    #[test]
    fn filter_map_chunks_1() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 1, |chunk, chunk_index, _| {
            Ok(Some(vec![(chunk.to_string(), chunk_index)]))
        })?;
        check_paths(
            paths,
            vec![vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)]],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| {
            Ok(Some(vec![(chunk.to_string(), chunk_index)]))
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| {
            Ok(Some(vec![(chunk.to_string(), chunk_index)]))
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| {
            Ok(Some(vec![(chunk.to_string(), chunk_index)]))
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_5() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 5, |chunk, chunk_index, _| {
            Ok(Some(vec![(chunk.to_string(), chunk_index)]))
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
                vec![("a b c d e", 0)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_1_map_many() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c", 1, |chunk, _, _| {
            Ok(Some((0..3).map(|i| format!("{chunk}{i}")).collect()))
        })?;
        assert_eq!(
            paths,
            vec![
                vec!["a0", "b0", "c0"],
                vec!["a0", "b0", "c1"],
                vec!["a0", "b0", "c2"],
                vec!["a0", "b1", "c0"],
                vec!["a0", "b1", "c1"],
                vec!["a0", "b1", "c2"],
                vec!["a0", "b2", "c0"],
                vec!["a0", "b2", "c1"],
                vec!["a0", "b2", "c2"],
                vec!["a1", "b0", "c0"],
                vec!["a1", "b0", "c1"],
                vec!["a1", "b0", "c2"],
                vec!["a1", "b1", "c0"],
                vec!["a1", "b1", "c1"],
                vec!["a1", "b1", "c2"],
                vec!["a1", "b2", "c0"],
                vec!["a1", "b2", "c1"],
                vec!["a1", "b2", "c2"],
                vec!["a2", "b0", "c0"],
                vec!["a2", "b0", "c1"],
                vec!["a2", "b0", "c2"],
                vec!["a2", "b1", "c0"],
                vec!["a2", "b1", "c1"],
                vec!["a2", "b1", "c2"],
                vec!["a2", "b2", "c0"],
                vec!["a2", "b2", "c1"],
                vec!["a2", "b2", "c2"]
            ]
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_map_many() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c", 2, |chunk, _, _| {
            Ok(Some((0..3).map(|i| format!("{chunk}{i}")).collect()))
        })?;
        assert_eq!(
            paths,
            vec![
                vec!["a0", "b0", "c0"],
                vec!["a0", "b0", "c1"],
                vec!["a0", "b0", "c2"],
                vec!["a0", "b1", "c0"],
                vec!["a0", "b1", "c1"],
                vec!["a0", "b1", "c2"],
                vec!["a0", "b2", "c0"],
                vec!["a0", "b2", "c1"],
                vec!["a0", "b2", "c2"],
                vec!["a0", "b c0"],
                vec!["a0", "b c1"],
                vec!["a0", "b c2"],
                vec!["a1", "b0", "c0"],
                vec!["a1", "b0", "c1"],
                vec!["a1", "b0", "c2"],
                vec!["a1", "b1", "c0"],
                vec!["a1", "b1", "c1"],
                vec!["a1", "b1", "c2"],
                vec!["a1", "b2", "c0"],
                vec!["a1", "b2", "c1"],
                vec!["a1", "b2", "c2"],
                vec!["a1", "b c0"],
                vec!["a1", "b c1"],
                vec!["a1", "b c2"],
                vec!["a2", "b0", "c0"],
                vec!["a2", "b0", "c1"],
                vec!["a2", "b0", "c2"],
                vec!["a2", "b1", "c0"],
                vec!["a2", "b1", "c1"],
                vec!["a2", "b1", "c2"],
                vec!["a2", "b2", "c0"],
                vec!["a2", "b2", "c1"],
                vec!["a2", "b2", "c2"],
                vec!["a2", "b c0"],
                vec!["a2", "b c1"],
                vec!["a2", "b c2"],
                vec!["a b0", "c0"],
                vec!["a b0", "c1"],
                vec!["a b0", "c2"],
                vec!["a b1", "c0"],
                vec!["a b1", "c1"],
                vec!["a b1", "c2"],
                vec!["a b2", "c0"],
                vec!["a b2", "c1"],
                vec!["a b2", "c2"]
            ]
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_1_prune_a() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 1, |chunk, chunk_index, _| match chunk {
            "a" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(paths, vec![]);
        Ok(())
    }

    #[test]
    fn filter_map_chunks_1_prune_b() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 1, |chunk, chunk_index, _| match chunk {
            "b" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(paths, vec![]);
        Ok(())
    }

    #[test]
    fn filter_map_chunks_1_prune_c() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 1, |chunk, chunk_index, _| match chunk {
            "c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(paths, vec![]);
        Ok(())
    }

    #[test]
    fn filter_map_chunks_1_prune_d() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 1, |chunk, chunk_index, _| match chunk {
            "d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(paths, vec![]);
        Ok(())
    }

    #[test]
    fn filter_map_chunks_1_prune_e() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 1, |chunk, chunk_index, _| match chunk {
            "e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(paths, vec![]);
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_a() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "a" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_b() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "b" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_c() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_d() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_e() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_ab() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "a b" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_bc() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "b c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_cd() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "c d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_de() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_a_bc() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "a" | "b c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_a_cd() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "a" | "c d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_bc_cd() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "b c" | "c d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_2_prune_bc_de() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 2, |chunk, chunk_index, _| match chunk {
            "b c" | "d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_a() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "a" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_b() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "b" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_c() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_d() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_e() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_ab() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "a b" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_bc() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "b c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_cd() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "c d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_de() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_abc() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "a b c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_bcd() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "b c d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_cde() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "c d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_3_prune_a_bc_cde() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 3, |chunk, chunk_index, _| match chunk {
            "a" | "b c" | "c d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_a() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "a" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_b() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "b" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_c() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_d() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_e() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_ab() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "a b" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_bc() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "b c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_cd() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "c d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_de() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_abc() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "a b c" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_bcd() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "b c d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_cde() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "c d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_abcd() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "a b c d" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a", 0), ("b c d e", 1)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_bcde() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "b c d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c d e", 2)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a", 0), ("b c d", 1), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_a_bc_de() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "a" | "b c" | "d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b", 0), ("c d e", 2)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_4_prune_a_bc_cde() -> anyhow::Result<()> {
        let paths = filter_map_chunks("a b c d e", 4, |chunk, chunk_index, _| match chunk {
            "a" | "b c" | "c d e" => Ok(None),
            _ => Ok(Some(vec![(chunk.to_string(), chunk_index)])),
        })?;
        check_paths(
            paths,
            vec![
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
                vec![("a b c", 0), ("d", 3), ("e", 4)],
                vec![("a b c", 0), ("d e", 3)],
                vec![("a b c d", 0), ("e", 4)],
            ],
        );
        Ok(())
    }

    #[test]
    fn filter_map_chunks_spaces() -> anyhow::Result<()> {
        let paths = filter_map_chunks("   a   b  c        d  e ", 2, |chunk, chunk_index, _| {
            Ok(Some(vec![(chunk.to_string(), chunk_index)]))
        })?;
        check_paths(
            paths,
            vec![
                vec![("a", 0), ("b", 1), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b", 1), ("c", 2), ("d e", 3)],
                vec![("a", 0), ("b", 1), ("c d", 2), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d", 3), ("e", 4)],
                vec![("a", 0), ("b c", 1), ("d e", 3)],
                vec![("a b", 0), ("c", 2), ("d", 3), ("e", 4)],
                vec![("a b", 0), ("c", 2), ("d e", 3)],
                vec![("a b", 0), ("c d", 2), ("e", 4)],
            ],
        );
        Ok(())
    }
}
