import { ColorTextureFormat, getTextureFormatType } from '../../../format_info.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../gpu_test.js';
import {
  getFragmentShaderCodeWithOutput,
  getPlainTypeInfo,
  kDefaultVertexShaderCode,
} from '../../../util/shader.js';

export type ColorTargetState = GPUColorTargetState & { format: ColorTextureFormat };

const values = [0, 1, 0, 1];
export function getDescriptorForCreateRenderPipelineValidationTest(
  device: GPUDevice,
  options: {
    primitive?: GPUPrimitiveState;
    targets?: ColorTargetState[];
    multisample?: GPUMultisampleState;
    depthStencil?: GPUDepthStencilState;
    fragmentShaderCode?: string;
    noFragment?: boolean;
    fragmentConstants?: Record<string, GPUPipelineConstantValue>;
  } = {}
): GPURenderPipelineDescriptor {
  const {
    primitive = {},
    targets = [{ format: 'rgba8unorm' }] as const,
    multisample = {},
    depthStencil,
    fragmentShaderCode = getFragmentShaderCodeWithOutput([
      {
        values,
        plainType: getPlainTypeInfo(
          getTextureFormatType(targets[0] ? targets[0].format : 'rgba8unorm')
        ),
        componentCount: 4,
      },
    ]),
    noFragment = false,
    fragmentConstants = {},
  } = options;

  return {
    vertex: {
      module: device.createShaderModule({
        code: kDefaultVertexShaderCode,
      }),
      entryPoint: 'main',
    },
    fragment: noFragment
      ? undefined
      : {
          module: device.createShaderModule({
            code: fragmentShaderCode,
          }),
          entryPoint: 'main',
          targets,
          constants: fragmentConstants,
        },
    layout: device.createPipelineLayout({ bindGroupLayouts: [] }),
    primitive,
    multisample,
    depthStencil,
  };
}

export class CreateRenderPipelineValidationTest extends AllFeaturesMaxLimitsGPUTest {
  getDescriptor(
    options: {
      primitive?: GPUPrimitiveState;
      targets?: ColorTargetState[];
      multisample?: GPUMultisampleState;
      depthStencil?: GPUDepthStencilState;
      fragmentShaderCode?: string;
      noFragment?: boolean;
      fragmentConstants?: Record<string, GPUPipelineConstantValue>;
    } = {}
  ): GPURenderPipelineDescriptor {
    return getDescriptorForCreateRenderPipelineValidationTest(this.device, options);
  }

  getPipelineLayout(): GPUPipelineLayout {
    return this.device.createPipelineLayout({ bindGroupLayouts: [] });
  }
}
