#pragma once

#include <ff.h>

#include <filesystem>
#include <memory>
#include <span>
#include <vector>

class FileSystem {
 public:
  FileSystem();

 private:
  FATFS fs_;
};

class File {
 public:
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

  static File Open(std::filesystem::path path, const OpenFlags& flags);

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
  File() = default;

  std::unique_ptr<FIL> fat_file_;
};

class Directory {
 public:
  static Directory Open(std::filesystem::path path);
  ~Directory();

  Directory(Directory&&) = default;
  Directory& operator=(Directory&&) = default;

  struct Entry {
    std::filesystem::path path;
    bool is_directory;
  };

  std::vector<Entry> Entries();

 private:
  Directory() = default;
  std::unique_ptr<DIR> fat_dir_;
};
