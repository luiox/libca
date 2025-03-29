#ifndef LIBCA_UTILITY_ZIP_HPP
#define LIBCA_UTILITY_ZIP_HPP

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "zip_utils/zip.h"
#include "zip_utils/unzip.h"

namespace ca {

// 
class ZipEntry{
public:
};

class ZipArchive {
    std::string filename_;
    HZIP hzip_;
public:
    ZipArchive(const std::string& filename) : filename_(filename) {}

    bool createZip();

    void putNextEntry();
    
    void closeEntry();

    
    

//     bool addFile(const std::string& filename, const std::vector<unsigned char>& data) {
//         if (!zipFile) {
//             std::cerr << "Zip file not open." << std::endl;
//             return false;
//         }

//         zip_fileinfo zipInfo = {0};
//         int err = zipOpenNewFileInZip(zipFile, filename.c_str(), &zipInfo,
//                                        NULL, 0, NULL, 0, NULL,
//                                        Z_DEFLATED, Z_DEFAULT_COMPRESSION);
//         if (err != ZIP_OK) {
//             std::cerr << "Failed to open new file in zip." << std::endl;
//             return false;
//         }

//         err = zipWriteInFileInZip(zipFile, data.data(), data.size());
//         if (err != ZIP_OK) {
//             std::cerr << "Failed to write data to zip." << std::endl;
//             zipCloseFileInZip(zipFile);
//             return false;
//         }

//         err = zipCloseFileInZip(zipFile);
//         if (err != ZIP_OK) {
//             std::cerr << "Failed to close file in zip." << std::endl;
//             return false;
//         }

//         return true;
//     }

//     bool closeZip() {
//         if (!zipFile) {
//             std::cerr << "Zip file not open." << std::endl;
//             return false;
//         }

//         int err = zipClose(zipFile, NULL);
//         if (err != ZIP_OK) {
//             std::cerr << "Failed to close zip file." << std::endl;
//             return false;
//         }

//         zipFile = NULL;
//         return true;
//     }


};
    
}

#endif // !LIBCA_UTILITY_ZIP_HPP
