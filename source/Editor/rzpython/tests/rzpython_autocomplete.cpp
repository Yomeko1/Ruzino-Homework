#include <GUI/window.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <rzpython/interpreter.hpp>
#include <rzpython/rzpython.hpp>

using namespace Ruzino;

int main()
{
    spdlog::set_level(spdlog::level::debug);

    auto window = std::make_unique<Window>();
    python::initialize();
    python::reference("window", window.get());

    auto interpreter = python::CreatePythonInterpreter();

    // 测试1: window对象补全
    spdlog::info("=== Test 1: window object autocomplete ===");
    auto suggestions1 = interpreter->Suggest("window.", 7);
    spdlog::info("Got {} suggestions for 'window.':", suggestions1.size());
    int count = 0;
    for (const auto& suggestion : suggestions1) {
        if (!suggestion.starts_with("__")) {
            spdlog::info("  - {}", suggestion);
            if (++count >= 10)
                break;
        }
    }

    // 测试2: Python内置函数补全
    spdlog::info("\n=== Test 2: Python builtin autocomplete ===");
    auto suggestions2 = interpreter->Suggest("pri", 3);
    spdlog::info("Got {} suggestions for 'pri':", suggestions2.size());
    for (const auto& suggestion : suggestions2) {
        spdlog::info("  - {}", suggestion);
    }

    // 测试3: 导入模块后的补全
    spdlog::info("\n=== Test 3: Module autocomplete after import ===");
    python::call<void>("import math");
    auto suggestions3 = interpreter->Suggest("math.", 5);
    spdlog::info("Got {} suggestions for 'math.':", suggestions3.size());
    count = 0;
    for (const auto& suggestion : suggestions3) {
        if (!suggestion.starts_with("__")) {
            spdlog::info("  - {}", suggestion);
            if (++count >= 10)
                break;
        }
    }

    // 测试4: 自定义变量补全
    spdlog::info("\n=== Test 4: Custom variable autocomplete ===");
    python::call<void>("my_test_var = 123");
    python::call<void>("my_other_var = 'hello'");
    auto suggestions4 = interpreter->Suggest("my_", 3);
    spdlog::info("Got {} suggestions for 'my_':", suggestions4.size());
    for (const auto& suggestion : suggestions4) {
        spdlog::info("  - {}", suggestion);
    }

    // 测试5: 再次测试window（验证可重复使用）
    spdlog::info("\n=== Test 5: window object autocomplete again ===");
    auto suggestions5 = interpreter->Suggest("window.get", 10);
    spdlog::info("Got {} suggestions for 'window.get':", suggestions5.size());
    for (const auto& suggestion : suggestions5) {
        spdlog::info("  - {}", suggestion);
    }

    spdlog::info("\n=== All tests completed ===");

    return 0;
}