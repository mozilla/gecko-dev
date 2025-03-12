/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source metrics.yaml files.
 * If you're updating some of the sources, see README for instructions.
 */

interface GleanImpl {

  newtabSearch: {
    issued: GleanEvent;
  }

  newtabSearchAd: {
    impression: GleanEvent;
    click: GleanEvent;
  }

  sap: {
    counts: GleanEvent;
    deprecatedCounts: Record<string, GleanCounter>;
  }

  serp: {
    impression: GleanEvent;
    engagement: GleanEvent;
    adImpression: GleanEvent;
    abandonment: GleanEvent;
    categorizationDuration: GleanTimingDistribution;
    categorization: GleanEvent;
    adsBlockedCount: Record<string, GleanCounter>;
    experimentInfo: GleanObject;
    categorizationNoMapFound: GleanCounter;
  }

  searchWith: {
    reportingUrl: GleanUrl;
    contextId: GleanUuid;
  }

  browserEngagementNavigation: {
    urlbar: Record<string, GleanCounter>;
    urlbarHandoff: Record<string, GleanCounter>;
    urlbarPersisted: Record<string, GleanCounter>;
    urlbarSearchmode: Record<string, GleanCounter>;
    searchbar: Record<string, GleanCounter>;
    aboutHome: Record<string, GleanCounter>;
    aboutNewtab: Record<string, GleanCounter>;
    contextmenu: Record<string, GleanCounter>;
    webextension: Record<string, GleanCounter>;
  }

  browserSearchContent: {
    urlbar: Record<string, GleanCounter>;
    urlbarHandoff: Record<string, GleanCounter>;
    urlbarPersisted: Record<string, GleanCounter>;
    urlbarSearchmode: Record<string, GleanCounter>;
    searchbar: Record<string, GleanCounter>;
    aboutHome: Record<string, GleanCounter>;
    aboutNewtab: Record<string, GleanCounter>;
    contextmenu: Record<string, GleanCounter>;
    webextension: Record<string, GleanCounter>;
    system: Record<string, GleanCounter>;
    tabhistory: Record<string, GleanCounter>;
    reload: Record<string, GleanCounter>;
    unknown: Record<string, GleanCounter>;
  }

  browserSearchWithads: {
    urlbar: Record<string, GleanCounter>;
    urlbarHandoff: Record<string, GleanCounter>;
    urlbarPersisted: Record<string, GleanCounter>;
    urlbarSearchmode: Record<string, GleanCounter>;
    searchbar: Record<string, GleanCounter>;
    aboutHome: Record<string, GleanCounter>;
    aboutNewtab: Record<string, GleanCounter>;
    contextmenu: Record<string, GleanCounter>;
    webextension: Record<string, GleanCounter>;
    system: Record<string, GleanCounter>;
    tabhistory: Record<string, GleanCounter>;
    reload: Record<string, GleanCounter>;
    unknown: Record<string, GleanCounter>;
  }

  browserSearchAdclicks: {
    urlbar: Record<string, GleanCounter>;
    urlbarHandoff: Record<string, GleanCounter>;
    urlbarPersisted: Record<string, GleanCounter>;
    urlbarSearchmode: Record<string, GleanCounter>;
    searchbar: Record<string, GleanCounter>;
    aboutHome: Record<string, GleanCounter>;
    aboutNewtab: Record<string, GleanCounter>;
    contextmenu: Record<string, GleanCounter>;
    webextension: Record<string, GleanCounter>;
    system: Record<string, GleanCounter>;
    tabhistory: Record<string, GleanCounter>;
    reload: Record<string, GleanCounter>;
    unknown: Record<string, GleanCounter>;
  }

  urlbarSearchmode: {
    bookmarkmenu: Record<string, GleanCounter>;
    handoff: Record<string, GleanCounter>;
    keywordoffer: Record<string, GleanCounter>;
    oneoff: Record<string, GleanCounter>;
    searchbutton: Record<string, GleanCounter>;
    shortcut: Record<string, GleanCounter>;
    tabmenu: Record<string, GleanCounter>;
    tabtosearch: Record<string, GleanCounter>;
    tabtosearchOnboard: Record<string, GleanCounter>;
    topsitesNewtab: Record<string, GleanCounter>;
    topsitesUrlbar: Record<string, GleanCounter>;
    touchbar: Record<string, GleanCounter>;
    typed: Record<string, GleanCounter>;
    historymenu: Record<string, GleanCounter>;
    other: Record<string, GleanCounter>;
  }

  searchbar: {
    selectedResultMethod: Record<string, GleanCounter>;
  }
}
