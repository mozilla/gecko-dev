/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  createEngine: "chrome://global/content/ml/EngineProcess.sys.mjs",
  MLEngineParent: "resource://gre/actors/MLEngineParent.sys.mjs",
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
});

export class MLAutofill {
  static engine = null;
  static modelRevision = null;

  static async initialize() {
    if (!lazy.FormAutofill.isMLExperimentEnabled) {
      return;
    }
    if (MLAutofill.engine) {
      return;
    }

    const config = MLAutofill.readConfig();

    try {
      MLAutofill.engine = await lazy.createEngine(config);
      const options = await lazy.MLEngineParent.getInferenceOptions(
        config.featureId,
        config.taskName
      );
      MLAutofill.modelRevision = options.modelRevision;
    } catch (e) {
      console.error("There was an error initializeing ML engine: ", e.message);
    }
  }

  static readConfig() {
    return {
      taskName: "text-classification",
      featureId: "autofill-classification",
      engineId: "autofill-ml",
      dtype: "int8",
      timeoutMS: -1,
    };
  }

  static run(request) {
    return MLAutofill.engine.run(request);
  }

  static async runInference(fieldDetails) {
    if (!MLAutofill.engine) {
      await MLAutofill.initialize();
    }

    const results = await Promise.all(
      fieldDetails.map(fieldDetail => {
        let markup = fieldDetail.htmlMarkup;
        if (fieldDetail.reason == "autocomplete") {
          // Remove the autocomplete attribute from the element markup to test
          // whether ML detection logic can still correctly identify
          // field types without relying on autocomplete as the source of truth.
          markup = markup.replace(/\s*autocomplete=[^\s/>]*(\s|\/|>)/gi, "$1");
        }
        const request = { args: [markup] };
        return MLAutofill.run(request);
      })
    );

    for (let idx = 0; idx < results.length; idx++) {
      const fieldDetail = fieldDetails[idx];
      const result = results[idx][0];

      const extra = {
        infer_field_name: fieldDetail.fieldName,
        infer_reason: fieldDetail.reason,
        fathom_infer_label:
          fieldDetail.reason == "fathom" ? fieldDetail.fieldName : "",
        fathom_infer_score: fieldDetail?.confidence?.toString() ?? "",
        ml_revision: MLAutofill.modelRevision ?? "",
        ml_infer_label: result?.label || "FAILED",
        ml_infer_score: result?.score?.toString() || "FAILED",
      };
      dump(
        "[Dimi]" + JSON.stringify(result) + " : " + JSON.stringify(extra) + "\n"
      );

      Glean.formautofillMl.fieldInferResult.record(extra);
    }
  }
}
