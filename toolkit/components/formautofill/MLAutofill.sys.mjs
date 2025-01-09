/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  createEngine: "chrome://global/content/ml/EngineProcess.sys.mjs",
  MLEngineParent: "resource://gre/actors/MLEngineParent.sys.mjs",
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "runInAutomation",
  "extensions.formautofill.ml.experiment.runInAutomation"
);

export class MLAutofill {
  static engine = null;
  static modelRevision = null;

  static async initialize() {
    if (
      !lazy.FormAutofill.isMLExperimentEnabled ||
      (Cu.isInAutomation && !lazy.runInAutomation)
    ) {
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
      MLAutofill.engine = null;
    }
  }

  static shutdown() {
    try {
      MLAutofill.engine?.terminate();
    } finally {
      MLAutofill.engine = null;
    }
  }

  static readConfig() {
    return {
      taskName: "text-classification",
      featureId: "autofill-classification",
      engineId: "autofill-ml",
      modelRevision: lazy.FormAutofill.MLModelRevision,
      dtype: "int8",
      timeoutMS: -1,
      numThreads: 2,
    };
  }

  static run(request) {
    return MLAutofill.engine.run(request);
  }

  static async runInference(fieldDetails) {
    if (!MLAutofill.engine) {
      await MLAutofill.initialize();
      if (!MLAutofill.engine) {
        return;
      }
    }

    const results = await Promise.all(
      fieldDetails.map((fieldDetail, index) => {
        const input = MLAutofill.constructMLInput(
          fieldDetail.mlHeaderInput ?? " ",
          fieldDetail.mlinput,
          fieldDetails[index - 1]?.mlinput ?? " ",
          fieldDetails[index + 1]?.mlinput ?? " ",
          fieldDetail.mlButtonInput ?? " "
        );
        const request = { args: [input] };
        return MLAutofill.run(request);
      })
    );

    for (let idx = 0; idx < results.length; idx++) {
      const fieldDetail = fieldDetails[idx];
      const result = results[idx][0];

      const extra = {
        infer_field_name: fieldDetail.fieldName,
        infer_reason: fieldDetail.reason,
        fathom_infer_label: fieldDetail.fathomLabel ?? "",
        fathom_infer_score: fieldDetail.fathomConfidence?.toString() ?? "",
        ml_revision: MLAutofill.modelRevision ?? "",
        ml_infer_label: result?.label || "FAILED",
        ml_infer_score: result?.score?.toString() || "FAILED",
      };

      Glean.formautofillMl.fieldInferResult.record(extra);
    }
  }

  static getLabel(element) {
    let labels = element.labels;
    if (labels !== null && labels.length) {
      return Array.from(labels)
        .map(label => label.textContent.trim())
        .join(" ");
    }

    labels = element.getAttribute("aria-labelledby");
    if (labels !== null && labels.length) {
      labels = labels
        .split(" ")
        .map(id => element.getRootNode().getElementById(id))
        .filter(el => el);
      if (labels.length >= 1) {
        return Array.from(labels)
          .map(label => label.textContent.trim())
          .join(" ");
      }
    }

    const previousElementSibling = element.previousElementSibling;
    if (previousElementSibling?.tagName === "LABEL") {
      return previousElementSibling.textContent;
    }

    const parentElement = element.parentElement;
    // Check if the input is in a <td>, and, if so, check the textContent of the containing <tr>
    if (parentElement?.tagName === "TD" && parentElement?.parentElement) {
      return parentElement.parentElement.textContent;
    }

    if (
      parentElement?.tagName === "DD" &&
      parentElement?.previousElementSibling
    ) {
      return parentElement.previousElementSibling.textContent;
    }

    return "";
  }

  static closestHeaderAbove(elements) {
    const root = elements[0].ownerDocument;
    const headers = Array.from(
      root.querySelectorAll(
        "h1,h2,h3,h4,h5,h6,div[class*=heading],div[class*=header],div[class*=title],legend"
      )
    ).reverse();

    return elements.map(element => {
      const header = headers.find(
        h =>
          h.compareDocumentPosition(element) & Node.DOCUMENT_POSITION_FOLLOWING
      );
      if (header) {
        return header.textContent;
      }
      return "";
    });
  }

  static closestButtonBelow(elements) {
    const root = elements[0].ownerDocument;
    const inputs = Array.from(
      root.querySelectorAll("input[type=submit],input[type=button]")
    );

    return elements.map(element => {
      const button = inputs.find(
        input =>
          input.compareDocumentPosition(element) &
          Node.DOCUMENT_POSITION_PRECEDING
      );
      if (button) {
        return [button.value, button.textContent, button.id, button.title].join(
          " "
        );
      }
      return "";
    });
  }

  static getMLMarkup(element) {
    const label = MLAutofill.getLabel(element);
    return `${element.localName} ${label} ${element.placeholder ?? ""} ${element.className ?? ""} ${element.id ?? ""}`;
  }

  static constructMLInput(header, current, previous, next, button) {
    const sep = `<SEP>`;
    const list = [`${header} ${current}`, previous, next, button];
    let input = "";
    for (const item of list) {
      input += item + sep;
    }
    // Trim and replace multiple spaces with a single space
    return input.trim().replace(/\s{2,}/g, " ");
  }
}
