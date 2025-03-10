/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */
"use strict";

/*
 * TypeScript transformer to automatically enable @ts-check for root JS files,
 * only those directly referenced from the tsconfig.json include option.
 */
exports.default = (program, host, _, { ts }) => {
  return ts.createProgram(
    program.getRootFileNames(),
    program.getCompilerOptions(),
    {
      ...host,
      getSourceFile(...args) {
        let file = host.getSourceFile(...args);
        // If source file doesn't have a @ts-check or @ts-nocheck directive.
        if (!file.checkJsDirective && file.scriptKind === ts.ScriptKind.JS) {
          // Add one based on it being included as root file in tsconfig.
          file.checkJsDirective = {
            enabled: program.getRootFileNames().includes(file.fileName),
          };
        }
        return file;
      },
    }
  );
};
