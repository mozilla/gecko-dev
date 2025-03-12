import asyncio

import pytest

URL = "https://shamir.com/my/lenses_and_more/shamir-autograph-intelligence/"

HERO_TEXT = "Continuously evolving with you"


async def does_text_appear(client):
    await client.navigate(URL)
    hero = client.execute_script(
        "return arguments[0].closest('[data-aos^=fade]')", client.await_text(HERO_TEXT)
    )
    await asyncio.sleep(1)
    hero.click()
    await asyncio.sleep(2)
    return client.is_displayed(hero)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await does_text_appear(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await does_text_appear(client)
