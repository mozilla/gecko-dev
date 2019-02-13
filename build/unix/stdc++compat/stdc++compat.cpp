/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <istream>
#include <string>
#include <stdarg.h>
#include <stdio.h>
#include <mozilla/Assertions.h>

/* GLIBCXX_3.4.8  is from gcc 4.1.1 (111691)
   GLIBCXX_3.4.9  is from gcc 4.2.0 (111690)
   GLIBCXX_3.4.10 is from gcc 4.3.0 (126287)
   GLIBCXX_3.4.11 is from gcc 4.4.0 (133006)
   GLIBCXX_3.4.12 is from gcc 4.4.1 (147138)
   GLIBCXX_3.4.13 is from gcc 4.4.2 (151127)
   GLIBCXX_3.4.14 is from gcc 4.5.0 (151126)
   GLIBCXX_3.4.15 is from gcc 4.6.0 (160071)
   GLIBCXX_3.4.16 is from gcc 4.6.1 (172240)
   GLIBCXX_3.4.17 is from gcc 4.7.0 (174383)
   GLIBCXX_3.4.18 is from gcc 4.8.0 (190787)
   GLIBCXX_3.4.19 is from gcc 4.8.1 (199309)
   GLIBCXX_3.4.20 is from gcc 4.9.0 (199307)
   GLIBCXX_3.4.21 is from gcc 5.0 (210290)

This file adds the necessary compatibility tricks to avoid symbols with
version GLIBCXX_3.4.11 and bigger, keeping binary compatibility with
libstdc++ 4.3.

*/

#define GLIBCXX_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))

namespace std {
#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 14)
    /* Instantiate these templates to avoid GLIBCXX_3.4.14 symbol versions
     * depending on optimization level */
    template char *string::_S_construct_aux_2(size_type, char, allocator<char> const&);
#ifdef _GLIBCXX_USE_WCHAR_T
    template wchar_t *wstring::_S_construct_aux_2(size_type, wchar_t, allocator<wchar_t> const&);
#endif /* _GLIBCXX_USE_WCHAR_T */
#ifdef __GXX_EXPERIMENTAL_CXX0X__
    template string::basic_string(string&&);
    template string& string::operator=(string&&);
    template wstring::basic_string(wstring&&);
    template wstring& wstring::operator=(wstring&&);
    template string& string::assign(string&&);
    template wstring& wstring::assign(wstring&&);
#endif /* __GXX_EXPERIMENTAL_CXX0X__ */
#endif /* (__GNUC__ == 4) && (__GNUC_MINOR__ >= 5) */
#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 16)
    /* Instantiate these templates to avoid GLIBCXX_3.4.16 symbol versions
     * depending on compiler optimizations */
    template int string::_S_compare(size_type, size_type);
#endif
}

namespace std MOZ_EXPORT {
#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 14)
    /* Hack to avoid GLIBCXX_3.4.14 symbol versions */
    struct _List_node_base
    {
        void hook(_List_node_base * const __position) throw ();

        void unhook() throw ();

        void transfer(_List_node_base * const __first,
                      _List_node_base * const __last) throw();

        void reverse() throw();

/* Hack to avoid GLIBCXX_3.4.15 symbol versions */
#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 15)
        static void swap(_List_node_base& __x, _List_node_base& __y) throw ();
    };

    namespace __detail {

    struct _List_node_base
    {
#endif
        void _M_hook(_List_node_base * const __position) throw ();

        void _M_unhook() throw ();

        void _M_transfer(_List_node_base * const __first,
                         _List_node_base * const __last) throw();

        void _M_reverse() throw();

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 15)
        static void swap(_List_node_base& __x, _List_node_base& __y) throw ();
#endif
    };

    /* The functions actually have the same implementation */
    void
    _List_node_base::_M_hook(_List_node_base * const __position) throw ()
    {
        ((std::_List_node_base *)this)->hook((std::_List_node_base * const) __position);
    }

    void
    _List_node_base::_M_unhook() throw ()
    {
        ((std::_List_node_base *)this)->unhook();
    }

    void
    _List_node_base::_M_transfer(_List_node_base * const __first,
                                 _List_node_base * const __last) throw ()
    {
        ((std::_List_node_base *)this)->transfer((std::_List_node_base * const)__first,
                                                 (std::_List_node_base * const)__last);
    }

    void
    _List_node_base::_M_reverse() throw ()
    {
        ((std::_List_node_base *)this)->reverse();
    }

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 15)
    void
    _List_node_base::swap(_List_node_base& __x, _List_node_base& __y) throw ()
    {
        std::_List_node_base::swap(*((std::_List_node_base *) &__x),
                                   *((std::_List_node_base *) &__y));
    }
}
#endif

#endif /*MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 14)*/

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 11)
    /* Hack to avoid GLIBCXX_3.4.11 symbol versions
       An inline definition of ctype<char>::_M_widen_init() used to be in
       locale_facets.h before GCC 4.4, but moved out of headers in more
       recent versions.
       It is actually safe to make it do nothing. */
    void ctype<char>::_M_widen_init() const {}
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 20)
    /* We shouldn't be throwing exceptions at all, but it sadly turns out
       we call STL (inline) functions that do. */
    void __throw_out_of_range_fmt(char const* fmt, ...)
    {
        va_list ap;
        char buf[1024]; // That should be big enough.

        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        buf[sizeof(buf) - 1] = 0;
        va_end(ap);

        __throw_range_error(buf);
    }
#endif

}

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 20)
/* Technically, this symbol is not in GLIBCXX_3.4.20, but in CXXABI_1.3.8,
   but that's equivalent, version-wise. Those calls are added by the compiler
   itself on `new Class[n]` calls. */
extern "C" void
__cxa_throw_bad_array_new_length()
{
    MOZ_CRASH();
}
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 21)
/* While we generally don't build with exceptions, we have some host tools
 * that do use them. libstdc++ from GCC 5.0 added exception constructors with
 * char const* argument. Older versions only have a constructor with
 * std::string. */
namespace std {
    runtime_error::runtime_error(char const* s)
    : runtime_error(std::string(s))
    {
    }
}
#endif
