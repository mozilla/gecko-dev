import pytest
import pytest_asyncio
from webdriver.bidi.modules.input import Actions
from webdriver.bidi.modules.script import ContextTarget

pytestmark = pytest.mark.asyncio


@pytest_asyncio.fixture
async def setup_beforeunload_page(url):
    async def setup_beforeunload_page(bidi_session, context):
        page_url = url("/webdriver/tests/support/html/beforeunload.html")
        await bidi_session.browsing_context.navigate(
            context=context["context"], url=page_url, wait="complete"
        )

        # Focus the input
        await bidi_session.script.evaluate(
            expression="""
                const input = document.querySelector("input");
                input.focus();
            """,
            target=ContextTarget(context["context"]),
            await_promise=False,
        )

        actions = Actions()
        actions.add_key().send_keys("foo")
        await bidi_session.input.perform_actions(
            actions=actions, context=context["context"]
        )

        return page_url

    return setup_beforeunload_page


async def test_close_browser(new_session, add_browser_capabilities):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )

    await bidi_session.browser.close()

    # Wait for the browser to actually close.
    bidi_session.current_browser.wait()

    assert bidi_session.current_browser.is_running is False


async def test_close_all_tabs_without_beforeunload_prompt(
    new_session,
    add_browser_capabilities,
    setup_beforeunload_page,
):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )

    new_tab = await bidi_session.browsing_context.create(type_hint="tab")
    new_window = await bidi_session.browsing_context.create(type_hint="window")

    await setup_beforeunload_page(bidi_session, new_tab)
    await setup_beforeunload_page(bidi_session, new_window)

    await bidi_session.browser.close()

    # Wait for the browser to actually close.
    bidi_session.current_browser.wait()

    assert bidi_session.current_browser.is_running is False


async def test_start_session_again(new_session, add_browser_capabilities):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )
    first_session_id = bidi_session.session_id

    await bidi_session.browser.close()

    # Wait for the browser to actually close.
    bidi_session.current_browser.wait()

    # Try to create a session again.
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )

    assert isinstance(bidi_session.session_id, str)
    assert first_session_id != bidi_session.session_id
