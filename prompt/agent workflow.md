第一步：接口设计 (Interface Design)
不要急着生成 .c 实现。先让 AI 设计 .h 头文件。

用户输入: "基于 PROMPTS.md 的规则，请帮我设计 protocol_parser 模块的头文件接口。需要支持解析变长数据包。"

第二步：实现与验证 (Implementation & Verification)
确认接口无误后，使用完整模板生成实现代码。

用户输入: (引用 PROMPTS.md 的内容)
Task: "请实现 protocol_parser.c。请注意处理边界情况，并在文件底部编写至少 3 个测试用例（正常解析、包头错误、长度溢出）。"

第三步：代码审查 (Review)
检查生成的代码是否包含以下特征：

是否包含了 datatype.h？
是否使用了 int 或 char （如果有，打回重写）。
文件底部是否有 #if TEST_ENABLE 包裹的 TEST_CASE。
