// HOST-ONLY — implementações no-op de GLES3 para LINKAR o motor nativo
// nos testes de host (nenhum pixel é desenhado; lógica pura é testada).
#include "GLES3/gl3.h"

extern "C" void glActiveTexture(GLenum texture) { (void)0;  }
extern "C" void glAttachShader(GLuint program, GLuint shader) { (void)0;  }
extern "C" void glBindAttribLocation(GLuint program, GLuint index, const GLchar * name) { (void)0;  }
extern "C" void glBindBuffer(GLenum target, GLuint buffer) { (void)0;  }
extern "C" void glBindFramebuffer(GLenum target, GLuint framebuffer) { (void)0;  }
extern "C" void glBindRenderbuffer(GLenum target, GLuint renderbuffer) { (void)0;  }
extern "C" void glBindTexture(GLenum target, GLuint texture) { (void)0;  }
extern "C" void glBindVertexArray(GLuint array) { (void)0;  }
extern "C" void glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) { (void)0;  }
extern "C" void glBlendEquation(GLenum mode) { (void)0;  }
extern "C" void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) { (void)0;  }
extern "C" void glBlendFunc(GLenum sfactor, GLenum dfactor) { (void)0;  }
extern "C" void glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) { (void)0;  }
extern "C" void glBufferData(GLenum target, GLsizeiptr size, const void * data, GLenum usage) { (void)0;  }
extern "C" void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void * data) { (void)0;  }
extern "C" GLenum glCheckFramebufferStatus(GLenum target) { (void)0; return 0; }
extern "C" void glClear(GLbitfield mask) { (void)0;  }
extern "C" void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) { (void)0;  }
extern "C" void glClearDepthf(GLclampf depth) { (void)0;  }
extern "C" void glClearStencil(GLint s) { (void)0;  }
extern "C" void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) { (void)0;  }
extern "C" void glCompileShader(GLuint shader) { (void)0;  }
extern "C" void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * data) { (void)0;  }
extern "C" void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) { (void)0;  }
extern "C" GLuint glCreateProgram() { (void)0; return 0; }
extern "C" GLuint glCreateShader(GLenum type) { (void)0; return 0; }
extern "C" void glCullFace(GLenum mode) { (void)0;  }
extern "C" void glDeleteBuffers(GLsizei n, const GLuint * buffers) { (void)0;  }
extern "C" void glDeleteFramebuffers(GLsizei n, const GLuint * framebuffers) { (void)0;  }
extern "C" void glDeleteProgram(GLuint program) { (void)0;  }
extern "C" void glDeleteRenderbuffers(GLsizei n, const GLuint * renderbuffers) { (void)0;  }
extern "C" void glDeleteShader(GLuint shader) { (void)0;  }
extern "C" void glDeleteTextures(GLsizei n, const GLuint * textures) { (void)0;  }
extern "C" void glDeleteVertexArrays(GLsizei n, const GLuint * arrays) { (void)0;  }
extern "C" void glDepthFunc(GLenum func) { (void)0;  }
extern "C" void glDepthMask(GLboolean flag) { (void)0;  }
extern "C" void glDepthRangef(GLclampf n, GLclampf f) { (void)0;  }
extern "C" void glDetachShader(GLuint program, GLuint shader) { (void)0;  }
extern "C" void glDisable(GLenum cap) { (void)0;  }
extern "C" void glDisableVertexAttribArray(GLuint index) { (void)0;  }
extern "C" void glDrawArrays(GLenum mode, GLint first, GLsizei count) { (void)0;  }
extern "C" void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) { (void)0;  }
extern "C" void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) { (void)0;  }
extern "C" void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount) { (void)0;  }
extern "C" void glEnable(GLenum cap) { (void)0;  }
extern "C" void glEnableVertexAttribArray(GLuint index) { (void)0;  }
extern "C" void glFinish() { (void)0;  }
extern "C" void glFlush() { (void)0;  }
extern "C" void glFrontFace(GLenum mode) { (void)0;  }
extern "C" void glGenBuffers(GLsizei n, GLuint * buffers) { (void)0;  }
extern "C" void glGenerateMipmap(GLenum target) { (void)0;  }
extern "C" void glGenFramebuffers(GLsizei n, GLuint * framebuffers) { (void)0;  }
extern "C" void glGenRenderbuffers(GLsizei n, GLuint * renderbuffers) { (void)0;  }
extern "C" void glGenTextures(GLsizei n, GLuint * textures) { (void)0;  }
extern "C" void glGenVertexArrays(GLsizei n, GLuint * arrays) { (void)0;  }
extern "C" void glGetBooleanv(GLenum pname, GLboolean * params) { (void)0;  }
extern "C" GLenum glGetError() { (void)0; return 0; }
extern "C" void glGetFloatv(GLenum pname, GLfloat * params) { (void)0;  }
extern "C" void glGetIntegerv(GLenum pname, GLint * params) { (void)0;  }
extern "C" void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog) { (void)0;  }
extern "C" void glGetProgramiv(GLuint program, GLenum pname, GLint * params) { (void)0;  }
extern "C" void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog) { (void)0;  }
extern "C" void glGetShaderiv(GLuint shader, GLenum pname, GLint * params) { (void)0;  }
extern "C" GLint glGetUniformLocation(GLuint program, const GLchar * name) { (void)0; return 0; }
extern "C" GLint glGetAttribLocation(GLuint program, const GLchar * name) { (void)0; return 0; }
extern "C" GLboolean glIsEnabled(GLenum cap) { (void)0; return 0; }
extern "C" void glPixelStorei(GLenum pname, GLint param) { (void)0;  }
extern "C" void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void * pixels) { (void)0;  }
extern "C" void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) { (void)0;  }
extern "C" void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) { (void)0;  }
extern "C" void glShaderSource(GLuint shader, GLsizei count, const GLchar * const * string, const GLint * length) { (void)0;  }
extern "C" void glStencilFunc(GLenum func, GLint ref, GLuint mask) { (void)0;  }
extern "C" void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) { (void)0;  }
extern "C" void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels) { (void)0;  }
extern "C" void glTexParameteri(GLenum target, GLenum pname, GLint param) { (void)0;  }
extern "C" void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels) { (void)0;  }
extern "C" void glUniform1f(GLint location, GLfloat v0) { (void)0;  }
extern "C" void glUniform1i(GLint location, GLint v0) { (void)0;  }
extern "C" void glUniform2f(GLint location, GLfloat v0, GLfloat v1) { (void)0;  }
extern "C" void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { (void)0;  }
extern "C" void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { (void)0;  }
extern "C" void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) { (void)0;  }
extern "C" void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) { (void)0;  }
extern "C" GLboolean glUnmapBuffer(GLenum target) { (void)0; return 0; }
extern "C" void glUseProgram(GLuint program) { (void)0;  }
extern "C" void glVertexAttrib1f(GLuint index, GLfloat v0) { (void)0;  }
extern "C" void glVertexAttrib3f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2) { (void)0;  }
extern "C" void glVertexAttrib4f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { (void)0;  }
extern "C" void glVertexAttribDivisor(GLuint index, GLuint divisor) { (void)0;  }
extern "C" void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer) { (void)0;  }
extern "C" void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) { (void)0;  }
extern "C" void glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) { (void)0;  }
extern "C" void glUniform2fv(GLint location, GLsizei count, const GLfloat * value) { (void)0;  }
extern "C" void glLinkProgram(GLuint program) { (void)0;  }
extern "C" void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) { (void)0;  }
extern "C" void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) { (void)0;  }
