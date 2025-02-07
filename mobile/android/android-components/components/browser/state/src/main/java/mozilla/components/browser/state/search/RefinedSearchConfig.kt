/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.search

/**
 * A data class representing all the search engine definitions available for the current app.
 * For more information: https://github.com/mozilla/application-services/pull/6381/files#top
 *
 * @property engines the list of the [SearchEngineDefinition].
 * @property appDefaultEngineId the ID of the default search engine.
 * @property appDefaultPrivateEngineId the ID of the default private search engine.
 */
data class RefinedSearchConfig(
    val engines: List<SearchEngineDefinition>,
    val appDefaultEngineId: String,
    val appDefaultPrivateEngineId: String?,
)

/**
 * A data class representing a search engine.
 *
 * @property aliases the list of aliases for the search engine.
 * @property classification the [SearchEngineClassification] for the search engine.
 * @property identifier the ID of the search engine.
 * @property name the name of the search engine.
 * @property partnerCode the partner code of the search engine, if any.
 * @property telemetrySuffix the identifier for telemetry collection, if any.
 * @property urls the [SearchEngineUrls] for the search engine.
 * @property orderHint the
 */
data class SearchEngineDefinition(
    val aliases: List<String>,
    val classification: SearchEngineClassification,
    val identifier: String,
    val name: String,
    val partnerCode: String? = null,
    val telemetrySuffix: String? = null,
    val urls: SearchEngineUrls,

    // A hint to the order that this engine should be in the engine list. This
    // is derived from the `engineOrders` section of the search configuration.
    // The higher the number, the nearer to the front it should be.
    // If the number is not specified, other methods of sorting may be relied
    // upon (e.g. alphabetical).
    val orderHint: UByte? = null,
)

/**
 * A enum class representing a search engine classification.
 *
 * @property value the enum value in [Int] form.
 * @property stringValue the enum value in [String] form.
 */
enum class SearchEngineClassification(val value: Int, val stringValue: String) {
    UNKNOWN(1, "unknown"),
    GENERAL(2, "general"), ;

    /**
     * Contains the methods to get enum value.
     */
    companion object {
        /**
         * Get [SearchEngineClassification] enum from Int value.
         *
         * @param value [Int] value of the [SearchEngineClassification]
         */
        fun fromInt(value: Int): SearchEngineClassification? =
            entries.firstOrNull { it.value == value }

        /**
         * Get [SearchEngineClassification] enum from String value.
         *
         * @param value [String] value of the [SearchEngineClassification]
         */
        fun fromString(value: String): SearchEngineClassification? =
            entries.firstOrNull { it.stringValue == value }
    }
}

/**
 * A data class for holding [SearchEngineUrl]s for a search engine.
 *
 * @property search the [SearchEngineUrl] for the search engine.
 * @property suggestions the [SearchEngineUrl] for the suggested searches for the search engine, if any.
 * @property trending the [SearchEngineUrl] for the trending searchs for the search engine, if any.
 */
data class SearchEngineUrls(
    val search: SearchEngineUrl,
    val suggestions: SearchEngineUrl? = null,
    val trending: SearchEngineUrl? = null,
)

/**
 * A data class representing the components of search engine url construction.
 *
 * @property base the base [String] of the search engine URL
 * @property method the method [String] of the search engine URL
 * @property params the list of [SearchUrlParam] for the search engine URL
 * @property searchTermParamName the name of the search term parameter, if any.
 */
data class SearchEngineUrl(
    val base: String,
    val method: String,
    val params: List<SearchUrlParam>,
    val searchTermParamName: String? = null,
)

/**
 * A data class representing the configuration components of a search engine parameter for the url.
 *
 * @property name [String] name of the param.
 * @property value [String] value for the param, if any.
 * @property experimentConfig [String] value for the experiment config for the param, if any.
 */
data class SearchUrlParam(
    val name: String,
    val value: String? = null,
    val experimentConfig: String? = null,
)
