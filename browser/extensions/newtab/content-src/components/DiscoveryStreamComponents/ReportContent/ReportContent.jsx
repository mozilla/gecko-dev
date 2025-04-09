/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
import React, { useRef, useEffect, useCallback, useState } from "react";
import { useSelector, useDispatch } from "react-redux";
import { actionTypes as at, actionCreators as ac } from "common/Actions.mjs";

export const ReportContent = spocs => {
  const dispatch = useDispatch();
  const modal = useRef(null);
  const radioGroupRef = useRef(null);
  const submitButtonRef = useRef(null);
  const report = useSelector(state => state.DiscoveryStream.report);
  const [valueSelected, setValueSelected] = useState(false);
  const [selectedReason, setSelectedReason] = useState(null);
  const spocData = spocs.spocs.data;

  // Sends a dispatch to update the redux store when modal is cancelled
  const handleCancel = () => {
    dispatch(
      ac.AlsoToMain({
        type: at.REPORT_CLOSE,
      })
    );
  };

  const handleSubmit = useCallback(() => {
    const {
      card_type,
      corpus_item_id,
      is_section_followed,
      position,
      received_rank,
      recommended_at,
      reporting_url,
      scheduled_corpus_item_id,
      section_position,
      section,
      title,
      topic,
      url,
    } = report;

    if (card_type === "organic") {
      dispatch(
        ac.AlsoToMain({
          type: at.REPORT_CONTENT_SUBMIT,
          data: {
            card_type,
            corpus_item_id,
            is_section_followed,
            received_rank,
            recommended_at,
            report_reason: selectedReason,
            scheduled_corpus_item_id,
            section_position,
            section,
            title,
            topic,
            url,
          },
        })
      );
    } else if (card_type === "spoc") {
      // Retrieve placement_id by comparing spocData with the ad that was reported
      const getPlacementId = () => {
        if (!spocData || !report.url) {
          return null;
        }

        for (const [placementId, spocList] of Object.entries(spocData)) {
          for (const spoc of Object.values(spocList)) {
            if (spoc?.url === report.url) {
              return placementId;
            }
          }
        }
        return null;
      };

      const placement_id = getPlacementId();

      dispatch(
        ac.AlsoToMain({
          type: at.REPORT_AD_SUBMIT,
          data: {
            report_reason: selectedReason,
            placement_id,
            position,
            reporting_url,
            url,
          },
        })
      );
    }

    dispatch(
      ac.AlsoToMain({
        type: at.BLOCK_URL,
        data: [{ ...report }],
      })
    );

    dispatch(
      ac.OnlyToOneContent(
        {
          type: at.SHOW_TOAST_MESSAGE,
          data: {
            toastId: "reportSuccessToast",
            showNotifications: true,
          },
        },
        "ActivityStream:Content"
      )
    );
  }, [dispatch, selectedReason, report, spocData]);

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

    const handleRadioChange = e => {
      const reasonValue = e?.target?.value;

      if (reasonValue) {
        setValueSelected(true);
        setSelectedReason(reasonValue);
      }
    };

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
  }, [valueSelected, selectedReason]);

  return (
    <dialog
      className="report-content-form"
      id="dialog-report"
      ref={modal}
      onClose={() => dispatch({ type: at.REPORT_CLOSE })}
    >
      <form action="">
        {/* spocs and stories are going to have different reporting
          options, so placed a conditional to render the different reasons */}
        {report.card_type === "spoc" ? (
          <>
            <moz-radio-group
              name="report"
              ref={radioGroupRef}
              id="report-group"
              data-l10n-id="newtab-report-ads-why-reporting"
            >
              <moz-radio
                data-l10n-id="newtab-report-ads-reason-not-interested"
                value="not_interested"
              ></moz-radio>
              <moz-radio
                data-l10n-id="newtab-report-ads-reason-inappropriate"
                value="inappropriate"
              ></moz-radio>
              <moz-radio
                data-l10n-id="newtab-report-ads-reason-seen-it-too-many-times"
                value="seen_too_many_times"
              ></moz-radio>
            </moz-radio-group>
          </>
        ) : (
          <>
            <moz-radio-group
              name="report"
              ref={radioGroupRef}
              id="report-group"
              data-l10n-id="newtab-report-content-why-reporting"
            >
              <moz-radio
                value="unsafe_content"
                data-l10n-id="newtab-report-ads-reason-unsafe"
              ></moz-radio>
              <moz-radio
                data-l10n-id="newtab-report-ads-reason-inappropriate"
                value="inappropriate_content"
              ></moz-radio>
              <moz-radio
                data-l10n-id="newtab-report-ads-reason-seen-it-too-many-times"
                value="seen_too_many_times"
              ></moz-radio>
            </moz-radio-group>
          </>
        )}

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
