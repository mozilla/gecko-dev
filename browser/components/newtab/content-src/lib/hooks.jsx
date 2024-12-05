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
  }, [callback, threshold]);

  return elementsRef;
}

export { useIntersectionObserver };
