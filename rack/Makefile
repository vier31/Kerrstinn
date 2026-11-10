RACK_DIR ?= $(HOME)/Code/toolchains/Rack-SDK-2.6.4

# The engine is shared with the plugin and the Clouds firmware.
ENGINE_DIR := ../engine
FLAGS += -I$(ENGINE_DIR)

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk

CXXFLAGS += -std=c++17

# The engine is header-only, so it is compiled inside KerrStinn.cpp. Slice
# boundaries are float maths: the SDK's -funsafe-math-optimizations and FMA
# contraction would move them by a sample compared to the original.
build/src/KerrStinn.cpp.o: CXXFLAGS += -fno-unsafe-math-optimizations -ffp-contract=off

LICENSE:
	cp ../LICENSE LICENSE

dist: LICENSE

RACK_APP_DIR ?= /Applications/VCV Rack 2 Pro.app/Contents/Resources

build/module_test: Tests/module_test.cpp src/KerrStinn.cpp src/dr_wav.cpp $(wildcard $(ENGINE_DIR)/*.h)
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -fno-unsafe-math-optimizations -ffp-contract=off -w -I$(ENGINE_DIR) -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include -o $@ Tests/module_test.cpp src/dr_wav.cpp -L$(RACK_DIR) -lRack

test: build/module_test
	DYLD_LIBRARY_PATH="$(RACK_APP_DIR)" ./build/module_test

.PHONY: test
