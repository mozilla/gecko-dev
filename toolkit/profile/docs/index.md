# Profiles Service

Gecko based applications store almost all of their persistent data inside a profile. This is true on
both Android and Desktop. Many instances of the same Gecko application can run at the same time as
long as they use different profiles. Profiles are locked using OS file locking mechanisms when in
use, attempting to start a second instance of an application with the same profile will fail with a
profile in use error.

This documentation covers the basic architecture of the profiles service. There is also specific
documentation on [changes made to the service](./changes.md).

Two layered services are responsible for maintaining the set of known profiles that can be selected
by users through a user interface and choosing a default profile at startup. It is also possible to
select profile directories unknown to these services through various mechanisms which is common for
development and automated testing scenarios.

The first layer uses the [Toolkit Profile Service](#toolkit-profile-service) which maintains a list
of `nsIToolkitProfiles`. This is called at startup and it selects the profile directories to use for
this instance of the application.

The second layer is the [Selectable Profile Service](#selectable-profile-service). This service is
only used on Desktop. When in use the `nsIToolkitProfile` managed by the Toolkit Profile Service can
be thought of as a profile group and the Selectable Profile Service allows for managing and
switching between profiles within that group.

## Profile Directories

A profile is made up of one or two directories. When made up of two directories, one directory is
used for persistent data while the second is used for caches that can safely be deleted. When just
one directory is used the persistent data and caches are colocated. These directories are commonly
referred to as the profile root directory and the profile local directory (because in Windows
enterprise environments the caches will be kept local to the computer while the persistent data may
be stored in a directory that is copied across different computers).

Whenever a Gecko application is running it is possible to get the root directory via the `ProfD` key
of the directory service and the local directory via the `ProfLD` key. In the case that only one
directory is in use these will be identical values. For the most part components interested in
storing their data should use these methods to get the profile directories and should not attempt to
use the profile services.

Whether two directories or just one are used depends on the location of the root directory in the
filesystem. If it is in the default location for profiles, which is the OS standard location for
user settings, then a second directory is used and is in the OS standard location for user caches.
Both profile directories will have the same leaf name.

The default locations for profile root directories are:

* Windows: `%APPDATA%\Mozilla\Firefox\Profiles`
* Linux: `~/.mozilla/firefox`
* macOS: `~/Library/Application Support/Firefox/Profiles`

The default locations for profile local directories are:

* Windows: `%LOCALAPPDATA%\Mozilla\Firefox\Profiles`
* Linux: `~/.cache/mozilla/firefox`
* macOS: `~/Library/Caches/Firefox/Profiles`

Profile directory selection happens during startup and depends on environment variables, command
line arguments, and defaults stored in `profiles.ini`. See [nsToolkitProfileService::SelectStartupProfile](https://searchfox.org/mozilla-central/rev/fccab99f5b400b33b9ad16e7f066a5020119fbdc/toolkit/profile/nsToolkitProfileService.cpp#1490)
for the specifics on how this works.

## Profile `storeID`

Every profile is assigned a `storeID` which is a short alphanumeric string. When the Selectable
Profile Service is in use the same `storeID` is shared by all profiles in the same group. In all
other cases the `storeID` is unique to every profile. This identifier is used as the mechanism for
grouping profiles as well as to allow storing per-group data.

The `storeID` is stored in the `toolkit.profiles.storeID` preference and is assigned the first time
it is needed.

## Profiles Datastore Service

The [`ProfilesDatastoreService`](https://searchfox.org/mozilla-central/rev/fccab99f5b400b33b9ad16e7f066a5020119fbdc/browser/components/profiles/ProfilesDatastoreService.sys.mjs)
manages a unique SQLite database for each `storeID`. This means that when there are a group of
profiles using the same `storeID` then they all use the same SQLite database allowing for persisting
data that can be used by every profile in the group. It provides direct access to a SQLite
connection and a mechanism for notifying running instances that are using the same database.

This `storeID` is used to construct a filename for the database file. For development and temporary
profiles with no associated `nsIToolkitProfile` the database is stored in the profile root
directory. Otherwise this file is stored in a directory shared by all profiles.

The database is created the first time a component requests a connection to it.

## Toolkit Profile Service

The Toolkit Profile Services manages a list of known `nsIToolkitProfiles` in the `profiles.ini`
and keeps track of which is the default for a given install of Firefox (though note that a
[legacy behaviour](./changes.md#profile-per-install) exists in some cases such as when
running as a Snap on Linux). Installs are differentiated based on their install directory or for
Windows Store installs the store package identifier. In either case the string is hashed using
CityHash to generate the unique identifier used in `profiles.ini`.

Each `nsIToolkitProfile` has a name and a path to the root directory for the profile. This path is
normally relative to the operating system defaults listed above but in some cases it may be an
absolute path provided by the user.

On startup this service is responsible for selecting the profile directories to use. Command line
arguments and environment variables can select a specific directory to use or the default
`nsIToolkitProfile` is selected from `profiles.ini` based on the current install. See
[nsToolkitProfileService::SelectStartupProfile](https://searchfox.org/mozilla-central/rev/fccab99f5b400b33b9ad16e7f066a5020119fbdc/toolkit/profile/nsToolkitProfileService.cpp#1490)
for the specifics on how this works. The service may also be configured to display a toolkit profile
selector UI on startup. This UI only shows the `nsIToolkitProfiles` listed in `profiles.ini`. If no
profile could be selected on startup then a new `nsIToolkitProfile` will be created and set as the
default for this installation.

In addition to the toolkit profile selector window optionally shown at startup, the `about:profiles`
page allows users to view and manage the toolkit profiles known to the Toolkit Profile Service.

In the case that the Selectable Profile Service is in use the `nsIToolkitProfile` should be thought
of as representing a group of profiles and the current root directory represents the current default
profile for the group. The Profiles Datastore Service's `storeID` for the profile group will be
stored with the `nsIToolkitProfile`. An additional setting controls whether the Selectable Profile
Service's selection UI is displayed on startup.

## Selectable Profile Service

The Selectable Profile Service manages a list of profiles in the Profiles Datastore Service. These
profiles all share a `storeID` and are linked by a single `nsIToolkitProfile`. When new profiles are
created this service is responsible for initializing the new profile with the same `storeID` so that
relevant data is shared across all profiles in the group. The `storeID` is stored in the
`nsIToolkitProfile` section in `profiles.ini` and the `Path` property is used to represent the most
recently used profile in the group. This will be changed frequently as the user switches between
running instances.

Users can create additional Selectable Profiles using the Profiles menu and menuitems (see
https://support.mozilla.org/en-US/kb/profile-management for details). The about:newprofile,
about:editprofile, about:deleteprofile, and about:profilemanager pages are used to manage Selectable
Profiles.
