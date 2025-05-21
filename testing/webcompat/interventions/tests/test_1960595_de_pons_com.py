import asyncio

import pytest

URL = "https://de.pons.com/%C3%BCbersetzung-2/englisch-deutsch/panacea"

HERO_TEXT = "Feedback"
UNSUPPORTED_TEXT = "Bitte verwende einen alternativen Browser"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_text(HERO_TEXT, is_displayed=True)
    await asyncio.sleep(1)
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(HERO_TEXT, is_displayed=True)
    await asyncio.sleep(1)
    assert client.find_text(UNSUPPORTED_TEXT, is_displayed=True)
