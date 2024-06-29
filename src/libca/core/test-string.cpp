/**
 * @brief string test
 * @author Canrad
 * @date 2024/04/19
 *
 */

#include <doctest/doctest.h>
#include <iostream>
#include <libca/core/string.hpp>

TEST_CASE("test string")
{
    using std::cout;
    using std::endl;

    libca::string str1("12三四");
    libca::string str2('a');
    libca::string str3(1234);
    libca::string str4{ 1234 };
    libca::string str5{ 'a' };
    libca::string str6 = "1234";
    cout << "str1.c_str = " << str1.c_str() << endl;
    cout << "str1.length = " << str1.length() << endl;
    cout << "str1.byte_length = " << str1.byte_length() << endl;
    cout << "str1.capacity = " << str1.capacity() << endl;
    str1.resize(20);
    cout << "after resize, str1.capacity = " << str1.capacity() << endl;
    str1.append("56").append(str2);
    cout << "after append, str1 = " << str1 << endl;
    str1.insert("0", 0);
    cout << "after insert, str1 = " << str1 << endl;
    str1.insert(str2, 0);
    cout << "after insert, str1 = " << str1 << endl;
    str1.erase(0);
    cout << "after erase, str1 = " << str1 << endl;
    str1.erase(0, 2);
    cout << "after erase, str1 = " << str1 << endl;
    str1.replace("2", "1");
    cout << "after replace, str1 = " << str1 << endl;
    auto str7 = str1.substr(0, 4);
    cout << "str1.substr(0,4) = " << str7 << endl;
    str1.insert("     ", 0);
    str1.append("                 ").append("1");
    cout << "after insert and append, str1 = " << str1 << endl;
    str1[str1.byte_length() - 1] = ' ';
    cout << "after str1[str1.byte_length() - 1]=' ' str1 = " << str1 << endl;
    cout << "after trim, str1 = " << str1.trim() << endl;
    cout << "after to_upper_case, str1 = " << str1.to_upper_case() << endl;
    cout << "after to_lower_case, str1 = " << str1.to_lower_case() << endl;
    str1.clear();
    cout << "after clear, str1 = " << str1 << endl;
}
