# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

<script src="chrome://browser/content/browser-context.js" hidden="true" />

#ifdef XP_MACOSX
      <menuitem id="context-back"
                data-l10n-id="main-context-menu-back-mac"
                command="Browser:BackOrBackDuplicate"/>
      <menuitem id="context-forward"
                data-l10n-id="main-context-menu-forward-mac"
                command="Browser:ForwardOrForwardDuplicate"/>
      <menuitem id="context-reload"
                tooltip="dynamic-shortcut-tooltip"
                data-l10n-id="main-context-menu-reload-mac"
                command="Browser:ReloadOrDuplicate"/>
      <menuitem id="context-stop"
                tooltip="dynamic-shortcut-tooltip"
                data-l10n-id="main-context-menu-stop-mac"
                command="Browser:Stop"/>
#else
      <menugroup id="context-navigation">
        <menuitem id="context-back"
                  data-l10n-id="main-context-menu-back-2"
                  data-l10n-args='{"shortcut":""}'
                  class="menuitem-iconic"
                  command="Browser:BackOrBackDuplicate"/>
        <menuitem id="context-forward"
                  data-l10n-id="main-context-menu-forward-2"
                  data-l10n-args='{"shortcut":""}'
                  class="menuitem-iconic"
                  command="Browser:ForwardOrForwardDuplicate"/>
        <menuitem id="context-reload"
                  class="menuitem-iconic"
                  tooltip="dynamic-shortcut-tooltip"
                  data-l10n-id="main-context-menu-reload"
                  command="Browser:ReloadOrDuplicate"/>
        <menuitem id="context-stop"
                  class="menuitem-iconic"
                  tooltip="dynamic-shortcut-tooltip"
                  data-l10n-id="main-context-menu-stop"
                  command="Browser:Stop"/>
        <menuitem id="context-bookmarkpage"
                  class="menuitem-iconic"
                  data-l10n-id="main-context-menu-bookmark-page"
                  />
      </menugroup>
#endif
      <menuseparator id="context-sep-navigation"/>
      <menuitem id="context-viewsource-goToLine"
                />
      <menuitem id="context-viewsource-wrapLongLines"
                type="checkbox"
                />
      <menuitem id="context-viewsource-highlightSyntax"
                type="checkbox"
                />
      <menuseparator id="context-sep-viewsource-commands"/>
      <menuitem id="spell-no-suggestions"
                disabled="true"
                data-l10n-id="text-action-spell-no-suggestions"/>
      <menuitem id="spell-add-to-dictionary"
                data-l10n-id="text-action-spell-add-to-dictionary"
                />
      <menuitem id="spell-undo-add-to-dictionary"
                data-l10n-id="text-action-spell-undo-add-to-dictionary"
                 />
      <menuseparator id="spell-suggestions-separator"/>
      <menuitem id="context-openlinkincurrent"
                class="context-menu-open-link"
                data-l10n-id="main-context-menu-open-link"
                />
