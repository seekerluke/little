#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "little.h"
#include "little_std.h"
}

namespace fs = std::filesystem;

// set had_error on callback, lt_dostring does not return anything on error
static bool had_error = false;
static std::string error_msg;

static void error(lt_VM *vm, const char *msg) {
  (void)vm;
  had_error = true;
  error_msg = msg;
}

int main(void) {
  lt_VM *vm = lt_open(malloc, free, error);
  ltstd_open_all(vm);

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
    lt_dostring(vm, buf.str().c_str(), path.filename().string().c_str());

    if (had_error) {
      failed++;
      failures.emplace_back(path.string(), error_msg);
    } else {
      passed++;
    }
  }

  lt_destroy(vm);

  std::cout << std::endl
            << passed << " passed, " << failed << " failed" << std::endl;

  if (failed > 0) {
    std::ofstream log("tests/error.log");
    for (const auto &failure : failures)
      log << failure.first << ": " << failure.second << std::endl;

    std::cout << "Check tests/error.log for failure reasons." << std::endl;
  }

  return 0;
}
