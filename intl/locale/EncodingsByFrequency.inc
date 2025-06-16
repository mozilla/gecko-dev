/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// List of encodings and their corresponding Lang atoms for font selection.
// nullptr indicates that the browser UI locale language will be used.

// These are in roughly-estimated frequency order sometimes by group.
// The main thing is that UTF-8 is first and windows-1252 is second,
// followed by other encodings that were a legacy default somewhere.
    _(UTF_8_ENCODING,          nullptr)
    _(WINDOWS_1252_ENCODING,   nsGkAtoms::x_western)
    _(GBK_ENCODING,            nsGkAtoms::zh_cn)
    _(SHIFT_JIS_ENCODING,      nsGkAtoms::Japanese)
    _(BIG5_ENCODING,           nsGkAtoms::zh_tw)
    _(EUC_KR_ENCODING,         nsGkAtoms::ko)
    _(WINDOWS_1250_ENCODING,   nsGkAtoms::x_western)
    _(WINDOWS_1251_ENCODING,   nsGkAtoms::x_cyrillic)
    _(WINDOWS_1253_ENCODING,   nsGkAtoms::el)
    _(WINDOWS_1254_ENCODING,   nsGkAtoms::x_western)
    _(WINDOWS_1255_ENCODING,   nsGkAtoms::he)
    _(WINDOWS_1256_ENCODING,   nsGkAtoms::ar)
    _(WINDOWS_1257_ENCODING,   nsGkAtoms::x_western)
    _(WINDOWS_1258_ENCODING,   nsGkAtoms::x_western)
    _(WINDOWS_874_ENCODING,    nsGkAtoms::th)
    _(ISO_8859_2_ENCODING,     nsGkAtoms::x_western)
    _(EUC_JP_ENCODING,         nsGkAtoms::Japanese)
    _(GB18030_ENCODING,        nsGkAtoms::zh_cn)
    _(UTF_16BE_ENCODING,       nullptr)
    _(UTF_16LE_ENCODING,       nullptr)
    _(ISO_2022_JP_ENCODING,    nsGkAtoms::Japanese)
    _(ISO_8859_3_ENCODING,     nsGkAtoms::x_western)
    _(ISO_8859_4_ENCODING,     nsGkAtoms::x_western)
    _(ISO_8859_5_ENCODING,     nsGkAtoms::x_cyrillic)
    _(ISO_8859_6_ENCODING,     nsGkAtoms::ar)
    _(ISO_8859_7_ENCODING,     nsGkAtoms::el)
    _(ISO_8859_8_ENCODING,     nsGkAtoms::he)
    _(ISO_8859_8_I_ENCODING,   nsGkAtoms::he)
    _(ISO_8859_10_ENCODING,    nsGkAtoms::x_western)
    _(ISO_8859_13_ENCODING,    nsGkAtoms::x_western)
    _(ISO_8859_14_ENCODING,    nsGkAtoms::x_western)
    _(ISO_8859_15_ENCODING,    nsGkAtoms::x_western)
    _(ISO_8859_16_ENCODING,    nsGkAtoms::x_western)
    _(KOI8_R_ENCODING,         nsGkAtoms::x_cyrillic)
    _(KOI8_U_ENCODING,         nsGkAtoms::x_cyrillic)
    _(MACINTOSH_ENCODING,      nsGkAtoms::x_western)
    _(REPLACEMENT_ENCODING,    nullptr)
    _(IBM866_ENCODING,         nsGkAtoms::x_cyrillic)
    _(X_MAC_CYRILLIC_ENCODING, nsGkAtoms::x_cyrillic)
    _(X_USER_DEFINED_ENCODING, nullptr)
