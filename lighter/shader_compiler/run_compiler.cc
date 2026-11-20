//
//  run_compiler.cc
//
//  Created by Pujun Lun on 5/17/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader_compiler/run_compiler.h"

#include <array>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "lighter/common/data.h"
#include "lighter/common/file.h"
#include "lighter/common/timer.h"
#include "lighter/common/util.h"
#include "lighter/shader_compiler/compilation_record.h"
#include "lighter/shader_compiler/compiler.h"
#include <absl/strings/str_format.h>
#include <absl/types/span.h>
#include <picosha2.h>

namespace lighter::shader_compiler::compiler {
namespace {

namespace stdfs = std::filesystem;

using FileHash = CompilationRecordHandler::FileHash;

// Returns the SHA256 of the file at 'path'.
std::string ComputeFileSha256(const stdfs::path& path) {
  std::ifstream file{path, std::ios::in | std::ios::binary};
  ASSERT_TRUE(file, absl::StrFormat("Failed to open file '%s'",
                                    stdfs::absolute(path).string()));
  static auto* buffer = new std::array<unsigned char, picosha2::k_digest_size>;
  picosha2::hash256(file, buffer->begin(), buffer->end());
  return picosha2::bytes_to_hex_string(*buffer);
}

// Returns the SHA256 of 'data'.
std::string ComputeDataSha256(absl::Span<const char> data) {
  return picosha2::hash256_hex_string(data);
}

// Helper class to config compiler options and invoke the compiler.
class CompilerRunner {
 public:
  CompilerRunner(std::filesystem::path&& shader_dir,
                 OptimizationLevel opt_level);

  // This class is neither copyable nor movable.
  CompilerRunner(const CompilerRunner&) = delete;
  CompilerRunner& operator=(const CompilerRunner&) = delete;

  // Compiles all shader files.
  void Run();

 private:
  // Returns the reason why the file needs to be compiled, or std::nullopt if
  // not needed.
  std::optional<std::string> NeedsCompilation(
      const stdfs::path& source_path) const;

  // Returns file hash values. The shader will not be compiled if the hash
  // values of existing files match the existing compilation record.
  FileHash CompileIfNeeded(const stdfs::path& source_path,
                           shaderc_shader_kind shader_kind) const;

  // Accessors.
  const CompilationRecordReader& record_reader() const {
    return record_handlers_.first;
  }
  CompilationRecordWriter& record_writer() { return record_handlers_.second; }

  const std::filesystem::path shader_dir_;
  std::pair<CompilationRecordReader, CompilationRecordWriter> record_handlers_;
  const Compiler compiler_;
  CompilerOptions options_;
};

CompilerRunner::CompilerRunner(std::filesystem::path&& shader_dir,
                               OptimizationLevel opt_level)
    : shader_dir_{std::move(shader_dir)},
      record_handlers_{
          CompilationRecordHandler::CreateHandlers(shader_dir_, opt_level)} {
  options_.SetOptimizationLevel(opt_level);
}

void CompilerRunner::Run() {
  stdfs::current_path(shader_dir_);
  for (const stdfs::directory_entry& entry :
           stdfs::recursive_directory_iterator(".")) {
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

    record_writer().RegisterFileHash(
        stdfs::path{path}, CompileIfNeeded(path, shader_kind.value()));
  }
  CompilationRecordWriter::WriteAll(std::move(record_writer()));
}

std::optional<std::string> CompilerRunner::NeedsCompilation(
    const stdfs::path& source_path) const {
  const stdfs::path compiled_path = util::GetShaderBinaryPath(source_path);
  if (!stdfs::exists(compiled_path)) {
    return "compiled file does not exist";
  }

  const FileHash* file_hash = record_reader().GetFileHash(source_path);
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

FileHash CompilerRunner::CompileIfNeeded(
    const stdfs::path& source_path, shaderc_shader_kind shader_kind) const {
  const std::optional<std::string> reason = NeedsCompilation(source_path);
  if (!reason.has_value()) {
    LOG_INFO << "\tSkip compilation";
    return *record_reader().GetFileHash(source_path);
  }
  LOG_INFO << absl::StreamFormat("\tNeed to compile: %s", reason.value());

  // Compile shader.
  const common::Data source_data =
      common::file::LoadDataFromFile(source_path.string());
  const auto source_data_span = absl::MakeSpan(source_data.data<char>(),
                                               source_data.size());
  const std::unique_ptr<CompilationResult> result = compiler_.Compile(
      /*shader_tag=*/source_path.filename().string(), shader_kind,
      source_data_span, options_);
  const auto result_data_span = result->GetDataSpan();

  // Write shader binary to disk.
  const stdfs::path compiled_path = util::GetShaderBinaryPath(source_path);
  stdfs::create_directories(compiled_path.parent_path());
  std::ofstream file{compiled_path,
                     std::ios::out | std::ios::binary | std::ios::trunc};
  ASSERT_TRUE(file, absl::StrFormat("Failed to open file '%s'",
                                    compiled_path.string()));
  file.write(result_data_span.data(), result_data_span.size());

  return FileHash {
      ComputeDataSha256(source_data_span),
      ComputeDataSha256(result_data_span),
  };
}

}  // namespace

void CompileShaders(stdfs::path&& shader_dir, OptimizationLevel opt_level) {
  LOG_INFO << "Compiling shaders...";

  const common::BasicTimer timer;
  CompilerRunner runner{std::move(shader_dir), opt_level};
  runner.Run();
  const float elapsed_time = timer.GetElapsedTimeSinceLaunch();

  LOG_INFO << absl::StreamFormat("Finished in %fs", elapsed_time);
}

}  // namespace lighter::shader_compiler::compiler
