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

#include <doctest/doctest.h>
#include <iostream>
#include <libca/utility/opt.hpp>

using namespace std;
using namespace libca::utility;

TEST_CASE("libca::utility::OPT")
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
        parser.addOption(CommandLineParser::OPT_MUST, "a", "option", "description", [](string str){
            cout << "Option 'a' was set" << endl;
        });
    }

    {
        CommandLineParser parser;
        parser.addOption(CommandLineParser::OPT_OPTIONAL, "o", "option", "description", [](string str){
            if(str =="value"){
                cout << "str is value" << endl;
            }
        });
        parser.parse(cmd3);
    }

    {
        // CommandLineParser parser;
        // parser.addSubCommand(std::string name, std::string description, std::function<void (std::string)> handler);
        // cmd8
    }
    
    
}