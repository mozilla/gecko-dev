%define optimized_build 1

%global mozappdir     %{_libdir}/%{name}
%global mozappdirdev  %{_libdir}/%{name}-devel-%{version}

Summary:        Mozilla Firefox Nightly Web browser
Name:           firefox-wayland
Version:        56.2
Release:        1%{?pre_tag}%{?dist}
URL:            https://www.mozilla.org/firefox/
License:        MPLv1.1 or GPLv2+ or LGPLv2+
Group:          Applications/Internet
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  pkgconfig(libpng)
BuildRequires:  zip
BuildRequires:  bzip2-devel
BuildRequires:  pkgconfig(zlib)
BuildRequires:  pkgconfig(libIDL-2.0)
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  pkgconfig(gtk+-2.0)
BuildRequires:  pkgconfig(krb5)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(freetype2) >= %{freetype_version}
BuildRequires:  pkgconfig(xt)
BuildRequires:  pkgconfig(xrender)
BuildRequires:  pkgconfig(libstartup-notification-1.0)
BuildRequires:  pkgconfig(dri)
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  dbus-glib-devel
BuildRequires:  autoconf213
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(icu-i18n)
BuildRequires:  pkgconfig(gconf-2.0)
BuildRequires:  yasm
Requires:       mozilla-filesystem
BuildRequires:  desktop-file-utils
BuildRequires:  system-bookmarks
BuildRequires:  rust
BuildRequires:  cargo

Obsoletes:      mozilla <= 37:1.7.13
Provides:       webclient

%description
Mozilla Firefox is an open-source web browser, designed for standards
compliance, performance and portability.

#---------------------------------------------------------------------

%prep
%setup -q

# Build patches, can't change backup suffix from default because during build
# there is a compare of config and js/config directories and .orig suffix is
# ignored during this compare.
patch -p1 < firefox-install-dir.patch

%{__rm} -f .mozconfig
%{__cp} firefox-mozconfig .mozconfig

echo "ac_add_options --enable-debug" >> .mozconfig
%if !%{?optimized_build}
echo "ac_add_options --disable-optimize" >> .mozconfig
%else
%define optimize_flags "none"
# Fedora 26 (gcc7) needs to disable default build flags (mozbz#1342344)
%if 0%{?fedora} > 25
%define optimize_flags "-g -O2"
%endif
%if %{optimize_flags} != "none"
echo 'ac_add_options --enable-optimize=%{?optimize_flags}' >> .mozconfig
%else
echo 'ac_add_options --enable-optimize' >> .mozconfig
%endif
%endif

#---------------------------------------------------------------------

%build
MOZ_OPT_FLAGS="-g -O2 -fno-delete-null-pointer-checks"
%if !%{?optimized_build}
MOZ_OPT_FLAGS=$(echo "$MOZ_OPT_FLAGS" | %{__sed} -e 's/-O2//')
%endif
export CFLAGS=$MOZ_OPT_FLAGS
export CXXFLAGS=$MOZ_OPT_FLAGS
export LDFLAGS=$MOZ_LINK_FLAGS

export PREFIX='%{_prefix}'
export LIBDIR='%{_libdir}'

MOZ_SMP_FLAGS=-j1
# On x86 architectures, Mozilla can build up to 4 jobs at once in parallel,
# however builds tend to fail on other arches when building in parallel.
%ifarch %{ix86} x86_64 ppc ppc64 ppc64le aarch64
[ -z "$RPM_BUILD_NCPUS" ] && \
     RPM_BUILD_NCPUS="`/usr/bin/getconf _NPROCESSORS_ONLN`"
[ "$RPM_BUILD_NCPUS" -ge 2 ] && MOZ_SMP_FLAGS=-j2
[ "$RPM_BUILD_NCPUS" -ge 4 ] && MOZ_SMP_FLAGS=-j4
[ "$RPM_BUILD_NCPUS" -ge 8 ] && MOZ_SMP_FLAGS=-j8
%endif

make -f client.mk build STRIP="/bin/true" MOZ_MAKE_FLAGS="$MOZ_SMP_FLAGS" MOZ_SERVICES_SYNC="1"

#---------------------------------------------------------------------

%install
DESTDIR=$RPM_BUILD_ROOT make -C objdir install

%{__mkdir_p} $RPM_BUILD_ROOT{%{_libdir},%{_bindir},%{_datadir}/applications}

desktop-file-install --dir $RPM_BUILD_ROOT%{_datadir}/applications firefox-wayland.desktop

# set up the firefox start script
%{__rm} -rf $RPM_BUILD_ROOT%{_bindir}/firefox
%{__cat} firefox-wayland.sh.in > $RPM_BUILD_ROOT%{_bindir}/firefox-wayland
%{__chmod} 755 $RPM_BUILD_ROOT%{_bindir}/firefox-wayland

