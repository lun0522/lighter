//
//  compilation_record.h
//
//  Created by Pujun Lun on 5/13/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SHADER_COMPILATION_RECORD_H
#define LIGHTER_SHADER_COMPILATION_RECORD_H

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <tuple>

#include "lighter/common/graphics_api.h"
#include "third_party/absl/container/flat_hash_map.h"

namespace lighter::shader {

// Forward declarations.
class CompilationRecordReader;
class CompilationRecordWriter;

// This is the base class of readers and writers of the compilation record file.
// Each line in the record file would have such a format:
// <graphics API> <source file path> <source file hash> <compiled file hash>
class CompilationRecordHandler {
 public:
  // Stores hash values of source and compiled files.
  struct FileHash {
    std::string source_file_hash;
    std::string compiled_file_hash;
  };

  static std::tuple<CompilationRecordReader, CompilationRecordWriter>
  CreateHandlers(std::string_view shader_dir);

  virtual ~CompilationRecordHandler() = default;

 protected:
  enum ApiIndex {
    kOpenglIndex = 0,
    kVulkanIndex,
    kNumApis,
  };

  // Each element is an abbreviation of an graphics API's name.
  using ApiAbbreviationArray = std::array<std::string, kNumApis>;

  // Returns an array of graphics API name abbreviations.
  static const ApiAbbreviationArray& GetApiAbbreviations();

  // Converts a graphics API to the index, which can be used for arrays, etc.
  static int ApiToIndex(common::GraphicsApi graphics_api);
};

// This class reads the compilation record file and converts it to hash maps for
// future queries.
class CompilationRecordReader : public CompilationRecordHandler {
 public:
  explicit CompilationRecordReader(
      const std::filesystem::path& record_file_path);

  // This class is only movable.
  CompilationRecordReader(CompilationRecordReader&&) noexcept = default;
  CompilationRecordReader& operator=(CompilationRecordReader&&) noexcept
      = default;

  // Returns a pointer to 'FileHash' if it is in the compilation record file.
  // Otherwise, returns nullptr.
  const FileHash* GetFileHash(common::GraphicsApi graphics_api,
                              std::string_view source_file_path) const;

 private:
  // Parses the compilation record file and populates 'file_hash_maps_'.
  void ParseRecordFile(std::ifstream& record_file);

  // Converts a graphics API name abbreviation to the index into
  // 'file_hash_maps_'.
  int ApiAbbreviationToIndex(std::string_view abbreviation) const;

  // Maps the source file path to hash values of source and compiled files.
  absl::flat_hash_map<std::string, FileHash> file_hash_maps_[kNumApis];
};

// This class collects hash values of files before/after compilation, and writes
// them to the compilation record file.
class CompilationRecordWriter : public CompilationRecordHandler {
 public:
  explicit CompilationRecordWriter(std::filesystem::path&& record_file_path)
      : record_file_path_{std::move(record_file_path)} {}

  // This class is only movable.
  CompilationRecordWriter(CompilationRecordWriter&&) noexcept = default;
  CompilationRecordWriter& operator=(CompilationRecordWriter&&) noexcept
      = default;

  // Registers file hash values, and throws a runtime exception if this file has
  // already been registered with the same graphics API.
  void RegisterFileHash(common::GraphicsApi graphics_api,
                        std::string&& source_file_path,
                        FileHash&& file_hash);

  // Writes all registered file hash values to the compilation record file.
  static void WriteToDisk(CompilationRecordWriter&& writer) {
    writer.WriteAll();
  }

 private:
  // Writes all registered file hash values to the compilation record file.
  // This should only be called once.
  void WriteAll() const;

  // Path to compilation record file.
  std::filesystem::path record_file_path_;

  // Maps the source file path to hash values of source and compiled files.
  absl::flat_hash_map<std::string, FileHash> file_hash_maps_[kNumApis];
};

}  // namespace lighter::shader

#endif  // LIGHTER_SHADER_COMPILATION_RECORD_H
