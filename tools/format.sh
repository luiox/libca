#!/usr/bin/env bash
# 一次性格式化仓库源码（clang-format）。
# 范围：libca / libca.em / tests / tools；不触碰 third_party 与构建产物。
# 手动：bash tools/format.sh ；周期任务见 .github/workflows/format.yml。
set -euo pipefail
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
find "$root_dir/libca" "$root_dir/libca.em" "$root_dir/tests" "$root_dir/tools" \
    -type f \( -iname '*.h' -o -iname '*.c' -o -iname '*.hpp' -o -iname '*.cpp' \) \
    -print0 | xargs -0 clang-format -i
echo "formatted: libca libca.em tests tools"
