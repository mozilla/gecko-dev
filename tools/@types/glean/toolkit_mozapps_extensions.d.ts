/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source metrics.yaml files.
 * If you're updating some of the sources, see README for instructions.
 */

interface GleanImpl {

  addonsManager: {
    install: GleanEvent;
    update: GleanEvent;
    installStats: GleanEvent;
    manage: GleanEvent;
    reportSuspiciousSite: GleanEvent;
  }

  blocklist: {
    lastModifiedRsAddonsMblf: GleanDatetime;
    mlbfSource: GleanString;
    mlbfSoftblocksSource: GleanString;
    mlbfGenerationTime: GleanDatetime;
    mlbfSoftblocksGenerationTime: GleanDatetime;
    mlbfStashTimeOldest: GleanDatetime;
    mlbfStashTimeNewest: GleanDatetime;
    addonBlockChange: GleanEvent;
  }
}
