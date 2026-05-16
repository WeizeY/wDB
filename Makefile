# Thin wrapper around cmake/ninja. Run from repo root.
# Designed to be run inside the dev container (where /wdb is mounted).

BUILD_DIR    ?= build
BUILD_TYPE   ?= Debug
GEN          ?= Ninja
DB_FILE      ?= wdb.data
COV_DIR      ?= build-coverage

.PHONY: all configure build run clean rebuild release test fmt coverage coverage-html coverage-clean

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
	rm -rf build build-release $(COV_DIR)

rebuild: clean build

fmt:
	find src tests -type f \( -name '*.h' -o -name '*.cpp' \) -print0 | xargs -0 clang-format -i

# Coverage: configure separate build dir with --coverage flags, run all tests,
# then summarise. `make coverage` prints the per-file table; `make coverage-html`
# additionally generates an HTML report under $(COV_DIR)/coverage-html/.
coverage:
	cmake -B $(COV_DIR) -G $(GEN) -DWDB_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug -S .
	cmake --build $(COV_DIR)
	cd $(COV_DIR) && ctest --output-on-failure
	gcovr --root . \
	      --filter '^src/' \
	      --exclude '.*main\.cpp' \
	      --exclude '.*\.h$$' \
	      --print-summary \
	      --txt $(COV_DIR)/coverage.txt
	@echo "---"
	@cat $(COV_DIR)/coverage.txt

coverage-html: coverage
	mkdir -p $(COV_DIR)/coverage-html
	gcovr --root . \
	      --filter '^src/' \
	      --exclude '.*main\.cpp' \
	      --html-details $(COV_DIR)/coverage-html/index.html
	@echo "HTML report: $(COV_DIR)/coverage-html/index.html"

coverage-clean:
	rm -rf $(COV_DIR)
