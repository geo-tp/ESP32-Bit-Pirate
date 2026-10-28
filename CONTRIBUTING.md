
# How to Contribute

If you are not familiar with PlatformIO, you can follow the build instructions here:  
https://github.com/geo-tp/ESP32-Bit-Pirate/wiki/99-Build

Bit Pirate runs under tight hardware constraints, especially RAM. Please avoid introducing global or static objects, containers, buffers, or other structures that allocate RAM at boot unless they are strictly necessary. Prefer const / constexpr data, flash-resident tables, local stack allocations, and on-demand allocation where appropriate. Keep memory usage in mind when adding features, as even small permanent allocations accumulate across the firmware.

## Architecture

The firmware is organized into small components with clearly separated responsibilities.

| Path | Role |
|---|---|
| `src/Abstracts/` | Base classes and abstract definitions shared by multiple implementations. |
| `src/Adapters/` | USB CDC adapter implementations used to expose Bit Pirate as a compatible interface for external software and protocols. |
| `src/Analyzers/` | Signal, protocol, or data analysis components such as binary or subGhz frames. |
| `src/Boards/` | Board-specific support, hardware initialization, pin mapping, displays, onboard inputs, and shared board abstractions. |
| `src/Configurators/` | Components responsible for configuring firmware features, peripherals, or runtime options. |
| `src/Controllers/` | High-level user-facing logic. Controllers handle feature workflows, validation, and coordination between lower-level services. |
| `src/Data/` | Static databases, lookup tables, signatures, and other firmware data. |
| `src/Dispatchers/` | Routing logic used to forward commands, events, or requests to the appropriate component. |
| `src/Enums/` | Shared enumerations and data mappers used across the firmware. |
| `src/Inputs/` | Input communication channels such as Web Serial or serial terminals. These represent how commands enter the firmware, not physical board inputs. |
| `src/Interfaces/` | Common interfaces used to decouple implementations and define shared contracts between components. |
| `src/Managers/` | Self-contained feature managers handling reusable application behaviors such as command history or alias handling. |
| `src/Models/` | Shared data structures used across the firmware, such as infrared frames, SubGHz packets, and other common data representations. |
| `src/Providers/` | Central access points exposing shared objects across the firmware, ensuring a single source of truth for common instances. |
| `src/Selectors/` | Selection logic used for menus, modes, devices, adapters, or configurable options on the device screen. |
| `src/Servers/` | Network and communication servers exposed by the firmware such as HTTP, DNS. |
| `src/Services/` | Low-level protocol and hardware logic used by higher-level controllers and shells. |
| `src/Shells/` | Specialized controller-like components grouping the user-facing logic for a related set of features or commands. |
| `src/States/` | State definitions and state-management components shared between controllers. |
| `src/Transformers/` | Data conversion and transformation helpers. |
| `src/Vendors/` | External or example code, often originating from Arduino sketches, integrated with minimal changes. |
| `src/Views/` | Output communication channels used to render terminal content, such as serial or web terminals. |
| `src/main.cpp` | Main firmware entry point and global application initialization. |

## Adding Support for a New Board

Bit Pirate provides a generic `[env:custom]` profile in `platformio.ini` for bringing up new ESP32-S3 boards without immediately adding board-specific code.

### Start with the custom profile

Configure the board through build flags whenever possible:

- protect GPIOs already used by onboard hardware with `PROTECTED_PINS`
- configure the input button
- configure UART, I2C, SPI and other protocol pins
- configure host serial (`USB CDC` or `DEVICE_HOST_SERIAL_UART`)
- configure the display if present

Build with:

```bash
pio run -e custom
```

### Display support

The custom profile currently supports these display drivers:

```text
CUSTOM_DISPLAY_DRIVER_ST7789_SPI
CUSTOM_DISPLAY_DRIVER_ST7789_PARALLEL
CUSTOM_DISPLAY_DRIVER_ILI9341_SPI
```

Only one display driver should be enabled.

If a board uses another controller, a new display driver can be added under the common board views and exposed through the custom configuration.

### When the custom profile is not enough

Some boards require hardware-specific initialization, power management, peripherals, input handling, or other behavior that cannot be described only with build flags.

In that case, adding a dedicated board implementation is acceptable. Follow the structure of the existing board classes and keep reusable code in the common board layer whenever possible.




## Add a New Command

### 1. Create a `handleXXX` Method in the Appropriate Controller

Each terminal command is handled in a *Controller* (e.g., `UartController`, `I2cController`, `LedController`, etc.).

Example: `src/Controllers/UartController`
```cpp
void UartController::handleRead() {
    terminalView.println("UART Read: Streaming until [ENTER] is pressed...");
    uartService.flush();

    while (true) {
        // Stop if ENTER is pressed
        char key = terminalInput.readChar(); // read a char, non blocking
        if (key == '\r' || key == '\n') {
            terminalView.println("\nUART Read: Stopped by user.");
            break;
        }

        // Print UART data as it comes
        while (uartService.available() > 0) {
            char c = uartService.read();
            terminalView.print(std::string(1, c)); // print to the terminal
        }
    }
}
```

