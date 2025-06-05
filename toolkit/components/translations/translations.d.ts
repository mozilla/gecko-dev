/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains the shared types for the translations component. The intended use
 * is for defining types to be used in JSDoc. They are used in a form that the TypeScript
 * language server can read them, and provide code hints.
 */

/**
 * For Remote Settings, the JSON details about the attachment.
 */
export interface Attachment {
  // e.g. "2f7c0f7bbc...ca79f0850c4de",
  hash: string;
  // e.g. 5047568,
  size: string;
  // e.g. "lex.50.50.deen.s2t.bin",
  filename: string;
  // e.g. "main-workspace/translations-models/316ebb3a-0682-42cc-8e73-a3ba4bbb280f.bin",
  location: string;
  // e.g. "application/octet-stream"
  mimetype: string;
}

/**
 * The JSON that is synced from Remote Settings for the translation models.
 */
export interface TranslationModelRecord {
  // e.g. "0d4db293-a17c-4085-9bd8-e2e146c85000"
  id: string;
  // The full model name, e.g. "lex.50.50.deen.s2t.bin"
  name: string;
  // The BCP 47 language tag, e.g. "de"
  fromLang: string;
  // The BCP 47 language tag, e.g. "en"
  toLang: string;
  // A model variant. This is a developer-only property that can be used in Nightly or
  // local builds to test different types of models.
  variant?: string;
  // The semver number, used for handling future format changes. e.g. 1.0
  version: string;
  // e.g. "lex"
  fileType: string;
  // The file attachment for this record
  attachment: Attachment;
  // e.g. 1673023100578
  schema: number;
  // e.g. 1673455932527
  last_modified: string;
  // A JEXL expression to determine whether this record should be pulled from Remote Settings
  // See: https://remote-settings.readthedocs.io/en/latest/target-filters.html#filter-expressions
  filter_expression: string;
}

/**
 * The JSON that is synced from Remote Settings for the wasm binaries.
 */
export interface WasmRecord {
  // e.g. "0d4db293-a17c-4085-9bd8-e2e146c85000"
  id: string;
  // The name of the project, e.g. "bergamot-translator"
  name: string;
  // The human readable identifier for the release. e.g. "v0.4.4"
  release: string;
  // The commit hash for the project that generated the wasm.
  revision: string;
  // The license of the wasm, as a https://spdx.org/licenses/
  license: string;
  // The semver number, used for handling future format changes. e.g. 1.0
  version: string;
  // The file attachment for this record
  attachment: Attachment;
  // e.g. 1673455932527
  last_modified: string;
  // A JEXL expression to determine whether this record should be pulled from Remote Settings
  // See: https://remote-settings.readthedocs.io/en/latest/target-filters.html#filter-expressions
  filter_expression: string;
}

/**
 * The following are the types that are provided by the Bergamot wasm library.
 *
 * See: https://github.com/mozilla/bergamot-translator/tree/main/wasm/bindings
 */
export namespace Bergamot {
  /**
   * The main module that is returned from bergamot-translator.js.
   */
  export interface ModuleExport {
    BlockingService: typeof BlockingService;
    AlignedMemoryList: typeof AlignedMemoryList;
    TranslationModel: typeof TranslationModel;
    AlignedMemory: typeof AlignedMemory;
    VectorResponseOptions: typeof VectorResponseOptions;
    VectorString: typeof VectorString;
  }

  /**
   * This class represents a C++ std::vector. The implementations will extend from it.
   */
  export class Vector<T> {
    size(): number;
    get(index: number): T;
    push_back(item: T): void;
  }

  export class VectorResponse extends Vector<Response> {}
  export class VectorString extends Vector<string> {}
  export class VectorResponseOptions extends Vector<ResponseOptions> {}
  export class AlignedMemoryList extends Vector<AlignedMemory> {}

