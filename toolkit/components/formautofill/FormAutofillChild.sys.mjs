/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddressResult: "resource://autofill/ProfileAutoCompleteResult.sys.mjs",
  AutofillTelemetry: "resource://gre/modules/shared/AutofillTelemetry.sys.mjs",
  CreditCardResult: "resource://autofill/ProfileAutoCompleteResult.sys.mjs",
  GenericAutocompleteItem: "resource://gre/modules/FillHelpers.sys.mjs",
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.sys.mjs",
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
  FormAutofillContent: "resource://autofill/FormAutofillContent.sys.mjs",
  FormAutofillHandler:
    "resource://gre/modules/shared/FormAutofillHandler.sys.mjs",
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.sys.mjs",
  FormScenarios: "resource://gre/modules/FormScenarios.sys.mjs",
  FormStateManager: "resource://gre/modules/shared/FormStateManager.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  FORM_SUBMISSION_REASON: "resource://gre/actors/FormHandlerChild.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DELEGATE_AUTOCOMPLETE",
  "toolkit.autocomplete.delegate",
  false
);

/**
 * Handles content's interactions for the frame.
 */
export class FormAutofillChild extends JSWindowActorChild {
  // Flag to indicate whethere there is an ongoing autofilling process.
  #autofillInProgress = false;

  constructor() {
    super();

    this.log = lazy.FormAutofill.defineLogGetter(this, "FormAutofillChild");
    this.debug("init");

    this._hasDOMContentLoadedHandler = false;

    this._hasRegisteredPageHide = new Set();
    /**
     * @type {FormAutofillFieldDetailsManager} handling state management of current forms and handlers.
     */
    this._fieldDetailsManager = new lazy.FormStateManager(
      this.onFilledModified.bind(this)
    );

    /**
     * Tracks whether the last form submission was triggered by a form submit event,
     * if so we'll ignore the page navigation that follows
     */
    this.isFollowingSubmitEvent = false;
  }

  /**
   * Identifies elements that are in the associated form of the passed element.
   *
   * @param {Element} element
   *        The element to be identified.
   * @param {boolean} triggerByFocus
   *        True to send a message to the parent when new fields are detected.
   *
   * @returns {FormAutofillHandler}
   *        The autofill handler instance for the form that is associated with the
   *        passed element.
   */
  identifyAutofillFields(element, triggerByFocus = true) {
    this.debug(
      `identifyAutofillFields: ${element.ownerDocument.location?.hostname}`
    );

    const { handler, newFieldsIdentified } =
      this._fieldDetailsManager.identifyAutofillFields(element);

    // Bail out if there is nothing changed since last time we identified this element
    // or there is no interested fields.
    if (!newFieldsIdentified || !handler.fieldDetails.length) {
      // This is for testing purposes only. It sends a notification to indicate that the
      // form has been identified and is ready to open the popup.
      // If new fields are detected, the message will be sent to the parent
      // once the parent finishes collecting information from sub-frames if they exist.
      if (triggerByFocus) {
        this.sendAsyncMessage("FormAutofill:FieldsIdentified");
        this.showCreditCardPopupIfEmpty(element);
      }
      return handler;
    }

    // Bug 1905040. This is only a temporarily workaround for now to skip marking address fields
    // autocompletable whenever we detect an address field. We only mark address field when
    // it is a valid address section (This is done in the parent)
    const addressFields = new Set(
      handler.fieldDetails
        .map(f => f.fieldName)
        .filter(fieldName => lazy.FormAutofillUtils.isAddressField(fieldName))
    );
    const validAddressSection =
      addressFields.size >= lazy.FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD;

    for (const fieldDetail of handler.fieldDetails) {
      if (
        !validAddressSection &&
        lazy.FormAutofillUtils.isAddressField(fieldDetail.fieldName)
      ) {
        continue;
      }
      // Inform the autocomplete controller these fields are autofillable
      this.#markAsAutofillField(fieldDetail);
    }

    this.manager.getActor("FormHandler").registerFormSubmissionInterest(this, {
      includesFormRemoval: lazy.FormAutofill.captureOnFormRemoval,
      includesPageNavigation: lazy.FormAutofill.captureOnPageNavigation,
    });

    // TODO (Bug 1901486): Integrate pagehide to FormHandler.
    if (!this._hasRegisteredPageHide.has(handler)) {
      this.registerPageHide(handler);
      this._hasRegisteredPageHide.add(true);
    }

    if (triggerByFocus) {
      // Notify the parent about the newly identified fields because
      // the autofill section information is maintained on the parent side.
      const fields = handler.getFieldsInfoIncludeIframe();
      this.sendQuery(
        "FormAutofill:OnFieldsDetected",
        fields.map(field => field.toVanillaObject())
      ).then(() => {
        this.showCreditCardPopupIfEmpty(element);
      });
    }

    return handler;
  }

