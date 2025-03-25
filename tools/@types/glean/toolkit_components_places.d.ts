/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source metrics.yaml files.
 * If you're updating some of the sources, see README for instructions.
 */

interface GleanImpl {

  places: {
    placesDatabaseCorruptionHandlingStage: Record<string, GleanString>;
    sponsoredVisitNoTriggeringUrl: GleanCounter;
    pagesNeedFrecencyRecalculation: GleanQuantity;
    previousdayVisits: GleanQuantity;
    pagesCount: GleanCustomDistribution;
    mostRecentExpiredVisit: GleanTimingDistribution;
    bookmarksCount: GleanCustomDistribution;
    tagsCount: GleanCustomDistribution;
    keywordsCount: GleanCustomDistribution;
    backupsDaysfromlast: GleanTimingDistribution;
    backupsBookmarkstree: GleanTimingDistribution;
    backupsTojson: GleanTimingDistribution;
    exportTohtml: GleanTimingDistribution;
    sortedBookmarksPerc: GleanCustomDistribution;
    taggedBookmarksPerc: GleanCustomDistribution;
    databaseFilesize: GleanMemoryDistribution;
    databaseFaviconsFilesize: GleanMemoryDistribution;
    expirationStepsToClean: GleanCustomDistribution;
    idleFrecencyDecayTime: GleanTimingDistribution;
    idleMaintenanceTime: GleanTimingDistribution;
    frecencyRecalcChunkTime: GleanTimingDistribution;
    annosPagesCount: GleanCustomDistribution;
    maintenanceDaysfromlast: GleanTimingDistribution;
  }

  pageIcon: {
    smallIconCount: GleanCounter;
    fitIconCount: GleanCounter;
  }
}
