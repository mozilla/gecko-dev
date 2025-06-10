import { isCompatibilityDevice } from '../common/framework/test_config.js';
import { keysOf } from '../common/util/data_tables.js';
import { assert, unreachable } from '../common/util/util.js';

import { align, roundDown } from './util/math.js';
import { getTextureDimensionFromView } from './util/texture/base.js';
import { ImageCopyType } from './util/texture/layout.js';

//
// Texture format tables
//

/**
 * Defaults applied to all texture format tables automatically. Used only inside
 * `formatTableWithDefaults`. This ensures keys are never missing, always explicitly `undefined`.
 *
 * All top-level keys must be defined here, or they won't be exposed at all.
 * Documentation is also written here; this makes it propagate through to the end types.
 */
const kFormatUniversalDefaults = {
  /** Texel block width. */
  blockWidth: undefined,
  /** Texel block height. */
  blockHeight: undefined,
  color: undefined,
  depth: undefined,
  stencil: undefined,
  /**
   * Info when this format can be used as a color render target. The format may require a feature
   * to actually be used as a render target. Eg: rg11b10ufloat which requires rg11b10ufloat-renderable
   * Call {@link isTextureFormatPossiblyUsableAsColorRenderAttachment} before having a device
   * Call {@link isTextureFormatColorRenderable}(device, format) to find out for a particular device.
   * Use {@link kPossibleColorRenderableTextureFormats} for params.
   */
  colorRender: undefined,
  /**
   * Whether the format can possibly be used as a multisample texture. The format may require a
   * feature to actually multisampled. Eg: rg11b10ufloat which requires rg11b10ufloat-renderable
   * Call {@link isTextureFormatPossiblyMultisampled} before having a device
   * Call {@link isTextureFormatMultisampled}(device, format) to find out for a particular device.
   * Use {@link kPossibleMultisampledTextureFormats} for params.
   */
  multisample: undefined,
  /** Optional feature required to use this format, or `undefined` if none. */
  feature: undefined,
  /** The base format for srgb formats. Specified on both srgb and equivalent non-srgb formats. */
  baseFormat: undefined,

  /** @deprecated Use `.color.bytes`, `.depth.bytes`, or `.stencil.bytes`. */
  bytesPerBlock: undefined,

  // IMPORTANT:
  // Add new top-level keys both here and in TextureFormatInfo_TypeCheck.
} as const;
/**
 * Takes `table` and applies `defaults` to every row, i.e. for each row,
 * `{ ... kUniversalDefaults, ...defaults, ...row }`.
 * This only operates at the first level; it doesn't support defaults in nested objects.
 */
function formatTableWithDefaults<Defaults extends {}, Table extends { readonly [K: string]: {} }>({
  defaults,
  table,
}: {
  defaults: Defaults;
  table: Table;
}): {
  readonly [F in keyof Table]: {
    readonly [K in keyof typeof kFormatUniversalDefaults]: K extends keyof Table[F]
      ? Table[F][K]
      : K extends keyof Defaults
      ? Defaults[K]
      : (typeof kFormatUniversalDefaults)[K];
  };
} {
  return Object.fromEntries(
    Object.entries(table).map(([k, row]) => [
      k,
      { ...kFormatUniversalDefaults, ...defaults, ...row },
    ])
    /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
  ) as any;
}

/** "plain color formats", plus rgb9e5ufloat. */
const kRegularTextureFormatInfo = formatTableWithDefaults({
  defaults: { blockWidth: 1, blockHeight: 1 },
  table: {
    // plain, 8 bits per component

    r8unorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 1,
      },
      colorRender: { blend: true, resolve: true, byteCost: 1, alignment: 1 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r8snorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 1,
      },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r8uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 1,
      },
      colorRender: { blend: false, resolve: false, byteCost: 1, alignment: 1 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r8sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 1,
      },
      colorRender: { blend: false, resolve: false, byteCost: 1, alignment: 1 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    rg8unorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 2,
      },
      colorRender: { blend: true, resolve: true, byteCost: 2, alignment: 1 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg8snorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 2,
      },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg8uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 2,
      },
      colorRender: { blend: false, resolve: false, byteCost: 2, alignment: 1 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg8sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 2,
      },
      colorRender: { blend: false, resolve: false, byteCost: 2, alignment: 1 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    rgba8unorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 1 },
      multisample: true,
      baseFormat: 'rgba8unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'rgba8unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 1 },
      multisample: true,
      baseFormat: 'rgba8unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba8snorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 4,
      },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba8uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: false, resolve: false, byteCost: 4, alignment: 1 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba8sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: false, resolve: false, byteCost: 4, alignment: 1 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    bgra8unorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 1 },
      multisample: true,
      baseFormat: 'bgra8unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'bgra8unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 1 },
      multisample: true,
      baseFormat: 'bgra8unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    // plain, 16 bits per component

    r16unorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 2,
      },
      colorRender: { blend: true, resolve: true, byteCost: 2, alignment: 2 },
      multisample: true,
      feature: 'texture-formats-tier1',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r16snorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 2,
      },
      colorRender: { blend: true, resolve: true, byteCost: 2, alignment: 2 },
      multisample: true,
      feature: 'texture-formats-tier1',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r16uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 2,
      },
      colorRender: { blend: false, resolve: false, byteCost: 2, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r16sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 2,
      },
      colorRender: { blend: false, resolve: false, byteCost: 2, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r16float: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 2,
      },
      colorRender: { blend: true, resolve: true, byteCost: 2, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    rg16unorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 4, alignment: 2 },
      multisample: true,
      feature: 'texture-formats-tier1',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg16snorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 4, alignment: 2 },
      multisample: true,
      feature: 'texture-formats-tier1',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg16uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: false, resolve: false, byteCost: 4, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg16sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: false, resolve: false, byteCost: 4, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg16float: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 4, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    rgba16unorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 8,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 4 },
      multisample: true,
      feature: 'texture-formats-tier1',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba16snorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 8,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 2 },
      multisample: true,
      feature: 'texture-formats-tier1',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba16uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 8,
      },
      colorRender: { blend: false, resolve: false, byteCost: 8, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba16sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 8,
      },
      colorRender: { blend: false, resolve: false, byteCost: 8, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba16float: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 8,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 2 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    // plain, 32 bits per component

    r32uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: true,
        bytes: 4,
      },
      colorRender: { blend: false, resolve: false, byteCost: 4, alignment: 4 },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r32sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: true,
        bytes: 4,
      },
      colorRender: { blend: false, resolve: false, byteCost: 4, alignment: 4 },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    r32float: {
      color: {
        type: 'unfilterable-float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: true,
        bytes: 4,
      },
      colorRender: { blend: false, resolve: false, byteCost: 4, alignment: 4 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    rg32uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 8,
      },
      colorRender: { blend: false, resolve: false, byteCost: 8, alignment: 4 },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg32sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 8,
      },
      colorRender: { blend: false, resolve: false, byteCost: 8, alignment: 4 },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg32float: {
      color: {
        type: 'unfilterable-float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 8,
      },
      colorRender: { blend: false, resolve: false, byteCost: 8, alignment: 4 },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    rgba32uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 16,
      },
      colorRender: { blend: false, resolve: false, byteCost: 16, alignment: 4 },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba32sint: {
      color: {
        type: 'sint',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 16,
      },
      colorRender: { blend: false, resolve: false, byteCost: 16, alignment: 4 },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgba32float: {
      color: {
        type: 'unfilterable-float',
        copySrc: true,
        copyDst: true,
        storage: true,
        readWriteStorage: false,
        bytes: 16,
      },
      colorRender: { blend: false, resolve: false, byteCost: 16, alignment: 4 },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    // plain, mixed component width, 32 bits per texel

    rgb10a2uint: {
      color: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: false, resolve: false, byteCost: 8, alignment: 4 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rgb10a2unorm: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 4 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    rg11b10ufloat: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      colorRender: { blend: true, resolve: true, byteCost: 8, alignment: 4 },
      multisample: true,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    // packed

    rgb9e5ufloat: {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      multisample: false,
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
  },
} as const);

// MAINTENANCE_TODO: Distinguishing "sized" and "unsized" depth stencil formats doesn't make sense
// because one aspect can be sized and one can be unsized. This should be cleaned up, but is kept
// this way during a migration phase.
const kSizedDepthStencilFormatInfo = formatTableWithDefaults({
  defaults: { blockWidth: 1, blockHeight: 1, multisample: true },
  table: {
    stencil8: {
      stencil: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 1,
      },
      bytesPerBlock: 1,
    },
    depth16unorm: {
      depth: {
        type: 'depth',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 2,
      },
      bytesPerBlock: 2,
    },
    depth32float: {
      depth: {
        type: 'depth',
        copySrc: true,
        copyDst: false,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      bytesPerBlock: 4,
    },
  },
} as const);
const kUnsizedDepthStencilFormatInfo = formatTableWithDefaults({
  defaults: { blockWidth: 1, blockHeight: 1, multisample: true },
  table: {
    depth24plus: {
      depth: {
        type: 'depth',
        copySrc: false,
        copyDst: false,
        storage: false,
        readWriteStorage: false,
        bytes: undefined,
      },
    },
    'depth24plus-stencil8': {
      depth: {
        type: 'depth',
        copySrc: false,
        copyDst: false,
        storage: false,
        readWriteStorage: false,
        bytes: undefined,
      },
      stencil: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 1,
      },
    },
    'depth32float-stencil8': {
      depth: {
        type: 'depth',
        copySrc: true,
        copyDst: false,
        storage: false,
        readWriteStorage: false,
        bytes: 4,
      },
      stencil: {
        type: 'uint',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 1,
      },
      feature: 'depth32float-stencil8',
    },
  },
} as const);

