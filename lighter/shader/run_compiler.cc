//
//  run_compiler.cc
//
//  Created by Pujun Lun on 5/17/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader/run_compiler.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include "lighter/common/file.h"
#include "lighter/common/graphics_api.h"
#include "lighter/common/util.h"
#include "lighter/shader/compilation_record.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"
#include "third_party/picosha2/picosha2.h"
#include "compilation_record.h"
#include "../common/file.h"

namespace lighter::shader::compiler {
namespace {

namespace stdfs = std::filesystem;

using common::api::GraphicsApi;
using FileHash = CompilationRecordHandler::FileHash;

constexpr int kNumSupportedApis = common::api::kNumSupportedApis;

// Returns the API specific macro that is used for shader compilation.
const char* GetTargetMacro(GraphicsApi graphics_api) {
  switch (graphics_api) {
    case GraphicsApi::kOpengl:
      return "TARGET_OPENGL";
    case GraphicsApi::kVulkan:
      return "TARGET_VULKAN";
  }
}

// Returns the path to output binary if compiling for 'graphics_api'.
stdfs::path GetCompiledFilePath(GraphicsApi graphics_api,
                                const stdfs::path& source_path) {
  stdfs::path res{common::api::GetApiAbbreviatedName(graphics_api)};
  res /= stdfs::relative(source_path);
  res += common::api::kSpirvBinaryFileExtension;
  return res;
}

// Returns the SHA256 of the file at 'path'.
std::string ComputeFileSha256(const stdfs::path& path) {
  std::ifstream file{path, std::ios::in | std::ios::binary};
  ASSERT_TRUE(file, absl::StrFormat("Failed to open file '%s'",
                                    stdfs::absolute(path).string()));
  static auto* buffer = new std::array<char, picosha2::k_digest_size>;
  picosha2::hash256(file, buffer->begin(), buffer->end());
  return picosha2::bytes_to_hex_string(*buffer);
}

// Returns the SHA256 of 'data'.
std::string ComputeDataSha256(absl::Span<const char> data) {
  return picosha2::hash256_hex_string(data);
}

// Returns the reason why the file needs to be compiled for 'graphics_api', or
// std::nullopt if no need.
std::optional<std::string> NeedsCompilation(
    GraphicsApi graphics_api, const stdfs::path& source_path,
    const CompilationRecordReader& record_reader) {
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

}  // namespace

void CompileShaders(const stdfs::path& shader_dir,
                    CompilerOptions::OptimizationLevel opt_level) {
  ASSERT_TRUE(stdfs::is_directory(shader_dir),
              absl::StrFormat("%s is not a valid directory",
                              shader_dir.string()));
  stdfs::current_path(shader_dir);

  const stdfs::path current_dir = stdfs::path(".");
  auto [record_reader, record_writer] =
      CompilationRecordHandler::CreateHandlers(current_dir);

  const std::array<GraphicsApi, kNumSupportedApis> all_apis =
      common::api::GetAllApis();
  const Compiler compiler{};
  std::array<std::unique_ptr<CompilerOptions>, kNumSupportedApis>
      options_array{};
  for (int i = 0; i < all_apis.size(); ++i) {
    options_array[i] = std::make_unique<CompilerOptions>();
    (*options_array[i])
        .SetOptimizationLevel(opt_level)
        .AddMacroDefinition(GetTargetMacro(all_apis[i]));
  }

  for (const stdfs::directory_entry& entry :
           stdfs::recursive_directory_iterator(current_dir)) {
    const stdfs::path& path = entry.path();
    if (!stdfs::is_regular_file(path)) {
      continue;
    }
    const std::optional<shaderc_shader_kind> shader_kind =
        Compiler::GetShaderKind(path.extension().string());
    if (!shader_kind.has_value()) {
      continue;
    }
    LOG_INFO << absl::StreamFormat("Found shader file '%s'",
                                   stdfs::absolute(path).string());

    for (int i = 0; i < all_apis.size(); ++i) {
      const GraphicsApi api = all_apis[i];
      const std::optional<std::string> reason =
          NeedsCompilation(api, path, record_reader);
      const char* api_name = common::api::GetApiFullName(api);
      if (!reason.has_value()) {
        LOG_INFO << absl::StreamFormat("\tSkip compilation for %s", api_name);
        continue;
      }

      LOG_INFO << absl::StreamFormat("\tNeed to compile for %s: %s",
                                     api_name, reason.value());

      const common::RawData source_data{path.string()};
      const std::unique_ptr<CompilationResult> result = compiler.Compile(
          /*shader_tag=*/path.filename().string(), shader_kind.value(),
          source_data.GetSpan(), *options_array[i]);

      const stdfs::path compiled_path = GetCompiledFilePath(api, path);
      stdfs::create_directories(compiled_path.parent_path());

      std::ofstream file{compiled_path, std::ios::out | std::ios::trunc};
      ASSERT_TRUE(file, absl::StrFormat("Failed to open file '%s'",
                                        compiled_path.string()));
      const auto result_data_span = result->GetDataSpan();
      file.write(result_data_span.data(), result_data_span.size());

      record_writer.RegisterFileHash(
          api, path.string(),
          CompilationRecordHandler::FileHash {
              ComputeDataSha256(source_data.GetSpan()),
              ComputeDataSha256(result_data_span),
          });
    }
  }

  CompilationRecordWriter::WriteAll(std::move(record_writer));
  LOG_INFO << "Done!";
}

}  // namespace lighter::shader::compiler
