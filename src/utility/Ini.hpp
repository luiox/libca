// ini文件解析
#ifndef LIBCA_UTILITY_INI_HPP
#define LIBCA_UTILITY_INI_HPP

#include <string>
#include <map>
#include <any>
#include "../base/Result.hpp"
#include "../base/Wrapper.hpp"

namespace ca {


class IniSection
{
    std::string                             name_;
    std::map<std::string, std::any>         items_;

public:
    IniSection() {}
    IniSection(const std::string& name)
        : name_(name)
    {}
    ~IniSection() {}
    inline std::string name() const { return name_; }
    inline void addItem(const std::string& key, const std::any& value) { items_[key] = value; }
    Result<Ref<std::any>, std::string>      findItem(const std::string& key);
    inline std::map<std::string, std::any>& items() { return items_; }
};

class IniFile
{
    std::string                       m_filename;
    std::map<std::string, IniSection> m_sections;

public:
    IniFile();
    IniFile(const std::string& filename);
    ~IniFile();

    bool load(const std::string& filename);
    void save(const std::string& filename);
    void show();
    void clear();

    Result<Ref<std::any>, std::string> get(const std::string& section, const std::string& key);
    void set(const std::string& section, const std::string& key, const std::any& value);

    bool has(const std::string& section);
    bool has(const std::string& section, const std::string& key);

    void remove(const std::string& section);
    void remove(const std::string& section, const std::string& key);

    std::string str();
};

}   // namespace ca

#endif   // LIBCA_UTILITY_INI_HPP
