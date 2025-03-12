/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source metrics.yaml files.
 * If you're updating some of the sources, see README for instructions.
 */

interface GleanImpl {

  searchEngineDefault: {
    engineId: GleanString;
    displayName: GleanString;
    loadPath: GleanString;
    submissionUrl: GleanUrl;
    changed: GleanEvent;
  }

  searchEnginePrivate: {
    engineId: GleanString;
    displayName: GleanString;
    loadPath: GleanString;
    submissionUrl: GleanUrl;
    changed: GleanEvent;
  }

  searchService: {
    startupTime: GleanTimingDistribution;
    initializationStatus: Record<string, GleanCounter>;
  }

  browserSearchinit: {
    engineInvalidWebextension: Record<string, GleanQuantity>;
    secureOpensearchEngineCount: GleanQuantity;
    insecureOpensearchEngineCount: GleanQuantity;
    secureOpensearchUpdateCount: GleanQuantity;
    insecureOpensearchUpdateCount: GleanQuantity;
  }

  search: {
    suggestionsLatency: Record<string, GleanTimingDistribution>;
  }
}
