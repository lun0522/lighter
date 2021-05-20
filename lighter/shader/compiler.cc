//
//  compiler.cc
//
//  Created by Pujun Lun on 5/17/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader/compiler.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "lighter/common/graphics_api.h"
#include "lighter/common/util.h"
#include "lighter/shader/compilation_record.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/types/span.h"

namespace lighter::shader::compiler {
namespace {

namespace stdfs = std::filesystem;
using common::GraphicsApi;

std::string ComputeSha256(absl::Span<const char> data) {
 return "";
}

std::vector<uint32_t> CompileFile(const stdfs::path& path) {
  return {};
}

}  // namespace

void Compile(const stdfs::path& shader_dir) {
  static const auto* graphics_apis = new std::array<GraphicsApi, 2>{
      GraphicsApi::kOpengl,
      GraphicsApi::kVulkan,
  };

  static const auto* file_extension_set =
      new absl::flat_hash_set<stdfs::path, common::util::PathHash>{
          {".vert"},
          {".frag"},
          {".comp"},
      };

  auto [reader, writer] = CompilationRecordHandler::CreateHandlers(shader_dir);

  for (const stdfs::directory_entry& entry :
           stdfs::recursive_directory_iterator(shader_dir)) {
    const stdfs::path& path = entry.path();
    if (stdfs::is_regular_file(path)) {
      if (file_extension_set->contains(path.extension())) {
        LOG_INFO << "Found file: " << stdfs::relative(path, shader_dir);
      }
    }
  }
}

}  // namespace lighter::shader::compiler
