#include "Zip.hpp"

namespace ca {
    
    bool ZipArchive::createZip() {
        hzip_= CreateZip(filename_.c_str(), 0);
        if (hzip_ == nullptr) {
            // Failed to create zip file.
            return false;
        }
        return true;
    }

}