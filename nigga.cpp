// glb_tree_viewer.cpp
// This code does: loads a real tree from tree.glb and renders its triangle mesh on screen.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <limits>

// ---- tinygltf ----
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf-release/tiny_gltf.h"

// ------------------ SHADERS ------------------
static const char *vsSrc = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 MVP;
void main() {
    gl_Position = MVP * vec4(aPos, 1.0);
}
)glsl";

static const char *fsSrc = R"glsl(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.2, 0.8, 0.3, 1.0);
}
)glsl";

// ------------------ GL UTILS ------------------
static GLuint compileShader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << "Shader compile error:\n"
                  << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint makeProgram()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs)
        return 0;

    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::cerr << "Program link error:\n"
                  << log << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// ------------------ SIMPLE MATH ------------------
struct Mat4
{
    float m[16];
};

static Mat4 identity()
{
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static Mat4 perspective(float fRad, float aspect, float zNear, float zFar)
{
    Mat4 r{};
    float t = 1.0f / std::tan(fRad * 0.5f);
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return r;
}

static Mat4 translate(float x, float y, float z)
{
    Mat4 r = identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

// Column-major multiply: r = a*b
static Mat4 mul(const Mat4 &a, const Mat4 &b)
{
    Mat4 r{};
    for (int c = 0; c < 4; c++)
    {
        for (int rr = 0; rr < 4; rr++)
        {
            r.m[c * 4 + rr] =
                a.m[0 * 4 + rr] * b.m[c * 4 + 0] +
                a.m[1 * 4 + rr] * b.m[c * 4 + 1] +
                a.m[2 * 4 + rr] * b.m[c * 4 + 2] +
                a.m[3 * 4 + rr] * b.m[c * 4 + 3];
        }
    }
    return r;
}

// ------------------ tinygltf helpers ------------------
static const unsigned char *accessorDataPtr(
    const tinygltf::Model &model,
    const tinygltf::Accessor &acc)
{
    const tinygltf::BufferView &view = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer &buf = model.buffers[view.buffer];
    return buf.data.data() + view.byteOffset + acc.byteOffset;
}

static int componentCount(int type)
{
    // POSITION is usually VEC3
    switch (type)
    {
    case TINYGLTF_TYPE_SCALAR:
        return 1;
    case TINYGLTF_TYPE_VEC2:
        return 2;
    case TINYGLTF_TYPE_VEC3:
        return 3;
    case TINYGLTF_TYPE_VEC4:
        return 4;
    default:
        return 0;
    }
}

struct Bounds
{
    float minx, miny, minz;
    float maxx, maxy, maxz;
};

static Bounds computeBoundsFromPositions(const float *pos, size_t vertexCount, size_t strideFloats)
{
    Bounds b;
    b.minx = b.miny = b.minz = std::numeric_limits<float>::infinity();
    b.maxx = b.maxy = b.maxz = -std::numeric_limits<float>::infinity();

    for (size_t i = 0; i < vertexCount; i++)
    {
        const float *p = pos + i * strideFloats;
        b.minx = std::min(b.minx, p[0]);
        b.maxx = std::max(b.maxx, p[0]);
        b.miny = std::min(b.miny, p[1]);
        b.maxy = std::max(b.maxy, p[1]);
        b.minz = std::min(b.minz, p[2]);
        b.maxz = std::max(b.maxz, p[2]);
    }
    return b;
}

int main()
{
    // ---- GLFW / GL init ----
    if (!glfwInit())
    {
        std::cerr << "glfwInit failed\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const int W = 800, H = 600;
    GLFWwindow *win = glfwCreateWindow(W, H, "GLB Tree", nullptr, nullptr);
    if (!win)
    {
        std::cerr << "glfwCreateWindow failed\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "gladLoadGLLoader failed\n";
        glfwTerminate();
        return 1;
    }

    glViewport(0, 0, W, H);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // avoid winding/handedness surprises
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // uncomment to debug wireframe

    GLuint prog = makeProgram();
    if (!prog)
    {
        glfwTerminate();
        return 1;
    }

    // ---- LOAD GLB ----
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    if (!loader.LoadBinaryFromFile(&model, &err, &warn, "tree.glb"))
    {
        std::cerr << "Failed to load tree.glb\n";
        if (!warn.empty())
            std::cerr << "WARN: " << warn << "\n";
        if (!err.empty())
            std::cerr << "ERR : " << err << "\n";
        glfwTerminate();
        return 1;
    }
    if (!warn.empty())
        std::cerr << "WARN: " << warn << "\n";

    if (model.meshes.empty())
    {
        std::cerr << "No meshes in GLB\n";
        glfwTerminate();
        return 1;
    }

    // We take the first primitive of the first mesh (good enough for a quick viewer)
    const tinygltf::Mesh &mesh = model.meshes[0];
    if (mesh.primitives.empty())
    {
        std::cerr << "Mesh has no primitives\n";
        glfwTerminate();
        return 1;
    }
    const tinygltf::Primitive &prim = mesh.primitives[0];

    if (prim.attributes.find("POSITION") == prim.attributes.end())
    {
        std::cerr << "Primitive has no POSITION\n";
        glfwTerminate();
        return 1;
    }
    if (prim.indices < 0)
    {
        std::cerr << "Primitive has no indices (not handled in this minimal viewer)\n";
        glfwTerminate();
        return 1;
    }

    // ---- Positions accessor ----
    const tinygltf::Accessor &posAcc = model.accessors[prim.attributes.at("POSITION")];
    const tinygltf::BufferView &posView = model.bufferViews[posAcc.bufferView];

    if (posAcc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
    {
        std::cerr << "POSITION is not float (unsupported in this minimal viewer)\n";
        glfwTerminate();
        return 1;
    }
    if (posAcc.type != TINYGLTF_TYPE_VEC3)
    {
        std::cerr << "POSITION is not VEC3 (unsupported)\n";
        glfwTerminate();
        return 1;
    }

    const unsigned char *posBase = accessorDataPtr(model, posAcc);

    // Stride: if byteStride == 0 then tightly packed
    size_t posStrideBytes = posView.byteStride ? posView.byteStride : sizeof(float) * 3;
    size_t posStrideFloats = posStrideBytes / sizeof(float);

    // We'll copy positions into a tight array to keep it simple/robust.
    std::vector<float> positions;
    positions.resize(posAcc.count * 3);

    for (size_t i = 0; i < posAcc.count; i++)
    {
        const float *p = reinterpret_cast<const float *>(posBase + i * posStrideBytes);
        positions[i * 3 + 0] = p[0];
        positions[i * 3 + 1] = p[1];
        positions[i * 3 + 2] = p[2];
    }

    // ---- Indices accessor (THIS WAS YOUR BUG) ----
    const tinygltf::Accessor &idxAcc = model.accessors[prim.indices];
    const tinygltf::BufferView &idxView = model.bufferViews[idxAcc.bufferView];
    const unsigned char *idxBase = accessorDataPtr(model, idxAcc);

    std::vector<uint32_t> indices;
    indices.reserve(idxAcc.count);

    if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        const uint16_t *p = reinterpret_cast<const uint16_t *>(idxBase);
        for (size_t i = 0; i < idxAcc.count; i++)
            indices.push_back((uint32_t)p[i]);
    }
    else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
    {
        const uint32_t *p = reinterpret_cast<const uint32_t *>(idxBase);
        for (size_t i = 0; i < idxAcc.count; i++)
            indices.push_back(p[i]);
    }
    else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
    {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(idxBase);
        for (size_t i = 0; i < idxAcc.count; i++)
            indices.push_back((uint32_t)p[i]);
    }
    else
    {
        std::cerr << "Unsupported index componentType: " << idxAcc.componentType << "\n";
        glfwTerminate();
        return 1;
    }

    // ---- Build draw vertex array (expanded triangles) ----
    // We expand indexed triangles into a flat position array to avoid EBO complexity.
    // Each 3 floats = 1 vertex position. 3 vertices per triangle.
    std::vector<float> drawVerts;
    drawVerts.reserve(indices.size() * 3);

    for (uint32_t id : indices)
    {
        if (id >= (uint32_t)posAcc.count)
            continue; // safety
        drawVerts.push_back(positions[id * 3 + 0]);
        drawVerts.push_back(positions[id * 3 + 1]);
        drawVerts.push_back(positions[id * 3 + 2]);
    }

    if (drawVerts.empty())
    {
        std::cerr << "No vertices to draw (check GLB)\n";
        glfwTerminate();
        return 1;
    }

    // ---- Auto-fit camera (prevents black screen from scale) ----
    Bounds b = computeBoundsFromPositions(positions.data(), posAcc.count, 3);
    float cx = 0.5f * (b.minx + b.maxx);
    float cy = 0.5f * (b.miny + b.maxy);
    float cz = 0.5f * (b.minz + b.maxz);
    float dx = (b.maxx - b.minx);
    float dy = (b.maxy - b.miny);
    float dz = (b.maxz - b.minz);
    float radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
    if (radius < 1e-6f)
        radius = 1.0f;

    // Camera distance: a bit more than radius so it fits
    float camDist = radius * 2.5f;

    // ---- GPU upload ----
    GLuint VAO = 0, VBO = 0;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(drawVerts.size() * sizeof(float)),
                 drawVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glBindVertexArray(0);

    GLint uMVP = glGetUniformLocation(prog, "MVP");

    // ---- Main loop ----
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        glClearColor(0.05f, 0.05f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // projection
        Mat4 P = perspective(60.0f * 3.1415926f / 180.0f, (float)W / (float)H, 0.01f, camDist * 20.0f);

        // "view": translate scene so center is at origin, then move camera back.
        // We don't implement full lookAt here; just center + z translation.
        Mat4 centerToOrigin = translate(-cx, -cy, -cz);
        Mat4 cameraBack = translate(0.0f, 0.0f, -camDist);
        Mat4 V = mul(cameraBack, centerToOrigin);

        Mat4 MVP = mul(P, V);

        glUseProgram(prog);
        glUniformMatrix4fv(uMVP, 1, GL_FALSE, MVP.m);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(drawVerts.size() / 3));
        glBindVertexArray(0);

        glfwSwapBuffers(win);

        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(win, 1);
        }
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(prog);

    glfwTerminate();
    return 0;
}
