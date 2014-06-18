// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// the "exported" symbols
let LoopUI;

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI", "resource:///modules/loop/MozLoopAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService", "resource:///modules/loop/MozLoopService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PanelFrame", "resource:///modules/PanelFrame.jsm");


(function() {

  LoopUI = {
    /**
     * Opens the panel for Loop and sizes it appropriately.
     *
     * @param {event} event The event opening the panel, used to anchor
     *                      the panel to the button which triggers it.
     */
    openCallPanel: function(event) {
      let callback = iframe => {
        iframe.addEventListener("DOMContentLoaded", function documentDOMLoaded() {
          iframe.removeEventListener("DOMContentLoaded", documentDOMLoaded, true);
          injectLoopAPI(iframe.contentWindow);

          // We use loopPanelInitialized so that we know we've finished localising before
          // sizing the panel.
          iframe.contentWindow.addEventListener("loopPanelInitialized",
            function documentLoaded() {
              iframe.contentWindow.removeEventListener("loopPanelInitialized",
                                                       documentLoaded, true);
            }, true);

        }, true);
      };

      PanelFrame.showPopup(window, PanelUI, event.target, "loop", null,
                           "about:looppanel", null, callback);
    },

    /**
     * Triggers the initialization of the loop service.  Called by
     * delayedStartup.
     */
    initialize: function() {
      MozLoopService.initialize();
    },
  };
})();
