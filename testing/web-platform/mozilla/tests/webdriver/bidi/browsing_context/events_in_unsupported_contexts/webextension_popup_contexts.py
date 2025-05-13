import os

import pytest
from tests.support.sync import AsyncPoll
from webdriver.error import TimeoutException

from .. import using_context

pytestmark = pytest.mark.asyncio


@pytest.mark.allow_system_access
@pytest.mark.parametrize(
    "event_name",
    [
        "browsingContext.contextCreated",
        "browsingContext.contextDestroyed",
        "browsingContext.navigationStarted",
        "browsingContext.navigationFailed",
    ],
)
async def test_webextension_popup_context(
    bidi_session,
    current_session,
    top_context,
    subscribe_events,
    event_name,
):
    # Install a webextension with a toolbar button showing a popup.
    path = os.path.join(
        os.path.abspath(os.path.dirname(__file__)),
        "..",
        "support",
        "popup_webextension",
    )
    extension_data = {"type": "path", "path": path}

    web_extension = await bidi_session.web_extension.install(
        extension_data=extension_data
    )

    # Subscribe to events and collect them all in an array.
    await subscribe_events(events=[event_name])
    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(event_name, on_event)

    # Click on the extension toolbar button to trigger potential browsing context events.
    with using_context(current_session, "chrome"):
        # Retrieve the webextension toolbar button.
        button = current_session.find.css(
            "toolbaritem[label=TestPopupExtension]", all=False
        )

        # Show and hide the webextension popup.
        mouse_chain = current_session.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "mouse"}
        )
        mouse_chain.pointer_move(0, 0, origin=button).pointer_down().pointer_up().pause(
            100
        ).pointer_down().pointer_up().perform()

    # Check that no event was emitted.
    wait = AsyncPoll(bidi_session, timeout=0.5)
    with pytest.raises(TimeoutException):
        await wait.until(lambda _: len(events) > 0)

    # Clean up the extension.
    await bidi_session.web_extension.uninstall(extension=web_extension)
    remove_listener()
