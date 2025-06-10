import pytest
from support.addons import is_addon_temporary_installed
from support.helpers import clear_pref, set_pref
from tests.bidi.web_extension import assert_extension_id
from webdriver.bidi import error

pytestmark = pytest.mark.asyncio


@pytest.mark.allow_system_access
@pytest.mark.parametrize(
    "permanent", [None, False, True], ids=["default", "temporary", "permanent"]
)
@pytest.mark.parametrize("mode", ["archivePath", "base64", "path"])
@pytest.mark.parametrize("signed", [True, False], ids=["signed", "unsigned"])
async def test_install_with_permanent(
    bidi_session, current_session, extension_data, mode, permanent, signed
):
    if mode == "path" and signed:
        # Unpacked extensions are not signed
        return
    data = {"type": mode}
    unsigned_tag = "" if signed or mode == "path" else "Unsigned"
    extension_data_value = extension_data[f"{mode}{unsigned_tag}"]
    if mode == "base64":
        data.update({"value": extension_data_value})
    else:
        data.update({"path": extension_data_value})

    kwargs = {"moz:permanent": permanent} if permanent is not None else {}

    if permanent and not signed:
        try:
            with pytest.raises(error.InvalidWebExtensionException):
                set_pref(current_session, "xpinstall.signatures.required", True)
                await bidi_session.web_extension.install(
                    extension_data=data,
                    **kwargs,
                )
        finally:
            clear_pref(current_session, "xpinstall.signatures.required")
        return

    try:
        set_pref(current_session, "xpinstall.signatures.required", True)
        web_extension = await bidi_session.web_extension.install(
            extension_data=data,
            **kwargs,
        )

        assert_extension_id(web_extension, extension_data)
        assert is_addon_temporary_installed(current_session, web_extension) is not bool(
            permanent
        )
    finally:
        # Clean up the extension.
        clear_pref(current_session, "xpinstall.signatures.required")
        await bidi_session.web_extension.uninstall(extension=web_extension)
