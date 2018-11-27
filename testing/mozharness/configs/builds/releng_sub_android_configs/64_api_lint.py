config = {
    'stage_platform': 'android-api-lint',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-frontend/nightly',
    'multi_locale_config_platform': 'android',
    # apilint doesn't produce a package. So don't collect package metrics.
    'disable_package_metrics': True,
    'postflight_build_mach_commands': [
        ['android',
         'api-lint',
        ],
    ],
    'artifact_flag_build_variant_in_try': None, # There's no artifact equivalent.
    'max_build_output_timeout': 0,
}