  /**
   * A blocking (e.g. non-threaded) translation service, via Bergamot.
   */
  export class BlockingService {
    /**
     * Translate multiple messages in a single synchronous API call using a single model.
     */
    translate(
      translationModel: TranslationModel,
      vectorSourceText: VectorString,
      vectorResponseOptions: VectorResponseOptions
    ): VectorResponse;

    /**
     * Translate by pivoting between two models
     *
     * For example to translate "fr" to "es", pivot using "en":
     *   "fr" to "en"
     *   "en" to "es"
     *
     * See https://github.com/mozilla/bergamot-translator/blob/5ae1b1ebb3fa9a3eabed8a64ca6798154bd486eb/src/translator/service.h#L80
     */
    translateViaPivoting(
      first: TranslationModel,
      second: TranslationModel,
      vectorSourceText: VectorString,
      vectorResponseOptions: VectorResponseOptions
    ): VectorResponse;
  }

  /**
   * The actual translation model, which is passed into the `BlockingService` methods.
   */
  export class TranslationModel {}

  /**
   * The models need to be placed in the wasm memory space. This object represents
   * aligned memory that was allocated on the wasm side of things. The memory contents
   * can be set via the getByteArrayView method and the Uint8Array.prototype.set method.
   */
  export class AlignedMemory {
    constructor(size: number, alignment: number);
    size(): number;
    getByteArrayView(): Uint8Array;
  }

  /**
   * The response from the translation. This definition isn't complete, but just
   * contains a subset of the available methods.
   *
   * See https://github.com/mozilla/bergamot-translator/blob/main/src/translator/response.h
   */
  export class Response {
    getOriginalText(): string;
    getTranslatedText(): string;
  }

  /**
   * The options to configure a translation response.
   *
   * See https://github.com/mozilla/bergamot-translator/blob/main/src/translator/response_options.h
   */
  export class ResponseOptions {
    // Include the quality estimations.
    qualityScores: boolean;
    // Include the alignments.
    alignment: boolean;
    // Remove HTML tags from text and insert it back into the output.
    html: boolean;
    // Whether to include sentenceMappings or not. Alignments require
    // sentenceMappings and are available irrespective of this option if
    // `alignment=true`.
    sentenceMappings: boolean;
  }
}

/**
 * The client to interact with RemoteSettings.
 * See services/settings/RemoteSettingsClient.sys.mjs
 */
interface RemoteSettingsClient {
  on: Function;
  get: Function;
  attachments: any;
  sync: Function;
}

/**
 * A single language model file.
 */
interface LanguageTranslationModelFile {
  buffer: ArrayBuffer;
  record: TranslationModelRecord;
}

/**
 * The data required to construct a Bergamot Translation Model.
 */
interface TranslationModelPayload {
  sourceLanguage: string;
  targetLanguage: string;
  variant?: string;
  languageModelFiles: LanguageTranslationModelFiles;
}

/**
 * The files required to construct a Bergamot Translation Model's aligned memory.
 */
interface LanguageTranslationModelFiles {
  // The machine learning language model.
  model: LanguageTranslationModelFile;
  // The lexical shortlist that limits possible output of the decoder and makes
  // inference faster.
  lex?: LanguageTranslationModelFile;
  // A model that can generate a translation quality estimation.
  qualityModel?: LanguageTranslationModelFile;

  // There is either a single vocab file:
  vocab?: LanguageTranslationModelFile;

  // Or there are two:
  srcvocab?: LanguageTranslationModelFile;
  trgvocab?: LanguageTranslationModelFile;
}

/**
 * This is the type that is generated when the models are loaded into wasm aligned memory.
 */
type LanguageTranslationModelFilesAligned = {
  [K in keyof LanguageTranslationModelFiles]: Bergamot.AlignedMemory;
};

/**
 * These are the files that are downloaded from Remote Settings that are necessary
 * to start the translations engine. These may not be available if running in tests,
 * and so the engine will be mocked.
 */
