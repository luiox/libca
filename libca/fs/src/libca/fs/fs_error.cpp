#include "fs_error.hpp"

namespace ca { namespace fs {

std::string to_string(FsError e)
{
    switch (e) {
        case FsError::Ok:               return "ok";
        case FsError::FileNotFound:     return "file not found";
        case FsError::NotARegularFile:  return "not a regular file";
        case FsError::NotADirectory:    return "not a directory";
        case FsError::OpenFailed:       return "failed to open file";
        case FsError::ReadFailed:       return "read failed";
        case FsError::WriteFailed:      return "write failed";
        case FsError::PermissionDenied: return "permission denied";
        case FsError::AlreadyExists:    return "file already exists";
        case FsError::DiskFull:         return "disk full";
        case FsError::IsADirectory:     return "is a directory";
        case FsError::DirectoryNotEmpty: return "directory not empty";
        case FsError::NameTooLong:      return "name too long";
        case FsError::TooManyOpenFiles: return "too many open files";
        case FsError::InvalidUtf8:      return "path is not valid UTF-8";
        case FsError::Unknown:          return "unknown error";
    }
    return "unknown error";
}

FsError classify_error(const std::error_code& ec) noexcept
{
    if (!ec) return FsError::Ok;
    // 优先用 std::errc 条件值判断，跨平台稳定。
    const auto cond = static_cast<std::errc>(ec.default_error_condition().value());
    switch (cond) {
        case std::errc::no_such_file_or_directory:    return FsError::FileNotFound;
        case std::errc::not_a_directory:              return FsError::NotADirectory;
        case std::errc::is_a_directory:               return FsError::IsADirectory;
        case std::errc::directory_not_empty:          return FsError::DirectoryNotEmpty;
        case std::errc::permission_denied:            return FsError::PermissionDenied;
        case std::errc::operation_not_permitted:      return FsError::PermissionDenied;
        case std::errc::read_only_file_system:        return FsError::PermissionDenied;
        case std::errc::file_exists:                  return FsError::AlreadyExists;
        case std::errc::no_space_on_device:           return FsError::DiskFull;
        case std::errc::no_buffer_space:              return FsError::DiskFull;
        case std::errc::filename_too_long:            return FsError::NameTooLong;
        case std::errc::too_many_files_open:          return FsError::TooManyOpenFiles;
        case std::errc::too_many_files_open_in_system: return FsError::TooManyOpenFiles;
        default:                                       return FsError::Unknown;
    }
}

}}  // namespace ca::fs
