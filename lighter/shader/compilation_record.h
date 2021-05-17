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
#include <string_view>

#include "lighter/common/graphics_api.h"
#include "third_party/absl/container/flat_hash_map.h"

namespace lighter::shader {

// Stores hash values of source and compiled files.
struct CompilationHash {
  std::string source_sha256;
  std::string compiled_sha256;
};

// This file reads the compilation record file and converts it to hash maps for
// future queries.
class CompilationRecordReader {
 public:
  explicit CompilationRecordReader(std::string_view shader_dir);

  // This class is only movable.
  CompilationRecordReader(CompilationRecordReader&&) noexcept = default;
  CompilationRecordReader& operator=(CompilationRecordReader&&) noexcept
      = default;

  // Returns a pointer to 'CompilationHash' if it is in the compilation record
  // file. Otherwise, returns nullptr.
  const CompilationHash* GetCompilationHash(
      std::string_view source_file_path,
      common::GraphicsApi graphics_api) const;

 private:
  enum ApiIndex {
    kOpenglIndex = 0,
    kVulkanIndex,
    kNumApis,
  };

  // Parses the compilation record file and populates hash maps.
  void ParseRecordFile(std::ifstream& record_file);

  // Converts a graphics API name abbreviation to the index into
  // 'file_hash_maps_'.
  int ApiToIndex(std::string_view abbreviation) const;

  // Converts a graphics API to the index into 'file_hash_maps_'.
  int ApiToIndex(common::GraphicsApi graphics_api) const;

  // Maps the source file name to hash values of source and compiled files.
  absl::flat_hash_map<std::string, CompilationHash> file_hash_maps_[kNumApis];
};

}  // namespace lighter::shader

#endif  // LIGHTER_SHADER_COMPILATION_RECORD_H
