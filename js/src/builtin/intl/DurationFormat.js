/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DurationFormat internal properties.
 */
function durationFormatLocaleData() {
  return {
    nu: getNumberingSystems,
    default: {
      nu: intl_numberingSystem,
    },
  };
}
var durationFormatInternalProperties = {
  localeData: durationFormatLocaleData,
  relevantExtensionKeys: ["nu"],
};

/**
 * Intl.DurationFormat ( [ locales [ , options ] ] )
 *
 * Compute an internal properties object from |lazyDurationFormatData|.
 */
function resolveDurationFormatInternals(lazyDurationFormatData) {
  assert(IsObject(lazyDurationFormatData), "lazy data not an object?");

  var internalProps = std_Object_create(null);

  var DurationFormat = durationFormatInternalProperties;

  // Compute effective locale.

  // Step 9.
  var r = ResolveLocale(
    "DurationFormat",
    lazyDurationFormatData.requestedLocales,
    lazyDurationFormatData.opt,
    DurationFormat.relevantExtensionKeys,
    DurationFormat.localeData
  );

  // Steps 10-11.
  internalProps.locale = r.locale;

  // Steps 12-21. (Not applicable in our implementation.)

  // Step 22.
  internalProps.numberingSystem = r.nu;

  // Step 24.
  internalProps.style = lazyDurationFormatData.style;

  // Step 26.
  internalProps.yearsStyle = lazyDurationFormatData.yearsStyle;
  internalProps.yearsDisplay = lazyDurationFormatData.yearsDisplay;

  internalProps.weeksStyle = lazyDurationFormatData.weeksStyle;
  internalProps.weeksDisplay = lazyDurationFormatData.weeksDisplay;

  internalProps.monthsStyle = lazyDurationFormatData.monthsStyle;
  internalProps.monthsDisplay = lazyDurationFormatData.monthsDisplay;

  internalProps.daysStyle = lazyDurationFormatData.daysStyle;
  internalProps.daysDisplay = lazyDurationFormatData.daysDisplay;

  internalProps.hoursStyle = lazyDurationFormatData.hoursStyle;
  internalProps.hoursDisplay = lazyDurationFormatData.hoursDisplay;

  internalProps.minutesStyle = lazyDurationFormatData.minutesStyle;
  internalProps.minutesDisplay = lazyDurationFormatData.minutesDisplay;

  internalProps.secondsStyle = lazyDurationFormatData.secondsStyle;
  internalProps.secondsDisplay = lazyDurationFormatData.secondsDisplay;

  internalProps.millisecondsStyle = lazyDurationFormatData.millisecondsStyle;
  internalProps.millisecondsDisplay =
    lazyDurationFormatData.millisecondsDisplay;

  internalProps.microsecondsStyle = lazyDurationFormatData.microsecondsStyle;
  internalProps.microsecondsDisplay =
    lazyDurationFormatData.microsecondsDisplay;

  internalProps.nanosecondsStyle = lazyDurationFormatData.nanosecondsStyle;
  internalProps.nanosecondsDisplay = lazyDurationFormatData.nanosecondsDisplay;

  // Step 27.
  internalProps.fractionalDigits = lazyDurationFormatData.fractionalDigits;

  // The caller is responsible for associating |internalProps| with the right
  // object using |setInternalProperties|.
  return internalProps;
}

/**
 * Returns an object containing the DurationFormat internal properties of |obj|.
 */
function getDurationFormatInternals(obj) {
  assert(IsObject(obj), "getDurationFormatInternals called with non-object");
  assert(
    intl_GuardToDurationFormat(obj) !== null,
    "getDurationFormatInternals called with non-DurationFormat"
  );

  var internals = getIntlObjectInternals(obj);
  assert(
    internals.type === "DurationFormat",
    "bad type escaped getIntlObjectInternals"
  );

  // If internal properties have already been computed, use them.
  var internalProps = maybeInternalProperties(internals);
  if (internalProps) {
    return internalProps;
  }

  // Otherwise it's time to fully create them.
  internalProps = resolveDurationFormatInternals(internals.lazyData);
  setInternalProperties(internals, internalProps);
  return internalProps;
}

/**
 * Intl.DurationFormat ( [ locales [ , options ] ] )
 *
 * Initializes an object as a DurationFormat.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a DurationFormat.
 * This later work occurs in |resolveDurationFormatInternals|; steps not noted
 * here occur there.
 */
