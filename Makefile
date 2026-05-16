# Thin wrapper around cmake/ninja. Run from repo root.
# Designed to be run inside the dev container (where /wdb is mounted).

BUILD_DIR    ?= build
BUILD_TYPE   ?= Debug
GEN          ?= Ninja
DB_FILE      ?= wdb.data

.PHONY: all configure build run clean rebuild release test fmt

all: build

configure:
	cmake -B $(BUILD_DIR) -G $(GEN) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -S .

build: configure
	cmake --build $(BUILD_DIR)

release:
	$(MAKE) build BUILD_DIR=build-release BUILD_TYPE=Release

run: build
	./$(BUILD_DIR)/src/wdb $(DB_FILE)

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	rm -rf build build-release

rebuild: clean build

# Format all source files in place (requires clang-format).
fmt:
	find src -type f \( -name '*.h' -o -name '*.cpp' \) -print0 | xargs -0 clang-format -i
