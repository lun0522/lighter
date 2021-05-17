//
//  compilation_record.cc
//
//  Created by Pujun Lun on 5/13/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader/compilation_record.h"

#include <algorithm>
#include <array>
#include <exception>
#include <optional>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/shader/util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/str_split.h"

namespace lighter::shader {
namespace {

namespace stdfs = std::filesystem;
using common::GraphicsApi;

constexpr char kRecordFileName[] = ".compilation_record";

// Returns the path to compilation record file. This file must either not exist
// or be a regular file.
stdfs::path GetRecordFilePath(std::string_view shader_dir) {
  stdfs::path path = stdfs::path{shader_dir} / kRecordFileName;
  if (stdfs::exists(path) && !stdfs::is_regular_file(path)) {
    FATAL(absl::StrFormat("%s exists, but is not a regular file",
                          path.string()));
  }
  return path;
}

}  // namespace

CompilationRecordReader::CompilationRecordReader(std::string_view shader_dir) {
  const stdfs::path record_file_path = GetRecordFilePath(shader_dir);
  if (!stdfs::exists(record_file_path)) {
    return;
  }
  std::ifstream record_file{record_file_path, std::ios::in | std::ios::binary};
  ASSERT_FALSE(
      !record_file.is_open() || record_file.bad() || record_file.fail(),
      absl::StrFormat("Failed to open %s", record_file_path.string()));
  ParseRecordFile(record_file);
}

void CompilationRecordReader::ParseRecordFile(std::ifstream& record_file) {
  enum RecordSegmentIndex {
    kGraphicsApiIndex = 0,
    kSourceFilePathIndex,
    kSourceFileSha256Index,
    kCompiledFileSha256Index,
    kNumSegments,
  };

  std::string line;
  int line_num = 1;
  try {
    for (; std::getline(record_file, line); ++line_num) {
      std::vector<std::string> segments = absl::StrSplit(line, ' ');
      ASSERT_TRUE(segments.size() == kNumSegments,
                  absl::StrFormat("Expected %d segments, get %d",
                                  kNumSegments, segments.size()));

      const std::string& api_abbreviation = segments[kGraphicsApiIndex];
      auto& api_specific_map = file_hash_maps_[ApiToIndex(api_abbreviation)];
      std::string& source_file_path = segments[kSourceFilePathIndex];
      ASSERT_FALSE(api_specific_map.contains(source_file_path),
                   "Duplicated entry");

      api_specific_map.insert({
          std::move(source_file_path),
          CompilationHash{
              std::move(segments[kSourceFileSha256Index]),
              std::move(segments[kCompiledFileSha256Index]),
          },
      });
    }
  } catch (const std::exception& e) {
    FATAL(absl::StrFormat("Failed to parse line %d: %s\n%s",
                          line_num, line, e.what()));
  }
}

int CompilationRecordReader::ApiToIndex(std::string_view abbreviation) const {
  using ApiAbbreviationArray = std::array<std::string, kNumApis>;
  static const ApiAbbreviationArray* api_abbreviations = nullptr;
  if (api_abbreviations == nullptr) {
    auto* abbreviations = new ApiAbbreviationArray{};
    (*abbreviations)[kOpenglIndex] =
        util::GetApiNameAbbreviation(GraphicsApi::kOpengl);
    (*abbreviations)[kVulkanIndex] =
        util::GetApiNameAbbreviation(GraphicsApi::kVulkan);
    api_abbreviations = abbreviations;
  }

  const std::optional<int> index = common::util::FindIndexOfFirst(
      absl::MakeSpan(*api_abbreviations), abbreviation);
  ASSERT_HAS_VALUE(
      index, absl::StrFormat("Unrecognized graphics API '%s'", abbreviation));
  return index.value();
}

int CompilationRecordReader::ApiToIndex(GraphicsApi graphics_api) const {
  switch (graphics_api) {
    case GraphicsApi::kOpengl:
      return kOpenglIndex;
    case GraphicsApi::kVulkan:
      return kVulkanIndex;
  }
}

const CompilationHash* CompilationRecordReader::GetCompilationHash(
    std::string_view source_file_path, GraphicsApi graphics_api) const {
  ASSERT_TRUE(stdfs::path{source_file_path}.is_relative(),
              "Source file path is assumed to be a relative path");
  const auto& api_specific_map = file_hash_maps_[ApiToIndex(graphics_api)];
  const auto iter = api_specific_map.find(source_file_path);
  return iter != api_specific_map.end() ? &iter->second : nullptr;
}

}  // namespace lighter::shader