interface TranslationsEnginePayload {
  bergamotWasmArrayBuffer: ArrayBuffer;
  translationModelPayloads: TranslationModelPayload[];
  isMocked: boolean;
}

/**
 * Nodes that are being translated are given priority according to their visibility.
 */
export type NodeVisibility = "in-viewport" | "beyond-viewport" | "hidden";

/**
 * Used to decide how to translate a page for full page translations.
 */
export interface LangTags {
  isDocLangTagSupported: boolean;
  docLangTag: string | null;
  userLangTag: string | null;
  htmlLangAttribute: string | null;
  identifiedLangTag: string | null;
  identifiedLangConfident?: boolean;
}

/**
 * All of the necessary information to pick models for doing a translation. This pair
 * should be solvable by picking model variants, and pivoting through English.
 */
export interface LanguagePair {
  sourceLanguage: string;
  targetLanguage: string;
  sourceVariant?: string;
  targetVariant?: string;
}

/**
 * In the case of a single model, there will only be a single potential model variant.
 * A LanguagePair can resolve into 1 or 2 NonPivotLanguagePair depending on the pivoting
 * needs and how they are resolved.
 */
export interface NonPivotLanguagePair {
  sourceLanguage: string;
  targetLanguage: string;
  variant?: string;
}

export interface SupportedLanguage {
  langTag: string;
  langTagKey: string;
  variant: string;
  displayName: string;
}

/**
 * A structure that contains all of the information needed to render dropdowns
 * for translation language selection.
 */
export interface SupportedLanguages {
  languagePairs: NonPivotLanguagePair[];
  sourceLanguages: Array<SupportedLanguage>;
  targetLanguages: Array<SupportedLanguage>;
}

export type TranslationErrors = "engine-load-error";

export type SelectTranslationsPanelState =
  // The panel is closed.
  | { phase: "closed" }

  // The panel is idle after successful initialization and ready to attempt translation.
  | {
      phase: "idle";
      sourceLanguage: string;
      targetLanguage: string;
      sourceText: string;
    }

  // The language dropdown menus failed to populate upon opening the panel.
  // This state contains all of the information for the try-again button to close and re-open the panel.
  | {
      phase: "init-failure";
      event: Event;
      screenX: number;
      screenY: number;
      sourceText: string;
      isTextSelected: boolean;
      langPairPromise: Promise<{
        sourceLanguage?: string;
        targetLanguage?: string;
      }>;
    }

  // The translation failed to complete.
  | {
      phase: "translation-failure";
      sourceLanguage: string;
      targetLanguage: string;
      sourceText: string;
    }

  // The selected language pair is determined to be translatable.
  | {
      phase: "translatable";
      sourceLanguage: string;
      targetLanguage: string;
      sourceText: string;
    }

  // The panel is actively translating the source text.
  | {
      phase: "translating";
      sourceLanguage: string;
      targetLanguage: string;
      sourceText: string;
    }

  // The source text has been translated successfully.
  | {
      phase: "translated";
      sourceLanguage: string;
      targetLanguage: string;
      sourceText: string;
      translatedText: string;
    }

  // The source language is not currently supported by Translations in Firefox.
  | {
      phase: "unsupported";
      detectedLanguage: string;
      targetLanguage: string;
      sourceText: string;
    };

export type RequestTranslationsPort = (
  languagePair: LanguagePair
) => Promise<MessagePort>;

export type TranslationsPortMessages =
  // We have determined that the source text is already translated into the target language, so do nothing.
  | { type: "TranslationsPort:Passthrough"; translationId: string }
  // We found translated text for this request in our cache, so send the targetText directly without translating.
  | {
      type: "TranslationsPort:CachedTranslation";
      translationId: string;
      targetText: string;
    }
  // This is a new, uncached request, and it needs to be translated by the TranslationsEngine.
  | {
      type: "TranslationsPort:TranslationRequest";
      translationId: string;
      sourceText: string;
      isHTML: boolean;
    };

