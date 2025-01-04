# 进入上一级目录
cd ..
# 获取上一级目录的路径
root_dir=$(pwd)
# 使用clang-format格式化代码
find "$root_dir" -type f \( -iname '*.h' -o -iname '*.c' -o -iname '*.hpp' -o -iname '*.cpp' \) -exec clang-format -i {} \;