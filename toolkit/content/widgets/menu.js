/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  let imports = {};
  ChromeUtils.defineESModuleGetters(imports, {
    ShortcutUtils: "resource://gre/modules/ShortcutUtils.sys.mjs",
  });

  const MozMenuItemBaseMixin = Base => {
    class MozMenuItemBase extends MozElements.BaseTextMixin(Base) {
      // nsIDOMXULSelectControlItemElement
      set value(val) {
        this.setAttribute("value", val);
      }
      get value() {
        return this.getAttribute("value") || "";
      }

      // nsIDOMXULSelectControlItemElement
      get selected() {
        return this.getAttribute("selected") == "true";
      }

      // nsIDOMXULSelectControlItemElement
      get control() {
        var parent = this.parentNode;
        // Return the parent if it is a menu or menulist.
        if (parent && XULMenuElement.isInstance(parent.parentNode)) {
          return parent.parentNode;
        }
        return null;
      }

      // nsIDOMXULContainerItemElement
      get parentContainer() {
        for (var parent = this.parentNode; parent; parent = parent.parentNode) {
          if (XULMenuElement.isInstance(parent)) {
            return parent;
          }
        }
        return null;
      }
    }
    MozXULElement.implementCustomInterface(MozMenuItemBase, [
      Ci.nsIDOMXULSelectControlItemElement,
      Ci.nsIDOMXULContainerItemElement,
    ]);
    return MozMenuItemBase;
  };

  const MozMenuBaseMixin = Base => {
    class MozMenuBase extends MozMenuItemBaseMixin(Base) {
      set open(val) {
        this.openMenu(val);
      }

      get open() {
        return this.hasAttribute("open");
      }

      get itemCount() {
        var menupopup = this.menupopup;
        return menupopup ? menupopup.children.length : 0;
      }

      get menupopup() {
        const XUL_NS =
          "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

        for (
          var child = this.firstElementChild;
          child;
          child = child.nextElementSibling
        ) {
          if (child.namespaceURI == XUL_NS && child.localName == "menupopup") {
            return child;
          }
        }
        return null;
      }

      appendItem(aLabel, aValue) {
        var menupopup = this.menupopup;
        if (!menupopup) {
          menupopup = this.ownerDocument.createXULElement("menupopup");
          this.appendChild(menupopup);
        }

        var menuitem = this.ownerDocument.createXULElement("menuitem");
        menuitem.setAttribute("label", aLabel);
        menuitem.setAttribute("value", aValue);

        return menupopup.appendChild(menuitem);
      }

      getIndexOfItem(aItem) {
        var menupopup = this.menupopup;
        if (menupopup) {
          var items = menupopup.children;
          var length = items.length;
          for (var index = 0; index < length; ++index) {
            if (items[index] == aItem) {
              return index;
            }
          }
        }
        return -1;
      }

      getItemAtIndex(aIndex) {
        var menupopup = this.menupopup;
        if (!menupopup || aIndex < 0 || aIndex >= menupopup.children.length) {
          return null;
        }

        return menupopup.children[aIndex];
      }
    }
    MozXULElement.implementCustomInterface(MozMenuBase, [
      Ci.nsIDOMXULContainerElement,
    ]);
    return MozMenuBase;
  };

  // The <menucaption> element is used for rendering <html:optgroup> inside of <html:select>,
  // See SelectParentHelper.sys.mjs.
  class MozMenuCaption extends MozMenuBaseMixin(MozXULElement) {
    static get inheritedAttributes() {
      return {
        ".menu-icon": "src=image,validate,src",
        ".menu-text": "value=label,crop",
      };
    }

    connectedCallback() {
      this.textContent = "";
      this.appendChild(
        MozXULElement.parseXULToFragment(`
      <image class="menu-icon" aria-hidden="true"></image>
      <label class="menu-text" crop="end" aria-hidden="true"/>
    `)
      );
      this.initializeAttributeInheritance();
    }
  }

  customElements.define("menucaption", MozMenuCaption);

  // In general, wait to render menus and menuitems inside menupopups
  // until they are going to be visible:
  window.addEventListener(
    "popupshowing",
    e => {
      if (e.originalTarget.ownerDocument != document) {
        return;
      }
      e.originalTarget.setAttribute("hasbeenopened", "true");
      for (let el of e.originalTarget.querySelectorAll("menuitem, menu")) {
        el.render();
      }
    },
    { capture: true }
  );

  class MozMenuItem extends MozMenuItemBaseMixin(MozXULElement) {
    static get observedAttributes() {
      return super.observedAttributes.concat("acceltext", "key");
    }

    attributeChangedCallback(name, oldValue, newValue) {
      if (name == "acceltext") {
        if (this._ignoreAccelTextChange) {
          this._ignoreAccelTextChange = false;
        } else {
          this._accelTextIsDerived = false;
          this._computeAccelTextFromKeyIfNeeded();
        }
      }
      if (name == "key") {
        this._computeAccelTextFromKeyIfNeeded();
      }
      super.attributeChangedCallback(name, oldValue, newValue);
    }

    static get inheritedAttributes() {
      return {
        ".menu-text": "value=label,crop,accesskey",
        // NOTE(emilio): This menu-highlightable-key thing is a hack for
        // find-in-page in preferences, it really sucks. We can't use
        // text=label everywhere because we rely on the accesskey...
        ".menu-highlightable-text": "text=label,crop,accesskey",
        ".menu-icon":
          "src=image,validate,triggeringprincipal=iconloadingprincipal",
        ".menu-accel": "value=acceltext",
      };
    }

    static get fragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
      <image class="menu-icon" aria-hidden="true"/>
      <label class="menu-text" crop="end" aria-hidden="true"/>
      <label class="menu-highlightable-text" crop="end" aria-hidden="true"/>
      <label class="menu-accel" aria-hidden="true"/>
    `),
        true
      );
      Object.defineProperty(this, "fragment", { value: frag });
      return frag;
    }

    get isMenulistChild() {
      return this.matches("menulist > menupopup > menuitem");
    }

    get isInHiddenMenupopup() {
      return this.matches("menupopup:not([hasbeenopened]) menuitem");
    }

    _computeAccelTextFromKeyIfNeeded() {
      if (!this._accelTextIsDerived && this.getAttribute("acceltext")) {
        return;
      }
      let accelText = (() => {
        if (!document.contains(this)) {
          return null;
        }
        let keyId = this.getAttribute("key");
        if (!keyId) {
          return null;
        }
        let key = document.getElementById(keyId);
        if (!key) {
          let msg =
            `Key ${keyId} of menuitem ${this.getAttribute("label")} ` +
            `could not be found`;
          if (keyId.startsWith("ext-key-id-")) {
            console.info(msg);
          } else {
            console.error(msg);
          }
          return null;
        }
        return imports.ShortcutUtils.prettifyShortcut(key);
      })();

      this._accelTextIsDerived = true;
      // We need to ignore the next attribute change callback for acceltext, in
      // order to not reenter here.
      this._ignoreAccelTextChange = true;
      if (accelText) {
        this.setAttribute("acceltext", accelText);
      } else {
        this.removeAttribute("acceltext");
      }
    }

    render() {
      if (this.renderedOnce) {
        return;
      }
      this.renderedOnce = true;
      this.textContent = "";
      this.append(this.constructor.fragment.cloneNode(true));

      this._computeAccelTextFromKeyIfNeeded();
      this.initializeAttributeInheritance();
    }

    connectedCallback() {
      if (this.renderedOnce) {
        this._computeAccelTextFromKeyIfNeeded();
      }
      // Eagerly render if we are being inserted into a menulist (since we likely need to
      // size it), or into an already-opened menupopup (since we are already visible).
      // Checking isConnectedAndReady is an optimization that will let us quickly skip
      // non-menulists that are being connected during parse.
      if (
        this.isMenulistChild ||
        (this.isConnectedAndReady && !this.isInHiddenMenupopup)
      ) {
        this.render();
      }
    }
  }

  customElements.define("menuitem", MozMenuItem);

  const isHiddenWindow =
    document.documentURI == "chrome://browser/content/hiddenWindowMac.xhtml";

  class MozMenu extends MozMenuBaseMixin(
    MozElements.MozElementMixin(XULMenuElement)
  ) {
    static get inheritedAttributes() {
      return {
        ".menu-text": "value=label,accesskey,crop",
        ".menu-icon":
          "src=image,triggeringprincipal=iconloadingprincipal,validate",
        ".menu-accel": "value=acceltext",
      };
    }

    get needsEagerRender() {
      return (
        this.isMenubarChild || this.isMenulistChild || !this.isInHiddenMenupopup
      );
    }

    get isMenubarChild() {
      return this.matches("menubar > menu");
    }

    get isMenulistChild() {
      return this.matches("menulist > menupopup > menu");
    }

    get isInHiddenMenupopup() {
      return this.matches("menupopup:not([hasbeenopened]) menu");
    }

    get fragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
      <image class="menu-icon"/>
      <label class="menu-text" flex="1" crop="end" aria-hidden="true"/>
      <label class="menu-accel" aria-hidden="true"/>
    `),
        true
      );
      Object.defineProperty(this, "fragment", { value: frag });
      return frag;
    }

    render() {
      // There are 2 main types of menus:
      //  (1) direct descendant of a menubar
      //  (2) all other menus
      // There is also an "iconic" variation of (1) and (2) based on the class.
      // To make this as simple as possible, we don't support menus being changed from one
      // of these types to another after the initial DOM connection. It'd be possible to make
      // this work by keeping track of the markup we prepend and then removing / re-prepending
      // during a change, but it's not a feature we use anywhere currently.
      if (this.renderedOnce) {
        return;
      }
      this.renderedOnce = true;

      // There will be a <menupopup /> already. Don't clear it out, just put our markup before it.
      this.prepend(this.fragment);
      this.initializeAttributeInheritance();
    }

    connectedCallback() {
      // On OSX we will have a bunch of menus in the hidden window. They get converted
      // into native menus based on the host attributes, so the inner DOM doesn't need
      // to be created.
      if (isHiddenWindow) {
        return;
      }

      if (this.delayConnectedCallback()) {
        return;
      }

      // Wait until we are going to be visible or required for sizing a popup.
      if (!this.needsEagerRender) {
        return;
      }

      this.render();
    }
  }

  customElements.define("menu", MozMenu);
}
