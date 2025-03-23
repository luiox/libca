#ifndef LIBCA_PLATFORM_HPP
#define LIBCA_PLATFORM_HPP

#include <string>

namespace ca {
class OsUtil{
public:
static std::string getOsName(){
#ifdef _WIN32
    return "Windows";
#elif __linux__
    return "Linux";
#elif __APPLE__
    return "MacOS";
#else
    return "Unknown";
    #endif   
}
static bool isWindows(){
#ifdef _WIN32
    return true;
#else
    return false;
    #endif
}

static bool isLinux(){
    #ifdef __linux__
        return true;
    #else
        return false;
        #endif
}

static bool isMac(){
    #ifdef __APPLE__
        return true;
    #else
        return false;   
    #endif
}
};

class ArchUtil{
public:
    static std::string getArchName(){
        #ifdef _WIN32
            return "x86_64";
        #elif __linux__
            return "x86_64";
        #elif __APPLE__
            return "x86_64";
        #else
            return "Unknown";
        #endif
    }

};

    
}

#ifdef _WIN32
#include <Windows.h>
#include <wincrypt.h>
#endif
#ifdef __linux__    
#include <unistd.h>
#endif
#ifdef __APPLE__
// unsuport
#endif


#endif // !LIBCA_PLATFORM_HPP