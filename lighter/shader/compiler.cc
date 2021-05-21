//
//  compiler.cc
//
//  Created by Pujun Lun on 5/17/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader/compiler.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "lighter/common/graphics_api.h"
#include "lighter/common/util.h"
#include "lighter/shader/compilation_record.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"
#include "third_party/picosha2/picosha2.h"
#include "third_party/shaderc/shaderc.hpp"
#include "compilation_record.h"

namespace lighter::shader::compiler {
namespace {

namespace stdfs = std::filesystem;
using common::GraphicsApi;
using FileHash = CompilationRecordHandler::FileHash;

bool IsShaderFile(const stdfs::path& path) {
  static const auto* shader_file_extension_set =
      new absl::flat_hash_set<stdfs::path, common::util::PathHash>{
          {".vert"},
          {".frag"},
          {".comp"},
      };
  return stdfs::is_regular_file(path) &&
         shader_file_extension_set->contains(path.extension());
}

stdfs::path GetCompiledFilePath(GraphicsApi graphics_api,
                                       const stdfs::path& source_path) {
  static const char* compiled_file_extension = ".spv";
  stdfs::path res{common::GetApiAbbreviatedName(graphics_api)};
  res /= stdfs::relative(source_path);
  res += compiled_file_extension;
  return res;
}

std::vector<char>& GetHashBuffer() {
  static auto* buffer = new std::vector<char>(picosha2::k_digest_size);
  return *buffer;
}

std::string ComputeFileSha256(const stdfs::path& path) {
  std::ifstream file{path, std::ios::in | std::ios::binary};
  ASSERT_TRUE(file, absl::StrFormat("Failed to open shader file %s",
                                    stdfs::absolute(path).string()));
  std::vector<char>& buffer = GetHashBuffer();
  picosha2::hash256(file, buffer.begin(), buffer.end());
  return picosha2::bytes_to_hex_string(buffer);
}

std::optional<std::string> NeedsCompilation(
    const CompilationRecordReader& record_reader, GraphicsApi graphics_api,
    const stdfs::path& source_path) {
  const stdfs::path compiled_path =
      GetCompiledFilePath(graphics_api, source_path);
  if (!stdfs::exists(compiled_path)) {
    return "compiled file does not exist";
  }

  const FileHash* file_hash =
      record_reader.GetFileHash(graphics_api, source_path);
  if (file_hash == nullptr) {
    return "no compilation record";
  }
  if (ComputeFileSha256(source_path) != file_hash->source_file_hash) {
    return "source file hash mismatch";
  }
  if (ComputeFileSha256(compiled_path) != file_hash->compiled_file_hash) {
    return "compiled file hash mismatch";
  }

  return std::nullopt;
}

std::vector<uint32_t> CompileFile(const stdfs::path& path) {
  return {};
}

}  // namespace

void Compile(const stdfs::path& shader_dir) {
  ASSERT_TRUE(stdfs::is_directory(shader_dir),
              absl::StrFormat("%s is not a valid directory",
                              shader_dir.string()));
  stdfs::current_path(shader_dir);

  const stdfs::path current_dir = stdfs::path(".");
  auto [record_reader, record_writer] =
      CompilationRecordHandler::CreateHandlers(current_dir);

  for (const stdfs::directory_entry& entry :
           stdfs::recursive_directory_iterator(current_dir)) {
    const stdfs::path& path = entry.path();
    if (!IsShaderFile(path)) {
      continue;
    }
    LOG_INFO << absl::StreamFormat("Found shader file %s",
                                   stdfs::absolute(path).string());

    for (GraphicsApi api : common::GetAllApis()) {
      const std::optional<std::string> reason =
          NeedsCompilation(record_reader, api, path);
      if (reason.has_value()) {
        LOG_INFO << absl::StreamFormat("\tNeed to compile for %s: %s",
                                       common::GetApiFullName(api),
                                       reason.value());
      }
    }
  }
}

}  // namespace lighter::shader::compiler
