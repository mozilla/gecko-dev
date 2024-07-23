dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_CLANG_PLUGIN], [

if test -n "$ENABLE_CLANG_PLUGIN"; then
    LLVM_CXXFLAGS=`$LLVM_CONFIG --cxxflags`

    LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags | tr '\n' ' '`

    if test "$HOST_OS_ARCH" = WINNT ; then
        dnl Replace any backslashes from paths with forward slash.
        LLVM_CXXFLAGS=$(echo "$LLVM_CXXFLAGS" | tr '\\' '/')
        LLVM_LDFLAGS=$(echo "$LLVM_LDFLAGS" | tr '\\' '/')
    fi

    CLANG_PLUGIN_FLAGS="-Xclang -load -Xclang $CLANG_PLUGIN -Xclang -add-plugin -Xclang moz-check"

    AC_DEFINE(MOZ_CLANG_PLUGIN)
fi

if test -n "$ENABLE_MOZSEARCH_PLUGIN"; then
    if test -z "${ENABLE_CLANG_PLUGIN}"; then
        AC_MSG_ERROR([Can't use mozsearch plugin without --enable-clang-plugin.])
    fi

    CLANG_PLUGIN_FLAGS="$CLANG_PLUGIN_FLAGS -Xclang -add-plugin -Xclang mozsearch-index"

    dnl Parameters are: srcdir, outdir (path where output JSON is stored), objdir.
    CLANG_PLUGIN_FLAGS="$CLANG_PLUGIN_FLAGS -Xclang -plugin-arg-mozsearch-index -Xclang $_topsrcdir"
    CLANG_PLUGIN_FLAGS="$CLANG_PLUGIN_FLAGS -Xclang -plugin-arg-mozsearch-index -Xclang $_objdir/mozsearch_index"
    CLANG_PLUGIN_FLAGS="$CLANG_PLUGIN_FLAGS -Xclang -plugin-arg-mozsearch-index -Xclang $_objdir"

    AC_DEFINE(MOZ_MOZSEARCH_PLUGIN)
fi

AC_SUBST_LIST(CLANG_PLUGIN_FLAGS)
AC_SUBST_LIST(LLVM_CXXFLAGS)
AC_SUBST_LIST(LLVM_LDFLAGS)

AC_SUBST(ENABLE_CLANG_PLUGIN)
AC_SUBST(ENABLE_CLANG_PLUGIN_ALPHA)
AC_SUBST(ENABLE_MOZSEARCH_PLUGIN)

])
