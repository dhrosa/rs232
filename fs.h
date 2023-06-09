#pragma once

#include <ff.h>
#include "flash.h"

#include <filesystem>
#include <memory>
#include <span>
#include <vector>

class File;
class Directory;

class FileSystem {
 public:
  FileSystem(FlashDisk& disk) : disk_(disk) {}

  // See http://elm-chan.org/fsw/ff/doc/open.html mode flags
  struct OpenFlags {
    bool read = false;
    bool write = false;
    bool open_existing = false;
    bool create_new = false;
    bool create_always = false;
    bool open_always = false;
    bool open_append = false;
  };

  File OpenFile(std::filesystem::path path, const OpenFlags& flags);
  Directory OpenDirectory(std::filesystem::path path);

  void Install();

 private:
  FlashDisk& disk_;
  FATFS fs_;
};

class File {
 public:
  // Only valid operation a default-constructed object is closing and
  // destructing.
  File() = default;

  ~File();

  File(File&& other) = default;
  File& operator=(File&& other) = default;

  void Close();

  int Tell();

  void Seek(int location);

  int Size();

  // buffer: Buffer to read data into. Number of bytes read is the size of this
  // buffer.
  //
  // Returns the subspan of the input buffer that was actually read into. This
  // may be smaller than the input if EOF was reached.
  std::span<std::byte> Read(std::span<std::byte> buffer);

  std::string ReadAll();

  // Returns the number of bytes actually written.
  int Write(std::span<const std::byte> buffer);

  // Returns the number of bytes actually written.
  int Write(std::string_view str);

  void Sync();

 private:
  friend class FileSystem;
  std::unique_ptr<FIL> fat_file_;
};

class Directory {
 public:
  // Only valid operation a default-constructed object is closing and
  // destructing.
  Directory() = default;
  ~Directory();

  Directory(Directory&&) = default;
  Directory& operator=(Directory&&) = default;

  struct Entry {
    std::filesystem::path path;
    bool is_directory;
  };

  std::vector<Entry> Entries();

 private:
  friend class FileSystem;
  std::unique_ptr<DIR> fat_dir_;
};