  showCreditCardPopupIfEmpty(element) {
    if (element != lazy.FormAutofillContent.focusedInput) {
      return;
    }

    if (element.value?.length !== 0) {
      this.debug(`Not opening popup because field is not empty.`);
      return;
    }

    const handler = this._fieldDetailsManager.getFormHandler(element);
    const fieldName =
      handler?.getFieldDetailByElement(element)?.fieldName ?? "";
    if (fieldName.startsWith("cc-") || AppConstants.platform === "android") {
      lazy.FormAutofillContent.showPopup();
    }
  }

  /**
   * We received a form-submission-detected event because
   * the page was navigated.
   */
  onPageNavigation() {
    if (!lazy.FormAutofill.captureOnPageNavigation) {
      return;
    }

    if (this.isFollowingSubmitEvent) {
      // The next page navigation should be handled as form submission again
      this.isFollowingSubmitEvent = false;
      return;
    }
    let weakIdentifiedForms = ChromeUtils.nondeterministicGetWeakMapKeys(
      this._fieldDetailsManager._formsDetails
    );
    const formSubmissionReason = lazy.FORM_SUBMISSION_REASON.PAGE_NAVIGATION;

    for (const form of weakIdentifiedForms) {
      // Disconnected forms are captured by the form removal heuristic
      if (!form.isConnected) {
        continue;
      }
      this.formSubmitted(form, formSubmissionReason);
    }
  }

  /**
   * We received a form-submission-detected event because
   * a form was removed from the DOM after a successful
   * xhr/fetch request
   *
   * @param {Event} form form to be submitted
   */
  onFormRemoval(form) {
    if (!lazy.FormAutofill.captureOnFormRemoval) {
      return;
    }

    const formSubmissionReason =
      lazy.FORM_SUBMISSION_REASON.FORM_REMOVAL_AFTER_FETCH;
    this.formSubmitted(form, formSubmissionReason);
    this.manager.getActor("FormHandler").unregisterFormRemovalInterest(this);
  }

  registerPageHide(handler) {
    // Check whether the section is in an <iframe>; and, if so,
    // watch for the <iframe> to pagehide.
    if (this.browsingContext != this.browsingContext.top) {
      this.debug(
        "Address/Credit card form is in an iframe -- watching for pagehide",
        handler.fieldDetails
      );
      handler.window.addEventListener(
        "pagehide",
        () => {
          this.debug("Credit card subframe is pagehiding", handler.form);

          const reason = lazy.FORM_SUBMISSION_REASON.IFRAME_PAGEHIDE;
          this.formSubmitted(handler.form.rootElement, reason, handler);
          this._hasRegisteredPageHide.delete(handler);
        },
        { once: true }
      );
    }
  }

  shouldIgnoreFormAutofillEvent(event) {
    if (!event.isTrusted) {
      return true;
    }

    const nodePrincipal = event.target.nodePrincipal;
    return nodePrincipal.isSystemPrincipal || nodePrincipal.schemeIs("about");
  }

  handleEvent(evt) {
    if (
      !lazy.FormAutofill.isAutofillEnabled ||
      this.shouldIgnoreFormAutofillEvent(evt)
    ) {
      return;
    }

    if (!this.windowContext) {
      // !this.windowContext must not be null, because we need the
      // windowContext and/or docShell to (un)register form submission listeners
      return;
    }

    switch (evt.type) {
      case "focusin": {
        this.onFocusIn(evt.target);
        break;
      }
      case "form-submission-detected": {
        const formElement = evt.detail.form;
        const formSubmissionReason = evt.detail.reason;
        this.onFormSubmission(formElement, formSubmissionReason);
        break;
      }

      default: {
        throw new Error("Unexpected event type");
      }
    }
  }

