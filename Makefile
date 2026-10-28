CXX      := clang++
# The libraries this one composes are sibling checkouts (see README.md); set
# LIBS to the folder that holds them. Their lite data is compiled in.
LIBS     ?= ..
LIB_DIRS := address birth country device email name nickname phone
EMBED    := $(addprefix -DDASMIG_,ADDRESS_EMBED BIRTH_EMBED COUNTRY_EMBED DEVICE_EMBED EMAIL_EMBED NAME_EMBED NICKNAME_EMBED PHONE_EMBED)
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -Wno-c2y-extensions -I. $(foreach d,$(LIB_DIRS),-I$(LIBS)/$(d)-generator) $(EMBED)
BUILD    := build
PYTHON   := python3

HEADERS := dasmig/usergen.hpp dasmig/core.hpp $(wildcard dasmig/core/*.hpp)

EXAMPLE_SRC := examples/example.cpp
EXAMPLE_BIN := $(BUILD)/example

TEST_SRC := tests/tests.cpp tests/catch_amalgamated.cpp
TEST_BIN := $(BUILD)/test_runner

BENCH_SRC   := bench/bench.cpp
BENCH_FLAGS := -O2 -DNDEBUG

COV_FLAGS := -fprofile-instr-generate -fcoverage-mapping
COV_DIR   := coverage

.PHONY: all clean lint test bench bench-gate test-embed coverage docs embed package

all: $(EXAMPLE_BIN)

$(EXAMPLE_BIN): $(EXAMPLE_SRC) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(EXAMPLE_SRC)

$(TEST_BIN): $(TEST_SRC) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRC)

test: $(TEST_BIN)
	./$(TEST_BIN)

# The same tests with the lite tier compiled in (DASMIG_USER_EMBED).
test-embed: $(TEST_SRC) $(HEADERS) dasmig/usergen_data.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -DDASMIG_USER_EMBED -o $(BUILD)/test_embed $(TEST_SRC)
	./$(BUILD)/test_embed

# Regenerate the compiled-in copy of the lite tier.
embed:
	$(PYTHON) scripts/embed.py resources --tier lite --name usergen --output dasmig/usergen_data.hpp

# Benchmarks (bench/): an optimized build, run on the lite tier (compiled
# in) and on the full tier, read from the libraries' checkouts in $(LIBS).
# Results in build/bench.tsv.
bench: $(BENCH_SRC) bench/bench.hpp $(HEADERS) | $(BUILD)
	rm -f $(BUILD)/bench.tsv
	$(CXX) $(CXXFLAGS) $(BENCH_FLAGS) -DDASMIG_USER_EMBED -DDASMIG_BENCH_EMBEDDED -o $(BUILD)/bench $(BENCH_SRC)
	./$(BUILD)/bench --tier lite --out $(BUILD)/bench.tsv
	cd $(LIBS) && $(CURDIR)/$(BUILD)/bench --tier full --out $(CURDIR)/$(BUILD)/bench.tsv

# The instruction-count gate: the benchmark in --gate mode under cachegrind,
# compared with bench/gate_baseline.txt (scripts/gate.py). Needs Valgrind.
bench-gate: $(BENCH_SRC) bench/bench.hpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(BENCH_FLAGS) -o $(BUILD)/bench_gate $(BENCH_SRC)
	$(PYTHON) scripts/gate.py $(BUILD)/bench_gate

coverage: $(TEST_SRC) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(COV_FLAGS) -o $(BUILD)/test_cov $(TEST_SRC)
	cd $(BUILD) && ./test_cov
	llvm-profdata merge -sparse $(BUILD)/default.profraw -o $(BUILD)/tests.profdata
	llvm-cov report $(BUILD)/test_cov --instr-profile=$(BUILD)/tests.profdata --sources $(HEADERS)
	llvm-cov show $(BUILD)/test_cov --instr-profile=$(BUILD)/tests.profdata --sources $(HEADERS) --format=html -o $(COV_DIR)
	@echo "Coverage report: $(COV_DIR)/index.html"

$(BUILD):
	@mkdir -p $(BUILD)

docs:
	doxygen Doxyfile
	@echo "API docs: doc/api/html/index.html"

lint:
	clang-tidy --header-filter='usergen[.]hpp' $(EXAMPLE_SRC) -- $(CXXFLAGS)

# The npm package (package/), built with Emscripten into $(BUILD)/package; see
# scripts/build_package.py. Test it with node package/test.mjs $(BUILD)/package.
package:
	$(PYTHON) scripts/build_package.py --libs $(LIBS) --out $(BUILD)/package

clean:
	rm -rf $(BUILD) $(COV_DIR) doc/api