const kBCTextureFormatInfo = formatTableWithDefaults({
  defaults: {
    blockWidth: 4,
    blockHeight: 4,
    multisample: false,
    feature: 'texture-compression-bc',
  },
  table: {
    'bc1-rgba-unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      baseFormat: 'bc1-rgba-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'bc1-rgba-unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      baseFormat: 'bc1-rgba-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'bc2-rgba-unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'bc2-rgba-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'bc2-rgba-unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'bc2-rgba-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'bc3-rgba-unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'bc3-rgba-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'bc3-rgba-unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'bc3-rgba-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'bc4-r-unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'bc4-r-snorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'bc5-rg-unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'bc5-rg-snorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'bc6h-rgb-ufloat': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'bc6h-rgb-float': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'bc7-rgba-unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'bc7-rgba-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'bc7-rgba-unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'bc7-rgba-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
  },
} as const);

const kETC2TextureFormatInfo = formatTableWithDefaults({
  defaults: {
    blockWidth: 4,
    blockHeight: 4,
    multisample: false,
    feature: 'texture-compression-etc2',
  },
  table: {
    'etc2-rgb8unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      baseFormat: 'etc2-rgb8unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'etc2-rgb8unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      baseFormat: 'etc2-rgb8unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'etc2-rgb8a1unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      baseFormat: 'etc2-rgb8a1unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'etc2-rgb8a1unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      baseFormat: 'etc2-rgb8a1unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'etc2-rgba8unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'etc2-rgba8unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'etc2-rgba8unorm-srgb': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'etc2-rgba8unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'eac-r11unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'eac-r11snorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 8,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'eac-rg11unorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'eac-rg11snorm': {
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
  },
} as const);

const kASTCTextureFormatInfo = formatTableWithDefaults({
  defaults: {
    multisample: false,
    feature: 'texture-compression-astc',
  },
  table: {
    'astc-4x4-unorm': {
      blockWidth: 4,
      blockHeight: 4,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-4x4-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-4x4-unorm-srgb': {
      blockWidth: 4,
      blockHeight: 4,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-4x4-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-5x4-unorm': {
      blockWidth: 5,
      blockHeight: 4,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-5x4-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-5x4-unorm-srgb': {
      blockWidth: 5,
      blockHeight: 4,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-5x4-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-5x5-unorm': {
      blockWidth: 5,
      blockHeight: 5,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-5x5-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-5x5-unorm-srgb': {
      blockWidth: 5,
      blockHeight: 5,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-5x5-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-6x5-unorm': {
      blockWidth: 6,
      blockHeight: 5,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-6x5-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-6x5-unorm-srgb': {
      blockWidth: 6,
      blockHeight: 5,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-6x5-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-6x6-unorm': {
      blockWidth: 6,
      blockHeight: 6,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-6x6-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-6x6-unorm-srgb': {
      blockWidth: 6,
      blockHeight: 6,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-6x6-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-8x5-unorm': {
      blockWidth: 8,
      blockHeight: 5,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-8x5-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-8x5-unorm-srgb': {
      blockWidth: 8,
      blockHeight: 5,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-8x5-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-8x6-unorm': {
      blockWidth: 8,
      blockHeight: 6,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-8x6-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-8x6-unorm-srgb': {
      blockWidth: 8,
      blockHeight: 6,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-8x6-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-8x8-unorm': {
      blockWidth: 8,
      blockHeight: 8,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-8x8-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-8x8-unorm-srgb': {
      blockWidth: 8,
      blockHeight: 8,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-8x8-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-10x5-unorm': {
      blockWidth: 10,
      blockHeight: 5,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-10x5-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-10x5-unorm-srgb': {
      blockWidth: 10,
      blockHeight: 5,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-10x5-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-10x6-unorm': {
      blockWidth: 10,
      blockHeight: 6,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-10x6-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-10x6-unorm-srgb': {
      blockWidth: 10,
      blockHeight: 6,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-10x6-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-10x8-unorm': {
      blockWidth: 10,
      blockHeight: 8,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-10x8-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-10x8-unorm-srgb': {
      blockWidth: 10,
      blockHeight: 8,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-10x8-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-10x10-unorm': {
      blockWidth: 10,
      blockHeight: 10,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-10x10-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-10x10-unorm-srgb': {
      blockWidth: 10,
      blockHeight: 10,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-10x10-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-12x10-unorm': {
      blockWidth: 12,
      blockHeight: 10,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-12x10-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-12x10-unorm-srgb': {
      blockWidth: 12,
      blockHeight: 10,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-12x10-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },

    'astc-12x12-unorm': {
      blockWidth: 12,
      blockHeight: 12,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-12x12-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
    'astc-12x12-unorm-srgb': {
      blockWidth: 12,
      blockHeight: 12,
      color: {
        type: 'float',
        copySrc: true,
        copyDst: true,
        storage: false,
        readWriteStorage: false,
        bytes: 16,
      },
      baseFormat: 'astc-12x12-unorm',
      /*prettier-ignore*/ get bytesPerBlock() { return this.color.bytes; },
    },
  },
} as const);

// Definitions for use locally.

// MAINTENANCE_TODO: Consider generating the exports below programmatically by filtering the big list, instead
// of using these local constants? Requires some type magic though.
/* prettier-ignore */ const   kCompressedTextureFormatInfo = { ...kBCTextureFormatInfo, ...kETC2TextureFormatInfo, ...kASTCTextureFormatInfo } as const;
/* prettier-ignore */ const        kColorTextureFormatInfo = { ...kRegularTextureFormatInfo, ...kCompressedTextureFormatInfo } as const;
/* prettier-ignore */ const    kEncodableTextureFormatInfo = { ...kRegularTextureFormatInfo, ...kSizedDepthStencilFormatInfo } as const;
/* prettier-ignore */ const        kSizedTextureFormatInfo = { ...kRegularTextureFormatInfo, ...kSizedDepthStencilFormatInfo, ...kCompressedTextureFormatInfo } as const;
/* prettier-ignore */ const        kDepthStencilFormatInfo = { ...kSizedDepthStencilFormatInfo, ...kUnsizedDepthStencilFormatInfo } as const;
/* prettier-ignore */ const kUncompressedTextureFormatInfo = { ...kRegularTextureFormatInfo, ...kSizedDepthStencilFormatInfo, ...kUnsizedDepthStencilFormatInfo } as const;
/* prettier-ignore */ const          kAllTextureFormatInfo = { ...kUncompressedTextureFormatInfo, ...kCompressedTextureFormatInfo } as const;

/** A "regular" texture format (uncompressed, sized, single-plane color formats). */
/* prettier-ignore */ export type      RegularTextureFormat = keyof typeof kRegularTextureFormatInfo;
/** A sized depth/stencil texture format. */
/* prettier-ignore */ export type   SizedDepthStencilFormat = keyof typeof kSizedDepthStencilFormatInfo;
/** An unsized depth/stencil texture format. */
/* prettier-ignore */ export type UnsizedDepthStencilFormat = keyof typeof kUnsizedDepthStencilFormatInfo;
/** A compressed (block) texture format. */
/* prettier-ignore */ export type   CompressedTextureFormat = keyof typeof kCompressedTextureFormatInfo;

/** A color texture format (regular | compressed). */
/* prettier-ignore */ export type        ColorTextureFormat = keyof typeof kColorTextureFormatInfo;
/** An encodable texture format (regular | sized depth/stencil). */
/* prettier-ignore */ export type    EncodableTextureFormat = keyof typeof kEncodableTextureFormatInfo;
/** A sized texture format (regular | sized depth/stencil | compressed). */
/* prettier-ignore */ export type        SizedTextureFormat = keyof typeof kSizedTextureFormatInfo;
/** A depth/stencil format (sized | unsized). */
/* prettier-ignore */ export type        DepthStencilFormat = keyof typeof kDepthStencilFormatInfo;
/** An uncompressed (block size 1x1) format (regular | depth/stencil). */
/* prettier-ignore */ export type UncompressedTextureFormat = keyof typeof kUncompressedTextureFormatInfo;

/* prettier-ignore */ export const        kRegularTextureFormats: readonly      RegularTextureFormat[] = keysOf(     kRegularTextureFormatInfo);
/* prettier-ignore */ export const     kSizedDepthStencilFormats: readonly   SizedDepthStencilFormat[] = keysOf(  kSizedDepthStencilFormatInfo);
/* prettier-ignore */ export const   kUnsizedDepthStencilFormats: readonly UnsizedDepthStencilFormat[] = keysOf(kUnsizedDepthStencilFormatInfo);
/* prettier-ignore */ export const     kCompressedTextureFormats: readonly   CompressedTextureFormat[] = keysOf(  kCompressedTextureFormatInfo);
/* prettier-ignore */ export const   kBCCompressedTextureFormats: readonly   CompressedTextureFormat[] = keysOf(          kBCTextureFormatInfo);
/* prettier-ignore */ export const kASTCCompressedTextureFormats: readonly   CompressedTextureFormat[] = keysOf(        kASTCTextureFormatInfo);

/* prettier-ignore */ export const        kColorTextureFormats: readonly        ColorTextureFormat[] = keysOf(       kColorTextureFormatInfo);
/* prettier-ignore */ export const    kEncodableTextureFormats: readonly    EncodableTextureFormat[] = keysOf(   kEncodableTextureFormatInfo);
/* prettier-ignore */ export const        kSizedTextureFormats: readonly        SizedTextureFormat[] = keysOf(       kSizedTextureFormatInfo);
/* prettier-ignore */ export const        kDepthStencilFormats: readonly        DepthStencilFormat[] = keysOf(       kDepthStencilFormatInfo);
/* prettier-ignore */ export const kUncompressedTextureFormats: readonly UncompressedTextureFormat[] = keysOf(kUncompressedTextureFormatInfo);
/* prettier-ignore */ export const          kAllTextureFormats: readonly          GPUTextureFormat[] = keysOf(         kAllTextureFormatInfo);

/** Per-GPUTextureFormat-per-aspect info. */
interface TextureFormatAspectInfo {
  /** Whether the aspect can be used as `COPY_SRC`. */
  copySrc: boolean;
  /** Whether the aspect can be used as `COPY_DST`. */
  copyDst: boolean;
  /** Whether the aspect can be used as `STORAGE`. */
  storage: boolean;
  /** Whether the aspect can be used as `STORAGE` with `read-write` storage texture access. */
  readWriteStorage: boolean;
  /** The "texel block copy footprint" of one texel block; `undefined` if the aspect is unsized. */
  bytes: number | undefined;
}
/** Per GPUTextureFormat-per-aspect info for color aspects. */
interface TextureFormatColorAspectInfo extends TextureFormatAspectInfo {
  bytes: number;
  /** "Best" sample type of the format. "float" also implies "unfilterable-float". */
  type: 'float' | 'uint' | 'sint' | 'unfilterable-float';
}
/** Per GPUTextureFormat-per-aspect info for depth aspects. */
interface TextureFormatDepthAspectInfo extends TextureFormatAspectInfo {
  /** "depth" also implies "unfilterable-float". */
  type: 'depth';
}
/** Per GPUTextureFormat-per-aspect info for stencil aspects. */
interface TextureFormatStencilAspectInfo extends TextureFormatAspectInfo {
  bytes: 1;
  type: 'uint';
}

/**
 * Per-GPUTextureFormat info.
 * This is not actually the type of values in kTextureFormatInfo; that type is fully const
 * so that it can be narrowed very precisely at usage sites by the compiler.
 * This type exists only as a type check on the inferred type of kTextureFormatInfo.
 */
type TextureFormatInfo_TypeCheck = {
  blockWidth: number;
  blockHeight: number;
  multisample: boolean;
  baseFormat: GPUTextureFormat | undefined;
  feature: GPUFeatureName | undefined;

  bytesPerBlock: number | undefined;

  // IMPORTANT:
  // Add new top-level keys both here and in kUniversalDefaults.
} & (
  | {
      /** Color aspect info. */
      color: TextureFormatColorAspectInfo;
      /** Defined if the format is a color format that can be used as `RENDER_ATTACHMENT`. */
      colorRender:
        | undefined
        | {
            /** Whether the format is blendable. */
            blend: boolean;
            /** Whether the format can be a multisample resolve target. */
            resolve: boolean;
            /** The "render target pixel byte cost" of the format. */
            byteCost: number;
            /** The "render target component alignment" of the format. */
            alignment: number;
          };
    }
  | (
      | {
          /** Depth aspect info. */
          depth: TextureFormatDepthAspectInfo;
          /** Stencil aspect info. */
          stencil: undefined | TextureFormatStencilAspectInfo;
          multisample: true;
        }
      | {
          /** Stencil aspect info. */
          stencil: TextureFormatStencilAspectInfo;
          multisample: true;
        }
    )
);

/**
 * DO NOT EXPORT THIS - functions that need info from this table should use the appropriate
 * method for their needs.
 *
 * For a list of textures formats for test parameters there are:
 *
 * Lists of formats that might require features to be enabled
 * * kPossibleColorRenderableTextureFormats
 * * kPossibleStorageTextureFormats
 * * kPossibleReadWriteStorageTextureFormats
 * * kPossibleMultisampledTextureFormats
 *
 * Lists of formats that end in -srgb
 * * kDifferentBaseFormatTextureFormats  (includes compressed textures)
 * * kDifferentBaseFormatRegularTextureFormats (does not include compressed textures)
 *
 * Formats that require a feature to use at all (mostly compressed formats)
 * * kOptionalTextureFormats
 *
 * Misc
 * * kRegularTextureFormats
 * * kSizedDepthStencilFormats
 * * kUnsizedDepthStencilFormats
 * * kCompressedTextureFormats
 * * kUncompressedTextureFormats
 * * kColorTextureFormats - color formats including compressed and sint/uint
 * * kEncodableTextureFormats - formats that TexelView supports.
 * * kSizedTextureFormats - formats that have a known size (so not depth24plus ...)
 * * kDepthStencilFormats - depth, stencil, depth-stencil
 * * kDepthTextureFormats - depth and depth-stencil
 * * kStencilTextureFormats - stencil and depth-stencil
 * * kAllTextureFormats
 *
 * If one of the list above does not work, add a new one or to filter in beforeAllSubcases you generally want to use
 * You will not know if you can actually use a texture for the given use case until the test runs and has a device.
 *
 * * isTextureFormatPossiblyUsableAsRenderAttachment
 * * isTextureFormatPossiblyUsableAsColorRenderAttachment
 * * isTextureFormatPossiblyMultisampled
 * * isTextureFormatPossiblyStorageReadable
 * * isTextureFormatPossiblyStorageReadWritable
 * * isTextureFormatPossiblyFilterableAsTextureF32
 *
 * These are also usable before or during a test
 *
 * * isColorTextureFormat
 * * isDepthTextureFormat
 * * isStencilTextureFormat
 * * isDepthOrStencilTextureFormat
 * * isEncodableTextureFormat
 * * isRegularTextureFormat
 * * isCompressedFloatTextureFormat
 * * isSintOrUintFormat
 *
 * To skip a test use the `skipIfXXX` tests in `GPUTest` if possible. Otherwise these functions
 * require a device to give a correct answer.
 *
 * * isTextureFormatUsableAsRenderAttachment
 * * isTextureFormatColorRenderable
 * * isTextureFormatResolvable
 * * isTextureFormatBlendable
 * * isTextureFormatMultisampled
 * * isTextureFormatUsableAsStorageFormat
 * * isTextureFormatUsableAsReadWriteStorageTexture
 * * isTextureFormatUsableAsStorageFormatInCreateShaderModule
 *
 * Per-GPUTextureFormat info.
 */
const kTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
  ...kUnsizedDepthStencilFormatInfo,
  ...kBCTextureFormatInfo,
  ...kETC2TextureFormatInfo,
  ...kASTCTextureFormatInfo,
} as const;

/** Defining this variable verifies the type of kTextureFormatInfo2. It is not used. */
// eslint-disable-next-line @typescript-eslint/no-unused-vars
const kTextureFormatInfo_TypeCheck: {
  readonly [F in GPUTextureFormat]: TextureFormatInfo_TypeCheck;
} = kTextureFormatInfo;

// Depth texture formats including formats that also support stencil
export const kDepthTextureFormats = [
  ...kDepthStencilFormats.filter(v => kTextureFormatInfo[v].depth),
] as const;
// Stencil texture formats including formats that also support depth
export const kStencilTextureFormats = kDepthStencilFormats.filter(
  v => kTextureFormatInfo[v].stencil
);

const kTextureFormatTier1AllowsRenderAttachmentBlendableMultisampleResolve: readonly ColorTextureFormat[] =
  ['r8snorm', 'rg8snorm', 'rgba8snorm', 'rg11b10ufloat'] as const;

const kTextureFormatsTier1EnablesStorageReadOnlyWriteOnly: readonly ColorTextureFormat[] = [
  'r8unorm',
  'r8snorm',
  'r8uint',
  'r8sint',
  'rg8unorm',
  'rg8snorm',
  'rg8uint',
  'rg8sint',
  'r16uint',
  'r16sint',
  'r16float',
  'rg16uint',
  'rg16sint',
  'rg16float',
  'rgb10a2uint',
  'rgb10a2unorm',
  'rg11b10ufloat',
] as const;

// Texture formats that may possibly be used as a storage texture.
// Some may require certain features to be enabled.
export const kPossibleStorageTextureFormats = [
  ...kRegularTextureFormats.filter(f => kTextureFormatInfo[f].color?.storage),
  'bgra8unorm',
  // these can be used as storage when texture-formats-tier1 is enabled
  ...kTextureFormatsTier1EnablesStorageReadOnlyWriteOnly,
] as readonly RegularTextureFormat[];

// Texture formats that may possibly be used as a storage texture.
// Some may require certain features to be enabled.
export const kPossibleReadWriteStorageTextureFormats = [
  ...kPossibleStorageTextureFormats.filter(f => kTextureFormatInfo[f].color?.readWriteStorage),
] as readonly RegularTextureFormat[];

// Texture formats that may possibly be multisampled.
// Some may require certain features to be enabled.
export const kPossibleMultisampledTextureFormats = [
  ...kRegularTextureFormats.filter(f => kTextureFormatInfo[f].multisample),
  ...kDepthStencilFormats.filter(f => kTextureFormatInfo[f].multisample),
] as const;

// Texture formats that may possibly be color renderable.
// Some may require certain features to be enabled.
export const kPossibleColorRenderableTextureFormats = [
  ...kRegularTextureFormats.filter(f => kTextureFormatInfo[f].colorRender),
] as const;
export type PossibleColorRenderTextureFormat =
  (typeof kPossibleColorRenderableTextureFormats)[number];

// Texture formats that have a different base format. This is effectively all -srgb formats
// including compressed formats.
export const kDifferentBaseFormatTextureFormats = kColorTextureFormats.filter(
  f => kTextureFormatInfo[f].baseFormat && kTextureFormatInfo[f].baseFormat !== f
);

// "Regular" texture formats that have a different base format. This is effectively all -srgb formats
// except compressed formats.
export const kDifferentBaseFormatRegularTextureFormats = kRegularTextureFormats.filter(
  f => kTextureFormatInfo[f].baseFormat && kTextureFormatInfo[f].baseFormat !== f
);

// Textures formats that are optional
export const kOptionalTextureFormats = kAllTextureFormats.filter(
  t => kTextureFormatInfo[t].feature !== undefined
);

/** Valid GPUTextureFormats for `copyExternalImageToTexture`, by spec. */
export const kValidTextureFormatsForCopyE2T = [
  'r8unorm',
  'r16float',
  'r32float',
  'rg8unorm',
  'rg16float',
  'rg32float',
  'rgba8unorm',
  'rgba8unorm-srgb',
  'bgra8unorm',
  'bgra8unorm-srgb',
  'rgb10a2unorm',
  'rgba16float',
  'rgba32float',
] as const;

//
// Other related stuff
//

const kDepthStencilFormatCapabilityInBufferTextureCopy = {
  // kUnsizedDepthStencilFormats
  depth24plus: {
    CopyB2T: [],
    CopyT2B: [],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': -1 },
  },
  'depth24plus-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['stencil-only'],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': 1 },
  },

  // kSizedDepthStencilFormats
  depth16unorm: {
    CopyB2T: ['all', 'depth-only'],
    CopyT2B: ['all', 'depth-only'],
    texelAspectSize: { 'depth-only': 2, 'stencil-only': -1 },
  },
  depth32float: {
    CopyB2T: [],
    CopyT2B: ['all', 'depth-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': -1 },
  },
  'depth32float-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['depth-only', 'stencil-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': 1 },
  },
  stencil8: {
    CopyB2T: ['all', 'stencil-only'],
    CopyT2B: ['all', 'stencil-only'],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': 1 },
  },
} as const;

