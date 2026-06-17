# dev_plan — ca::str 字符串所有权地基重构

> 分支：`refactor/str-ownership`（libca 独立仓）
> 契约依据：morpher 仓 `spec/str-ownership-spec.md`（八条决策 A–H）
> 目标：把 `ca::str` 的类型族补齐到 spec 契约，地基夯实后 morpher 再 bump submodule。

## 现状审计（vs spec，已核实）

| spec 决策 | 现状 | 差距 |
|---|---|---|
| §6.1 PooledPtr 隐式转 Ref | 仅显式 `ref()` | 补隐式 `operator Utf8StringRef()` |
| §3 Utf8Twine | 不存在 | 新建 `utf8_twine.hpp/cpp` |
| §7.1 三者同一内容 hash | ✅ FNV-1a 一致（Ref/Pooled/String） | 无 |
| §7.1 异构查找 | 缺透明比较器 | `map<PooledPtr>` 不能用 Ref 查，需补 |
| §7.2 == 先指针后内容 | `PooledPtr==` 只比指针 | 补跨类型 `PooledPtr==Ref`（内容回退） |
| §10-D Pool 真删 | 墓碑（`alive=false`+删字节，节点留 list/桶） | 改真删（核心难点） |
| §10-C Ref 24 字节带码点 | ✅ 已符合 | 无 |
| §10-G intern 纯拷贝 | ✅ memcpy | 无 |

## 实现顺序（风险阶梯，每步 build+test）— ✅ 全部完成（234 tests）

### Step 1 — PooledPtr 隐式转 Ref（纯增，零风险）✅ `331a492`
- `utf8_string_pool.hpp`：加 `operator Utf8StringRef() const noexcept;`（复用现有 `ref()`）。
- 测试：`PooledPtr` 直接传进收 `Utf8StringRef` 的函数、参与 `equals`。

### Step 2 — Utf8Twine 懒拼接（纯增，独立）✅ `7fc9842`
- 新建 `utf8_twine.hpp/cpp`：持各片段视图，`operator+` 链式，`materialize(Arena&)→Ref` / `materialize(Pool&)→PooledPtr` / `to_string()→Utf8String`。
- 铁律落到注释：只作参数 / 单表达式即弃，绝不存成员、绝不跨语句。
- 测试：`pool.intern(a + "/" + b)` 内容正确；嵌套拼接；空片段。

### Step 3 — 异构比较器 + 跨类型相等（改造，中风险）✅ `994c564`
- 补 `PooledPtr==Ref` / `Ref==PooledPtr`：**先指针、不同指针回退内容比较**。
- ⚠️ 修正：`unordered_map` 异构查找是 C++20，本项目 C++17 不可用。改用 `Pool::find(Ref)→PooledPtr`（Pool 自身即内容索引，不分配/命中 refcount++）。
- 测试：`Pool::find` 命中/未命中/refcount；跨「两个池同内容」相等为真。

### Step 4 — Pool 真删（核心难点，重测试护体）✅ `4b0900c`
- 难点：`release()` 是 PooledPtr 成员，拿不到 Pool 的 `entries_`/`hash_index_`。
- 方案：`Utf8PoolEntry` 加 `Utf8StringPool* owner`（+8B/entry）；release 归零时回调 `owner->erase_entry(entry)`：
  1. 从 `hash_index_[hash]` 的 vector 摘除该 entry 指针；
  2. vector 空则 erase 该 hash key；
  3. 从 `entries_`(std::list，节点稳定) erase 节点。
- 去掉 `alive` 墓碑逻辑（真删后无墓碑）。
- 测试：intern→drop→`size()` 归零、`active_entries()` 准确；drop 后同内容再 intern 新建；多 PooledPtr 共享同 entry 的 refcount 正确；hash 桶不残留死指针。

### Step 5 — Pool 退出 fail-safe：disown（评审修复）✅ `bb2e4cb`

**触发**：PR#114 评审（@gemini-code-assist）指出 Step 4 的真删在「Pool 析构/clear/move-assign 时仍有 PooledPtr 存活」场景下 UAF。

**根因**：Step 4 的 `~Pool()` / `clear()` / move-assign 无条件 `delete[] ep->data; delete ep;`，但 entry 可能还被存活 PooledPtr 持有。之后该句柄 `release()` 走 `entry_->owner->erase_entry(entry_)` → 访问已释放内存（owner 也可能已析构，双重 UB）。而 spec §8 规定 `clear()` 是「不依赖 refcount 全部归零的兜底」——故该场景是设计预期路径上的崩溃，不是违约。

**方案（disown）**：不照搬评审字面建议（条目永远自管会废掉真删/去重收益），采用分层：
- `PooledPtr::release()` 双分支：`owner` 非空 → Pool 真删（正常路径零开销）；`owner` 为空 → 自管 `delete`。
- 新增 `Pool::disown_all()` 统一替换三处裸 delete 循环（析构/move-assign 释放自身旧条目/clear）：凡 `hash_index_` 残留的 entry 都仍有外部 PooledPtr 持有（refcount 归零的早已被 `erase_entry` 移出），一律置 `owner=nullptr`，把所有权移交给存活句柄自管释放。
- spec §5 的 outlive 由**硬契约降为软契约**：违反不崩溃（fail-safe 兜住），仅失去真删/去重收益。

**测试**：新增 5 条 UAF 路径（PoolDestructorBefore/ClearWhile/MoveAssignSource/DisownedPtrReleases/DisownDoesNotAffectIndependent）。原 229 + 5 = **234 全过**（release + debug 双模式）。实现过程中修正一处自误：disown 条件是「refcount > 0」而非「> 1」——Pool 自身不占引用计数，hash_index_ 残留即证明有外部句柄持有。

## 验证

```bash
cd /d/WorkSpace/libca
xmake f -p windows -a x64 -m release --with_tests=y -y
xmake build libca_str_unittest && xmake run libca_str_unittest   # 基线 211 tests，每步只增不减
```

## 注（不在本轮，记录给上层）

- §3.1 `from_static` 全局 mutex 是热点锁隐患——morpher reader 属性名比较若密集走它需量化，必要时换无锁/线程局部缓存。本轮不动，标记待评估。
