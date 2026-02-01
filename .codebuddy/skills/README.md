# libca Skills

本目录包含用于开发 libca 嵌入式组件和驱动的自定义技能。

## 可用技能

### 1. em-driver-dev

**用途**: 为嵌入式 MCU 系统开发 em_driver 硬件驱动

**使用场景**:
- 创建新的传感器或外设驱动
- 实现硬件抽象层
- 遵循 OOP 风格驱动模式

**触发示例**:
- "开发 DS1302 RTC 驱动"
- "实现 MQ-2 气体传感器驱动"
- "编写 W25QXX Flash 存储驱动"

**结构**:
```
em-driver-dev/
├── SKILL.md                                    # 快速开始指南
└── references/
    ├── 01_code_rule.md                        # 代码规范摘要（包括单元测试策略）
    ├── 02_design_principle.md                 # 设计原则与最佳实践
    ├── 03_develop_workflow.md                 # 驱动开发流程（从硬件分析到测试）
    └── legacy/                                # 原始详细文档备份（示例与完整规范）
```

**核心特性**:
- OOP 风格驱动模式，带 port 层抽象
- 标准化 API 设计（`xxx_init`, `xxx_bind_port` 等）
- 通过函数指针实现硬件独立性
- 无需单元测试（在真实硬件或模拟环境中测试）

**快速参考**:
```c
// Port 绑定
void xxx_bind_port(const xxx_port_t* port);

// API 模式
void xxx_init(xxx_t* self, /* params */);
i32  xxx_read(xxx_t* self, u8* data);

// 错误处理（驱动内所有参数检查都用 param_check）
param_check(self != NULL);
param_check(data != NULL);
param_check(g_xxx_port != NULL);
```

---

### 2. em-component-dev

**用途**: 开发工具组件（算法、数据结构、工具）

**使用场景**:
- 创建可复用的工具函数
- 实现算法或数据结构
- 编写需要单元测试的代码

**触发示例**:
- "实现滑动平均滤波器"
- "开发优先级队列"
- "编写 CRC 校验算法模块"

**结构**:
```
em-component-dev/
├── SKILL.md                                    # 快速开始指南
└── references/
    ├── 02_单元测试规范.md                       # 中文测试规范
    └── component_examples.md                   # 组件示例（RingBuffer、CRC、PID）
```

**核心特性**:
- 硬件无关设计
- 完整单元测试覆盖
- 原子测试原则
- xmake test 集成

**快速参考**:
```c
// 参数检查
param_check(self != NULL);
param_check(data != NULL);

// 测试
TEST_CASE(xxx_init) {
    // Arrange
    xxx_t obj;

    // Act
    xxx_init(&obj);

    // Assert
    TEST_ASSERT_EQUAL_INT(1, obj.initialized);
}
```

---

## 对比

| 方面 | em-driver-dev | em-component-dev |
|------|---------------|------------------|
| **用途** | 硬件驱动 | 工具组件 |
| **依赖** | 硬件相关 | 硬件无关 |
| **测试** | 模拟/真实硬件 | 单元测试 |
| **位置** | `src/em_driver/` | `src/em_util/` |
| **OOP 风格** | 必需 (xxx_t* self) | 可选 |
| **Port 层** | 有（硬件抽象） | 无 |
| **示例** | LED、BH1750、DHT11 | RingBuffer、CRC、PID |

## 使用方法

AI 会在你提出相关请求时自动加载这些技能，无需手动配置。

### 测试技能加载

尝试以下请求：

**测试 em-driver-dev**:
```
"开发 DS18B20 温度传感器驱动"
```

**测试 em-component-dev**:
```
"实现加权移动平均滤波器"
```

## 文档

- **em-driver-dev**: 参见 `em-driver-dev/SKILL.md` 快速开始
- **em-component-dev**: 参见 `em-component-dev/SKILL.md` 快速开始

## 贡献

创建新技能或更新现有技能时：

1. 保持 SKILL.md 简洁（仅快速参考）
2. 详细内容放在 `references/` 目录
3. 参考文档按顺序编号
4. 使用描述性文件名
5. 在参考文档中提供代码示例
