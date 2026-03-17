# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DICE-CALC RP-20 is an Arduino-based retro dice calculator for tabletop RPGs. It runs on ATmega328P boards (Uno/Nano) with a 128x64 SSD1306 OLED display and a 4x5 matrix keypad. Users type dice expressions like `2d6+4` and the device rolls dice and evaluates arithmetic.

## Build & Upload

This is an Arduino `.ino` sketch. Use either the Arduino IDE or `arduino-cli`:

```bash
# Compile
arduino-cli compile --fqbn arduino:avr:uno dice_calc/

# Upload
arduino-cli compile --fqbn arduino:avr:uno -u -p /dev/cu.usbmodem* dice_calc/
```

### Required Libraries

Install via Arduino Library Manager or `arduino-cli lib install`:
- Adafruit SSD1306
- Adafruit GFX
- Keypad (by Mark Stanley & Alexander Brevig)

## Architecture

Everything lives in a single sketch file: `dice_calc/dice_calc.ino`

### Key subsystems (all in the one file):

1. **Input handling** (`loop()`): Keypad polling with state machine — C clears, `<` backspaces, `=` evaluates, operators after a result chain from previous answer.

2. **Dice resolver** (`resolveDice()`): Scans the expression string for `NdM` patterns (e.g., `3d6`), rolls dice via `random()`, replaces dice notation with numeric sums in a separate `resolved[]` buffer. Individual roll results are stored in `rollResults[]` for display.

3. **Expression evaluator** (`parseExpr/parseTerm/parseFactor/parseNumber`): Recursive-descent parser operating on the resolved string. Supports `+`, `-`, `*`, `/`, and parentheses. Uses a global `evalPos` cursor.

4. **Display rendering** (`drawScreen()`): Three-zone OLED layout — expression top (size 1), result middle (size 3), individual roll values bottom (size 1). Right-aligned text. Replaces `*`/`/` with retro calculator symbols.

5. **Roll animation** (`animateRoll()`): Flickers random numbers for visual effect before showing actual result.

### Important constraints

- **Memory limits**: ATmega328P has 2KB RAM. Expression buffer is 40 chars (`MAX_EXPR`), resolved buffer is 80 chars (`MAX_RESOLVED`), max 32 tracked rolls (`MAX_ROLLS`). Be conservative with stack and heap usage.
- **Integer arithmetic**: All math uses `long` (32-bit). No floating point.
- **Dice limits**: 1-99 dice, 1-9999 sides per die notation.
- **RNG**: Seeded from floating analog pins — adequate for tabletop, not cryptographic.

### Pin assignments

- Keypad rows: D2-D6, columns: D7-D10
- OLED: I2C on A4 (SDA) / A5 (SCL), address 0x3C
