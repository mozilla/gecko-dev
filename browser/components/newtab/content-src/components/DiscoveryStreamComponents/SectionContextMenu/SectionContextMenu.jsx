/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import React, { useState } from "react";
import { LinkMenu } from "../../LinkMenu/LinkMenu";

/**
 * A context menu for blocking, following and unfollowing sections.
 *
 * @param props
 * @returns {React.FunctionComponent}
 */
export function SectionContextMenu(props) {
  const type = props.type || "DISCOVERY_STREAM";
  const title = props.title || props.source;
  const { index, dispatch } = props;

  // Initial context menu options: block this section only.
  const SECTIONS_CONTEXT_MENU_OPTIONS = ["SectionBlock"];

  const [showContextMenu, setShowContextMenu] = useState(false);

  const onClick = e => {
    e.preventDefault();
    setShowContextMenu(!showContextMenu);
  };

  return (
    <div className="section-context-menu">
      <moz-button
        type="icon ghost"
        size="default"
        iconsrc="chrome://global/skin/icons/more.svg"
        title={title}
        onClick={onClick}
      />
      {showContextMenu && (
        <LinkMenu
          dispatch={dispatch}
          index={index}
          source={type.toUpperCase()}
          options={SECTIONS_CONTEXT_MENU_OPTIONS}
          shouldSendImpressionStats={false}
          site={{}}
        />
      )}
    </div>
  );
}
