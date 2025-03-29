#include "Csv.hpp"
#include "libca/base/String.hpp"
#include <fstream>
#include <sstream>


namespace ca {

CsvFile::CsvFile(std::string filename, bool containTitle)
    : m_filename(filename)
    , m_isContainTitle(containTitle)
{}

// 加载文件并解析
bool CsvFile::load()
{
    std::ifstream ifs(m_filename);
    if (!ifs.is_open()) {
        return false;
    }

    std::string line;
    if (m_isContainTitle) {
        // 把第一行当做是标题行处理
        if (getline(ifs, line)) {
            // 读取第一行以后，按照,进行分割
            std::istringstream iss(line);
            std::string        item;
            // 使用getline方法逐个提取字符串
            while (std::getline(iss, item, ',')) {
                m_title.push_back(StringUtil::trim(item));
            }
        }
    }

    while (getline(ifs, line)) {
        // 读取一行以后，按照,进行分割
        std::istringstream       iss(line);
        std::string              item;
        std::vector<std::string> record;
        // 使用getline方法逐个提取字符串
        while (std::getline(iss, item, ',')) {
            record.push_back(StringUtil::trim(item));
        }
        // 添加一行记录到总记录集中
        m_records.push_back(record);
    }

    return true;
}

// 写数据到文件
bool CsvFile::wrtie(std::string filename)
{
    // 打开文件流
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        return false;
    }

    // 写入标题行
    if (!m_title.empty()) {
        ofs << m_title[0];
        for (int i = 1; i < m_title.size(); i++) {
            ofs << ", " << m_title[i];
        }
        ofs << "\r\n";
    }

    // 写入记录
    if (!m_records.empty()) {
        for (const auto record : m_records) {
            ofs << record[0];
            for (int i = 1; i < record.size(); i++) {
                ofs << ", " << record[i];
            }
            ofs << "\r\n";
        }
    }

    return true;
}

// 添加标题行，如果已经有标题行则会替换原有的整个标题行
void CsvFile::addTitle(std::vector<std::string> title)
{
    m_title = title;
}

// 获取标题行
std::vector<std::string>& CsvFile::getTitle()
{
    return m_title;
}

// 获取所有记录
std::vector<std::vector<std::string>>& CsvFile::getAllRecords()
{
    return m_records;
}

// 获取一个记录，参数为第几行
std::vector<std::string>& CsvFile::getRecord(int id)
{
    if (id < 0 || id >= m_records.size())
        return m_records[0];
    return m_records[id];
}

// 添加一行记录
void CsvFile::addRecord(std::vector<std::string>& record)
{
    m_records.push_back(record);
}

}   // namespace ca
