#include "GlUtils.hpp"
#include <cstring>

namespace brazilmr {

const char* glErrorString(GLenum err) {
    switch (err) {
        case GL_NO_ERROR: return "NO_ERROR";
        case 0x0500: return "INVALID_ENUM";
        case 0x0501: return "INVALID_VALUE";
        case 0x0502: return "INVALID_OPERATION";
        case 0x0503: return "STACK_OVERFLOW";
        case 0x0504: return "STACK_UNDERFLOW";
        case 0x0505: return "OUT_OF_MEMORY";
        case 0x0506: return "INVALID_FRAMEBUFFER_OPERATION";
        default: return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Shader
// ---------------------------------------------------------------------------
static GLuint compileStage(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    const GLchar* srcs[1] = {src};
    GLint lens[1] = {static_cast<GLint>(std::strlen(src))};
    glShaderSource(shader, 1, srcs, lens);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(shader, sizeof(log) - 1, &len, log);
        log[len] = '\0';
        BMR_LOGE("shader compile error: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::build(const char* vsSrc, const char* fsSrc) {
    destroy();
    GLuint vs = compileStage(GL_VERTEX_SHADER, vsSrc);
    if (!vs) return false;
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsSrc);
    if (!fs) { glDeleteShader(vs); return false; }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetProgramInfoLog(p, sizeof(log) - 1, &len, log);
        log[len] = '\0';
        BMR_LOGE("program link error: %s", log);
        glDeleteProgram(p);
        return false;
    }
    program_ = p;
    return true;
}

void Shader::destroy() {
    if (program_) { glDeleteProgram(program_); program_ = 0; }
}

GLint Shader::uniform(const char* name) const {
    return glGetUniformLocation(program_, name);
}

GLint Shader::attrib(const char* name) const {
    return glGetAttribLocation(program_, name);
}

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------
bool Texture::allocateRgba(int width, int height, const uint8_t* pixels,
                           bool mipmaps) {
    destroy();
    width_ = width; height_ = height;
    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    return id_ != 0;
}

bool Texture::allocateLuma(int width, int height, const uint8_t* pixels) {
    destroy();
    width_ = width; height_ = height;
    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, pixels);
    return id_ != 0;
}

void Texture::updateRgba(const uint8_t* pixels, int x, int y, int w, int h) {
    if (!id_) return;
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void Texture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::destroy() {
    if (id_ && owned_) glDeleteTextures(1, &id_);
    id_ = 0;
}

// ---------------------------------------------------------------------------
// Framebuffer
// ---------------------------------------------------------------------------
bool Framebuffer::create(int width, int height, bool withDepth, GLint msaa) {
    (void)msaa; // MSAA via FBO multissample fica como extensão futura
    destroy();
    width_ = width; height_ = height; withDepth_ = withDepth;

    glGenTextures(1, &color_);
    glBindTexture(GL_TEXTURE_2D, color_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color_, 0);

    if (withDepth) {
        glGenRenderbuffers(1, &depth_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depth_);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        BMR_LOGE("FBO incompleto (0x%x)", status);
        destroy();
        return false;
    }
    return true;
}

void Framebuffer::destroy() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (color_) glDeleteTextures(1, &color_);
    if (depth_) glDeleteRenderbuffers(1, &depth_);
    fbo_ = color_ = depth_ = 0;
    width_ = height_ = 0;
}

bool Framebuffer::resize(int width, int height) {
    if (width == width_ && height == height_) return true;
    return create(width, height, withDepth_);
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
}

void Framebuffer::bindDefault(int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
}

// ---------------------------------------------------------------------------
// Mesh
// ---------------------------------------------------------------------------
bool Mesh::upload(const void* vertices, std::size_t vertexCount,
                  std::size_t vertexSize, const std::vector<VertexAttrib>& attrs,
                  const void* indices, std::size_t indexCount, GLenum indexType,
                  GLenum usage) {
    destroy();
    vertexCount_ = vertexCount;
    indexCount_ = indexCount;
    indexType_ = indexType;

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertexSize * vertexCount, vertices, usage);

    std::size_t offset = 0;
    for (std::size_t i = 0; i < attrs.size(); ++i) {
        glEnableVertexAttribArray(static_cast<GLuint>(i));
        glVertexAttribPointer(static_cast<GLuint>(i), attrs[i].size, attrs[i].type,
                              attrs[i].normalized ? GL_TRUE : GL_FALSE,
                              static_cast<GLsizei>(vertexSize),
                              reinterpret_cast<const void*>(offset));
        offset += attrs[i].size * (attrs[i].type == GL_FLOAT ? 4 : 2);
    }

    if (indices && indexCount) {
        glGenBuffers(1, &ibo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indexCount * (indexType == GL_UNSIGNED_INT ? 4 : 2),
                     indices, usage);
    }
    glBindVertexArray(0);
    return vao_ != 0;
}

void Mesh::destroy() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
    vao_ = vbo_ = ibo_ = 0;
    vertexCount_ = indexCount_ = 0;
}

void Mesh::draw(GLenum mode) const {
    if (!vao_) return;
    glBindVertexArray(vao_);
    if (indexCount_ > 0) {
        glDrawElements(mode, static_cast<GLsizei>(indexCount_), indexType_, nullptr);
    } else {
        glDrawArrays(mode, 0, static_cast<GLsizei>(vertexCount_));
    }
    glBindVertexArray(0);
}

void Mesh::drawRange(GLenum mode, std::size_t first, std::size_t count) const {
    if (!vao_) return;
    glBindVertexArray(vao_);
    if (indexCount_ > 0) {
        glDrawElements(mode, static_cast<GLsizei>(count), indexType_,
                       reinterpret_cast<const void*>(first *
                           (indexType_ == GL_UNSIGNED_INT ? 4 : 2)));
    } else {
        glDrawArrays(mode, static_cast<GLsizei>(first), static_cast<GLsizei>(count));
    }
    glBindVertexArray(0);
}

} // namespace brazilmr