# label and data-usercontextid are dynamically set.
      <menuitem id="context-openlinkincontainertab"
                class="context-menu-open-link"
                data-l10n-id="main-context-menu-open-link-in-container-tab"
                data-l10n-args='{"containerName":""}'
                />
      <menuitem id="context-openlinkintab"
                class="context-menu-open-link"
                data-l10n-id="main-context-menu-open-link-new-tab"
                data-usercontextid="0"
                />

      <menu id="context-openlinkinusercontext-menu"
            class="context-menu-open-link"
            data-l10n-id="main-context-menu-open-link-container-tab"
            hidden="true">
        <menupopup id="context-openlinkinusercontext-popup"/>
      </menu>

      <menuitem id="context-openlink"
                class="context-menu-open-link"
                data-l10n-id="main-context-menu-open-link-new-window"
                />
      <menuitem id="context-openlinkprivate"
                class="context-menu-open-link"
                data-l10n-id="main-context-menu-open-link-new-private-window"
                />
      <menuseparator id="context-sep-open"/>
      <menuitem id="context-bookmarklink"
                data-l10n-id="main-context-menu-bookmark-link-2"
                />
      <menuitem id="context-savelink"
                data-l10n-id="main-context-menu-save-link"
                />
      <menuitem id="context-savelinktopocket"
                data-l10n-id="main-context-menu-save-link-to-pocket"
                />
      <menuitem id="context-copyemail"
                data-l10n-id="main-context-menu-copy-email"
                />
      <menuitem id="context-copyphone"
                data-l10n-id="main-context-menu-copy-phone"
                />
      <menuitem id="context-copylink"
                data-l10n-id="main-context-menu-copy-link-simple"
                />
      <menuitem id="context-stripOnShareLink"
                data-l10n-id="main-context-menu-copy-clean-link"
                hidden="true"
                />
      <menu id="context-sendlinktodevice"
            class="sync-ui-item"
            data-l10n-id="main-context-menu-link-send-to-device"
            hidden="true">
            <menupopup id="context-sendlinktodevice-popup"/>
      </menu>
      <menuseparator id="context-sep-sendlinktodevice" class="sync-ui-item"
                hidden="true"/>
      <menuseparator id="context-sep-copylink"/>
      <menuitem id="context-media-play"
                data-l10n-id="main-context-menu-media-play"
                />
      <menuitem id="context-media-pause"
                data-l10n-id="main-context-menu-media-pause"
                />
      <menuitem id="context-media-mute"
                data-l10n-id="main-context-menu-media-mute"
                />
      <menuitem id="context-media-unmute"
                data-l10n-id="main-context-menu-media-unmute"
                />
      <menu id="context-media-playbackrate" data-l10n-id="main-context-menu-media-play-speed-2">
        <menupopup>
          <menuitem id="context-media-playbackrate-050x"
                    data-l10n-id="main-context-menu-media-play-speed-slow-2"
                    type="radio"
                    name="playbackrate"
                    />
          <menuitem id="context-media-playbackrate-100x"
                    data-l10n-id="main-context-menu-media-play-speed-normal-2"
                    type="radio"
                    name="playbackrate"
                    checked="true"
                    />
          <menuitem id="context-media-playbackrate-125x"
                    data-l10n-id="main-context-menu-media-play-speed-fast-2"
                    type="radio"
                    name="playbackrate"
                    />
          <menuitem id="context-media-playbackrate-150x"
                    data-l10n-id="main-context-menu-media-play-speed-faster-2"
                    type="radio"
                    name="playbackrate"
                    />
          <menuitem id="context-media-playbackrate-200x"
                    data-l10n-id="main-context-menu-media-play-speed-fastest-2"
                    type="radio"
                    name="playbackrate"
                    />
        </menupopup>
      </menu>
      <menuitem id="context-media-loop"
                data-l10n-id="main-context-menu-media-loop"
                type="checkbox"
                />
      <menuitem id="context-leave-dom-fullscreen"
                data-l10n-id="main-context-menu-media-video-leave-fullscreen"
                />
      <menuitem id="context-video-fullscreen"
                data-l10n-id="main-context-menu-media-video-fullscreen"
                />
      <menuitem id="context-media-hidecontrols"
                data-l10n-id="main-context-menu-media-hide-controls"
                />
      <menuitem id="context-media-showcontrols"
                data-l10n-id="main-context-menu-media-show-controls"
                />
      <menuseparator id="context-media-sep-video-commands"/>
      <menuitem id="context-viewvideo"
                data-l10n-id="main-context-menu-video-view-new-tab"
                />
      <menuitem id="context-video-pictureinpicture"
                data-l10n-id="main-context-menu-media-watch-pip"
                type="checkbox"
                />
      <menuseparator id="context-media-sep-commands"/>
      <menuitem id="context-reloadimage"
                data-l10n-id="main-context-menu-image-reload"
                />
      <menuitem id="context-viewimage"
                data-l10n-id="main-context-menu-image-view-new-tab"
                />
      <menuitem id="context-saveimage"
                data-l10n-id="main-context-menu-image-save-as"
                />
      <menuitem id="context-video-saveimage"
                data-l10n-id="main-context-menu-video-take-snapshot"
                />
      <menuitem id="context-savevideo"
                data-l10n-id="main-context-menu-video-save-as"
                />
      <menuitem id="context-saveaudio"
                data-l10n-id="main-context-menu-audio-save-as"
                />
#ifdef CONTEXT_COPY_IMAGE_CONTENTS
      <menuitem id="context-copyimage-contents"
                data-l10n-id="main-context-menu-image-copy"
                />
