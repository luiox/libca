#include "Ini.hpp"
#include "libca/base/BasicValue.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <string>

namespace ca {

Result<Ref<BasicValue>, std::string> IniSection::findItem(const std::string& key)
{
    auto it = items_.find(key);
    if (it != items_.end()) {
        return Ok(Ref(it->second));
    }
    return Err(std::string("Key not found: ") + key);
}

IniFile::IniFile() {}

IniFile::IniFile(const std::string& filename)
{
    load(filename);
}

IniFile::~IniFile() {}

std::string trim(std::string s)
{
    if (s.empty()) {
        return s;
    }
    s.erase(0, s.find_first_not_of(" \r\n"));
    s.erase(s.find_last_not_of(" \r\n") + 1);
    return s;
}

bool IniFile::load(const std::string& filename)
{
    m_sections.clear();
    m_filename = filename;

    std::string name;
    std::string line;

    std::ifstream fin(filename.c_str());
    if (fin.fail()) {
        printf("loading file failed: %s is not found.\n", m_filename.c_str());
        return false;
    }
    while (std::getline(fin, line)) {
        line = trim(line);
        if (line == "") {
            continue;
        }
        if (line[0] == '#')   // it's comment
        {
            continue;
        }
        if ('[' == line[0])   // it's section
        {
            int pos = line.find_first_of(']');
            if (pos < 0) {
                return false;
            }
            name             = trim(line.substr(1, pos - 1));
            m_sections[name] = IniSection();
        }
        else   // it's key = value
        {
            int pos = line.find_first_of('=');
            if (pos < 0) {
                return false;
            }
            std::string key               = trim(line.substr(0, pos));
            key                           = trim(key);
            std::string value             = trim(line.substr(pos + 1, line.size() - pos - 1));
            value                         = trim(value);
            m_sections[name].items()[key] = value;
        }
    }
    return true;
}

void IniFile::save(const std::string& filename)
{
    std::ofstream fout(filename.c_str());
    if (fout.fail()) {
        printf("opening file failed: %s.\n", m_filename.c_str());
        return;
    }
    fout << str();
    fout.close();
}

std::string IniFile::str()
{
    std::stringstream ss;
    for (auto it = m_sections.begin(); it != m_sections.end(); ++it) {
        ss << "[" << it->first << "]" << std::endl;
        for (auto iter = it->second.items().begin(); iter != it->second.items().end(); ++iter) {
            ss << iter->first << " = " << (std::string)iter->second.asString() << std::endl;
        }
        ss << std::endl;
    }
    return ss.str();
}

void IniFile::show()
{
    std::cout << str();
}

void IniFile::clear()
{
    m_sections.clear();
}

bool IniFile::has(const std::string& section)
{
    return (m_sections.find(section) != m_sections.end());
}

bool IniFile::has(const std::string& section, const std::string& key)
{
    auto it = m_sections.find(section);
    if (it != m_sections.end()) {
        return (it->second.items().find(key) != it->second.items().end());
    }
    return false;
}

Result<Ref<BasicValue>, std::string> IniFile::get(const std::string& section,
                                                  const std::string& key)
{
    // 先查找一下有没有这个section和key
    if (!has(section, key)) {
        return Err(std::string("key not found: "));
    }
    // 找得到就返回引用
    return Ok(Ref(m_sections[section].items()[key]));
}

void IniFile::set(const std::string& section, const std::string& key, const BasicValue& value)
{
    m_sections[section].items()[key] = value;
}

void IniFile::remove(const std::string& section)
{
    m_sections.erase(section);
}

void IniFile::remove(const std::string& section, const std::string& key)
{
    auto it = m_sections.find(section);
    if (it != m_sections.end()) {
        it->second.items().erase(key);
    }
}



}   // namespace ca