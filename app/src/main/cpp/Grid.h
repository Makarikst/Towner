#pragma once
#include <glm/glm.hpp>
#include <unordered_map>

struct Cell {
    bool occupied = false;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
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
};