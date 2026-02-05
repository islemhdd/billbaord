#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Voxel 3D (un "board" devient un box 3D)
struct Box {
    int x0;
    int y0;
    int z0;
    int x1;
    int y1;
    int z1;
};

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

// Dessin simple en mode fil de fer pour une box
static void drawBoxWire(const Box &b, float cellSize) {
    float x0 = b.x0 * cellSize;
    float y0 = b.y0 * cellSize;
    float z0 = b.z0 * cellSize;
    float x1 = b.x1 * cellSize;
    float y1 = b.y1 * cellSize;
    float z1 = b.z1 * cellSize;

    // 12 aretes d'un parallelepipede
    glBegin(GL_LINES);
    // bas
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y0, z0);
    // haut
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z1); glVertex3f(x0, y0, z1);
    // montants
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1);
    glEnd();
}

// Visualisation OpenGL simple
static void visualizeBoxes(const std::vector<Box> &boxes, float cellSize, int gridW, int gridH, int gridD) {
    if (!glfwInit()) {
        std::cerr << "Erreur GLFW.\n";
        return;
    }

    GLFWwindow *window = glfwCreateWindow(800, 600, "Billboard Cloud 3D", nullptr, nullptr);
    if (!window) {
        std::cerr << "Impossible de creer la fenetre.\n";
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Erreur GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    float maxDim = std::max(std::max(gridW, gridH), gridD) * cellSize;

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = width > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        glOrtho(-maxDim, maxDim * aspect, -maxDim, maxDim, -maxDim * 2.0f, maxDim * 2.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(25.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(35.0f, 0.0f, 1.0f, 0.0f);

        glColor3f(0.8f, 0.9f, 0.2f);
        for (const auto &box : boxes) {
            drawBoxWire(box, cellSize);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}

int main(int argc, char **argv) {
    // L'algorithme est 3D : on lit un objet 3D discretise en voxels.
    // Format du fichier (exemple dans test_shape.txt) :
    // cellSize
    // gridW gridH gridD
    // numVoxels
    // x y z
    // x y z
    // ...
    // Chaque triplet (x,y,z) est l'indice d'un voxel rempli.
    // Les indices commencent a 0.

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
    int gridW = 0;
    int gridH = 0;
    int gridD = 0;
    input >> cellSize;
    input >> gridW >> gridH >> gridD;
    if (!input || cellSize <= 0.0f || gridW <= 0 || gridH <= 0 || gridD <= 0) {
        std::cerr << "Parametres invalides.\n";
        return 1;
    }

    std::vector<std::vector<std::vector<int>>> grid(
        gridD, std::vector<std::vector<int>>(gridH, std::vector<int>(gridW, 0)));

    int numVoxels = 0;
    input >> numVoxels;
    if (!input || numVoxels <= 0) {
        std::cerr << "Nombre de voxels invalide.\n";
        return 1;
    }

    for (int i = 0; i < numVoxels; ++i) {
        int vx = 0;
        int vy = 0;
        int vz = 0;
        input >> vx >> vy >> vz;
        if (!input || vx < 0 || vy < 0 || vz < 0 || vx >= gridW || vy >= gridH || vz >= gridD) {
            std::cerr << "Voxel invalide.\n";
            return 1;
        }
        grid[vz][vy][vx] = 1;
    }

    auto boxes = buildBoxes(grid);

    std::cout << "Nombre de boxes: " << boxes.size() << "\n";
    for (size_t i = 0; i < boxes.size(); ++i) {
        const Box &b = boxes[i];
        float bx0 = b.x0 * cellSize;
        float by0 = b.y0 * cellSize;
        float bz0 = b.z0 * cellSize;
        float bx1 = b.x1 * cellSize;
        float by1 = b.y1 * cellSize;
        float bz1 = b.z1 * cellSize;
        std::cout << "Box " << i << ": (" << bx0 << ", " << by0 << ", " << bz0
                  << ") -> (" << bx1 << ", " << by1 << ", " << bz1 << ")\n";
    }

    // Visualisation simple en OpenGL (fenetre interactive)
    visualizeBoxes(boxes, cellSize, gridW, gridH, gridD);

    return 0;
}
