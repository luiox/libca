#include <libca/utility/bits_util.hpp>
namespace libca::utility {
int32_t BitsUitl::lowbit(int32_t x)
{
    return x & -x;
}

}   // namespace libca::utility