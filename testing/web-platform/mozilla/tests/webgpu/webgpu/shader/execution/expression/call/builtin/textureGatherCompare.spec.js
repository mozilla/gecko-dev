/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Execution tests for the 'textureGatherCompare' builtin function

A texture gather compare operation performs a depth comparison on four texels in a depth texture and collects the results into a single vector, as follows:
 * Find the four texels that would be used in a depth sampling operation with linear filtering, from mip level 0:
   - Use the specified coordinate, array index (when present), and offset (when present).
   - The texels are adjacent, forming a square, when considering their texture space coordinates (u,v).
   - Selected texels at the texture edge, cube face edge, or cube corners are handled as in ordinary texture sampling.
 * For each texel, perform a comparison against the depth reference value, yielding a 0.0 or 1.0 value, as controlled by the comparison sampler parameters.
 * Yield the four-component vector where the components are the comparison results with the texels with relative texel coordinates as follows:

   Result component  Relative texel coordinate
    x                (umin,vmax)
    y                (umax,vmax)
    z                (umax,vmin)
    w                (umin,vmin)
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { kCompareFunctions } from '../../../../../capability_info.js';
import {
  isDepthTextureFormat,
  isEncodableTextureFormat,
  kDepthStencilFormats } from
'../../../../../format_info.js';

import {
  checkCallResults,
  chooseTextureSize,
  createTextureWithRandomDataAndGetTexels,
  doTextureCalls,
  generateSamplePointsCube,
  generateTextureBuiltinInputs2D,
  kCubeSamplePointMethods,
  kSamplePointMethods,



  WGSLTextureSampleTest } from
'./texture_utils.js';

export const g = makeTestGroup(WGSLTextureSampleTest);

g.test('array_2d_coords').
specURL('https://www.w3.org/TR/WGSL/#texturegathercompare').
desc(
  `
A: i32, u32

fn textureGatherCompare(t: texture_depth_2d_array, s: sampler_comparison, coords: vec2<f32>, array_index: A, depth_ref: f32) -> vec4<f32>
fn textureGatherCompare(t: texture_depth_2d_array, s: sampler_comparison, coords: vec2<f32>, array_index: A, depth_ref: f32, offset: vec2<i32>) -> vec4<f32>

Parameters:
 * t: The depth texture to read from
 * s: The sampler_comparison
 * coords: The texture coordinates
 * array_index: The 0-based array index.
 * depth_ref: The reference value to compare the sampled depth value against
 * offset:
    - The optional texel offset applied to the unnormalized texture coordinate before sampling the texture.
      This offset is applied before applying any texture wrapping modes.
    - The offset expression must be a creation-time expression (e.g. vec2<i32>(1, 2)).
    - Each offset component must be at least -8 and at most 7.
      Values outside of this range will result in a shader-creation error.
`
).
params((u) =>
u.
combine('format', kDepthStencilFormats)
// filter out stencil only formats
.filter((t) => isDepthTextureFormat(t.format))
// MAINTENANCE_TODO: Remove when support for depth24plus, depth24plus-stencil8, and depth32float-stencil8 is added.
.filter((t) => isEncodableTextureFormat(t.format)).
combine('minFilter', ['nearest', 'linear']).
beginSubcases().
combine('samplePoints', kSamplePointMethods).
combine('A', ['i32', 'u32']).
combine('addressModeU', ['clamp-to-edge', 'repeat', 'mirror-repeat']).
combine('addressModeV', ['clamp-to-edge', 'repeat', 'mirror-repeat']).
combine('compare', kCompareFunctions).
combine('offset', [false, true])
).
beforeAllSubcases((t) => {
  t.skipIfTextureFormatNotSupported(t.params.format);
}).
fn(async (t) => {
  const { format, samplePoints, A, addressModeU, addressModeV, minFilter, compare, offset } =
  t.params;

  const [width, height] = chooseTextureSize({ minSize: 8, minBlocks: 4, format });
  const depthOrArrayLayers = 4;

  const descriptor = {
    format,
    size: { width, height, depthOrArrayLayers },
    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING
  };
  const { texels, texture } = await createTextureWithRandomDataAndGetTexels(t, descriptor);
  const sampler = {
    addressModeU,
    addressModeV,
    compare,
    minFilter,
    magFilter: minFilter,
    mipmapFilter: minFilter
  };

  const calls = generateTextureBuiltinInputs2D(50, {
    method: samplePoints,
    sampler,
    descriptor,
    arrayIndex: { num: texture.depthOrArrayLayers, type: A },
    depthRef: true,
    offset,
    hashInputs: [format, samplePoints, A, addressModeU, addressModeV, minFilter, offset]
  }).map(({ coords, arrayIndex, depthRef, offset }) => {
    return {
      builtin: 'textureGatherCompare',
      coordType: 'f',
      coords,
      arrayIndex,
      arrayIndexType: A === 'i32' ? 'i' : 'u',
      depthRef,
      offset
    };
  });
  const textureType = 'texture_depth_2d_array';
  const viewDescriptor = {};
  const results = await doTextureCalls(t, texture, viewDescriptor, textureType, sampler, calls);
  const res = await checkCallResults(
    t,
    { texels, descriptor, viewDescriptor },
    textureType,
    sampler,
    calls,
    results
  );
  t.expectOK(res);
});

