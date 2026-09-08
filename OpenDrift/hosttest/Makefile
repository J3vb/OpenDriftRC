# Host-side tests for hardware-free OpenDrift modules. Needs only g++ and make.
#   make -C OpenDrift/hosttest test

CXX ?= g++
BUILD := build
MODULE_DIR := ../lib/BatteryCompensation
SETTINGS_DIR := ../lib/Settings
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wshadow -Werror
INCLUDES := -I$(MODULE_DIR)

TEST_SOURCES := test_battery_compensation.cpp test_macro_clash.cpp $(MODULE_DIR)/BatteryCompensation.cpp
SETTINGS_SOURCES := test_settings_migration.cpp $(SETTINGS_DIR)/Settings.cpp

all: $(BUILD)/battery_tests $(BUILD)/settings_tests

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/battery_tests: $(TEST_SOURCES) $(MODULE_DIR)/BatteryCompensation.h | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TEST_SOURCES) -o $@

# The ESP32 build uses gnu++11 and a single-precision FPU: prove the module
# compiles under that standard with no float-to-double promotion.
strict-float: | $(BUILD)
	$(CXX) -std=gnu++11 -O2 -Wall -Wextra -Wshadow -Werror -Wdouble-promotion $(INCLUDES) -c $(MODULE_DIR)/BatteryCompensation.cpp -o $(BUILD)/battery_strict.o

# The real Settings code compiled against a fake NVS store (shim/) to prove
# every stored profile layout migrates and the new fields persist.
$(BUILD)/settings_tests: $(SETTINGS_SOURCES) $(SETTINGS_DIR)/Settings.h shim/Arduino.h shim/Preferences.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -DOPENDRIFT_BOARD_AMOLED_164 -Ishim -I$(SETTINGS_DIR) $(SETTINGS_SOURCES) -o $@

test: $(BUILD)/battery_tests $(BUILD)/settings_tests strict-float
	./$(BUILD)/battery_tests
	./$(BUILD)/settings_tests

clean:
	rm -rf $(BUILD)

.PHONY: all test strict-float clean
