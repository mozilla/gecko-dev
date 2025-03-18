import pytest

URL = "https://modules.sms-timing.com/livetiming/?key=Zm9ybXVsYS1udWVybmJlcmc6OTUyN2Q1ODAtZmNkMi00NTZjLWI0YmQtZDY5OGQ5OWVhNWZl&locale=GER"

SUPPORTED_CSS = "#timingTable td"
UNSUPPORTED_CSS = "a[href*=browserchoice]"
NO_RACES_TEXT = "Keine laufenden Rennen"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    races, no_races = client.await_first_element_of(
        [client.css(SUPPORTED_CSS), client.text(NO_RACES_TEXT)], is_displayed=True
    )
    assert races or no_races
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
