import React from "react";
import schema from "./FXASignupSnippet.schema.json";
import {SubmitFormSnippet} from "../SubmitFormSnippet/SubmitFormSnippet.jsx";

export const FXASignupSnippet = props => {
  const userAgent = window.navigator.userAgent.match(/Firefox\/([0-9]+)\./);
  const firefox_version = userAgent ? parseInt(userAgent[1], 10) : 0;
  const extendedContent = {
    form_action: "https://accounts.firefox.com/",
    scene1_button_label: schema.properties.scene1_button_label.default,
    scene2_email_placeholder_text: schema.properties.scene2_email_placeholder_text.default,
    scene2_button_label: schema.properties.scene2_button_label.default,
    ...props.content,
    hidden_inputs: {
      action: "email",
      context: "fx_desktop_v3",
      entrypoint: "snippets",
      service: "sync",
      utm_source: "snippet",
      utm_content: firefox_version,
      utm_campaign: props.content.utm_campaign,
      utm_term: props.content.utm_term,
      ...props.content.hidden_inputs,
    },
  };

  return (<SubmitFormSnippet
    {...props}
    content={extendedContent}
    form_method="GET" />);
};
