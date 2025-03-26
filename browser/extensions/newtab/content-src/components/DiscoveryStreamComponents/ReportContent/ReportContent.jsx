/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
import React, { useRef, useEffect, useCallback, useState } from "react";
import { useSelector, useDispatch } from "react-redux";
import { actionTypes as at, actionCreators as ac } from "common/Actions.mjs";

export const ReportContent = () => {
  const dispatch = useDispatch();
  const modal = useRef(null);
  const radioGroupRef = useRef(null);
  const submitButtonRef = useRef(null);
  const report = useSelector(state => state.DiscoveryStream.report);
  const [valueSelected, setValueSelected] = useState(false);

  // Sends a dispatch to update the redux store when modal is cancelled
  const handleCancel = useCallback(() => {
    dispatch(
      ac.BroadcastToContent({
        type: at.REPORT_CLOSE,
      })
    );
  }, [dispatch]);

  const handleSubmit = useCallback(e => {
    e.preventDefault();
  }, []);

  // Opens and closes the modal based on user interaction
  useEffect(() => {
    if (report.visible && modal?.current) {
      modal.current.showModal();
    } else if (!report.visible && modal?.current?.open) {
      modal.current.close();
    }
  }, [report.visible]);

  // Updates the submit button's state based on if a value is selected
  useEffect(() => {
    const radioGroup = radioGroupRef.current;
    const submitButton = submitButtonRef.current;

    const handleRadioChange = () => setValueSelected(true);

    if (radioGroup) {
      radioGroup.addEventListener("change", handleRadioChange);
    }

    // Handle submit button state on valueSelected change
    const updateSubmitState = () => {
      if (valueSelected) {
        submitButton.removeAttribute("disabled");
      } else {
        submitButton.setAttribute("disabled", "");
      }
    };

    updateSubmitState();

    return () => {
      if (radioGroup) {
        radioGroup.removeEventListener("change", handleRadioChange);
      }
    };
  }, [valueSelected]);

  return (
    <dialog
      className="report-content-form"
      id="dialog-report"
      ref={modal}
      onClose={() => dispatch({ type: at.REPORT_CLOSE })}
    >
      <form action="">
        <moz-radio-group
          name="report"
          ref={radioGroupRef}
          id="report-group"
          data-l10n-id="newtab-report-ads-why-reporting"
        >
          <moz-radio
            value="unsafe"
            data-l10n-id="newtab-report-ads-reason-unsafe"
          ></moz-radio>
          <moz-radio
            data-l10n-id="newtab-report-ads-reason-inappropriate"
            value="inappropriate"
          ></moz-radio>
          <moz-radio
            data-l10n-id="newtab-report-ads-reason-seen-it-too-many-times"
            value="too-many"
          ></moz-radio>
        </moz-radio-group>

        <moz-button-group>
          <moz-button
            data-l10n-id="newtab-topic-selection-cancel-button"
            onClick={handleCancel}
          ></moz-button>

          <moz-button
            type="primary"
            data-l10n-id="newtab-report-submit"
            ref={submitButtonRef}
            onClick={handleSubmit}
          ></moz-button>
        </moz-button-group>
      </form>
    </dialog>
  );
};
