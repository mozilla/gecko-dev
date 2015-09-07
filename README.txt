This is an experimental patch set for Wayland in latest Firefox.

Build instruction (Fedora Linux version 22,23,Rawhide):

1) git pull https://github.com/stransky/gecko-dev src
2) cd src; ./mach build
3) cd objdir/dist/bin
4) ./firefox -ProfileManager (create your custom profile)
5) ./firefox -P your_profile -no-remote

See https://bugzilla.redhat.com/show_bug.cgi?id=1054334 for details.



An explanation of the Mozilla Source Code Directory Structure and links to
project pages with documentation can be found at:

    https://developer.mozilla.org/en/Mozilla_Source_Code_Directory_Structure

For information on how to build Mozilla from the source code, see:

    http://developer.mozilla.org/en/docs/Build_Documentation

To have your bug fix / feature added to Mozilla, you should create a patch and
submit it to Bugzilla (https://bugzilla.mozilla.org). Instructions are at:

    http://developer.mozilla.org/en/docs/Creating_a_patch
    http://developer.mozilla.org/en/docs/Getting_your_patch_in_the_tree

If you have a question about developing Mozilla, and can't find the solution
on http://developer.mozilla.org, you can try asking your question in a
mozilla.* Usenet group, or on IRC at irc.mozilla.org. [The Mozilla news groups
are accessible on Google Groups, or news.mozilla.org with a NNTP reader.]

You can download nightly development builds from the Mozilla FTP server.
Keep in mind that nightly builds, which are used by Mozilla developers for
testing, may be buggy. Firefox nightlies, for example, can be found at:

    https://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central/
            - or -
    http://nightly.mozilla.org/
