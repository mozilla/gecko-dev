/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Build: <objdir>/dist/@types/lib.gecko.dom.d.ts,
 *
 * from:  <srcdir>/dom/webidl/*.webidl and
 *        <srcdir>/dom/chrome-webidl/*.webidl sources,
 *        using @typescript/dom-lib-generator.
 */

const fs = require("fs");

const TAGLIST = require.resolve("../../parser/htmlparser/nsHTMLTagList.h");
const BINDINGS = require.resolve("../../dom/bindings/Bindings.conf");

const HEADER = `/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source .webidl files.
 * If you're updating some of the sources, see README for instructions.
 */

/// <reference no-default-lib="true" />
/// <reference lib="es2024" />

type HTMLCollectionOf<T> = any;
type IsInstance<T> = (obj: any) => obj is T;
type NodeListOf<T> = any;
interface WindowProxy extends Window {}
`;

// Convert Mozilla-flavor webidl into idl parsable by @w3c/webidl2.js.
function preprocess(webidl) {
  return webidl
    .replaceAll(/^#.+/gm, "")
    .replaceAll(/\binterface \w+;/gm, "")
    .replaceAll(/\bUTF8String\b/gm, "DOMString")
    .replaceAll(/^\s*legacycaller /gm, "getter ")
    .replaceAll(/^callback constructor /gm, "callback ")
    .replaceAll(/(ElementCreationOptions) or (DOMString)/gm, "$2 or $1")
    .replaceAll(/(attribute boolean aecDebug;)/gm, "readonly $1");
}

function customize(all, baseTypes) {
  // Parse HTML element interfaces from nsHTMLTagList.h.
  const RE = /^HTML_(HTMLELEMENT_)?TAG\((\w+)(, \w+, (\w*))?\)/gm;
  for (let match of fs.readFileSync(TAGLIST, "utf8").matchAll(RE)) {
    let iface = all.interfaces.interface[`HTML${match[4] ?? ""}Element`];
    if (iface) {
      iface.element ??= [];
      iface.element.push({ name: match[2] ?? match[3] });
    }
  }

  // Add static isInstance methods to interfaces with constructors.
  for (let i of Object.values(all.interfaces.interface)) {
    if (i.name && !i.noInterfaceObject) {
      i.properties.property.isInstance = {
        name: "isInstance",
        type: "IsInstance",
        subtype: { type: i.name },
        static: true,
      };
    }
  }

  // Parse DOM<->XPCOM type aliases from Bindings.conf.
  let conf = fs.readFileSync(BINDINGS, "utf8");
  let aliases = [];

  for (let [desc, id] of conf.matchAll(/^\s*'(\w+)'\s*:\s*\{[^}]*/gm)) {
    let type = desc.match(/'nativeType'\s*:\s*'(nsI\w+)'/);
    aliases.push([id, type ? type[1] : id]);
  }
  for (let [desc, id] of conf.matchAll(/addExternalIface\('(\w+)[^)]*/gm)) {
    let type = desc.match(/nativeType\s*=\s*'(\w+)'/);
    aliases.push([id, type ? type[1] : `nsIDOM${id}`]);
  }
  for (let [name, type] of aliases) {
    if (name != type && !all.interfaces.interface[name]) {
      all.typedefs.typedef.push({ name, type });
      baseTypes.set(type, type);
      baseTypes.delete(name);
    }
  }
}

// Preprocess, convert, merge and customize webidl, emit and postprocess dts.
async function emitDom(webidls, builtin = "builtin.webidl") {
  const { merge, baseTypeConversionMap } = await import(
    "@typescript/dom-lib-generator/lib/build/helpers.js"
  );
  const { emitWebIdl } = await import(
    "@typescript/dom-lib-generator/lib/build/emitter.js"
  );
  const { convert } = await import(
    "@typescript/dom-lib-generator/lib/build/widlprocess.js"
  );
  const { getExposedTypes } = await import(
    "@typescript/dom-lib-generator/lib/build/expose.js"
  );

  function mergePartial(partials, bases) {
    for (let p of partials) {
      let base = Object.values(bases).find(b => b.name === p.name);
      merge(base.members, p.members, true);
      merge(base.constants, p.constants, true);
      merge(base.properties, p.properties, true);
      merge(base.methods, p.methods, true);
    }
  }

  webidls.push(require.resolve(`./config/${builtin}`));
  let idls = webidls.map(f => fs.readFileSync(f, "utf-8"));
  let all = {};

  // Add external forward type declarations to base types.
  for (let [, id] of idls.join().matchAll(/\binterface (\w+);/gm)) {
    baseTypeConversionMap.set(id, id);
  }

  for (let w of idls.map(idl => convert(preprocess(idl), {}))) {
    merge(all, w.browser, true);

    mergePartial(w.partialDictionaries, all.dictionaries.dictionary);
    mergePartial(w.partialInterfaces, all.interfaces.interface);
    mergePartial(w.partialMixins, all.mixins.mixin);
    mergePartial(w.partialNamespaces, all.namespaces);

    for (let inc of w.includes) {
      let target = all.interfaces.interface[inc.target];
      target.implements ??= [];
      target.implements.push(inc.includes);
    }
  }

  customize(all, baseTypeConversionMap);
  let exposed = getExposedTypes(all, ["Window"], new Set());
  let dts = await Promise.all([
    emitWebIdl(exposed, "Window", "", {}),
    emitWebIdl(exposed, "Window", "sync", {}),
    emitWebIdl(exposed, "Window", "async", {}),
  ]);
  return postprocess(dts.join("\n"));
}
exports.emitDom = emitDom;

// Post-process dom.generated.d.ts into lib.gecko.dom.d.ts.
function postprocess(generated) {
  let text = `${HEADER}\n${generated}`;
  return text
    .replaceAll(/declare var isInstance: /g, "// @ts-ignore\n$&")
    .replace(/interface BeforeUnloadEvent /, "// @ts-ignore\n$&")
    .replace(/interface HTMLScriptElement /, "// @ts-ignore\n$&")
    .replace(/interface SVGElement /, "// @ts-ignore\n$&")
    .replace("interface IsInstance {\n}\n", "")
    .replace("type JSON = any;\n", "");
}

// Build and save the dom lib.
async function main(lib_dts, webidl_dir, ...webidl_files) {
  let dts = await emitDom(webidl_files.map(f => `${webidl_dir}/${f}`));
  console.log(`[INFO] ${lib_dts} (${dts.length.toLocaleString()} bytes)`);
  fs.writeFileSync(lib_dts, dts);
}

if (require.main === module) {
  main(...process.argv.slice(2));
}
