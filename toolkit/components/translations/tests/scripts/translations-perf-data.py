# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
description:
  A CLI tool to extract perftest metadata from a Translations HTML test file.

example:
  ❯ python3 toolkit/components/translations/tests/scripts/translations-perf-data.py \\
    --page_path="toolkit/components/translations/tests/browser/translations-bencher-es.html" \\
    --model_path="~/Downloads/cab5e093-7b55-47ea-a247-9747cc0109e3.spm"

note:
  The vocab model file can be downloaded from the following page:
  https://gregtatum.github.io/taskcluster-tools/src/models/
"""

import argparse
import sys
from pathlib import Path

import sentencepiece as spm
from bs4 import BeautifulSoup
from icu import BreakIterator, Locale


class CustomArgumentParser(argparse.ArgumentParser):
    """Custom argument parser to display help on errors."""

    def error(self, message):
        """Override error to display help message."""
        print(f"\nerror: {message}\n", file=sys.stderr)
        self.print_help()
        sys.exit(2)


def parse_arguments() -> argparse.Namespace:
    """Parse CLI arguments."""
    parser = CustomArgumentParser(
        description=__doc__,  # Use the module's docstring as the description
        formatter_class=argparse.RawDescriptionHelpFormatter,  # Use custom formatter
    )
    parser.add_argument(
        "--page_path",
        required=True,
        type=Path,
        help="The HTML test file from which to extract perftest metadata.",
    )
    parser.add_argument(
        "--model_path",
        required=True,
        type=Path,
        help="The SentencePiece vocab model file for the test page's language.",
    )
    return parser


def extract_page_language(html_path: Path) -> str:
    """Extract the lang attribute from the HTML file."""
    with html_path.open("r", encoding="utf-8") as file:
        soup = BeautifulSoup(file, "html.parser")
    lang = soup.find("html").get("lang")

    if not lang:
        raise ValueError(f"Language not specified in the HTML file at {html_path}.")
    return lang


def extract_body_text(page_language: str, html_path: Path) -> str:
    """Extract text content from the <body> element of an HTML file,
    ignoring sub-elements with a lang attribute not matching page language."""
    with html_path.open("r", encoding="utf-8") as file:
        soup = BeautifulSoup(file, "html.parser")

    body = soup.find("body")
    if body is None:
        raise ValueError(f"No <body> element found in the HTML file at {html_path}.")

    # Find all elements with a `lang` attribute that does not match source_lang_tag
    for element in body.find_all(attrs={"lang": True}):
        if element["lang"] != page_language:
            element.decompose()  # Remove the element and its children

    return body.get_text()


def is_word_like(segment: str) -> bool:
    """Determine if a segment is word-like."""
    segment = segment.strip()

    if not segment:
        # A word-like segment should not be only whitespace.
        return False

    # A word-like segment should not be only punctuation.
    return any(char.isalnum() for char in segment)


def count_words(text: str, language: str) -> int:
    """Count the words in text using ICU BreakIterator."""
    locale = Locale(language)
    break_iterator = BreakIterator.createWordInstance(locale)
    break_iterator.setText(text)

    word_count = 0
    lhs_boundary = break_iterator.first()
    rhs_boundary = break_iterator.nextBoundary()

    while rhs_boundary != BreakIterator.DONE:
        if is_word_like(text[lhs_boundary:rhs_boundary]):
            word_count += 1

        lhs_boundary = rhs_boundary
        rhs_boundary = break_iterator.nextBoundary()

    return word_count


def count_tokens(text: str, model_path: Path) -> int:
    """Count the tokens in the text using SentencePiece."""
    processor = spm.SentencePieceProcessor(model_file=str(model_path))
    return len(processor.encode(text))


def main() -> None:
    parser = parse_arguments()
    args = parser.parse_args()

    args.page_path = args.page_path.expanduser()
    args.model_path = args.model_path.expanduser()

    page_language = extract_page_language(args.page_path)
    body_text = extract_body_text(page_language, args.page_path)

    token_count = count_tokens(body_text, args.model_path)
    word_count = count_words(body_text, page_language)

    print()
    print(f'pageLanguage: "{page_language}",')
    print(f"tokenCount: {token_count},")
    print(f"wordCount: {word_count},")

    print("\n⏩ NEXT STEPS ⏩\n")
    print(
        "These metadata should be added to the TranslationsBencher static #PAGE_DATA located in:\n"
    )
    print("browser/components/translations/tests/browser/head.js")
    print()


if __name__ == "__main__":
    main()
