import pytest

URL = "https://www.aliexpress.com/ssr/300000512/nn2024update?spm=a2g0n.home.3fornn.2.50c276dbeE1R1C&disableNav=YES&pha_manifest=ssr&_immersiveMode=true&productIds=1005006422500160&browser_id=b0b0e7b79568430794a11f7d8cab49c8&aff_trace_key=null&aff_platform=msite&m_page_id=drilnynmcabwbkwi194dba601458b8fe2231f53dc9&gclid="

FIRST_ITEM_IMAGE_CSS = ".swiper .swiper-slide-active .AIC-MI-img.square"


async def are_any_product_images_too_big(client):
    await client.navigate(URL, wait="none")
    item = client.await_css(FIRST_ITEM_IMAGE_CSS, is_displayed=True)
    return client.execute_script(
        """
        return arguments[0].getBoundingClientRect().width > screen.width / 2;
    """,
        item,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await are_any_product_images_too_big(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await are_any_product_images_too_big(client)
