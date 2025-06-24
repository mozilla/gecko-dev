import pytest

URL = "https://bapenda.jabarprov.go.id/infopkb/"
IFRAME_CSS = "iframe[src*='sambara-v2.bapenda.jabarprov.go.id']"
INPUT_CSS = "#root > main.cointainer input[type=number][placeholder=XXXX]"


async def get_results(client):
    await client.navigate(URL, wait="none")
    # the site can be very slow
    frame = client.await_css(IFRAME_CSS, timeout=180)
    height = client.execute_script("return arguments[0].clientHeight", frame)
    client.switch_to_frame(frame)
    input = client.await_css(INPUT_CSS, is_displayed=True)
    appearance = client.execute_script(
        "return getComputedStyle(arguments[0]).appearance", input
    )
    return [height, appearance]


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    [frame_height, input_appearance] = await get_results(client)
    assert 150 < frame_height
    assert "textfield" == input_appearance


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    [frame_height, input_appearance] = await get_results(client)
    assert 150 == frame_height
    assert "textfield" != input_appearance
