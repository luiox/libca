#include "Opt.hpp"

namespace libca::utility {
CommandLineParser::CommandLineParser() {}
CommandLineParser::~CommandLineParser() {}

void CommandLineParser::addOption(OptionType type, std::string shortName, std::string longName,
                                  std::string description, std::function<void(std::string)> handler)
{
    m_options.push_back({type, shortName, longName, description, handler});
}
void CommandLineParser::addSubCommand(std::string name, std::string description,
                                      std::function<void(std::string)> handler)
{
    m_subCommands.push_back({name, description, handler});
}
void CommandLineParser::parse(std::string cmd)
{
    // 查找出第一个空格
    auto spacePos = cmd.find(' ');
    if (spacePos == std::string::npos) {
        return;
    }
    // 命令名
    // auto cmdName = cmd.substr(0, spacePos);

    // 开始解析后面的内容
    if (std::isalpha(cmd.at(spacePos + 1))) {
        // 子命令
        // 把子命令名和后面的内容分开
        auto subCmdPos = cmd.find(' ', spacePos + 1);
        if (subCmdPos == std::string::npos) {
            return;
        }

        auto subCmdName = cmd.substr(spacePos + 1, subCmdPos - spacePos - 1);

        for (auto& subCommand : m_subCommands) {
            if (subCommand.name == subCmdName) {
                subCommand.handler(cmd.substr(subCmdPos + 1));
            }
        }
    }
    else {
        // 解析参数
        // 先判断是长名字选项还是短名字选项
        if (cmd.at(spacePos + 2) == '-') {
            // 长名字选项
            // 查找空格
            auto spacePos2  = cmd.find(' ', spacePos + 2);
            auto optionName = cmd.substr(spacePos + 2, spacePos2);
            for (auto& option : m_options) {
                if (option.longName == optionName) {
                    // 把剩下的内容传给handler
                    option.handler(cmd.substr(spacePos2 + 1));
                }
            }
        }
        else {
            // 短名字选项
        }
    }
}
}   // namespace libca::utility