  onFocusIn(element) {
    // When autofilling, we focus on the element before setting the autofill value
    // (See FormAutofillHandler.fillFieldValue). We ignore the focus event for this
    // case to avoid showing popup while autofilling.
    if (
      !lazy.FormAutofillUtils.isCreditCardOrAddressFieldType(element) ||
      this.#autofillInProgress
    ) {
      return;
    }

    const doc = element.ownerDocument;
    if (doc.readyState === "loading") {
      // For auto-focused input, we might receive focus event before document becomes ready.
      // When this happens, run field identification after receiving `DOMContentLoaded` event
      if (!this._hasDOMContentLoadedHandler) {
        this._hasDOMContentLoadedHandler = true;
        doc.addEventListener(
          "DOMContentLoaded",
          () => {
            const element = lazy.FormAutofillContent.focusedInput;
            this.onFocusIn(element);
          },
          { once: true }
        );
      }
      return;
    }

    if (
      lazy.DELEGATE_AUTOCOMPLETE ||
      !lazy.FormAutofillContent.savedFieldNames
    ) {
      this.debug("identifyAutofillFields: savedFieldNames are not known yet");

      // Init can be asynchronous because we don't need anything from the parent
      // at this point.
      this.sendAsyncMessage("FormAutofill:InitStorage");
    }

    this.identifyAutofillFields(element);
  }

  /**
   * Handle form-submission-detected event (dispatched by FormHandlerChild)
   *
   * Depending on the heuristic that detected the form submission,
   * the form that is submitted is retrieved differently
   *
   * @param {HTMLFormElement} form that is being submitted
   * @param {string} reason heuristic that detected the form submission
   *                        (see FormHandlerChild.FORM_SUBMISSION_REASON)
   */
  onFormSubmission(form, reason) {
    switch (reason) {
      case lazy.FORM_SUBMISSION_REASON.PAGE_NAVIGATION:
        this.onPageNavigation();
        break;
      case lazy.FORM_SUBMISSION_REASON.FORM_SUBMIT_EVENT:
        this.formSubmitted(form, reason);
        break;
      case lazy.FORM_SUBMISSION_REASON.FORM_REMOVAL_AFTER_FETCH:
        this.onFormRemoval(form);
        break;
    }
  }

  async receiveMessage(message) {
    if (!lazy.FormAutofill.isAutofillEnabled) {
      return false;
    }

    switch (message.name) {
      case "FormAutofill:FillFields": {
        const { focusedId, ids, profile } = message.data;
        const result = this.fillFields(focusedId, ids, profile);

        // Return the autofilled result to the parent. The result
        // is used by both tests and telemetry.
        return result;
      }
      case "FormAutofill:ClearFilledFields": {
        const { ids } = message.data;
        const handler = this.#getHandlerByElementId(ids[0]);
        handler?.clearFilledFields(ids);
        break;
      }
      case "FormAutofill:PreviewFields": {
        const { ids, profile } = message.data;
        const handler = this.#getHandlerByElementId(ids[0]);

        if (profile) {
          handler?.previewFields(ids, profile);
        } else {
          handler?.clearPreviewedFields(ids);
        }
        break;
      }
      case "FormAutofill:IdentifyFields": {
        const { focusedBCId } = message.data ?? {};
        return this.identifyFields(focusedBCId).map(fieldDetail =>
          fieldDetail.toVanillaObject()
        );
      }
      case "FormAutofill:GetFilledInfo": {
        const { rootElementId } = message.data;
        const handler = this.#getHandlerByElementId(rootElementId);
        return handler?.collectFormFilledData();
      }
      case "FormAutofill:InspectFields": {
        const fieldDetails = this.inspectFields();
        return fieldDetails.map(field => field.toVanillaObject());
      }
    }
    return true;
  }

