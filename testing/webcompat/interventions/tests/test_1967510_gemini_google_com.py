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


async def check_addons_work(client):
    # This is a regression test for the issue described in
    # https://bugzilla.mozilla.org/show_bug.cgi?id=1967510#c25.
    client.install_addon(
        {
            "manifest.json": """{
                "manifest_version": 2,
                "name": "Pasting Test",
                "version": "1.0",
                "content_scripts": [
                  {
                    "matches": ["*://gemini.google.com/*"],
                    "js": ["test.js"]
                  }
                ]
              }
              """,
            "test.js": """
                document.addEventListener("paste", e => {
                  const clipboardInit = cloneInto({}, window);
                  clipboardInit.clipboardData = new DataTransfer();
                  const newEvent = new wrappedJSObject.ClipboardEvent(e.type, clipboardInit);
                  try {
                    newEvent.clipboardData.setData("text/plain", e.clipboardData.getData("text/plain"));
                    console.error("Test result: success");
                  } catch (_) {
                    console.error("Test result: failed");
                  }
                });
                """,
        }
    )
    await client.navigate(URL)
    client.set_clipboard("test")
    client.await_css(ADD_PROMPT_CSS, is_displayed=True).click()
    promise = await client.promise_console_message_listener("Test result:")
    client.do_paste()
    msg = (await promise)["args"][0]["value"]
    assert msg == "Test result: success"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_paste_works(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_addons_works(client):
    await check_addons_work(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_paste_works(client)