%{__rm} -f $RPM_BUILD_ROOT/%{mozappdir}/firefox-config
%{__rm} -f $RPM_BUILD_ROOT/%{mozappdir}/update-settings.ini

for s in 16 22 24 32 48 256; do
    %{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/${s}x${s}/apps
    %{__cp} -p browser/branding/official/default${s}.png \
               $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/${s}x${s}/apps/firefox.png
done

# Install hight contrast icon
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/symbolic/apps

# Register as an application to be visible in the software center
#
# NOTE: It would be *awesome* if this file was maintained by the upstream
# project, translated and installed into the right place during `make install`.
#
# See http://www.freedesktop.org/software/appstream/docs/ for more details.
#
mkdir -p $RPM_BUILD_ROOT%{_datadir}/appdata
cat > $RPM_BUILD_ROOT%{_datadir}/appdata/%{name}.appdata.xml <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!-- Copyright 2014 Richard Hughes <richard@hughsie.com> -->
<!--
BugReportURL: https://bugzilla.mozilla.org/show_bug.cgi?id=1071061
SentUpstream: 2014-09-22
-->
<application>
  <id type="desktop">firefox.desktop</id>
  <metadata_license>CC0-1.0</metadata_license>
  <description>
    <p>
      Bringing together all kinds of awesomeness to make browsing better for you.
      Get to your favorite sites quickly – even if you don’t remember the URLs.
      Type your term into the location bar (aka the Awesome Bar) and the autocomplete
      function will include possible matches from your browsing history, bookmarked
      sites and open tabs.
    </p>
    <!-- FIXME: Needs another couple of paragraphs -->
  </description>
  <url type="homepage">http://www.mozilla.org/en-US/</url>
  <screenshots>
    <screenshot type="default">https://raw.githubusercontent.com/hughsie/fedora-appstream/master/screenshots-extra/firefox/a.png</screenshot>
    <screenshot>https://raw.githubusercontent.com/hughsie/fedora-appstream/master/screenshots-extra/firefox/b.png</screenshot>
    <screenshot>https://raw.githubusercontent.com/hughsie/fedora-appstream/master/screenshots-extra/firefox/c.png</screenshot>
  </screenshots>
  <!-- FIXME: change this to an upstream email address for spec updates
  <updatecontact>someone_who_cares@upstream_project.org</updatecontact>
   -->
</application>
EOF

%{__mkdir_p} $RPM_BUILD_ROOT/%{mozappdir}/browser/defaults/preferences

# System config dir
%{__mkdir_p} $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}/pref

# Copy over the LICENSE
%{__install} -p -c -m 644 LICENSE $RPM_BUILD_ROOT/%{mozappdir}

# Use the system hunspell dictionaries
%{__rm} -rf ${RPM_BUILD_ROOT}%{mozappdir}/dictionaries
ln -s %{_datadir}/myspell ${RPM_BUILD_ROOT}%{mozappdir}/dictionaries

# Default
%{__cp} firefox-redhat-default-prefs.js ${RPM_BUILD_ROOT}%{mozappdir}/browser/defaults/preferences

# Add distribution.ini
%{__mkdir_p} ${RPM_BUILD_ROOT}%{mozappdir}/distribution
%{__cp} distribution.ini ${RPM_BUILD_ROOT}%{mozappdir}/distribution

# Remove copied libraries to speed up build
rm -f ${RPM_BUILD_ROOT}%{mozappdirdev}/sdk/lib/libmozjs.so
rm -f ${RPM_BUILD_ROOT}%{mozappdirdev}/sdk/lib/libmozalloc.so
rm -f ${RPM_BUILD_ROOT}%{mozappdirdev}/sdk/lib/libxul.so

chmod 644 ${RPM_BUILD_ROOT}%{mozappdir}/browser/blocklist.xml
#---------------------------------------------------------------------

# Moves defaults/preferences to browser/defaults/preferences
%pretrans -p <lua>
require 'posix'
require 'os'
if (posix.stat("%{mozappdir}/browser/defaults/preferences", "type") == "link") then
  posix.unlink("%{mozappdir}/browser/defaults/preferences")
  posix.mkdir("%{mozappdir}/browser/defaults/preferences")
  if (posix.stat("%{mozappdir}/defaults/preferences", "type") == "directory") then
    for i,filename in pairs(posix.dir("%{mozappdir}/defaults/preferences")) do
      os.rename("%{mozappdir}/defaults/preferences/"..filename, "%{mozappdir}/browser/defaults/preferences/"..filename)
    end
    f = io.open("%{mozappdir}/defaults/preferences/README","w")
    if f then
      f:write("Content of this directory has been moved to %{mozappdir}/browser/defaults/preferences.")
      f:close()
    end
  end
end


%preun
# is it a final removal?
if [ $1 -eq 0 ]; then
  %{__rm} -rf %{mozappdir}/components
  %{__rm} -rf %{mozappdir}/extensions
  %{__rm} -rf %{mozappdir}/plugins  
