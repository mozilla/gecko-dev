Settings Files
==============

The files in this folder are copies of saved search settings files. They are used in tests to ensure the settings are loaded and migrated correctly.

When adding new files, please add an additional entry here to describe what those files are.

Naming
------

The name formatting should be one of:

* v1-<usefulname>.json
  * This is used when the settings file is being used to test migration from
    a specific version. It should never be updated to a newer version.
* <usefulname>.json
  * This is used when the version doesn't matter, as long as it is still
    maintaining the intent of the test.

Descriptions
------------

* ignorelist.json
  * Used for testing that the ignore list correctly ignores engines loaded from
    settings when the ignore list matches those engines.
* settings-loading.json
  * Used for testing that loading engine settings works correctly.
* v1-metadata-migration.json
  * Used for testing migration of saved engine (metadata) data from the old format
    to the newest. Also as a best for testing removing old profile based xml engines.
* v1-migrate-to-webextension.json
  * Used to test migration of legacy add-ons that used the OpenSearch defintions
    to WebExtensions if the WebExtension is installed.
* v1-obsolete-app.json
  * Used to test that old app engines are ignored when loading the settings.
* v1-obsolete-distribution.json
  * Used to ensure that old distribution engines are ignored when loading the settings.
* v1-obsolete-langpack.json
  * Used to ensure that old language pack engines are ignored when loading the settings.
* v6-correct-default-engine-hashes.json
  * Used to ensure metadata hashes are correctly upgraded.
* v6-ids-upgrade.json
  * Used to ensure that ids are correctly added to search engines on upgrade.
* v6-migration-renames.json
  * Used to test that some Wikipedia renames are correctly handled when upgrading
    from v6.
* v6-wrong-default-engine-hashes.json
  * Used to ensure that incorrect metadata hashes are not upgraded.
* v6-wrong-third-party-engine-hashes.json
  * Used to ensure that incorrect metadata hashes are not upgraded when a third
    party engine is default.
* v7-loadPath-migration.json
  * Used to ensure that load paths are correctly migrated.
