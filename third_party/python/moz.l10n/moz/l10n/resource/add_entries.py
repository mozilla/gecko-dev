# Copyright Mozilla Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import annotations

from dataclasses import replace
from typing import Any

from . import data as res

RS = res.Section[Any, Any]
RE = res.Entry[Any, Any]


def add_entries(
    target: res.Resource[res.V, res.M],
    source: res.Resource[res.V, res.M],
    *,
    use_source_entries: bool = False,
) -> int:
    """
    Modifies `target` by adding entries from `source` that are not already present in `target`.
    Standalone comments are not added.
    If `use_source_entries` is set,
    entries from `source` override those in `target` when they differ,
    as well as updating section comments and metadata from `source`.

    Entries are not copied, so further changes will be reflected in both resources.

    Returns a count of added or changed entries and sections.
    """

    change_count = 0
    cur_tgt_section: RS | None = None
    for src_section in source.sections:
        tgt_match = [s for s in target.sections if s.id == src_section.id]
        prev_pos: tuple[RS, int] | None = None
        new_entries: list[RE | res.Comment] = []
        for entry in src_section.entries:
            if isinstance(entry, res.Entry):
                target_pos = next(
                    (
                        (s, i)
                        for s in tgt_match
                        for i, e in enumerate(s.entries)
                        if isinstance(e, res.Entry) and e.id == entry.id
                    ),
                    None,
                )
                sc = src_section.comment
                sm = src_section.meta
                if target_pos:
                    cur_tgt_section, idx = target_pos
                    if use_source_entries:
                        if cur_tgt_section.entries[idx] != entry:
                            cur_tgt_section.entries[idx] = entry
                            change_count += 1
                        if cur_tgt_section.comment != sc or cur_tgt_section.meta != sm:
                            cur_tgt_section.comment = sc
                            cur_tgt_section.meta = sm
                            change_count += 1
                    prev_pos = target_pos
                else:
                    # Entry has no section-id + entry-id match in target,
                    # so needs to be added.
                    change_count += 1
                    if prev_pos and prev_pos[0].comment == sc:
                        # The preceding entry did have a match in a section
                        # exactly matching this one, so we can add an entry there.
                        idx = prev_pos[1] + 1
                        prev_pos[0].entries.insert(idx, entry)
                        prev_pos = (prev_pos[0], idx)
                    else:
                        ts = next((s for s in tgt_match if s.comment == sc), None)
                        if ts:
                            # An exactly matching section exists in target,
                            # so add this entry there.
                            ts.entries.append(entry)
                            prev_pos = (ts, len(ts.entries) - 1)
                        else:
                            # A new section needs to be added for this entry.
                            new_entries.append(entry)
                            prev_pos = None
        if new_entries:
            idx = target.sections.index(cur_tgt_section) + 1 if cur_tgt_section else 0
            cur_tgt_section = replace(src_section, entries=new_entries)
            target.sections.insert(idx, cur_tgt_section)
    return change_count