/** `kDepthStencilFormatResolvedAspect[format][aspect]` returns the aspect-specific format for a
 *  depth-stencil format, or `undefined` if the format doesn't have the aspect.
 */
export const kDepthStencilFormatResolvedAspect: {
  readonly [k in DepthStencilFormat]: {
    readonly [a in GPUTextureAspect]: DepthStencilFormat | undefined;
  };
} = {
  // kUnsizedDepthStencilFormats
  depth24plus: {
    all: 'depth24plus',
    'depth-only': 'depth24plus',
    'stencil-only': undefined,
  },
  'depth24plus-stencil8': {
    all: 'depth24plus-stencil8',
    'depth-only': 'depth24plus',
    'stencil-only': 'stencil8',
  },

  // kSizedDepthStencilFormats
  depth16unorm: {
    all: 'depth16unorm',
    'depth-only': 'depth16unorm',
    'stencil-only': undefined,
  },
  depth32float: {
    all: 'depth32float',
    'depth-only': 'depth32float',
    'stencil-only': undefined,
  },
  'depth32float-stencil8': {
    all: 'depth32float-stencil8',
    'depth-only': 'depth32float',
    'stencil-only': 'stencil8',
  },
  stencil8: {
    all: 'stencil8',
    'depth-only': undefined,
    'stencil-only': 'stencil8',
  },
} as const;

