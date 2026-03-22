#ifndef LIBCA_BASE_DLL_EXPORT_HPP
#define LIBCA_BASE_DLL_EXPORT_HPP

#ifdef LIBCA_DLL_MODE
#    ifdef LIBCA_DLL_EXPORT
#        define LIBCA_API __declspec(dllexport)
#    else
#        define LIBCA_API __declspec(dllimport)
#    endif
#else
#    define LIBCA_API
#endif

#endif // !LIBCA_BASE_DLL_EXPORT_HPP
