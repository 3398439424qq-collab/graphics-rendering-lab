#pragma once
#include <array>
#include <cstdint>
#include <vector>

// Codex整理已审声明；CpuMesh按值成员依据本人草案。
// CPU-only data. Each index addresses one complete MeshVertex record.
struct MeshVertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 2> uv{};
};

struct CpuMesh {
    std::vector<MeshVertex> vertices{};      // Owns vertex records.
    std::vector<std::uint32_t> indices{};    // Owns vertex-record indices.
};
