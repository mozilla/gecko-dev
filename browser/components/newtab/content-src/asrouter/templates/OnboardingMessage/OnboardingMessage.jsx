import {ModalOverlay} from "../../components/ModalOverlay/ModalOverlay";
import React from "react";

class OnboardingCard extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
  }

  onClick() {
    const {props} = this;
    const ping = {
      event: "CLICK_BUTTON",
      message_id: props.id,
      id: props.UISurface,
    };
    props.sendUserActionTelemetry(ping);
    props.onAction(props.content.primary_button.action);
  }

  render() {
    const {content} = this.props;
    return (
      <div className="onboardingMessage">
        <div className={`onboardingMessageImage ${content.icon}`} />
        <div className="onboardingContent">
          <span>
            <h3> {content.title} </h3>
            <p> {content.text} </p>
          </span>
          <span>
            <button tabIndex="1" className="button onboardingButton" onClick={this.onClick}> {content.primary_button.label} </button>
          </span>
        </div>
      </div>
    );
  }
}

export class OnboardingMessage extends React.PureComponent {
  render() {
    const {props} = this;
    const {button_label, header} = props.extraTemplateStrings;
    return (
      <ModalOverlay {...props} button_label={button_label} title={header}>
        <div className="onboardingMessageContainer">
          {props.bundle.map(message => (
            <OnboardingCard key={message.id}
              sendUserActionTelemetry={props.sendUserActionTelemetry}
              onAction={props.onAction}
              UISurface={props.UISurface}
              {...message} />
          ))}
        </div>
      </ModalOverlay>
    );
  }
}
