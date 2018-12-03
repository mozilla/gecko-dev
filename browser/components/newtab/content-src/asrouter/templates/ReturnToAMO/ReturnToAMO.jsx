import React from "react";
import {RichText} from "../../components/RichText/RichText";

export class ReturnToAMO extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onClickAddExtension = this.onClickAddExtension.bind(this);
    this.onBlockButton = this.onBlockButton.bind(this);
  }

  componentDidMount() {
    this.props.onReady();
  }

  onClickAddExtension() {
    this.props.onAction(this.props.content.primary_button.action);
    this.props.onBlock();
    document.body.classList.remove("welcome", "hide-main", "amo");
  }

  onBlockButton() {
    this.props.onBlock();
    document.body.classList.remove("welcome", "hide-main", "amo");
  }

  renderText() {
    const customElement = <img src={this.props.content.addon_icon} width="20px" height="20px" />;
    return (<RichText
      customElements={{icon: customElement}}
      amo_html={this.props.content.text}
      localization_id="amo_html" />);
  }

  render() {
    const {content} = this.props;
    return (
      <div className="ReturnToAMOOverlay" >
        <div>
          <h2> {content.header} </h2>
          <div className="ReturnToAMOContainer" >
            <div className="ReturnToAMOAddonContents">
              <p> {content.title} </p>
              <div className="ReturnToAMOText">
                <span> {this.renderText()} </span>
              </div>
              <button onClick={this.onClickAddExtension} className="puffy blue ReturnToAMOAddExtension"> <span className="icon icon-add" /> {content.primary_button.label} </button>
            </div>
            <div className="ReturnToAMOIcon" />
          </div>
          <button onClick={this.onBlockButton} className="default grey ReturnToAMOGetStarted"> {content.secondary_button.label} </button>
        </div>
      </div>);
  }
}
