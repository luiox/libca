#pragma once

/// @file xml.hpp
/// @brief libca_xml 聚合头，引入 XML 配置子集读写的全部组件。
/// @note 支持范围见 README：元素/属性/文本、注释（保留为节点）、CDATA、混合内容、
///       XML 声明、命名实体与数字字符引用。命名空间不特殊处理（prefix:local 整体为名字）。
///       DOCTYPE/DTD、自定义实体、非声明处理指令（PI）明确报错拒绝。

#include "libca/xml/parse_error.hpp"
#include "libca/xml/source_location.hpp"
#include "libca/xml/xml_document.hpp"
#include "libca/xml/xml_node.hpp"
#include "libca/xml/xml_reader.hpp"
