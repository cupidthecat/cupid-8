# cupid-8 Chip8 Emulator

A simple Chip-8 (and SCHIP extended) emulator written in C using SDL2 for graphics, audio, and input. This emulator loads a Chip-8 ROM, interprets its opcodes, and renders the output in a resizable window with basic audio synthesis.

---

## Overview

The emulator implements the core features of the Chip-8 virtual machine:
- **Memory & Registers:** Emulates 4096 bytes of memory, 16 general-purpose registers, and a dedicated index register.
- **Stack & Timers:** Supports a 16-level stack for subroutine calls, with delay and sound timers.
- **Graphics:** Renders a display of 64×32 pixels in normal mode, with an extended mode (128×64) available for SCHIP-compatible programs.
- **Input:** Uses SDL2 to map keyboard inputs to Chip-8 keys.
- **Audio:** Generates a sine-wave tone (440 Hz) when the sound timer is active.
- **Extended Opcodes:** Supports additional opcodes (e.g., scrolling and mode switching) for SCHIP.

---

## Features

- **Standard Chip-8 Opcodes:** Supports opcodes for arithmetic, control flow, drawing sprites, and handling input.
- **SCHIP Extensions:** Implements extended opcodes to enable an extended display mode and additional scrolling functions.
- **Dynamic Display Scaling:** Renders the output using a configurable scale factor (default: 10×) for better visibility.
- **Sine-wave Audio Synthesis:** Uses SDL2’s audio callback to produce sound when needed.
- **Keyboard Mapping:** Maps common keys (e.g., `1,2,3,4`, `Q,W,E,R`, etc.) to the Chip-8 keypad.

---

## Requirements

- **C Compiler:** A standard C compiler (such as GCC or Clang).
- **SDL2 Library:** Ensure SDL2 is installed on your system.
- **Math Library:** The code uses functions from `math.h` (linked with `-lm`).

---

## Installation & Compilation

1. **Install SDL2:**  
   On Ubuntu/Debian-based systems:  
   ```bash
   sudo apt-get install libsdl2-dev
   ```  
   On macOS (using Homebrew):  
   ```bash
   brew install sdl2
   ```

2. **Compile the Code:**  
   Use the following command to compile the emulator:
   ```bash
   gcc -o cupid-8 chip8_emulator.c -lSDL2 -lm
   ```
   or use the Makefile:
   ```bash
   make
   ```
---

## Usage

Run the emulator from the command line by providing the path to a Chip-8 ROM:
```bash
./cupid-8 path/to/romfile
```
If the ROM is too large or if there are any errors in initialization, the emulator will print error messages to the terminal.

---

## Keyboard Mapping

The emulator maps physical keys to the Chip-8 keypad as follows:

| Chip-8 Key | Mapped Key    |
|------------|---------------|
| 1          | `1`           |
| 2          | `2`           |
| 3          | `3`           |
| C          | `4`           |
| 4          | `Q`           |
| 5          | `W`           |
| 6          | `E`           |
| D          | `R`           |
| 7          | `A`           |
| 8          | `S`           |
| 9          | `D`           |
| E          | `F`           |
| A          | `Z`           |
| 0          | `X`           |
| B          | `C`           |
| F          | `V`           |

---

## Extended Mode (SCHIP)

- **Switching Modes:**  
  - **Enable Extended Mode:** Opcode `00FF` switches to an extended 128×64 display with a bright cyan foreground on a dark blue background.
  - **Disable Extended Mode:** Opcode `00FE` reverts to the normal 64×32 display with a white foreground on a black background.
- **Scrolling:**  
  - **Scroll Right:** Opcode `00FB` scrolls the display 4 pixels to the right.
  - **Scroll Left:** Opcode `00FC` scrolls the display 4 pixels to the left.
  - **Scroll Down:** Specific opcodes scroll the display down by a given number of rows.

---

## Code Structure

- **Main Loop:**  
  The `main()` function initializes the Chip-8 state, loads a ROM, sets up SDL2 (for video and audio), and enters the main emulation loop.  
- **Emulation Cycle:**  
  The `emulateCycle()` function fetches, decodes, and executes opcodes, updating registers, memory, timers, and the display.
- **Graphics Rendering:**  
  The `drawGraphics()` function uses SDL2 to render the pixel data onto a window, scaling each pixel by `WINDOW_SCALE`.
- **Audio Callback:**  cupid
  The `audio_callback()` function generates a sine-wave tone when the sound timer is active.
- **Input Handling:**  
  Keyboard events are captured to update the Chip-8 keypad state.

---

## Contributing

Contributions, suggestions, and bug reports are welcome. Feel free to fork the repository, create new branches for features or fixes, and submit pull requests.

---

## License

This project is released under the GNU v3 License. See the [LICENSE](LICENSE) file for more details.
