//
//  compilation_record.h
//
//  Created by Pujun Lun on 5/13/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SHADER_COMPILATION_RECORD_H
#define LIGHTER_SHADER_COMPILATION_RECORD_H

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "lighter/common/util.h"
#include "lighter/shader_compiler/util.h"
#include "third_party/absl/container/flat_hash_map.h"

namespace lighter::shader_compiler {

// Forward declarations.
class CompilationRecordReader;
class CompilationRecordWriter;

// This is the base class of readers and writers of the compilation record file.
// Each line in the record file would have such a format:
// <source file path> <source file hash> <compiled file hash>
class CompilationRecordHandler {
 public:
  // Stores hash values of source and compiled files.
  struct FileHash {
    std::string source_file_hash;
    std::string compiled_file_hash;
  };

  static std::pair<CompilationRecordReader, CompilationRecordWriter>
  CreateHandlers(const std::filesystem::path& shader_dir,
                 OptimizationLevel opt_level);

  virtual ~CompilationRecordHandler() = default;

 protected:
  // Maps the source file path to hash values of source and compiled files.
  using FileHashValueMap = absl::flat_hash_map<std::filesystem::path, FileHash,
                                               common::util::PathHash>;
};

// This class reads the compilation record file and converts it to a hash map
// for future queries.
class CompilationRecordReader : public CompilationRecordHandler {
 public:
  CompilationRecordReader(const std::filesystem::path& record_file_path,
                          OptimizationLevel opt_level);

  // This class is only movable.
  CompilationRecordReader(CompilationRecordReader&&) noexcept = default;
  CompilationRecordReader& operator=(CompilationRecordReader&&) noexcept
      = default;

  // Returns a pointer to 'FileHash' if it is in the compilation record file.
  // Otherwise, returns nullptr.
  const FileHash* GetFileHash(
      const std::filesystem::path& source_file_path) const;

 private:
  // Parses the compilation record file and populates 'file_hash_map_'.
  void ParseRecordFile(std::ifstream& record_file,
                       OptimizationLevel opt_level);

  // Maps the source file path to hash values of source and compiled files.
  FileHashValueMap file_hash_map_;
};

// This class collects hash values of files before/after compilation, and writes
// them to the compilation record file.
class CompilationRecordWriter : public CompilationRecordHandler {
 public:
  CompilationRecordWriter(std::filesystem::path&& record_file_path,
                          OptimizationLevel opt_level)
      : record_file_path_{std::move(record_file_path)},
        opt_level_{opt_level} {}

  // This class is only movable.
  CompilationRecordWriter(CompilationRecordWriter&&) noexcept = default;
  CompilationRecordWriter& operator=(CompilationRecordWriter&&) noexcept
      = default;

  // Registers file hash values, and throws a runtime exception if this file has
  // already been registered.
  void RegisterFileHash(std::filesystem::path&& source_file_path,
                        FileHash&& file_hash);

  // Writes all registered file hash values to the compilation record file.
  static void WriteAll(CompilationRecordWriter&& writer) {
    writer.WriteAll();
  }

 private:
  // Writes all registered file hash values to the compilation record file.
  // This should only be called once.
  void WriteAll() const;

  // Path to compilation record file.
  std::filesystem::path record_file_path_;

  // Optimization level is part of the record file header.
  OptimizationLevel opt_level_;

  // Maps the source file path to hash values of source and compiled files.
  FileHashValueMap file_hash_map_;
};

}  // namespace lighter::shader_compiler

#endif  // LIGHTER_SHADER_COMPILATION_RECORD_H
