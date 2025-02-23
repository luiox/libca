// 命令行解析
#ifndef LIBCA_UTILITY_OPT_H
#define LIBCA_UTILITY_OPT_H

#include <string>
#include <functional>
#include <vector>

namespace libca::utility {

class Command
{
public:
    Command(std::string name, std::function<void()> func);
    std::string           name;
    std::function<void()> func;
};

class CommandLineParser
{
public:
    enum OptionType
    {
        OPT_MUST,
        OPT_OPTIONAL,
    };

    CommandLineParser();
    ~CommandLineParser();

    void addOption(OptionType type, std::string shortName, std::string longName,
                   std::string description, std::function<void(std::string)> handler);
    void addSubCommand(std::string name, std::string description,
                       std::function<void(std::string)> handler);
    void parse(std::string cmd);

private:
    struct Option
    {
        OptionType                       type;          // 选项类型
        std::string                      shortName;     // 短选项名
        std::string                      longName;      // 长选项名
        std::string                      description;   // 选项描述
        std::function<void(std::string)> handler;       // 选项处理函数
    };
    struct SubCommand
    {
        std::string                      name;          // 子命令名
        std::string                      description;   // 子命令描述
        std::function<void(std::string)> handler;       // 子命令处理函数
    };
    std::vector<Option>     m_options;       // 选项列表
    std::vector<SubCommand> m_subCommands;   // 子命令列表
};

}   // namespace libca::utility

#endif   // !LIBCA_UTILITY_OPT_H
