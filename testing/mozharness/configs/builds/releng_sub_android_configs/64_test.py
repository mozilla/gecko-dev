config = {
    'base_name': 'Android armv7 unit tests %(branch)s',
    'stage_platform': 'android-test',
    'build_type': 'api-15-opt',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-15-frontend/nightly',
    'tooltool_manifest_src': 'mobile/android/config/tooltool-manifests/android-frontend/releng.manifest',
    'multi_locale_config_platform': 'android',
    'postflight_build_mach_commands': [
        ['gradle', 'app:testAutomationDebugUnitTest'],
    ],
}
