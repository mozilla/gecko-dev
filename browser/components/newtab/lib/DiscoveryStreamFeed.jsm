/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm"
);
const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);
ChromeUtils.defineModuleGetter(
  this,
  "perfService",
  "resource://activity-stream/common/PerfService.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UserDomainAffinityProvider",
  "resource://activity-stream/lib/UserDomainAffinityProvider.jsm"
);
const { actionTypes: at, actionCreators: ac } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PersistentCache",
  "resource://activity-stream/lib/PersistentCache.jsm"
);
XPCOMUtils.defineLazyServiceGetters(this, {
  gUUIDGenerator: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
});

const CACHE_KEY = "discovery_stream";
const LAYOUT_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const STARTUP_CACHE_EXPIRE_TIME = 7 * 24 * 60 * 60 * 1000; // 1 week
const COMPONENT_FEEDS_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const SPOCS_FEEDS_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const DEFAULT_RECS_EXPIRE_TIME = 60 * 60 * 1000; // 1 hour
const MIN_DOMAIN_AFFINITIES_UPDATE_TIME = 12 * 60 * 60 * 1000; // 12 hours
const MAX_LIFETIME_CAP = 500; // Guard against misconfiguration on the server
const DEFAULT_MAX_HISTORY_QUERY_RESULTS = 1000;
const FETCH_TIMEOUT = 45 * 1000;
const PREF_CONFIG = "discoverystream.config";
const PREF_ENDPOINTS = "discoverystream.endpoints";
const PREF_IMPRESSION_ID = "browser.newtabpage.activity-stream.impressionId";
const PREF_ENABLED = "discoverystream.enabled";
const PREF_HARDCODED_BASIC_LAYOUT = "discoverystream.hardcoded-basic-layout";
const PREF_SPOCS_ENDPOINT = "discoverystream.spocs-endpoint";
const PREF_TOPSTORIES = "feeds.section.topstories";
const PREF_SPOCS_CLEAR_ENDPOINT = "discoverystream.endpointSpocsClear";
const PREF_SHOW_SPONSORED = "showSponsored";
const PREF_SPOC_IMPRESSIONS = "discoverystream.spoc.impressions";
const PREF_CAMPAIGN_BLOCKS = "discoverystream.campaign.blocks";
const PREF_REC_IMPRESSIONS = "discoverystream.rec.impressions";

let getHardcodedLayout;

