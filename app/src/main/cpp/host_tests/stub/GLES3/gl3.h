// HOST-ONLY — stub mínimo de GLES 3.0 para COMPILAÇÃO dos fontes do renderer
// fora do Android (verificação de sintaxe/tipos em CI/sandbox; não linka).
// No device, o header real do NDK é usado (ver gl/GlHeaders.hpp).
#ifndef BMR_HOST_GLES3_STUB
#define BMR_HOST_GLES3_STUB

#include <cstddef>

typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef void           GLvoid;
typedef int            GLint;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef int            GLsizei;
typedef float          GLfloat;
typedef float          GLclampf;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef ptrdiff_t      GLsizeiptr;
typedef ptrdiff_t      GLintptr;
typedef char           GLchar;
typedef ptrdiff_t      GLSSIZE_T;
#define GL_APIENTRY
#define GL_API extern "C"

// ---- constantes ----------------------------------------------------------
#define GL_NO_ERROR 0
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_ZERO 0
#define GL_ONE 1
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CULL_FACE 0x0B44
#define GL_BLEND 0x0BE2
#define GL_DITHER 0x0BD0
#define GL_SCISSOR_TEST 0x0C11
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_WRAP_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE0 0x84C0
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32 0x81A7
#define GL_STENCIL_INDEX8 0x8D48
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_BUFFER_SIZE 0x8764
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_CCW 0x0901
#define GL_CW 0x0900
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_BLEND_DST_RGB 0x80C8
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_SAMPLES 0x80A9
#define GL_MULTISAMPLE 0x809D
#define GL_LINE_WIDTH 0x0B21
#define GL_ALIASED_LINE_WIDTH_RANGE 0x846E
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278

// ---- funções -------------------------------------------------------------
GL_API void GL_APIENTRY glActiveTexture(GLenum texture);
GL_API void GL_APIENTRY glAttachShader(GLuint program, GLuint shader);
GL_API void GL_APIENTRY glBindAttribLocation(GLuint program, GLuint index, const GLchar* name);
GL_API void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer);
GL_API void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer);
GL_API void GL_APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer);
GL_API void GL_APIENTRY glBindTexture(GLenum target, GLuint texture);
GL_API void GL_APIENTRY glBindVertexArray(GLuint array);
GL_API void GL_APIENTRY glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GL_API void GL_APIENTRY glBlendEquation(GLenum mode);
GL_API void GL_APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
GL_API void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor);
GL_API void GL_APIENTRY glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
GL_API void GL_APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
GL_API void GL_APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
GL_API GLenum GL_APIENTRY glCheckFramebufferStatus(GLenum target);
GL_API void GL_APIENTRY glClear(GLbitfield mask);
GL_API void GL_APIENTRY glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GL_API void GL_APIENTRY glClearDepthf(GLclampf depth);
GL_API void GL_APIENTRY glClearStencil(GLint s);
GL_API void GL_APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
GL_API void GL_APIENTRY glCompileShader(GLuint shader);
GL_API void GL_APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
GL_API void GL_APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GL_API GLuint GL_APIENTRY glCreateProgram(void);
GL_API GLuint GL_APIENTRY glCreateShader(GLenum type);
GL_API void GL_APIENTRY glCullFace(GLenum mode);
GL_API void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint* buffers);
GL_API void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
GL_API void GL_APIENTRY glDeleteProgram(GLuint program);
GL_API void GL_APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
GL_API void GL_APIENTRY glDeleteShader(GLuint shader);
GL_API void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint* textures);
GL_API void GL_APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
GL_API void GL_APIENTRY glDepthFunc(GLenum func);
GL_API void GL_APIENTRY glDepthMask(GLboolean flag);
GL_API void GL_APIENTRY glDepthRangef(GLclampf n, GLclampf f);
GL_API void GL_APIENTRY glDetachShader(GLuint program, GLuint shader);
GL_API void GL_APIENTRY glDisable(GLenum cap);
GL_API void GL_APIENTRY glDisableVertexAttribArray(GLuint index);
GL_API void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count);
GL_API void GL_APIENTRY glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
GL_API void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
GL_API void GL_APIENTRY glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
GL_API void GL_APIENTRY glEnable(GLenum cap);
GL_API void GL_APIENTRY glEnableVertexAttribArray(GLuint index);
GL_API void GL_APIENTRY glFinish(void);
GL_API void GL_APIENTRY glFlush(void);
GL_API void GL_APIENTRY glFrontFace(GLenum mode);
GL_API void GL_APIENTRY glGenBuffers(GLsizei n, GLuint* buffers);
GL_API void GL_APIENTRY glGenerateMipmap(GLenum target);
GL_API void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint* framebuffers);
GL_API void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint* renderbuffers);
GL_API void GL_APIENTRY glGenTextures(GLsizei n, GLuint* textures);
GL_API void GL_APIENTRY glGenVertexArrays(GLsizei n, GLuint* arrays);
GL_API void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean* params);
GL_API GLenum GL_APIENTRY glGetError(void);
GL_API void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat* params);
GL_API void GL_APIENTRY glGetIntegerv(GLenum pname, GLint* params);
GL_API void GL_APIENTRY glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
GL_API void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint* params);
GL_API void GL_APIENTRY glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
GL_API void GL_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
GL_API const GLubyte* GL_APIENTRY glGetString(GLenum name);
GL_API GLint GL_APIENTRY glGetUniformLocation(GLuint program, const GLchar* name);
GL_API GLint GL_APIENTRY glGetAttribLocation(GLuint program, const GLchar* name);
GL_API GLboolean GL_APIENTRY glIsEnabled(GLenum cap);
GL_API void* GL_APIENTRY glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GL_API void GL_APIENTRY glPixelStorei(GLenum pname, GLint param);
GL_API void GL_APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
GL_API void GL_APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
GL_API void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
GL_API void GL_APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
GL_API void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask);
GL_API void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
GL_API void GL_APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
GL_API void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param);
GL_API void GL_APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
GL_API void GL_APIENTRY glUniform1f(GLint location, GLfloat v0);
GL_API void GL_APIENTRY glUniform1i(GLint location, GLint v0);
GL_API void GL_APIENTRY glUniform2f(GLint location, GLfloat v0, GLfloat v1);
GL_API void GL_APIENTRY glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
GL_API void GL_APIENTRY glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
GL_API void GL_APIENTRY glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GL_API void GL_APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GL_API GLboolean GL_APIENTRY glUnmapBuffer(GLenum target);
GL_API void GL_APIENTRY glUseProgram(GLuint program);
GL_API void GL_APIENTRY glVertexAttrib1f(GLuint index, GLfloat v0);
GL_API void GL_APIENTRY glVertexAttrib3f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
GL_API void GL_APIENTRY glVertexAttrib4f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
GL_API void GL_APIENTRY glVertexAttribDivisor(GLuint index, GLuint divisor);
GL_API void GL_APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
GL_API void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

#endif // BMR_HOST_GLES3_STUB

// extras usados pelo BrazilMR
GL_API void GL_APIENTRY glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GL_API void GL_APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat* value);
GL_API void GL_APIENTRY glLinkProgram(GLuint program);
GL_API void GL_APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GL_API void GL_APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
#define GL_REPEAT 0x2901
