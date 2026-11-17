#pragma once
#include "mesh_data.h"
#include <filesystem>
#include <string>

// Codex组装的已审CPU接口；实际读取/转换函数体由本人实现。
struct MeshLoadResult {
    bool success = false;
    CpuMesh mesh{};
    std::string error{};
};

// Borrow input only for this call; returned data/error are owned values.
// Success: complete nonempty triangle mesh, error empty.
// Failure: success=false, both mesh vectors empty, error nonempty.
// Convert every shape/triangle/corner in order, without deduplication.
// Fixed tinyobj configuration: triangulate=false, reject all warnings.
// Require valid position/normal/UV references and finite referenced values.
MeshLoadResult parse_obj_mesh(const std::string& obj_text);

// Read only the named OBJ file, then delegate to parse_obj_mesh.
// Ordinary open/read failure follows the same empty-result contract.
MeshLoadResult load_obj_mesh(const std::filesystem::path& path);