this.DiscoveryStreamFeed = class DiscoveryStreamFeed {
  constructor() {
    // Internal state for checking if we've intialized all our data
    this.loaded = false;

    // Persistent cache for remote endpoint data.
    this.cache = new PersistentCache(CACHE_KEY, true);
    this._impressionId = this.getOrCreateImpressionId();
    // Internal in-memory cache for parsing json prefs.
    this._prefCache = {};
  }

  getOrCreateImpressionId() {
    let impressionId = Services.prefs.getCharPref(PREF_IMPRESSION_ID, "");
    if (!impressionId) {
      impressionId = String(gUUIDGenerator.generateUUID());
      Services.prefs.setCharPref(PREF_IMPRESSION_ID, impressionId);
    }
    return impressionId;
  }

  /**
   * Send SPOCS Fill telemetry.
   * @param {object} filteredItems An object keyed on filter reasons, and the value
   *                 is a list of SPOCS.
   *                 reasons: blocked_by_user, frequency_cap, below_min_score, campaign_duplicate
   * @param {boolean} fullRecalc A boolean indicating if it's a full recalculation.
   *                  Calling `loadSpocs` will be treated as a full recalculation.
   *                  Whereas responding the action "DISCOVERY_STREAM_SPOC_IMPRESSION"
   *                  is not a full recalculation.
   */
  _sendSpocsFill(filteredItems, fullRecalc) {
    const full_recalc = fullRecalc ? 1 : 0;
    const spocsFill = [];
    for (const [reason, items] of Object.entries(filteredItems)) {
      items.forEach(item => {
        // Only send SPOCS (i.e. it has a campaign_id)
        if (item.campaign_id) {
          spocsFill.push({ reason, full_recalc, id: item.id, displayed: 0 });
        }
      });
    }

    if (spocsFill.length) {
      this.store.dispatch(
        ac.DiscoveryStreamSpocsFill({ spoc_fills: spocsFill })
      );
    }
  }

  finalLayoutEndpoint(url, apiKey) {
    if (url.includes("$apiKey") && !apiKey) {
      throw new Error(
        `Layout Endpoint - An API key was specified but none configured: ${url}`
      );
    }
    return url.replace("$apiKey", apiKey);
  }

  get config() {
    if (this._prefCache.config) {
      return this._prefCache.config;
    }
    try {
      this._prefCache.config = JSON.parse(
        this.store.getState().Prefs.values[PREF_CONFIG]
      );
      const layoutUrl = this._prefCache.config.layout_endpoint;

      const apiKeyPref = this._prefCache.config.api_key_pref;
      if (layoutUrl && apiKeyPref) {
        const apiKey = Services.prefs.getCharPref(apiKeyPref, "");
        this._prefCache.config.layout_endpoint = this.finalLayoutEndpoint(
          layoutUrl,
          apiKey
        );
      }
    } catch (e) {
      // istanbul ignore next
      this._prefCache.config = {};
      // istanbul ignore next
      Cu.reportError(
        `Could not parse preference. Try resetting ${PREF_CONFIG} in about:config. ${e}`
      );
    }
    this._prefCache.config.enabled =
      this._prefCache.config.enabled &&
      this.store.getState().Prefs.values[PREF_ENABLED];

    return this._prefCache.config;
  }

  resetConfigDefauts() {
    this.store.dispatch({
      type: at.CLEAR_PREF,
      data: {
        name: PREF_CONFIG,
      },
    });
  }

  get showSpocs() {
    // Combine user-set sponsored opt-out with Mozilla-set config
    return (
      this.store.getState().Prefs.values[PREF_SHOW_SPONSORED] &&
      this.config.show_spocs
    );
  }

  get personalized() {
    return this.config.personalized;
  }

  setupPrefs() {
    // Send the initial state of the pref on our reducer
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_CONFIG_SETUP,
        data: this.config,
      })
    );
  }

  uninitPrefs() {
    // Reset in-memory cache
    this._prefCache = {};
  }

  async fetchFromEndpoint(rawEndpoint, options = {}) {
    if (!rawEndpoint) {
      Cu.reportError("Tried to fetch endpoint but none was configured.");
      return null;
    }

    const apiKeyPref = this._prefCache.config.api_key_pref;
    const apiKey = Services.prefs.getCharPref(apiKeyPref, "");

    // The server somtimes returns this value already replaced, but we try this for two reasons:
    // 1. Layout endpoints are not from the server.
    // 2. Hardcoded layouts don't have this already done for us.
    const endpoint = rawEndpoint.replace("$apiKey", apiKey);

    try {
      // Make sure the requested endpoint is allowed
      const allowed = this.store
        .getState()
        .Prefs.values[PREF_ENDPOINTS].split(",");
      if (!allowed.some(prefix => endpoint.startsWith(prefix))) {
        throw new Error(`Not one of allowed prefixes (${allowed})`);
      }

      const controller = new AbortController();
      const { signal } = controller;

      const fetchPromise = fetch(endpoint, {
        ...options,
        credentials: "omit",
        signal,
      });
      // istanbul ignore next
      const timeoutId = setTimeout(() => {
        controller.abort();
      }, FETCH_TIMEOUT);

      const response = await fetchPromise;
      if (!response.ok) {
        throw new Error(`Unexpected status (${response.status})`);
      }
      clearTimeout(timeoutId);
      return response.json();
    } catch (error) {
      Cu.reportError(`Failed to fetch ${endpoint}: ${error.message}`);
    }
    return null;
  }

  /**
   * Returns true if data in the cache for a particular key has expired or is missing.
   * @param {object} cachedData data returned from cache.get()
   * @param {string} key a cache key
   * @param {string?} url for "feed" only, the URL of the feed.
   * @param {boolean} is this check done at initial browser load
   */
  isExpired({ cachedData, key, url, isStartup }) {
    const { layout, spocs, feeds } = cachedData;
    const updateTimePerComponent = {
      layout: LAYOUT_UPDATE_TIME,
      spocs: SPOCS_FEEDS_UPDATE_TIME,
      feed: COMPONENT_FEEDS_UPDATE_TIME,
    };
    const EXPIRATION_TIME = isStartup
      ? STARTUP_CACHE_EXPIRE_TIME
      : updateTimePerComponent[key];
    switch (key) {
      case "layout":
        // This never needs to expire, as it's not expected to change.
        if (this.config.hardcoded_layout) {
          return false;
        }
        return !layout || !(Date.now() - layout.lastUpdated < EXPIRATION_TIME);
      case "spocs":
        return !spocs || !(Date.now() - spocs.lastUpdated < EXPIRATION_TIME);
      case "feed":
        return (
          !feeds ||
          !feeds[url] ||
          !(Date.now() - feeds[url].lastUpdated < EXPIRATION_TIME)
        );
      default:
        // istanbul ignore next
        throw new Error(`${key} is not a valid key`);
    }
  }

  async _checkExpirationPerComponent() {
    const cachedData = (await this.cache.get()) || {};
    const { feeds } = cachedData;
    return {
      layout: this.isExpired({ cachedData, key: "layout" }),
      spocs: this.isExpired({ cachedData, key: "spocs" }),
      feeds:
        !feeds ||
        Object.keys(feeds).some(url =>
          this.isExpired({ cachedData, key: "feed", url })
        ),
    };
  }

  /**
   * Returns true if any data for the cached endpoints has expired or is missing.
   */
  async checkIfAnyCacheExpired() {
    const expirationPerComponent = await this._checkExpirationPerComponent();
    return (
      expirationPerComponent.layout ||
      expirationPerComponent.spocs ||
      expirationPerComponent.feeds
    );
  }

  async fetchLayout(isStartup) {
    const cachedData = (await this.cache.get()) || {};
    let { layout } = cachedData;
    if (this.isExpired({ cachedData, key: "layout", isStartup })) {
      const start = perfService.absNow();
      const layoutResponse = await this.fetchFromEndpoint(
        this.config.layout_endpoint
      );
      if (layoutResponse && layoutResponse.layout) {
        this.layoutRequestTime = Math.round(perfService.absNow() - start);
        layout = {
          lastUpdated: Date.now(),
          spocs: layoutResponse.spocs,
          layout: layoutResponse.layout,
          status: "success",
        };

        await this.cache.set("layout", layout);
      } else {
        Cu.reportError("No response for response.layout prop");
      }
    }
    return layout;
  }

  updatePlacements(sendUpdate, layout) {
    const placements = [];
    const placementsMap = {};
    for (const row of layout.filter(r => r.components && r.components.length)) {
      for (const component of row.components) {
        if (component.placement) {
          // Throw away any dupes for the request.
          if (!placementsMap[component.placement.name]) {
            placementsMap[component.placement.name] = component.placement;
            placements.push(component.placement);
          }
        }
      }
    }
    if (placements.length) {
      sendUpdate({
        type: at.DISCOVERY_STREAM_SPOCS_PLACEMENTS,
        data: { placements },
      });
    }
  }

  async loadLayout(sendUpdate, isStartup) {
    let layoutResp = {};
    let url = "";
    if (!this.config.hardcoded_layout) {
      layoutResp = await this.fetchLayout(isStartup);
    }

    if (!layoutResp || !layoutResp.layout) {
      // Set a hardcoded layout if one is needed.
      // Changing values in this layout in memory object is unnecessary.
      layoutResp = getHardcodedLayout(
        this.config.hardcoded_basic_layout ||
          this.store.getState().Prefs.values[PREF_HARDCODED_BASIC_LAYOUT]
      );
    }

    sendUpdate({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: layoutResp,
    });

    if (layoutResp.spocs) {
      url =
        this.store.getState().Prefs.values[PREF_SPOCS_ENDPOINT] ||
        this.config.spocs_endpoint ||
        layoutResp.spocs.url;

      if (
        url &&
        url !== this.store.getState().DiscoveryStream.spocs.spocs_endpoint
      ) {
        sendUpdate({
          type: at.DISCOVERY_STREAM_SPOCS_ENDPOINT,
          data: {
            url,
            spocs_per_domain: layoutResp.spocs.spocs_per_domain,
          },
        });
      }
    }
  }

  /**
   * buildFeedPromise - Adds the promise result to newFeeds and
   *                    pushes a promise to newsFeedsPromises.
   * @param {Object} Has both newFeedsPromises (Array) and newFeeds (Object)
   * @param {Boolean} isStartup We have different cache handling for startup.
   * @returns {Function} We return a function so we can contain
   *                     the scope for isStartup and the promises object.
   *                     Combines feed results and promises for each component with a feed.
   */
  buildFeedPromise({ newFeedsPromises, newFeeds }, isStartup, sendUpdate) {
    return component => {
      const { url } = component.feed;

      if (!newFeeds[url]) {
        // We initially stub this out so we don't fetch dupes,
        // we then fill in with the proper object inside the promise.
        newFeeds[url] = {};
        const feedPromise = this.getComponentFeed(url, isStartup);

        feedPromise
          .then(feed => {
            newFeeds[url] = this.filterRecommendations(feed);
            sendUpdate({
              type: at.DISCOVERY_STREAM_FEED_UPDATE,
              data: {
                feed: newFeeds[url],
                url,
              },
            });

            // We grab affinities off the first feed for the moment.
            // Ideally this would be returned from the server on the layout,
            // or from another endpoint.
            if (!this.affinities) {
              const { settings } = feed.data;
              this.affinities = {
                timeSegments: settings.timeSegments,
                parameterSets: settings.domainAffinityParameterSets,
                maxHistoryQueryResults:
                  settings.maxHistoryQueryResults ||
                  DEFAULT_MAX_HISTORY_QUERY_RESULTS,
                version: settings.version,
              };
            }
          })
          .catch(
            /* istanbul ignore next */ error => {
              Cu.reportError(
                `Error trying to load component feed ${url}: ${error}`
              );
            }
          );
        newFeedsPromises.push(feedPromise);
      }
    };
  }

  filterRecommendations(feed) {
    if (
      feed &&
      feed.data &&
      feed.data.recommendations &&
      feed.data.recommendations.length
    ) {
      const { data: recommendations } = this.filterBlocked(
        feed.data.recommendations
      );
      return {
        ...feed,
        data: {
          ...feed.data,
          recommendations,
        },
      };
    }
    return feed;
  }

  /**
   * reduceFeedComponents - Filters out components with no feeds, and combines
   *                        all feeds on this component with the feeds from other components.
   * @param {Boolean} isStartup We have different cache handling for startup.
   * @returns {Function} We return a function so we can contain the scope for isStartup.
   *                     Reduces feeds into promises and feed data.
   */
  reduceFeedComponents(isStartup, sendUpdate) {
    return (accumulator, row) => {
      row.components
        .filter(component => component && component.feed)
        .forEach(this.buildFeedPromise(accumulator, isStartup, sendUpdate));
      return accumulator;
    };
  }

  /**
   * buildFeedPromises - Filters out rows with no components,
   *                     and gets us a promise for each unique feed.
   * @param {Object} layout This is the Discovery Stream layout object.
   * @param {Boolean} isStartup We have different cache handling for startup.
   * @returns {Object} An object with newFeedsPromises (Array) and newFeeds (Object),
   *                   we can Promise.all newFeedsPromises to get completed data in newFeeds.
   */
  buildFeedPromises(layout, isStartup, sendUpdate) {
    const initialData = {
      newFeedsPromises: [],
      newFeeds: {},
    };
    return layout
      .filter(row => row && row.components)
      .reduce(this.reduceFeedComponents(isStartup, sendUpdate), initialData);
  }

  async loadComponentFeeds(sendUpdate, isStartup) {
    const { DiscoveryStream } = this.store.getState();

    if (!DiscoveryStream || !DiscoveryStream.layout) {
      return;
    }

    // Reset the flag that indicates whether or not at least one API request
    // was issued to fetch the component feed in `getComponentFeed()`.
    this.componentFeedFetched = false;
    const start = perfService.absNow();
    const { newFeedsPromises, newFeeds } = this.buildFeedPromises(
      DiscoveryStream.layout,
      isStartup,
      sendUpdate
    );

    // Each promise has a catch already built in, so no need to catch here.
    await Promise.all(newFeedsPromises);

    if (this.componentFeedFetched) {
      this.cleanUpTopRecImpressionPref(newFeeds);
      this.componentFeedRequestTime = Math.round(perfService.absNow() - start);
    }
    await this.cache.set("feeds", newFeeds);
    sendUpdate({
      type: at.DISCOVERY_STREAM_FEEDS_UPDATE,
    });
  }

  placementsForEach(callback) {
    const { placements } = this.store.getState().DiscoveryStream.spocs;
    // Backwards comp for before we had placements, assume just a single spocs placement.
    if (!placements || !placements.length) {
      [{ name: "spocs" }].forEach(callback);
    } else {
      placements.forEach(callback);
    }
  }

  async loadSpocs(sendUpdate, isStartup) {
    const cachedData = (await this.cache.get()) || {};
    let spocsState;

    const { placements } = this.store.getState().DiscoveryStream.spocs;

    if (this.showSpocs) {
      spocsState = cachedData.spocs;
      if (this.isExpired({ cachedData, key: "spocs", isStartup })) {
        const endpoint = this.store.getState().DiscoveryStream.spocs
          .spocs_endpoint;
        const start = perfService.absNow();

        const headers = new Headers();
        headers.append("content-type", "application/json");

        const apiKeyPref = this._prefCache.config.api_key_pref;
        const apiKey = Services.prefs.getCharPref(apiKeyPref, "");

        const spocsResponse = await this.fetchFromEndpoint(endpoint, {
          method: "POST",
          headers,
          body: JSON.stringify({
            pocket_id: this._impressionId,
            version: 1,
            consumer_key: apiKey,
            ...(placements.length ? { placements } : {}),
          }),
        });

        if (spocsResponse) {
          this.spocsRequestTime = Math.round(perfService.absNow() - start);
          spocsState = {
            lastUpdated: Date.now(),
            spocs: {
              ...spocsResponse,
            },
          };

          this.cleanUpCampaignImpressionPref(spocsState.spocs);
          await this.cache.set("spocs", spocsState);
        } else {
          Cu.reportError("No response for spocs_endpoint prop");
        }
      }
    }

    // Use good data if we have it, otherwise nothing.
    // We can have no data if spocs set to off.
    // We can have no data if request fails and there is no good cache.
    // We want to send an update spocs or not, so client can render something.
    spocsState =
      spocsState && spocsState.spocs
        ? spocsState
        : {
            lastUpdated: Date.now(),
            spocs: {},
          };

    let frequencyCapped = [];
    let blockedItems = [];
    let belowMinScore = [];
    let campaignDupes = [];
    this.placementsForEach(placement => {
      const freshSpocs = spocsState.spocs[placement.name];

      if (!freshSpocs || !freshSpocs.length) {
        return;
      }

      const { data: capResult, filtered: caps } = this.frequencyCapSpocs(
        freshSpocs
      );
      frequencyCapped = [...frequencyCapped, ...caps];

      const { data: blockedResults, filtered: blocks } = this.filterBlocked(
        capResult
      );
      blockedItems = [...blockedItems, ...blocks];

      let { data: transformResult, filtered: transformFilter } = this.transform(
        blockedResults
      );
      let {
        below_min_score: minScoreFilter,
        campaign_duplicate: dupes,
      } = transformFilter;
      belowMinScore = [...belowMinScore, ...minScoreFilter];
      campaignDupes = [...campaignDupes, ...dupes];

      spocsState.spocs = {
        ...spocsState.spocs,
        [placement.name]: transformResult,
      };
    });

    sendUpdate({
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: {
        lastUpdated: spocsState.lastUpdated,
        spocs: spocsState.spocs,
      },
    });
    // TODO make sure this works in other places we use it.
    // TODO make sure to also validate all of these that they still contain the right ites in the array.
    this._sendSpocsFill(
      {
        frequency_cap: frequencyCapped,
        blocked_by_user: blockedItems,
        below_min_score: belowMinScore,
        campaign_duplicate: campaignDupes,
      },
      true
    );
  }

  async clearSpocs() {
    const endpoint = this.store.getState().Prefs.values[
      PREF_SPOCS_CLEAR_ENDPOINT
    ];
    if (!endpoint) {
      return;
    }
    const headers = new Headers();
    headers.append("content-type", "application/json");

    await this.fetchFromEndpoint(endpoint, {
      method: "DELETE",
      headers,
      body: JSON.stringify({
        pocket_id: this._impressionId,
      }),
    });
  }

  async loadAffinityScoresCache() {
    const cachedData = (await this.cache.get()) || {};
    const { affinities } = cachedData;
    if (this.personalized && affinities && affinities.scores) {
      this.affinityProvider = new UserDomainAffinityProvider(
        affinities.timeSegments,
        affinities.parameterSets,
        affinities.maxHistoryQueryResults,
        affinities.version,
        affinities.scores
      );

      this.domainAffinitiesLastUpdated = affinities._timestamp;
    }
  }

  updateDomainAffinityScores() {
    if (
      !this.personalized ||
      !this.affinities ||
      !this.affinities.parameterSets ||
      Date.now() - this.domainAffinitiesLastUpdated <
        MIN_DOMAIN_AFFINITIES_UPDATE_TIME
    ) {
      return;
    }

    this.affinityProvider = new UserDomainAffinityProvider(
      this.affinities.timeSegments,
      this.affinities.parameterSets,
      this.affinities.maxHistoryQueryResults,
      this.affinities.version,
      undefined
    );

    const affinities = this.affinityProvider.getAffinities();
    this.domainAffinitiesLastUpdated = Date.now();
    affinities._timestamp = this.domainAffinitiesLastUpdated;
    this.cache.set("affinities", affinities);
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "idle-daily":
        this.updateDomainAffinityScores();
        break;
    }
  }

  scoreItems(items) {
    const filtered = [];
    const data = items
      .map(item => this.scoreItem(item))
      // Remove spocs that are scored too low.
      .filter(s => {
        if (s.score >= s.min_score) {
          return true;
        }
        filtered.push(s);
        return false;
      })
      // Sort by highest scores.
      .sort((a, b) => b.score - a.score);
    return { data, filtered };
  }

  scoreItem(item) {
    item.score = item.item_score;
    item.min_score = item.min_score || 0;
    if (item.score !== 0 && !item.score) {
      item.score = 1;
    }
    if (this.personalized && this.affinityProvider) {
      const scoreResult = this.affinityProvider.calculateItemRelevanceScore(
        item
      );
      if (scoreResult === 0 || scoreResult) {
        item.score = scoreResult;
      }
    }
    return item;
  }

  filterBlocked(data) {
    const filtered = [];
    if (data && data.length) {
      let campaigns = this.readDataPref(PREF_CAMPAIGN_BLOCKS);
      const filteredItems = data.filter(item => {
        const blocked =
          NewTabUtils.blockedLinks.isBlocked({ url: item.url }) ||
          campaigns[item.campaign_id];
        if (blocked) {
          filtered.push(item);
        }
        return !blocked;
      });
      return {
        data: filteredItems,
        filtered,
      };
    }
    return { data, filtered };
  }

  transform(spocs) {
    if (spocs && spocs.length) {
      const spocsPerDomain =
        this.store.getState().DiscoveryStream.spocs.spocs_per_domain || 1;
      const campaignMap = {};
      const campaignDuplicates = [];

      // This order of operations is intended.
      // scoreItems must be first because it creates this.score.
      const { data: items, filtered: belowMinScoreItems } = this.scoreItems(
        spocs
      );
      // This removes campaign dupes.
      // We do this only after scoring and sorting because that way
      // we can keep the first item we see, and end up keeping the highest scored.
      const newSpocs = items.filter(s => {
        if (!campaignMap[s.campaign_id]) {
          campaignMap[s.campaign_id] = 1;
          return true;
        } else if (campaignMap[s.campaign_id] < spocsPerDomain) {
          campaignMap[s.campaign_id]++;
          return true;
        }
        campaignDuplicates.push(s);
        return false;
      });
      return {
        data: newSpocs,
        filtered: {
          below_min_score: belowMinScoreItems,
          campaign_duplicate: campaignDuplicates,
        },
      };
    }
    return {
      data: spocs,
      filtered: {
        below_min_score: [],
        campaign_duplicate: [],
      },
    };
  }

  // Filter spocs based on frequency caps
  //
  // @param {Object} data  An object that might have a SPOCS array.
  // @returns {Object} An object with a property `data` as the result, and a property
  //                   `filterItems` as the frequency capped items.
  frequencyCapSpocs(spocs) {
    if (spocs && spocs.length) {
      const impressions = this.readDataPref(PREF_SPOC_IMPRESSIONS);
      const caps = [];
      const result = spocs.filter(s => {
        const isBelow = this.isBelowFrequencyCap(impressions, s);
        if (!isBelow) {
          caps.push(s);
        }
        return isBelow;
      });
      // send caps to redux if any.
      if (caps.length) {
        this.store.dispatch({
          type: at.DISCOVERY_STREAM_SPOCS_CAPS,
          data: caps,
        });
      }
      return { data: result, filtered: caps };
    }
    return { data: spocs, filtered: [] };
  }

  // Frequency caps are based on campaigns, which may include multiple spocs.
  // We currently support two types of frequency caps:
  // - lifetime: Indicates how many times spocs from a campaign can be shown in total
  // - period: Indicates how many times spocs from a campaign can be shown within a period
  //
  // So, for example, the feed configuration below defines that for campaign 1 no more
  // than 5 spocs can be shown in total, and no more than 2 per hour.
  // "campaign_id": 1,
  // "caps": {
  //  "lifetime": 5,
  //  "campaign": {
  //    "count": 2,
  //    "period": 3600
  //  }
  // }
  isBelowFrequencyCap(impressions, spoc) {
    const campaignImpressions = impressions[spoc.campaign_id];
    if (!campaignImpressions) {
      return true;
    }

    const lifetime = spoc.caps && spoc.caps.lifetime;

    const lifeTimeCap = Math.min(
      lifetime || MAX_LIFETIME_CAP,
      MAX_LIFETIME_CAP
    );
    const lifeTimeCapExceeded = campaignImpressions.length >= lifeTimeCap;
    if (lifeTimeCapExceeded) {
      return false;
    }

    const campaignCap = spoc.caps && spoc.caps.campaign;
    if (campaignCap) {
      const campaignCapExceeded =
        campaignImpressions.filter(
          i => Date.now() - i < campaignCap.period * 1000
        ).length >= campaignCap.count;
      return !campaignCapExceeded;
    }
    return true;
  }

  async retryFeed(feed) {
    const { url } = feed;
    const result = await this.getComponentFeed(url);
    const newFeed = this.filterRecommendations(result);
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_FEED_UPDATE,
        data: {
          feed: newFeed,
          url,
        },
      })
    );
  }

  async getComponentFeed(feedUrl, isStartup) {
    const cachedData = (await this.cache.get()) || {};
    const { feeds } = cachedData;

    let feed = feeds ? feeds[feedUrl] : null;
    if (this.isExpired({ cachedData, key: "feed", url: feedUrl, isStartup })) {
      const feedResponse = await this.fetchFromEndpoint(feedUrl);
      if (feedResponse) {
        const { data: scoredItems } = this.scoreItems(
          feedResponse.recommendations
        );
        const { recsExpireTime } = feedResponse.settings;
        const recommendations = this.rotate(scoredItems, recsExpireTime);
        this.componentFeedFetched = true;
        feed = {
          lastUpdated: Date.now(),
          data: {
            settings: feedResponse.settings,
            recommendations,
            status: "success",
          },
        };
      } else {
        Cu.reportError("No response for feed");
      }
    }

    // If we have no feed at this point, both fetch and cache failed for some reason.
    return (
      feed || {
        data: {
          status: "failed",
        },
      }
    );
  }

  /**
   * Called at startup to update cached data in the background.
   */
  async _maybeUpdateCachedData() {
    const expirationPerComponent = await this._checkExpirationPerComponent();
    // Pass in `store.dispatch` to send the updates only to main
    if (expirationPerComponent.layout) {
      await this.loadLayout(this.store.dispatch);
    }
    if (expirationPerComponent.spocs) {
      await this.loadSpocs(this.store.dispatch);
    }
    if (expirationPerComponent.feeds) {
      await this.loadComponentFeeds(this.store.dispatch);
    }
  }

  /**
   * @typedef {Object} RefreshAllOptions
   * @property {boolean} updateOpenTabs - Sends updates to open tabs immediately if true,
   *                                      updates in background if false
   * @property {boolean} isStartup - When the function is called at browser startup
   *
   * Refreshes layout, component feeds, and spocs in order if caches have expired.
   * @param {RefreshAllOptions} options
   */
  async refreshAll(options = {}) {
    const { updateOpenTabs, isStartup } = options;
    const dispatch = updateOpenTabs
      ? action => this.store.dispatch(ac.BroadcastToContent(action))
      : this.store.dispatch;

    this.loadAffinityScoresCache();
    await this.loadLayout(dispatch, isStartup);
    await Promise.all([
      this.loadSpocs(dispatch, isStartup).catch(error =>
        Cu.reportError(`Error trying to load spocs feeds: ${error}`)
      ),
      this.loadComponentFeeds(dispatch, isStartup).catch(error =>
        Cu.reportError(`Error trying to load component feeds: ${error}`)
      ),
    ]);
    if (isStartup) {
      await this._maybeUpdateCachedData();
    }
  }

  // We have to rotate stories on the client so that
  // active stories are at the front of the list, followed by stories that have expired
  // impressions i.e. have been displayed for longer than recsExpireTime.
  rotate(recommendations, recsExpireTime) {
    const maxImpressionAge = Math.max(
      recsExpireTime * 1000 || DEFAULT_RECS_EXPIRE_TIME,
      DEFAULT_RECS_EXPIRE_TIME
    );
    const impressions = this.readDataPref(PREF_REC_IMPRESSIONS);
    const expired = [];
    const active = [];
    for (const item of recommendations) {
      if (
        impressions[item.id] &&
        Date.now() - impressions[item.id] >= maxImpressionAge
      ) {
        expired.push(item);
      } else {
        active.push(item);
      }
    }
    return active.concat(expired);
  }

  /**
   * Reports the cache age in second for Discovery Stream.
   */
  async reportCacheAge() {
    const cachedData = (await this.cache.get()) || {};
    const { layout, spocs, feeds } = cachedData;
    let cacheAge = Date.now();
    let updated = false;

    if (layout && layout.lastUpdated && layout.lastUpdated < cacheAge) {
      updated = true;
      cacheAge = layout.lastUpdated;
    }

    if (spocs && spocs.lastUpdated && spocs.lastUpdated < cacheAge) {
      updated = true;
      cacheAge = spocs.lastUpdated;
    }

    if (feeds) {
      Object.keys(feeds).forEach(url => {
        const feed = feeds[url];
        if (feed.lastUpdated && feed.lastUpdated < cacheAge) {
          updated = true;
          cacheAge = feed.lastUpdated;
        }
      });
    }

    if (updated) {
      this.store.dispatch(
        ac.PerfEvent({
          event: "DS_CACHE_AGE_IN_SEC",
          value: Math.round((Date.now() - cacheAge) / 1000),
        })
      );
    }
  }

  /**
   * Reports various time durations when the feed is requested from endpoint for
   * the first time. This could happen on the browser start-up, or the pref changes
   * of discovery stream.
   *
   * Metrics to be reported:
   *   - Request time for layout endpoint
   *   - Request time for feed endpoint
   *   - Request time for spoc endpoint
   *   - Total request time for data completeness
   */
  reportRequestTime() {
    if (this.layoutRequestTime) {
      this.store.dispatch(
        ac.PerfEvent({
          event: "LAYOUT_REQUEST_TIME",
          value: this.layoutRequestTime,
        })
      );
    }
    if (this.spocsRequestTime) {
      this.store.dispatch(
        ac.PerfEvent({
          event: "SPOCS_REQUEST_TIME",
          value: this.spocsRequestTime,
        })
      );
    }
    if (this.componentFeedRequestTime) {
      this.store.dispatch(
        ac.PerfEvent({
          event: "COMPONENT_FEED_REQUEST_TIME",
          value: this.componentFeedRequestTime,
        })
      );
    }
    if (this.totalRequestTime) {
      this.store.dispatch(
        ac.PerfEvent({
          event: "DS_FEED_TOTAL_REQUEST_TIME",
          value: this.totalRequestTime,
        })
      );
    }
  }

  async enable() {
    // Note that cache age needs to be reported prior to refreshAll.
    await this.reportCacheAge();
    const start = perfService.absNow();
    await this.refreshAll({ updateOpenTabs: true, isStartup: true });
    Services.obs.addObserver(this, "idle-daily");
    this.loaded = true;
    this.totalRequestTime = Math.round(perfService.absNow() - start);
    this.reportRequestTime();
  }

  async reset() {
    this.resetDataPrefs();
    await this.resetCache();
    if (this.loaded) {
      Services.obs.removeObserver(this, "idle-daily");
    }
    this.resetState();
  }

  async resetCache() {
    await this.cache.set("layout", {});
    await this.cache.set("feeds", {});
    await this.cache.set("spocs", {});
    await this.cache.set("affinities", {});
  }

  resetDataPrefs() {
    this.writeDataPref(PREF_SPOC_IMPRESSIONS, {});
    this.writeDataPref(PREF_REC_IMPRESSIONS, {});
    this.writeDataPref(PREF_CAMPAIGN_BLOCKS, {});
  }

  resetState() {
    // Reset reducer
    this.store.dispatch(
      ac.BroadcastToContent({ type: at.DISCOVERY_STREAM_LAYOUT_RESET })
    );
    this.loaded = false;
    this.layoutRequestTime = undefined;
    this.spocsRequestTime = undefined;
    this.componentFeedRequestTime = undefined;
    this.totalRequestTime = undefined;
  }

  async onPrefChange() {
    // We always want to clear the cache/state if the pref has changed
    await this.reset();
    if (this.config.enabled) {
      // Load data from all endpoints
      await this.enable();
    }
  }

  recordCampaignImpression(campaignId) {
    let impressions = this.readDataPref(PREF_SPOC_IMPRESSIONS);

    const timeStamps = impressions[campaignId] || [];
    timeStamps.push(Date.now());
    impressions = { ...impressions, [campaignId]: timeStamps };

    this.writeDataPref(PREF_SPOC_IMPRESSIONS, impressions);
  }

  recordTopRecImpressions(recId) {
    let impressions = this.readDataPref(PREF_REC_IMPRESSIONS);
    if (!impressions[recId]) {
      impressions = { ...impressions, [recId]: Date.now() };
      this.writeDataPref(PREF_REC_IMPRESSIONS, impressions);
    }
  }

  recordBlockCampaignId(campaignId) {
    const campaigns = this.readDataPref(PREF_CAMPAIGN_BLOCKS);
    if (!campaigns[campaignId]) {
      campaigns[campaignId] = 1;
      this.writeDataPref(PREF_CAMPAIGN_BLOCKS, campaigns);
    }
  }

  cleanUpCampaignImpressionPref(data) {
    let campaignIds = [];
    this.placementsForEach(placement => {
      const newSpocs = data[placement.name];
      if (!newSpocs) {
        return;
      }
      campaignIds = [...campaignIds, ...newSpocs.map(s => `${s.campaign_id}`)];
    });
    if (campaignIds && campaignIds.length) {
      this.cleanUpImpressionPref(
        id => !campaignIds.includes(id),
        PREF_SPOC_IMPRESSIONS
      );
    }
  }

  // Clean up rec impression pref by removing all stories that are no
  // longer part of the response.
  cleanUpTopRecImpressionPref(newFeeds) {
    // Need to build a single list of stories.
    const activeStories = Object.keys(newFeeds)
      .filter(currentValue => newFeeds[currentValue].data)
      .reduce((accumulator, currentValue) => {
        const { recommendations } = newFeeds[currentValue].data;
        return accumulator.concat(recommendations.map(i => `${i.id}`));
      }, []);
    this.cleanUpImpressionPref(
      id => !activeStories.includes(id),
      PREF_REC_IMPRESSIONS
    );
  }

  writeDataPref(pref, impressions) {
    this.store.dispatch(ac.SetPref(pref, JSON.stringify(impressions)));
  }

  readDataPref(pref) {
    const prefVal = this.store.getState().Prefs.values[pref];
    return prefVal ? JSON.parse(prefVal) : {};
  }

  cleanUpImpressionPref(isExpired, pref) {
    const impressions = this.readDataPref(pref);
    let changed = false;

    Object.keys(impressions).forEach(id => {
      if (isExpired(id)) {
        changed = true;
        delete impressions[id];
      }
    });

    if (changed) {
      this.writeDataPref(pref, impressions);
    }
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        // During the initialization of Firefox:
        // 1. Set-up listeners and initialize the redux state for config;
        this.setupPrefs();
        // 2. If config.enabled is true, start loading data.
        if (this.config.enabled) {
          await this.enable();
        }
        break;
      case at.SYSTEM_TICK:
        // Only refresh if we loaded once in .enable()
        if (
          this.config.enabled &&
          this.loaded &&
          (await this.checkIfAnyCacheExpired())
        ) {
          await this.refreshAll({ updateOpenTabs: false });
        }
        break;
      case at.DISCOVERY_STREAM_CONFIG_SET_VALUE:
        // Use the original string pref to then set a value instead of
        // this.config which has some modifications
        this.store.dispatch(
          ac.SetPref(
            PREF_CONFIG,
            JSON.stringify({
              ...JSON.parse(this.store.getState().Prefs.values[PREF_CONFIG]),
              [action.data.name]: action.data.value,
            })
          )
        );
        break;
      case at.DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS:
        this.resetConfigDefauts();
        break;
      case at.DISCOVERY_STREAM_RETRY_FEED:
        this.retryFeed(action.data.feed);
        break;
      case at.DISCOVERY_STREAM_CONFIG_CHANGE:
        // When the config pref changes, load or unload data as needed.
        await this.onPrefChange();
        break;
      case at.DISCOVERY_STREAM_IMPRESSION_STATS:
        if (
          action.data.tiles &&
          action.data.tiles[0] &&
          action.data.tiles[0].id
        ) {
          this.recordTopRecImpressions(action.data.tiles[0].id);
        }
        break;
      case at.DISCOVERY_STREAM_SPOC_IMPRESSION:
        if (this.showSpocs) {
          this.recordCampaignImpression(action.data.campaignId);

          // Apply frequency capping to SPOCs in the redux store, only update the
          // store if the SPOCs are changed.
          const spocsState = this.store.getState().DiscoveryStream.spocs;

          let frequencyCapped = [];
          this.placementsForEach(placement => {
            const freshSpocs = spocsState.data[placement.name];
            if (!freshSpocs) {
              return;
            }

            const { data: newSpocs, filtered } = this.frequencyCapSpocs(
              freshSpocs
            );
            frequencyCapped = [...frequencyCapped, ...filtered];

            spocsState.data = {
              ...spocsState.data,
              [placement.name]: newSpocs,
            };
          });
          if (frequencyCapped.length) {
            this.store.dispatch(
              ac.AlsoToPreloaded({
                type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
                data: {
                  lastUpdated: spocsState.lastUpdated,
                  spocs: spocsState.data,
                },
              })
            );
            this._sendSpocsFill({ frequency_cap: frequencyCapped }, false);
          }
        }
        break;
      // This is fired from the browser, it has no concept of spocs, campaign or pocket.
      // We match the blocked url with our available spoc urls to see if there is a match.
      // I suspect we *could* instead do this in BLOCK_URL but I'm not sure.
      case at.PLACES_LINK_BLOCKED:
        if (this.showSpocs) {
          const spocsState = this.store.getState().DiscoveryStream.spocs;
          let spocsList = [];
          this.placementsForEach(placement => {
            const spocs = spocsState.data[placement.name];
            if (spocs && spocs.length) {
              spocsList = [...spocsList, ...spocs];
            }
          });
          const filtered = spocsList.filter(s => s.url === action.data.url);
          if (filtered.length) {
            this._sendSpocsFill({ blocked_by_user: filtered }, false);

            // If we're blocking a spoc, we want a slightly different treatment for open tabs.
            // AlsoToPreloaded updates the source data and preloaded tabs with a new spoc.
            // BroadcastToContent updates open tabs with a non spoc instead of a new spoc.
            this.store.dispatch(
              ac.AlsoToPreloaded({
                type: at.DISCOVERY_STREAM_LINK_BLOCKED,
                data: action.data,
              })
            );
            this.store.dispatch(
              ac.BroadcastToContent({
                type: at.DISCOVERY_STREAM_SPOC_BLOCKED,
                data: action.data,
              })
            );
            break;
          }
        }
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.DISCOVERY_STREAM_LINK_BLOCKED,
            data: action.data,
          })
        );
        break;
      case at.UNINIT:
        // When this feed is shutting down:
        this.uninitPrefs();
        break;
      case at.BLOCK_URL: {
        // If we block a story that also has a campaign_id
        // we want to record that as blocked too.
        // This is because a single campaign might have slightly different urls.
        const { campaign_id } = action.data;
        if (campaign_id) {
          this.recordBlockCampaignId(campaign_id);
        }
        break;
      }
      case at.PREF_CHANGED:
        switch (action.data.name) {
          case PREF_CONFIG:
          case PREF_ENABLED:
          case PREF_HARDCODED_BASIC_LAYOUT:
          case PREF_SPOCS_ENDPOINT:
            // Clear the cached config and broadcast the newly computed value
            this._prefCache.config = null;
            this.store.dispatch(
              ac.BroadcastToContent({
                type: at.DISCOVERY_STREAM_CONFIG_CHANGE,
                data: this.config,
              })
            );
            break;
          case PREF_TOPSTORIES:
            if (!action.data.value) {
              // Ensure we delete any remote data potentially related to spocs.
              this.clearSpocs();
            }
            break;
          // Check if spocs was disabled. Remove them if they were.
          case PREF_SHOW_SPONSORED:
            if (!action.data.value) {
              // Ensure we delete any remote data potentially related to spocs.
              this.clearSpocs();
            }
            await this.loadSpocs(update =>
              this.store.dispatch(ac.BroadcastToContent(update))
            );
            break;
        }
        break;
    }
  }
};

