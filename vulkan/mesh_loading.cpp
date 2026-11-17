#include "mesh_loading.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <utility>

// Codex固定依赖接入；实现宏只在这一个翻译单元定义。
#define TINYOBJLOADER_IMPLEMENTATION
#include "third_party/tinyobjloader/tiny_obj_loader.h"


MeshLoadResult load_obj_mesh(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {false, {}, "file: 打开失败: " + path.u8string()};
    }

    std::string text;
    std::string line;
    while (std::getline(input, line)) {
        text += line;
        text += '\n';
    }

    if (input.bad() || !input.eof()) {
        return {false, {}, "file: 读取失败: " + path.u8string()};
    }

    return parse_obj_mesh(text);
}

MeshLoadResult parse_obj_mesh(const std::string& obj_text)
{
    tinyobj::ObjReaderConfig cfg{};
    cfg.triangulate = false;
    tinyobj::ObjReader reader{};
    const bool parsed = reader.ParseFromString(obj_text, "", cfg);

    const std::string& error = reader.Error();
    const std::string& warning = reader.Warning();
    if (!parsed || !warning.empty()) {
        std::string message;
        if (!error.empty()) {
            message = error;
        } else if (!warning.empty()) {
            message = warning;
        } else {
            message = "obj: 解析失败，库未提供详细信息";
        }
        return {false, {}, std::move(message)};
    }
    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    CpuMesh candidate{};

    for (std::size_t s = 0; s < shapes.size(); ++s) {
        const auto& shape = shapes[s];
        if (!shape.lines.indices.empty() || !shape.points.indices.empty()) {
            return {false, {},
                    "obj: shape " + std::to_string(s)
                        + " 含线段或点数据，不支持"};
        }

        std::size_t offset = 0;
        for (std::size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            const int n = static_cast<int>(shape.mesh.num_face_vertices[f]);
            if (n != 3) {
                return {false, {},
                        "obj: shape " + std::to_string(s)
                            + " face " + std::to_string(f)
                            + " 不是三角面"};
            }

            for (int c = 0; c < n; ++c) {
                const std::size_t corner =
                    offset + static_cast<std::size_t>(c);

                if (corner >= shape.mesh.indices.size()) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " 角记录越界"};
                }

                const auto& index = shape.mesh.indices[corner];

                if (index.vertex_index < 0) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " vertex_index 为负数"};
                }
                const std::size_t p =
                    static_cast<std::size_t>(index.vertex_index);
                if (p >= attrib.vertices.size() / 3) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " vertex_index 越界"};
                }

                if (index.normal_index < 0) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " normal_index 为负数"};
                }
                const std::size_t nidx =
                    static_cast<std::size_t>(index.normal_index);
                if (nidx >= attrib.normals.size() / 3) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " normal_index 越界"};
                }

                if (index.texcoord_index < 0) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " texcoord_index 为负数"};
                }
                const std::size_t t =
                    static_cast<std::size_t>(index.texcoord_index);
                if (t >= attrib.texcoords.size() / 2) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " texcoord_index 越界"};
                }

                const float px = attrib.vertices[3 * p];
                const float py = attrib.vertices[3 * p + 1];
                const float pz = attrib.vertices[3 * p + 2];

                const float nx = attrib.normals[3 * nidx];
                const float ny = attrib.normals[3 * nidx + 1];
                const float nz = attrib.normals[3 * nidx + 2];

                const float u = attrib.texcoords[2 * t];
                const float v = attrib.texcoords[2 * t + 1];

                if (!(std::isfinite(px) && std::isfinite(py) &&
                      std::isfinite(pz) && std::isfinite(nx) &&
                      std::isfinite(ny) && std::isfinite(nz) &&
                      std::isfinite(u) && std::isfinite(v))) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " 属性含非有限值"};
                }

                if (candidate.indices.size() >=
                    static_cast<std::size_t>(
                        std::numeric_limits<std::uint32_t>::max())) {
                    return {false, {},
                            "obj: shape " + std::to_string(s)
                                + " face " + std::to_string(f)
                                + " corner " + std::to_string(c)
                                + " 顶点索引数量超过 uint32_t 上限"};
                }

                MeshVertex vertex{};
                vertex.position = {px, py, pz};
                vertex.normal = {nx, ny, nz};
                vertex.uv = {u, v};

                const std::size_t new_index = candidate.vertices.size();
                candidate.vertices.push_back(vertex);
                candidate.indices.push_back(
                    static_cast<std::uint32_t>(new_index));
            }

            offset += static_cast<std::size_t>(n);
        }
    }

    if (candidate.indices.empty()) {
        return {false, {}, "obj: 没有三角面"};
    }

    return {true, std::move(candidate), {}};
}
