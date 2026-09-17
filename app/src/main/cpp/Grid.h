#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <string>
#include <fstream>
#include <android/log.h>

#define LOG_TAG "Towner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct Cell {
    bool occupied = false;
    glm::vec3 color = glm::vec3(1.0f);
};

class Grid {
public:
    std::unordered_map<int, Cell> cells;

    static int key(int x, int y, int z) {
        return (x & 0x3FF) | ((y & 0x3FF) << 10) | ((z & 0x3FF) << 20);
    }

    bool isOccupied(int x, int y, int z) const {
        auto it = cells.find(key(x, y, z));
        return it != cells.end() && it->second.occupied;
    }

    void place(int x, int y, int z, glm::vec3 color = glm::vec3(1.0f)) {
        cells[key(x, y, z)] = {true, color};
    }

    void remove(int x, int y, int z) {
        cells.erase(key(x, y, z));
    }

    int count() const {
        int n = 0;
        for (auto& p : cells) if (p.second.occupied) n++;
        return n;
    }

    void clear() {
        cells.clear();
    }

    // ---------- Сохранение ----------
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
            out.write(reinterpret_cast<const char*>(&p.second.color.x), sizeof(float));
            out.write(reinterpret_cast<const char*>(&p.second.color.y), sizeof(float));
            out.write(reinterpret_cast<const char*>(&p.second.color.z), sizeof(float));
        }
        return true;
    }

    // ---------- Загрузка ----------
    bool load(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;

        cells.clear();
        int count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));

        for (int i = 0; i < count; i++) {
            int k = 0;
            float r, g, b;
            in.read(reinterpret_cast<char*>(&k), sizeof(k));
            in.read(reinterpret_cast<char*>(&r), sizeof(float));
            in.read(reinterpret_cast<char*>(&g), sizeof(float));
            in.read(reinterpret_cast<char*>(&b), sizeof(float));
            cells[k] = {true, glm::vec3(r, g, b)};
        }
        LOGI("Loaded %d blocks from %s", count, path.c_str());
        return true;
    }
};