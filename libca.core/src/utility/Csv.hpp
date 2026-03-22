#ifndef LIBCA_UTILITY_CSV_FILE_H
#define LIBCA_UTILITY_CSV_FILE_H

#include <vector>
#include <string>

namespace ca {

class CsvFile
{
public:
    CsvFile(std::string filename = "", bool containTitle = true);
    ~CsvFile() = default;

    // 加载文件并解析
    bool load();
    // 写数据到文件
    bool wrtie(std::string filename);
    // 添加标题行，如果已经有标题行则会替换原有的整个标题行
    void addTitle(std::vector<std::string> title);
    // 获取标题行
    std::vector<std::string>& getTitle();
    // 获取所有记录
    std::vector<std::vector<std::string>>& getAllRecords();
    // 获取一个记录，参数为第几行
    std::vector<std::string>& getRecord(int id);
    // 添加一行记录
    void addRecord(std::vector<std::string>& record);

private:
    std::string                           m_filename;         // csv文件名
    std::vector<std::string>              m_title;            // csv文件的标题行
    bool                                  m_isContainTitle;   // 是否包含标题行
    std::vector<std::vector<std::string>> m_records;          // 每一行的记录
};

}   // namespace ca


#endif   // !LIBCA_UTILITY_CSV_FILE_H
