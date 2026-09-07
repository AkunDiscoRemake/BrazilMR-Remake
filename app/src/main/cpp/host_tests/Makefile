# BrazilMR — testes de host (Linux/WSL/macOS, sem Android).
# make        → compila e roda os testes (LINKA o motor nativo COMPLETO,
#               com shim GL no-op — valida todos os símbolos do core)
# make syntax → apenas verificação de sintaxe rápida
ROOT := ..
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I$(ROOT) -Istub

# motor nativo completo
ENGINE_SRCS := \
	$(ROOT)/brazilmr/gl/GlUtils.cpp \
	$(ROOT)/brazilmr/glb/Json.cpp \
	$(ROOT)/brazilmr/glb/GlbParser.cpp \
	$(ROOT)/brazilmr/glb/GltfRuntime.cpp \
	$(ROOT)/brazilmr/tracking/OpticalFlow.cpp \
	$(ROOT)/brazilmr/tracking/VisualOdometry.cpp \
	$(ROOT)/brazilmr/renderer/DistortionMesh.cpp \
	$(ROOT)/brazilmr/renderer/SkyRenderer.cpp \
	$(ROOT)/brazilmr/renderer/ModelRenderer.cpp \
	$(ROOT)/brazilmr/renderer/WindowRenderer.cpp \
	$(ROOT)/brazilmr/renderer/HandRenderer.cpp \
	$(ROOT)/brazilmr/renderer/UiRenderer.cpp \
	$(ROOT)/brazilmr/renderer/StereoRenderer.cpp \
	$(ROOT)/brazilmr/renderer/Compositor.cpp \
	$(ROOT)/brazilmr/ui/UiScene.cpp \
	$(ROOT)/brazilmr/windows/SpatialWindow.cpp \
	$(ROOT)/brazilmr/sdk/VR.cpp \
	$(ROOT)/brazilmr_jni/JniBridge.cpp \
	stub/gl_stub_impl.cpp

.PHONY: all run syntax clean

all: bmr_host_tests

bmr_host_tests: test_main.cpp $(ENGINE_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ test_main.cpp $(ENGINE_SRCS) -lm -lpthread

run: bmr_host_tests
	./bmr_host_tests

# verificação de sintaxe individual (rápida, sem link)
syntax:
	@fail=0; \
	for f in $(ENGINE_SRCS); do \
		case "$$f" in stub/*) continue ;; esac; \
		out=`$(CXX) $(CXXFLAGS) -fsyntax-only $$f 2>&1`; \
		if [ -n "$$out" ]; then echo "== $$f =="; echo "$$out"; fail=1; fi; \
	done; \
	if [ $$fail -eq 0 ]; then echo "SINTAXE OK (motor nativo completo)"; \
	else exit 1; fi

clean:
	rm -f bmr_host_tests
