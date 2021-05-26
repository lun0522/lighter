//
//  compilation_record.cc
//
//  Created by Pujun Lun on 5/13/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader/compilation_record.h"

#include <algorithm>
#include <exception>
#include <optional>
#include <vector>

#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/str_split.h"

namespace lighter::shader {
namespace {

namespace stdfs = std::filesystem;
using common::api::GraphicsApi;

constexpr char kRecordFileName[] = ".compilation_record";

}  // namespace

std::pair<CompilationRecordReader, CompilationRecordWriter>
CompilationRecordHandler::CreateHandlers(
    const std::filesystem::path& shader_dir) {
  stdfs::path record_file_path = shader_dir / kRecordFileName;
  if (stdfs::exists(record_file_path) &&
      !stdfs::is_regular_file(record_file_path)) {
    FATAL(absl::StrFormat("%s exists, but is not a regular file",
                          stdfs::absolute(record_file_path).string()));
  }

  CompilationRecordReader reader{record_file_path};
  CompilationRecordWriter writer{std::move(record_file_path)};
  return {std::move(reader), std::move(writer)};
}

const CompilationRecordHandler::ApiAbbreviationArray&
CompilationRecordHandler::GetApiAbbreviations() {
  static const ApiAbbreviationArray* api_abbreviations = nullptr;
  if (api_abbreviations == nullptr) {
    auto* abbreviations = new ApiAbbreviationArray{};
    (*abbreviations)[kOpenglIndex] =
        common::api::GetApiAbbreviatedName(GraphicsApi::kOpengl);
    (*abbreviations)[kVulkanIndex] =
        common::api::GetApiAbbreviatedName(GraphicsApi::kVulkan);
    api_abbreviations = abbreviations;
  }
  return *api_abbreviations;
}

int CompilationRecordHandler::ApiToIndex(GraphicsApi graphics_api) {
  switch (graphics_api) {
    case GraphicsApi::kOpengl:
      return kOpenglIndex;
    case GraphicsApi::kVulkan:
      return kVulkanIndex;
  }
}

CompilationRecordReader::CompilationRecordReader(
    const stdfs::path& record_file_path) {
  if (!stdfs::exists(record_file_path)) {
    return;
  }
  std::ifstream record_file{record_file_path, std::ios::in | std::ios::binary};
  ASSERT_TRUE(record_file,
              absl::StrFormat("Failed to open %s", record_file_path.string()));
  ParseRecordFile(record_file);
}

void CompilationRecordReader::ParseRecordFile(std::ifstream& record_file) {
  enum RecordSegmentIndex {
    kGraphicsApiIndex = 0,
    kSourceFilePathIndex,
    kSourceFileHashIndex,
    kCompiledFileHashIndex,
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
      auto& api_specific_map =
          file_hash_maps_[ApiAbbreviationToIndex(api_abbreviation)];
      stdfs::path source_file_path{std::move(segments[kSourceFilePathIndex])};
      ASSERT_FALSE(api_specific_map.contains(source_file_path),
                   "Duplicated entry");

      api_specific_map.insert({
          std::move(source_file_path),
          FileHash{
              std::move(segments[kSourceFileHashIndex]),
              std::move(segments[kCompiledFileHashIndex]),
          },
      });
    }
  } catch (const std::exception& e) {
    FATAL(absl::StrFormat("Failed to parse line %d: %s\n%s",
                          line_num, line, e.what()));
  }
}

int CompilationRecordReader::ApiAbbreviationToIndex(
    std::string_view abbreviation) const {
  const std::optional<int> index = common::util::FindIndexOfFirst(
      absl::MakeSpan(GetApiAbbreviations()), abbreviation);
  ASSERT_HAS_VALUE(
      index, absl::StrFormat("Unrecognized graphics API '%s'", abbreviation));
  return index.value();
}

const CompilationRecordHandler::FileHash* CompilationRecordReader::GetFileHash(
    GraphicsApi graphics_api, const stdfs::path& source_file_path) const {
  ASSERT_TRUE(source_file_path.is_relative(),
              "Source file path is assumed to be a relative path");
  const auto& api_specific_map = file_hash_maps_[ApiToIndex(graphics_api)];
  const auto iter = api_specific_map.find(source_file_path);
  return iter != api_specific_map.end() ? &iter->second : nullptr;
}

void CompilationRecordWriter::RegisterFileHash(GraphicsApi graphics_api,
                                               stdfs::path&& source_file_path,
                                               FileHash&& file_hash) {
  auto& api_specific_map = file_hash_maps_[ApiToIndex(graphics_api)];
  ASSERT_FALSE(api_specific_map.contains(source_file_path),
               absl::StrFormat("%s: Duplicated entry for %s",
                               GetApiAbbreviations()[ApiToIndex(graphics_api)],
                               source_file_path.string()));
  api_specific_map.insert({std::move(source_file_path), std::move(file_hash)});
}

void CompilationRecordWriter::WriteAll() const {
  std::ofstream record_file{record_file_path_, std::ios::out | std::ios::trunc};
  ASSERT_TRUE(record_file,
              absl::StrFormat("Failed to open %s", record_file_path_.string()));

  for (int api_index = 0; api_index < kNumApis; ++api_index) {
    const std::string& api_abbreviation = GetApiAbbreviations()[api_index];
    const auto& api_specific_map = file_hash_maps_[api_index];
    for (const auto& [source_file_path, file_hash] : api_specific_map) {
      record_file << absl::StreamFormat(
          "%s %s %s %s\n",
          api_abbreviation, source_file_path.string(),
          file_hash.source_file_hash, file_hash.compiled_file_hash);
    }
  }
}

}  // namespace lighter::shader