---

### 2. Use `terminalView` and `terminalInput` for I/O

- `terminalView` handles output (print, println) in the web or serial terminal.
```cpp
class ITerminalView {
public:
    virtual ~ITerminalView() = default;
    
    // Initialize the terminal
    virtual void initialize() = 0;

    // Show welcome message with logo and infos
    virtual void welcome(TerminalTypeEnum& terminalType, std::string& terminalInfos) = 0;

    // Print to the terminal
    virtual void print(const std::string& text) = 0;
    virtual void println(const std::string& text) = 0;
    virtual void printPrompt(const std::string& mode = "HIZ") = 0;

    // Wait press
    virtual void waitPress() = 0;

    // Clear the terminal
    virtual void clear() = 0;
};

```

- `terminalInput` handles input (readChar for keypresses, etc.) in the web or serial terminal.
```cpp
class IInput {
public:
    virtual ~IInput() = default;

    // Blocking read
    virtual char handler() = 0;

    // Non blocking read
    virtual char readChar() = 0;

    // Wait an input
    virtual void waitPress() = 0;
    
};
```

---

### 3. Use Helpers to validate and parse arguments

Your `handleXXX` method can optionally take a `const TerminalCommand& cmd` parameter, which contains the user's input split into `root`, `subcommand`, and `args`.

For example, the UART command:

```cpp
void UartController::handleSpam(const TerminalCommand& cmd); // Usage: spam <text> <ms>
```

Will be interpreted as:

- `cmd.getRoot() -> "spam"`
- `cmd.getSubCommand() -> "Hello"`
- `cmd.getArgs() -> "10"`

You can then use `ArgTransformer` to parse and validate these arguments as needed.

- `ArgTransformer` to validate and convert strings into `uint8_t`, `uint32_t`, hex, etc.
- `UserInputManager` for interactive prompts and validation (yes/no, pin number, validated strings, hex list, etc.).

Example:
```cpp

// Split args into a list of string
std::vector<std::string> args = argTransformer.splitArgs(cmd.getArgs());

// Validate string arg
bool valid = argTransformer.isValidNumber(args[0])

// Transform string arg into an unsigned int
uint8_t pin = argTransformer.toUint8(args[0]);

// Ask and validate a char choice
char parityChar = userInputManager.readCharChoice("Parity (N/E/O)", defaultParity, {'N', 'E', 'O'});

// Ask yes or no
bool inverted = userInputManager.readYesNo("Inverted?", state.isUartInverted());

```

---

### 4. Use `GlobalState` to Share State Between Controllers

The `GlobalState` object provides a centralized way to share data across controllers.

You can use it to:
- Store and access **pin mappings** for each protocol (e.g., UART RX/TX pins, I2C SCL/SDA).
- Share **global configuration or flags** (e.g., current terminal mode, webui IP, etc).

Example:
```cpp
int rxPin = state.getUartRxPin();
state.setUartRxPin(rxPin);
```

---

### 5. Delegate Low-Level Logic to Services

The **Controller** is for high-level logic and user interaction.
The **Service** handles protocol-level actions.

Example:
```cpp
char UartService::read() {
    return Serial1.read();
}

void UartService::write(char c) {
    Serial1.write(c);
}

```

---

### 📎 Summary

These components are already integrated into each controller, you can simply use them directly in your new handleXXX method.


| Component         | Responsibility                  |
|------------------|----------------------------------|
| service (uartService, i2cService, etc)          | Low-level protocol logic        |
| terminalView      | Output to terminal (web and serial)             |
| terminalInput     | Input from user (web and serial)               |
| argTransformer    | String arguments to typed value            |
| userInputManager  | Ask for a certain input type and validate it  |
| state  | Application state shared between controllers |

Example:

```cpp
void FictiveController::handleXXX(const TerminalCommand& cmd) {
    terminalView.println("[FICTIVE] Command execution started.");

    // Read arguments from the TerminalCommand
    std::string rawValue = cmd.getSubCommand();
    if (rawValue.empty()) {
        terminalView.println("[FICTIVE] No value provided. Please enter a number");
        return;
    }

    // Validate string using ArgTransformer
    if (!argTransformer.isValidNumber(rawValue)) {
        terminalView.println("[FICTIVE] Invalid number format.");
        return;
    }
   
    // Convert string arg into int using ArgTransformer
    uint8_t value = argTransformer.toUint8(rawValue);
   
    // Ask user confirmation using UserInputManager
    bool confirmation = userInputManager.readYesNo("Are you sure?", false);
    if (!confirmation) {
        terminalView.println("[FICTIVE] User is not sure. Stopped.");
        return;
    }

    // Use service logic (e.g., simulate sending the value somewhere)
    bool result = fictiveService.sendValue(value);
    if (result) {
        terminalView.println("[FICTIVE] Value sent successfully.");
    } else {
        terminalView.println("[FICTIVE] Failed to send value.");
    }

    // Access shared state (e.g., configured pin for this protocol)
    int pin = state.getFictiveDataPin();
    terminalView.println("[FICTIVE] Used data pin: " + std::to_string(pin));
}
```

