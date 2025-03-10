/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  accessibleSpec,
} = require("resource://devtools/shared/specs/accessibility.js");

const {
  accessibility: { AUDIT_TYPE },
} = require("resource://devtools/shared/constants.js");

loader.lazyRequireGetter(
  this,
  "getContrastRatioFor",
  "resource://devtools/server/actors/accessibility/audit/contrast.js",
  true
);
loader.lazyRequireGetter(
  this,
  "auditKeyboard",
  "resource://devtools/server/actors/accessibility/audit/keyboard.js",
  true
);
loader.lazyRequireGetter(
  this,
  "auditTextLabel",
  "resource://devtools/server/actors/accessibility/audit/text-label.js",
  true
);
loader.lazyRequireGetter(
  this,
  "isDefunct",
  "resource://devtools/server/actors/utils/accessibility.js",
  true
);
loader.lazyRequireGetter(
  this,
  "findCssSelector",
  "resource://devtools/shared/inspector/css-logic.js",
  true
);
loader.lazyRequireGetter(
  this,
  "events",
  "resource://devtools/shared/event-emitter.js"
);
loader.lazyRequireGetter(
  this,
  "getBounds",
  "resource://devtools/server/actors/highlighters/utils/accessibility.js",
  true
);
loader.lazyRequireGetter(
  this,
  "isFrameWithChildTarget",
  "resource://devtools/shared/layout/utils.js",
  true
);
const lazy = {};
loader.lazyGetter(
  lazy,
  "ContentDOMReference",
  () =>
    ChromeUtils.importESModule(
      "resource://gre/modules/ContentDOMReference.sys.mjs",
      // ContentDOMReference needs to be retrieved from the shared global
      // since it is a shared singleton.
      { global: "shared" }
    ).ContentDOMReference
);

const RELATIONS_TO_IGNORE = new Set([
  Ci.nsIAccessibleRelation.RELATION_CONTAINING_APPLICATION,
  Ci.nsIAccessibleRelation.RELATION_CONTAINING_TAB_PANE,
  Ci.nsIAccessibleRelation.RELATION_CONTAINING_WINDOW,
  Ci.nsIAccessibleRelation.RELATION_PARENT_WINDOW_OF,
  Ci.nsIAccessibleRelation.RELATION_SUBWINDOW_OF,
]);

const nsIAccessibleRole = Ci.nsIAccessibleRole;
const TEXT_ROLES = new Set([
  nsIAccessibleRole.ROLE_TEXT_LEAF,
  nsIAccessibleRole.ROLE_STATICTEXT,
]);

const STATE_DEFUNCT = Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT;
const CSS_TEXT_SELECTOR = "#text";

/**
 * Get node inforamtion such as nodeType and the unique CSS selector for the node.
 * @param  {DOMNode} node
 *         Node for which to get the information.
 * @return {Object}
 *         Information about the type of the node and how to locate it.
 */
function getNodeDescription(node) {
  if (!node || Cu.isDeadWrapper(node)) {
    return { nodeType: undefined, nodeCssSelector: "" };
  }

  const { nodeType } = node;
  return {
    nodeType,
    // If node is a text node, we find a unique CSS selector for its parent and add a
    // CSS_TEXT_SELECTOR postfix to indicate that it's a text node.
    nodeCssSelector:
      nodeType === Node.TEXT_NODE
        ? `${findCssSelector(node.parentNode)}${CSS_TEXT_SELECTOR}`
        : findCssSelector(node),
  };
}

/**
 * Get a snapshot of the nsIAccessible object including its subtree. None of the subtree
 * queried here is cached via accessible walker's refMap.
 * @param  {nsIAccessible} acc
 *         Accessible object to take a snapshot of.
 * @param  {nsIAccessibilityService} a11yService
 *         Accessibility service instance in the current process, used to get localized
 *         string representation of various accessible properties.
 * @param  {WindowGlobalTargetActor} targetActor
 * @return {JSON}
 *         JSON snapshot of the accessibility tree with root at current accessible.
 */
