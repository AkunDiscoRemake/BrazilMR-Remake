// BrazilMR — utilidades GL (logs, shader program, texturas, FBOs, VAOs).
#pragma once

#include "GlHeaders.hpp"
#include "../core/Log.hpp"
#include "../math/MathTypes.hpp"
#include <string>
#include <vector>

namespace brazilmr {

// ---------------------------------------------------------------------------
// Shader program
// ---------------------------------------------------------------------------
class Shader {
public:
    Shader() = default;
    ~Shader() { destroy(); }

    bool build(const char* vsSrc, const char* fsSrc);
    void destroy();

    void use() const { glUseProgram(program_); }
    GLuint program() const { return program_; }
    bool valid() const { return program_ != 0; }

    GLint uniform(const char* name) const;
    GLint attrib(const char* name) const;

    void setMat4(const char* name, const Mat4& m) const {
        GLint loc = uniform(name);
        if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, m.m);
    }
    void setVec3(const char* name, const Vec3& v) const {
        GLint loc = uniform(name);
        if (loc >= 0) glUniform3f(loc, v.x, v.y, v.z);
    }
    void setVec2(const char* name, const Vec2& v) const {
        GLint loc = uniform(name);
        if (loc >= 0) glUniform2f(loc, v.x, v.y);
    }
    void setFloat(const char* name, float v) const {
        GLint loc = uniform(name);
        if (loc >= 0) glUniform1f(loc, v);
    }
    void setInt(const char* name, int v) const {
        GLint loc = uniform(name);
        if (loc >= 0) glUniform1i(loc, v);
    }
    void setVec4(const char* name, float x, float y, float z, float w) const {
        GLint loc = uniform(name);
        if (loc >= 0) glUniform4f(loc, x, y, z, w);
    }

private:
    GLuint program_ = 0;
};

// ---------------------------------------------------------------------------
// Textura 2D
// ---------------------------------------------------------------------------
class Texture {
public:
    Texture() = default;
    ~Texture() { destroy(); }

    // RGBA8888
    bool allocateRgba(int width, int height, const uint8_t* pixels = nullptr,
                      bool mipmaps = false);
    // single channel (atlas de máscara, luminância)
    bool allocateLuma(int width, int height, const uint8_t* pixels = nullptr);
    // atualização parcial (streaming de superfícies)
    void updateRgba(const uint8_t* pixels, int x, int y, int w, int h);
    void bind(int unit) const;
    void destroy();

    GLuint id() const { return id_; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Wrap externo (não destrói no destrutor).
    void adoptExternal(GLuint id) { id_ = id; owned_ = false; }

private:
    GLuint id_ = 0;
    int width_ = 0, height_ = 0;
    bool owned_ = true;
};

// ---------------------------------------------------------------------------
// Framebuffer (color RGBA + depth16/24)
// ---------------------------------------------------------------------------
class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer() { destroy(); }

    bool create(int width, int height, bool withDepth = true, GLint msaa = 0);
    void destroy();
    bool resize(int width, int height);

    void bind() const;
    static void bindDefault(int w, int h);

    GLuint framebuffer() const { return fbo_; }
    GLuint colorTexture() const { return color_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const { return fbo_ != 0; }

private:
    GLuint fbo_ = 0, color_ = 0, depth_ = 0;
    int width_ = 0, height_ = 0;
    bool withDepth_ = true;
};

// ---------------------------------------------------------------------------
// Mesh — VAO com atributos declarados
// ---------------------------------------------------------------------------
struct VertexAttrib {
    int size = 3;          // componentes
    GLenum type = GL_FLOAT;
    bool normalized = false;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh() { destroy(); }

    // @param vertexSize  bytes por vértice
    // @param attrs       layout dos atributos em ordem
    bool upload(const void* vertices, std::size_t vertexCount,
                std::size_t vertexSize, const std::vector<VertexAttrib>& attrs,
                const void* indices = nullptr, std::size_t indexCount = 0,
                GLenum indexType = GL_UNSIGNED_SHORT, GLenum usage = GL_STATIC_DRAW);
    void destroy();

    void draw(GLenum mode = GL_TRIANGLES) const;
    void drawRange(GLenum mode, std::size_t first, std::size_t count) const;

    std::size_t vertexCount() const { return vertexCount_; }
    std::size_t indexCount() const { return indexCount_; }

private:
    GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
    std::size_t vertexCount_ = 0, indexCount_ = 0;
    GLenum indexType_ = GL_UNSIGNED_SHORT;
};

} // namespace brazilmr
