/// @file ota_cfg.h
/// @brief em_ota 编译期配置开关（对齐 lwIP opt.h 惯例）。
///
/// 用户工程提供同名 `ota_cfg.h` 并置于更优先的包含路径即可覆盖全部默认值；
/// 未定义的宏取此处默认值。默认策略：核心恒开，扩展特性全关，
/// 关闭的特性按文件整体不参与链接，保证体积可预期。
#ifndef LIBCA_EM_OTA_OTA_CFG_H
#define LIBCA_EM_OTA_OTA_CFG_H

/// 镜像头格式与完整性校验（核心能力，建议恒开）
#ifndef OTA_CFG_ENABLE_IMAGE
#    define OTA_CFG_ENABLE_IMAGE 1
#endif

/// E1：断电安全激活标记（STAGED/ACTIVATED 两阶段记录），「不变砖」地基
#ifndef OTA_CFG_ENABLE_ACTIVATE
#    define OTA_CFG_ENABLE_ACTIVATE 0
#endif

/// E2：搬运器（暂存区 → app 区，带二次校验），单区+暂存拓扑闭环
#ifndef OTA_CFG_ENABLE_MOVER
#    define OTA_CFG_ENABLE_MOVER 0
#endif

/// E3：boot 决策模板（启动判定/确认/回滚的纯函数决策 + 注入式跳转）
#ifndef OTA_CFG_ENABLE_BOOT_TEMPLATE
#    define OTA_CFG_ENABLE_BOOT_TEMPLATE 0
#endif

/// E4：A/B 双区拓扑（与默认单区+暂存互为替代，编译期二选一）
#ifndef OTA_CFG_ENABLE_DUAL_SLOT
#    define OTA_CFG_ENABLE_DUAL_SLOT 0
#endif

/// E5：签名校验钩子（仅接口位，不含算法实现）
#ifndef OTA_CFG_ENABLE_SIGN_HOOK
#    define OTA_CFG_ENABLE_SIGN_HOOK 0
#endif

// ---------------------------------------------------------------------------
// 编译期依赖断言：防止错误的功能组合
// ---------------------------------------------------------------------------

#if OTA_CFG_ENABLE_MOVER && !OTA_CFG_ENABLE_IMAGE
#    error "OTA_CFG_ENABLE_MOVER 依赖 OTA_CFG_ENABLE_IMAGE：搬运前必须具备镜像校验能力"
#endif

#if OTA_CFG_ENABLE_BOOT_TEMPLATE && !(OTA_CFG_ENABLE_ACTIVATE || OTA_CFG_ENABLE_DUAL_SLOT)
#    error \
        "OTA_CFG_ENABLE_BOOT_TEMPLATE 需要 OTA_CFG_ENABLE_ACTIVATE 或 OTA_CFG_ENABLE_DUAL_SLOT：模板至少要有一种拓扑可决策"
#endif

#endif   // !LIBCA_EM_OTA_OTA_CFG_H