function InitializeDurationFormat(durationFormat, locales, options) {
  assert(
    IsObject(durationFormat),
    "InitializeDurationFormat called with non-object"
  );
  assert(
    intl_GuardToDurationFormat(durationFormat) !== null,
    "InitializeDurationFormat called with non-DurationFormat"
  );

  // Lazy DurationFormat data has the following structure:
  //
  //   {
  //     requestedLocales: List of locales,
  //     style: "long" / "short" / "narrow" / "digital",
  //
  //     yearsStyle: "long" / "short" / "narrow",
  //     yearsDisplay: "auto" / "always",
  //
  //     monthsStyle: "long" / "short" / "narrow",
  //     monthsDisplay: "auto" / "always",
  //
  //     weeksStyle: "long" / "short" / "narrow",
  //     weeksDisplay: "auto" / "always",
  //
  //     daysStyle: "long" / "short" / "narrow",
  //     daysDisplay: "auto" / "always",
  //
  //     hoursStyle: "long" / "short" / "narrow" / "numeric" / "2-digit",
  //     hoursDisplay: "auto" / "always",
  //
  //     minutesStyle: "long" / "short" / "narrow" / "numeric" / "2-digit",
  //     minutesDisplay: "auto" / "always",
  //
  //     secondsStyle: "long" / "short" / "narrow" / "numeric" / "2-digit",
  //     secondsDisplay: "auto" / "always",
  //
  //     millisecondsStyle: "long" / "short" / "narrow" / "numeric",
  //     millisecondsDisplay: "auto" / "always",
  //
  //     microsecondsStyle: "long" / "short" / "narrow" / "numeric",
  //     microsecondsDisplay: "auto" / "always",
  //
  //     nanosecondsStyle: "long" / "short" / "narrow" / "numeric",
  //     nanosecondsDisplay: "auto" / "always",
  //
  //     fractionalDigits: integer ∈ [0, 9] / undefined,
  //
  //     opt: // opt object computed in InitializeDurationFormat
  //       {
  //         localeMatcher: "lookup" / "best fit",
  //
  //         nu: string matching a Unicode extension type, // optional
  //       }
  //   }
  //
  // Note that lazy data is only installed as a final step of initialization,
  // so every DurationFormat lazy data object has *all* these properties,
  // never a subset of them.
  var lazyDurationFormatData = std_Object_create(null);

  // Step 3.
  var requestedLocales = CanonicalizeLocaleList(locales);
  lazyDurationFormatData.requestedLocales = requestedLocales;

  // Step 4.
  if (options === undefined) {
    options = std_Object_create(null);
  } else if (!IsObject(options)) {
    ThrowTypeError(
      JSMSG_OBJECT_REQUIRED,
      options === null ? "null" : typeof options
    );
  }

  // Step 5.
  var matcher = GetOption(
    options,
    "localeMatcher",
    "string",
    ["lookup", "best fit"],
    "best fit"
  );

  // Step 6.
  var numberingSystem = GetOption(
    options,
    "numberingSystem",
    "string",
    undefined,
    undefined
  );

  // Step 7.
  if (numberingSystem !== undefined) {
    numberingSystem = intl_ValidateAndCanonicalizeUnicodeExtensionType(
      numberingSystem,
      "numberingSystem",
      "nu"
    );
  }

  // Step 8.
  var opt = new_Record();
  opt.localeMatcher = matcher;
  opt.nu = numberingSystem;

  lazyDurationFormatData.opt = opt;

  // Compute formatting options.

  // Steps 23-24.
  var style = GetOption(
    options,
    "style",
    "string",
    ["long", "short", "narrow", "digital"],
    "short"
  );
  lazyDurationFormatData.style = style;

  // Step 25. (Not applicable in our implementation)

  // Step 26, unit = "years".
  var yearsOptions = GetDurationUnitOptions(
    "years",
    options,
    style,
    ["long", "short", "narrow"],
    "short",
    /* prevStyle= */ ""
  );
  lazyDurationFormatData.yearsStyle = yearsOptions.style;
  lazyDurationFormatData.yearsDisplay = yearsOptions.display;

  // Step 26, unit = "months".
  var monthsOptions = GetDurationUnitOptions(
    "months",
    options,
    style,
    ["long", "short", "narrow"],
    "short",
    /* prevStyle= */ ""
  );
  lazyDurationFormatData.monthsStyle = monthsOptions.style;
  lazyDurationFormatData.monthsDisplay = monthsOptions.display;

  // Step 26, unit = "weeks".
  var weeksOptions = GetDurationUnitOptions(
    "weeks",
    options,
    style,
    ["long", "short", "narrow"],
    "short",
    /* prevStyle= */ ""
  );
  lazyDurationFormatData.weeksStyle = weeksOptions.style;
  lazyDurationFormatData.weeksDisplay = weeksOptions.display;

  // Step 26, unit = "days".
  var daysOptions = GetDurationUnitOptions(
    "days",
    options,
    style,
    ["long", "short", "narrow"],
    "short",
    /* prevStyle= */ ""
  );
  lazyDurationFormatData.daysStyle = daysOptions.style;
  lazyDurationFormatData.daysDisplay = daysOptions.display;

  // Step 26, unit = "hours".
  var hoursOptions = GetDurationUnitOptions(
    "hours",
    options,
    style,
    ["long", "short", "narrow", "numeric", "2-digit"],
    "numeric",
    /* prevStyle= */ ""
  );
  lazyDurationFormatData.hoursStyle = hoursOptions.style;
  lazyDurationFormatData.hoursDisplay = hoursOptions.display;

  // Step 26, unit = "minutes".
  var minutesOptions = GetDurationUnitOptions(
    "minutes",
    options,
    style,
    ["long", "short", "narrow", "numeric", "2-digit"],
    "numeric",
    hoursOptions.style
  );
  lazyDurationFormatData.minutesStyle = minutesOptions.style;
  lazyDurationFormatData.minutesDisplay = minutesOptions.display;

  // Step 26, unit = "seconds".
  var secondsOptions = GetDurationUnitOptions(
    "seconds",
    options,
    style,
    ["long", "short", "narrow", "numeric", "2-digit"],
    "numeric",
    minutesOptions.style
  );
  lazyDurationFormatData.secondsStyle = secondsOptions.style;
  lazyDurationFormatData.secondsDisplay = secondsOptions.display;

  // Step 26, unit = "milliseconds".
  var millisecondsOptions = GetDurationUnitOptions(
    "milliseconds",
    options,
    style,
    ["long", "short", "narrow", "numeric"],
    "numeric",
    secondsOptions.style
  );
  lazyDurationFormatData.millisecondsStyle = millisecondsOptions.style;
  lazyDurationFormatData.millisecondsDisplay = millisecondsOptions.display;

  // Step 26, unit = "microseconds".
  var microsecondsOptions = GetDurationUnitOptions(
    "microseconds",
    options,
    style,
    ["long", "short", "narrow", "numeric"],
    "numeric",
    millisecondsOptions.style
  );
  lazyDurationFormatData.microsecondsStyle = microsecondsOptions.style;
  lazyDurationFormatData.microsecondsDisplay = microsecondsOptions.display;

  // Step 26, unit = "milliseconds".
  var nanosecondsOptions = GetDurationUnitOptions(
    "nanoseconds",
    options,
    style,
    ["long", "short", "narrow", "numeric"],
    "numeric",
    microsecondsOptions.style
  );
  lazyDurationFormatData.nanosecondsStyle = nanosecondsOptions.style;
  lazyDurationFormatData.nanosecondsDisplay = nanosecondsOptions.display;

  // Step 27.
  lazyDurationFormatData.fractionalDigits = GetNumberOption(
    options,
    "fractionalDigits",
    0,
    9,
    undefined
  );

  // We've done everything that must be done now: mark the lazy data as fully
  // computed and install it.
  initializeIntlObject(
    durationFormat,
    "DurationFormat",
    lazyDurationFormatData
  );
}

