/*
 * Copyright (c) 2015, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#if defined(OS_WINDOWS)

#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
// TODO(VS): lint error: Streams are highly discouraged.
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <sstream>

#include "Shlwapi.h"
#include "utils/file_system.h"
#include "utils/string_utils.h"
#include "utils/winhdr.h"

#pragma comment(lib, "Shlwapi.lib")

#ifdef max
#undef max
#endif

uint64_t file_system::GetAvailableDiskSpace(const std::string& path) {
  DWORD sectors_per_cluster;
  DWORD bytes_per_sector;
  DWORD number_of_free_clusters;

  const BOOL res = GetDiskFreeSpace(path.c_str(),
                                    &sectors_per_cluster,
                                    &bytes_per_sector,
                                    &number_of_free_clusters,
                                    NULL);
  if (0 != res) {
    return number_of_free_clusters * sectors_per_cluster * bytes_per_sector;
  } else {
    return 0;
  }
}

int64_t file_system::FileSize(const std::string& path) {
  if (file_system::FileExists(path)) {
    struct _stat file_info = {0};
    _stat(path.c_str(), &file_info);
    return file_info.st_size;
  }
  return 0;
}

size_t file_system::DirectorySize(const std::string& path) {
  size_t size = 0;
  if (!DirectoryExists(path)) {
    return size;
  }

  const std::string find_string = ConcatPath(path, "*");
  WIN32_FIND_DATA ffd;

  HANDLE find = FindFirstFile(find_string.c_str(), &ffd);
  if (INVALID_HANDLE_VALUE == find) {
    return size;
  }

  do {
    if (FILE_ATTRIBUTE_DIRECTORY == ffd.dwFileAttributes) {
      if (strncmp(ffd.cFileName, ".", 1) != 0 &&
          strncmp(ffd.cFileName, "..", 2) != 0) {
        size += DirectorySize(ffd.cFileName);
      }
    } else {
      uint64_t file_size = 0;
      file_size |= ffd.nFileSizeHigh;
      file_size <<= 32;
      file_size |= ffd.nFileSizeLow;

      size += file_size;
    }
  } while (FindNextFile(find, &ffd) != 0);

  FindClose(find);
  return size;
}

std::string file_system::CreateDirectory(const std::string& name) {
  if (!DirectoryExists(name)) {
    _mkdir(name.c_str());
  }
  return name;
}

bool file_system::CreateDirectoryRecursively(const std::string& path) {
  size_t pos = 0;
  bool ret_val = true;

  // We have a lot of hardcoded posix paths.
  // So lets, just in case, try to replace delimiters
  const std::string delimiter = GetPathDelimiter();
  utils::ReplaceString(path, "/", delimiter);

  while (ret_val == true && pos < path.length()) {
    pos = path.find(delimiter, pos + 1);
    if (pos == std::string::npos) {
      pos = path.length();
    }
    if (!DirectoryExists(path.substr(0, pos))) {
      if (0 != _mkdir(path.substr(0, pos).c_str())) {
        ret_val = false;
      }
    }
  }
  return ret_val;
}

bool file_system::IsDirectory(const std::string& name) {
  struct _stat status = {0};
  if (-1 == _stat(name.c_str(), &status)) {
    return false;
  }
  return S_IFDIR == status.st_mode;
}

bool file_system::DirectoryExists(const std::string& name) {
  DWORD attrib = GetFileAttributes(name.c_str());
  return (attrib != INVALID_FILE_ATTRIBUTES &&
          (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool file_system::FileExists(const std::string& name) {
  struct _stat status = {0};
  if (-1 == _stat(name.c_str(), &status)) {
    return false;
  }
  return true;
}

bool file_system::Write(const std::string& file_name,
                        const std::vector<uint8_t>& data,
                        std::ios_base::openmode mode) {
  std::ofstream file(file_name.c_str(), std::ios_base::binary | mode);
  if (file.is_open()) {
    for (uint32_t i = 0; i < data.size(); ++i) {
      file << data[i];
    }
    file.close();
    return true;
  }
  return false;
}

std::ofstream* file_system::Open(const std::string& file_name,
                                 std::ios_base::openmode mode) {
  std::ofstream* file = new std::ofstream();
  file->open(file_name.c_str(), std::ios_base::binary | mode);
  if (file->is_open()) {
    return file;
  }

  delete file;
  return NULL;
}

bool file_system::Write(std::ofstream* const file_stream,
                        const uint8_t* data,
                        uint32_t data_size) {
  bool result = false;
  if (file_stream) {
    for (size_t i = 0; i < data_size; ++i) {
      (*file_stream) << data[i];
    }
    result = true;
  }
  return result;
}

void file_system::Close(std::ofstream* file_stream) {
  if (file_stream) {
    file_stream->close();
  }
}

std::string file_system::CurrentWorkingDirectory() {
  const size_t filename_max_length = 1024;
  char path[filename_max_length];
  _getcwd(path, filename_max_length);
  return std::string(path);
}

bool file_system::DeleteFile(const std::string& name) {
  if (FileExists(name) && IsWritingAllowed(name)) {
    return !remove(name.c_str());
  }
  return false;
}

void file_system::RemoveDirectoryContent(const std::string& directory_path) {
  if (!DirectoryExists(directory_path)) {
    return;
  }

  const std::string find_string = ConcatPath(directory_path, "*");
  WIN32_FIND_DATA ffd;

  HANDLE find = FindFirstFile(find_string.c_str(), &ffd);
  if (INVALID_HANDLE_VALUE == find) {
    return;
  }

  do {
    if (FILE_ATTRIBUTE_DIRECTORY == ffd.dwFileAttributes) {
      if (strncmp(ffd.cFileName, ".", 1) != 0 &&
          strncmp(ffd.cFileName, "..", 2) != 0) {
        RemoveDirectory(ffd.cFileName, true);
      }
    } else {
      remove(ffd.cFileName);
    }
  } while (FindNextFile(find, &ffd) != 0);

  FindClose(find);
}

bool file_system::RemoveDirectory(const std::string& directory_path,
                                  bool is_recursively) {
  if (DirectoryExists(directory_path) && IsWritingAllowed(directory_path)) {
    if (is_recursively) {
      RemoveDirectoryContent(directory_path);
    }
    return !_rmdir(directory_path.c_str());
  }
  return false;
}

bool file_system::IsAccessible(const std::string& name, int32_t how) {
  return !_access(name.c_str(), how);
}

bool file_system::IsWritingAllowed(const std::string& name) {
  return IsAccessible(name, 2) || IsAccessible(name, 6);
}

bool file_system::IsReadingAllowed(const std::string& name) {
  return IsAccessible(name, 4) || IsAccessible(name, 6);
}

std::vector<std::string> file_system::ListFiles(
    const std::string& directory_name) {
  std::vector<std::string> list_files;
  if (!DirectoryExists(directory_name)) {
    return list_files;
  }

  const std::string find_string = ConcatPath(directory_name, "*");
  WIN32_FIND_DATA ffd;

  HANDLE find = FindFirstFile(find_string.c_str(), &ffd);
  if (INVALID_HANDLE_VALUE == find) {
    return list_files;
  }

  do {
    if (FILE_ATTRIBUTE_DIRECTORY != ffd.dwFileAttributes) {
      list_files.push_back(ffd.cFileName);
    }
  } while (FindNextFile(find, &ffd) != 0);

  FindClose(find);
  return list_files;
}

bool file_system::WriteBinaryFile(const std::string& name,
                                  const std::vector<uint8_t>& contents) {
  using namespace std;
  ofstream output(name.c_str(), ios_base::binary | ios_base::trunc);
  output.write(reinterpret_cast<const char*>(&contents.front()),
               contents.size());
  return output.good();
}

bool file_system::ReadBinaryFile(const std::string& name,
                                 std::vector<uint8_t>& result) {
  if (!FileExists(name) || !IsReadingAllowed(name)) {
    return false;
  }

  std::ifstream file(name.c_str(), std::ios_base::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  const std::string& s = ss.str();

  result.resize(s.length());
  std::copy(s.begin(), s.end(), result.begin());
  return true;
}

bool file_system::ReadFile(const std::string& name, std::string& result) {
  if (!FileExists(name) || !IsReadingAllowed(name)) {
    return false;
  }

  std::ifstream file(name.c_str());
  std::ostringstream ss;
  ss << file.rdbuf();
  result = ss.str();
  return true;
}

const std::string file_system::ConvertPathForURL(const std::string& path) {
  std::string::const_iterator it_path = path.begin();
  std::string::const_iterator it_path_end = path.end();

  // list of characters to be encoded from the link:
  // http://www.blooberry.com/indexdot/html/topics/urlencoding.htm
  const std::string reserved_symbols = "$+,<>%{}|\^~[]` ";
  std::string::const_iterator it_sym = reserved_symbols.begin();
  std::string::const_iterator it_sym_end = reserved_symbols.end();

  std::string converted_path;
  while (it_path != it_path_end) {
    it_sym = reserved_symbols.begin();
    for (; it_sym != it_sym_end; ++it_sym) {
      if (*it_path == *it_sym) {
        const size_t size = 100;
        char percent_value[size];
        _snprintf_s(percent_value, size, "%%%x", *it_path);
        converted_path += percent_value;
        ++it_path;
        continue;
      }
    }
    converted_path += *it_path;
    ++it_path;
  }
  return converted_path;
}

bool file_system::CreateFile(const std::string& path) {
  std::ofstream file(path);
  if (!(file.is_open())) {
    return false;
  } else {
    file.close();
    return true;
  }
}

uint64_t file_system::GetFileModificationTime(const std::string& path) {
  struct _stat info;
  _stat(path.c_str(), &info);
  return static_cast<uint64_t>(info.st_mtime);
}

bool file_system::CopyFile(const std::string& src, const std::string& dst) {
  if (!FileExists(src) || FileExists(dst) || !CreateFile(dst)) {
    return false;
  }
  std::vector<uint8_t> data;
  if (!ReadBinaryFile(src, data) || !WriteBinaryFile(dst, data)) {
    DeleteFile(dst);
    return false;
  }
  return true;
}

bool file_system::MoveFile(const std::string& src, const std::string& dst) {
  if (!CopyFile(src, dst)) {
    return false;
  }
  if (!DeleteFile(src)) {
    DeleteFile(dst);
    return false;
  }
  return true;
}

bool file_system::IsRelativePath(const std::string& path) {
  return static_cast<bool>(PathIsRelative(path.c_str()));
}

void file_system::MakeAbsolutePath(std::string& path) {
  TCHAR buffer[MAX_PATH];
  // Handle the case when we receive abs linux path.
  // Removal of the leading slash will allow to get
  // correct path from the GetFullPathName
  int offset = 0;
  if (path.find("/") == 0) {
    offset = 1;
  }
  const DWORD size =
      GetFullPathName(path.c_str() + offset, MAX_PATH, buffer, NULL);
  if (size != 0) {
    path.assign(buffer);
  }
}

std::string file_system::GetPathDelimiter() {
  return "\\";
}

std::string file_system::ConcatPath(const std::string& str1,
                                    const std::string& str2) {
  return str1 + GetPathDelimiter() + str2;
}
std::string file_system::ConcatPath(const std::string& str1,
                                    const std::string& str2,
                                    const std::string& str3) {
  return ConcatPath(ConcatPath(str1, str2), str3);
}

std::string file_system::RetrieveFileNameFromPath(const std::string& path) {
  size_t slash_pos = path.find_last_of("/", path.length());
  size_t back_slash_pos = path.find_last_of("\\", path.length());
  return path.substr(
      std::max(slash_pos != std::string::npos ? slash_pos + 1 : 0,
               back_slash_pos != std::string::npos ? back_slash_pos + 1 : 0));
}

#endif  // OS_WINDOWS
