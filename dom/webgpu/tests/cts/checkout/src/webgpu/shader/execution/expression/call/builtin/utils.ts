import { assert, unreachable } from '../../../../../../common/util/util.js';
import { virtualMipSize } from '../../../../../util/texture/base.js';

/* Valid types of Boundaries */
export type Boundary =
  | 'in-bounds'
  | 'x-min-wrap'
  | 'x-min-boundary'
  | 'x-max-wrap'
  | 'x-max-boundary'
  | 'y-min-wrap'
  | 'y-min-boundary'
  | 'y-max-wrap'
  | 'y-max-boundary'
  | 'z-min-wrap'
  | 'z-min-boundary'
  | 'z-max-wrap'
  | 'z-max-boundary';

export function isBoundaryNegative(boundary: Boundary) {
  return boundary.endsWith('min-wrap');
}

/**
 * Generates the boundary entries for the given number of dimensions
 *
 * @param numDimensions: The number of dimensions to generate for
 * @returns an array of generated coord boundaries
 */
export function generateCoordBoundaries(numDimensions: number): Boundary[] {
  const ret: Boundary[] = ['in-bounds'];

  if (numDimensions < 1 || numDimensions > 3) {
    throw new Error(`invalid numDimensions: ${numDimensions}`);
  }

  const name = 'xyz';
  for (let i = 0; i < numDimensions; ++i) {
    for (const j of ['min', 'max']) {
      for (const k of ['wrap', 'boundary']) {
        ret.push(`${name[i]}-${j}-${k}` as Boundary);
      }
    }
  }

  return ret;
}

export type LevelSpec = -1 | 0 | 'numLevels-1' | 'numLevels';

export function getMipLevelFromLevelSpec(mipLevelCount: number, levelSpec: LevelSpec): number {
  switch (levelSpec) {
    case -1:
      return -1;
    case 0:
      return 0;
    case 'numLevels':
      return mipLevelCount;
    case 'numLevels-1':
      return mipLevelCount - 1;
    default:
      unreachable();
  }
}

export function isLevelSpecNegative(levelSpec: LevelSpec) {
  return levelSpec === -1;
}

export type LayerSpec = -1 | 0 | 'numLayers-1' | 'numLayers';

export function getLayerFromLayerSpec(arrayLayerCount: number, layerSpec: LayerSpec): number {
  switch (layerSpec) {
    case -1:
      return -1;
    case 0:
      return 0;
    case 'numLayers':
      return arrayLayerCount;
    case 'numLayers-1':
      return arrayLayerCount - 1;
    default:
      unreachable();
  }
}

export function isLayerSpecNegative(layerSpec: LayerSpec) {
  return layerSpec === -1;
}

function getCoordForSize(size: [number, number, number], boundary: Boundary) {
  const coord = size.map(v => Math.floor(v / 2));
  switch (boundary) {
    case 'in-bounds':
      break;
    default: {
      const axis = boundary[0];
      const axisIndex = axis.charCodeAt(0) - 'x'.charCodeAt(0);
      const axisSize = size[axisIndex];
      const location = boundary.substring(2);
      let v = 0;
      switch (location) {
        case 'min-wrap':
          v = -1;
          break;
        case 'min-boundary':
          v = 0;
          break;
        case 'max-wrap':
          v = axisSize;
          break;
        case 'max-boundary':
          v = axisSize - 1;
          break;
        default:
          unreachable();
      }
      coord[axisIndex] = v;
    }
  }
  return coord;
}

function getNumDimensions(dimension: GPUTextureDimension) {
  switch (dimension) {
    case '1d':
      return 1;
    case '2d':
      return 2;
    case '3d':
      return 3;
  }
}

export function getCoordinateForBoundaries<T>(
  texture: GPUTexture,
  mipLevel: number,
  boundary: Boundary
) {
  const size = virtualMipSize(texture.dimension, texture, mipLevel);
  const coord = getCoordForSize(size, boundary);
  return coord.slice(0, getNumDimensions(texture.dimension)) as T;
}

/**
 * Generates a set of offset values to attempt in the range [-8, 7].
 *
 * @param numDimensions: The number of dimensions to generate for
 * @return an array of generated offset values
 */
export function generateOffsets(numDimensions: number) {
  assert(numDimensions >= 2 && numDimensions <= 3);
  const ret: Array<undefined | Array<number>> = [undefined];
  for (const val of [-8, 0, 1, 7]) {
    const v = [];
    for (let i = 0; i < numDimensions; ++i) {
      v.push(val);
    }
    ret.push(v);
  }
  return ret;
}
