This branch contains modifications to the mozilla:release branch for compiling
the Web Replay browser.

Sample mozconfig for macOS:

BEGIN

. $topsrcdir/browser/config/mozconfig

mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/wr-opt
mk_add_options MOZ_MAKE_FLAGS="-j12"
mk_add_options AUTOCLOBBER=1

ac_add_options --with-macos-sdk=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk
ac_add_options --with-branding=browser/branding/webreplay
ac_add_options --enable-replace-malloc

END
