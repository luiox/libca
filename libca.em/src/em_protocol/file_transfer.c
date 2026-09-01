#include "file_transfer.h"
#include "xmodem.h"
#include "ymodem.h"

// 引用各协议定义的全局虚函数表
extern const file_transfer_ops_t g_xmodem_ops;
extern const file_transfer_ops_t g_ymodem_ops;

void file_transfer_init(file_transfer_t* owner, transfer_protocol_enum proto, void* proto_ins)
{
    if (!owner)
        return;

    owner->proto     = proto;
    owner->proto_ins = proto_ins;

    switch (proto) {
    case TP_XMODEM: owner->ops = (file_transfer_ops_t*)&g_xmodem_ops; break;
    case TP_YMODEM: owner->ops = (file_transfer_ops_t*)&g_ymodem_ops; break;
    case TP_ZMODEM:
        // owner->ops = &g_zmodem_ops;
        break;
    default: owner->ops = NULL; break;
    }
}
