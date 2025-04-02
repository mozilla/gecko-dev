/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
import { useEffect, useRef } from "react";

/**
 * A custom react hook that sets up an IntersectionObserver to observe a single
 * or list of elements and triggers a callback when the element comes into the viewport
 * Note: The refs used should be an array type
 * @function useIntersectionObserver
 * @param {function} callback - The function to call when an element comes into the viewport
 * @param {Object} options - Options object passed to Intersection Observer:
 * https://developer.mozilla.org/en-US/docs/Web/API/IntersectionObserver/IntersectionObserver#options
 * @param {Boolean} [isSingle = false] Boolean if the elements are an array or single element
 *
 * @returns {React.MutableRefObject} a ref containing an array of elements or single element
 *
 *
 *
 */
function useIntersectionObserver(callback, threshold = 0.3) {
  const elementsRef = useRef([]);
  useEffect(() => {
    const observer = new IntersectionObserver(
      entries => {
        entries.forEach(entry => {
          if (entry.isIntersecting) {
            callback(entry.target);
            observer.unobserve(entry.target);
          }
        });
      },
      { threshold }
    );

    elementsRef.current.forEach(el => {
      if (el) {
        observer.observe(el);
      }
    });

    // Cleanup function to disconnect observer on unmount
    return () => observer.disconnect();
  }, [callback, threshold]);

  return elementsRef;
}

/**
 * Determines the active card size ("small", "medium", or "large") based on the screen width
 * and class names applied to the card element at the time of an event (example: click)
 *
 * @param {number} screenWidth - The current window width (in pixels).
 * @param {string | string[]} classNames - A string or array of class names applied to the sections card.
 * @param {boolean[]} sectionsEnabled - If sections is not enabled, all cards are `medium-card`
 * @param {number} flightId - Error ege case: This function should not be called on spocs, which have flightId
 * @returns {"small-card" | "medium-card" | "large-card" | null} The active card type, or null if none is matched.
 */
function getActiveCardSize(screenWidth, classNames, sectionsEnabled, flightId) {
  // Only applies to sponsored content
  if (flightId) {
    return "spoc";
  }

  // Default layout only supports `medium-card`
  if (!sectionsEnabled) {
    // Missing arguments
    return "medium-card";
  }

  // Return null if no values are available
  if (!screenWidth || !classNames) {
    // Missing arguments
    return null;
  }

  const classList = classNames.split(" ");

  // Each breakpoint corresponds to a minimum screen width and its associated column class
  const breakpoints = [
    { min: 1374, column: "col-4" }, // $break-point-sections-variant
    { min: 1122, column: "col-3" }, // $break-point-widest
    { min: 724, column: "col-2" }, // $break-point-layout-variant
    { min: 0, column: "col-1" }, // (default layout)
  ];

  const cardTypes = ["small", "medium", "large"];

  // Determine which column is active based on the current screen width
  const currColumnCount = breakpoints.find(bp => screenWidth >= bp.min).column;

  // Match the card type for that column count
  for (let type of cardTypes) {
    const className = `${currColumnCount}-${type}`;
    if (classList.includes(className)) {
      // Special case: below $break-point-medium (610px), report `col-1-small` as medium
      if (
        screenWidth < 610 &&
        currColumnCount === "col-1" &&
        type === "small"
      ) {
        return "medium-card";
      }
      // Will be either "small-card", "medium-card", or "large-card"
      return `${type}-card`;
    }
  }

  return null;
}

export { useIntersectionObserver, getActiveCardSize };
