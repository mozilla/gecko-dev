# Shavar Lists Documentation

## Shavar - Brief Overview

Shavar is a Python based service that provides Tracking Protection lists to Firefox clients. Every 6 hours, the Firefox client sends a POST request to the service for an updated list and, if there is an update, Shavar returns the CloudFront URL where the client can download the latest list from. Note that users can trigger the update manually using `about:url-classifier`

## Updating Lists Flow

[shavar-list-creation](https://github.com/mozilla-services/shavar-list-creation)  fetches blocklist .json from [shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists) and generates safebrowsing-compatible digest list files.

Disconnect provides Mozilla with an updated blocklist every so often, with a pull request to the [https://github.com/mozilla-services/shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists) repository, from which the responsible engineer has to manually merge the PR to trigger an automatic update on the shavar server.

### The Lists

The following lists are kept in the [shavar-prod-lists]([https://github.com/mozilla-services/shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists)) repository.

| List | Purpose |
| ---  | ------- |
| [disconnect-blacklist.json](https://github.com/mozilla-services/shavar-prod-lists/blob/master/disconnect-blacklist.json)| This blocklist is the core of tracking protection in Firefox. Firefox consumes the list as follows: <br> <br> <strong>Tracking:</strong> anything in the Advertising, Analytics, Social, Content, or Disconnect category. Firefox ships two versions of the tracking lists: the "Level 1" list, which excludes the "Content" category, and the "Level 2" list which includes the "Content" category <br> <br> <strong>Cryptomining:</strong> anything in the Cryptomining category <br> <br> <strong>Fingerprinting:</strong> anything in the FingerprintingInvasive category. By default, ETP's fingerprinting blocking only blocks Tracking Fingerprinters, that is domains which appear in both the FingerprintingInvasive category and one of the Tracking categories. Firefox does not use the FingerprintingGeneral category at this time. |
| [disconnect-entitylist.json](https://github.com/mozilla-services/shavar-prod-lists/blob/master/disconnect-entitylist.json) | A version controlled copy of Disconnect's list of entities. ETP classifies a resource as a tracking resource when it is present on the blocklist and loaded as a third-party. The Entity list is used to allow third-party subresources that are wholly owned by the same company that owns the top-level website that the user is visiting.|

### Updating Process

The Tracking Protection lists updates happen every two or three months. The Disconnect folks aggregate the changes within the latest time frame and create a PR to submit the list update to the [shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists) repo. We then take the following steps to review and merge the changes to our production list.

* An automation script first verifies the integrity of the updated list
* Shavar Reviewers inspect the commits in the PR manually. Normally, we accept the change sets and won’t request further modifications from Disconnect. However, we can request them to merge or divide commits to uplift changes to different versions easily.
* Shavar Reviewers merge the PR to the [master](https://github.com/mozilla-services/shavar-prod-lists/tree/master) branch
* Checking whether we need to uplift some commits to previous versions. This is required occasionally to deploy fixes for ETP Webcompat issues.
* Creating branches for the Firefox versions. Note that there is a special rule for creating the branches; we must create odd versions first if the Nightly channel is an even version. Otherwise, we need to create even versions first. Then, we need to wait for 30 minutes and then create branches for other versions.

To ensure Firefox clients receive the appropriate update for their ETP lists, we leverage a branching strategy. This means we maintain separate branches for different Firefox versions. When a client running Firefox 80 (fx80) requests an ETP list update, it's directed to the branch named 80.0 within the [shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists/branches) repository on Github.

## Shavar Infrastructure
![alt_text](media/shavarPipeline.jpg)

The infrastructure consists of a Jenkins Pipeline that builds the actual Tracking Protection lists and uploads them to an S3 Bucket. The lists are served to Firefox clients by the Shavar Service, which runs on an auto-scaling EC2 instance group behind a Cloudfront distribution.

### .ini File Structure

The shavar_list_creation.ini file is used for configuring the creation and management of tracking protection and block lists in the Mozilla Shavar service. It consists of multiple sections, each specifying different aspects of the list creation and uploading processes.

Example .ini file: [sample_shavar_list_creation.ini](https://github.com/mozilla-services/shavar-list-creation/blob/main/sample_shavar_list_creation.ini)

#### [main] section
The **[main]** section contains general configuration settings for the upload process. Key variables include:

- `s3_upload`: Boolean indicating whether to upload the output to S3.
- `remote_settings_upload`: Boolean indicating whether to upload to remote settings.
- `default_disconnect_url`: URL to the default Disconnect blacklist JSON.

#### Format of Other Sections

Other sections in the `shavar_list_creation.ini` file configure specific lists and their settings. These sections typically follow a common format, including settings for categories, output filenames, and versioning requirements.

Here is a general example of how these sections are structured:

```ini
[section-name]
categories=Category1|Category2|Category3
output=output-filename
versioning_needed=true
```

- `categories`: Specifies the categories to include, separated by `|`.
- `output`: Filename for the generated data file.
- `versioning_needed`: Boolean indicating if versioning is required.

## Remote Settings

Remote Settings (RS) is used to manage evergreen data in Firefox, which can be accessed with an API to sync data between clients and the server. You can find more about it here [Remote Settings — Firefox Source Docs documentation](https://firefox-source-docs.mozilla.org/services/settings/index.html)

We intend to migrate the ETP list management to leverage RS. Benefits include,

* **Reduced Maintenance:** RS is an existing service with established tooling and infrastructure, reducing the need for a dedicated Shavar maintenance team.
* **GCP Integration:** Since RS already integrates with GCP, no additional setup or team would be required for cloud storage and no migration would be required from aws to gcp.
* **Shavar Deprecation:** Shifting to RS allows for the complete deprecation of Shavar, simplifying the overall architecture.


### How to run the shavar-lists-creation scripts for Remote Settings



The remote settings upload logic is contained within the [shavar-list-creation](https://github.com/mozilla-services/shavar-list-creation) repo. There are certain environment variables that need to be set to run these scripts locally.



1. Set your environment variables using `export KEY=value` in your terminal or directly in your bash/zshrc file

```
# The server locations are as follows
# dev: https://remote-settings-dev.allizom.org/v1
# stage: https://remote-settings.allizom.org/v1
# prod: https://remote-settings.mozilla.org/v1
export SERVER="server_location"

# For testing purposes, we use auth tokens, but the server uses userpass. To run locally, set the auth method to tokens
export REMOTE_SETTINGS_AUTH_METHOD="token"

# Get your auth token by logging into the server using single sign on, it will be of the format "Bearer token_string"
export AUTHORIZATION="Bearer your_token"

# Since this script is used for both shavar and remote settings, we need to set an execution environment. JENKINS is used for the shavar server, and GKE is used for remote settings. Since we are concerned with remote settings here, set it to GKE
export EXECUTION_ENVIRONMENT="GKE"

# Lastly, we want to set which server we are uploading to, make sure you set this based on the AUTHORIZATION and SERVER you have set
export ENVIRONMENT="dev"
```

2. Once these environment variables are set, we can directly run the scripts to upload to the server. The `settings.py` code will use one of rs_dev.ini, rs_stage.ini, rs_prod.ini based on where you mean to upload to. These ini files already have rs upload enabled and s3 upload disabled.
```
python3 lists2safebrowsing.py
```

### Remote settings execution environment

The scripts are run daily on the google cloud servers (GKE) for each environment (dev, stage and prod). The scripts compare the lists from shavar-prod-lists and the ones already uploaded using the chunknum values, and only trigger an update if theres a new change.

Dev changes are automatically updated without a reviewer on remote settings. Stage and prod changes require a reviewer to merge the changes in. An email is sent out when there are new changes.

## Migration from Shavar to Remote Settings

As of August 2024, we are in the final stages of transitioning completely from Shavar to Remote Settings for providing versioned lists to Firefox.

However, Shavar support will be needed for an extended period to support older firefox versions.

### Versioning in Remote Settings

Remote Settings implements versioning through the use of `filter_expression` tags on lists. These tags indicate which version of Firefox each list supports.

#### How it works:
1. When an update is triggered, Firefox clients receive all Remote Settings lists.
2. Upon fetching, each client retrieves the specific list that matches its `filter_expression`.

This approach ensures that each Firefox version receives the appropriate list tailored to its capabilities and requirements.

### Maintaining Nightly Builds

#### Important Note
The **master** branch in the [shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists/branches) repository provides the list for Nightly builds. To maintain consistency and prevent issues:

- **Do not** create a new branch for Nightly versions without first updating the `lists2safebrowsing.py` script. Adding a new branch could lead to errors since the script uses the master branch as the latest list, which is what is used for Nightly versions.
- Ensure all changes to the Nightly list are properly reflected in the script to avoid discrepancies.

## Known Pain Points of Shavar

* **Frequent Build Errors:** We frequently encounter errors during the build process, as evidenced in the shavar-ci channel. Investigating and resolving these errors should be a priority to ensure a smooth publishing workflow.
* **Inconsistent Versioning Rules:** Branching for different versions currently follows a set of undocumented rules. These rules need to be reviewed, standardized, and documented to eliminate confusion and potential bugs. A thorough review can identify and address any lingering bugs from the system's inception.
* **Unverifiable Versioning System:** The current system lacks mechanisms to verify the integrity and accuracy of the versioning process.

## Links

* [https://github.com/mozilla-services/shavar](https://github.com/mozilla-services/shavar)
* [https://github.com/mozilla-services/shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists)
* [https://github.com/mozilla-services/shavar-list-creation](https://github.com/mozilla-services/shavar-list-creation)
* [https://wiki.mozilla.org/Services/Shavar](https://wiki.mozilla.org/Services/Shavar)
