import time

import pytest
from webdriver.error import NoSuchElementException, UnexpectedAlertOpenException

URL = "https://www.eyebuydirect.ca/"

FIRST_PRODUCT_CSS = "[class*='product-list-item_product'].event-product-click a"
FIRST_FRAME_SIZE_CSS = "button[class*='size-list_size']"
SELECTED_FRAME_SIZE_CSS = "button[class*='size-list_size'][class*='size-list_current']"
SELECT_LENSES_CSS = "[aria-label='Select Lenses']"
NON_PRESCRIPTION_CSS = ".use-for-non-rx"
CLEAR_CSS = "#color-type-clear"
STANDARD_LENSES_CSS = ".package-show.package-standard.lenses"
ADD_TO_CART_CSS = "button.btn-add-cart"
PAYPAL_CHECKOUT_CSS = "#paypal-checkout-button"
PAYPAL_OVERLAY_CSS = "[id^='paypal-overlay-uid']"


async def can_click_paypal_button(client):
    await client.navigate(URL)
    client.soft_click(client.await_css(FIRST_PRODUCT_CSS))

    # we must wait for the DOM listeners to be added before the size selector will
    # react, so just keep clicking and checking a few times until it reacts (or fail).
    tries = 0
    while True:
        tries += 1
        client.click(
            client.await_css(FIRST_FRAME_SIZE_CSS, is_displayed=True), force=True
        )
        try:
            client.await_css(SELECTED_FRAME_SIZE_CSS, is_displayed=True, timeout=1)
            break
        except NoSuchElementException as e:
            if tries > 5:
                raise e

    client.click(client.await_css(SELECT_LENSES_CSS, is_displayed=True), force=True)
    client.soft_click(client.await_css(NON_PRESCRIPTION_CSS))
    client.soft_click(client.await_css(CLEAR_CSS))
    client.soft_click(client.await_css(STANDARD_LENSES_CSS))
    client.soft_click(client.await_css(ADD_TO_CART_CSS))

    # Now the tricky parts begin. When we click on the PayPal button, if things
    # are working then the PayPal iframe will receive the click event. But in
    # the broken case, the underlying top frame will receive it instead. As such,
    # we check which frame receives a mousedown. But in order to do so, we need
    # to use chrome js functions, as WebDriver currently does not send mouse
    # events via APZ. And to simplify the task of detection which frame received
    # the event, we simply alert from whatever frame received it, catch the
    # 'unexpected' alert, and read what was alerted. On top of that, we need to
    # wait for the frame to finish loading, and the only reliable way I've found
    # to do that is just to switch to the frame's context and wait for a hero
    # element to appear in the DOM, and then add our mousedown listener.
    frame = client.await_css(f"{PAYPAL_CHECKOUT_CSS} iframe")
    client.execute_script(
        """
        document.documentElement.addEventListener("mousedown", e => {
            if (e.target.nodeName !== "IFRAME") {
                alert("Clicked on the wrong node: " + e.target.outerHTML);
            }
            alert("top");
        }, true);
    """
    )

    while True:
        client.switch_to_frame(frame)
        buttons = client.await_css("#buttons-container")
        client.execute_script(
            """
            arguments[0].addEventListener("mousedown", () => {
                alert("frame");
            }, true);
        """,
            buttons,
        )
        break

    client.switch_to_frame()

    clicks = 10
    try:
        for i in range(clicks):
            await client.apz_click(frame, no_up=True)
            time.sleep(0.5)
    except UnexpectedAlertOpenException as e:
        s = str(e)
        if "wrong node" in s:
            raise e
        return "frame" in s

    raise ValueError(f"no alert opened after {clicks} clicks")


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_click_paypal_button(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_click_paypal_button(client)
