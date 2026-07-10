#include <gmock/gmock.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-success") == 0) {
        std::cout << "stdout";
        std::cerr << "stderr";
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-timeout") == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-failure") == 0) {
        return 7;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-echo") == 0) {
        std::string input;
        std::getline(std::cin, input);
        std::cout << input;
        return 0;
    }
    if (argc == 4 && std::strcmp(argv[1], "--subprocess-args") == 0) {
        std::cout << argv[2] << "|" << argv[3];
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-large-output") == 0) {
        const std::string bytes(256 * 1024, 'x');
        std::cout << bytes;
        std::cerr << bytes;
        return 0;
    }

    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