/**
 * @returns the GPUTextureFormat corresponding to the @param aspect of @param format.
 * This allows choosing the correct format for depth-stencil aspects when creating pipelines that
 * will have to match the resolved format of views, or to get per-aspect information like the
 * `blockByteSize`.
 *
 * Many helpers use an `undefined` `aspect` to means `'all'` so this is also the default for this
 * function.
 */
export function resolvePerAspectFormat(
  format: GPUTextureFormat,
  aspect?: GPUTextureAspect
): GPUTextureFormat {
  if (aspect === 'all' || aspect === undefined) {
    return format;
  }
  assert(!!kTextureFormatInfo[format].depth || !!kTextureFormatInfo[format].stencil);
  const resolved = kDepthStencilFormatResolvedAspect[format as DepthStencilFormat][aspect ?? 'all'];
  assert(resolved !== undefined);
  return resolved;
}

/**
 * @returns the sample type of the specified aspect of the specified format.
 */
export function sampleTypeForFormatAndAspect(
  format: GPUTextureFormat,
  aspect: GPUTextureAspect
): 'uint' | 'depth' | 'float' | 'sint' | 'unfilterable-float' {
  const info = kTextureFormatInfo[format];
  if (info.color) {
    assert(aspect === 'all', `color format ${format} used with aspect ${aspect}`);
    return info.color.type;
  } else if (info.depth && info.stencil) {
    if (aspect === 'depth-only') {
      return info.depth.type;
    } else if (aspect === 'stencil-only') {
      return info.stencil.type;
    } else {
      unreachable(`depth-stencil format ${format} used with aspect ${aspect}`);
    }
  } else if (info.depth) {
    assert(aspect !== 'stencil-only', `depth-only format ${format} used with aspect ${aspect}`);
    return info.depth.type;
  } else if (info.stencil) {
    assert(aspect !== 'depth-only', `stencil-only format ${format} used with aspect ${aspect}`);
    return info.stencil.type;
  }
  unreachable();
}

