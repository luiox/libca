/// @file csv.hpp
/// @brief libca/csv 聚合头文件，包含数据模型、Reader 与 Writer。
///        用 `#include <libca/csv/csv.hpp>` 一次引入整个模块。
/// @details 数据模型 CsvDocument 采用 Arena 架构：字段经 `intern_raw` 入池（不校验 UTF-8，
///          按原始字节保留——CSV 不规定编码，字段可能含任意字节）。

#pragma once

#include "libca/csv/source_location.hpp"
#include "libca/csv/parse_error.hpp"
#include "libca/csv/csv_document.hpp"
#include "libca/csv/csv_reader.hpp"
#include "libca/csv/csv_writer.hpp"
