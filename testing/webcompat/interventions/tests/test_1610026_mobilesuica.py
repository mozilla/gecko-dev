import pytest

ADDRESS_CSS = "input[name=MailAddress]"
PASSWORD_CSS = "input[name=Password]"
CLOSE_BUTTON_CSS = "input[name=winclosebutton]"
UNAVAILABLE_TEXT = "時間をお確かめの上、再度実行してください。"
UNSUPPORTED_TEXT = "ご利用のブラウザでは正しく"


async def verify_site_loads(client):
    await client.navigate("https://www.mobilesuica.com/")

    error1, error2, site_is_down, address = client.await_first_element_of(
        [
            client.css(CLOSE_BUTTON_CSS),
            client.text(UNSUPPORTED_TEXT),
            client.text(UNAVAILABLE_TEXT),
            client.css(ADDRESS_CSS),
        ],
        is_displayed=True,
        timeout=10,
    )

    # The page can be down at certain times, making testing impossible. For instance:
    # "モバイルSuicaサービスが可能な時間は4:00～翌日2:00です。
    #  時間をお確かめの上、再度実行してください。"
    # "Mobile Suica service is available from 4:00 to 2:00 the next day.
    #  Please check the time and try again."
    if site_is_down:
        pytest.xfail("Site is currently down")
        return False

    if error1 or error2:
        return False

    return address and client.await_css(PASSWORD_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await verify_site_loads(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await verify_site_loads(client)
