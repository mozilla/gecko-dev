/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements a service used to access storage and communicate with content.
 *
 * A "fields" array is used to communicate with FormAutofillChild. Each item
 * represents a single input field in the content page as well as its
 * @autocomplete properties. The schema is as below. Please refer to
 * FormAutofillChild.js for more details.
 *
 * [
 *   {
 *     section,
 *     addressType,
 *     contactType,
 *     fieldName,
 *     value,
 *     index
 *   },
 *   {
 *     // ...
 *   }
 * ]
 */

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
import { FormAutofill } from "resource://autofill/FormAutofill.sys.mjs";
import { FormAutofillUtils } from "resource://gre/modules/shared/FormAutofillUtils.sys.mjs";

const { FIELD_STATES } = FormAutofillUtils;

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddressComponent: "resource://gre/modules/shared/AddressComponent.sys.mjs",
  // eslint-disable-next-line mozilla/no-browser-refs-in-toolkit
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  FormAutofillAddressSection:
    "resource://gre/modules/shared/FormAutofillSection.sys.mjs",
  FormAutofillCreditCardSection:
    "resource://gre/modules/shared/FormAutofillSection.sys.mjs",
  FormAutofillSection:
    "resource://gre/modules/shared/FormAutofillSection.sys.mjs",
  FormAutofillPreferences:
    "resource://autofill/FormAutofillPreferences.sys.mjs",
  FormAutofillPrompter: "resource://autofill/FormAutofillPrompter.sys.mjs",
  FirefoxRelay: "resource://gre/modules/FirefoxRelay.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  OSKeyStore: "resource://gre/modules/OSKeyStore.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () =>
  FormAutofill.defineLogGetter(lazy, "FormAutofillParent")
);

const { ENABLED_AUTOFILL_ADDRESSES_PREF, ENABLED_AUTOFILL_CREDITCARDS_PREF } =
  FormAutofill;

const { ADDRESSES_COLLECTION_NAME, CREDITCARDS_COLLECTION_NAME } =
  FormAutofillUtils;

let gMessageObservers = new Set();

export let FormAutofillStatus = {
  _initialized: false,

  /**
   * Cache of the Form Autofill status (considering preferences and storage).
   */
  _active: null,

  /**
   * Initializes observers and registers the message handler.
   */
  init() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    Services.obs.addObserver(this, "privacy-pane-loaded");

    // Observing the pref and storage changes
    Services.prefs.addObserver(ENABLED_AUTOFILL_ADDRESSES_PREF, this);
    Services.obs.addObserver(this, "formautofill-storage-changed");

    // Only listen to credit card related preference if it is available
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      Services.prefs.addObserver(ENABLED_AUTOFILL_CREDITCARDS_PREF, this);
    }

    Services.telemetry.setEventRecordingEnabled("creditcard", true);
    Services.telemetry.setEventRecordingEnabled("address", true);
  },

  /**
   * Uninitializes FormAutofillStatus. This is for testing only.
   *
   * @private
   */
  uninit() {
    lazy.gFormAutofillStorage._saveImmediately();

    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    this._active = null;

    Services.obs.removeObserver(this, "privacy-pane-loaded");
    Services.prefs.removeObserver(ENABLED_AUTOFILL_ADDRESSES_PREF, this);
    Services.wm.removeListener(this);

    if (FormAutofill.isAutofillCreditCardsAvailable) {
      Services.prefs.removeObserver(ENABLED_AUTOFILL_CREDITCARDS_PREF, this);
    }
  },

  get formAutofillStorage() {
    return lazy.gFormAutofillStorage;
  },

  /**
   * Broadcast the status to frames when the form autofill status changes.
   */
  onStatusChanged() {
    lazy.log.debug("onStatusChanged: Status changed to", this._active);
    Services.ppmm.sharedData.set("FormAutofill:enabled", this._active);
    // Sync autofill enabled to make sure the value is up-to-date
    // no matter when the new content process is initialized.
    Services.ppmm.sharedData.flush();
  },

  /**
   * Query preference and storage status to determine the overall status of the
   * form autofill feature.
   *
   * @returns {boolean} whether form autofill is active (enabled and has data)
   */
  computeStatus() {
    const savedFieldNames = Services.ppmm.sharedData.get(
      "FormAutofill:savedFieldNames"
    );

    return (
      (Services.prefs.getBoolPref(ENABLED_AUTOFILL_ADDRESSES_PREF) ||
        Services.prefs.getBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF)) &&
      savedFieldNames &&
      savedFieldNames.size > 0
    );
  },

  /**
   * Update the status and trigger onStatusChanged, if necessary.
   */
  updateStatus() {
    lazy.log.debug("updateStatus");
    let wasActive = this._active;
    this._active = this.computeStatus();
    if (this._active !== wasActive) {
      this.onStatusChanged();
    }
  },

  async updateSavedFieldNames() {
    lazy.log.debug("updateSavedFieldNames");

    let savedFieldNames;
    const addressNames =
      await lazy.gFormAutofillStorage.addresses.getSavedFieldNames();

    // Don't access the credit cards store unless it is enabled.
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      const creditCardNames =
        await lazy.gFormAutofillStorage.creditCards.getSavedFieldNames();
      savedFieldNames = new Set([...addressNames, ...creditCardNames]);
    } else {
      savedFieldNames = addressNames;
    }

    Services.ppmm.sharedData.set(
      "FormAutofill:savedFieldNames",
      savedFieldNames
    );
    Services.ppmm.sharedData.flush();

    this.updateStatus();
  },

  async observe(subject, topic, data) {
    lazy.log.debug("observe:", topic, "with data:", data);
    switch (topic) {
      case "privacy-pane-loaded": {
        let formAutofillPreferences = new lazy.FormAutofillPreferences();
        let document = subject.document;
        let prefFragment = formAutofillPreferences.init(document);
        let formAutofillGroupBox = document.getElementById(
          "formAutofillGroupBox"
        );
        formAutofillGroupBox.appendChild(prefFragment);
        break;
      }

      case "nsPref:changed": {
        // Observe pref changes and update _active cache if status is changed.
        this.updateStatus();
        break;
      }

      case "formautofill-storage-changed": {
        // Early exit if only metadata is changed
        if (data == "notifyUsed") {
          break;
        }

        await this.updateSavedFieldNames();
        break;
      }

      default: {
        throw new Error(
          `FormAutofillStatus: Unexpected topic observed: ${topic}`
        );
      }
    }
  },
};

