import time

import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.eyebuydirect.ca/sunglasses"

POPUP_CSS = ".ebd-modal-root"
GOOGLE_LOGIN_CSS = "#credential_picker_container"
FIRST_PRODUCT_CSS = "a[href^='https://www.eyebuydirect.ca/sunglasses/frames/']"
FIRST_FRAME_SIZE_CSS = "button[class*='size-list_size']"
SELECTED_FRAME_SIZE_CSS = "button[class*='size-list_size'][class*='size-list_current']"
SELECT_LENSES_CSS = "[aria-label='Select Lenses']"
NON_PRESCRIPTION_CSS = ".use-for-non-rx"
FIRST_COLOR_TYPE_CSS = "#color-type-container [data-option-name]"
BASIC_TYPE_CSS = "#sunglasses, #sunglasses-non-rx"
FIRST_COLOR_OPT_CSS = "[data-color-value-id]"
FIRST_COLOR_OPT_SELECTED_CSS = "[data-color-value-id].current"
CONFIRM_COLOR_BTN_CSS = "button.color-confirm"
STANDARD_LENSES_CSS = ".package-show.package-standard.lenses"
ADD_TO_CART_CSS = "button.btn-add-cart"
PAYPAL_CHECKOUT_CSS = "#paypal-checkout-button"
PAYPAL_OVERLAY_CSS = "[id^='paypal-overlay-uid']"

NON_PRESCRIPTION_OPT_CSS = (
    "[class*=lens-prescription_select-lens] [aria-label^=Non-Prescription]"
)
NON_PRESCRIPTION_OPT_SELECTED_CSS = (
    "[aria-label^=Non-Prescription][class^=lens-prescription_selected]"
)


async def can_click_paypal_button(client):
    await client.make_preload_script(
        f"""
           document.addEventListener("DOMContentLoaded", () => {{
             const s = document.createElement("style");
             s.textContent = "{POPUP_CSS}, {GOOGLE_LOGIN_CSS} {{ opacity:0 !important; pointer-events:none !important; }}";
             document.head.appendChild(s);
           }});
        """
    )

    await client.navigate(URL)
    client.soft_click(client.await_css(FIRST_PRODUCT_CSS))

    # we must wait for the DOM listeners to be added before the selectors will
    # react, so just keep clicking and checking a few times until they do.
    for tries in range(10):
        non_rx = client.await_css(NON_PRESCRIPTION_OPT_CSS, is_displayed=True)
        if client.find_css(NON_PRESCRIPTION_OPT_SELECTED_CSS, is_displayed=True):
            break
        non_rx.click()
        time.sleep(0.5)

    try:
        first_frame_size = client.await_css(
            FIRST_FRAME_SIZE_CSS, is_displayed=True, timeout=4
        )
        if first_frame_size:
            for tries in range(10):
                first_frame_size.click()
                time.sleep(0.5)
                if client.find_css(SELECTED_FRAME_SIZE_CSS, is_displayed=True):
                    break
    except NoSuchElementException:
        pass

    client.click(client.await_css(SELECT_LENSES_CSS, is_displayed=True), force=True)
    client.soft_click(client.await_css(FIRST_COLOR_TYPE_CSS, is_displayed=True))
    client.soft_click(client.await_css(BASIC_TYPE_CSS, is_displayed=True))

    first_color_opt = client.await_css(FIRST_COLOR_OPT_CSS, is_displayed=True)
    for tries in range(10):
        if client.find_css(FIRST_COLOR_OPT_SELECTED_CSS, is_displayed=True):
            break
        first_color_opt.click()
        time.sleep(0.5)

    client.soft_click(client.await_css(CONFIRM_COLOR_BTN_CSS, is_displayed=True))
    client.soft_click(client.await_css(STANDARD_LENSES_CSS, is_displayed=True))
    client.soft_click(client.await_css(ADD_TO_CART_CSS, is_displayed=True))

    # Now the tricky parts begin. When we click on the PayPal button, if things
    # are working then the PayPal iframe will receive the click event. But in
    # the broken case, the underlying top frame will receive it instead. As such,
    # we check which frame receives a mousedown. But in order to do so, we need
    # to use chrome js functions, as WebDriver currently does not send mouse
    # events via APZ. And we have to click a few times, as it's possible that the
    # first clicks will still target the top frame, despite simulating APZ clicks.
    frame = client.await_css(f"{PAYPAL_CHECKOUT_CSS} iframe")

    while True:
        client.switch_to_frame(frame)
        buttons = client.await_css("#buttons-container", timeout=20)
        client.execute_script(
            """
            arguments[0].addEventListener("mousedown", e => {
                window.__clicked = true;
            }, true);
        """,
            buttons,
        )
        break

    client.switch_to_frame()

    # Now we mousedown over the PayPal button. Note that we only send mousedown
    # events here to limit the chances that we will trigger any reaction from
    # the page aside from our own mousedown-detection listener above. We try
    # to mousedown multiple times as it does not always work the first time
    # for some unknown reason (and may still fail outright intermittently).
    for i in range(10):
        await client.apz_down(element=frame)
        time.sleep(0.2)

    client.switch_to_frame(frame)
    return client.execute_script("return !!window.__clicked")


@pytest.mark.skip_platforms("android")
@pytest.mark.only_firefox_versions(max=134)
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_click_paypal_button(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.only_firefox_versions(max=134)
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_click_paypal_button(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.only_firefox_versions(min=135)
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_regression(client):
    assert await can_click_paypal_button(client)