  /**
   * Handle a form submission and early return when:
   * 1. In private browsing mode.
   * 2. Could not map any autofill handler by form element.
   * 3. Number of filled fields is less than autofill threshold
   *
   * @param {HTMLElement} formElement Root element which receives submit event.
   * @param {string} formSubmissionReason Reason for invoking the form submission
   *                 (see options for FORM_SUBMISSION_REASON in FormAutofillUtils))
   * @param {object} handler FormAutofillHander, if known by caller
   */
  formSubmitted(formElement, formSubmissionReason, handler = undefined) {
    this.debug(`Handling form submission - infered by ${formSubmissionReason}`);

    lazy.AutofillTelemetry.recordFormSubmissionHeuristicCount(
      formSubmissionReason
    );

    if (!lazy.FormAutofill.isAutofillEnabled) {
      this.debug("Form Autofill is disabled");
      return;
    }

    // The `domWin` truthiness test is used by unit tests to bypass this check.
    const domWin = formElement.ownerGlobal;
    if (!domWin) {
      return;
    }

    if (lazy.PrivateBrowsingUtils.isContentWindowPrivate(domWin)) {
      this.debug("Ignoring submission in a private window");
      return;
    }

    handler = handler || this._fieldDetailsManager.getFormHandler(formElement);
    if (!handler) {
      this.debug("Form element could not map to an existing handler");
      return;
    }

    const formFilledData = handler.collectFormFilledData();
    if (!formFilledData) {
      this.debug("Form handler could not obtain filled data");
      return;
    }

    // After a form submit event follows (most likely) a page navigation, so we set this flag
    // to not handle the following one as form submission in order to avoid re-submitting the same form.
    // Ideally, we should keep a record of the last submitted form details and based on that we
    // should decide if we want to submit a form (bug 1895437)
    this.isFollowingSubmitEvent = true;

    this.sendAsyncMessage("FormAutofill:OnFormSubmit", {
      rootElementId: handler.rootElementId,
      formFilledData,
    });
  }

  /**
   * This is called by FormAutofillHandler
   */
  onFilledModified(fieldDetail, previousState, newState) {
    const element = fieldDetail.element;
    if (HTMLInputElement.isInstance(element)) {
      // If the user manually blanks a credit card field, then
      // we want the popup to be activated.
      if (
        lazy.FormAutofillUtils.isCreditCardField(fieldDetail.fieldName) &&
        element.value === ""
      ) {
        lazy.FormAutofillContent.showPopup();
      }
    }

    if (
      previousState == lazy.FormAutofillUtils.FIELD_STATES.AUTO_FILLED &&
      newState == lazy.FormAutofillUtils.FIELD_STATES.NORMAL
    ) {
      this.sendAsyncMessage(
        "FormAutofill:FieldFilledModified",
        fieldDetail.elementId
      );
    }
  }

  async fillFields(focusedId, elementIds, profile) {
    this.#autofillInProgress = true;
    let result = new Map();
    try {
      Services.obs.notifyObservers(null, "autofill-fill-starting");
      const handler = this.#getHandlerByElementId(elementIds[0]);
      handler.fillFields(focusedId, elementIds, profile);

      // Return the autofilled result to the parent. The result
      // is used by both tests and telemetry.
      result = handler.collectFormFilledData();

      Services.obs.notifyObservers(null, "autofill-fill-complete");
    } catch {}

    this.#autofillInProgress = false;
    return result;
  }

  /**
   * Returns all the identified fields for this document.
   * This function is only used by about:autofill extension.
   */
  inspectFields() {
    const elements = Array.from(
      this.document.querySelectorAll("input, select")
    );

    const roots = new Set();
    const fieldDetails = [];
    for (const element of elements) {
      const formLike = lazy.FormLikeFactory.createFromField(element);
      if (roots.has(formLike.rootElement)) {
        continue;
      }
      roots.add(formLike.rootElement);
      const handler = new lazy.FormAutofillHandler(formLike);

      // Fields that cannot be recognized will still be reported with this API.
      const ignoreUnknown = false;
      const fields = handler.collectFormFields(ignoreUnknown);
      fieldDetails.push(...fields);
    }

    // For inspection, we want to return the field according to their order
    return elements
      .map(element => fieldDetails.find(field => field.element == element))
      .filter(field => !!field);
  }

  /**
   * This function is called by the parent when a field is detected in another
   * frame. The parent uses this function to collect field information from frames
   * that are part of the same form as the detected field.
   *
   * @param {string} focusedBCId
   *        The browsing context ID of the top-level iframe
   *        that contains the detected field.
   *        Note that this value is set only when the current frame is the top-level.
   *
   * @returns {Array}
   *        Array of FieldDetail objects of identified fields (including iframes).
   */
  identifyFields(focusedBCId) {
    const isTop = this.browsingContext == this.browsingContext.top;

    let element;
    if (isTop) {
      // Find the focused iframe
      element = BrowsingContext.get(focusedBCId).embedderElement;
    } else {
      // Ignore form as long as the frame is not the top-level, which means
      // we can just pick any of the eligible elements to identify.
      element = this.document.querySelector("input, select, iframe");
    }

    if (!element) {
      return [];
    }

    const handler = this.identifyAutofillFields(element, false);
    return handler.getFieldsInfoIncludeIframe();
  }

  #markAsAutofillField(fieldDetail) {
    const element = fieldDetail.element;