function getSnapshot(acc, a11yService, targetActor) {
  if (isDefunct(acc)) {
    return {
      states: [a11yService.getStringStates(0, STATE_DEFUNCT)],
    };
  }

  const actions = [];
  for (let i = 0; i < acc.actionCount; i++) {
    actions.push(acc.getActionDescription(i));
  }

  const attributes = {};
  if (acc.attributes) {
    for (const { key, value } of acc.attributes.enumerate()) {
      attributes[key] = value;
    }
  }

  const state = {};
  const extState = {};
  acc.getState(state, extState);
  const states = [...a11yService.getStringStates(state.value, extState.value)];

  const children = [];
  for (let child = acc.firstChild; child; child = child.nextSibling) {
    // Ignore children from different documents when we have targets for every documents.
    if (
      targetActor.ignoreSubFrames &&
      child.DOMNode.ownerDocument !== targetActor.contentDocument
    ) {
      continue;
    }
    children.push(getSnapshot(child, a11yService, targetActor));
  }

  const { nodeType, nodeCssSelector } = getNodeDescription(acc.DOMNode);
  const snapshot = {
    name: acc.name,
    role: getStringRole(acc, a11yService),
    actions,
    value: acc.value,
    nodeCssSelector,
    nodeType,
    description: acc.description,
    keyboardShortcut: acc.accessKey || acc.keyboardShortcut,
    childCount: acc.childCount,
    indexInParent: acc.indexInParent,
    states,
    children,
    attributes,
  };
  const useChildTargetToFetchChildren =
    acc.role === Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME &&
    isFrameWithChildTarget(targetActor, acc.DOMNode);
  if (useChildTargetToFetchChildren) {
    snapshot.useChildTargetToFetchChildren = useChildTargetToFetchChildren;
    snapshot.childCount = 1;
    snapshot.contentDOMReference = lazy.ContentDOMReference.get(acc.DOMNode);
  }

  return snapshot;
}

/**
 * Get a string indicating the role of the nsIAccessible object.
 * An ARIA role token will be returned unless the role can't be mapped to an
 * ARIA role (e.g. <iframe>), in which case a Gecko role string will be
 * returned.
 * @param  {nsIAccessible} acc
 *         Accessible object to take a snapshot of.
 * @param  {nsIAccessibilityService} a11yService
 *         Accessibility service instance in the current process, used to get localized
 *         string representation of various accessible properties.
 * @return String
 */
function getStringRole(acc, a11yService) {
  let role = acc.computedARIARole;
  if (!role) {
    // We couldn't map to an ARIA role, so use a Gecko role string.
    role = a11yService.getStringRole(acc.role);
  }
  return role;
}

/**
 * The AccessibleActor provides information about a given accessible object: its
 * role, name, states, etc.
 */
class AccessibleActor extends Actor {
  constructor(walker, rawAccessible) {
    super(walker.conn, accessibleSpec);
    this.walker = walker;
    this.rawAccessible = rawAccessible;

    /**
     * Indicates if the raw accessible is no longer alive.
     *
     * @return Boolean
     */
    Object.defineProperty(this, "isDefunct", {
      get() {
        const defunct = isDefunct(this.rawAccessible);
        if (defunct) {
          delete this.isDefunct;
          this.isDefunct = true;
          return this.isDefunct;
        }

        return defunct;
      },
      configurable: true,
    });
  }

  destroy() {
    super.destroy();
    this.walker = null;
    this.rawAccessible = null;
  }

  get role() {
    if (this.isDefunct) {
      return null;
    }
    return getStringRole(this.rawAccessible, this.walker.a11yService);
  }

  get name() {
    if (this.isDefunct) {
      return null;
    }
    return this.rawAccessible.name;
  }

  get value() {
    if (this.isDefunct) {
      return null;
    }
    return this.rawAccessible.value;
  }

  get description() {
    if (this.isDefunct) {
      return null;
    }
    return this.rawAccessible.description;
  }

  get keyboardShortcut() {
    if (this.isDefunct) {
      return null;
    }
    // Gecko accessibility exposes two key bindings: Accessible::AccessKey and
    // Accessible::KeyboardShortcut. The former is used for accesskey, where the latter
    // is used for global shortcuts defined by XUL menu items, etc. Here - do what the
    // Windows implementation does: try AccessKey first, and if that's empty, use
    // KeyboardShortcut.
    return this.rawAccessible.accessKey || this.rawAccessible.keyboardShortcut;
  }