// Lazily load the storage JSM to avoid disk I/O until absolutely needed.
// Once storage is loaded we need to update saved field names and inform content processes.
ChromeUtils.defineLazyGetter(lazy, "gFormAutofillStorage", () => {
  let { formAutofillStorage } = ChromeUtils.importESModule(
    "resource://autofill/FormAutofillStorage.sys.mjs"
  );
  lazy.log.debug("Loading formAutofillStorage");

  formAutofillStorage.initialize().then(() => {
    // Update the saved field names to compute the status and update child processes.
    FormAutofillStatus.updateSavedFieldNames();
  });

  return formAutofillStorage;
});

export class FormAutofillParent extends JSWindowActorParent {
  // An array of section that are found in this form
  sectionsByRootId = {};

  constructor() {
    super();
    FormAutofillStatus.init();
  }

  static addMessageObserver(observer) {
    gMessageObservers.add(observer);
  }

  static removeMessageObserver(observer) {
    gMessageObservers.delete(observer);
  }

  /**
   * Handles the message coming from FormAutofillChild.
   *
   * @param   {object} message
   * @param   {string} message.name The name of the message.
   * @param   {object} message.data The data of the message.
   */
  async receiveMessage({ name, data }) {
    switch (name) {
      case "FormAutofill:InitStorage": {
        await lazy.gFormAutofillStorage.initialize();
        await FormAutofillStatus.updateSavedFieldNames();
        break;
      }
      case "FormAutofill:GetRecords": {
        const records = await FormAutofillParent.getRecords(data);
        return { records };
      }
      case "FormAutofill:OnFormSubmit": {
        const { rootElementId, formFilledData } = data;
        this.notifyMessageObservers("onFormSubmitted", data);
        await this._onFormSubmit(rootElementId, formFilledData);
        break;
      }
      case "FormAutofill:UpdateWarningMessage":
        this.notifyMessageObservers("updateWarningNote", data);
        break;

      case "FormAutofill:FieldsDetected":
        this.onFormDetected(data);
        break;
      case "FormAutofill:FieldsIdentified":
        this.notifyMessageObservers("fieldsIdentified", data);
        break;
      case "FormAutofill:FieldFilledModified": {
        this.onFieldFilledModified(data);
        break;
      }

      // The remaining Save and Remove messages are invoked only by tests.
      case "FormAutofill:SaveAddress": {
        if (data.guid) {
          await lazy.gFormAutofillStorage.addresses.update(
            data.guid,
            data.address
          );
        } else {
          await lazy.gFormAutofillStorage.addresses.add(data.address);
        }
        break;
      }
      case "FormAutofill:SaveCreditCard": {
        // Setting the first parameter of OSKeyStore.ensurLoggedIn as false
        // since this case only called in tests. Also the reason why we're not calling FormAutofill.verifyUserOSAuth.
        if (!(await lazy.OSKeyStore.ensureLoggedIn(false)).authenticated) {
          lazy.log.warn("User canceled encryption login");
          return undefined;
        }
        await lazy.gFormAutofillStorage.creditCards.add(data.creditcard);
        break;
      }
      case "FormAutofill:RemoveAddresses": {
        data.guids.forEach(guid =>
          lazy.gFormAutofillStorage.addresses.remove(guid)
        );
        break;
      }
      case "FormAutofill:RemoveCreditCards": {
        data.guids.forEach(guid =>
          lazy.gFormAutofillStorage.creditCards.remove(guid)
        );
        break;
      }
    }

    return undefined;
  }

