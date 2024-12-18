import asyncio

import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.tiktok.com/"

LOGIN_BUTTON1_CSS = "#header-login-button"
USE_PW_OPT1_TEXT = "Use phone / email / username"
USE_PW_OPT2_CSS = "a[href='/login/phone-or-email/email']"
USER_CSS = "#loginContainer input[name=username]"
PASS_CSS = "#loginContainer input[type=password]"
LOGIN_USER = "testuser1"
LOGIN_PASS = "asdfadsf"
LOGIN_BUTTON2_CSS = "#loginContainer [type=submit]"
CAPTCHA_CONTAINER_CSS = ".TUXModal"
CAPTCHA_ROTATING_IMG_CSS = ".TUXModal img"
CAPTCHA_LOADING_SPINNER_CSS = ".TUXLoadingSpinner-container"
DRAGGABLE_CSS = ".TUXModal [draggable=true]"


async def check_captcha_slider_moves(client, in_headless_mode):
    if in_headless_mode:
        # Unfortunately, our hacky method of dragging via APZ with
        # sendNativeMouseEvent does not seem to work in headless mode.
        # Although mouse events are being sent, they appear to not
        # target the same elements, and no drag events are ever fired.
        pytest.xfail("This test does not work in headless mode")
        return

    await client.navigate(URL)

    client.soft_click(client.await_css(LOGIN_BUTTON1_CSS, is_displayed=True))
    client.soft_click(client.await_text(USE_PW_OPT1_TEXT, is_displayed=True))
    client.soft_click(client.await_css(USE_PW_OPT2_CSS, is_displayed=True))
    client.await_css(USER_CSS, is_displayed=True).send_keys(LOGIN_USER)
    client.await_css(PASS_CSS, is_displayed=True).send_keys(LOGIN_PASS)
    client.soft_click(client.await_css(LOGIN_BUTTON2_CSS, is_displayed=True))

    # We first need to confirm that we got the right Captcha, as TikTok will
    # sometimes show a different one, or just not show one at all. We could
    # feasibly re-run the test in a new private window over and over until
    # the right one appears, but for now we just ask the user to try again.
    try:
        rotating_img = client.await_css(CAPTCHA_ROTATING_IMG_CSS)
        captcha_container = client.await_css(CAPTCHA_CONTAINER_CSS)
    except NoSuchElementException:
        pytest.xfail("The wrong type of Captcha was received; please try again")
        return False

    # When things work, the Captcha image rotates while we drag the slider.
    # In either case the Captcha will show a loading spinner a few moments after
    # we mouse-up, while checking if the images line up. So we can use mutation
    # observers to check which case happens first, and if the image rotated
    # before the loading indicator appeared, we know things worked as expected.
    client.execute_script(
        """
        const [rotatingImg, captchaContainer, loadingSpinnerMatch] = arguments;
        const origRot = rotatingImg.style.transform;
        window.__checkSliderWorks = new Promise(resolve => {
            new MutationObserver(changes => {
                for (const {addedNodes} of changes) {
                    for (const node of Array.from(addedNodes)) {
                        if (node.querySelector(loadingSpinnerMatch)) {
                            return resolve(false);
                        }
                    }
                }
            }).observe(captchaContainer, {
                childList: true,
                subtree: true,
            });
            new MutationObserver(changes => {
                for (const {target, type} of changes) {
                    if (origRot !== target.style.transform) {
                        return resolve(true);
                    }
                }
            }).observe(rotatingImg, {
                attributes: true,
                attributeFilter: ["style"],
            });
        });
    """,
        rotating_img,
        captcha_container,
        CAPTCHA_LOADING_SPINNER_CSS,
    )

    draggable = client.await_css(DRAGGABLE_CSS, is_displayed=True)

    coords = await client.apz_down(element=draggable)
    await asyncio.sleep(0.1)
    coords = await client.apz_move(coords=(coords[0] + 5, coords[1]))
    await asyncio.sleep(0.1)
    coords = await client.apz_move(coords=(coords[0] + 5, coords[1]))
    await asyncio.sleep(0.1)
    await client.apz_move(coords=(coords[0] + 5, coords[1]))

    # Finally, we wait for the MutationObservers to process the results.
    return client.execute_async_script("window.__checkSliderWorks.then(arguments[0])")


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, in_headless_mode):
    assert await check_captcha_slider_moves(client, in_headless_mode)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, in_headless_mode):
    assert not await check_captcha_slider_moves(client, in_headless_mode)
