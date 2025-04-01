import pytest

URL = "https://nicochannel.jp/engage-kiss/video/smBAc4UhdTUivpbuzBexSa9d"


async def check_for_regression(client, url, shouldPass=True):
    CONSENT = client.css(".MuiDialog-container button.MuiButton-containedPrimary")
    PREMIUM = client.text(
        "視聴するには、会員プランまたはレンタルプランを購入してください"
    )
    BLOCKED = client.text("このブラウザはサポートされていません。")
    PLAY = client.css(".nfcp-overlay-play-lg")

    await client.navigate(url)

    while True:
        consent, premium, blocked, play = client.await_first_element_of(
            [
                CONSENT,
                PREMIUM,
                BLOCKED,
                PLAY,
            ],
            is_displayed=True,
            timeout=30,
        )
        if not consent:
            break
        consent.click()
        client.await_element_hidden(CONSENT)
        continue
    if shouldPass:
        assert play or premium
    else:
        assert blocked


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await check_for_regression(client, URL, shouldPass=True)