/**
 * GetDurationUnitOptions ( unit, options, baseStyle, stylesList, digitalBase, prevStyle, twoDigitHours )
 */
function GetDurationUnitOptions(
  unit,
  options,
  baseStyle,
  stylesList,
  digitalBase,
  prevStyle
) {
  assert(typeof unit === "string", "unit is a string");
  assert(IsObject(options), "options is an object");
  assert(typeof baseStyle === "string", "baseStyle is a string");
  assert(IsArray(stylesList), "stylesList is an array");
  assert(typeof digitalBase === "string", "digitalBase is a string");
  assert(typeof prevStyle === "string", "prevStyle is a string");

  // Step 1.
  var styleOption = GetOption(options, unit, "string", stylesList, undefined);

  var style = styleOption;

  // Step 2.
  var displayDefault = "always";

  // Step 3.
  if (style === undefined) {
    // Steps 3.a-b.
    if (baseStyle === "digital") {
      // Step 3.a.i.
      if (unit !== "hours" && unit !== "minutes" && unit !== "seconds") {
        displayDefault = "auto";
      }

      // Step 3.a.ii.
      style = digitalBase;
    } else {
      // Steps 3.b.i-ii. ("fractional" handled implicitly)
      if (prevStyle === "numeric" || prevStyle === "2-digit") {
        // Step 3.b.i.1.
        if (unit !== "minutes" && unit !== "seconds") {
          // Step 3.b.i.1.a.
          displayDefault = "auto";
        }

        // Step 3.b.i.2.
        style = "numeric";
      } else {
        // Step 3.b.ii.1.
        displayDefault = "auto";

        // Step 3.b.ii.2.
        style = baseStyle;
      }
    }
  }

  // Step 4.
  var isFractional =
    style === "numeric" &&
    (unit === "milliseconds" ||
      unit === "microseconds" ||
      unit === "nanoseconds");
  if (isFractional) {
    // Step 4.a.i. (Not applicable in our implementation)

    // Step 4.a.ii.
    displayDefault = "auto";
  }

  // Step 5.
  var displayField = unit + "Display";

  // Step 6.
  var displayOption = GetOption(
    options,
    displayField,
    "string",
    ["auto", "always"],
    undefined
  );

  var display = displayOption ?? displayDefault;

  // Step 7.
  if (display === "always" && isFractional) {
    assert(
      styleOption !== undefined || displayOption !== undefined,
      "no error is thrown when both 'style' and 'display' are absent"
    );

    ThrowRangeError(
      // eslint-disable-next-line no-nested-ternary
      styleOption !== undefined && displayOption !== undefined
        ? JSMSG_INTL_DURATION_INVALID_DISPLAY_OPTION
        : displayOption !== undefined
        ? JSMSG_INTL_DURATION_INVALID_DISPLAY_OPTION_DEFAULT_STYLE
        : JSMSG_INTL_DURATION_INVALID_DISPLAY_OPTION_DEFAULT_DISPLAY,
      unit
    );
  }

  // Steps 8-9.
  if (prevStyle === "numeric" || prevStyle === "2-digit") {
    // Step 8.a. and 9.a.
    if (style !== "numeric" && style !== "2-digit") {
      ThrowRangeError(
        JSMSG_INTL_DURATION_INVALID_NON_NUMERIC_OPTION,
        unit,
        `"${style}"`
      );
    }

    // Step 9.b.
    else if (unit === "minutes" || unit === "seconds") {
      style = "2-digit";
    }
  }

  // Step 10. (Our implementation doesn't use |twoDigitHours|.)

  // Step 11.
  return { style, display };
}

