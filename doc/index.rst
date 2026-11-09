libca 设计规格
==============

状态：机制先行，存量转写完成。跨模块设计规格层已就位（Sphinx ``-W`` 门禁 +
``check_refs`` 引用门禁 + Doxygen 微观层），模块级存量设计 md 已全部转写落位。

三层按**权威来源**分工，扩展名即角色（规范全文见 ``AGENTS.md`` 的「文档规范」
一节）：代码 ``///`` Doxygen 注释是微观契约（单符号参数/返回/失败模式与单模块
背景定位，``@defgroup`` 组注承载）；``doc/*.rst``（本目录）是宏观规格（跨模块
设计、模块思想、类型组织与重要取舍），含各模块设计页（``<mod>-design``，
21 篇，自模块级存量 md 转写集中落位）；markdown 只承载流程文档（README、
AGENTS、路线图、方案规划）。

代码注释以 ``doc:<label>`` 形式指向本目录的具体小节；``check_refs.py`` 校验
这些指向不会腐烂。RST 里「``X`` 头注」指向微观层的 ``@defgroup`` 组名或头
文件名。

.. toctree::
   :maxdepth: 2
   :caption: 设计与契约

   collection-design
   config-design
   core-design
   crypto-design
   csv-design
   fs-design
   http-design
   ini-design
   io-design
   json-design
   log-design
   net-design
   opt-design
   process-design
   random-design
   str-design
   thread-design
   time-design
   toml-design
   xml-design
   yaml-design
