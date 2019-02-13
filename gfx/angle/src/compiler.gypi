# Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'variables':
    {
        # This file list is shared with the GN build.
        'angle_translator_lib_sources':
        [
            '../include/EGL/egl.h',
            '../include/EGL/eglext.h',
            '../include/EGL/eglplatform.h',
            '../include/GLES2/gl2.h',
            '../include/GLES2/gl2ext.h',
            '../include/GLES2/gl2platform.h',
            '../include/GLES3/gl3.h',
            '../include/GLES3/gl3ext.h',
            '../include/GLES3/gl3platform.h',
            '../include/GLSLANG/ShaderLang.h',
            '../include/GLSLANG/ShaderVars.h',
            '../include/KHR/khrplatform.h',
            '../include/angle_gl.h',
            'common/RefCountObject.cpp',
            'common/RefCountObject.h',
            'common/angleutils.h',
            'common/angleutils.cpp',
            'common/blocklayout.cpp',
            'common/blocklayout.h',
            'common/debug.cpp',
            'common/debug.h',
            'common/event_tracer.cpp',
            'common/event_tracer.h',
            'common/mathutil.cpp',
            'common/mathutil.h',
            'common/platform.h',
            'common/tls.cpp',
            'common/tls.h',
            'common/utilities.cpp',
            'common/utilities.h',
            'common/version.h',
            'compiler/translator/BaseTypes.h',
            'compiler/translator/BuiltInFunctionEmulator.cpp',
            'compiler/translator/BuiltInFunctionEmulator.h',
            'compiler/translator/CodeGen.cpp',
            'compiler/translator/Common.h',
            'compiler/translator/Compiler.cpp',
            'compiler/translator/Compiler.h',
            'compiler/translator/ConstantUnion.h',
            'compiler/translator/DetectCallDepth.cpp',
            'compiler/translator/DetectCallDepth.h',
            'compiler/translator/DetectDiscontinuity.cpp',
            'compiler/translator/DetectDiscontinuity.h',
            'compiler/translator/Diagnostics.cpp',
            'compiler/translator/Diagnostics.h',
            'compiler/translator/DirectiveHandler.cpp',
            'compiler/translator/DirectiveHandler.h',
            'compiler/translator/ExtensionBehavior.h',
            'compiler/translator/FlagStd140Structs.cpp',
            'compiler/translator/FlagStd140Structs.h',
            'compiler/translator/ForLoopUnroll.cpp',
            'compiler/translator/ForLoopUnroll.h',
            'compiler/translator/HashNames.h',
            'compiler/translator/InfoSink.cpp',
            'compiler/translator/InfoSink.h',
            'compiler/translator/Initialize.cpp',
            'compiler/translator/Initialize.h',
            'compiler/translator/InitializeDll.cpp',
            'compiler/translator/InitializeDll.h',
            'compiler/translator/InitializeGlobals.h',
            'compiler/translator/InitializeParseContext.cpp',
            'compiler/translator/InitializeParseContext.h',
            'compiler/translator/InitializeVariables.cpp',
            'compiler/translator/InitializeVariables.h',
            'compiler/translator/IntermTraverse.cpp',
            'compiler/translator/Intermediate.h',
            'compiler/translator/Intermediate.cpp',
            'compiler/translator/IntermNode.h',
            'compiler/translator/IntermNode.cpp',
            'compiler/translator/LoopInfo.cpp',
            'compiler/translator/LoopInfo.h',
            'compiler/translator/MMap.h',
            'compiler/translator/NodeSearch.h',
            'compiler/translator/OutputESSL.cpp',
            'compiler/translator/OutputESSL.h',
            'compiler/translator/OutputGLSL.cpp',
            'compiler/translator/OutputGLSL.h',
            'compiler/translator/OutputGLSLBase.cpp',
            'compiler/translator/OutputGLSLBase.h',
            'compiler/translator/OutputHLSL.cpp',
            'compiler/translator/OutputHLSL.h',
            'compiler/translator/ParseContext.cpp',
            'compiler/translator/ParseContext.h',
            'compiler/translator/PoolAlloc.cpp',
            'compiler/translator/PoolAlloc.h',
            'compiler/translator/Pragma.h',
            'compiler/translator/QualifierAlive.cpp',
            'compiler/translator/QualifierAlive.h',
            'compiler/translator/RegenerateStructNames.cpp',
            'compiler/translator/RegenerateStructNames.h',
            'compiler/translator/RemoveTree.cpp',
            'compiler/translator/RemoveTree.h',
            'compiler/translator/RenameFunction.h',
            'compiler/translator/RewriteElseBlocks.cpp',
            'compiler/translator/RewriteElseBlocks.h',
            'compiler/translator/ScalarizeVecAndMatConstructorArgs.cpp',
            'compiler/translator/ScalarizeVecAndMatConstructorArgs.h',
            'compiler/translator/SearchSymbol.cpp',
            'compiler/translator/SearchSymbol.h',
            'compiler/translator/StructureHLSL.cpp',
            'compiler/translator/StructureHLSL.h',
            'compiler/translator/SymbolTable.cpp',
            'compiler/translator/SymbolTable.h',
            'compiler/translator/TranslatorESSL.cpp',
            'compiler/translator/TranslatorESSL.h',
            'compiler/translator/TranslatorGLSL.cpp',
            'compiler/translator/TranslatorGLSL.h',
            'compiler/translator/TranslatorHLSL.cpp',
            'compiler/translator/TranslatorHLSL.h',
            'compiler/translator/Types.cpp',
            'compiler/translator/Types.h',
            'compiler/translator/UnfoldShortCircuit.cpp',
            'compiler/translator/UnfoldShortCircuit.h',
            'compiler/translator/UnfoldShortCircuitAST.cpp',
            'compiler/translator/UnfoldShortCircuitAST.h',
            'compiler/translator/UniformHLSL.cpp',
            'compiler/translator/UniformHLSL.h',
            'compiler/translator/UtilsHLSL.cpp',
            'compiler/translator/UtilsHLSL.h',
            'compiler/translator/ValidateLimitations.cpp',
            'compiler/translator/ValidateLimitations.h',
            'compiler/translator/ValidateOutputs.cpp',
            'compiler/translator/ValidateOutputs.h',
            'compiler/translator/VariableInfo.cpp',
            'compiler/translator/VariableInfo.h',
            'compiler/translator/VariablePacker.cpp',
            'compiler/translator/VariablePacker.h',
            'compiler/translator/VersionGLSL.cpp',
            'compiler/translator/VersionGLSL.h',
            'compiler/translator/compilerdebug.cpp',
            'compiler/translator/compilerdebug.h',
            'compiler/translator/depgraph/DependencyGraph.cpp',
            'compiler/translator/depgraph/DependencyGraph.h',
            'compiler/translator/depgraph/DependencyGraphBuilder.cpp',
            'compiler/translator/depgraph/DependencyGraphBuilder.h',
            'compiler/translator/depgraph/DependencyGraphOutput.cpp',
            'compiler/translator/depgraph/DependencyGraphOutput.h',
            'compiler/translator/depgraph/DependencyGraphTraverse.cpp',
            'compiler/translator/glslang.h',
            'compiler/translator/glslang.l',
            'compiler/translator/glslang.y',
            'compiler/translator/glslang_lex.cpp',
            'compiler/translator/glslang_tab.cpp',
            'compiler/translator/glslang_tab.h',
            'compiler/translator/intermOut.cpp',
            'compiler/translator/intermediate.h',
            'compiler/translator/length_limits.h',
            'compiler/translator/parseConst.cpp',
            'compiler/translator/timing/RestrictFragmentShaderTiming.cpp',
            'compiler/translator/timing/RestrictFragmentShaderTiming.h',
            'compiler/translator/timing/RestrictVertexShaderTiming.cpp',
            'compiler/translator/timing/RestrictVertexShaderTiming.h',
            'compiler/translator/util.cpp',
            'compiler/translator/util.h',
            'third_party/compiler/ArrayBoundsClamper.cpp',
            'third_party/compiler/ArrayBoundsClamper.h',
        ],
        'angle_preprocessor_sources':
        [
            'compiler/preprocessor/DiagnosticsBase.cpp',
            'compiler/preprocessor/DiagnosticsBase.h',
            'compiler/preprocessor/DirectiveHandlerBase.cpp',
            'compiler/preprocessor/DirectiveHandlerBase.h',
            'compiler/preprocessor/DirectiveParser.cpp',
            'compiler/preprocessor/DirectiveParser.h',
            'compiler/preprocessor/ExpressionParser.cpp',
            'compiler/preprocessor/ExpressionParser.h',
            'compiler/preprocessor/ExpressionParser.y',
            'compiler/preprocessor/Input.cpp',
            'compiler/preprocessor/Input.h',
            'compiler/preprocessor/Lexer.cpp',
            'compiler/preprocessor/Lexer.h',
            'compiler/preprocessor/Macro.cpp',
            'compiler/preprocessor/Macro.h',
            'compiler/preprocessor/MacroExpander.cpp',
            'compiler/preprocessor/MacroExpander.h',
            'compiler/preprocessor/Preprocessor.cpp',
            'compiler/preprocessor/Preprocessor.h',
            'compiler/preprocessor/SourceLocation.h',
            'compiler/preprocessor/Token.cpp',
            'compiler/preprocessor/Token.h',
            'compiler/preprocessor/Tokenizer.cpp',
            'compiler/preprocessor/Tokenizer.h',
            'compiler/preprocessor/Tokenizer.l',
            'compiler/preprocessor/numeric_lex.h',
            'compiler/preprocessor/pp_utils.h',
        ],
    },
    # Everything below this is duplicated in the GN build. If you change
    # anything also change angle/BUILD.gn
    'targets':
    [
        {
            'target_name': 'preprocessor',
            'type': 'static_library',
            'includes': [ '../build/common_defines.gypi', ],
            'sources': [ '<@(angle_preprocessor_sources)', ],
            'conditions':
            [
                ['angle_build_winrt==1',
                {
                    'msvs_enable_winrt' : '1',
                }],
                ['angle_build_winphone==1',
                {
                    'msvs_enable_winphone' : '1',
                }],
            ],
        },
        {
            'target_name': 'translator_lib',
            'type': 'static_library',
            'dependencies': [ 'preprocessor' ],
            'includes': [ '../build/common_defines.gypi', ],
            'include_dirs':
            [
                '.',
                '../include',
            ],
            'defines':
            [
                # define the static translator to indicate exported
                # classes are (in fact) locally defined
                'ANGLE_TRANSLATOR_STATIC',
            ],
            'sources':
            [
                '<@(angle_translator_lib_sources)',
            ],
            'msvs_settings':
            {
              'VCLibrarianTool':
              {
                'AdditionalOptions': ['/ignore:4221']
              },
            },
            'conditions':
            [
                ['angle_build_winrt==1',
                {
                    'msvs_enable_winrt' : '1',
                }],
                ['angle_build_winphone==1',
                {
                    'msvs_enable_winphone' : '1',
                }],
            ],
        },

        {
            'target_name': 'translator',
            'type': '<(component)',
            'dependencies': [ 'translator_lib' ],
            'includes': [ '../build/common_defines.gypi', ],
            'include_dirs':
            [
                '.',
                '../include',
            ],
            'defines':
            [
                'ANGLE_TRANSLATOR_IMPLEMENTATION',
            ],
            'sources':
            [
                'compiler/translator/ShaderLang.cpp',
                'compiler/translator/ShaderVars.cpp'
            ],
            'conditions':
            [
                ['angle_build_winrt==1',
                {
                    'msvs_enable_winrt' : '1',
                }],
                ['angle_build_winphone==1',
                {
                    'msvs_enable_winphone' : '1',
                }],
            ],
        },

        {
            'target_name': 'translator_static',
            'type': 'static_library',
            'dependencies': [ 'translator_lib' ],
            'includes': [ '../build/common_defines.gypi', ],
            'include_dirs':
            [
                '.',
                '../include',
            ],
            'defines':
            [
                'ANGLE_TRANSLATOR_STATIC',
            ],
            'direct_dependent_settings':
            {
                'defines':
                [
                    'ANGLE_TRANSLATOR_STATIC',
                ],
            },
            'sources':
            [
                'compiler/translator/ShaderLang.cpp',
                'compiler/translator/ShaderVars.cpp'
            ],
            'conditions':
            [
                ['angle_build_winrt==1',
                {
                    'msvs_enable_winrt' : '1',
                }],
                ['angle_build_winphone==1',
                {
                    'msvs_enable_winphone' : '1',
                }],
            ],
        },
    ],
}
