# Copyright Mozilla Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import annotations

from re import fullmatch

ANDROID_LOCALES = {"he": "iw", "id": "in", "yi": "ji"}


# https://android.googlesource.com/platform/tools/base/+/master/sdk-common/src/main/java/com/android/ide/common/resources/configuration/LocaleQualifier.java#28
def get_android_locale(locale: str) -> str:
    lc, *rest = locale.split("-")
    if lc in ANDROID_LOCALES:
        lc = ANDROID_LOCALES[lc]
    if not rest:
        return lc
    if len(rest) == 1 and fullmatch(r"[A-Z]{2}", rest[0]):
        return f"{lc}-r{rest[0]}"
    return f"b+{lc}+{'+'.join(rest)}"


def parse_android_locale(alocale: str) -> str | None:
    if alocale.startswith("b+"):
        lc, *rest = alocale[2:].split("+")
    else:
        m = fullmatch(r"([a-z]{2,3})(?:-r([A-Z]{2}))?", alocale)
        if m is None:
            return None  # Not a valid Android locale
        lc = m[1]
        rest = [m[2]] if m[2] else []
    for current, legacy in ANDROID_LOCALES.items():
        if lc == legacy:
            lc = current
            break
    return "-".join((lc, *rest))
