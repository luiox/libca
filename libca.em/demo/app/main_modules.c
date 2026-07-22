#include <em_component/skv.h>
#include <em_util/endian_util.h>

i32 main(void)
{
    bool little_endian = is_little_endian();
    (void)little_endian;
    return 0;
}