// This function generates a hardcoded layout each call.
// This is because modifying the original object would
// persist across pref changes and system_tick updates.
getHardcodedLayout = basic => {
  if (basic) {
    // Hardcoded version of layout_variant `basic`
    return {
      lastUpdate: Date.now(),
      spocs: {
        url: "https://spocs.getpocket.com/spocs",
        spocs_per_domain: 1,
      },
      layout: [
        {
          width: 12,
          components: [
            {
              type: "TopSites",
              header: {
                title: "Top Sites",
              },
              properties: {},
            },
            {
              type: "Message",
              header: {
                title: "Recommended by Pocket",
                subtitle: "",
                link_text: "How it works",
                link_url: "https://getpocket.com/firefox/new_tab_learn_more",
                icon:
                  "resource://activity-stream/data/content/assets/glyph-pocket-16.svg",
              },
              properties: {},
              styles: {
                ".ds-message": "margin-bottom: -20px",
              },
            },
            {
              type: "CardGrid",
              properties: {
                items: 3,
              },
              header: {
                title: "",
              },
              feed: {
                embed_reference: null,
                url:
                  "https://getpocket.cdn.mozilla.net/v3/firefox/global-recs?version=3&consumer_key=$apiKey&locale_lang=en-US&feed_variant=default_spocs_on",
              },
              spocs: {
                probability: 1,
                positions: [
                  {
                    index: 2,
                  },
                ],
              },
            },
            {
              type: "Navigation",
              properties: {
                alignment: "left-align",
                links: [
                  {
                    name: "Must Reads",
                    url:
                      "https://getpocket.com/explore/must-reads?src=fx_new_tab",
                  },
                  {
                    name: "Productivity",
                    url:
                      "https://getpocket.com/explore/productivity?src=fx_new_tab",
                  },
                  {
                    name: "Health",
                    url: "https://getpocket.com/explore/health?src=fx_new_tab",
                  },
                  {
                    name: "Finance",
                    url: "https://getpocket.com/explore/finance?src=fx_new_tab",
                  },
                  {
                    name: "Technology",
                    url:
                      "https://getpocket.com/explore/technology?src=fx_new_tab",
                  },
                  {
                    name: "More Recommendations ›",
                    url:
                      "https://getpocket.com/explore/trending?src=fx_new_tab",
                  },
                ],
              },
            },
          ],
        },
      ],
    };
  }
  // Hardcoded version of layout_variant `3-col-7-row-octr`
  return {
    lastUpdate: Date.now(),
    spocs: {
      url: "https://spocs.getpocket.com/spocs",
      spocs_per_domain: 1,
    },
    layout: [
      {
        width: 12,
        components: [
          {
            type: "TopSites",
            header: {
              title: "Top Sites",
            },
          },
        ],
      },
      {
        width: 12,
        components: [
          {
            type: "Message",
            header: {
              title: "Recommended by Pocket",
              subtitle: "",
              link_text: "How it works",
              link_url: "https://getpocket.com/firefox/new_tab_learn_more",
              icon:
                "resource://activity-stream/data/content/assets/glyph-pocket-16.svg",
            },
            properties: {},
            styles: {
              ".ds-message": "margin-bottom: -20px",
            },
          },
        ],
      },
      {
        width: 12,
        components: [
          {
            type: "CardGrid",
            properties: {
              items: 21,
            },
            header: {
              title: "",
            },
            feed: {
              embed_reference: null,
              url:
                "https://getpocket.cdn.mozilla.net/v3/firefox/global-recs?version=3&consumer_key=$apiKey&locale_lang=en-US&count=30",
            },
            spocs: {
              probability: 1,
              positions: [
                {
                  index: 2,
                },
                {
                  index: 4,
                },
                {
                  index: 11,
                },
                {
                  index: 20,
                },
              ],
            },
          },
          {
            type: "Navigation",
            properties: {
              alignment: "left-align",
              links: [
                {
                  name: "Must Reads",
                  url:
                    "https://getpocket.com/explore/must-reads?src=fx_new_tab",
                },
                {
                  name: "Productivity",
                  url:
                    "https://getpocket.com/explore/productivity?src=fx_new_tab",
                },
                {
                  name: "Health",
                  url: "https://getpocket.com/explore/health?src=fx_new_tab",
                },
                {
                  name: "Finance",
                  url: "https://getpocket.com/explore/finance?src=fx_new_tab",
                },
                {
                  name: "Technology",
                  url:
                    "https://getpocket.com/explore/technology?src=fx_new_tab",
                },
                {
                  name: "More Recommendations ›",
                  url: "https://getpocket.com/explore/trending?src=fx_new_tab",
                },
              ],
            },
            header: {
              title: "Popular Topics",
            },
            styles: {
              ".ds-navigation": "margin-top: -10px;",
            },
          },
        ],
      },
    ],
  };
};

const EXPORTED_SYMBOLS = ["DiscoveryStreamFeed"];
