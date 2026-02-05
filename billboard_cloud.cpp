#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// Structure simple pour un point 2D
struct Point2D {
    float x;
    float y;
};

// Voxel 3D (un "board" devient un box 3D)
struct Box {
    int x0;
    int y0;
    int z0;
    int x1;
    int y1;
    int z1;
};

// Test de point dans un polygone (algorithme du rayon)
static bool pointInPolygon(const std::vector<Point2D> &poly, float x, float y) {
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point2D &pi = poly[i];
        const Point2D &pj = poly[j];
        bool intersect = ((pi.y > y) != (pj.y > y)) &&
                         (x < (pj.x - pi.x) * (y - pi.y) / (pj.y - pi.y + 1e-6f) + pi.x);
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
}

// Rasterisation simple : transforme un polygone 2D en grille binaire
static std::vector<std::vector<int>> rasterizePolygon(
    const std::vector<Point2D> &poly,
    int gridW,
    int gridH,
    float minX,
    float minY,
    float cellSize) {

    std::vector<std::vector<int>> grid(gridH, std::vector<int>(gridW, 0));
    for (int y = 0; y < gridH; ++y) {
        for (int x = 0; x < gridW; ++x) {
            float px = minX + (x + 0.5f) * cellSize;
            float py = minY + (y + 0.5f) * cellSize;
            if (pointInPolygon(poly, px, py)) {
                grid[y][x] = 1;
            }
        }
    }
    return grid;
}

// Recherche greedy de boites 3D maximales pour couvrir les voxels pleins
static std::vector<Box> buildBoxes(std::vector<std::vector<std::vector<int>>> &grid) {
    int d = static_cast<int>(grid.size());
    int h = static_cast<int>(grid[0].size());
    int w = static_cast<int>(grid[0][0].size());
    std::vector<Box> boxes;

    for (int z = 0; z < d; ++z) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (grid[z][y][x] == 0) {
                    continue;
                }

                // Extension en X
                int maxW = 0;
                while (x + maxW < w && grid[z][y][x + maxW] == 1) {
                    maxW++;
                }

                // Extension en Y
                int maxH = 1;
                bool canGrowY = true;
                while (y + maxH < h && canGrowY) {
                    for (int dx = 0; dx < maxW; ++dx) {
                        if (grid[z][y + maxH][x + dx] == 0) {
                            canGrowY = false;
                            break;
                        }
                    }
                    if (canGrowY) {
                        maxH++;
                    }
                }

                // Extension en Z
                int maxD = 1;
                bool canGrowZ = true;
                while (z + maxD < d && canGrowZ) {
                    for (int yy = 0; yy < maxH; ++yy) {
                        for (int xx = 0; xx < maxW; ++xx) {
                            if (grid[z + maxD][y + yy][x + xx] == 0) {
                                canGrowZ = false;
                                break;
                            }
                        }
                        if (!canGrowZ) {
                            break;
                        }
                    }
                    if (canGrowZ) {
                        maxD++;
                    }
                }

                // Enregistrer la box
                boxes.push_back(Box{x, y, z, x + maxW, y + maxH, z + maxD});

                // Marquer les voxels couverts comme vides
                for (int zz = z; zz < z + maxD; ++zz) {
                    for (int yy = y; yy < y + maxH; ++yy) {
                        for (int xx = x; xx < x + maxW; ++xx) {
                            grid[zz][yy][xx] = 0;
                        }
                    }
                }
            }
        }
    }

    return boxes;
}

int main(int argc, char **argv) {
    // L'algorithme est 3D : on lit des coupes 2D empilees.
    // Format du fichier (exemple dans test_shape.txt) :
    // cellSize
    // numSlices
    // zIndex numPoints
    // x0 y0
    // x1 y1
    // ...
    // (repeter pour chaque coupe)
    // zIndex est un entier, les coupes sont placees a z = zIndex * cellSize.

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <fichier_forme>\n";
        return 1;
    }

    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "Impossible d'ouvrir le fichier.\n";
        return 1;
    }

    float cellSize = 1.0f;
    int numSlices = 0;
    input >> cellSize;
    input >> numSlices;
    if (!input || cellSize <= 0.0f || numSlices <= 0) {
        std::cerr << "Parametres invalides.\n";
        return 1;
    }

    struct Slice {
        int zIndex;
        std::vector<Point2D> poly;
    };

    std::vector<Slice> slices;
    slices.reserve(numSlices);

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    int minZ = std::numeric_limits<int>::max();
    int maxZ = std::numeric_limits<int>::lowest();

    for (int s = 0; s < numSlices; ++s) {
        int zIndex = 0;
        int n = 0;
        input >> zIndex >> n;
        if (!input || n < 3) {
            std::cerr << "Coupe invalide.\n";
            return 1;
        }
        Slice slice;
        slice.zIndex = zIndex;
        slice.poly.resize(n);
        for (int i = 0; i < n; ++i) {
            input >> slice.poly[i].x >> slice.poly[i].y;
            minX = std::min(minX, slice.poly[i].x);
            minY = std::min(minY, slice.poly[i].y);
            maxX = std::max(maxX, slice.poly[i].x);
            maxY = std::max(maxY, slice.poly[i].y);
        }
        minZ = std::min(minZ, zIndex);
        maxZ = std::max(maxZ, zIndex);
        slices.push_back(slice);
    }

    int gridW = static_cast<int>(std::ceil((maxX - minX) / cellSize));
    int gridH = static_cast<int>(std::ceil((maxY - minY) / cellSize));
    int gridD = maxZ - minZ + 1;

    if (gridW <= 0 || gridH <= 0 || gridD <= 0) {
        std::cerr << "Dimensions de grille invalides.\n";
        return 1;
    }

    std::vector<std::vector<std::vector<int>>> grid(
        gridD, std::vector<std::vector<int>>(gridH, std::vector<int>(gridW, 0)));

    for (const auto &slice : slices) {
        int z = slice.zIndex - minZ;
        auto sliceGrid = rasterizePolygon(slice.poly, gridW, gridH, minX, minY, cellSize);
        for (int y = 0; y < gridH; ++y) {
            for (int x = 0; x < gridW; ++x) {
                grid[z][y][x] = sliceGrid[y][x];
            }
        }
    }

    auto boxes = buildBoxes(grid);

    std::cout << "Nombre de boxes: " << boxes.size() << "\n";
    for (size_t i = 0; i < boxes.size(); ++i) {
        const Box &b = boxes[i];
        float bx0 = minX + b.x0 * cellSize;
        float by0 = minY + b.y0 * cellSize;
        float bz0 = (minZ + b.z0) * cellSize;
        float bx1 = minX + b.x1 * cellSize;
        float by1 = minY + b.y1 * cellSize;
        float bz1 = (minZ + b.z1) * cellSize;
        std::cout << "Box " << i << ": (" << bx0 << ", " << by0 << ", " << bz0
                  << ") -> (" << bx1 << ", " << by1 << ", " << bz1 << ")\n";
    }

    return 0;
}
