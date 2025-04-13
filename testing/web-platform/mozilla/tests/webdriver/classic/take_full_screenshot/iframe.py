import pytest
from tests.support.asserts import assert_success
from tests.support.image import png_dimensions

from . import document_dimensions

DEFAULT_CSS_STYLE = """
    <style>
      div, iframe {
        display: block;
        border: 1px solid blue;
        width: 10em;
        height: 10em;
      }
    </style>
"""

DEFAULT_CONTENT = "<div>Lorem ipsum dolor sit amet.</div>"


def take_full_screenshot(session):
    return session.transport.send(
        "GET",
        f"/session/{session.session_id}/moz/screenshot/full",
    )


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
def test_source_origin(session, url, domain, inline, iframe):
    session.url = inline(f"""{DEFAULT_CSS_STYLE}{DEFAULT_CONTENT}""")

    response = take_full_screenshot(session)
    reference_screenshot = assert_success(response)
    assert png_dimensions(reference_screenshot) == document_dimensions(session)

    iframe_content = f"<style>body {{ margin: 0; }}</style>{DEFAULT_CONTENT}"
    session.url = inline(
        f"""{DEFAULT_CSS_STYLE}{iframe(iframe_content, domain=domain)}"""
    )

    response = take_full_screenshot(session)
    screenshot = assert_success(response)
    assert png_dimensions(screenshot) == document_dimensions(session)

    assert screenshot == reference_screenshot
