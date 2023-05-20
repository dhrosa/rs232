#pragma once

#include <ff.h>

#include <filesystem>
#include <memory>
#include <span>
#include <vector>

class File;
class Directory;

class FileSystem {
 public:
  FileSystem();

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

 private:
  FATFS fs_;
};

class File {
 public:
  ~File();

  File(File&& other) = default;
  File& operator=(File&& other) = default;

  void Close();

  int Tell();

  void Seek(int location);

  // buffer: Buffer to read data into. Number of bytes read is the size of this
  // buffer.
  //
  // Returns the subspan of the input buffer that was actually read into. This
  // may be smaller than the input if EOF was reached.
  std::span<std::byte> Read(std::span<std::byte> buffer);

  // Returns the number of bytes actually written.
  int Write(std::span<const std::byte> buffer);

  void Sync();

 private:
  friend class FileSystem;
  File() = default;

  std::unique_ptr<FIL> fat_file_;
};

class Directory {
 public:
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
  Directory() = default;
  std::unique_ptr<DIR> fat_dir_;
};
