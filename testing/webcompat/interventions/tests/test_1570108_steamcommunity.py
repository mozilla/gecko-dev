import pytest

URL = "https://steamcommunity.com/chat"


USERID_CSS = "[data-featuretarget=login] input[type=text]"
PASSWORD_CSS = "[data-featuretarget=login] input[type=password]"
SIGNIN_CSS = "[data-featuretarget=login] button[type=submit]"
GEAR_CSS = ".friendListButton"
LOGIN_FAIL_TEXT = "Please check your password and account name and try again."
AUTH_CSS = "[data-featuretarget=login] input.Focusable"
AUTH_DIGITS_CSS = "[data-featuretarget=login] input.Focusable"
AUTH_RETRY_TEXT = "Incorrect code, please try again"
AUTH_RETRY_LOADER_CSS = "[data-featuretarget=login] div:has(+ input.Focusable)"
AUTH_RETRY_EXPIRED_TEXT = "Expired"
RATE_TEXT = "Too Many Retries"
VOICE_XPATH = "//*[contains(text(), 'Voice')]"
MIC_BUTTON_CSS = "button.LocalMicTestButton"
UNSUPPORTED_TEXT = "currently unsupported in Firefox"


async def do_2fa(client):
    digits = client.await_css(AUTH_DIGITS_CSS, all=True)
    assert len(digits) > 0

    loader = client.css(AUTH_RETRY_LOADER_CSS)
    if client.find_element(loader):
        client.await_element_hidden(loader)

    for digit in digits:
        if digit.property("value"):
            digit.send_keys("\ue003")  # backspace

    code = input("**** Enter two-factor authentication code: ")
    for i, digit in enumerate(code):
        if len(digits) > i:
            digits[i].send_keys(digit)

    client.await_element(loader, timeout=10)
    client.await_element_hidden(loader)


async def load_mic_test(client, credentials, should_do_2fa):
    await client.navigate(URL)

    async def login():
        userid = client.await_css(USERID_CSS)
        password = client.find_css(PASSWORD_CSS)
        submit = client.find_css(SIGNIN_CSS)
        assert client.is_displayed(userid)
        assert client.is_displayed(password)
        assert client.is_displayed(submit)

        userid.send_keys(credentials["username"])
        password.send_keys(credentials["password"])
        submit.click()

    await login()

    while True:
        auth, retry, gear, fail, rate = client.await_first_element_of(
            [
                client.css(AUTH_CSS),
                client.text(AUTH_RETRY_TEXT),
                client.css(GEAR_CSS),
                client.text(LOGIN_FAIL_TEXT),
                client.text(RATE_TEXT),
            ],
            is_displayed=True,
            timeout=20,
        )

        if retry:
            await do_2fa(client)
            continue

        if rate:
            pytest.skip(
                "Too many Steam login attempts in a short time; try again later."
            )
            return None
        elif auth:
            if should_do_2fa:
                await do_2fa(client)
                continue

            pytest.skip(
                "Two-factor authentication requested; disable Steam Guard"
                " or run this test with --do2fa to live-input codes"
            )
            return None
        elif fail:
            pytest.skip("Invalid login provided.")
            return None
        else:
            break

    assert gear
    gear.click()

    voice = client.await_xpath(VOICE_XPATH, is_displayed=True)
    voice.click()

    mic_test = client.await_css(MIC_BUTTON_CSS, is_displayed=True)
    return mic_test


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, credentials, should_do_2fa):
    mic_test = await load_mic_test(client, credentials, should_do_2fa)
    if not mic_test:
        return

    with client.assert_getUserMedia_called():
        mic_test.click()

    assert not client.find_text(UNSUPPORTED_TEXT)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, credentials, should_do_2fa):
    mic_test = await load_mic_test(client, credentials, should_do_2fa)
    if not mic_test:
        return

    mic_test.click()

    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