/**
 * Gets all copyable aspects for copies between texture and buffer for specified depth/stencil format and copy type, by spec.
 */
export function depthStencilFormatCopyableAspects(
  type: ImageCopyType,
  format: DepthStencilFormat
): readonly GPUTextureAspect[] {
  const appliedType = type === 'WriteTexture' ? 'CopyB2T' : type;
  return kDepthStencilFormatCapabilityInBufferTextureCopy[format][appliedType];
}

/**
 * Computes whether a copy between a depth/stencil texture aspect and a buffer is supported, by spec.
 */
export function depthStencilBufferTextureCopySupported(
  type: ImageCopyType,
  format: DepthStencilFormat,
  aspect: GPUTextureAspect
): boolean {
  const supportedAspects: readonly GPUTextureAspect[] = depthStencilFormatCopyableAspects(
    type,
    format
  );
  return supportedAspects.includes(aspect);
}

/**
 * Returns the byte size of the depth or stencil aspect of the specified depth/stencil format,
 * or -1 if none.
 */
export function depthStencilFormatAspectSize(
  format: DepthStencilFormat,
  aspect: 'depth-only' | 'stencil-only'
) {
  const texelAspectSize =
    kDepthStencilFormatCapabilityInBufferTextureCopy[format].texelAspectSize[aspect];
  assert(texelAspectSize > 0);
  return texelAspectSize;
}

/**
 * Returns true iff a texture can be created with the provided GPUTextureDimension
 * (defaulting to 2d) and GPUTextureFormat, by spec.
 */
export function textureFormatAndDimensionPossiblyCompatible(
  dimension: undefined | GPUTextureDimension,
  format: GPUTextureFormat
): boolean {
  if (dimension === '3d' && (isBCTextureFormat(format) || isASTCTextureFormat(format))) {
    return true;
  }
  const info = kAllTextureFormatInfo[format];
  return !(
    (dimension === '1d' || dimension === '3d') &&
    (info.blockWidth > 1 || info.depth || info.stencil)
  );
}

