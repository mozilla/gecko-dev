export const description = `
Execution tests for textureSampleBaseClampToEdge
`;

import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { TexelView } from '../../../../../util/texture/texel_view.js';

import {
  checkCallResults,
  createTextureWithRandomDataAndGetTexels,
  createVideoFrameWithRandomDataAndGetTexels,
  doTextureCalls,
  generateTextureBuiltinInputs2D,
  kSamplePointMethods,
  TextureCall,
  vec2,
  WGSLTextureSampleTest,
} from './texture_utils.js';

export const g = makeTestGroup(WGSLTextureSampleTest);

async function createTextureAndDataForTest(
  t: GPUTest,
  descriptor: GPUTextureDescriptor,
  isExternal: boolean
): Promise<{
  texels: TexelView[];
  texture: GPUTexture | GPUExternalTexture;
  videoFrame?: VideoFrame;
}> {
  if (isExternal) {
    const { texels, videoFrame } = createVideoFrameWithRandomDataAndGetTexels(descriptor.size);
    const texture = t.device.importExternalTexture({ source: videoFrame });
    return { texels, texture, videoFrame };
  } else {
    return await createTextureWithRandomDataAndGetTexels(t, descriptor);
  }
}

g.test('2d_coords')
  .specURL('https://www.w3.org/TR/WGSL/#texturesamplebaseclamptoedge')
  .desc(
    `
fn textureSampleBaseClampToEdge(t: texture_2d<f32>, s: sampler, coords: vec2<f32>) -> vec4<f32>
fn textureSampleBaseClampToEdge(t: texture_external, s: sampler, coords: vec2<f32>) -> vec4<f32>


Parameters:
 * t  The texture to sample.
 * s  The sampler type.
 * coords The texture coordinates used for sampling.
`
  )
  .params(u =>
    u
      .combine('textureType', ['texture_2d<f32>', 'texture_external'] as const)
      .beginSubcases()
      .combine('samplePoints', kSamplePointMethods)
      .combine('addressModeU', ['clamp-to-edge', 'repeat', 'mirror-repeat'] as const)
      .combine('addressModeV', ['clamp-to-edge', 'repeat', 'mirror-repeat'] as const)
      .combine('minFilter', ['nearest', 'linear'] as const)
  )
  .beforeAllSubcases(t =>
    t.skipIf(
      t.params.textureType === 'texture_external' && typeof VideoFrame === 'undefined',
      'VideoFrames are not supported'
    )
  )
  .fn(async t => {
    const { textureType, samplePoints, addressModeU, addressModeV, minFilter } = t.params;

    const descriptor: GPUTextureDescriptor = {
      format: 'rgba8unorm',
      size: [8, 8],
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
      mipLevelCount: 3,
    };

    const isExternal = textureType === 'texture_external';
    const { texture, texels, videoFrame } = await createTextureAndDataForTest(
      t,
      descriptor,
      isExternal
    );
    try {
      const sampler: GPUSamplerDescriptor = {
        addressModeU,
        addressModeV,
        minFilter,
        magFilter: minFilter,
        mipmapFilter: minFilter,
      };

      const calls: TextureCall<vec2>[] = generateTextureBuiltinInputs2D(50, {
        method: samplePoints,
        sampler,
        descriptor,
        hashInputs: [samplePoints, addressModeU, addressModeV, minFilter],
      }).map(({ coords }) => {
        return {
          builtin: 'textureSampleBaseClampToEdge',
          coordType: 'f',
          coords,
        };
      });
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
    } finally {
      videoFrame?.close();
    }
  });
