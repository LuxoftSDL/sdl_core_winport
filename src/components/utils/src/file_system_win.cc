/*
 * Copyright (c) 2016, Ford Motor Company
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
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <io.h>
// TODO(VS): lint error: Streams are highly discouraged.
#include <sstream>
#include <fstream>
#include <cstddef>
#include <algorithm>

#include "utils/winhdr.h"
#include "Shlwapi.h"
#include "utils/file_system.h"
#include "utils/string_utils.h"

namespace {

/**
  * @brief Converts UTF-8 string to wide string
  * @param str String to be converted
  * @return Result wide string
  */
std::wstring ConvertUTF8ToWString(const std::string& utf8_str) {
  if (utf8_str.empty()) {
    return std::wstring();
  }
  std::string extended_utf8_str(utf8_str);
  if (!file_system::IsRelativePath(utf8_str)) {
    extended_utf8_str = "\\\\?\\" + extended_utf8_str;
  }
  int size = MultiByteToWideChar(CP_UTF8,
                                 0,
                                 &extended_utf8_str[0],
                                 static_cast<int>(extended_utf8_str.size()),
                                 NULL,
                                 0);
  std::wstring wide_str(size, 0);
  MultiByteToWideChar(CP_UTF8,
                      0,
                      &extended_utf8_str[0],
                      static_cast<int>(extended_utf8_str.size()),
                      &wide_str[0],
                      size);
  return wide_str;
}

/**
  * @brief Converts wide string to UTF-8 string
  * @param str String to be converted
  * @return Result UTF-8 string
  */
std::string ConvertWStringToUTF8(const std::wstring& wide_str) {
  if (wide_str.empty()) {
    return std::string();
  }
  int size = WideCharToMultiByte(CP_UTF8,
                                 0,
                                 &wide_str[0],
                                 static_cast<int>(wide_str.size()),
                                 NULL,
                                 0,
                                 NULL,
                                 NULL);
  std::string utf8_str(size, 0);
  WideCharToMultiByte(CP_UTF8,
                      0,
                      &wide_str[0],
                      static_cast<int>(wide_str.size()),
                      &utf8_str[0],
                      size,
                      NULL,
                      NULL);
  return utf8_str;
}

}  // namespace

uint64_t file_system::GetAvailableDiskSpace(const std::string& utf8_path) {
  DWORD sectors_per_cluster;
  DWORD bytes_per_sector;
  DWORD number_of_free_clusters;

  const BOOL res = GetDiskFreeSpaceW(ConvertUTF8ToWString(utf8_path).c_str(),
                                     &sectors_per_cluster,
                                     &bytes_per_sector,
                                     &number_of_free_clusters,
                                     NULL);
  if (0 == res) {
    return 0;
  }
  return number_of_free_clusters * sectors_per_cluster * bytes_per_sector;
}

uint64_t file_system::FileSize(const std::string& utf8_path) {
  WIN32_FIND_DATAW ffd;
  HANDLE find = FindFirstFileW(ConvertUTF8ToWString(utf8_path).c_str(), &ffd);
  if (INVALID_HANDLE_VALUE == find) {
    return 0;
  }

  uint64_t file_size = 0;
  file_size |= ffd.nFileSizeHigh;
  file_size <<= 32;
  file_size |= ffd.nFileSizeLow;

  FindClose(find);
  return file_size;
}

uint64_t file_system::DirectorySize(const std::string& utf8_path) {
  uint64_t size = 0;
  if (!DirectoryExists(utf8_path)) {
    return size;
  }

  const std::string find_string = ConcatPath(utf8_path, "*");
  WIN32_FIND_DATAW ffd;

  HANDLE find = FindFirstFileW(ConvertUTF8ToWString(find_string).c_str(), &ffd);
  if (INVALID_HANDLE_VALUE == find) {
    return size;
  }

  do {
    if (FILE_ATTRIBUTE_DIRECTORY == ffd.dwFileAttributes) {
      const std::string utf8_file_name = ConvertWStringToUTF8(ffd.cFileName);
      if (strncmp(utf8_file_name.c_str(), ".", 1) != 0 &&
          strncmp(utf8_file_name.c_str(), "..", 2) != 0) {
        size += DirectorySize(utf8_file_name);
      }
    } else {
      uint64_t file_size = 0;
      file_size |= ffd.nFileSizeHigh;
      file_size <<= 32;
      file_size |= ffd.nFileSizeLow;

      size += file_size;
    }
  } while (FindNextFileW(find, &ffd) != 0);

  FindClose(find);
  return size;
}

