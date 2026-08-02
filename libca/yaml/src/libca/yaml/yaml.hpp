#pragma once

/// @file yaml.hpp
/// @brief libca_yaml 聚合头，引入 YAML 配置子集读写的全部组件。
/// @note 支持范围见 README：块式 mapping/sequence、YAML 1.2 core schema 标量、
///       单行 flow、块标量 |/>、注释。锚点/别名/标签/多文档明确报错拒绝。

#include "libca/yaml/source_location.hpp"
#include "libca/yaml/parse_error.hpp"
#include "libca/yaml/yaml_value.hpp"
#include "libca/yaml/yaml_document.hpp"
#include "libca/yaml/yaml_reader.hpp"
#include "libca/yaml/yaml_writer.hpp"