  get formOrigin() {
    return lazy.LoginHelper.getLoginOrigin(
      this.manager.documentPrincipal?.originNoSuffix
    );
  }

  onFormDetected(fields) {
    if (!fields?.length) {
      return;
    }

    const sections = lazy.FormAutofillSection.classifySections(fields);

    // This function is not only called when a form is detected,
    // but also called when the elements in a form are changed, which means we would
    // treat the "updated" section as a new detected section.
    sections.forEach(section => section.onDetected());

    const rootElementId = fields[0].rootElementId;
    this.sectionsByRootId[rootElementId] = sections;
  }

  notifyMessageObservers(callbackName, data) {
    for (let observer of gMessageObservers) {
      try {
        if (callbackName in observer) {
          observer[callbackName](
            data,
            this.manager.browsingContext.topChromeWindow
          );
        }
      } catch (ex) {
        console.error(ex);
      }
    }
  }

  /**
   * Get the records from profile store and return results back to content
   * process. It will decrypt the credit card number and append
   * "cc-number-decrypted" to each record if OSKeyStore isn't set.
   *
   * This is static as a unit test calls this.
   *
   * @param  {object} data
   * @param  {string} data.searchString
   *         The typed string for filtering out the matched records.
   * @param  {string} data.collectionName
   *         The name used to specify which collection to retrieve records.
   * @param  {string} data.fieldName
   *         The field name to search. If not specified, return all records in
   *         the collection
   */
  static async getRecords({ searchString, collectionName, fieldName }) {
    // Derive the collection name from field name if it doesn't exist
    collectionName ||=
      FormAutofillUtils.getCollectionNameFromFieldName(fieldName);

    const collection = lazy.gFormAutofillStorage[collectionName];
    if (!collection) {
      return [];
    }

    const records = await collection.getAll();
    if (!fieldName || !records.length) {
      return records;
    }

    // We don't filter "cc-number"
    if (collectionName == CREDITCARDS_COLLECTION_NAME) {
      if (fieldName == "cc-number") {
        return records.filter(record => !!record["cc-number"]);
      }
    }

    const lcSearchString = searchString.toLowerCase();
    return records.filter(record => {
      const fieldValue = record[fieldName];
      if (!fieldValue) {
        return false;
      }

      if (
        collectionName == ADDRESSES_COLLECTION_NAME &&
        record.country &&
        !FormAutofill.isAutofillAddressesAvailableInCountry(record.country)
      ) {
        // Address autofill isn't supported for the record's country so we don't
        // want to attempt to potentially incorrectly fill the address fields.
        return false;
      }

      return (
        !lcSearchString ||
        String(fieldValue).toLowerCase().startsWith(lcSearchString)
      );
    });
  }

  /*
   * Capture-related functions
   */

