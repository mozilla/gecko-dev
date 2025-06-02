# Profiles Service Changes

## Consistent profiles.ini

Prior to Gecko 67 the Toolkit Profile Service loaded data from `profiles.ini` into a custom
in-memory structure and then manually serialized that data back to `profiles.ini` when needed. This
meant that any unknown data in `profiles.ini` would be ignored when read and then lost when the
data was serialized. Since `profiles.ini` must be readable and writable by any version of Gecko
that a user might be expected to have installed this makes it very challenging to evolve the schema
of `profiles.ini`.

In Gecko 67 this was changed to load the full INI data into memory, modify that when changes were
made and then serialize the entire INI data back to disk so any unknown data was not lost. In order
to detect whether data might have been lost due to the use of an older version of the application a
new `Version` property was added to the INI file, initially set to `2`. The absence of this property
indicates that a destructive write to `profiles.ini` has taken place.

This is still not perfect, the data is read only once during startup and then serialized whenever
needed but if multiple Gecko processes are running then it would be possible for them to clobber
each others changes. To protect against this when the INI file is read the process captures its last
modification time and size. If data needs to be changed then the process checks that the INI file
on disk still has the same properties and if so refuses to overwrite the file. This avoids
clobbering another process's data at the risk that the current process's changes cannot be written
to disk. The various profile management interfaces were made to check that writing is possible
before allowing the user to make modifications however there are still time windows where this
problem occurs. Since the number of users using profile management was low this edge case was
considered not worth solving.

## Profile per install

Prior to Gecko 67 the Toolkit Profile Service only supported one default profile for the entire OS
user. This meant that different installs of an application such as Release and Beta would use the
same default profile. This was marked in the profile section in `profiles.ini` with the `Default=1`
flag. This behaviour was undesirable with the introduction of profile downgrade protection which
blocked using a profile if it had most recently been used with a newer version of the application.

In order to solve this new `[InstallXXXXXXX]` sections were added to `profiles.ini`. Each section
used a hash of the application installation directory to store per-install data. The data included
the current default profile for the install using the profile relative path as the identifier
because the profile section numbers are not stable. It also included a `Locked` flag used as part of
the migration process.

To protect against loss of data in the event that the user made use of an application based on a
version of Gecko prior to 67 the install sections would also be backed up to an `installs.ini` file.
When a destructive modification is detected (by the absence of the `profiles.ini` `Version` property)
this data is re-inserted into profiles.ini.

Migrating to profile per install happened at startup for each install that did not already have an
install section present in profiles.ini and either no specific profile was selected on the command
line or via the profile manager UI or if a profile was chosen via an environment variable (normally
used when the application has restarted to apply an update). The goals of migration were:

* For a user with only a single install of the application continue using the old default profile.
* For a user with multiple installs only one of them should select the old default profile as its
  new default, other installs should have new profiles created.
  * In this case prefer to assign the old default to whichever install is the default browser for
    the operating system.

For each install on first startup after the feature was enabled the old default profile would be
inspected:

1. If the profile was last run by a different install (by looking at its `compatibility.ini` file)
   then it would be ignored.
2. If the profile had already been *locked* by a different install then it would be ignored.
3. Otherwise the profile will be marked as default for this install but not locked, the profile will
   also be removed as default from any other installs. The profile is also unmarked as the old
   default.
4. If the old default was ignored then a new profile is created and locked to the current install.
5. If the old default was selected and the current install is the default browser then the profile
   is locked to this install. For technical reasons this has to happen later in startup.

Note that since a claimed profile is unmarked as the old default this selection process happens for
the first application instance and then only for subsequent instances that are restarted via
application update.

In order to maintain compatibility with older versions of the application an empty profile is
created and marked as the old-style default profile. Any future uses of older version will use this
profile instead of downgrading the other profiles.

## New profiles UI, profile groups and shared data

In Gecko 138 a new profiles experience was implemented with more user friendly experience. Early on
in the development process the decision was made to implement this as a mostly separate service
with separate storage layered on top of the old
[Toolkit Profile Service](./index.md#toolkit-profile-service). There were many reasons for this
choice:

* The new feature was to be rolled out slowly to existing users so it was desirable to make minimal
  changes to the existing service that might cause bugs for users not yet included in the rollout.
* New profiles were to have richer metadata including avatars and themes than was currently
  available. Attempting to store these in `profiles.ini` would cause issues if users [used older
  versions](#consistent-profiles-ini) and telemetry showed that an unnerving number of users still
  do.
* The new UI also needed to be able to display up to date information about all profiles and allow
  for concurrent modification by multiple running instances. The existing `profiles.ini` file is not
  suitable for this purpose.
* Many users already have multiple profiles without necessarily realising it. Different installs
  will create different default profiles, profile reset sometimes leaves behind old profiles.
  Exposing these in the new interface would be confusing.
* Some users use different profiles for privacy related reasons. Exposing these in the interface
  could inadvertently expose sensitive information.
* It was desirable to still provide a way for QA and developers to switch between entirely clean
  setups of the application.

The main changes to the older Toolkit Profile Service were:

* Adding a `StoreID` property to profile sections to allow selecting the profile group.
* Detecting the selected `nsIToolkitProfile` based on the `storeID` of the currently running
  profile.
* Adding a new method to update just the current `nsIToolkitProfile's` section in `profiles.ini`
  without clobbering another application's changes to the rest of the file.
