//
//  compilation_record.cc
//
//  Created by Pujun Lun on 5/13/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader_compiler/compilation_record.h"

#include <algorithm>
#include <exception>
#include <format>
#include <optional>
#include <vector>

namespace lighter::shader_compiler {
namespace {

namespace stdfs = std::filesystem;

constexpr char kRecordFileName[] = ".compilation_record";

}  // namespace

std::pair<CompilationRecordReader, CompilationRecordWriter>
CompilationRecordHandler::CreateHandlers(
    const std::filesystem::path& shader_dir, OptimizationLevel opt_level) {
  stdfs::path record_file_path = shader_dir / kRecordFileName;
  if (stdfs::exists(record_file_path) &&
      !stdfs::is_regular_file(record_file_path)) {
    FATAL(std::format("{} exists, but is not a regular file",
                      stdfs::absolute(record_file_path).string()));
  }

  CompilationRecordReader reader{record_file_path, opt_level};
  CompilationRecordWriter writer{std::move(record_file_path), opt_level};
  return {std::move(reader), std::move(writer)};
}

CompilationRecordReader::CompilationRecordReader(
    const stdfs::path& record_file_path, OptimizationLevel opt_level) {
  if (!stdfs::exists(record_file_path)) {
    LOG_INFO << "No compilation record file found";
    return;
  }
  std::ifstream record_file{record_file_path, std::ios::in | std::ios::binary};
  ASSERT_TRUE(record_file,
              std::format("Failed to open '{}'", record_file_path.string()));
  ParseRecordFile(record_file, opt_level);
}

void CompilationRecordReader::ParseRecordFile(std::ifstream& record_file,
                                              OptimizationLevel opt_level) {
  enum RecordSegmentIndex {
    kSourceFilePathIndex = 0,
    kSourceFileHashIndex,
    kCompiledFileHashIndex,
    kNumSegments,
  };

  std::string line;
  int line_num = 1;
  try {
    for (; std::getline(record_file, line); ++line_num) {
      if (line_num == 1) {
        const std::optional<OptimizationLevel> opt_level_in_record =
            util::OptLevelFromText(line);
        ASSERT_HAS_VALUE(opt_level_in_record,
                         "Unrecognized optimization level");
        if (opt_level_in_record.value() != opt_level) {
          LOG_INFO << "Optimization level has changed, discarding old records";
          break;
        } else {
          continue;
        }
      }

      std::vector<std::string> segments = common::util::StrSplit(line, ' ');
      if (segments.size() != kNumSegments) {
        LOG_INFO << "Compilation record format has changed, discarding old "
                    "records";
        file_hash_map_.clear();
        break;
      }

      stdfs::path source_file_path{std::move(segments[kSourceFilePathIndex])};
      ASSERT_FALSE(file_hash_map_.contains(source_file_path),
                   "Duplicated entry");

      file_hash_map_.insert({
          std::move(source_file_path),
          FileHash{
              std::move(segments[kSourceFileHashIndex]),
              std::move(segments[kCompiledFileHashIndex]),
          },
      });
    }
  } catch (const std::exception& e) {
    FATAL(std::format("Failed to parse line {}: {}\n{}", line_num, line,
                      e.what()));
  }
}

const CompilationRecordHandler::FileHash* CompilationRecordReader::GetFileHash(
    const stdfs::path& source_file_path) const {
  ASSERT_TRUE(source_file_path.is_relative(),
              "Source file path is assumed to be a relative path");
  const auto iter = file_hash_map_.find(source_file_path);
  return iter != file_hash_map_.end() ? &iter->second : nullptr;
}

void CompilationRecordWriter::RegisterFileHash(stdfs::path&& source_file_path,
                                               FileHash&& file_hash) {
  ASSERT_FALSE(
      file_hash_map_.contains(source_file_path),
      std::format("Duplicated entry for '{}'", source_file_path.string()));
  file_hash_map_.insert({std::move(source_file_path), std::move(file_hash)});
}

void CompilationRecordWriter::WriteAll() const {
  std::ofstream record_file{record_file_path_,
                            std::ios::out | std::ios::binary | std::ios::trunc};
  ASSERT_TRUE(record_file,
              std::format("Failed to open '{}'", record_file_path_.string()));

  // Write header.
  record_file << std::format("{}\n", util::OptLevelToText(opt_level_));

  // Write body.
  for (const auto& [source_file_path, file_hash] : file_hash_map_) {
    record_file << std::format("{} {} {}\n", source_file_path.string(),
                               file_hash.source_file_hash,
                               file_hash.compiled_file_hash);
  }
}

}  // namespace lighter::shader_compiler