/**
 * Returns true iff a texture can be created with the provided GPUTextureDimension
 * (defaulting to 2d) and GPUTextureFormat for a GPU device, by spec.
 */
export function textureDimensionAndFormatCompatibleForDevice(
  device: GPUDevice,
  dimension: undefined | GPUTextureDimension,
  format: GPUTextureFormat
): boolean {
  if (
    dimension === '3d' &&
    ((isBCTextureFormat(format) && device.features.has('texture-compression-bc-sliced-3d')) ||
      (isASTCTextureFormat(format) && device.features.has('texture-compression-astc-sliced-3d')))
  ) {
    return true;
  }
  const info = kAllTextureFormatInfo[format];
  return !(
    (dimension === '1d' || dimension === '3d') &&
    (info.blockWidth > 1 || info.depth || info.stencil)
  );
}

/**
 * Returns true iff a texture can be used with the provided GPUTextureViewDimension
 */
export function textureViewDimensionAndFormatCompatibleForDevice(
  device: GPUDevice,
  dimension: GPUTextureViewDimension,
  format: GPUTextureFormat
): boolean {
  return textureDimensionAndFormatCompatibleForDevice(
    device,
    getTextureDimensionFromView(dimension),
    format
  );
}

/**
 * Check if two formats are view format compatible.
 */
export function textureFormatsAreViewCompatible(
  device: GPUDevice,
  a: GPUTextureFormat,
  b: GPUTextureFormat
) {
  return isCompatibilityDevice(device)
    ? a === b
    : a === b || a + '-srgb' === b || b + '-srgb' === a;
}

/**
 * Gets the block width, height, and bytes per block for a color texture format.
 * This is for color textures only. For all texture formats @see {@link getBlockInfoForTextureFormat}
 * The point of this function is bytesPerBlock is always defined so no need to check that it's not
 * vs getBlockInfoForTextureFormat where it may not be defined.
 */
export function getBlockInfoForColorTextureFormat(format: ColorTextureFormat) {
  const info = kTextureFormatInfo[format];
  return {
    blockWidth: info.blockWidth,
    blockHeight: info.blockHeight,
    bytesPerBlock: info.color?.bytes,
  };
}

/**
 * Gets the block width, height, and bytes per block for a sized texture format.
 * This is for sized textures only. For all texture formats @see {@link getBlockInfoForTextureFormat}
 * The point of this function is bytesPerBlock is always defined so no need to check that it's not
 * vs getBlockInfoForTextureFormat where it may not be defined.
 */
export function getBlockInfoForSizedTextureFormat(format: SizedTextureFormat) {
  const info = kTextureFormatInfo[format];
  const bytesPerBlock = info.color?.bytes || info.depth?.bytes || info.stencil?.bytes;
  assert(!!bytesPerBlock);
  return {
    blockWidth: info.blockWidth,
    blockHeight: info.blockHeight,
    bytesPerBlock,
  };
}

/**
 * Gets the block width, height, and bytes per block for an encodable texture format.
 * This is for encodable textures only. For all texture formats @see {@link getBlockInfoForTextureFormat}
 * The point of this function is bytesPerBlock is always defined so no need to check that it's not
 * vs getBlockInfoForTextureFormat where it may not be defined.
 */
export function getBlockInfoForEncodableTextureFormat(format: EncodableTextureFormat) {
  const info = kTextureFormatInfo[format];
  const bytesPerBlock = info.color?.bytes || info.depth?.bytes || info.stencil?.bytes;
  assert(!!bytesPerBlock);
  return {
    blockWidth: info.blockWidth,
    blockHeight: info.blockHeight,
    bytesPerBlock,
  };
}

/**
 * Gets the block width, height, and bytes per block for a color texture format.
 * Note that bytesPerBlock will be undefined if format's size is undefined.
 * If you are only using color or encodable formats, @see {@link getBlockInfoForColorTextureFormat}
 * or {@link getBlockInfoForEncodableTextureFormat}
 */
export function getBlockInfoForTextureFormat(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  return {
    blockWidth: info.blockWidth,
    blockHeight: info.blockHeight,
    bytesPerBlock: info.color?.bytes ?? info.depth?.bytes ?? info.stencil?.bytes,
  };
}

/**
 * Returns the "byteCost" of rendering to a color texture format.
 */
export function getColorRenderByteCost(format: PossibleColorRenderTextureFormat) {
  const byteCost = kTextureFormatInfo[format].colorRender?.byteCost;
  // MAINTENANCE_TODO: remove this assert. The issue is typescript thinks
  // PossibleColorRenderTextureFormat contains all texture formats and not just
  // a filtered list.
  assert(byteCost !== undefined);
  return byteCost;
}

/**
 * Returns the "alignment" of rendering to a color texture format.
 */
export function getColorRenderAlignment(format: PossibleColorRenderTextureFormat) {
  const alignment = kTextureFormatInfo[format].colorRender?.alignment;
  // MAINTENANCE_TODO: remove this assert. The issue is typescript thinks
  // PossibleColorRenderTextureFormat contains all texture formats and not just
  // a filtered list.
  assert(alignment !== undefined);
  return alignment;
}

/**
 * Gets the baseFormat for a texture format.
 */
export function getBaseFormatForTextureFormat(
  format: (typeof kDifferentBaseFormatTextureFormats)[number]
): ColorTextureFormat {
  return kTextureFormatInfo[format].baseFormat!;
}

export function getBaseFormatForRegularTextureFormat(
  format: RegularTextureFormat
): RegularTextureFormat | undefined {
  return kTextureFormatInfo[format].baseFormat as RegularTextureFormat;
}

/**
 * Gets the feature needed for a give texture format or undefined if none.
 */
export function getRequiredFeatureForTextureFormat(format: GPUTextureFormat) {
  return kTextureFormatInfo[format].feature;
}

export function getFeaturesForFormats<T>(
  formats: readonly (T & (GPUTextureFormat | undefined))[]
): readonly (GPUFeatureName | undefined)[] {
  return Array.from(new Set(formats.map(f => (f ? kTextureFormatInfo[f].feature : undefined))));
}

export function filterFormatsByFeature<T>(
  feature: GPUFeatureName | undefined,
  formats: readonly (T & (GPUTextureFormat | undefined))[]
): readonly (T & (GPUTextureFormat | undefined))[] {
  return formats.filter(f => f === undefined || kTextureFormatInfo[f].feature === feature);
}

function isTextureFormatTier1EnablesRenderAttachmentBlendableMultisampleResolve(
  format: GPUTextureFormat
) {
  return kTextureFormatTier1AllowsRenderAttachmentBlendableMultisampleResolve.includes(
    format as ColorTextureFormat
  );
}

function isTextureFormatTier1EnablesStorageReadOnlyWriteOnly(format: GPUTextureFormat) {
  return kTextureFormatsTier1EnablesStorageReadOnlyWriteOnly.includes(format as ColorTextureFormat);
}

export function canCopyToAspectOfTextureFormat(format: GPUTextureFormat, aspect: GPUTextureAspect) {
  const info = kTextureFormatInfo[format];
  switch (aspect) {
    case 'depth-only':
      assert(isDepthTextureFormat(format));
      return info.depth && info.depth.copyDst;
    case 'stencil-only':
      assert(isStencilTextureFormat(format));
      return info.stencil && info.stencil.copyDst;
    case 'all':
      return (
        (!isDepthTextureFormat(format) || info.depth?.copyDst) &&
        (!isStencilTextureFormat(format) || info.stencil?.copyDst) &&
        (!isColorTextureFormat(format) || !info.color?.copyDst)
      );
  }
}

