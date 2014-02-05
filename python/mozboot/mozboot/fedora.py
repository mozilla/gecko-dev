# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozboot.base import BaseBootstrapper

class FedoraBootstrapper(BaseBootstrapper):
    def __init__(self, version, dist_id):
        BaseBootstrapper.__init__(self)

        self.version = version
        self.dist_id = dist_id

        self.group_packages = [
            'Development Tools',
            'Development Libraries',
            'GNOME Software Development',
        ]

        self.packages = [
            'alsa-lib-devel',
            'autoconf213',
            'gcc-c++',
            'glibc-static',
            'gstreamer-devel',
            'gstreamer-plugins-base-devel',
            'libstdc++-static',
            'libXt-devel',
            'mercurial',
            'mesa-libGL-devel',
            'pulseaudio-libs-devel',
            'wireless-tools-devel',
            'yasm',
        ]

    def install_system_packages(self):
        self.yum_groupinstall(*self.group_packages)
        self.yum_install(*self.packages)

    def upgrade_mercurial(self, current):
        self.yum_update('mercurial')
