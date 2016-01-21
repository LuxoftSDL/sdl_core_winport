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
#include "utils/file_system.h"
#include "utils/logger.h"
#include "utils/string_utils.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>
#include <QtCore>
#include <QStorageInfo>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>

#include <io.h>
#include <fstream>
#include <cstddef>
#include <algorithm>

#define R_OK 4
#define W_OK 2

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

uint64_t file_system::GetAvailableDiskSpace(const std::string& utf8_path) {
  QStorageInfo mstor(QString(utf8_path.c_str()));
  qint64 b_aval = mstor.bytesAvailable();
  return static_cast<uint64_t>(b_aval);
}

int64_t file_system::FileSize(const std::string& utf8_path) {
  if (file_system::FileExists(utf8_path)) {
    return static_cast<int64_t>(QFileInfo(QString(utf8_path.c_str())).size());
  }
  return 0;
}

size_t file_system::DirectorySize(const std::string& utf8_path) {
  quint64 size = 0;
  QFileInfo str_info(QString(utf8_path.c_str()));
  if (str_info.isDir()) {
    QDir dir(QString(utf8_path.c_str()));
    QFileInfoList list =
        dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden |
                          QDir::NoSymLinks | QDir::NoDotAndDotDot);
    foreach (QFileInfo fileInfo, list) {
      if (fileInfo.isDir()) {
        size += DirectorySize(fileInfo.absoluteFilePath().toStdString());
      } else {
        size += fileInfo.size();
      }
    }
  }
  return static_cast<size_t>(size);
}

std::string file_system::CreateDirectory(const std::string& utf8_path) {
  QDir dir;
  if (dir.mkdir(QString(utf8_path.c_str()))) {
    return utf8_path;
  }
  return "";
}

bool file_system::CreateDirectoryRecursively(const std::string& utf8_path) {
  QDir dir;
  return dir.mkpath(QString(utf8_path.c_str()));
}

bool file_system::IsDirectory(const std::string& utf8_path) {
  QFileInfo checkFile(QString(utf8_path.c_str()));
  return checkFile.isDir();
}

bool file_system::DirectoryExists(const std::string& utf8_path) {
  QFileInfo checkFile(QString(utf8_path.c_str()));
  return checkFile.exists() && checkFile.isDir();
}

bool file_system::FileExists(const std::string& utf8_path) {
  QFileInfo checkFile(QString(utf8_path.c_str()));
  return checkFile.exists() && checkFile.isFile();
}

bool file_system::Write(const std::string& utf8_path,
                        const std::vector<uint8_t>& data,
                        std::ios_base::openmode mode) {
  QByteArray text(reinterpret_cast<uint8_t>(data.data()), data.size());
  QFile file(QString(utf8_path.c_str()));
  file.open(QIODevice::WriteOnly | static_cast<QIODevice::OpenModeFlag>(mode));
  if (file.isOpen()) {
    file.write(text);
    file.close();
    return true;
  }
  return false;
}

std::ofstream* file_system::Open(const std::string& utf8_path,
                                 std::ios_base::openmode mode) {
  std::ofstream* file = new std::ofstream();
  file->open(QString(utf8_path.c_str()).toStdWString(),
             std::ios_base::binary | mode);
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
  return QDir::currentPath().toStdString();
}

bool file_system::DeleteFile(const std::string& utf8_path) {
  if (FileExists(utf8_path) && IsAccessible(utf8_path, W_OK)) {
    return QFile::remove(QString(utf8_path.c_str()));
  }
  return false;
}

void file_system::RemoveDirectoryContent(const std::string& utf8_path) {
  QDir dir(QString(utf8_path.c_str()));
  dir.setNameFilters(QStringList() << "*.*");
  dir.setFilter(QDir::Files);
  foreach (QString dirFile, dir.entryList()) { dir.remove(dirFile); }
  dir.rmpath(QString(utf8_path.c_str()));
}

bool file_system::RemoveDirectory(const std::string& utf8_path,
                                  bool is_recursively) {
  QDir dir(QString(utf8_path.c_str()));
  if (DirectoryExists(utf8_path) && IsAccessible(utf8_path, W_OK)) {
    if (is_recursively) {
      return dir.removeRecursively();
    }
    return dir.rmdir(QString(utf8_path.c_str()));
  }
  return false;
}

