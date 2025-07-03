/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This class is copied from https://searchfox.org/mozilla-central/source/browser/components/screenshots/overlayHelpers.mjs
 * with some slight modifications such as forcing the region to be a square.
 * Bug 1974999: Actually import and use the screenshots Region class.
 * The class holds references to each side the of region.
 * x1 is the left boundary, x2 is the right boundary, y1 is the top boundary,
 * and y2 is the bottom boundary. Sometimes the sides can get switched where
 * x1 > x2 or y1 > y2. To mitigate this, the getters (left, right, top, bottom)
 * will choose to return the "correct" value. For example, the left getter
 * returns the min of x1 and x2. The same goes for the other three getters.
 */
export class Region {
  #x1;
  #x2;
  #y1;
  #y2;
  #viewDimensions;

  constructor(viewDimensions) {
    this.resetDimensions();
    this.#viewDimensions = viewDimensions;
  }

  /**
   * Sets the dimensions if the given dimension is defined.
   * Otherwise will reset the dimensions
   *
   * @param {object} dims The new region dimensions
   *  {
   *    left: new left dimension value or undefined
   *    top: new top dimension value or undefined
   *    right: new right dimension value or undefined
   *    bottom: new bottom dimension value or undefined
   *   }
   */
  set #dimensions(dims) {
    if (dims == null) {
      this.resetDimensions();
      return;
    }

    if (dims.left != null) {
      this.left = dims.left;
    }
    if (dims.top != null) {
      this.top = dims.top;
    }
    if (dims.right != null) {
      this.right = dims.right;
    }
    if (dims.bottom != null) {
      this.bottom = dims.bottom;
    }
  }

  /**
   * Set the new dimension for the region. This is called from pointer move
   * events where the region is being moved.
   *
   * @param {object} dims The new dimensions. The object should contain left,
   *   top, right, bottom.
   * @param {string} direction The corner of the region being dragged. It is
   *   used to determine which sides of the region to contain to a square.
   */
  resizeToSquare(dims, direction) {
    // eslint-disable-next-line no-shadow
    let { left, right, top, bottom } = dims;
    switch (direction) {
      case "mover-topLeft": {
        let newDiameter = Math.max(this.right - left, this.bottom - top);
        this.left = this.right - newDiameter;
        this.top = this.bottom - newDiameter;
        break;
      }
      case "mover-topRight": {
        let newDiameter = Math.max(right - this.left, this.bottom - top);
        this.right = this.left + newDiameter;
        this.top = this.bottom - newDiameter;
        break;
      }
      case "mover-bottomRight": {
        let newDiameter = Math.max(right - this.left, bottom - this.top);
        this.right = this.left + newDiameter;
        this.bottom = this.top + newDiameter;
        break;
      }
      case "mover-bottomLeft": {
        let newDiameter = Math.max(this.right - left, bottom - this.top);
        this.left = this.right - newDiameter;
        this.bottom = this.top + newDiameter;
        break;
      }
      default: {
        if (
          left < 0 ||
          right > this.#viewDimensions.width ||
          top < 0 ||
          bottom > this.#viewDimensions.height
        ) {
          // The region would be invalid so just return
          return;
        }
        this.#dimensions = dims;
        break;
      }
    }

    this.forceSquare(direction);
  }

  get dimensions() {
    return {
      left: this.left,
      top: this.top,
      right: this.right,
      bottom: this.bottom,
      width: this.width,
      height: this.height,
      radius: this.radius,
    };
  }

  resetDimensions() {
    this.#x1 = 0;
    this.#x2 = 0;
    this.#y1 = 0;
    this.#y2 = 0;
  }

  /**
   * Sort the coordinates so x1 < x2 and y1 < y2
   */
  sortCoords() {
    if (this.#x1 > this.#x2) {
      [this.#x1, this.#x2] = [this.#x2, this.#x1];
    }
    if (this.#y1 > this.#y2) {
      [this.#y1, this.#y2] = [this.#y2, this.#y1];
    }
  }

  forceSquare(direction) {
    if (this.width === this.height) {
      // Already square
      return;
    }

    let newDiameter = Math.min(this.width, this.height);
    switch (direction) {
      case "mover-topLeft": {
        this.left = this.right - newDiameter;
        this.top = this.bottom - newDiameter;
        break;
      }
      case "mover-topRight": {
        this.right = this.left + newDiameter;
        this.top = this.bottom - newDiameter;
        break;
      }
      case "mover-bottomRight": {
        this.right = this.left + newDiameter;
        this.bottom = this.top + newDiameter;
        break;
      }
      case "mover-bottomLeft": {
        this.left = this.right - newDiameter;
        this.bottom = this.top + newDiameter;
        break;
      }
      default: {
        if (this.width < newDiameter) {
          // Move both the left and the right half of the difference
          let diff = newDiameter - this.width;
          let halfDiff = Math.floor(diff / 2);
          this.left = this.left + halfDiff;
          this.right = this.right - halfDiff;
        } else if (this.height > newDiameter) {
          // Move both the top and the bottom half of the difference
          let diff = newDiameter - this.height;
          let halfDiff = Math.floor(diff / 2);
          this.top = this.left + halfDiff;
          this.bottom = this.right - halfDiff;
        }
      }
    }
  }

  get top() {
    return Math.min(this.#y1, this.#y2);
  }
  set top(val) {
    this.#y1 = Math.min(this.#viewDimensions.height, Math.max(0, val));
  }

  get left() {
    return Math.min(this.#x1, this.#x2);
  }
  set left(val) {
    this.#x1 = Math.min(this.#viewDimensions.width, Math.max(0, val));
  }

  get right() {
    return Math.max(this.#x1, this.#x2);
  }
  set right(val) {
    this.#x2 = Math.min(this.#viewDimensions.width, Math.max(0, val));
  }

  get bottom() {
    return Math.max(this.#y1, this.#y2);
  }
  set bottom(val) {
    this.#y2 = Math.min(this.#viewDimensions.height, Math.max(0, val));
  }

  get width() {
    return Math.abs(this.#x2 - this.#x1);
  }
  get height() {
    return Math.abs(this.#y2 - this.#y1);
  }

  get radius() {
    return Math.floor(this.width / 2);
  }
}

export class ViewDimensions {
  #height = null;
  #width = null;
  #devicePixelRatio = null;

  set dimensions(dimensions) {
    if (dimensions.height != null) {
      this.#height = dimensions.height;
    }
    if (dimensions.width != null) {
      this.#width = dimensions.width;
    }
    if (dimensions.devicePixelRatio != null) {
      this.#devicePixelRatio = dimensions.devicePixelRatio;
    }
  }

  get dimensions() {
    return {
      height: this.height,
      width: this.width,
      devicePixelRatio: this.devicePixelRatio,
    };
  }

  get width() {
    return this.#width;
  }

  get height() {
    return this.#height;
  }

  get devicePixelRatio() {
    return this.#devicePixelRatio;
  }

  reset() {
    this.#width = 0;
    this.#height = 0;
  }
}
