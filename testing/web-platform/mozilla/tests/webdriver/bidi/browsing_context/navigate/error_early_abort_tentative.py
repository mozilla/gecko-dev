import pytest
from webdriver.bidi.error import UnknownErrorException

pytestmark = pytest.mark.asyncio


async def test_early_abort_via_inline_js_redirect(bidi_session, new_tab, url, inline):
    url_redirect = url("/webdriver/tests/bidi/browsing_context/support/empty.html")
    page = inline(
        f"""
        <script type="text/javascript">
            window.location.href = "{url_redirect}";
        </script>
    """
    )

    with pytest.raises(UnknownErrorException) as error:
        await bidi_session.browsing_context.navigate(
            context=new_tab["context"], url=page, wait="complete"
        )

    assert "Navigation was aborted by another navigation" in str(error.value)


async def test_early_abort_via_separate_js_redirect(bidi_session, new_tab, url, inline):
    url_redirect = url("/webdriver/tests/bidi/browsing_context/support/empty.html")
    script_url = inline(f"""window.location.href = "{url_redirect}";""", doctype="js")
    page = inline(f"""<script type="text/javascript" src="{script_url}"></script>""")

    with pytest.raises(UnknownErrorException) as error:
        await bidi_session.browsing_context.navigate(
            context=new_tab["context"], url=page, wait="complete"
        )

    assert "Navigation was aborted by another navigation" in str(error.value)
