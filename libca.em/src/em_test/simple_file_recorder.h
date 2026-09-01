/*
 * @file simple_file_recorder.h
 * @brief 简单文件输出插件（使用TEST_PLUGIN_REGISTER自动注册）
 *
 * 使用方法：
 * 只需包含此头文件并链接对应的.c文件，插件会自动注册
 * 无需在main中手动调用任何初始化函数
 */

#ifndef SIMPLE_FILE_RECORDER_H
#define SIMPLE_FILE_RECORDER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 这个插件使用 TEST_PLUGIN_REGISTER 自动注册
 * 只需将 simple_file_recorder.c 添加到项目中即可
 *
 * 输出文件：test_report.txt
 * 格式：
 *   Test Suite Started: N tests
 *   ================================
 *   [RUN] test_name
 *   [PASS/FAIL] test_name
 *   ================================
 *   Results: X passed, Y failed
 */

#ifdef __cplusplus
}
#endif

#endif /* SIMPLE_FILE_RECORDER_H */
