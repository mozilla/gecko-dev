#!/bin/bash -vex

# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

WORKSPACE=$1

### Check that require variables are defined
test -d $WORKSPACE
test $GECKO_HEAD_REPOSITORY # Should be an hg repository url to pull from
test $GECKO_BASE_REPOSITORY # Should be an hg repository url to clone from
test $GECKO_HEAD_REV # Should be an hg revision to pull down
test $MOZHARNESS_REPOSITORY # mozharness repository
test $MOZHARNESS_REV # mozharness revision
test $MOZHARNESS_REF # mozharness ref
test $TARGET
test $VARIANT

. ../builder/setup-ccache.sh

tc-vcs checkout mozharness $MOZHARNESS_REPOSITORY $MOZHARNESS_REPOSITORY $MOZHARNESS_REV $MOZHARNESS_REF

# Figure out where the remote manifest is so we can use caches for it.
MANIFEST=$(repository-url.py $GECKO_HEAD_REPOSITORY $GECKO_HEAD_REV b2g/config/$TARGET/sources.xml)
tc-vcs repo-checkout $WORKSPACE/B2G https://git.mozilla.org/b2g/B2G.git $MANIFEST

# Ensure symlink has been created to gecko...
rm -f $WORKSPACE/B2G/gecko
ln -s $WORKSPACE/gecko $WORKSPACE/B2G/gecko

debug_flag=""
if [ 0$B2G_DEBUG -ne 0 ]; then
  debug_flag='--debug'
fi

backup_file=$(aws --output=text s3 ls s3://b2g-phone-backups/$TARGET/ | tail -1 | awk '{print $NF}')

if echo $backup_file | grep '\.tar\.bz2'; then
    aws s3 cp s3://b2g-phone-backups/$TARGET/$backup_file .
    tar -xjf $backup_file -C $WORKSPACE/B2G
    rm -f $backup_file
fi

