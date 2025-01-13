/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import { div, span } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";

import SourceIcon from "../shared/SourceIcon";
import AccessibleImage from "../shared/AccessibleImage";

import {
  getGeneratedSourceByURL,
  getHideIgnoredSources,
  isSourceOverridden,
} from "../../selectors/index";
import actions from "../../actions/index";

import { sourceTypes } from "../../utils/source";
import { createLocation } from "../../utils/location";

const classnames = require("resource://devtools/client/shared/classnames.js");

class SourceTreeItemContents extends Component {
  static get propTypes() {
    return {
      autoExpand: PropTypes.bool.isRequired,
      depth: PropTypes.number.isRequired,
      expanded: PropTypes.bool.isRequired,
      focusItem: PropTypes.func.isRequired,
      focused: PropTypes.bool.isRequired,
      hasMatchingGeneratedSource: PropTypes.bool,
      item: PropTypes.object.isRequired,
      selectSourceItem: PropTypes.func.isRequired,
      setExpanded: PropTypes.func.isRequired,
      getParent: PropTypes.func.isRequired,
      hideIgnoredSources: PropTypes.bool,
      arrow: PropTypes.object,
    };
  }

  componentDidMount() {
    const { autoExpand, item } = this.props;
    if (autoExpand) {
      this.props.setExpanded(item, true, false);
    }
  }

  onClick = () => {
    const { item, focusItem, selectSourceItem } = this.props;

    focusItem(item);
    if (item.type == "source") {
      selectSourceItem(item);
    }
  };

  onContextMenu = event => {
    event.stopPropagation();
    event.preventDefault();
    this.props.showSourceTreeItemContextMenu(
      event,
      this.props.item,
      this.props.depth,
      this.props.setExpanded,
      this.renderItemName(),
      this.props.isSourceOverridden
    );
  };

  renderIcon(item) {
    if (item.type == "thread") {
      const icon = item.thread.targetType.includes("worker")
        ? "worker"
        : "window";
      return React.createElement(AccessibleImage, {
        className: classnames(icon),
      });
    }
    if (item.type == "group") {
      if (item.groupName === "Webpack") {
        return React.createElement(AccessibleImage, {
          className: "webpack",
        });
      } else if (item.groupName === "Angular") {
        return React.createElement(AccessibleImage, {
          className: "angular",
        });
      }
      // Check if the group relates to an extension.
      // This happens when a webextension injects a content script.
      if (item.isForExtensionSource) {
        return React.createElement(AccessibleImage, {
          className: "extension",
        });
      }
      return React.createElement(AccessibleImage, {
        className: "globe-small",
      });
    }
    if (item.type == "directory") {
      return React.createElement(AccessibleImage, {
        className: "folder",
      });
    }
    if (item.type == "source") {
      const { source, sourceActor } = item;
      return React.createElement(SourceIcon, {
        location: createLocation({
          source,
          sourceActor,
        }),
        modifier: icon => {
          // In the SourceTree, extension files should use the file-extension based icon,
          // whereas we use the extension icon in other Components (eg. source tabs and breakpoints pane).
          if (icon === "extension") {
            return sourceTypes[source.displayURL.fileExtension] || "javascript";
          }
          return (
            icon +
            (this.props.isSourceOverridden ? " has-network-override" : "")
          );
        },
      });
    }
    return null;
  }
  renderItemName() {
    const { item } = this.props;

    if (item.type == "thread") {
      const { thread } = item;
      return (
        thread.name +
        (thread.serviceWorkerStatus ? ` (${thread.serviceWorkerStatus})` : "")
      );
    }
    if (item.type == "group") {
      return item.groupName;
    }
    if (item.type == "directory") {
      const parentItem = this.props.getParent(item);
      return item.path.replace(parentItem.path, "").replace(/^\//, "");
    }
    if (item.type == "source") {
      return item.source.longName;
    }

    return null;
  }

  renderItemTooltip() {
    const { item } = this.props;

    if (item.type == "thread") {
      return item.thread.name;
    }
    if (item.type == "group") {
      return item.groupName;
    }
    if (item.type == "directory") {
      return item.path;
    }
    if (item.type == "source") {
      return item.source.url;
    }

    return null;
  }

  render() {
    const { item, focused, hasMatchingGeneratedSource, hideIgnoredSources } =
      this.props;

    if (hideIgnoredSources && item.isBlackBoxed) {
      return null;
    }
    const suffix = hasMatchingGeneratedSource
      ? span(
          {
            className: "suffix",
          },
          L10N.getStr("sourceFooter.mappedSuffix")
        )
      : null;
    return div(
      {
        className: classnames("node", {
          focused,
          blackboxed: item.type == "source" && item.isBlackBoxed,
        }),
        key: item.path,
        onClick: this.onClick,
        onContextMenu: this.onContextMenu,
        title: this.renderItemTooltip(),
      },
      this.props.arrow,
      this.renderIcon(item),
      span(
        {
          className: "label",
        },
        this.renderItemName(),
        suffix
      )
    );
  }
}

function getHasMatchingGeneratedSource(state, source) {
  if (!source || !source.isOriginal) {
    return false;
  }

  return !!getGeneratedSourceByURL(state, source.url);
}

const toolboxMapStateToProps = (state, props) => {
  const { item } = props;
  return {
    isSourceOverridden: isSourceOverridden(state, item.source),
  };
};

const SourceTreeItemInner = connect(toolboxMapStateToProps, {}, undefined, {
  storeKey: "toolbox-store",
})(SourceTreeItemContents);

class SourcesTreeItem extends Component {
  static get propTypes() {
    return {
      autoExpand: PropTypes.bool.isRequired,
      depth: PropTypes.bool.isRequired,
      expanded: PropTypes.bool.isRequired,
      focusItem: PropTypes.func.isRequired,
      focused: PropTypes.bool.isRequired,
      hasMatchingGeneratedSource: PropTypes.bool.isRequired,
      item: PropTypes.object.isRequired,
      selectSourceItem: PropTypes.func.isRequired,
      setExpanded: PropTypes.func.isRequired,
      showSourceTreeItemContextMenu: PropTypes.func.isRequired,
      getParent: PropTypes.func.isRequired,
      hideIgnoredSources: PropTypes.bool,
      arrow: PropTypes.object,
    };
  }

  render() {
    return React.createElement(SourceTreeItemInner, {
      autoExpand: this.props.autoExpand,
      depth: this.props.depth,
      expanded: this.props.expanded,
      focusItem: this.props.focusItem,
      focused: this.props.focused,
      hasMatchingGeneratedSource: this.props.hasMatchingGeneratedSource,
      item: this.props.item,
      selectSourceItem: this.props.selectSourceItem,
      setExpanded: this.props.setExpanded,
      showSourceTreeItemContextMenu: this.props.showSourceTreeItemContextMenu,
      getParent: this.props.getParent,
      hideIgnoredSources: this.props.hideIgnoredSources,
      arrow: this.props.arrow,
    });
  }
}

const mapStateToProps = (state, props) => {
  const { item } = props;
  if (item.type == "source") {
    const { source } = item;
    return {
      hasMatchingGeneratedSource: getHasMatchingGeneratedSource(state, source),
      hideIgnoredSources: getHideIgnoredSources(state),
    };
  }
  return {};
};

export default connect(mapStateToProps, {
  showSourceTreeItemContextMenu: actions.showSourceTreeItemContextMenu,
})(SourcesTreeItem);