#endif
      <menuitem id="context-copyimage"
                data-l10n-id="main-context-menu-image-copy-link"
                />
      <menuitem id="context-copyvideourl"
                data-l10n-id="main-context-menu-video-copy-link"
                />
      <menuitem id="context-copyaudiourl"
                data-l10n-id="main-context-menu-audio-copy-link"
                />
      <menuitem id="context-sendimage"
                data-l10n-id="main-context-menu-image-email"
                />
      <menuitem id="context-sendvideo"
                data-l10n-id="main-context-menu-video-email"
                />
      <menuitem id="context-sendaudio"
                data-l10n-id="main-context-menu-audio-email"
                />
      <menuitem id="context-imagetext"
                data-l10n-id="main-context-menu-image-copy-text"
                />
      <menuitem id="context-viewimageinfo"
                hidden="true"
                data-l10n-id="main-context-menu-image-info"
                />
      <menuitem id="context-viewimagedesc"
                data-l10n-id="main-context-menu-image-desc"
                />
      <menuseparator id="context-sep-setbackground"/>
      <menuitem id="context-setDesktopBackground"
                data-l10n-id="main-context-menu-image-set-image-as-background"
                />
      <menuseparator id="context-sep-sharing"/>
#ifdef XP_MACOSX
      <menuitem id="context-bookmarkpage"
                data-l10n-id="main-context-menu-bookmark-page-mac"
                />
#endif
      <menuitem id="context-savepage"
                data-l10n-id="main-context-menu-page-save"
                />
      <menuitem id="context-pocket"
                data-l10n-id="main-context-menu-save-to-pocket"
                />
      <menu id="context-sendpagetodevice"
            class="sync-ui-item"
            data-l10n-id="main-context-menu-send-to-device"
            hidden="true">
        <menupopup id="context-sendpagetodevice-popup"/>
      </menu>
      <menu id="fill-login" hidden="true">
        <menupopup id="fill-login-popup" />
      </menu>
      <menuitem id="fill-login-generated-password"
                data-l10n-id="main-context-menu-suggest-strong-password"
                hidden="true"
                />
      <menuitem id="use-relay-mask"
                data-l10n-id="main-context-menu-use-relay-mask"
                hidden="true"
                />
      <menuitem id="manage-saved-logins"
                data-l10n-id="main-context-menu-manage-passwords"
                hidden="true"
                />
      <menuseparator id="passwordmgr-items-separator"/>
      <menuitem id="context-undo"
                data-l10n-id="text-action-undo"
                command="cmd_undo"/>
      <menuitem id="context-redo"
                data-l10n-id="text-action-redo"
                command="cmd_redo"/>
      <menuseparator id="context-sep-redo"/>
      <menuitem id="context-cut"
                data-l10n-id="text-action-cut"
                command="cmd_cut"/>
      <menuitem id="context-copy"
                data-l10n-id="text-action-copy"
                command="cmd_copy"/>
      <menuitem id="context-paste"
                data-l10n-id="text-action-paste"
                command="cmd_paste"/>
      <menuitem id="context-paste-no-formatting"
                data-l10n-id="text-action-paste-no-formatting"
                command="cmd_pasteNoFormatting"/>
      <menuitem id="context-delete"
                data-l10n-id="text-action-delete"
                command="cmd_delete"/>
      <menuitem id="context-selectall"
                data-l10n-id="text-action-select-all"
                command="cmd_selectAll"/>
      <menuitem id="context-pdfjs-highlight-selection"
                data-l10n-id="text-action-highlight-selection"/>
      <menuitem id="context-reveal-password"
                type="checkbox"
                data-l10n-id="main-context-menu-reveal-password"/>
      <menuitem id="context-print-selection"
                data-l10n-id="main-context-menu-print-selection-2"/>
      <menuseparator id="context-sep-selectall"/>

      <menuitem id="context-pdfjs-undo"
                data-l10n-id="text-action-undo"/>
      <menuitem id="context-pdfjs-redo"
                data-l10n-id="text-action-redo"/>
      <menuseparator id="context-sep-pdfjs-redo"/>
      <menuitem id="context-pdfjs-cut"
                data-l10n-id="text-action-cut"/>
      <menuitem id="context-pdfjs-copy"
                data-l10n-id="text-action-copy"/>
      <menuitem id="context-pdfjs-paste"
                data-l10n-id="text-action-paste"/>
      <menuitem id="context-pdfjs-delete"
                data-l10n-id="text-action-delete"/>
      <menuitem id="context-pdfjs-selectall"
                data-l10n-id="text-action-select-all"/>
      <menuseparator id="context-sep-pdfjs-selectall"/>

      <menuitem id="context-take-screenshot"
                data-l10n-id="main-context-menu-take-screenshot"/>
      <menuseparator id="context-sep-screenshots"/>
      <menuitem id="context-keywordfield"
                data-l10n-id="main-context-menu-keyword"/>
      <menuitem id="context-searchselect"/>
      <menuitem id="context-searchselect-private"/>
      <menuitem id="context-translate-selection"
                data-l10n-id="main-context-menu-translate-selection"/>
      <menu id="context-ask-chat"/>

      <menuseparator id="frame-sep"/>
      <menu id="frame" data-l10n-id="main-context-menu-frame">
        <menupopup>
          <menuitem id="context-showonlythisframe"
                    data-l10n-id="main-context-menu-frame-show-this"
                    />
          <menuitem id="context-openframeintab"
                    data-l10n-id="main-context-menu-frame-open-tab"
                    />
          <menuitem id="context-openframe"
                    data-l10n-id="main-context-menu-frame-open-window"
                    />
          <menuseparator id="open-frame-sep"/>
          <menuitem id="context-reloadframe"
                    data-l10n-id="main-context-menu-frame-reload"
                    />
          <menuseparator/>
          <menuitem id="context-bookmarkframe"
                    data-l10n-id="main-context-menu-frame-add-bookmark"
                    />
          <menuitem id="context-saveframe"
                    data-l10n-id="main-context-menu-frame-save-as"
                    />
          <menuseparator/>
          <menuitem id="context-printframe"
                    data-l10n-id="main-context-menu-frame-print"
                    />
          <menuseparator/>
          <menuitem id="context-take-frame-screenshot"
                    data-l10n-id="main-context-menu-take-frame-screenshot"
                    />
          <menuseparator id="context-sep-frame-screenshot"/>
          <menuitem id="context-viewframesource"
                    data-l10n-id="main-context-menu-frame-view-source"
                    />
          <menuitem id="context-viewframeinfo"
                    data-l10n-id="main-context-menu-frame-view-info"
                    />
