#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "little.h"
#include "little_dev.h"
#include "little_std.h"
}

namespace fs = std::filesystem;

static bool had_error = false;
static std::string error_msg;

static void error(lt_VM *vm, const char *msg) {
  (void)vm;
  had_error = true;
  error_msg = msg;
}

static uint8_t test_assert(lt_VM *vm, uint8_t argc) {
  if (argc < 1) {
    lt_error(vm, "test.assert expects 1 argument");
    return 0;
  }
  lt_Value val = lt_pop(vm);
  if (!LT_IS_TRUTHY(val)) {
    lt_runtime_error(vm, "assertion failed");
    return 0;
  }
  return 0;
}

static uint8_t test_expect_error(lt_VM *vm, uint8_t argc) {
  if (argc < 1) {
    lt_error(vm, "test.expect_error expects 1 argument");
    return 0;
  }
  lt_Value val = lt_pop(vm);
  if (!LT_IS_STRING(val)) {
    lt_error(vm, "test.expect_error expects a string argument");
    return 0;
  }
  const char *source = lt_get_string(vm, val);

  bool prev = had_error;
  std::string prev_msg = error_msg;
  had_error = false;
  error_msg.clear();

  lt_dostring(vm, source, (uint32_t)strlen(source), "expect_error");

  bool caught = had_error;
  had_error = prev;
  error_msg = prev_msg;

  if (!caught) {
    lt_error(vm, "expected error but code succeeded");
    return 0;
  }
  return 0;
}

static uint8_t test_make_ptr(lt_VM *vm, uint8_t argc) {
  (void)argc;
  void *ptr = vm->alloc(sizeof(int));
  // lt_make_ptr takes ownership of this pointer and garbage collects it later,
  // no need to free it yourself
  lt_push(vm, lt_make_ptr(vm, ptr));
  return 1;
}

int main(void) {
  lt_VM *vm = lt_open(malloc, free, error);
  ltstd_open_all(vm);

  lt_Value test_table = lt_make_table(vm);
  lt_table_set(vm, test_table, lt_make_string(vm, "assert"),
               lt_make_native(vm, test_assert));
  lt_table_set(vm, test_table, lt_make_string(vm, "expect_error"),
               lt_make_native(vm, test_expect_error));
  lt_table_set(vm, test_table, lt_make_string(vm, "make_ptr"),
               lt_make_native(vm, test_make_ptr));
  lt_table_set(vm, vm->global, lt_make_string(vm, "test"), test_table);

  std::vector<fs::path> files;
  for (const auto &entry : fs::directory_iterator("tests")) {
    if (entry.is_regular_file() && entry.path().extension() == ".little")
      files.push_back(entry.path());
  }

  std::sort(files.begin(), files.end());

  int passed = 0;
  int failed = 0;
  std::vector<std::pair<std::string, std::string>> failures;

  for (const fs::path &path : files) {
    std::ifstream in(path);
    std::stringstream buf;
    buf << in.rdbuf();

    had_error = false;
    error_msg.clear();

    std::cout << "Running test " << path.filename() << "..." << std::endl;
    std::string source = buf.str();
    std::string name = path.filename().string();
    lt_dostring(vm, source.c_str(), (uint32_t)source.size(), name.c_str());

    if (had_error) {
      failed++;
      failures.emplace_back(path.string(), error_msg);
    } else {
      passed++;
    }
  }

  lt_destroy(vm);

  std::cout << passed << " passed, " << failed << " failed" << std::endl;

  if (failed > 0) {
    std::ofstream log("tests/error.log");
    for (const auto &failure : failures)
      log << failure.first << ": " << failure.second << "\n\n";

    std::cout << "Check tests/error.log for failure reasons." << std::endl;
  }

  return 0;
}
