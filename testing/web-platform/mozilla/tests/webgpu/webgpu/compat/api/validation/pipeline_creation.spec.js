/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Tests that createComputePipeline(async), and createRenderPipeline(async)
reject pipelines that are invalid in compat mode

- test that depth textures can not be used with non-comparison samplers

TODO:
- test that a shader that has more than min(maxSamplersPerShaderStage, maxSampledTexturesPerShaderStage)
  texture+sampler combinations generates a validation error.
`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { kShaderStages } from '../../../shader/validation/decl/util.js';
import { CompatibilityTest } from '../../compatibility_test.js';

export const g = makeTestGroup(CompatibilityTest);

g.test('depth_textures').
desc('Tests that depth textures can not be used with non-comparison samplers in compat mode.').
params((u) =>
u //
.combineWithParams([
{
  sampleWGSL: 'textureSample(t, s, vec2f(0))', // should pass
  textureType: 'texture_2d<f32>'
},
{
  sampleWGSL: 'textureSample(t, s, vec2f(0))',
  textureType: 'texture_depth_2d'
},
{
  sampleWGSL: 'textureSample(t, s, vec3f(0))',
  textureType: 'texture_depth_cube'
},
{
  sampleWGSL: 'textureSample(t, s, vec2f(0), 0)',
  textureType: 'texture_depth_2d_array'
},
{
  sampleWGSL: 'textureSample(t, s, vec2f(0), vec2i(0, 0))',
  textureType: 'texture_depth_2d'
},
{
  sampleWGSL: 'textureSample(t, s, vec2f(0), 0, vec2i(0, 0))',
  textureType: 'texture_depth_2d_array'
},
{
  sampleWGSL: 'textureSampleLevel(t, s, vec2f(0), 0)',
  textureType: 'texture_depth_2d'
},
{
  sampleWGSL: 'textureSampleLevel(t, s, vec3f(0), 0)',
  textureType: 'texture_depth_cube'
},
{
  sampleWGSL: 'textureSampleLevel(t, s, vec2f(0), 0, 0)',
  textureType: 'texture_depth_2d_array'
},
{
  sampleWGSL: 'textureSampleLevel(t, s, vec2f(0), 0, vec2i(0, 0))',
  textureType: 'texture_depth_2d'
},
{
  sampleWGSL: 'textureSampleLevel(t, s, vec2f(0), 0, 0, vec2i(0, 0))',
  textureType: 'texture_depth_2d_array'
},
{
  sampleWGSL: 'textureGather(t, s, vec2f(0))',
  textureType: 'texture_depth_2d'
},
{
  sampleWGSL: 'textureGather(t, s, vec3f(0))',
  textureType: 'texture_depth_cube'
},
{
  sampleWGSL: 'textureGather(t, s, vec2f(0), 0)',
  textureType: 'texture_depth_2d_array'
},
{
  sampleWGSL: 'textureGather(t, s, vec2f(0), vec2i(0, 0))',
  textureType: 'texture_depth_2d'
},
{
  sampleWGSL: 'textureGather(t, s, vec2f(0), 0, vec2i(0, 0))',
  textureType: 'texture_depth_2d_array'
}]
).
combine('stage', kShaderStages).
filter((t) => t.sampleWGSL.startsWith('textureGather') || t.stage === 'fragment').
combine('async', [false, true])
).
fn((t) => {
  const { sampleWGSL, textureType, stage, async } = t.params;

  const usageWGSL = `_ = ${sampleWGSL};`;
  const module = t.device.createShaderModule({
    code: `
        @group(0) @binding(0) var t: ${textureType};
        @group(1) @binding(0) var s: sampler;

        // make sure it's fine such a combination exists but it's not used.
        fn unused() {
          ${usageWGSL};
        }

        @vertex fn vs() -> @builtin(position) vec4f {
            ${stage === 'vertex' ? usageWGSL : ''}
            return vec4f(0);
        }

        @fragment fn fs() -> @location(0) vec4f {
            ${stage === 'fragment' ? usageWGSL : ''}
            return vec4f(0);
        }

        @compute @workgroup_size(1) fn cs() {
            ${stage === 'compute' ? usageWGSL : ''};
        }
      `
  });

  const success = !t.isCompatibility || textureType === 'texture_2d<f32>';
  switch (stage) {
    case 'compute':
      t.doCreateComputePipelineTest(async, success, {
        layout: 'auto',
        compute: {
          module
        }
      });
      break;
    case 'fragment':
    case 'vertex':
      t.doCreateRenderPipelineTest(async, success, {
        layout: 'auto',
        vertex: {
          module
        },
        fragment: {
          module,
          targets: [{ format: 'rgba8unorm' }]
        }
      });
      break;
  }
});