  get childCount() {
    if (this.isDefunct) {
      return 0;
    }
    // In case of a remote frame declare at least one child (the #document
    // element) so that they can be expanded.
    if (this.useChildTargetToFetchChildren) {
      return 1;
    }

    return this.rawAccessible.childCount;
  }

  get domNodeType() {
    if (this.isDefunct) {
      return 0;
    }
    return this.rawAccessible.DOMNode ? this.rawAccessible.DOMNode.nodeType : 0;
  }

  get parentAcc() {
    if (this.isDefunct) {
      return null;
    }
    return this.walker.addRef(this.rawAccessible.parent);
  }

  children() {
    const children = [];
    if (this.isDefunct) {
      return children;
    }

    for (
      let child = this.rawAccessible.firstChild;
      child;
      child = child.nextSibling
    ) {
      children.push(this.walker.addRef(child));
    }
    return children;
  }

  get indexInParent() {
    if (this.isDefunct) {
      return -1;
    }

    try {
      return this.rawAccessible.indexInParent;
    } catch (e) {
      // Accessible is dead.
      return -1;
    }
  }

  get actions() {
    const actions = [];
    if (this.isDefunct) {
      return actions;
    }

    for (let i = 0; i < this.rawAccessible.actionCount; i++) {
      actions.push(this.rawAccessible.getActionDescription(i));
    }
    return actions;
  }

  get states() {
    if (this.isDefunct) {
      return [];
    }

    const state = {};
    const extState = {};
    this.rawAccessible.getState(state, extState);
    return [
      ...this.walker.a11yService.getStringStates(state.value, extState.value),
    ];
  }

  get attributes() {
    if (this.isDefunct || !this.rawAccessible.attributes) {
      return {};
    }

    const attributes = {};
    for (const { key, value } of this.rawAccessible.attributes.enumerate()) {
      attributes[key] = value;
    }

    return attributes;
  }

  get bounds() {
    if (this.isDefunct) {
      return null;
    }

    let x = {},
      y = {},
      w = {},
      h = {};
    try {
      this.rawAccessible.getBoundsInCSSPixels(x, y, w, h);
      x = x.value;
      y = y.value;
      w = w.value;
      h = h.value;
    } catch (e) {
      return null;
    }

    // Check if accessible bounds are invalid.
    const left = x,
      right = x + w,
      top = y,
      bottom = y + h;
    if (left === right || top === bottom) {
      return null;
    }

    return { x, y, w, h };
  }

  async getRelations() {
    const relationObjects = [];
    if (this.isDefunct) {
      return relationObjects;
    }

    const relations = [
      ...this.rawAccessible.getRelations().enumerate(Ci.nsIAccessibleRelation),
    ];
    if (relations.length === 0) {
      return relationObjects;
    }

    const doc = await this.walker.getDocument();
    if (this.isDestroyed()) {
      // This accessible actor is destroyed.
      return relationObjects;
    }
    relations.forEach(relation => {
      if (RELATIONS_TO_IGNORE.has(relation.relationType)) {
        return;
      }

      const type = this.walker.a11yService.getStringRelationType(
        relation.relationType
      );
      const targets = [...relation.getTargets().enumerate(Ci.nsIAccessible)];
      let relationObject;
      for (const target of targets) {
        let targetAcc;
        try {
          targetAcc = this.walker.attachAccessible(target, doc.rawAccessible);
        } catch (e) {
          // Target is not available.
        }

        if (targetAcc) {
          if (!relationObject) {
            relationObject = { type, targets: [] };
          }

          relationObject.targets.push(targetAcc);
        }
      }

      if (relationObject) {
        relationObjects.push(relationObject);
      }
    });

    return relationObjects;
  }

  get useChildTargetToFetchChildren() {
    if (this.isDefunct) {
      return false;
    }

    return (
      this.rawAccessible.role === Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME &&
      isFrameWithChildTarget(
        this.walker.targetActor,
        this.rawAccessible.DOMNode
      )
    );
  }

  form() {
    return {
      actor: this.actorID,
      role: this.role,
      name: this.name,
      useChildTargetToFetchChildren: this.useChildTargetToFetchChildren,
      childCount: this.childCount,
      checks: this._lastAudit,
    };
  }