    // Since Form Autofill popup is only for input element, any non-Input
    // element should be excluded here.
    if (!HTMLInputElement.isInstance(element)) {
      return;
    }

    this.manager
      .getActor("AutoComplete")
      ?.markAsAutoCompletableField(element, this);
  }

  get actorName() {
    return "FormAutofill";
  }

  /**
   * Get the search options when searching for autocomplete entries in the parent
   *
   * @param {HTMLInputElement} input - The input element to search for autocomplete entries
   * @returns {object} the search options for the input
   */
  getAutoCompleteSearchOption(input) {
    const fieldDetail = this._fieldDetailsManager
      .getFormHandler(input)
      ?.getFieldDetailByElement(input);

    const scenarioName = lazy.FormScenarios.detect({ input }).signUpForm
      ? "SignUpFormScenario"
      : "";
    return {
      fieldName: fieldDetail?.fieldName,
      elementId: fieldDetail?.elementId,
      scenarioName,
    };
  }

  /**
   * Ask the provider whether it might have autocomplete entry to show
   * for the given input.
   *
   * @param {HTMLInputElement} input - The input element to search for autocomplete entries
   * @returns {boolean} true if we shold search for autocomplete entries
   */
  shouldSearchForAutoComplete(input) {
    const fieldDetail = this._fieldDetailsManager
      .getFormHandler(input)
      ?.getFieldDetailByElement(input);
    if (!fieldDetail) {
      return false;
    }
    const fieldName = fieldDetail.fieldName;
    const isAddressField = lazy.FormAutofillUtils.isAddressField(fieldName);
    const searchPermitted = isAddressField
      ? lazy.FormAutofill.isAutofillAddressesEnabled
      : lazy.FormAutofill.isAutofillCreditCardsEnabled;
    // If the specified autofill feature is pref off, do not search
    if (!searchPermitted) {
      return false;
    }

    // No profile can fill the currently-focused input.
    if (!lazy.FormAutofillContent.savedFieldNames.has(fieldName)) {
      return false;
    }

    return true;
  }

  /**
   * Convert the search result to autocomplete results
   *
   * @param {string} searchString - The string to search for
   * @param {HTMLInputElement} input - The input element to search for autocomplete entries
   * @param {Array<object>} records - autocomplete records
   * @returns {AutocompleteResult}
   */
  searchResultToAutoCompleteResult(searchString, input, records) {
    if (!records) {
      return null;
    }

    const handler = this._fieldDetailsManager.getFormHandler(input);
    const fieldDetail = handler?.getFieldDetailByElement(input);
    if (!fieldDetail) {
      return null;
    }

    const adaptedRecords = handler.getAdaptedProfiles(records.records);
    const isSecure = lazy.InsecurePasswordUtils.isFormSecure(handler.form);
    const isInputAutofilled =
      input.autofillState == lazy.FormAutofillUtils.FIELD_STATES.AUTO_FILLED;

    let AutocompleteResult;

    // TODO: This should be calculated in the parent
    // The field categories will be filled if the corresponding profile is
    // used for autofill. We don't display this information for credit
    // cards, so this is only calculated for address fields.
    let fillCategories;
    if (lazy.FormAutofillUtils.isAddressField(fieldDetail.fieldName)) {
      AutocompleteResult = lazy.AddressResult;
      fillCategories = adaptedRecords.map(profile => {
        const fields = Object.keys(profile).filter(fieldName => {
          const detail = handler.getFieldDetailByName(fieldName);
          return detail ? handler.isFieldAutofillable(detail, profile) : false;
        });
        return lazy.FormAutofillUtils.getCategoriesFromFieldNames(fields);
      });
    } else {
      AutocompleteResult = lazy.CreditCardResult;
    }

    const acResult = new AutocompleteResult(
      searchString,
      fieldDetail,
      records.allFieldNames,
      adaptedRecords,
      fillCategories,
      { isSecure, isInputAutofilled }
    );

    const externalEntries = records.externalEntries;

    acResult.externalEntries.push(
      ...externalEntries.map(
        entry =>
          new lazy.GenericAutocompleteItem(
            entry.image,
            entry.title,
            entry.subtitle,
            entry.fillMessageName,
            entry.fillMessageData
          )
      )
    );

    return acResult;
  }

  #getHandlerByElementId(elementId) {
    const element = lazy.FormAutofillUtils.getElementByIdentifier(elementId);
    return this._fieldDetailsManager.getFormHandler(element);
  }
}