  async _onAddressSubmit(address, browser) {
    const storage = lazy.gFormAutofillStorage.addresses;

    // Make sure record is normalized before comparing with records in the storage
    try {
      storage._normalizeRecord(address.record);
    } catch (_e) {
      return false;
    }

    const newAddress = new lazy.AddressComponent(
      address.record,
      // Invalid address fields in the address form will not be captured.
      { ignoreInvalid: true }
    );

    // Exams all stored record to determine whether to show the prompt or not.
    let mergeableFields = [];
    let preserveFields = [];
    let oldRecord = {};

    for (const record of await storage.getAll()) {
      const savedAddress = new lazy.AddressComponent(record);
      // filter invalid field
      const result = newAddress.compare(savedAddress);

      // If any of the fields in the new address are different from the corresponding fields
      // in the saved address, the two addresses are considered different. For example, if
      // the name, email, country are the same but the street address is different, the two
      // addresses are not considered the same.
      if (Object.values(result).includes("different")) {
        continue;
      }

      // If none of the fields in the new address are mergeable, the new address is considered
      // a duplicate of a local address. Therefore, we don't need to capture this address.
      const fields = Object.entries(result)
        .filter(v => ["superset", "similar"].includes(v[1]))
        .map(v => v[0]);
      if (!fields.length) {
        lazy.log.debug(
          "A duplicated address record is found, do not show the prompt"
        );
        storage.notifyUsed(record.guid);
        return false;
      }

      // If the new address is neither a duplicate of the saved address nor a different address.
      // There must be at least one field we can merge, show the update doorhanger
      lazy.log.debug(
        "A mergeable address record is found, show the update prompt"
      );

      // If one record has fewer mergeable fields compared to another, it suggests greater similarity
      // to the merged record. In such cases, we opt for the record with the fewest mergeable fields.
      // TODO: Bug 1830841. Add a testcase
      if (!mergeableFields.length || mergeableFields > fields.length) {
        mergeableFields = fields;
        preserveFields = Object.entries(result)
          .filter(v => ["same", "subset"].includes(v[1]))
          .map(v => v[0]);
        oldRecord = record;
      }
    }

    // Find a mergeable old record, construct the new record by only copying mergeable fields
    // from the new address.
    let newRecord = {};
    if (mergeableFields.length) {
      // TODO: This is only temporarily, should be removed after Bug 1836438 is fixed
      if (mergeableFields.includes("name")) {
        mergeableFields.push("given-name", "additional-name", "family-name");
      }
      mergeableFields.forEach(f => {
        if (f in newAddress.record) {
          newRecord[f] = newAddress.record[f];
        }
      });

      if (preserveFields.includes("name")) {
        preserveFields.push("given-name", "additional-name", "family-name");
      }
      preserveFields.forEach(f => {
        if (f in oldRecord) {
          newRecord[f] = oldRecord[f];
        }
      });
    } else {
      newRecord = newAddress.record;
    }

    if (!this._shouldShowSaveAddressPrompt(newAddress.record)) {
      return false;
    }

    return async () => {
      await lazy.FormAutofillPrompter.promptToSaveAddress(
        browser,
        storage,
        address.flowId,
        { oldRecord, newRecord }
      );
    };
  }

  async _onCreditCardSubmit(creditCard, browser) {
    const storage = lazy.gFormAutofillStorage.creditCards;

    // Make sure record is normalized before comparing with records in the storage
    try {
      storage._normalizeRecord(creditCard.record);
    } catch (_e) {
      return false;
    }

    // If the record alreay exists in the storage, don't bother showing the prompt
    const matchRecord = (
      await storage.getMatchRecords(creditCard.record).next()
    ).value;
    if (matchRecord) {
      storage.notifyUsed(matchRecord.guid);
      return false;
    }

    // Suppress the pending doorhanger from showing up if user disabled credit card in previous doorhanger.
    if (!FormAutofill.isAutofillCreditCardsEnabled) {
      return false;
    }

    // Overwrite the guid if there is a duplicate
    const duplicateRecord =
      (await storage.getDuplicateRecords(creditCard.record).next()).value ?? {};

    return async () => {
      await lazy.FormAutofillPrompter.promptToSaveCreditCard(
        browser,
        storage,
        creditCard.flowId,
        { oldRecord: duplicateRecord, newRecord: creditCard.record }
      );
    };
  }

  async _onFormSubmit(rootElementId, formFilledData) {
    const browser = this.manager.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }

