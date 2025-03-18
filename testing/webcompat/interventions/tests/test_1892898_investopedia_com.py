import pytest

URL = "https://www.investopedia.com/banking-4427754"

CARD_CSS = ".card__content[data-tag=Banking]"


async def are_cards_too_tall(client):
    await client.navigate(URL)
    card = client.await_css(CARD_CSS, is_displayed=True)
    return client.execute_script(
        """
      return arguments[0].getBoundingClientRect().height > window.innerHeight;
    """,
        card,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await are_cards_too_tall(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await are_cards_too_tall(client)
