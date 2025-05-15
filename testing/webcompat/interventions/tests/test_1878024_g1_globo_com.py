import pytest

URL = "https://g1.globo.com/"
WEBCOMPONENT_CSS = "nova-barra-globocom"


async def are_imgs_squashed(client):
    await client.navigate(URL)

    comp = client.await_css(WEBCOMPONENT_CSS, is_displayed=True)

    # To test this without logging in, we inject a reference PNG (24x24 black
    # square), and then compare it via screenshots to one which we've also placed
    # in an HTML fragment similar to what the page uses.
    [ref, actual] = client.execute_async_script(
        """
        const [comp, done] = arguments;
        const container = comp.shadowRoot.querySelector(".container-right");
        // 24x24 black square png
        const src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAIAAABvFaqvAAAAAXNSR0IB2cksfwAAAARnQU1BAACxjwv8YQUAAAAgY0hSTQAAeiYAAICEAAD6AAAAgOgAAHUwAADqYAAAOpgAABdwnLpRPAAAACJJREFUOMtjVODhZqAGYGKgEhg1aNSgUYNGDRo1aNQg8gEAI0UAZ45NawoAAAAASUVORK5CYII=";
        const div = document.createElement("div");
        div.innerHTML = `
          <div class="container-right">
            <img style="border:none" class="button-login-icon" decoding=sync src="${src}">
          </div>
          <div class="container-right">
            <div class="button-login-icon">
              <img style="border:none" decoding="sync" src="${src}">
            </div>
          </div>
        `;
        container.parentNode.insertBefore(div, container);
        done(div.querySelectorAll("img"));
    """,
        comp,
    )

    return ref.screenshot() != actual.screenshot()


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await are_imgs_squashed(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await are_imgs_squashed(client)
