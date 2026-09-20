#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <string>
#include <fstream>
#include <vector>
#include <android/log.h>

#define LOG_TAG "Towner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

enum Material : uint8_t {
    MAT_COLOR = 0,      // чистый цвет
    MAT_BRICK,          // 1
    MAT_STONE,          // 2
    MAT_GLASS,          // 3
    MAT_WOOD,           // 4
    MAT_METAL,          // 5
    MAT_PLANKS,         // 6
    MAT_NANOBLOCK,      // 7
    MAT_TILE,           // 8
    MAT_GRANITE,        // 9
    MAT_SAND,           // 10
    MAT_LIMESTONE,      // 11
    MAT_FABRIC,         // 12
    MAT_GOLD,           // 13
    MAT_NANOBLOCK2 = 14,
    MAT_MONOBLOCK = 15

};

struct Cell {
    bool occupied = false;
    uint8_t material = MAT_COLOR;
    glm::vec3 color = glm::vec3(1.0f);
};

struct Action {
    enum Type { PLACE, REMOVE } type;
    int x, y, z;
    uint8_t material;
    glm::vec3 color;
};

class Grid {
public:
    std::unordered_map<int, Cell> cells;
    std::vector<Action> undoStack;
    std::vector<Action> redoStack;

    static int key(int x, int y, int z) {
        return (x & 0x3FF) | ((y & 0x3FF) << 10) | ((z & 0x3FF) << 20);
    }

    bool isOccupied(int x, int y, int z) const {
        auto it = cells.find(key(x, y, z));
        return it != cells.end() && it->second.occupied;
    }

    void recordPlace(int x, int y, int z, uint8_t mat, glm::vec3 color) {
        undoStack.push_back({Action::PLACE, x, y, z, mat, color});
        redoStack.clear();
    }

    void recordRemove(int x, int y, int z, uint8_t mat, glm::vec3 color) {
        undoStack.push_back({Action::REMOVE, x, y, z, mat, color});
        redoStack.clear();
    }

    void place(int x, int y, int z, uint8_t mat, glm::vec3 color, bool record = true) {
        if (record) recordPlace(x, y, z, mat, color);
        cells[key(x, y, z)] = {true, mat, color};
    }

    void remove(int x, int y, int z, bool record = true) {
        auto it = cells.find(key(x, y, z));
        if (it == cells.end()) return;
        if (record) recordRemove(x, y, z, it->second.material, it->second.color);
        cells.erase(it);
    }

    bool undo() {
        if (undoStack.empty()) return false;
        Action a = undoStack.back();
        undoStack.pop_back();
        redoStack.push_back(a);

        if (a.type == Action::PLACE) {
            cells.erase(key(a.x, a.y, a.z));
        } else {
            cells[key(a.x, a.y, a.z)] = {true, a.material, a.color};
        }
        return true;
    }

    bool redo() {
        if (redoStack.empty()) return false;
        Action a = redoStack.back();
        redoStack.pop_back();
        undoStack.push_back(a);

        if (a.type == Action::PLACE) {
            cells[key(a.x, a.y, a.z)] = {true, a.material, a.color};
        } else {
            cells.erase(key(a.x, a.y, a.z));
        }
        return true;
    }

    int count() const {
        int n = 0;
        for (auto& p : cells) if (p.second.occupied) n++;
        return n;
    }

    void clear() {
        cells.clear();
        undoStack.clear();
        redoStack.clear();
    }

    bool save(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;

        int count = 0;
        for (auto& p : cells) if (p.second.occupied) count++;
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (auto& p : cells) {
            if (!p.second.occupied) continue;
            int k = p.first;
            out.write(reinterpret_cast<const char*>(&k), sizeof(k));
            out.write(reinterpret_cast<const char*>(&p.second.material), sizeof(uint8_t));
            out.write(reinterpret_cast<const char*>(&p.second.color.x), sizeof(float));
            out.write(reinterpret_cast<const char*>(&p.second.color.y), sizeof(float));
            out.write(reinterpret_cast<const char*>(&p.second.color.z), sizeof(float));
        }
        return true;
    }

    bool load(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;

        cells.clear();
        undoStack.clear();
        redoStack.clear();

        int count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));

        for (int i = 0; i < count; i++) {
            int k = 0;
            uint8_t mat = 0;
            float r, g, b;
            in.read(reinterpret_cast<char*>(&k), sizeof(k));
            in.read(reinterpret_cast<char*>(&mat), sizeof(uint8_t));
            in.read(reinterpret_cast<char*>(&r), sizeof(float));
            in.read(reinterpret_cast<char*>(&g), sizeof(float));
            in.read(reinterpret_cast<char*>(&b), sizeof(float));
            cells[k] = {true, mat, glm::vec3(r, g, b)};
        }
        LOGI("Loaded %d blocks", count);
        return true;
    }
};