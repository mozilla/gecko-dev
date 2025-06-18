import pytest

URL = "https://gemini.google.com/"

ADD_PROMPT_CSS = ".ql-editor.textarea.new-input-ui"
SEND_CSS = "[data-mat-icon-name=send]"
SIGNED_OUT_TEXT = "been signed out"
RESPONSE_CSS = "message-content"
EDIT_OLD_PROMPT_CSS = "[data-mat-icon-name=edit]"
OLD_PROMPT_CSS = "textarea[id^=mat-input-]"
CANCEL_CSS = "button.cancel-button"


async def check_paste_works(client):
    client.set_clipboard("")
    await client.navigate(URL)
    client.await_css(ADD_PROMPT_CSS, is_displayed=True).send_keys("hello")
    client.await_css(SEND_CSS, is_displayed=True).click()
    signed_out, _ = client.await_first_element_of(
        [client.text(SIGNED_OUT_TEXT), client.css(RESPONSE_CSS)], is_displayed=True
    )
    if signed_out:
        pytest.skip("Blocked from accessing site. Please try testing manually.")
        return
    client.soft_click(client.await_css(EDIT_OLD_PROMPT_CSS))
    client.await_css(OLD_PROMPT_CSS, is_displayed=True).click()
    client.execute_script("document.execCommand('selectAll')")
    client.execute_script("document.execCommand('copy')")
    client.await_css(CANCEL_CSS, is_displayed=True).click()
    prompt = client.await_css(ADD_PROMPT_CSS, is_displayed=True)
    await client.apz_click(element=prompt, offset=[40, 20])
    client.do_paste()
    return client.execute_script(
        """
        return arguments[0].innerText.trim() === "hello"
      """,
        prompt,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_paste_works(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_paste_works(client)