g.test('array_3d_coords').
specURL('https://www.w3.org/TR/WGSL/#texturegathercompare').
desc(
  `
A: i32, u32

fn textureGatherCompare(t: texture_depth_cube_array, s: sampler_comparison, coords: vec3<f32>, array_index: A, depth_ref: f32) -> vec4<f32>

Parameters:
 * t: The depth texture to read from
 * s: The sampler_comparison
 * coords: The texture coordinates
 * array_index: The 0-based array index.
 * depth_ref: The reference value to compare the sampled depth value against
`
).
params((u) =>
u.
combine('format', kDepthStencilFormats)
// filter out stencil only formats
.filter((t) => isDepthTextureFormat(t.format))
// MAINTENANCE_TODO: Remove when support for depth24plus, depth24plus-stencil8, and depth32float-stencil8 is added.
.filter((t) => isEncodableTextureFormat(t.format)).
combine('minFilter', ['nearest', 'linear']).
beginSubcases().
combine('samplePoints', kCubeSamplePointMethods).
combine('A', ['i32', 'u32']).
combine('addressMode', ['clamp-to-edge', 'repeat', 'mirror-repeat']).
combine('compare', kCompareFunctions)
).
beforeAllSubcases((t) => {
  t.skipIfTextureViewDimensionNotSupported('cube-array');
}).
fn(async (t) => {
  const { format, A, samplePoints, addressMode, minFilter, compare } = t.params;

  const viewDimension = 'cube-array';
  const size = chooseTextureSize({ minSize: 8, minBlocks: 2, format, viewDimension });

  const descriptor = {
    format,
    ...(t.isCompatibility && { textureBindingViewDimension: viewDimension }),
    size,
    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING
  };
  const { texels, texture } = await createTextureWithRandomDataAndGetTexels(t, descriptor);
  const sampler = {
    addressModeU: addressMode,
    addressModeV: addressMode,
    addressModeW: addressMode,
    compare,
    minFilter,
    magFilter: minFilter,
    mipmapFilter: minFilter
  };

  const calls = generateSamplePointsCube(50, {
    method: samplePoints,
    sampler,
    descriptor,
    textureBuiltin: 'textureGatherCompare',
    arrayIndex: { num: texture.depthOrArrayLayers / 6, type: A },
    depthRef: true,
    hashInputs: [format, samplePoints, addressMode, minFilter]
  }).map(({ coords, depthRef, arrayIndex }) => {
    return {
      builtin: 'textureGatherCompare',
      arrayIndex,
      arrayIndexType: A === 'i32' ? 'i' : 'u',
      coordType: 'f',
      coords,
      depthRef
    };
  });
  const viewDescriptor = {
    dimension: viewDimension
  };
  const textureType = 'texture_depth_cube_array';
  const results = await doTextureCalls(t, texture, viewDescriptor, textureType, sampler, calls);
  const res = await checkCallResults(
    t,
    { texels, descriptor, viewDescriptor },
    textureType,
    sampler,
    calls,
    results
  );
  t.expectOK(res);
});

