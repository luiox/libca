#ifndef LIBCA_UTILITY_BITS_H
#define LIBCA_UTILITY_BITS_H

#include <cstdint>

namespace libca::utility {
class BitsUitl
{
public:
    static int32_t lowbit(int32_t x);
};
}   // namespace libca::utility

#endif   // !LIBCA_UTILITY_BITS_H
