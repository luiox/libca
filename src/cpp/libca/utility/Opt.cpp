#include "Opt.hpp"

namespace ca {
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
}   // namespace ca

/*

基本选项和参数：
单个短选项：-a
多个短选项组合：-abc
单个长选项：--option
选项和参数值用空格分隔：-o value 或 --option value
选项和参数值用等号连接：--option=value
包含空格的情况：
参数值包含空格，用引号包围：--message "Hello World"
参数值包含引号：--quote "\"Quoted\""
参数值包含转义字符：--path "C:\\Program Files"
特殊字符：
参数值包含特殊字符：--special @#$%^&*()_+
参数值包含控制字符：--control \n\t\r
互斥和依赖选项：
同时使用互斥选项：--backup --restore
依赖选项未提供所需参数：--output
子命令和参数：
使用子命令：command subcommand --arg value
子命令后跟多个参数：command subcommand arg1 arg2 arg3
类型转换：
整数参数：--number 42
浮点数参数：--pi 3.14159
布尔值参数：--flag true 或 --flag false
错误和异常情况：
未知选项：--unknown
缺少必需的参数：--output
参数格式错误：--number forty-two
参数类型错误：--flag 123
参数顺序和位置：
位置参数：command arg1 arg2
选项和位置参数混合：command --option value arg1 arg2
帮助和版本信息：
请求帮助信息：--help 或 -h
请求版本信息：--version 或 -v

# 正常情况
./app -a
./app --option value
./app -o=value
./app --message "Hello World"
./app --path "C:\\Program Files"

# 复杂情况
./app -abc
./app --option="A \"quoted\" value"
./app --special @#$%^&*()_+
./app --control \n\t\r

# 错误情况
./app --unknown
./app --output
./app --number forty-two
./app --flag 123

# 子命令和参数
./app subcommand --arg value
./app subcommand arg1 arg2 arg3

# 帮助和版本
./app --help
./app -h
./app --version
./app -v


 */

#ifdef TEST_ENABLE

#    include "libca/test/Test.hpp"

using namespace std;
using namespace ca;

TEST_CASE(OptTest)
{

    string cmd1 = "./app -a";
    string cmd2 = "./app --option value";
    string cmd3 = "./app -o=value";
    string cmd4 = "./app --message \"Hello World\"";
    string cmd5 = "./app --special @#$%^&*()_+";
    string cmd6 = "./app --control \n\t\r";
    string cmd7 = "./app subcommand --arg value";
    string cmd8 = "./app subcommand1 subcommand2 subcommand3 --arg value";

    {
        CommandLineParser parser;
        parser.addOption(CommandLineParser::OPT_MUST, "a", "option", "description", [](string str) {
            cout << "Option 'a' was set" << endl;
        });
    }

    {
        CommandLineParser parser;
        parser.addOption(
            CommandLineParser::OPT_OPTIONAL, "o", "option", "description", [](string str) {
                if (str == "value") {
                    cout << "str is value" << endl;
                }
            });
        parser.parse(cmd3);
    }

    {
        // CommandLineParser parser;
        // parser.addSubCommand(std::string name, std::string description, std::function<void
        // (std::string)> handler); cmd8
    }
}

#endif
