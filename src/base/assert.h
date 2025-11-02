#ifndef ASSETS_H
#define ASSETS_H

#include "compiler_detect.h"


// #define C_ASSERT_STATIC(condition) \
//     typedef char c_assert_##__LINE__[(condition) ? 1 : -1]

// #define C_ASSERT_STATIC(condition) \
//     struct c_assert_##__LINE__ { unsigned int : (condition) ? 1 : 0; }

#ifdef __cplusplus

#    ifdef NDEBUG

#        define ca_assert(expr) static_cast<void>(0)

#    else

#        define ca_assert(expr) static_cast<void>(           \
    (expr) ? 0 :                     \
                 (                    \
                     ::ca::assertion_failed( \
                         #expr, __FILE__, __LINE__), \
                     0)

#    endif   // NDEBUG


#endif   // __cplusplus

#ifndef NDEBUG

#    define ca_assert(expr) ((void)0)
#else

#    define ca_assert(expr) ((void)0)

#endif   // NDEBUG



#endif   // !ASSETS_H