fi

%post
update-desktop-database &> /dev/null || :
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%postun
update-desktop-database &> /dev/null || :
if [ $1 -eq 0 ] ; then
    touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%files
%defattr(-,root,root,-)
%{_bindir}/firefox-wayland
%dir %{_sysconfdir}/%{name}/*
%{_datadir}/appdata/*.appdata.xml
%{_datadir}/applications/*.desktop
%dir %{mozappdir}
# That's Windows only
%exclude %{mozappdir}/removed-files
%{mozappdir}/*
%{_datadir}/icons/hicolor/16x16/apps/firefox.png
%{_datadir}/icons/hicolor/22x22/apps/firefox.png
%{_datadir}/icons/hicolor/24x24/apps/firefox.png
%{_datadir}/icons/hicolor/256x256/apps/firefox.png
%{_datadir}/icons/hicolor/32x32/apps/firefox.png
%{_datadir}/icons/hicolor/48x48/apps/firefox.png

#---------------------------------------------------------------------

%changelog
* Fri Jul 14 2017 Martin Stransky <stransky@redhat.com> 56.2-1
- Fixed rhbz#1464916 - missing popup rendering (stransky@redhat.com)
- Tweaked wl_surface_damage() calls (stransky@redhat.com)
- Reverted commit 32899bf0d996dbe1008bd9abd79724a8217fb6b8 as it fixes nothing
  (stransky@redhat.com)
- Remove unrealize handler (rhbz#1467104) (stransky@redhat.com)
- Destroy GdkWindow owned by mozcontainer when unrealize (rhbz#1467104)
  (stransky@redhat.com)
- Map Wayland subsurface only when GdkWindow is already mapped
  (stransky@redhat.com)
- Set damage region for wl_surface after wl_buffer attach, rhbz#1464916
  (stransky@redhat.com)
- Added missing gtk_widget_input_shape_combine_region linkage (rhbz#1466377),
  thanks to Hiroshi Hatake (stransky@redhat.com)
- Fixed mouse transparency for popups (rhbz#1466377) (stransky@redhat.com)
- Fixed rendering of noautohide panels (rhbz#1466377) (stransky@redhat.com)

* Thu Jun 29 2017 Martin Stransky <stransky@redhat.com> 56.1-1
- Don't explicitly grab on Wayland (use only implicit grab), see mozbz#1377084
  for details (stransky@redhat.com)
- Removed the gdk_seat_* code - let's solve
  https://bugzilla.mozilla.org/show_bug.cgi?id=1377084 first
  (stransky@redhat.com)
- Specfile tweak - version up (stransky@redhat.com)
- Don't call gdk_x11_window_get_xid() from LOG() under wayland
  (stransky@redhat.com)

* Thu Jun 29 2017 Martin Stransky <stransky@redhat.com> 56.0-1
- Rpm version up to match Firefox version

* Thu Jun 29 2017 Martin Stransky <stransky@redhat.com> 55.11-1
- Removed double / to fix rpm build (stransky@redhat.com)
- Merged with upstream

* Fri Jun 23 2017 Martin Stransky <stransky@redhat.com> 55.10-1
- Fixed rhbz#1464017 - [Wayland] Hamburger menu popup and other panels does not
  render transparent background (stransky@redhat.com)
- Fixed error handling for posix_fallocate and formatting, by Hiroshi Hatake
  (stransky@redhat.com)


* Thu Jun 22 2017 Martin Stransky <stransky@redhat.com> 55.9-1
- Fixed shell launcher script (stransky@redhat.com)
- Updated to latest mozilla trunk

* Tue Jun 20 2017 Martin Stransky <stransky@redhat.com> 55.8-1
- Fixed typo in launch script (stransky@redhat.com)
- Updated firefox-wayland launch script (stransky@redhat.com)
- Added reference to https://bugzilla.gnome.org/show_bug.cgi?id=783957
  (stransky@redhat.com)
- Use subsurfaces for popup creation (rhbz#1457201) (stransky@redhat.com)
* Wed May 31 2017 Martin Stransky <stransky@redhat.com> 55.7-1
- Fixed fullscreen on Weston (stransky@redhat.com)
- Fixed clipboard crashes after browser start, rhbz#1455915
  (stransky@redhat.com)

* Tue May 30 2017 Martin Stransky <stransky@redhat.com> 55.6-1
- Updated to latest upstream
* Fri May 26 2017 Martin Stransky <stransky@redhat.com> 55.5-1
- Don't crash when we're missing clipboard data, rhbz#1455915
  (stransky@redhat.com)

* Thu May 25 2017 Martin Stransky <stransky@redhat.com> 55.4-1
- Fixed desktop file (stransky@redhat.com)

* Wed May 24 2017 Martin Stransky <stransky@redhat.com> 55.3-1
- Added tito build files
