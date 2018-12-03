/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * RelativeTimeFormat internal properties.
 *
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.3.3.
 */
var relativeTimeFormatInternalProperties = {
    localeData: relativeTimeFormatLocaleData,
    _availableLocales: null,
    availableLocales: function() // eslint-disable-line object-shorthand
    {
        var locales = this._availableLocales;
        if (locales)
            return locales;

        locales = intl_RelativeTimeFormat_availableLocales();
        addSpecialMissingLanguageTags(locales);
        return (this._availableLocales = locales);
    },
    relevantExtensionKeys: [],
};

function relativeTimeFormatLocaleData() {
    // RelativeTimeFormat doesn't support any extension keys.
    return {};
}

/**
 * Compute an internal properties object from |lazyRelativeTimeFormatData|.
 */
function resolveRelativeTimeFormatInternals(lazyRelativeTimeFormatData) {
    assert(IsObject(lazyRelativeTimeFormatData), "lazy data not an object?");

    var internalProps = std_Object_create(null);

    var RelativeTimeFormat = relativeTimeFormatInternalProperties;

    // Steps 7-8.
    const r = ResolveLocale(callFunction(RelativeTimeFormat.availableLocales, RelativeTimeFormat),
                            lazyRelativeTimeFormatData.requestedLocales,
                            lazyRelativeTimeFormatData.opt,
                            RelativeTimeFormat.relevantExtensionKeys,
                            RelativeTimeFormat.localeData);

    // Steps 9-10.
    internalProps.locale = r.locale;

    // Step 11.
    assert(r.locale === r.dataLocale,
           "resolved locale matches the resolved data-locale when no extension-keys are present");

    // Step 13.
    internalProps.style = lazyRelativeTimeFormatData.style;

    // Step 15.
    internalProps.numeric = lazyRelativeTimeFormatData.numeric;

    // Steps 16-20 (Not relevant in our implementation).

    return internalProps;
}

/**
 * Returns an object containing the RelativeTimeFormat internal properties of |obj|,
 * or throws a TypeError if |obj| isn't RelativeTimeFormat-initialized.
 */
function getRelativeTimeFormatInternals(obj, methodName) {
    assert(IsObject(obj), "getRelativeTimeFormatInternals called with non-object");
    assert(GuardToRelativeTimeFormat(obj) !== null, "getRelativeTimeFormatInternals called with non-RelativeTimeFormat");

    var internals = getIntlObjectInternals(obj);
    assert(internals.type === "RelativeTimeFormat", "bad type escaped getIntlObjectInternals");

    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    internalProps = resolveRelativeTimeFormatInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}

/**
 * Initializes an object as a RelativeTimeFormat.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a RelativeTimeFormat.
 * This later work occurs in |resolveRelativeTimeFormatInternals|; steps not noted
 * here occur there.
 *
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.1.1.
 */
function InitializeRelativeTimeFormat(relativeTimeFormat, locales, options) {
    assert(IsObject(relativeTimeFormat),
           "InitializeRelativeimeFormat called with non-object");
    assert(GuardToRelativeTimeFormat(relativeTimeFormat) !== null,
           "InitializeRelativeTimeFormat called with non-RelativeTimeFormat");

    // Lazy RelativeTimeFormat data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //     style: "long" / "short" / "narrow",
    //     numeric: "always" / "auto",
    //
    //     opt: // opt object computed in InitializeRelativeTimeFormat
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //       }
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every RelativeTimeFormat lazy data object has *all* these properties, never a
    // subset of them.
    const lazyRelativeTimeFormatData = std_Object_create(null);

    // Step 1.
    let requestedLocales = CanonicalizeLocaleList(locales);
    lazyRelativeTimeFormatData.requestedLocales = requestedLocales;

    // Steps 2-3.
    if (options === undefined)
        options = std_Object_create(null);
    else
        options = ToObject(options);

    // Step 4.
    let opt = new Record();

    // Steps 5-6.
    let matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
    opt.localeMatcher = matcher;

    lazyRelativeTimeFormatData.opt = opt;

    // Steps 12-13.
    const style = GetOption(options, "style", "string", ["long", "short", "narrow"], "long");
    lazyRelativeTimeFormatData.style = style;

    // Steps 14-15.
    const numeric = GetOption(options, "numeric", "string", ["always", "auto"], "always");
    lazyRelativeTimeFormatData.numeric = numeric;

    initializeIntlObject(relativeTimeFormat, "RelativeTimeFormat", lazyRelativeTimeFormatData);
}

/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.3.2.
 */
function Intl_RelativeTimeFormat_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    // Step 1.
    var availableLocales = callFunction(relativeTimeFormatInternalProperties.availableLocales,
                                        relativeTimeFormatInternalProperties);
    // Step 2.
    let requestedLocales = CanonicalizeLocaleList(locales);

    // Step 3.
    return SupportedLocales(availableLocales, requestedLocales, options);
}

/**
 * Returns a String value representing the written form of a relative date
 * formatted according to the effective locale and the formatting options
 * of this RelativeTimeFormat object.
 *
 * Spec: ECMAScript 402 API, RelativeTImeFormat, 1.4.3.
 */
function Intl_RelativeTimeFormat_format(value, unit) {
    // Step 1.
    let relativeTimeFormat = this;

    // Step 2.
    if (!IsObject(relativeTimeFormat) ||
        (relativeTimeFormat = GuardToRelativeTimeFormat(relativeTimeFormat)) === null)
    {
        return callFunction(CallRelativeTimeFormatMethodIfWrapped, this, value, unit,
                            "Intl_RelativeTimeFormat_format");
    }

    // Ensure the RelativeTimeFormat internals are resolved.
    var internals = getRelativeTimeFormatInternals(relativeTimeFormat);

    // Step 3.
    let t = ToNumber(value);

    // Step 4.
    let u = ToString(unit);

    // PartitionRelativeTimePattern, step 4.
    if (!Number_isFinite(t)) {
        ThrowRangeError(JSMSG_DATE_NOT_FINITE, "RelativeTimeFormat");
    }

    // PartitionRelativeTimePattern, step 5.
    switch (u) {
      case "second":
      case "seconds":
      case "minute":
      case "minutes":
      case "hour":
      case "hours":
      case "day":
      case "days":
      case "week":
      case "weeks":
      case "month":
      case "months":
      case "quarter":
      case "quarters":
      case "year":
      case "years":
        break;
      default:
        ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, "unit", u);
    }

    // Step 5.
    return intl_FormatRelativeTime(relativeTimeFormat, t, u, internals.numeric);
}

/**
 * Returns the resolved options for a RelativeTimeFormat object.
 *
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.4.4.
 */
function Intl_RelativeTimeFormat_resolvedOptions() {
    // Step 1.
    var relativeTimeFormat = this;

    // Steps 2-3.
    if (!IsObject(relativeTimeFormat) ||
        (relativeTimeFormat = GuardToRelativeTimeFormat(relativeTimeFormat)) === null)
    {
        return callFunction(CallRelativeTimeFormatMethodIfWrapped, this,
                            "Intl_RelativeTimeFormat_resolvedOptions");
    }

    var internals = getRelativeTimeFormatInternals(relativeTimeFormat, "resolvedOptions");

    // Steps 4-5.
    var result = {
        locale: internals.locale,
        style: internals.style,
        numeric: internals.numeric,
    };

    // Step 6.
    return result;
}
