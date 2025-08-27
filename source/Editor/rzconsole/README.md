# RZ Console

A powerful command-line console interface for the USTC_CG framework, adapted from NVIDIA's Donut Engine console system.

## Features

- **Command System**: Register custom commands with auto-completion and help
- **Variable System**: Console variables (cvars) with type safety and persistence
- **ImGui Integration**: Modern GUI console with history and filtering
- **Logging Integration**: Optional spdlog integration for log capture
- **Math Support**: Built-in support for GLM vector types
- **Configuration**: INI file parsing for variable initialization

## Quick Start

### 1. Basic Console Widget

```cpp
#include <rzconsole/imgui_console.h>
#include <rzconsole/ConsoleInterpreter.h>
#include <rzconsole/ConsoleObjects.h>

// Create interpreter
auto interpreter = std::make_shared<console::Interpreter>();

// Create console widget - it's already a widget!
ImGui_Console::Options opts;
auto console = std::make_unique<ImGui_Console>(interpreter, opts);

// Register directly with window
window.register_widget(std::move(console));
```

### 2. Register Commands

```cpp
console::CommandDesc echo_cmd = {
    "echo",                    // Command name
    "Echo text to console",    // Description
    [](console::Command::Args const& args) -> console::Command::Result {
        std::string output;
        for (size_t i = 1; i < args.size(); ++i) {
            output += args[i] + " ";
        }
        return { true, output + "\n" };
    }
};
console::RegisterCommand(echo_cmd);
```

### 3. Console Variables

```cpp
// Create typed console variables
cvarFloat my_speed("speed", "Movement speed", 5.0f);
cvarBool debug_mode("debug", "Enable debug mode", false);
cvarString player_name("name", "Player name", "Player1");

// Use them in code
float current_speed = my_speed;  // Implicit conversion
my_speed = 10.0f;               // Assignment
```

### 4. Integration with GUI Framework

```cpp
// Option 1: Register console directly as widget (No wrapper needed!)
auto interpreter = std::make_shared<console::Interpreter>();
ImGui_Console::Options opts;
auto console = std::make_unique<ImGui_Console>(interpreter, opts);
console->Print("Welcome to console!");
window.register_widget(std::move(console));

// Option 2: Create via factory for menu integration
class ConsoleFactory : public IWidgetFactory {
public:
    std::unique_ptr<IWidget> Create(
        const std::vector<std::unique_ptr<IWidget>>& others) override {
        auto interpreter = std::make_shared<console::Interpreter>();
        auto console = std::make_unique<ImGui_Console>(interpreter, ImGui_Console::Options{});
        return std::move(console);
    }
};

window.register_openable_widget(
    std::make_unique<ConsoleFactory>(), 
    { "Tools", "Console" }
);
```

## Built-in Commands

- `help [command]` - Show help for commands
- `help --list [pattern]` - List all commands matching pattern

## Console Variables (CVars)

Console variables provide a powerful way to expose configuration parameters:

```cpp
// Basic types
cvarBool   enable_vsync("vsync", "Enable VSync", true);
cvarInt    max_fps("max_fps", "Maximum FPS", 60);
cvarFloat  fov("fov", "Field of view", 75.0f);
cvarString config_file("config", "Config file path", "config.ini");

// Vector types (GLM)
cvarFloat2 window_size("window_size", "Window size", glm::vec2(1920, 1080));
cvarFloat3 camera_pos("camera_pos", "Camera position", glm::vec3(0, 0, 5));
```

### Variable Properties

- **Read-only**: Variables that can't be modified at runtime
- **Cheat**: Variables that can only be set from code, not console/config
- **Type safety**: Automatic type conversion and validation

```cpp
cvarFloat gravity("gravity", "Gravity constant", 9.8f, true);  // read-only
cvarBool god_mode("god_mode", "God mode", false, false, true); // cheat
```

## INI File Integration

Load configuration from INI files:

```cpp
const char* ini_content = R"(
speed = 10.5
debug = true
name = "MyPlayer"
window_size = 1920 1080
)";

console::ParseIniFile(ini_content, "config.ini");
```

## Logging Integration

### Simple Logging

```cpp
// Direct logging to console
m_console->Print("Hello World!");
m_console->Print("Player health: %d", health);
```

### spdlog Integration (Optional)

```cpp
#include <rzconsole/spdlog_console_sink.h>

// Setup spdlog to forward to console
setup_console_logging(m_console.get());

// Now spdlog messages appear in console
spdlog::info("Game started");
spdlog::warn("Low health warning");
spdlog::error("Connection failed");
```

## Advanced Features

### Command Auto-completion

Commands support auto-completion and suggestion:

```cpp
console::CommandDesc advanced_cmd = {
    "advanced",
    "Advanced command with suggestions",
    [](console::Command::Args const& args) -> console::Command::Result {
        return { true, "Advanced command executed\n" };
    },
    // Suggestion callback
    [](std::string_view cmdline, size_t cursor_pos) -> std::vector<std::string> {
        return { "option1", "option2", "option3" };
    }
};
```

### Variable Callbacks

React to variable changes:

```cpp
cvarFloat volume("volume", "Audio volume", 1.0f);
volume.SetOnChangeCallback([](console::Variable& var) {
    float new_volume = static_cast<cvarFloat&>(var);
    AudioSystem::SetVolume(new_volume);
});
```

## Files Structure

```
rzconsole/
├── include/rzconsole/
│   ├── ConsoleInterpreter.h    # Command parsing and execution
│   ├── ConsoleObjects.h        # Commands and variables system  
│   ├── imgui_console.h         # ImGui console widget
│   ├── string_utils.h          # String manipulation utilities
│   └── spdlog_console_sink.h   # spdlog integration (optional)
├── source/
│   ├── ConsoleInterpreter.cpp
│   ├── ConsoleObjects.cpp
│   ├── imgui_console.cpp
│   └── spdlog_console_sink.cpp
├── examples/
│   └── simple_console_example.cpp
└── tests/
    └── console_gui.cpp
```

## Dependencies

- **Required**: ImGui, GLM, spdlog, USTC_CG framework
- **Optional**: regex support for pattern matching

## Migration from Donut Engine

This console system has been adapted from NVIDIA's Donut Engine with the following changes:

- Replaced `donut::log` with `spdlog`
- Replaced `donut::math` with `glm`
- Removed texture cache functionality
- Updated namespace to `USTC_CG`
- Simplified font handling
- Added USTC_CG GUI widget integration

## Troubleshooting

### Common Issues

1. **Compile errors with donut references**: Make sure all files are using the updated version
2. **spdlog integration not working**: Use direct `Print()` calls instead
3. **Missing symbols**: Ensure all required dependencies are linked

### Performance Tips

- Use console variables sparingly in hot code paths
- Consider caching variable values if accessed frequently
- Disable console in release builds if not needed

## Examples

See `examples/simple_console_example.cpp` for a complete working example.
See `tests/console_gui.cpp` for unit test examples.