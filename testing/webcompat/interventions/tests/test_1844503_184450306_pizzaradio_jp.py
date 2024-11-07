import pytest

URL = "https://pizzaradio.jp/video/sma4EXkDVriHP3cRx2xRdm2D"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.test_nicochannel_like_site(URL, shouldPass=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.test_nicochannel_like_site(URL, shouldPass=False)
