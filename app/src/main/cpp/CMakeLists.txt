# ===========================================================================
# BrazilMR — build nativo (NDK / CMake)
# libbrazilmr.so: renderer estéreo, tracking, filtros, SLAM CV, GLB, UI VR.
# ===========================================================================
cmake_minimum_required(VERSION 3.22.1)
project(brazilmr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Opções
# ---------------------------------------------------------------------------
option(BRAZILMR_BUILD_LUAJIT "Compila LuaJIT nativo (opcional; engine padrão é LuaJ na JVM)" OFF)

# ---------------------------------------------------------------------------
# Biblioteca principal
# ---------------------------------------------------------------------------
set(BMR_SOURCES
    brazilmr/gl/GlUtils.cpp
    brazilmr/glb/Json.cpp
    brazilmr/glb/GlbParser.cpp
    brazilmr/glb/GltfRuntime.cpp
    brazilmr/tracking/OpticalFlow.cpp
    brazilmr/tracking/VisualOdometry.cpp
    brazilmr/renderer/DistortionMesh.cpp
    brazilmr/renderer/SkyRenderer.cpp
    brazilmr/renderer/ModelRenderer.cpp
    brazilmr/renderer/WindowRenderer.cpp
    brazilmr/renderer/HandRenderer.cpp
    brazilmr/renderer/UiRenderer.cpp
    brazilmr/renderer/StereoRenderer.cpp
    brazilmr/renderer/Compositor.cpp
    brazilmr/ui/UiScene.cpp
    brazilmr/windows/SpatialWindow.cpp
    brazilmr/sdk/VR.cpp
    brazilmr_jni/JniBridge.cpp
)

add_library(brazilmr SHARED ${BMR_SOURCES})

target_include_directories(brazilmr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(log-lib log)
find_library(android-lib android)
target_link_libraries(brazilmr
    ${log-lib}
    ${android-lib}
    EGL
    GLESv3
)

# Otimizações de tamanho/código (VR Box: binário enxuto)
target_compile_options(brazilmr PRIVATE
    -ffunction-sections
    -fdata-sections
)
target_link_options(brazilmr PRIVATE
    -Wl,--gc-sections
)

# ---------------------------------------------------------------------------
# LuaJIT nativo (opcional) — ver docs/SDK_LUA.md
# Requer: third_party/luajit (git subtree). O engine padrão da SDK Lua é
# LuaJ (JVM, dependência Gradle org.luaj:luaj-jse), 100% funcional.
# ---------------------------------------------------------------------------
if(BRAZILMR_BUILD_LUAJIT)
    set(LUAJIT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/luajit)
    if(EXISTS ${LUAJIT_DIR}/src)
        # compilação do LuaJIT para Android é feita via script próprio
        # (tools/build_luajit_android.sh) por causa do host/toolchain.
        # Aqui apenas linkamos a biblioteca pré-compilada.
        add_library(luajit STATIC IMPORTED)
        set_target_properties(luajit PROPERTIES IMPORTED_LOCATION
            ${LUAJIT_DIR}/src/libluajit.a)
        target_include_directories(brazilmr PRIVATE ${LUAJIT_DIR}/src)
        target_link_libraries(brazilmr luajit)
        target_compile_definitions(brazilmr PRIVATE BMR_HAS_LUAJIT=1)
    else()
        message(WARNING "BRAZILMR_BUILD_LUAJIT=ON mas third_party/luajit ausente — desabilitado.")
    endif()
endif()