    const sections = this.sectionsByRootId[rootElementId];
    if (!sections) {
      return;
    }

    const address = [];
    const creditCard = [];

    for (const section of sections) {
      const secRecord = section.createRecord(formFilledData);
      if (!secRecord) {
        continue;
      }

      if (section instanceof lazy.FormAutofillAddressSection) {
        address.push(secRecord);
      } else if (section instanceof lazy.FormAutofillCreditCardSection) {
        creditCard.push(secRecord);
      } else {
        throw new Error("Unknown section type");
      }

      // Used for telemetry
      section.onSubmitted(formFilledData);
    }

    // Transmit the telemetry immediately in the meantime form submitted, and handle these pending
    // doorhangers at a later.
    await Promise.all(
      [
        await Promise.all(
          address.map(addrRecord => this._onAddressSubmit(addrRecord, browser))
        ),
        await Promise.all(
          creditCard.map(ccRecord =>
            this._onCreditCardSubmit(ccRecord, browser)
          )
        ),
      ]
        .map(pendingDoorhangers => {
          return pendingDoorhangers.filter(
            pendingDoorhanger =>
              !!pendingDoorhanger && typeof pendingDoorhanger == "function"
          );
        })
        .map(pendingDoorhangers =>
          (async () => {
            for (const showDoorhanger of pendingDoorhangers) {
              await showDoorhanger();
            }
          })()
        )
    );
  }

  _shouldShowSaveAddressPrompt(record) {
    if (!FormAutofill.isAutofillAddressesCaptureEnabled) {
      return false;
    }

    // Do not save address for regions that we don't support
    if (
      FormAutofill._isAutofillAddressesAvailable == "detect" &&
      !FormAutofill.isAutofillAddressesAvailableInCountry(record.country)
    ) {
      lazy.log.debug(
        `Do not show the address capture prompt for unsupported regions - ${record.country}`
      );
      return false;
    }

    // Display the address capture doorhanger only when the submitted form contains all
    // the required fields. This approach is implemented to prevent excessive prompting.
    let requiredFields = FormAutofill.addressCaptureRequiredFields;
    requiredFields ??=
      FormAutofillUtils.getFormFormat(record.country).countryRequiredFields ??
      [];

    if (!requiredFields.every(field => field in record)) {
      lazy.log.debug(
        "Do not show the address capture prompt when the submitted form doesn't contain all the required fields"
      );
      return false;
    }

    return true;
  }

  /*
   * AutoComplete-related functions
   */

  /**
   * Retrieves autocomplete entries for a given search string and data context.
   *
   * @param {string} searchString
   *                 The search string used to filter autocomplete entries.
   * @param {object} options
   * @param {string} options.fieldName
   *                 The name of the field for which autocomplete entries are being fetched.
   * @param {string} options.elementId
   *                 The id of the element for which we are searching for an autocomplete entry.
   * @param {string} options.scenarioName
   *                 The scenario name used in the autocomplete operation to fetch external entries.
   * @returns {Promise<object>} A promise that resolves to an object containing two properties: `records` and `externalEntries`.
   *         `records` is an array of autofill records from the form's internal data, sorted by `timeLastUsed`.
   *         `externalEntries` is an array of external autocomplete items fetched based on the scenario.
   *         `allFieldNames` is an array containing all the matched field name found in this section.
   */
  async searchAutoCompleteEntries(searchString, options) {
    const { fieldName, elementId, scenarioName } = options;

    const section = this.getSectionByElementId(elementId);
    if (!section.isValidSection() || !section.isEnabled()) {
      return null;
    }

    const relayPromise = lazy.FirefoxRelay.autocompleteItemsAsync({
      formOrigin: this.formOrigin,
      scenarioName,
      hasInput: !!searchString?.length,
    });

    // Retrieve information for the autocomplete entry
    const recordsPromise = FormAutofillParent.getRecords({
      searchString,
      fieldName,
    });

    const [records, externalEntries] = await Promise.all([
      recordsPromise,
      relayPromise,
    ]);

    // Sort addresses by timeLastUsed for showing the lastest used address at top.
    records.sort((a, b) => b.timeLastUsed - a.timeLastUsed);
    return { records, externalEntries, allFieldNames: section.allFieldNames };
  }

  /**
   * This function is called when an autocomplete entry that is provided by
   * formautofill is selected by the user.
   */
  async onAutoCompleteEntrySelected(message, data) {
    switch (message) {
      case "FormAutofill:OpenPreferences": {
        const win = lazy.BrowserWindowTracker.getTopWindow();
        win.openPreferences("privacy-form-autofill");
        break;
      }

      case "FormAutofill:ClearForm": {
        this.clearForm(data.focusElementId);
        break;
      }

      case "FormAutofill:FillForm": {
        this.autofillFields(data.focusElementId, data.profile);
        break;
      }

      default: {
        lazy.log.debug("Unsupported autocomplete message:", message);
        break;
      }
    }
  }

  onAutoCompletePopupOpened(elementId) {
    const section = this.getSectionByElementId(elementId);
    section?.onPopupOpened(elementId);
  }

  onAutoCompleteEntryClearPreview(message, data) {
    this.previewFields(data.focusElementId, null);
  }

  onAutoCompleteEntryHovered(message, data) {
    if (message == "FormAutofill:FillForm") {
      this.previewFields(data.focusElementId, data.profile);
    } else {
      // Make sure the preview is cleared when users select an entry
      // that doesn't support preview.
      this.previewFields(data.focusElementId, null);
    }
  }

  clearForm(elementId) {
    const section = this.getSectionByElementId(elementId);

    section.onCleared(elementId);

    const ids = section.fieldDetails.map(detail => detail.elementId);
    this.sendAsyncMessage("FormAutofill:ClearFilledFields", ids);
  }

  async previewFields(elementId, profile) {
    const section = this.getSectionByElementId(elementId);
    if (!(await section.preparePreviewProfile(profile))) {
      lazy.log.debug("profile cannot be previewed");
      return false;
    }

    const ids = section.fieldDetails.map(detail => detail.elementId);

    try {
      if (profile) {
        await this.sendQuery("FormAutofill:PreviewFields", { ids, profile });
      } else {
        await this.sendQuery("FormAutofill:ClearPreviewedFields", { ids });
      }
    } catch (e) {
      console.error("There was an error previewing: ", e.message);
    }

    // For testing only
    Services.obs.notifyObservers(null, "formautofill-preview-complete");
    return true;
  }

  async autofillFields(elementId, profile) {
    const section = this.getSectionByElementId(elementId);
    if (!(await section.prepareFillingProfile(profile))) {
      lazy.log.debug("profile cannot be filled");
      return;
    }

    const ids = section.fieldDetails.map(detail => detail.elementId);

    let filledResult = new Map();
    try {
      filledResult = await this.sendQuery("FormAutofill:FillFields", {
        focusedElementId: elementId,
        ids,
        profile,
      });

      this.filledResult = this.filledResult ?? new Map();
      filledResult.forEach((value, key) => this.filledResult.set(key, value));

      section.onFilled(filledResult);
    } catch (e) {
      console.error("There was an error autofilling: ", e.message);
    }

    // eslint-disable-next-line consistent-return
    return filledResult;
  }

  onFieldFilledModified(elementId) {
    if (!this.filledResult?.get(elementId)) {
      return;
    }

    this.filledResult.get(elementId).filledState = FIELD_STATES.NORMAL;

    const section = this.getSectionByElementId(elementId);

    // For telemetry
    section?.onFilledModified(elementId);

    // Restore <select> fields to their initial state once we know
    // that the user intends to manually clear the filled form.
    const fieldDetails = section.fieldDetails;
    const selects = fieldDetails.filter(field => field.tagName == "SELECT");
    if (selects.length) {
      const inputs = fieldDetails.filter(field => field.tagName == "INPUT");
      if (
        inputs.every(
          field =>
            this.filledResult.get(field.elementId).filledState ==
            FIELD_STATES.NORMAL
        )
      ) {
        const ids = selects.map(field => field.elementId);
        this.sendAsyncMessage("FormAutofill:ClearFilledFields", ids);
      }
    }
  }

  getSectionByElementId(elementId) {
    for (const sections of Object.values(this.sectionsByRootId)) {
      const section = sections.find(s =>
        s.getFieldDetailByElementId(elementId)
      );
      if (section) {
        return section;
      }
    }
    return null;
  }

  // For testing
  getSections() {
    return Object.values(this.sectionsByRootId).flat();
  }
}
