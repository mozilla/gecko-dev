/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Component } from "resource://devtools/client/shared/vendor/react.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { div } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";
import { createFactories } from "resource://devtools/client/shared/react-utils.mjs";

import SearchBoxClass from "resource://devtools/client/jsonview/components/SearchBox.mjs";

const { SearchBox } = createFactories(SearchBoxClass);
import ToolbarClass from "resource://devtools/client/jsonview/components/reps/Toolbar.mjs";

const { Toolbar, ToolbarButton } = createFactories(ToolbarClass);

/* 100kB file */
const EXPAND_THRESHOLD = 100 * 1024;

/**
 * This template represents a toolbar within the 'JSON' panel.
 */
class JsonToolbar extends Component {
  static get propTypes() {
    return {
      actions: PropTypes.object,
      dataSize: PropTypes.number,
    };
  }

  constructor(props) {
    super(props);
    this.onSave = this.onSave.bind(this);
    this.onCopy = this.onCopy.bind(this);
    this.onCollapse = this.onCollapse.bind(this);
    this.onExpand = this.onExpand.bind(this);
  }

  // Commands

  onSave() {
    this.props.actions.onSaveJson();
  }

  onCopy() {
    this.props.actions.onCopyJson();
  }

  onCollapse() {
    this.props.actions.onCollapse();
  }

  onExpand() {
    this.props.actions.onExpand();
  }

  render() {
    return Toolbar(
      {},
      ToolbarButton(
        { className: "btn save", onClick: this.onSave },
        JSONView.Locale["jsonViewer.Save"]
      ),
      ToolbarButton(
        { className: "btn copy", onClick: this.onCopy },
        JSONView.Locale["jsonViewer.Copy"]
      ),
      ToolbarButton(
        { className: "btn collapse", onClick: this.onCollapse },
        JSONView.Locale["jsonViewer.CollapseAll"]
      ),
      ToolbarButton(
        { className: "btn expand", onClick: this.onExpand },
        this.props.dataSize > EXPAND_THRESHOLD
          ? JSONView.Locale["jsonViewer.ExpandAllSlow"]
          : JSONView.Locale["jsonViewer.ExpandAll"]
      ),
      div({ className: "devtools-separator" }),
      SearchBox({
        actions: this.props.actions,
      })
    );
  }
}

export default { JsonToolbar };