std::string file_system::CreateDirectory(const std::string& utf8_path) {
  if (!DirectoryExists(utf8_path)) {
    _wmkdir(ConvertUTF8ToWString(utf8_path).c_str());
  }
  return utf8_path;
}

bool file_system::CreateDirectoryRecursively(const std::string& utf8_path) {
  const std::string delimiter = GetPathDelimiter();
  size_t pos = utf8_path.find(delimiter, 0);
  while (pos < utf8_path.length()) {
    pos = utf8_path.find(delimiter, pos + 1);
    if (pos == std::string::npos) {
      pos = utf8_path.length();
    }
    if (!DirectoryExists(utf8_path.substr(0, pos))) {
      if (0 !=
          _wmkdir(ConvertUTF8ToWString(utf8_path.substr(0, pos)).c_str())) {
        return false;
      }
    }
  }
  return true;
}

bool file_system::IsDirectory(const std::string& utf8_path) {
  struct _stat status = {0};
  if (-1 == _wstat(ConvertUTF8ToWString(utf8_path).c_str(), &status)) {
    return false;
  }
  return S_IFDIR == status.st_mode;
}

bool file_system::DirectoryExists(const std::string& utf8_path) {
  DWORD attrib = GetFileAttributesW(ConvertUTF8ToWString(utf8_path).c_str());
  return (attrib != INVALID_FILE_ATTRIBUTES &&
          (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool file_system::FileExists(const std::string& utf8_path) {
  struct _stat status = {0};
  if (-1 == _wstat(ConvertUTF8ToWString(utf8_path).c_str(), &status)) {
    return false;
  }
  return true;
}

bool file_system::Write(const std::string& utf8_path,
                        const std::vector<uint8_t>& data,
                        std::ios_base::openmode mode) {
  std::ofstream file(ConvertUTF8ToWString(utf8_path),
                     std::ios_base::binary | mode);
  if (!file.is_open()) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(&data[0]), data.size());
  file.close();
  return file.good();
}

std::ofstream* file_system::Open(const std::string& utf8_path,
                                 std::ios_base::openmode mode) {
  std::ofstream* file = new std::ofstream();
  file->open(ConvertUTF8ToWString(utf8_path), std::ios_base::binary | mode);
  if (!file->is_open()) {
    delete file;
    return NULL;
  }
  return file;
}

bool file_system::Write(std::ofstream* const file_stream,
                        const uint8_t* data,
                        size_t data_size) {
  if (!file_stream) {
    return false;
  }
  file_stream->write(reinterpret_cast<const char*>(&data[0]), data_size);
  return file_stream->good();
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

bool file_system::DeleteFile(const std::string& utf8_path) {
  if (FileExists(utf8_path) && IsWritingAllowed(utf8_path)) {
    return !_wremove(ConvertUTF8ToWString(utf8_path).c_str());
  }
  return false;
}

void file_system::RemoveDirectoryContent(const std::string& utf8_path) {
  if (!DirectoryExists(utf8_path)) {
    return;
  }

  const std::string find_string = ConcatPath(utf8_path, "*");
  WIN32_FIND_DATAW ffd;

  HANDLE find = FindFirstFileW(ConvertUTF8ToWString(find_string).c_str(), &ffd);
  if (INVALID_HANDLE_VALUE == find) {
    return;
  }

  do {
    if (FILE_ATTRIBUTE_DIRECTORY == ffd.dwFileAttributes) {
      const std::string utf8_file_name = ConvertWStringToUTF8(ffd.cFileName);
      if (strncmp(utf8_file_name.c_str(), ".", 1) != 0 &&
          strncmp(utf8_file_name.c_str(), "..", 2) != 0) {
        RemoveDirectory(utf8_file_name, true);
      }
    } else {
      _wremove(ffd.cFileName);
    }
  } while (FindNextFileW(find, &ffd) != 0);

  FindClose(find);
}

bool file_system::RemoveDirectory(const std::string& utf8_path,
                                  bool is_recursively) {
  if (DirectoryExists(utf8_path) && IsWritingAllowed(utf8_path)) {
    if (is_recursively) {
      RemoveDirectoryContent(utf8_path);
    }
    return !_wrmdir(ConvertUTF8ToWString(utf8_path).c_str());
  }
  return false;
}

bool file_system::IsAccessible(const std::string& utf8_path, int32_t how) {
  return !_waccess(ConvertUTF8ToWString(utf8_path).c_str(), how);
}

bool file_system::IsWritingAllowed(const std::string& utf8_path) {
  return IsAccessible(utf8_path, 2) || IsAccessible(utf8_path, 6);
}

bool file_system::IsReadingAllowed(const std::string& utf8_path) {
  return IsAccessible(utf8_path, 4) || IsAccessible(utf8_path, 6);
}

std::vector<std::string> file_system::ListFiles(const std::string& utf8_path) {
  std::vector<std::string> list_files;
  if (!DirectoryExists(utf8_path)) {
    return list_files;
  }

  const std::string find_string = ConcatPath(utf8_path, "*");
  WIN32_FIND_DATAW ffd;

  HANDLE find = FindFirstFileW(ConvertUTF8ToWString(find_string).c_str(), &ffd);
  if (INVALID_HANDLE_VALUE == find) {
    return list_files;
  }

  do {
    if (FILE_ATTRIBUTE_DIRECTORY != ffd.dwFileAttributes) {
      list_files.push_back(ConvertWStringToUTF8(ffd.cFileName));
    }
  } while (FindNextFileW(find, &ffd) != 0);

  FindClose(find);
  return list_files;
}

bool file_system::WriteBinaryFile(const std::string& utf8_path,
                                  const std::vector<uint8_t>& data) {
  using namespace std;
  ofstream output(ConvertUTF8ToWString(utf8_path),
                  ios_base::binary | ios_base::trunc);
  output.write(reinterpret_cast<const char*>(&data.front()), data.size());
  return output.good();
}

bool file_system::ReadBinaryFile(const std::string& utf8_path,
                                 std::vector<uint8_t>& result) {
  if (!FileExists(utf8_path) || !IsReadingAllowed(utf8_path)) {
    return false;
  }

  std::ifstream file(ConvertUTF8ToWString(utf8_path), std::ios_base::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  const std::string& s = ss.str();

  result.resize(s.length());
  std::copy(s.begin(), s.end(), result.begin());
  return true;
}

bool file_system::ReadFile(const std::string& utf8_path, std::string& result) {
  if (!FileExists(utf8_path) || !IsReadingAllowed(utf8_path)) {
    return false;
  }

  std::ifstream file(ConvertUTF8ToWString(utf8_path));
  std::ostringstream ss;
  ss << file.rdbuf();
  result = ss.str();
  return true;
}

const std::string file_system::ConvertPathForURL(const std::string& utf8_path) {
  std::string::const_iterator it_path = utf8_path.begin();
  std::string::const_iterator it_path_end = utf8_path.end();

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

bool file_system::CreateFile(const std::string& utf8_path) {
  std::ofstream file(ConvertUTF8ToWString(utf8_path));
  if (!(file.is_open())) {
    return false;
  } else {
    file.close();
    return true;
  }
}

uint64_t file_system::GetFileModificationTime(const std::string& utf8_path) {
  struct _stat info;
  _wstat(ConvertUTF8ToWString(utf8_path).c_str(), &info);
  return static_cast<uint64_t>(info.st_mtime);
}

bool file_system::CopyFile(const std::string& utf8_src_path,
                           const std::string& utf8_dst_path) {
  if (!FileExists(utf8_src_path) || FileExists(utf8_dst_path) ||
      !CreateFile(utf8_dst_path)) {
    return false;
  }
  std::vector<uint8_t> data;
  if (!ReadBinaryFile(utf8_src_path, data) ||
      !WriteBinaryFile(utf8_dst_path, data)) {
    DeleteFile(utf8_dst_path);
    return false;
  }
  return true;
}

bool file_system::MoveFile(const std::string& utf8_src_path,
                           const std::string& utf8_dst_path) {
  if (!CopyFile(utf8_src_path, utf8_dst_path)) {
    return false;
  }
  if (!DeleteFile(utf8_src_path)) {
    DeleteFile(utf8_dst_path);
    return false;
  }
  return true;
}

bool file_system::IsRelativePath(const std::string& utf8_path) {
  return std::string::npos == utf8_path.find(":");
}

std::string file_system::GetPathDelimiter() {
  return "\\";
}

std::string file_system::ConcatPath(const std::string& utf8_path1,
                                    const std::string& utf8_path2) {
  return utf8_path1 + GetPathDelimiter() + utf8_path2;
}

std::string file_system::ConcatPath(const std::string& utf8_path1,
                                    const std::string& utf8_path2,
                                    const std::string& utf8_path3) {
  return ConcatPath(ConcatPath(utf8_path1, utf8_path2), utf8_path3);
}

std::string file_system::RetrieveFileNameFromPath(
    const std::string& utf8_path) {
  size_t slash_pos = utf8_path.find_last_of("/", utf8_path.length());
  size_t back_slash_pos = utf8_path.find_last_of("\\", utf8_path.length());
  return utf8_path.substr(
      std::max(slash_pos != std::string::npos ? slash_pos + 1 : 0,
               back_slash_pos != std::string::npos ? back_slash_pos + 1 : 0));
}
