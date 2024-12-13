/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const selectLayoutRender = ({ state = {}, prefs = {} }) => {
  const { layout, feeds, spocs } = state;
  let spocIndexPlacementMap = {};

  /* This function fills spoc positions on a per placement basis with available spocs.
   * It does this by looping through each position for a placement and replacing a rec with a spoc.
   * If it runs out of spocs or positions, it stops.
   * If it sees the same placement again, it remembers the previous spoc index, and continues.
   * If it sees a blocked spoc, it skips that position leaving in a regular story.
   */
  function fillSpocPositionsForPlacement(
    data,
    spocsPositions,
    spocsData,
    placementName
  ) {
    if (
      !spocIndexPlacementMap[placementName] &&
      spocIndexPlacementMap[placementName] !== 0
    ) {
      spocIndexPlacementMap[placementName] = 0;
    }
    const results = [...data];
    for (let position of spocsPositions) {
      const spoc = spocsData[spocIndexPlacementMap[placementName]];
      // If there are no spocs left, we can stop filling positions.
      if (!spoc) {
        break;
      }

      // A placement could be used in two sections.
      // In these cases, we want to maintain the index of the previous section.
      // If we didn't do this, it might duplicate spocs.
      spocIndexPlacementMap[placementName]++;

      // A spoc that's blocked is removed from the source for subsequent newtab loads.
      // If we have a spoc in the source that's blocked, it means it was *just* blocked,
      // and in this case, we skip this position, and show a regular spoc instead.
      if (!spocs.blocked.includes(spoc.url)) {
        results.splice(position.index, 0, spoc);
      }
    }

    return results;
  }

  const positions = {};
  const DS_COMPONENTS = [
    "Message",
    "TextPromo",
    "SectionTitle",
    "Signup",
    "Navigation",
    "CardGrid",
    "CollectionCardGrid",
    "HorizontalRule",
    "PrivacyLink",
  ];

  const filterArray = [];

  if (!prefs["feeds.topsites"]) {
    filterArray.push("TopSites");
  }

  const pocketEnabled =
    prefs["feeds.section.topstories"] && prefs["feeds.system.topstories"];
  if (!pocketEnabled) {
    filterArray.push(...DS_COMPONENTS);
  }

  // function to determine amount of tiles shown per section per viewport
  function getMaxTiles(responsiveLayouts) {
    return responsiveLayouts
      .flatMap(responsiveLayout => responsiveLayout)
      .reduce((acc, t) => {
        acc[t.columnCount] = t.tiles.length;

        // Update maxTile if current tile count is greater
        if (!acc.maxTile || t.tiles.length > acc.maxTile) {
          acc.maxTile = t.tiles.length;
        }
        return acc;
      }, {});
  }

  const placeholderComponent = component => {
    if (!component.feed) {
      // TODO we now need a placeholder for topsites and textPromo.
      return {
        ...component,
        data: {
          spocs: [],
        },
      };
    }
    const data = {
      recommendations: [],
      sections: [
        {
          layout: {
            responsiveLayouts: [],
          },
          data: [],
        },
      ],
    };

    let items = 0;
    if (component.properties && component.properties.items) {
      items = component.properties.items;
    }
    for (let i = 0; i < items; i++) {
      data.recommendations.push({ placeholder: true });
    }

    const sectionsEnabled = prefs["discoverystream.sections.enabled"];
    if (sectionsEnabled) {
      for (let i = 0; i < items; i++) {
        data.sections[0].data.push({ placeholder: true });
      }
    }

    return { ...component, data };
  };

  // TODO update devtools to show placements
  const handleSpocs = (data = [], spocsPositions, spocsPlacement) => {
    let result = [...data];
    // Do we ever expect to possibly have a spoc.
    if (spocsPositions?.length) {
      const placement = spocsPlacement || {};
      const placementName = placement.name || "newtab_spocs";
      const spocsData = spocs.data[placementName];
      // We expect a spoc, spocs are loaded, and the server returned spocs.
      if (spocs.loaded && spocsData?.items?.length) {
        result = fillSpocPositionsForPlacement(
          result,
          spocsPositions,
          spocsData.items,
          placementName
        );
      }
    }
    return result;
  };

  const handleSections = (sections = [], recommendations = []) => {
    let result = sections.sort((a, b) => a.receivedRank - b.receivedRank);

    const sectionsMap = recommendations.reduce((acc, recommendation) => {
      const { section } = recommendation;
      acc[section] = acc[section] || [];
      acc[section].push(recommendation);
      return acc;
    }, {});

    result.forEach(section => {
      const { sectionKey } = section;
      section.data = sectionsMap[sectionKey];
    });

    return result;
  };

  const handleComponent = component => {
    if (component?.spocs?.positions?.length) {
      const placement = component.placement || {};
      const placementName = placement.name || "newtab_spocs";
      const spocsData = spocs.data[placementName];
      if (spocs.loaded && spocsData?.items?.length) {
        return {
          ...component,
          data: {
            spocs: spocsData.items
              .filter(spoc => spoc && !spocs.blocked.includes(spoc.url))
              .map((spoc, index) => ({
                ...spoc,
                pos: index,
              })),
          },
        };
      }
    }
    return {
      ...component,
      data: {
        spocs: [],
      },
    };
  };

  const handleComponentWithFeed = component => {
    positions[component.type] = positions[component.type] || 0;
    let data = {
      recommendations: [],
      sections: [],
    };

    const feed = feeds.data[component.feed.url];
    if (feed?.data) {
      data = {
        ...feed.data,
        recommendations: [...(feed.data.recommendations || [])],
        sections: [...(feed.data.sections || [])],
      };
    }

    if (component && component.properties && component.properties.offset) {
      data = {
        ...data,
        recommendations: data.recommendations.slice(
          component.properties.offset
        ),
      };
    }
    const spocsPositions = component?.spocs?.positions;
    const spocsPlacement = component?.placement;

    const sectionsEnabled = prefs["discoverystream.sections.enabled"];
    data = {
      ...data,
      ...(sectionsEnabled
        ? {
            sections: handleSections(data.sections, data.recommendations).map(
              section => {
                const sectionsSpocsPositions = [];
                section.layout.responsiveLayouts
                  // Initial position for spocs is going to be for the smallest breakpoint.
                  // We can then move it from there via breakpoints.
                  .find(item => item.columnCount === 1)
                  .tiles.forEach(tile => {
                    if (tile.hasAd) {
                      sectionsSpocsPositions.push({ index: tile.position });
                    }
                  });
                return {
                  ...section,
                  data: handleSpocs(
                    section.data,
                    sectionsSpocsPositions,
                    spocsPlacement
                  ),
                };
              }
            ),
            // We don't fill spocs in recs if sections are enabled,
            // because recs are not going to be seen.
            recommendations: data.recommendations,
          }
        : {
            recommendations: handleSpocs(
              data.recommendations,
              spocsPositions,
              spocsPlacement
            ),
          }),
    };

    let items = 0;
    if (component.properties && component.properties.items) {
      items = Math.min(component.properties.items, data.recommendations.length);
    }

    // loop through a component items
    // Store the items position sequentially for multiple components of the same type.
    // Example: A second card grid starts pos offset from the last card grid.
    for (let i = 0; i < items; i++) {
      data.recommendations[i] = {
        ...data.recommendations[i],
        pos: positions[component.type]++,
      };
    }

    // Setup absolute positions for sections layout.
    if (sectionsEnabled) {
      let currentPosition = 0;
      data.sections.forEach(section => {
        // We assume the count for the breakpoint with the most tiles.
        const { maxTile } = getMaxTiles(section?.layout?.responsiveLayouts);
        for (let i = 0; i < maxTile; i++) {
          if (section.data[i]) {
            section.data[i] = {
              ...section.data[i],
              pos: currentPosition++,
            };
          }
        }
      });
    }

    return { ...component, data };
  };

  const renderLayout = () => {
    const renderedLayoutArray = [];
    for (const row of layout.filter(
      r => r.components.filter(c => !filterArray.includes(c.type)).length
    )) {
      let components = [];
      renderedLayoutArray.push({
        ...row,
        components,
      });
      for (const component of row.components.filter(
        c => !filterArray.includes(c.type)
      )) {
        const spocsConfig = component.spocs;
        if (spocsConfig || component.feed) {
          if (
            (component.feed && !feeds.data[component.feed.url]) ||
            (spocsConfig &&
              spocsConfig.positions &&
              spocsConfig.positions.length &&
              !spocs.loaded)
          ) {
            components.push(placeholderComponent(component));
          } else if (component.feed) {
            components.push(handleComponentWithFeed(component));
          } else {
            components.push(handleComponent(component));
          }
        } else {
          components.push(component);
        }
      }
    }
    return renderedLayoutArray;
  };

  const layoutRender = renderLayout();

  return { layoutRender };
};
