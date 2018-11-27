// Copyright (C) 2018 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Re-resolve a poisoned module should be consistent on the failure path
esid: sec-finishdynamicimport
info: |
    Runtime Semantics: HostImportModuleDynamically

        Failure path
            - At some future time, the host environment must perform FinishDynamicImport(referencingScriptOrModule,
            specifier, promiseCapability, an abrupt completion), with the abrupt completion representing the cause
            of failure.

        The intent of this specification is to not violate run to completion semantics. The spec-level formalization
        of this is a work-in-progress.

        Every call to HostImportModuleDynamically with the same referencingScriptOrModule and specifier arguments
        must conform to the same set of requirements above as previous calls do. That is, if the host environment
        takes the success path once for a given referencingScriptOrModule, specifier pair, it must always do so,
        and the same for the failure path.

    Runtime Semantics: FinishDynamicImport ( referencingScriptOrModule, specifier, promiseCapability, completion )

    2. Otherwise,
        a. Assert: completion is a normal completion and completion.[[Value]] is undefined.
        b. Let moduleRecord be ! HostResolveImportedModule(referencingScriptOrModule, specifier).
        c. Assert: Evaluate has already been invoked on moduleRecord and successfully completed.
        d. Let namespace be GetModuleNamespace(moduleRecord).
        ...
        f. Otherwise, perform ! Call(promiseCapability.[[Resolve]], undefined, « namespace.[[Value]] »).
flags: [async]
features: [dynamic-import]
includes: [compareArray.js]
---*/

function callImport(name) {
    return import('./double-error-resolution_FIXTURE.js').catch(error => {
        assert.sameValue(error, 'foo');
        return `caught ${name}`;
    });
}

Promise.all([
    callImport(1),
    callImport(2)
]).then(resolutions => assert.compareArray(resolutions, ['caught 1', 'caught 2'])).then($DONE, $DONE);
