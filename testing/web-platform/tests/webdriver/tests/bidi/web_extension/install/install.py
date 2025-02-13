import pytest
from tests.support.helpers import get_addon_path, get_base64_for_addon_file


EXTENSION_ID = "{d3e7c1f1-2e35-4a49-89fe-9f46eb8abf0a}"


@pytest.mark.asyncio
async def test_install_from_base64(bidi_session):
    web_extension = await bidi_session.web_extension.install(
        extensionData={
            "type": "base64",
            "value": get_base64_for_addon_file("webextension-unsigned.xpi")
        }
    )
    try:
        assert web_extension == EXTENSION_ID
    finally:
        # Clean up the addon.
        await bidi_session.web_extension.uninstall(extension=web_extension)


@pytest.mark.asyncio
async def test_install_from_path(bidi_session):
    web_extension = await bidi_session.web_extension.install(
        extensionData={
            "type": "path",
            "path": get_addon_path("unpacked")
        }
    )
    try:
        assert web_extension == EXTENSION_ID
    finally:
        # Clean up the addon.
        await bidi_session.web_extension.uninstall(extension=web_extension)


@pytest.mark.asyncio
async def test_install_from_archive_path(bidi_session):
    web_extension = await bidi_session.web_extension.install(
        extensionData={
            "type": "archivePath",
            "path": get_addon_path("webextension-unsigned.xpi")
        }
    )
    try:
        assert web_extension == EXTENSION_ID
    finally:
        # Clean up the addon.
        await bidi_session.web_extension.uninstall(extension=web_extension)
