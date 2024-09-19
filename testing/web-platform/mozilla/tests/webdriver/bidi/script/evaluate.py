import asyncio

import pytest
from webdriver.bidi.modules.script import ContextTarget

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("type_hint", ["tab", "window"])
async def test_when_context_created(
    bidi_session, wait_for_event, wait_for_future_safe, type_hint
):
    await bidi_session.session.subscribe(events=["browsingContext.contextCreated"])
    on_context_created = wait_for_event("browsingContext.contextCreated")

    # Start the browsingContext.create command without awaiting on it yet,
    # because browsingContext.create already waits for the initial navigation
    # to be completed.
    task = asyncio.create_task(
        bidi_session.browsing_context.create(type_hint=type_hint)
    )

    context_info = await wait_for_future_safe(on_context_created)

    # Execute a script that will only resolve on the next animation frame, so
    # that the command is likely to resolve when the initial browsing context
    # gets destroyed.
    # Here we expect that the command will automatically be retried the initial
    # attempt was performed on the initial document.
    result = await bidi_session.script.evaluate(
        expression="new Promise(r => window.requestAnimationFrame(() => r('done')))",
        await_promise=True,
        target=ContextTarget(context_info["context"]),
    )

    new_tab = await task

    assert result["value"] == "done"

    await bidi_session.browsing_context.close(context=new_tab["context"])