bool file_system::IsAccessible(const std::string& utf8_path, int32_t how) {
  QFileInfo qFileInfo(QString(utf8_path.c_str()));
  switch (how) {
    case (W_OK):
      return qFileInfo.isWritable();
    case (R_OK):
      return qFileInfo.isReadable();
    default:
      return false;
  }
}

bool file_system::IsWritingAllowed(const std::string& utf8_path) {
  return IsAccessible(utf8_path, W_OK);
}

bool file_system::IsReadingAllowed(const std::string& utf8_path) {
  return IsAccessible(utf8_path, R_OK);
}

std::vector<std::string> file_system::ListFiles(const std::string& utf8_path) {
  std::vector<std::string> listFiles;
  if (!DirectoryExists(utf8_path)) {
    return listFiles;
  }
  QDir dir(QString(utf8_path.c_str()));
  QStringList list_files = dir.entryList(QDir::Files);
  foreach (QString str, list_files) { listFiles.push_back(str.toStdString()); }
  return listFiles;
}

bool file_system::WriteBinaryFile(const std::string& utf8_path,
                                  const std::vector<uint8_t>& contents) {
  using namespace std;
  ofstream output(QString(utf8_path.c_str()).toStdWString(),
                  ios_base::binary | ios_base::trunc);
  output.write(reinterpret_cast<const char*>(&contents.front()),
               contents.size());
  return output.good();
}

bool file_system::ReadBinaryFile(const std::string& utf8_path,
                                 std::vector<uint8_t>& result) {
  if (!FileExists(utf8_path) || !IsAccessible(utf8_path, R_OK)) {
    return false;
  }

  std::ifstream file(QString(utf8_path.c_str()).toStdWString(),
                     std::ios_base::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  const std::string& s = ss.str();

  result.resize(s.length());
  std::copy(s.begin(), s.end(), result.begin());
  return true;
}

bool file_system::ReadFile(const std::string& utf8_path, std::string& result) {
  if (!FileExists(utf8_path) || !IsAccessible(utf8_path, R_OK)) {
    return false;
  }

  std::ifstream file(QString(utf8_path.c_str()).toStdWString());
  std::ostringstream ss;
  ss << file.rdbuf();
  result = ss.str();
  return true;
}

const std::string file_system::ConvertPathForURL(const std::string& utf8_path) {
  QString p(utf8_path.c_str());
  return QUrl::fromLocalFile(p).url().toStdString();
}

bool file_system::CreateFile(const std::string& utf8_path) {
  std::ofstream file(QString(utf8_path.c_str()).toStdWString());
  if (!(file.is_open())) {
    return false;
  } else {
    file.close();
    return true;
  }
}

uint64_t file_system::GetFileModificationTime(const std::string& utf8_path) {
  QFileInfo f(QString(utf8_path.c_str()));
  return static_cast<uint64_t>(f.lastModified().toMSecsSinceEpoch());
}

bool file_system::CopyFile(const std::string& utf8_src_path,
                           const std::string& utf8_dst_path) {
  if (!FileExists(utf8_src_path) || FileExists(utf8_dst_path) ||
      !CreateFile(utf8_dst_path)) {
    return false;
  }
  return QFile::copy(QString(utf8_src_path.c_str()),
                     QString(utf8_dst_path.c_str()));
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

std::string file_system::GetPathDelimiter() {
  return QString(QDir::separator()).toStdString();
}

bool file_system::IsRelativePath(const std::string& utf8_path) {
  return QDir::isRelativePath(utf8_path.c_str());
}

void file_system::MakeAbsolutePath(std::string& utf8_path) {
  if (!IsRelativePath(utf8_path)) {
    return;
  }
  utf8_path = ConcatPath(CurrentWorkingDirectory(), utf8_path);
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
  QFile fname(utf8_path.c_str());
  QFileInfo file_info(fname.fileName());
  return file_info.fileName().toStdString();
}