export type EngineStatus = "uninitialized" | "ready" | "error" | "closed";

export type PortToPage =
  // The targetText may be null if the TranslationsEngine had an error, or if this is a response to a Passthrough.
  | {
      type: "TranslationsPort:TranslationResponse";
      targetText: string | null;
      translationId: number;
    }
  | { type: "TranslationsPort:GetEngineStatusResponse"; status: EngineStatus }
  | { type: "TranslationsPort:EngineTerminated" };

/**
 * The translation mode of the page.
 *
 * - In "lazy" mode only nodes within proximity to the viewport are translated.
 *
 * - In "content-eager" mode, all nodes with translatable text content will be translated,
 *   but nodes with attribute translations will still be translated lazily.
 */
export type TranslationsMode = "lazy" | "content-eager";

/**
 * A hint at the user's most recent scroll direction on the page.
 */
export type ScrollDirection = "up" | "down";

/**
 * The location of a node with respect to the viewport.
 */
export type NodeViewportContext =
  | "within"
  | "above"
  | "right"
  | "below"
  | "left";

/**
 * The spatial context of a node, which may include the top, left, and right coordinates
 * of the node's bounding client rect, as well as the node's location with respect to the viewport.
 */
export interface NodeSpatialContext {
  top?: number;
  right?: number;
  left?: number;
  viewportContext?: NodeViewportContext;
}

/**
 * The eligibility of a node to be updated with translated content when its request completes.
 */
export type UpdateEligibility = "stale" | "detached" | "valid";

/**
 * An element with translatable content that is sortable based on its spatial context with
 * respect to the viewport.
 */
export interface SortableContentElement {
  element: Element;
  nodeSet: Set<Node>;
  top?: number;
  left?: number;
  right?: number;
}

/**
 * Elements that have been prioritized for content translations based on their spatial context
 * with respect to the viewport.
 */
export interface PrioritizedContentElements {
  titleElement?: Element;
  inViewportContent: Array<SortableContentElement>;
  aboveViewportContent: Array<SortableContentElement>;
  belowViewportContent: Array<SortableContentElement>;
  otherContent: Array<SortableContentElement>;
}

/**
 * An element with translatable attributes that is sortable based on its spatial context with
 * respect to the viewport.
 */
export interface SortableAttributeElement {
  element: Element;
  attributeSet: Set<string>;
  top?: number;
  left?: number;
  right?: number;
}

/**
 * Elements that have been prioritized for content translations based on their spatial context
 * with respect to the viewport.
 */
export interface PrioritizedAttributeElements {
  inViewportAttributes: Array<SortableAttributeElement>;
  aboveViewportAttributes: Array<SortableAttributeElement>;
  belowViewportAttributes: Array<SortableAttributeElement>;
  otherAttributes: Array<SortableAttributeElement>;
}

/**
 * These are the kinds of priorities that a translation request may be assigned.
 * Each time requests are prioritized and sent to the scheduler, each kind of
 * priority defined below will receive a unique number. Depending on the current
 * context within the page, some of these priorities may be more or less important.
 */
export interface TranslationPriorityKinds {
  inViewportContentPriority: number;
  inViewportAttributePriority: number;
  aboveViewportContentPriority: number;
  aboveViewportAttributePriority: number;
  belowViewportContentPriority: number;
  belowViewportAttributePriority: number;
  otherContentPriority: number;
  otherAttributePriority: number;
}

/**
 * All of the information needed to perform a translation request.
 */
export interface TranslationRequest {
  node: Node;
  sourceText: string;
  translationId: number;
  isHTML: boolean;
  priority: number;
  resolve: (translation: Promise<string> | string | null) => unknown;
  reject: (reason: any) => unknown;
}

/**
 * A convenience type describing a function that executes a translation.
 */
export type TranslationFunction = (message: string) => Promise<string>;
