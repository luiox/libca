/*
 * @file simple_file_recorder.h
 * @brief 简单文件输出插件
 * 
 * 将测试输出同时记录到文件
 * 用法：
 *   test_file_recorder_init("report.txt", 0);  // 覆盖模式
 *   test_run();
 *   test_file_recorder_close();
 */

#ifndef SIMPLE_FILE_RECORDER_H
#define SIMPLE_FILE_RECORDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/*
 * @brief 初始化文件记录器
 * @param filepath 输出文件路径
 * @param append 0=覆盖模式, 1=追加模式
 * @return 0=成功, -1=失败
 */
int test_file_recorder_init(const char* filepath, int append);

/*
 * @brief 关闭文件记录器
 */
void test_file_recorder_close(void);

/*
 * @brief 获取文件指针（高级用法）
 * @return 文件指针，未初始化时返回 NULL
 */
FILE* test_file_recorder_get_fp(void);

#ifdef __cplusplus
}
#endif

#endif /* SIMPLE_FILE_RECORDER_H */
