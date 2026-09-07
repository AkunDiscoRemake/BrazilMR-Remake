// BrazilMR — inclusão de headers GL.
// Android: GLES3 do NDK. Host (testes de compilação): stub declarativo.
#pragma once

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h> // extensões (nem todo NDK tem GLES3/gl3ext.h)
#else
#include "GLES3/gl3.h" // stub host_tests/stub/ (via -I)
#endif

// constantes de extensão — sempre disponíveis como fallback
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

namespace brazilmr {

// Verifica erros GL (log apenas; não lança exceções).
const char* glErrorString(GLenum err);

} // namespace brazilmr
