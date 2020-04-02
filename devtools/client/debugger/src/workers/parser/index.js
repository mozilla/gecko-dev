/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { workerUtils } from "devtools-utils";
const { WorkerDispatcher } = workerUtils;

import type { AstSource, AstLocation, AstPosition } from "./types";
import type { SourceLocation, SourceId, SourceContent } from "../../types";
import type { SourceScope } from "./getScopes/visitor";
import type { SymbolDeclarations } from "./getSymbols";

export class ParserDispatcher extends WorkerDispatcher {
  async findOutOfScopeLocations(
    sourceId: string,
    position: AstPosition
  ): Promise<AstLocation[]> {
    ChromeUtils.recordReplayLog(`WorkerDispatch Parser findOutOfScopeLocations`);
    return this.invoke("findOutOfScopeLocations", sourceId, position);
  }

  async getNextStep(
    sourceId: SourceId,
    pausedPosition: AstPosition
  ): Promise<?SourceLocation> {
    ChromeUtils.recordReplayLog(`WorkerDispatch Parser getNextStep`);
    return this.invoke("getNextStep", sourceId, pausedPosition);
  }

  async clearState(): Promise<void> {
    ChromeUtils.recordReplayLog(`WorkerDispatch Parser clearState`);
    return this.invoke("clearState");
  }

  async getScopes(location: SourceLocation): Promise<SourceScope[]> {
    ChromeUtils.recordReplayLog(`WorkerDispatch Parser getScopes`);
    return this.invoke("getScopes", location);
  }

  async getSymbols(sourceId: string): Promise<SymbolDeclarations> {
    ChromeUtils.recordReplayLog(`WorkerDispatch Parser getSymbols`);
    return this.invoke("getSymbols", sourceId);
  }

  async setSource(sourceId: SourceId, content: SourceContent): Promise<void> {
    const astSource: AstSource = {
      id: sourceId,
      text: content.type === "wasm" ? "" : content.value,
      contentType: content.contentType || null,
      isWasm: content.type === "wasm",
    };

    ChromeUtils.recordReplayLog(`WorkerDispatch Parser setSource`);
    return this.invoke("setSource", astSource);
  }

  async hasSyntaxError(input: string): Promise<string | false> {
    ChromeUtils.recordReplayLog(`WorkerDispatch Parser hasSyntaxError`);
    return this.invoke("hasSyntaxError", input);
  }

  async mapExpression(
    expression: string,
    mappings: {
      [string]: string | null,
    } | null,
    bindings: string[],
    shouldMapBindings?: boolean,
    shouldMapAwait?: boolean
  ): Promise<{ expression: string }> {
    ChromeUtils.recordReplayLog(`WorkerDispatch Parser mapExpression`);
    return this.invoke(
      "mapExpression",
      expression,
      mappings,
      bindings,
      shouldMapBindings,
      shouldMapAwait
    );
  }

  async clear() {
    await this.clearState();
  }
}

export type {
  SourceScope,
  BindingData,
  BindingLocation,
  BindingLocationType,
  BindingDeclarationLocation,
  BindingMetaValue,
  BindingType,
} from "./getScopes";

export type { AstLocation, AstPosition } from "./types";

export type {
  ClassDeclaration,
  SymbolDeclaration,
  SymbolDeclarations,
  IdentifierDeclaration,
  FunctionDeclaration,
} from "./getSymbols";