/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 */
function Intl_DurationFormat_supportedLocalesOf(locales /*, options*/) {
  var options = ArgumentsLength() > 1 ? GetArgument(1) : undefined;

  // Step 1.
  var availableLocales = "DurationFormat";

  // Step 2.
  var requestedLocales = CanonicalizeLocaleList(locales);

  // Step 3.
  return SupportedLocales(availableLocales, requestedLocales, options);
}

/**
 * Returns the resolved options for a DurationFormat object.
 */
function Intl_DurationFormat_resolvedOptions() {
  // Step 1.
  var durationFormat = this;

  // Step 2.
  if (
    !IsObject(durationFormat) ||
    (durationFormat = intl_GuardToDurationFormat(durationFormat)) === null
  ) {
    return callFunction(
      intl_CallDurationFormatMethodIfWrapped,
      this,
      "Intl_DurationFormat_resolvedOptions"
    );
  }

  var internals = getDurationFormatInternals(durationFormat);

  // Steps 3-4.
  var result = {
    locale: internals.locale,
    numberingSystem: internals.numberingSystem,
    style: internals.style,
    years: internals.yearsStyle,
    yearsDisplay: internals.yearsDisplay,
    months: internals.monthsStyle,
    monthsDisplay: internals.monthsDisplay,
    weeks: internals.weeksStyle,
    weeksDisplay: internals.weeksDisplay,
    days: internals.daysStyle,
    daysDisplay: internals.daysDisplay,
    hours: internals.hoursStyle,
    hoursDisplay: internals.hoursDisplay,
    minutes: internals.minutesStyle,
    minutesDisplay: internals.minutesDisplay,
    seconds: internals.secondsStyle,
    secondsDisplay: internals.secondsDisplay,
    milliseconds: internals.millisecondsStyle,
    millisecondsDisplay: internals.millisecondsDisplay,
    microseconds: internals.microsecondsStyle,
    microsecondsDisplay: internals.microsecondsDisplay,
    nanoseconds: internals.nanosecondsStyle,
    nanosecondsDisplay: internals.nanosecondsDisplay,
  };

  if (internals.fractionalDigits !== undefined) {
    DefineDataProperty(result, "fractionalDigits", internals.fractionalDigits);
  }

  // Step 5.
  return result;
}
