/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'experimental',
  'engines': {
    'Firefox': '> 28'
  }
};

const { Cu } = require('chrome');
const { on, off, emit } = require('../../event/core');

const { id: addonID, data } = require('sdk/self');
const buttonPrefix =
  'button--' + addonID.toLowerCase().replace(/[^a-z0-9-_]/g, '');

const { isObject } = require('../../lang/type');

const { getMostRecentBrowserWindow } = require('../../window/utils');
const { ignoreWindow } = require('../../private-browsing/utils');
const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
const { AREA_PANEL, AREA_NAVBAR } = CustomizableUI;

const { events: viewEvents } = require('./view/events');

const XUL_NS = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';

const toWidgetID = id => buttonPrefix + '-' + id;
const toButtonID = id => id.substr(buttonPrefix.length + 1);

const views = new Map();
const customizedWindows = new WeakMap();

const buttonListener = {
  onCustomizeStart: window => {
    for (let [id, view] of views) {
      setIcon(id, window, view.icon);
      setLabel(id, window, view.label);
    }

    customizedWindows.set(window, true);
  },
  onCustomizeEnd: window => {
    customizedWindows.delete(window);

    for (let [id, ] of views) {
      let placement = CustomizableUI.getPlacementOfWidget(toWidgetID(id));

      if (placement)
        emit(viewEvents, 'data', { type: 'update', target: id, window: window });
    }
  },
  onWidgetAfterDOMChange: (node, nextNode, container) => {
    let id = toButtonID(node.id);
    let view = views.get(id);
    let window = node.ownerDocument.defaultView;

    if (view) {
      emit(viewEvents, 'data', { type: 'update', target: id, window: window });
    }
  }
};

CustomizableUI.addListener(buttonListener);

require('../../system/unload').when( _ =>
  CustomizableUI.removeListener(buttonListener)
);

function getNode(id, window) {
  return !views.has(id) || ignoreWindow(window)
    ? null
    : CustomizableUI.getWidget(toWidgetID(id)).forWindow(window).node
};

function isInToolbar(id) {
  let placement = CustomizableUI.getPlacementOfWidget(toWidgetID(id));

  return placement && CustomizableUI.getAreaType(placement.area) === 'toolbar';
}


function getImage(icon, isInToolbar, pixelRatio) {
  let targetSize = (isInToolbar ? 18 : 32) * pixelRatio;
  let bestSize = 0;
  let image = icon;

  if (isObject(icon)) {
    for (let size of Object.keys(icon)) {
      size = +size;
      let offset = targetSize - size;

      if (offset === 0) {
        bestSize = size;
        break;
      }

      let delta = Math.abs(offset) - Math.abs(targetSize - bestSize);

      if (delta < 0)
        bestSize = size;
    }

    image = icon[bestSize];
  }

  if (image.indexOf('./') === 0)
    return data.url(image.substr(2));

  return image;
}

function create(options) {
  let { id, label, icon, type } = options;

  if (views.has(id))
    throw new Error('The ID "' + id + '" seems already used.');

  CustomizableUI.createWidget({
    id: toWidgetID(id),
    type: 'custom',
    removable: true,
    defaultArea: AREA_NAVBAR,
    allowedAreas: [ AREA_PANEL, AREA_NAVBAR ],

    onBuild: function(document) {
      let window = document.defaultView;

      let node = document.createElementNS(XUL_NS, 'toolbarbutton');

      let image = getImage(icon, false, window.devicePixelRatio);

      if (ignoreWindow(window))
        node.style.display = 'none';

      node.setAttribute('id', this.id);
      node.setAttribute('class', 'toolbarbutton-1 chromeclass-toolbar-additional');
      node.setAttribute('type', type);
      node.setAttribute('label', label);
      node.setAttribute('tooltiptext', label);
      node.setAttribute('image', image);
      node.setAttribute('sdk-button', 'true');

      views.set(id, {
        area: this.currentArea,
        icon: icon,
        label: label
      });

      node.addEventListener('command', function(event) {
        if (views.has(id)) {
          emit(viewEvents, 'data', {
            type: 'click',
            target: id,
            window: event.view
          });
        }
      });

      return node;
    }
  });
};
exports.create = create;

function dispose(id) {
  if (!views.has(id)) return;

  views.delete(id);
  CustomizableUI.destroyWidget(toWidgetID(id));
}
exports.dispose = dispose;

function setIcon(id, window, icon) {
  let node = getNode(id, window);

  if (node) {
    icon = customizedWindows.has(window) ? views.get(id).icon : icon;
    let image = getImage(icon, isInToolbar(id), window.devicePixelRatio);

    node.setAttribute('image', image);
  }
}
exports.setIcon = setIcon;

function setLabel(id, window, label) {
  let node = customizedWindows.has(window) ? null : getNode(id, window);

  if (node) {
    node.setAttribute('label', label);
    node.setAttribute('tooltiptext', label);
  }
}
exports.setLabel = setLabel;

function setDisabled(id, window, disabled) {
  let node = customizedWindows.has(window) ? null : getNode(id, window);

  if (node)
    node.disabled = disabled;
}
exports.setDisabled = setDisabled;

function setChecked(id, window, checked) {
  let node = customizedWindows.has(window) ? null : getNode(id, window);

  if (node)
    node.checked = checked;
}
exports.setChecked = setChecked;

function click(id) {
  let window = getMostRecentBrowserWindow();
  let node = customizedWindows.has(window) ? null : getNode(id, window);

  if (node)
    node.click();
}
exports.click = click;
