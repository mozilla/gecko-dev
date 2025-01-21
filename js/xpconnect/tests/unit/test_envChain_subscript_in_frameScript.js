/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Verify the environment chain for subscripts in frame script described in
// js/src/vm/EnvironmentObject.h.

const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);

XPCShellContentUtils.init(this);

add_task(async function unique_scope_with_target() {
  const page = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });

  const envsPromise = new Promise(resolve => {
    Services.mm.addMessageListener("unique-with-target-envs-result", msg => {
      resolve(msg.data);
    });
  });

  const runInUniqueScope = true;
  const runInGlobalScope = !runInUniqueScope;

  Services.mm.loadFrameScript(`data:,
const target = {};
Services.scriptloader.loadSubScript(\`data:,
var unique_target_qualified = 10;
unique_target_unqualified = 20;
let unique_target_lexical = 30;
this.unique_target_prop = 40;

const funcs = Cu.getJSTestingFunctions();
const envs = [];
let env = funcs.getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: funcs.getEnvironmentObjectType(env) || "*SystemGlobal*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "unique_target_qualified"),
    unqualified: !!Object.getOwnPropertyDescriptor(env, "unique_target_unqualified"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "unique_target_lexical"),
    prop: !!Object.getOwnPropertyDescriptor(env, "unique_target_prop"),
  });

  env = funcs.getEnclosingEnvironmentObject(env);
}

this.ENVS = envs;
\`, target);

const envs = target.ENVS;

sendSyncMessage("unique-with-target-envs-result", envs);
`, false, runInGlobalScope);

  const envs = await envsPromise;

  Assert.equal(envs.length, 7);

  let i = 0, env;

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, true, "lexical must live in the NSLEO");
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "WithEnvironmentObject");
  Assert.equal(env.qualified, true, "qualified var must live in the with env");
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, true, "this property must live in the with env");

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "WithEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticVariablesObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, true, "unqualified var must live in the NSVO");
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "GlobalLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "*SystemGlobal*");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  await page.close();
});

add_task(async function non_unique_scope_with_target() {
  const page = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });

  const envsPromise = new Promise(resolve => {
    Services.mm.addMessageListener("non-unique-with-target-envs-result", msg => {
      resolve(msg.data);
    });
  });

  const runInUniqueScope = false;
  const runInGlobalScope = !runInUniqueScope;

  Services.mm.loadFrameScript(`data:,
const target = {};
Services.scriptloader.loadSubScript(\`data:,
var non_unique_target_qualified = 10;
non_unique_target_unqualified = 20;
let non_unique_target_lexical = 30;
this.non_unique_target_prop = 40;

const funcs = Cu.getJSTestingFunctions();
const envs = [];
let env = funcs.getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: funcs.getEnvironmentObjectType(env) || "*SystemGlobal*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "non_unique_target_qualified"),
    unqualified: !!Object.getOwnPropertyDescriptor(env, "non_unique_target_unqualified"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "non_unique_target_lexical"),
    prop: !!Object.getOwnPropertyDescriptor(env, "non_unique_target_prop"),
  });

  env = funcs.getEnclosingEnvironmentObject(env);
}

this.ENVS = envs;
\`, target);

const envs = target.ENVS;

sendSyncMessage("non-unique-with-target-envs-result", envs);
`, false, runInGlobalScope);

  const envs = await envsPromise;

  Assert.equal(envs.length, 4);

  let i = 0, env;

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, true, "lexical must live in the NSLEO");
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "WithEnvironmentObject");
  Assert.equal(env.qualified, true, "qualified var must live in the with env");
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, true, "this property must live in the with env");

  env = envs[i]; i++;
  Assert.equal(env.type, "GlobalLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "*SystemGlobal*");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, true, "unqualified var must live in the system global");
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  await page.close();
});

add_task(async function unique_scope_no_target() {
  const page = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });

  const envsPromise = new Promise(resolve => {
    Services.mm.addMessageListener("unique-no-target-envs-result", msg => {
      resolve(msg.data);
    });
  });

  const runInUniqueScope = true;
  const runInGlobalScope = !runInUniqueScope;

  Services.mm.loadFrameScript(`data:,
Services.scriptloader.loadSubScript(\`data:,
var unique_no_target_qualified = 10;
unique_no_target_unqualified = 20;
let unique_no_target_lexical = 30;
this.unique_no_target_prop = 40;

const funcs = Cu.getJSTestingFunctions();
const envs = [];
let env = funcs.getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: funcs.getEnvironmentObjectType(env) || "*SystemGlobal*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "unique_no_target_qualified"),
    unqualified: !!Object.getOwnPropertyDescriptor(env, "unique_no_target_unqualified"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "unique_no_target_lexical"),
    prop: !!Object.getOwnPropertyDescriptor(env, "unique_no_target_prop"),
  });

  env = funcs.getEnclosingEnvironmentObject(env);
}

globalThis.ENVS = envs;
\`);

const outerEnvs = globalThis.ENVS;

sendSyncMessage("unique-no-target-envs-result", outerEnvs);
`, false, runInGlobalScope);

  const envs = await envsPromise;

  Assert.equal(envs.length, 5);

  let i = 0, env;

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, true, "lexical must live in the NSLEO");
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "WithEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, true, "this property must live in the with env");

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticVariablesObject");
  Assert.equal(env.qualified, true, "qualified var must live in the NSVO");
  Assert.equal(env.unqualified, true, "unqualified var must live in the NSVO");
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "GlobalLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "*SystemGlobal*");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  await page.close();
});

add_task(async function non_unique_scope_no_target() {
  const page = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });

  const envsPromise = new Promise(resolve => {
    Services.mm.addMessageListener("non-unique-no-target-envs-result", msg => {
      resolve(msg.data);
    });
  });

  const runInUniqueScope = false;
  const runInGlobalScope = !runInUniqueScope;

  Services.mm.loadFrameScript(`data:,
Services.scriptloader.loadSubScript(\`data:,
var non_unique_no_target_qualified = 10;
non_unique_no_target_unqualified = 20;
let non_unique_no_target_lexical = 30;
this.non_unique_no_target_prop = 40;

const funcs = Cu.getJSTestingFunctions();
const envs = [];
let env = funcs.getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: funcs.getEnvironmentObjectType(env) || "*SystemGlobal*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "non_unique_no_target_qualified"),
    unqualified: !!Object.getOwnPropertyDescriptor(env, "non_unique_no_target_unqualified"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "non_unique_no_target_lexical"),
    prop: !!Object.getOwnPropertyDescriptor(env, "non_unique_no_target_prop"),
  });

  env = funcs.getEnclosingEnvironmentObject(env);
}

globalThis.ENVS = envs;
\`);

const outerEnvs = globalThis.ENVS;

sendSyncMessage("non-unique-no-target-envs-result", outerEnvs);
`, false, runInGlobalScope);

  const envs = await envsPromise;

  Assert.equal(envs.length, 2);

  let i = 0, env;

  env = envs[i]; i++;
  Assert.equal(env.type, "GlobalLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, true, "lexical must live in the global lexical");
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "*SystemGlobal*");
  Assert.equal(env.qualified, true, "qualified var must live in the with system global");
  Assert.equal(env.unqualified, true, "unqualified name must live in the system global");
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, true, "this property must live in the with system global");

  await page.close();
});
