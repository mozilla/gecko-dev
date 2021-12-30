
#ifndef CUBEB_EXPORT_H
#define CUBEB_EXPORT_H

#ifdef CUBEB_STATIC_DEFINE
#  define CUBEB_EXPORT
#  define CUBEB_NO_EXPORT
#else
#  ifndef CUBEB_EXPORT
#    ifdef cubeb_EXPORTS
        /* We are building this library */
#      define CUBEB_EXPORT 
#    else
        /* We are using this library */
#      define CUBEB_EXPORT 
#    endif
#  endif

#  ifndef CUBEB_NO_EXPORT
#    define CUBEB_NO_EXPORT 
#  endif
#endif

#ifndef CUBEB_DEPRECATED
#  define CUBEB_DEPRECATED __declspec(deprecated)
#endif

#ifndef CUBEB_DEPRECATED_EXPORT
#  define CUBEB_DEPRECATED_EXPORT CUBEB_EXPORT CUBEB_DEPRECATED
#endif

#ifndef CUBEB_DEPRECATED_NO_EXPORT
#  define CUBEB_DEPRECATED_NO_EXPORT CUBEB_NO_EXPORT CUBEB_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef CUBEB_NO_DEPRECATED
#    define CUBEB_NO_DEPRECATED
#  endif
#endif

#endif /* CUBEB_EXPORT_H */
