#include "fs_error.hpp"

namespace ca {
namespace fs {

std::string to_string(FsError e)
{
    switch (e) {
    case FsError::Ok: return "ok";
    case FsError::FileNotFound: return "file not found";
    case FsError::NotARegularFile: return "not a regular file";
    case FsError::NotADirectory: return "not a directory";
    case FsError::OpenFailed: return "failed to open file";
    case FsError::ReadFailed: return "read failed";
    case FsError::WriteFailed: return "write failed";
    case FsError::PermissionDenied: return "permission denied";
    case FsError::AlreadyExists: return "file already exists";
    case FsError::DiskFull: return "disk full";
    case FsError::IsADirectory: return "is a directory";
    case FsError::DirectoryNotEmpty: return "directory not empty";
    case FsError::NameTooLong: return "name too long";
    case FsError::TooManyOpenFiles: return "too many open files";
    case FsError::Unknown: return "unknown error";
    }
    return "unknown error";
}

}   // namespace fs
}   // namespace ca
