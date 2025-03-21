/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.testdata

object WebCompatTestData {

    val basicDataJson = """
        {
          "antitracking": {
            "blockList": "basic",
            "btpHasPurgedSite": false,
            "hasMixedActiveContentBlocked": false,
            "hasMixedDisplayContentBlocked": false,
            "hasTrackingContentBlocked": false,
            "isPrivateBrowsing": false
          },
          "browser": {
            "app": {
              "defaultUserAgent": "testDefaultUserAgent"
            },
            "graphics": {
              "devices": [
                { "id": "device1" },
                { "id": "device2" },
                { "id": "device3" }
              ],
              "drivers": [
                { "id": "driver1" },
                { "id": "driver2" },
                { "id": "driver3" }
              ],
              "features": {
                "id": "feature1"
              },
              "hasTouchScreen": true,
              "monitors": [
                { "id": "monitor1" },
                { "id": "monitor2" },
                { "id": "monitor3" }
              ]
            },
            "locales": ["en-CA", "en-US"],
            "platform": {
              "fissionEnabled": false,
              "memoryMB": 1
            },
            "prefs": {
              "browser.opaqueResponseBlocking": false,
              "extensions.InstallTrigger.enabled": false,
              "gfx.webrender.software": false,
              "network.cookie.cookieBehavior": 1,
              "privacy.globalprivacycontrol.enabled": false,
              "privacy.resistFingerprinting": false
            }
          },
          "url": "https://www.mozilla.org",
          "devicePixelRatio": 1.5,
          "frameworks": {
            "fastclick": true,
            "marfeel": true,
            "mobify": true
          },
          "languages": ["en-CA", "en-US"],
          "userAgent": "testUserAgent"
        }
    """.trimIndent()

    val extraDataJson = """
        {
          "antitracking": {
            "blockList": "basic",
            "btpHasPurgedSite": false,
            "hasMixedActiveContentBlocked": false,
            "hasMixedDisplayContentBlocked": false,
            "hasTrackingContentBlocked": false,
            "isPrivateBrowsing": false
          },
          "browser": {
            "app": {
              "defaultUserAgent": "testDefaultUserAgent"
            },
            "graphics": {
              "devices": [
                { "id": "device1" },
                { "id": "device2" },
                { "id": "device3" }
              ],
              "drivers": [
                { "id": "driver1" },
                { "id": "driver2" },
                { "id": "driver3" }
              ],
              "features": {
                "id": "feature1"
              },
              "hasTouchScreen": true,
              "monitors": [
                { "id": "monitor1" },
                { "id": "monitor2" },
                { "id": "monitor3" }
              ]
            },
            "locales": ["en-CA", "en-US"],
            "platform": {
              "fissionEnabled": false,
              "memoryMB": 1
            },
            "prefs": {
              "browser.opaqueResponseBlocking": false,
              "extensions.InstallTrigger.enabled": false,
              "gfx.webrender.software": false,
              "network.cookie.cookieBehavior": 1,
              "privacy.globalprivacycontrol.enabled": false,
              "privacy.resistFingerprinting": false
            }
          },
          "url": "https://www.mozilla.org",
          "devicePixelRatio": 1.5,
          "frameworks": {
            "fastclick": true,
            "marfeel": true,
            "mobify": true
          },
          "languages": ["en-CA", "en-US"],
          "userAgent": "testUserAgent",
          "irrelevantData": "irrelevantData"
        }
    """.trimIndent()

    val missingDataJson = """
        {
          "devicePixelRatio": "1.5"
        }
    """.trimIndent()
}
