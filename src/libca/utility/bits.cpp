#include <libca/utility/bits.h>
namespace libca
{
    int32_t
    BitsUitl::lowbit(int32_t x)
    {
        return x & -x;
    }

} // namespace libca