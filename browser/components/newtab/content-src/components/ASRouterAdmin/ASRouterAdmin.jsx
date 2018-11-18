import {ASRouterUtils} from "../../asrouter/asrouter-content";
import {ModalOverlay} from "../../asrouter/components/ModalOverlay/ModalOverlay";
import React from "react";

export class ASRouterAdmin extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onMessage = this.onMessage.bind(this);
    this.handleEnabledToggle = this.handleEnabledToggle.bind(this);
    this.handleUserPrefToggle = this.handleUserPrefToggle.bind(this);
    this.onChangeMessageFilter = this.onChangeMessageFilter.bind(this);
    this.findOtherBundledMessagesOfSameTemplate = this.findOtherBundledMessagesOfSameTemplate.bind(this);
    this.handleExpressionEval = this.handleExpressionEval.bind(this);
    this.onChangeTargetingParameters = this.onChangeTargetingParameters.bind(this);
    this.onCopyTargetingParams = this.onCopyTargetingParams.bind(this);
    this.onPasteTargetingParams = this.onPasteTargetingParams.bind(this);
    this.onNewTargetingParams = this.onNewTargetingParams.bind(this);
    this.state = {
      messageFilter: "all",
      evaluationStatus: {},
      stringTargetingParameters: null,
      newStringTargetingParameters: null,
      copiedToClipboard: false,
      pasteFromClipboard: false,
    };
  }

  onMessage({data: action}) {
    if (action.type === "ADMIN_SET_STATE") {
      this.setState(action.data);
      if (!this.state.stringTargetingParameters) {
        const stringTargetingParameters = {};
        for (const param of Object.keys(action.data.targetingParameters)) {
          stringTargetingParameters[param] = JSON.stringify(action.data.targetingParameters[param], null, 2);
        }
        this.setState({stringTargetingParameters});
      }
    }
  }

  componentWillMount() {
    const endpoint = ASRouterUtils.getPreviewEndpoint();
    ASRouterUtils.sendMessage({type: "ADMIN_CONNECT_STATE", data: {endpoint}});
    ASRouterUtils.addListener(this.onMessage);
  }

  componentWillUnmount() {
    ASRouterUtils.removeListener(this.onMessage);
  }

  findOtherBundledMessagesOfSameTemplate(template) {
    return this.state.messages.filter(msg => msg.template === template && msg.bundled);
  }

  handleBlock(msg) {
    if (msg.bundled) {
      // If we are blocking a message that belongs to a bundle, block all other messages that are bundled of that same template
      let bundle = this.findOtherBundledMessagesOfSameTemplate(msg.template);
      return () => ASRouterUtils.blockBundle(bundle);
    }
    return () => ASRouterUtils.blockById(msg.id);
  }

  handleUnblock(msg) {
    if (msg.bundled) {
      // If we are unblocking a message that belongs to a bundle, unblock all other messages that are bundled of that same template
      let bundle = this.findOtherBundledMessagesOfSameTemplate(msg.template);
      return () => ASRouterUtils.unblockBundle(bundle);
    }
    return () => ASRouterUtils.unblockById(msg.id);
  }

  handleOverride(id) {
    return () => ASRouterUtils.overrideMessage(id);
  }

  expireCache() {
    ASRouterUtils.sendMessage({type: "EXPIRE_QUERY_CACHE"});
  }

  resetPref() {
    ASRouterUtils.sendMessage({type: "RESET_PROVIDER_PREF"});
  }

  handleExpressionEval() {
    const context = {};
    for (const param of Object.keys(this.state.stringTargetingParameters)) {
      const value = this.state.stringTargetingParameters[param];
      context[param] = value ? JSON.parse(value) : null;
    }
    ASRouterUtils.sendMessage({
      type: "EVALUATE_JEXL_EXPRESSION",
      data: {
        expression: this.refs.expressionInput.value,
        context,
      },
    });
  }

  onChangeTargetingParameters(event) {
    const {name} = event.target;
    const {value} = event.target;

    this.setState(({stringTargetingParameters}) => {
      let targetingParametersError = null;
      const updatedParameters = {...stringTargetingParameters};
      updatedParameters[name] = value;
      try {
        JSON.parse(value);
      } catch (e) {
        console.log(`Error parsing value of parameter ${name}`); // eslint-disable-line no-console
        targetingParametersError = {id: name};
      }

      return {
        copiedToClipboard: false,
        evaluationStatus: {},
        stringTargetingParameters: updatedParameters,
        targetingParametersError,
      };
    });
  }

  handleEnabledToggle(event) {
    const provider = this.state.providerPrefs.find(p => p.id === event.target.dataset.provider);
    const userPrefInfo = this.state.userPrefs;

    const isUserEnabled = provider.id in userPrefInfo ? userPrefInfo[provider.id] : true;
    const isSystemEnabled = provider.enabled;
    const isEnabling = event.target.checked;

    if (isEnabling) {
      if (!isUserEnabled) {
        ASRouterUtils.sendMessage({type: "SET_PROVIDER_USER_PREF", data: {id: provider.id, value: true}});
      }
      if (!isSystemEnabled) {
        ASRouterUtils.sendMessage({type: "ENABLE_PROVIDER", data: provider.id});
      }
    } else {
      ASRouterUtils.sendMessage({type: "DISABLE_PROVIDER", data: provider.id});
    }

    this.setState({messageFilter: "all"});
  }

  handleUserPrefToggle(event) {
    const action = {type: "SET_PROVIDER_USER_PREF", data: {id: event.target.dataset.provider, value: event.target.checked}};
    ASRouterUtils.sendMessage(action);
    this.setState({messageFilter: "all"});
  }

  onChangeMessageFilter(event) {
    this.setState({messageFilter: event.target.value});
  }

  // Simulate a copy event that sets to clipboard all targeting paramters and values
  onCopyTargetingParams(event) {
    const stringTargetingParameters = {...this.state.stringTargetingParameters};
    for (const key of Object.keys(stringTargetingParameters)) {
      // If the value is not set the parameter will be lost when we stringify
      if (stringTargetingParameters[key] === undefined) {
        stringTargetingParameters[key] = null;
      }
    }
    const setClipboardData = e => {
      e.preventDefault();
      e.clipboardData.setData("text", JSON.stringify(stringTargetingParameters, null, 2));
      document.removeEventListener("copy", setClipboardData);
      this.setState({copiedToClipboard: true});
    };

    document.addEventListener("copy", setClipboardData);

    document.execCommand("copy");
  }

  // Copy all clipboard data to targeting parameters
  onPasteTargetingParams(event) {
    this.setState(({pasteFromClipboard}) => ({
      pasteFromClipboard: !pasteFromClipboard,
      newStringTargetingParameters: "",
    }));
  }

  onNewTargetingParams(event) {
    this.setState({newStringTargetingParameters: event.target.value});
    event.target.classList.remove("errorState");
    this.refs.targetingParamsEval.innerText = "";

    try {
      const stringTargetingParameters = JSON.parse(event.target.value);
      this.setState({stringTargetingParameters});
    } catch (e) {
      event.target.classList.add("errorState");
      this.refs.targetingParamsEval.innerText = e.message;
    }
  }

  renderMessageItem(msg) {
    const isCurrent = msg.id === this.state.lastMessageId;
    const isBlocked = this.state.messageBlockList.includes(msg.id) || this.state.messageBlockList.includes(msg.campaign);
    const impressions = this.state.messageImpressions[msg.id] ? this.state.messageImpressions[msg.id].length : 0;

    let itemClassName = "message-item";
    if (isCurrent) { itemClassName += " current"; }
    if (isBlocked) { itemClassName += " blocked"; }

    return (<tr className={itemClassName} key={msg.id}>
      <td className="message-id"><span>{msg.id} <br /></span></td>
      <td>
        <button className={`button ${(isBlocked ? "" : " primary")}`} onClick={isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg)}>{isBlocked ? "Unblock" : "Block"}</button>
       {isBlocked ? null : <button className="button" onClick={this.handleOverride(msg.id)}>Show</button>}
       <br />({impressions} impressions)
      </td>
      <td className="message-summary">
        <pre>{JSON.stringify(msg, null, 2)}</pre>
      </td>
    </tr>);
  }

  renderMessages() {
    if (!this.state.messages) {
      return null;
    }
    const messagesToShow = this.state.messageFilter === "all" ? this.state.messages : this.state.messages.filter(message => message.provider === this.state.messageFilter);
    return (<table><tbody>
      {messagesToShow.map(msg => this.renderMessageItem(msg))}
    </tbody></table>);
  }

  renderMessageFilter() {
    if (!this.state.providers) {
      return null;
    }
    return (<p>Show messages from <select value={this.state.messageFilter} onChange={this.onChangeMessageFilter}>
      <option value="all">all providers</option>
      {this.state.providers.map(provider => (<option key={provider.id} value={provider.id}>{provider.id}</option>))}
    </select></p>);
  }

  renderTableHead() {
    return (<thead>
      <tr className="message-item">
        <td className="min" />
        <td className="min">Provider ID</td>
        <td>Source</td>
        <td className="min">Cohort</td>
        <td className="min">Last Updated</td>
      </tr>
    </thead>);
  }

  renderProviders() {
    const providersConfig = this.state.providerPrefs;
    const providerInfo = this.state.providers;
    const userPrefInfo = this.state.userPrefs;

    return (<table>{this.renderTableHead()}<tbody>
      {providersConfig.map((provider, i) => {
        const isTestProvider = provider.id === "snippets_local_testing";
        const info = providerInfo.find(p => p.id === provider.id) || {};
        const isUserEnabled = provider.id in userPrefInfo ? userPrefInfo[provider.id] : true;
        const isSystemEnabled = (isTestProvider || provider.enabled);

        let label = "local";
        if (provider.type === "remote") {
          label = (<span>endpoint (<a className="providerUrl" target="_blank" href={info.url}>{info.url}</a>)</span>);
        } else if (provider.type === "remote-settings") {
          label = `remote settings (${provider.bucket})`;
        }

        let reasonsDisabled = [];
        if (!isSystemEnabled) {
          reasonsDisabled.push("system pref");
        }
        if (!isUserEnabled) {
          reasonsDisabled.push("user pref");
        }
        if (reasonsDisabled.length) {
          label = `disabled via ${reasonsDisabled.join(", ")}`;
        }

        return (<tr className="message-item" key={i}>

          <td>{isTestProvider ? <input type="checkbox" disabled={true} readOnly={true} checked={true} /> : <input type="checkbox" data-provider={provider.id} checked={isUserEnabled && isSystemEnabled} onChange={this.handleEnabledToggle} />}</td>
          <td>{provider.id}</td>
          <td><span className={`sourceLabel${(isUserEnabled && isSystemEnabled) ? "" : " isDisabled"}`}>{label}</span></td>
          <td>{provider.cohort}</td>
          <td style={{whiteSpace: "nowrap"}}>{info.lastUpdated ? new Date(info.lastUpdated).toLocaleString() : ""}</td>
        </tr>);
      })}
    </tbody></table>);
  }

  renderPasteModal() {
    if (!this.state.pasteFromClipboard) {
      return null;
    }
    const errors = this.refs.targetingParamsEval && this.refs.targetingParamsEval.innerText.length;
    return (
      <ModalOverlay title="New targeting parameters" button_label={errors ? "Cancel" : "Done"} onDoneButton={this.onPasteTargetingParams}>
        <div className="onboardingMessage">
          <p>
            <textarea onChange={this.onNewTargetingParams} value={this.state.newStringTargetingParameters} autoFocus={true} rows="20" cols="60" />
          </p>
          <p ref="targetingParamsEval" />
        </div>
      </ModalOverlay>
    );
  }

  renderTargetingParameters() {
    // There was no error and the result is truthy
    const success = this.state.evaluationStatus.success && !!this.state.evaluationStatus.result;
    const result = JSON.stringify(this.state.evaluationStatus.result, null, 2) || "(Empty result)";

    return (<table><tbody>
      <tr><td><h2>Evaluate JEXL expression</h2></td></tr>
      <tr>
        <td>
          <p><textarea ref="expressionInput" rows="10" cols="60" placeholder="Evaluate JEXL expressions and mock parameters by changing their values below" /></p>
          <p>Status: <span ref="evaluationStatus">{success ? "✅" : "❌"}, Result: {result}</span></p>
        </td>
        <td>
           <button className="ASRouterButton secondary" onClick={this.handleExpressionEval}>Evaluate</button>
        </td>
      </tr>
      <tr><td><h2>Modify targeting parameters</h2></td></tr>
      <tr>
        <td>
          <button className="ASRouterButton secondary" onClick={this.onCopyTargetingParams} disabled={this.state.copiedToClipboard}>
            {this.state.copiedToClipboard ? "Parameters copied!" : "Copy parameters"}
          </button>
          <button className="ASRouterButton secondary" onClick={this.onPasteTargetingParams} disabled={this.state.pasteFromClipboard}>
            Paste parameters
          </button>
        </td>
      </tr>
      {this.state.stringTargetingParameters && Object.keys(this.state.stringTargetingParameters).map((param, i) => {
        const value = this.state.stringTargetingParameters[param];
        const errorState = this.state.targetingParametersError && this.state.targetingParametersError.id === param;
        const className = errorState ? "errorState" : "";
        const inputComp = (value && value.length) > 30 ?
          <textarea name={param} className={className} value={value} rows="10" cols="60" onChange={this.onChangeTargetingParameters} /> :
          <input name={param} className={className} value={value} onChange={this.onChangeTargetingParameters} />;

        return (<tr key={i}>
          <td>{param}</td>
          <td>{inputComp}</td>
          </tr>);
      })}
      </tbody></table>);
  }

  render() {
    return (<div className="asrouter-admin outer-wrapper">
      <h1>AS Router Admin</h1>
      <h2>Targeting Utilities</h2>
      <button className="button" onClick={this.expireCache}>Expire Cache</button> (This expires the cache in ASR Targeting for bookmarks and top sites)
      <h2>Message Providers <button title="Restore all provider settings that ship with Firefox" className="button" onClick={this.resetPref}>Restore default prefs</button></h2>

      {this.state.providers ? this.renderProviders() : null}
      <h2>Messages</h2>
      {this.renderMessageFilter()}
      {this.renderMessages()}
      {this.renderPasteModal()}
      {this.renderTargetingParameters()}
    </div>);
  }
}