g.test('sampled_2d_coords').
specURL('https://www.w3.org/TR/WGSL/#texturegathercompare').
desc(
  `
fn textureGatherCompare(t: texture_depth_2d, s: sampler_comparison, coords: vec2<f32>, depth_ref: f32) -> vec4<f32>
fn textureGatherCompare(t: texture_depth_2d, s: sampler_comparison, coords: vec2<f32>, depth_ref: f32, offset: vec2<i32>) -> vec4<f32>

Parameters:
 * t: The depth texture to read from
 * s: The sampler_comparison
 * coords: The texture coordinates
 * depth_ref: The reference value to compare the sampled depth value against
 * offset:
    - The optional texel offset applied to the unnormalized texture coordinate before sampling the texture.
      This offset is applied before applying any texture wrapping modes.
    - The offset expression must be a creation-time expression (e.g. vec2<i32>(1, 2)).
    - Each offset component must be at least -8 and at most 7.
      Values outside of this range will result in a shader-creation error.
`
).
params((u) =>
u.
combine('format', kDepthStencilFormats)
// filter out stencil only formats
.filter((t) => isDepthTextureFormat(t.format))
// MAINTENANCE_TODO: Remove when support for depth24plus, depth24plus-stencil8, and depth32float-stencil8 is added.
.filter((t) => isEncodableTextureFormat(t.format)).
combine('minFilter', ['nearest', 'linear']).
beginSubcases().
combine('C', ['i32', 'u32']).
combine('samplePoints', kSamplePointMethods).
combine('addressMode', ['clamp-to-edge', 'repeat', 'mirror-repeat']).
combine('compare', kCompareFunctions).
combine('offset', [false, true])
).
fn(async (t) => {
  const { format, C, samplePoints, addressMode, compare, minFilter, offset } = t.params;

  const [width, height] = chooseTextureSize({ minSize: 8, minBlocks: 4, format });
  const descriptor = {
    format,
    size: { width, height },
    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING
  };
  const { texels, texture } = await createTextureWithRandomDataAndGetTexels(t, descriptor);
  const sampler = {
    addressModeU: addressMode,
    addressModeV: addressMode,
    compare,
    minFilter,
    magFilter: minFilter,
    mipmapFilter: minFilter
  };

  const calls = generateTextureBuiltinInputs2D(50, {
    method: samplePoints,
    sampler,
    descriptor,
    offset,
    depthRef: true,
    hashInputs: [format, C, samplePoints, addressMode, minFilter, compare, offset]
  }).map(({ coords, depthRef, offset }) => {
    return {
      builtin: 'textureGatherCompare',
      coordType: 'f',
      coords,
      depthRef,
      offset
    };
  });
  const textureType = 'texture_depth_2d';
  const viewDescriptor = {};
  const results = await doTextureCalls(t, texture, viewDescriptor, textureType, sampler, calls);
  const res = await checkCallResults(
    t,
    { texels, descriptor, viewDescriptor },
    textureType,
    sampler,
    calls,
    results
  );
  t.expectOK(res);
});

g.test('sampled_3d_coords').
specURL('https://www.w3.org/TR/WGSL/#texturegathercompare').
desc(
  `
fn textureGatherCompare(t: texture_depth_cube, s: sampler_comparison, coords: vec3<f32>, depth_ref: f32) -> vec4<f32>

Parameters:
 * t: The depth texture to read from
 * s: The sampler_comparison
 * coords: The texture coordinates
 * depth_ref: The reference value to compare the sampled depth value against
`
).
params((u) =>
u.
combine('format', kDepthStencilFormats)
// filter out stencil only formats
.filter((t) => isDepthTextureFormat(t.format))
// MAINTENANCE_TODO: Remove when support for depth24plus, depth24plus-stencil8, and depth32float-stencil8 is added.
.filter((t) => isEncodableTextureFormat(t.format)).
combine('minFilter', ['nearest', 'linear']).
beginSubcases().
combine('samplePoints', kCubeSamplePointMethods).
combine('addressMode', ['clamp-to-edge', 'repeat', 'mirror-repeat']).
combine('compare', kCompareFunctions)
).
fn(async (t) => {
  const { format, samplePoints, addressMode, minFilter, compare } = t.params;

  const viewDimension = 'cube';
  const [width, height] = chooseTextureSize({ minSize: 8, minBlocks: 2, format, viewDimension });
  const depthOrArrayLayers = 6;

  const descriptor = {
    format,
    ...(t.isCompatibility && { textureBindingViewDimension: viewDimension }),
    size: { width, height, depthOrArrayLayers },
    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING
  };
  const { texels, texture } = await createTextureWithRandomDataAndGetTexels(t, descriptor);
  const sampler = {
    addressModeU: addressMode,
    addressModeV: addressMode,
    addressModeW: addressMode,
    compare,
    minFilter,
    magFilter: minFilter,
    mipmapFilter: minFilter
  };

  const calls = generateSamplePointsCube(50, {
    method: samplePoints,
    sampler,
    descriptor,
    depthRef: true,
    textureBuiltin: 'textureGatherCompare',
    hashInputs: [format, samplePoints, addressMode, minFilter, compare]
  }).map(({ coords, depthRef }) => {
    return {
      builtin: 'textureGatherCompare',
      coordType: 'f',
      coords,
      depthRef
    };
  });
  const viewDescriptor = {
    dimension: viewDimension
  };
  const textureType = 'texture_depth_cube';
  const results = await doTextureCalls(t, texture, viewDescriptor, textureType, sampler, calls);
  const res = await checkCallResults(
    t,
    { texels, descriptor, viewDescriptor },
    textureType,
    sampler,
    calls,
    results
  );
  t.expectOK(res);
});