#ifdef NIGHTLY_BUILD
          <menuitem id="context-frameOsPid"
                    label="PID: Unknown"
                    disabled="true"/>
#endif
        </menupopup>
      </menu>
      <menuseparator id="spell-separator"/>
      <menuitem id="spell-check-enabled"
                data-l10n-id="text-action-spell-check-toggle"
                type="checkbox"
                />
      <menuitem id="spell-add-dictionaries-main"
                data-l10n-id="text-action-spell-add-dictionaries"
                />
      <menu id="spell-dictionaries"
            data-l10n-id="text-action-spell-dictionaries">
          <menupopup id="spell-dictionaries-menu">
              <menuseparator id="spell-language-separator"/>
              <menuitem id="spell-add-dictionaries"
                        data-l10n-id="text-action-spell-add-dictionaries"
                        />
          </menupopup>
      </menu>
      <menuseparator hidden="true" id="context-sep-bidi"/>
      <menuitem hidden="true" id="context-bidi-text-direction-toggle"
                data-l10n-id="main-context-menu-bidi-switch-text"
                command="cmd_switchTextDirection"/>
      <menuitem hidden="true" id="context-bidi-page-direction-toggle"
                data-l10n-id="main-context-menu-bidi-switch-page"
                />
      <menuseparator id="inspect-separator" hidden="true"/>
      <menuitem id="context-viewpartialsource-selection"
                data-l10n-id="main-context-menu-view-selection-source"
                />
      <menuitem id="context-viewsource"
                data-l10n-id="main-context-menu-view-page-source"
                />
      <menuitem id="context-inspect-a11y"
                hidden="true"
                data-l10n-id="main-context-menu-inspect-a11y-properties"
                />
      <menuitem id="context-inspect"
                hidden="true"
                data-l10n-id="main-context-menu-inspect"
                />
      <menuseparator id="context-media-eme-separator" hidden="true"/>
      <menuitem id="context-media-eme-learnmore"
                class="menuitem-iconic"
                hidden="true"
                data-l10n-id="main-context-menu-eme-learn-more"
                />