  /**
   * Provide additional (full) information about the accessible object that is
   * otherwise missing from the form.
   *
   * @return {Object}
   *         Object that contains accessible object information such as states,
   *         actions, attributes, etc.
   */
  hydrate() {
    return {
      value: this.value,
      description: this.description,
      keyboardShortcut: this.keyboardShortcut,
      domNodeType: this.domNodeType,
      indexInParent: this.indexInParent,
      states: this.states,
      actions: this.actions,
      attributes: this.attributes,
    };
  }

  _isValidTextLeaf(rawAccessible) {
    return (
      !isDefunct(rawAccessible) &&
      TEXT_ROLES.has(rawAccessible.role) &&
      rawAccessible.name &&
      !!rawAccessible.name.trim().length
    );
  }

  /**
   * Calculate the contrast ratio of the given accessible.
   */
  async _getContrastRatio() {
    if (!this._isValidTextLeaf(this.rawAccessible)) {
      return null;
    }

    const { bounds } = this;
    if (!bounds) {
      return null;
    }

    const { DOMNode: rawNode } = this.rawAccessible;
    const win = rawNode.ownerGlobal;

    // Keep the reference to the walker actor in case the actor gets destroyed
    // during the colour contrast ratio calculation.
    const { walker } = this;
    await walker.clearStyles(win);
    const contrastRatio = await getContrastRatioFor(rawNode.parentNode, {
      bounds: getBounds(win, bounds),
      win,
      appliedColorMatrix: this.walker.colorMatrix,
    });

    if (this.isDestroyed()) {
      // This accessible actor is destroyed.
      return null;
    }
    await walker.restoreStyles(win);

    return contrastRatio;
  }

  /**
   * Run an accessibility audit for a given audit type.
   * @param {String} type
   *        Type of an audit (Check AUDIT_TYPE in devtools/shared/constants
   *        to see available audit types).
   *
   * @return {null|Object}
   *         Object that contains accessible audit data for a given type or null
   *         if there's nothing to report for this accessible.
   */
  _getAuditByType(type) {
    switch (type) {
      case AUDIT_TYPE.CONTRAST:
        return this._getContrastRatio();
      case AUDIT_TYPE.KEYBOARD:
        // Determine if keyboard accessibility is lacking where it is necessary.
        return auditKeyboard(this.rawAccessible);
      case AUDIT_TYPE.TEXT_LABEL:
        // Determine if text alternative is missing for an accessible where it
        // is necessary.
        return auditTextLabel(this.rawAccessible);
      default:
        return null;
    }
  }

  /**
   * Audit the state of the accessible object.
   *
   * @param  {Object} options
   *         Options for running audit, may include:
   *         - types: Array of audit types to be performed during audit.
   *
   * @return {Object|null}
   *         Audit results for the accessible object.
   */
  audit(options = {}) {
    if (this._auditing) {
      return this._auditing;
    }

    const { types } = options;
    let auditTypes = Object.values(AUDIT_TYPE);
    if (types && types.length) {
      auditTypes = auditTypes.filter(auditType => types.includes(auditType));
    }

    this._auditing = (async () => {
      const results = [];
      for (const auditType of auditTypes) {
        // For some reason keyboard checks for focus styling affect values (that are
        // used by other types of checks (text names and values)) returned by
        // accessible objects. This happens only when multiple checks are run at the
        // same time (asynchronously) and the audit might return unexpected
        // failures. We thus run checks sequentially to avoid this.
        // See bug 1594743 for more detail.
        const audit = await this._getAuditByType(auditType);
        results.push(audit);
      }
      return results;
    })()
      .then(results => {
        if (this.isDefunct || this.isDestroyed()) {
          return null;
        }

        const audit = results.reduce((auditResults, result, index) => {
          auditResults[auditTypes[index]] = result;
          return auditResults;
        }, {});
        this._lastAudit = this._lastAudit || {};
        Object.assign(this._lastAudit, audit);
        events.emit(this, "audited", audit);

        return audit;
      })
      .catch(error => {
        if (!this.isDefunct && !this.isDestroyed()) {
          throw error;
        }
        return null;
      })
      .finally(() => {
        this._auditing = null;
      });

    return this._auditing;
  }

  snapshot() {
    return getSnapshot(
      this.rawAccessible,
      this.walker.a11yService,
      this.walker.targetActor
    );
  }
}

exports.AccessibleActor = AccessibleActor;