export function canCopyFromAspectOfTextureFormat(
  format: GPUTextureFormat,
  aspect: GPUTextureAspect
) {
  const info = kTextureFormatInfo[format];
  switch (aspect) {
    case 'depth-only':
      assert(isDepthTextureFormat(format));
      return info.depth && info.depth.copySrc;
    case 'stencil-only':
      assert(isStencilTextureFormat(format));
      return info.stencil && info.stencil.copySrc;
    case 'all':
      return (
        (!isDepthTextureFormat(format) || info.depth?.copySrc) &&
        (!isStencilTextureFormat(format) || info.stencil?.copySrc) &&
        (!isColorTextureFormat(format) || !info.color?.copySrc)
      );
  }
}

/**
 * Returns true if all aspects of texture can be copied to (used with COPY_DST)
 */
export function canCopyToAllAspectsOfTextureFormat(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  return (
    (!info.color || info.color.copyDst) &&
    (!info.depth || info.depth.copyDst) &&
    (!info.stencil || info.stencil.copyDst)
  );
}

/**
 * Returns true if all aspects of texture can be copied from (used with COPY_SRC)
 */
export function canCopyFromAllAspectsOfTextureFormat(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  return (
    (!info.color || info.color.copySrc) &&
    (!info.depth || info.depth.copySrc) &&
    (!info.stencil || info.stencil.copySrc)
  );
}

export function isCompressedTextureFormat(format: GPUTextureFormat) {
  return format in kCompressedTextureFormatInfo;
}

export function isBCTextureFormat(format: GPUTextureFormat) {
  return format in kBCTextureFormatInfo;
}

export function isASTCTextureFormat(format: GPUTextureFormat) {
  return format in kASTCTextureFormatInfo;
}

export function isColorTextureFormat(format: GPUTextureFormat) {
  return !!kTextureFormatInfo[format].color;
}

export function isDepthTextureFormat(format: GPUTextureFormat) {
  return !!kTextureFormatInfo[format].depth;
}

export function isStencilTextureFormat(format: GPUTextureFormat) {
  return !!kTextureFormatInfo[format].stencil;
}

export function isDepthOrStencilTextureFormat(format: GPUTextureFormat) {
  return isDepthTextureFormat(format) || isStencilTextureFormat(format);
}

export function isEncodableTextureFormat(format: GPUTextureFormat) {
  return kEncodableTextureFormats.includes(format as EncodableTextureFormat);
}

/**
 * Returns if a texture can be used as a render attachment. some color formats and all
 * depth textures and stencil textures are usable with usage RENDER_ATTACHMENT.
 */
export function isTextureFormatUsableAsRenderAttachment(
  device: GPUDevice,
  format: GPUTextureFormat
) {
  if (format === 'rg11b10ufloat') {
    return device.features.has('rg11b10ufloat-renderable');
  }
  return kTextureFormatInfo[format].colorRender || isDepthOrStencilTextureFormat(format);
}

/**
 * Returns if a texture can be used as a "colorAttachment".
 */
export function isTextureFormatColorRenderable(
  device: GPUDevice,
  format: GPUTextureFormat
): boolean {
  if (format === 'rg11b10ufloat') {
    return device.features.has('rg11b10ufloat-renderable');
  }
  if (isTextureFormatTier1EnablesRenderAttachmentBlendableMultisampleResolve(format)) {
    return device.features.has('texture-formats-tier1');
  }
  return !!kAllTextureFormatInfo[format].colorRender;
}

/**
 * Returns if a texture can be blended.
 */
export function isTextureFormatBlendable(device: GPUDevice, format: GPUTextureFormat): boolean {
  if (!isTextureFormatColorRenderable(device, format)) {
    return false;
  }
  if (format === 'rg11b10ufloat') {
    return device.features.has('rg11b10ufloat-renderable');
  }
  if (is32Float(format)) {
    return device.features.has('float32-blendable');
  }
  return !!kAllTextureFormatInfo[format].colorRender?.blend;
}

/**
 * Returns the texture's type (float, unsigned-float, sint, uint, depth)
 */
export function getTextureFormatType(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  const type = info.color?.type ?? info.depth?.type ?? info.stencil?.type;
  assert(!!type);
  return type;
}

/**
 * Returns the regular texture's type (float, unsigned-float, sint, uint)
 */
export function getTextureFormatColorType(format: RegularTextureFormat) {
  const info = kTextureFormatInfo[format];
  const type = info.color?.type;
  assert(!!type);
  return type;
}

/**
 * Returns true if a texture can possibly be used as a render attachment.
 * The texture may require certain features to be enabled.
 */
export function isTextureFormatPossiblyUsableAsRenderAttachment(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  return (
    isDepthOrStencilTextureFormat(format) ||
    !!info.colorRender ||
    isTextureFormatTier1EnablesRenderAttachmentBlendableMultisampleResolve(format)
  );
}

/**
 * Returns true if a texture can possibly be used as a color render attachment.
 * The texture may require certain features to be enabled.
 */
export function isTextureFormatPossiblyUsableAsColorRenderAttachment(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  return (
    !!info.colorRender ||
    isTextureFormatTier1EnablesRenderAttachmentBlendableMultisampleResolve(format)
  );
}

/**
 * Returns true if a texture can possibly be used multisampled.
 * The texture may require certain features to be enabled.
 */
export function isTextureFormatPossiblyMultisampled(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  return (
    info.multisample ||
    isTextureFormatTier1EnablesRenderAttachmentBlendableMultisampleResolve(format)
  );
}

/**
 * Returns true if a texture can possibly be used as a storage texture.
 * The texture may require certain features to be enabled.
 */
export function isTextureFormatPossiblyStorageReadable(format: GPUTextureFormat) {
  return (
    !!kTextureFormatInfo[format].color?.storage ||
    isTextureFormatTier1EnablesStorageReadOnlyWriteOnly(format)
  );
}

/**
 * Returns true if a texture can possibly be used as a read-write storage texture.
 * The texture may require certain features to be enabled.
 */
export function isTextureFormatPossiblyStorageReadWritable(format: GPUTextureFormat) {
  return !!kTextureFormatInfo[format].color?.readWriteStorage;
}

export function is16Float(format: GPUTextureFormat) {
  return format === 'r16float' || format === 'rg16float' || format === 'rgba16float';
}

export function is32Float(format: GPUTextureFormat) {
  return format === 'r32float' || format === 'rg32float' || format === 'rgba32float';
}

/**
 * Returns true if texture is filterable as `texture_xxx<f32>`
 *
 * examples:
 * * 'rgba8unorm' -> true
 * * 'depth16unorm' -> false
 * * 'rgba32float' -> true (you need to enable feature 'float32-filterable')
 */
export function isTextureFormatPossiblyFilterableAsTextureF32(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  return info.color?.type === 'float' || is32Float(format);
}

export const kCompatModeUnsupportedStorageTextureFormats: readonly GPUTextureFormat[] = [
  'rg32float',
  'rg32sint',
  'rg32uint',
] as const;

