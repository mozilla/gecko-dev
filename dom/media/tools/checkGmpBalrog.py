# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import hashlib
import re
from datetime import datetime
from urllib.parse import urlparse
from xml.etree import ElementTree

import requests


def check_hash(plugin, url, valid_urls):
    if url in valid_urls:
        return
    valid_urls[url] = True

    response = requests.get(url)
    response.raise_for_status()

    if "hashValue" in plugin.attrib:
        hashValue = hashlib.sha512(response.content).hexdigest()
        if hashValue != plugin.attrib["hashValue"]:
            raise Exception(
                "Given hash {} and calculated hash {} differ",
                plugin.attrib["hashValue"],
                hashValue,
            )
    if "size" in plugin.attrib:
        size = len(response.content)
        if size != int(plugin.attrib["size"]):
            raise Exception(
                "Given size {} and calculated size {} differ",
                int(plugin.attrib["size"]),
                size,
            )


def fetch_balrog_xml(
    url_base: str, plugin_id, version: str, buildid: str, channels, targets, checkHash
) -> str:
    url = "{url_base}/{version}/{buildid}/{target}/en-US/{channel}/default/default/default/update.xml"
    valid_urls = {}
    results = {}
    for channel in channels:
        results[channel] = {}
        for target in targets:
            balrog_url = url.format_map(
                {
                    "url_base": url_base,
                    "buildid": buildid,
                    "channel": channel,
                    "version": version,
                    "target": target,
                }
            )

            response = requests.get(balrog_url)
            response.raise_for_status()

            plugin_urls = []
            tree = ElementTree.fromstring(response.content)
            for plugin in tree.findall("./addons/addon"):
                if not "id" in plugin.attrib:
                    continue
                if plugin.attrib["id"] != plugin_id:
                    continue
                if "URL" in plugin.attrib:
                    if checkHash:
                        check_hash(plugin, plugin.attrib["URL"], valid_urls)
                    plugin_urls.append(plugin.attrib["URL"])
                for mirror in plugin.findall("./mirror"):
                    if "URL" in mirror.attrib:
                        if checkHash:
                            check_hash(plugin, plugin.attrib["URL"], valid_urls)
                        plugin_urls.append(mirror.attrib["URL"])

            results[channel][target] = plugin_urls

    matching_channels = {}
    for channel in channels:
        matching_channels[channel] = [channel]
        for other_channel in channels:
            if (
                channel == other_channel
                or channel not in results
                or other_channel not in results
            ):
                continue
            if results[channel] == results[other_channel]:
                matching_channels[channel].append(other_channel)
                del results[other_channel]

    for channel in results:
        print(", ".join(matching_channels[channel]))
        for target in targets:
            print("\t{}".format(target))
            for url in results[channel][target]:
                print("\t\t{}".format(url))


def main():
    examples = """examples:
  python dom/media/tools/checkGmpBalrog.py widevine 133.0
  python dom/media/tools/checkGmpBalrog.py widevine 133.0 --target Darwin_aarch64-gcc3 Darwin_x86_64-gcc3
  python dom/media/tools/checkGmpBalrog.py --url http://localhost:8080 openh264 125.0
  python dom/media/tools/checkGmpBalrog.py widevine_l1 115.14.0 --staging --channel nightly beta"""

    parser = argparse.ArgumentParser(
        description="Check Balrog XML for GMP plugin updates",
        epilog=examples,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "plugin",
        help="which plugin: openh264, widevine, widevine_l1",
    )
    parser.add_argument("version", help="version of Firefox")
    parser.add_argument(
        "--channel", action="extend", nargs="+", help="check specific channel(s)"
    )
    parser.add_argument(
        "--list-channels", action="store_true", help="list the supported channels"
    )
    parser.add_argument(
        "--target", action="extend", nargs="+", help="check specific target(s)"
    )
    parser.add_argument(
        "--list-targets", action="store_true", help="list the supported targets"
    )
    parser.add_argument("--buildid", help="override generated build ID to be specific")
    parser.add_argument(
        "--url", help="override base URL from which to fetch the balrog configuration"
    )
    parser.add_argument(
        "--staging",
        action="store_true",
        help="using the balrog staging URL instead of production",
    )
    parser.add_argument(
        "--checkHash",
        action="store_true",
        help="download plugins and validate the size/hash",
    )
    args = parser.parse_args()

    valid_channels = ["esr", "release", "beta", "nightly", "nightlytest"]
    if args.list_channels:
        for channel in valid_channels:
            print(channel)
        return
    if args.channel is not None:
        for channel in args.channel:
            if channel not in valid_channels:
                parser.error("`%s` is invalid, see --list-channels" % channel)
                return
        channels = args.channel
    else:
        channels = valid_channels

    valid_targets = [
        "Darwin_aarch64-gcc3",
        "Darwin_x86_64-gcc3",
        "Linux_aarch64-gcc3",
        "Linux_x86-gcc3",
        "Linux_x86_64-gcc3",
        "WINNT_aarch64-msvc-aarch64",
        "WINNT_x86-msvc",
        "WINNT_x86_64-msvc",
    ]
    valid_aliases = [
        "Linux_x86_64-gcc3-asan",
        "WINNT_x86-msvc-x64",
        "WINNT_x86-msvc-x86",
        "WINNT_x86_64-msvc-x64",
        "WINNT_x86_64-msvc-x64-asan",
    ]
    if args.list_targets:
        for target in valid_targets:
            print(target)
        for target in valid_aliases:
            print("%s (alias)" % target)
        return
    if args.target is not None:
        for target in args.target:
            if target not in valid_targets and target not in valid_aliases:
                parser.error("`%s` is invalid, see --list-targets" % target)
                return
        targets = args.target
    else:
        targets = valid_targets

    if args.buildid is not None:
        if not re.match(r"^\d{14}$", args.buildid):
            parser.error("`%s` is invalid, build id must be 14 digits")
            return
        buildid = args.buildid

    else:
        buildid = datetime.today().strftime("%y%m%d%H%M%S")

    url_base = "https://aus5.mozilla.org"
    if args.staging:
        url_base = "https://stage.balrog.nonprod.cloudops.mozgcp.net"
    if args.url is not None:
        url_base = args.url
    if url_base[-1] == "/":
        url_base = url_base[:-1]
    url_base += "/update/3/GMP"

    parsed_url = urlparse(url_base)
    if parsed_url.scheme not in ("http", "https"):
        parser.error("expected http(s) scheme, got `%s`" % parsed_url.scheme)
        return
    if parsed_url.path != "/update/3/GMP":
        parser.error("expected url path of `/update/3/GMP`, got `%s`" % parsed_url.path)
        return

    if args.plugin == "openh264":
        plugin = "gmp-gmpopenh264"
    elif args.plugin == "widevine":
        plugin = "gmp-widevinecdm"
    elif args.plugin == "widevine_l1":
        plugin = "gmp-widevinecdm-l1"
    else:
        parser.error("plugin not recognized")
        return

    if not re.match(r"^\d+\.\d+(\.\d+)?$", args.version):
        parser.error("version must be of the form ###.###(.###)")
        return

    fetch_balrog_xml(
        url_base, plugin, args.version, buildid, channels, targets, args.checkHash
    )


main()
