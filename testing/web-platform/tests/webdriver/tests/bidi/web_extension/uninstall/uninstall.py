import pytest
import webdriver.bidi.error as error
from tests.support.helpers import get_base64_for_addon_file


@pytest.mark.asyncio
async def test_uninstall(bidi_session):
    web_extension = await bidi_session.web_extension.install(
        extensionData={
            "type": "base64",
            "value": get_base64_for_addon_file("webextension-unsigned.xpi")
        }
    )
    await bidi_session.web_extension.uninstall(
        extension=web_extension
    )
    # proof that the uninstall was successful
    with pytest.raises(error.NoSuchWebExtensionException):
        await bidi_session.web_extension.uninstall(
            extension=web_extension
        )