/**
 * Return true if the format can be used as a storage texture.
 * Note: Some formats can be compiled in a shader but can not be used
 * in a pipeline or elsewhere. This function returns whether or not the format
 * can be used in general. If you want to know if the format can used when compiling
 * a shader @see {@link isTextureFormatUsableAsStorageFormatInCreateShaderModule}
 */
export function isTextureFormatUsableAsStorageFormat(
  device: GPUDevice,
  format: GPUTextureFormat
): boolean {
  if (isCompatibilityDevice(device)) {
    if (kCompatModeUnsupportedStorageTextureFormats.indexOf(format) >= 0) {
      return false;
    }
  }
  if (format === 'bgra8unorm' && device.features.has('bgra8unorm-storage')) {
    return true;
  }
  if (
    isTextureFormatTier1EnablesStorageReadOnlyWriteOnly(format) &&
    device.features.has('texture-formats-tier1')
  ) {
    return true;
  }
  const info = kTextureFormatInfo[format];
  return !!(info.color?.storage || info.depth?.storage || info.stencil?.storage);
}

/**
 * Returns true if format can be used with createShaderModule on the device.
 * Some formats may require a feature to be enabled before they can be used
 * as a storage texture. Others, can't be used in a pipeline but can be compiled
 * in a shader. Examples are rg32float, rg32uint, rg32sint which are not usable
 * in compat mode but shaders can be compiled. Similarly, bgra8unorm can be
 * compiled but can't be used in a pipeline unless feature 'bgra8unorm-storage'
 * is available.
 */
export function isTextureFormatUsableAsStorageFormatInCreateShaderModule(
  device: GPUDevice,
  format: GPUTextureFormat
): boolean {
  if (format === 'bgra8unorm') {
    return true;
  }
  const info = kTextureFormatInfo[format];
  return !!(info.color?.storage || info.depth?.storage || info.stencil?.storage);
}

export function isTextureFormatUsableAsReadWriteStorageTexture(
  device: GPUDevice,
  format: GPUTextureFormat
): boolean {
  return (
    isTextureFormatUsableAsStorageFormat(device, format) &&
    !!kTextureFormatInfo[format].color?.readWriteStorage
  );
}

export function isRegularTextureFormat(format: GPUTextureFormat) {
  return format in kRegularTextureFormatInfo;
}

/**
 * Returns true if format is both compressed and a float format, for example 'bc6h-rgb-ufloat'.
 */
export function isCompressedFloatTextureFormat(format: GPUTextureFormat) {
  return isCompressedTextureFormat(format) && format.includes('float');
}

/**
 * Returns true if format is sint or uint
 */
export function isSintOrUintFormat(format: GPUTextureFormat) {
  const info = kTextureFormatInfo[format];
  const type = info.color?.type ?? info.depth?.type ?? info.stencil?.type;
  return type === 'sint' || type === 'uint';
}

/**
 * Returns true if format can be multisampled.
 */
export const kCompatModeUnsupportedMultisampledTextureFormats: readonly GPUTextureFormat[] = [
  'r8uint',
  'r8sint',
  'rg8uint',
  'rg8sint',
  'rgba8uint',
  'rgba8sint',
  'r16uint',
  'r16sint',
  'rg16uint',
  'rg16sint',
  'rgba16uint',
  'rgba16sint',
  'rgb10a2uint',
  'rgba16float',
  'r32float',
] as const;

/**
 * Returns true if you can make a multisampled texture from the given format.
 */
export function isTextureFormatMultisampled(device: GPUDevice, format: GPUTextureFormat): boolean {
  if (isCompatibilityDevice(device)) {
    if (kCompatModeUnsupportedMultisampledTextureFormats.indexOf(format) >= 0) {
      return false;
    }
  }
  if (format === 'rg11b10ufloat') {
    return device.features.has('rg11b10ufloat-renderable');
  }
  if (isTextureFormatTier1EnablesRenderAttachmentBlendableMultisampleResolve(format)) {
    return device.features.has('texture-formats-tier1');
  }
  return kAllTextureFormatInfo[format].multisample;
}

/**
 * Returns true if a texture can be "resolved". uint/sint formats can be multisampled but
 * can not be resolved.
 */
export function isTextureFormatResolvable(device: GPUDevice, format: GPUTextureFormat): boolean {
  if (format === 'rg11b10ufloat') {
    return device.features.has('rg11b10ufloat-renderable');
  }
  if (isTextureFormatTier1EnablesRenderAttachmentBlendableMultisampleResolve(format)) {
    return device.features.has('texture-formats-tier1');
  }
  // You can't resolve a non-multisampled format.
  if (!isTextureFormatMultisampled(device, format)) {
    return false;
  }
  const info = kAllTextureFormatInfo[format];
  return !!info.colorRender?.resolve;
}

// MAINTENANCE_TODD: See if we can remove this. This doesn't seem useful since
// formats are not on/off by feature. Some are on but a feature allows them to be
// used in more cases, like going from un-renderable to renderable, etc...
export const kFeaturesForFormats = getFeaturesForFormats(kAllTextureFormats);

/**
 * Given an array of texture formats return the number of bytes per sample.
 */
export function computeBytesPerSampleFromFormats(formats: readonly GPUTextureFormat[]) {
  let bytesPerSample = 0;
  for (const format of formats) {
    // MAINTENANCE_TODO: Add colorRender to rg11b10ufloat format in kTextureFormatInfo
    // The issue is if we add it now lots of tests will break as they'll think they can
    // render to the format but are not enabling 'rg11b10ufloat-renderable'. Once we
    // get the CTS refactored (see issue 4181), then fix this.
    const info = kTextureFormatInfo[format];
    const alignedBytesPerSample = align(bytesPerSample, info.colorRender!.alignment);
    bytesPerSample = alignedBytesPerSample + info.colorRender!.byteCost;
  }
  return bytesPerSample;
}

/**
 * Given an array of GPUColorTargetState return the number of bytes per sample
 */
export function computeBytesPerSample(targets: GPUColorTargetState[]) {
  return computeBytesPerSampleFromFormats(targets.map(({ format }) => format));
}

/**
 * Returns the maximum valid size in each dimension for a given texture format.
 * This is useful because compressed formats must be a multiple of blocks in size
 * so, for example, the largest valid width of a 2d texture
 * roundDown(device.limits.maxTextureDimension2D, blockWidth)
 */
export function getMaxValidTextureSizeForFormatAndDimension(
  device: GPUDevice,
  format: GPUTextureFormat,
  dimension: GPUTextureDimension
): [number, number, number] {
  const info = getBlockInfoForTextureFormat(format);
  switch (dimension) {
    case '1d':
      return [device.limits.maxTextureDimension1D, 1, 1];
    case '2d':
      return [
        roundDown(device.limits.maxTextureDimension2D, info.blockWidth),
        roundDown(device.limits.maxTextureDimension2D, info.blockHeight),
        device.limits.maxTextureArrayLayers,
      ];
    case '3d':
      return [
        roundDown(device.limits.maxTextureDimension3D, info.blockWidth),
        roundDown(device.limits.maxTextureDimension3D, info.blockHeight),
        device.limits.maxTextureDimension3D,
      ];
  }
}
