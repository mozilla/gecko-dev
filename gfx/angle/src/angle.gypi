# Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'variables':
    {
        'angle_code': 1,
        'angle_post_build_script%': 0,
        'angle_gen_path': '<(SHARED_INTERMEDIATE_DIR)/angle',
        'angle_id_script_base': 'commit_id.py',
        'angle_id_script': '<(angle_gen_path)/<(angle_id_script_base)',
        'angle_id_header_base': 'commit.h',
        'angle_id_header': '<(angle_gen_path)/id/<(angle_id_header_base)',
        'angle_use_commit_id%': '<!(python <(angle_id_script_base) check ..)',
        'angle_enable_d3d9%': 0,
        'angle_enable_d3d11%': 0,
        'conditions':
        [
            ['OS=="win"',
            {
                'angle_enable_d3d9%': 1,
                'angle_enable_d3d11%': 1,
            }],
        ],
    },
    'includes':
    [
        'compiler.gypi',
        'libGLESv2.gypi',
        'libEGL.gypi'
    ],

    'targets':
    [
        {
            'target_name': 'copy_scripts',
            'type': 'none',
            'includes': [ '../build/common_defines.gypi', ],
            'hard_dependency': 1,
            'copies':
            [
                {
                    'destination': '<(angle_gen_path)',
                    'files': [ 'copy_compiler_dll.bat', '<(angle_id_script_base)' ],
                },
            ],
            'conditions':
            [
                ['angle_build_winrt==1',
                {
                    'msvs_enable_winrt' : '1',
                    'type' : 'shared_library',
                }],
                ['angle_build_winphone==1',
                {
                    'msvs_enable_winphone' : '1',
                }],
            ],
        },
    ],
    'conditions':
    [
        ['angle_use_commit_id!=0',
        {
            'targets':
            [
                {
                    'target_name': 'commit_id',
                    'type': 'none',
                    'includes': [ '../build/common_defines.gypi', ],
                    'dependencies': [ 'copy_scripts', ],
                    'hard_dependency': 1,
                    'actions':
                    [
                        {
                            'action_name': 'Generate ANGLE Commit ID Header',
                            'message': 'Generating ANGLE Commit ID',
                            # reference the git index as an input, so we rebuild on changes to the index
                            'inputs': [ '<(angle_id_script)', '<(angle_path)/.git/index' ],
                            'outputs': [ '<(angle_id_header)' ],
                            'msvs_cygwin_shell': 0,
                            'action':
                            [
                                'python', '<(angle_id_script)', 'gen', '<(angle_path)', '<(angle_id_header)'
                            ],
                        },
                    ],
                    'all_dependent_settings':
                    {
                        'include_dirs':
                        [
                            '<(angle_gen_path)',
                        ],
                    },
                    'conditions':
                    [
                        ['angle_build_winrt==1',
                        {
                            'msvs_enable_winrt' : '1',
                            'type' : 'shared_library',
                        }],
                        ['angle_build_winphone==1',
                        {
                            'msvs_enable_winphone' : '1',
                        }],
                    ],
                }
            ]
        },
        { # angle_use_commit_id==0
            'targets':
            [
                {
                    'target_name': 'commit_id',
                    'type': 'none',
                    'hard_dependency': 1,
                    'includes': [ '../build/common_defines.gypi', ],
                    'copies':
                    [
                        {
                            'destination': '<(angle_gen_path)/id',
                            'files': [ '<(angle_id_header_base)' ]
                        }
                    ],
                    'all_dependent_settings':
                    {
                        'include_dirs':
                        [
                            '<(angle_gen_path)',
                        ],
                    },
                    'conditions':
                    [
                        ['angle_build_winrt==1',
                        {
                            'msvs_enable_winrt' : '1',
                            'type' : 'shared_library',
                        }],
                        ['angle_build_winphone==1',
                        {
                            'msvs_enable_winphone' : '1',
                        }],
                    ],
                }
            ]
        }],
        ['OS=="win"',
        {
            'targets':
            [
                {
                    'target_name': 'copy_compiler_dll',
                    'type': 'none',
                    'dependencies': [ 'copy_scripts', ],
                    'includes': [ '../build/common_defines.gypi', ],
                    'conditions':
                    [
                        ['angle_build_winrt==0',
                        {
                            'actions':
                            [
                                {
                                    'action_name': 'copy_dll',
                                    'message': 'Copying D3D Compiler DLL...',
                                    'msvs_cygwin_shell': 0,
                                    'inputs': [ 'copy_compiler_dll.bat' ],
                                    'outputs': [ '<(PRODUCT_DIR)/d3dcompiler_46.dll' ],
                                    'action':
                                    [
                                        "<(angle_gen_path)/copy_compiler_dll.bat",
                                        "$(PlatformName)",
                                        "<(windows_sdk_path)",
                                        "<(PRODUCT_DIR)"
                                    ],
                                },
                            ], #actions
                        }],
                        ['angle_build_winrt==1',
                        {
                            'msvs_enable_winrt' : '1',
                            'type' : 'shared_library',
                        }],
                        ['angle_build_winphone==1',
                        {
                            'msvs_enable_winphone' : '1',
                        }],
                    ]
                },
            ], # targets
        }],
        ['angle_post_build_script!=0 and OS=="win"',
        {
            'targets':
            [
                {
                    'target_name': 'post_build',
                    'type': 'none',
                    'includes': [ '../build/common_defines.gypi', ],
                    'dependencies': [ 'libGLESv2', 'libEGL' ],
                    'actions':
                    [
                        {
                            'action_name': 'ANGLE Post-Build Script',
                            'message': 'Running <(angle_post_build_script)...',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<(angle_post_build_script)', '<!@(["python", "<(angle_post_build_script)", "inputs", "<(angle_path)", "<(CONFIGURATION_NAME)", "$(PlatformName)", "<(PRODUCT_DIR)"])' ],
                            'outputs': [ '<!@(python <(angle_post_build_script) outputs "<(angle_path)" "<(CONFIGURATION_NAME)" "$(PlatformName)" "<(PRODUCT_DIR)")' ],
                            'action': ['python', '<(angle_post_build_script)', 'run', '<(angle_path)', '<(CONFIGURATION_NAME)', '$(PlatformName)', '<(PRODUCT_DIR)'],
                        },
                    ], #actions
                },
            ], # targets
        }],
    ] # conditions
}
