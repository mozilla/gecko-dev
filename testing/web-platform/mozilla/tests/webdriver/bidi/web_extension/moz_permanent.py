import pytest
from support.addons import is_addon_temporary_installed
from tests.bidi.web_extension import assert_extension_id
from webdriver.bidi import error

pytestmark = pytest.mark.asyncio


@pytest.mark.allow_system_access
@pytest.mark.parametrize(
    "permanent", [None, False, True], ids=["default", "temporary", "permanent"]
)
@pytest.mark.parametrize("mode", ["archivePath", "base64", "path"])
async def test_install_with_permanent(
    bidi_session, current_session, extension_data, mode, permanent
):
    data = {"type": mode}
    if mode == "base64":
        data.update({"value": extension_data[mode]})
    else:
        data.update({"path": extension_data[mode]})

    kwargs = {"moz:permanent": permanent} if permanent is not None else {}

    if mode == "path" and permanent:
        # Only signed webextensions in XPI format can be installed permanently
        with pytest.raises(error.InvalidWebExtensionException):
            await bidi_session.web_extension.install(
                extension_data=data,
                **kwargs,
            )
        return

    web_extension = await bidi_session.web_extension.install(
        extension_data=data,
        **kwargs,
    )

    try:
        assert_extension_id(web_extension, extension_data)
        assert is_addon_temporary_installed(current_session, web_extension) is not bool(
            permanent
        )
    finally:
        # Clean up the extension.
        await bidi_session.web_extension.uninstall(extension=web_extension)
