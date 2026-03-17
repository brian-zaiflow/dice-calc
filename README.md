# DICE-CALC RP-20

A retro-style handheld calculator for tabletop RPG players. Supports standard arithmetic alongside dice notation, so expressions like `3d12+2` work natively — roll three 12-sided dice and add 2, all from a single device.

Built with an Arduino, a small OLED display, and a 4x5 matrix keypad.

## Features

- **Dice notation** — supports `NdM` syntax inline with normal math (e.g. `2d6+1d8*2`)
- **Individual roll display** — shows each die result so you don't have to take the calculator's word for it
- **Full arithmetic** — addition, subtraction, multiplication, division, and parentheses with proper operator precedence
- **Roll animation** — numbers flicker briefly on screen before resolving, because dice should feel like dice
- **Splash screen** — boots with a startup screen, because why not

## Parts List

| Component | Description | Notes |
|-----------|-------------|-------|
| Arduino Uno or Nano | Any ATmega328P-based board | Nano is better for a compact build |
| SSD1306 OLED (128x64) | I2C monochrome display | Most common I2C address is `0x3C`; some modules use `0x3D` |
| 4x5 matrix keypad | 20-key membrane or mechanical keypad | A 4x4 keypad works if you drop parentheses, but 4x5 is recommended |
| Breadboard + jumper wires | For prototyping | Or perfboard/PCB for a permanent build |
| USB cable | For programming and power | Matches your Arduino board (USB-B for Uno, Mini-USB or USB-C for Nano) |

## Wiring

### OLED Display (I2C)

| OLED Pin | Arduino Pin |
|----------|-------------|
| VCC      | 5V          |
| GND      | GND         |
| SDA      | A4          |
| SCL      | A5          |

### Keypad (4x5 matrix)

Default pin assignments (adjust in the sketch to match your wiring):

| Keypad | Arduino Pins |
|--------|-------------|
| Row 0–4 | D2, D3, D4, D5, D6 |
| Col 0–3 | D7, D8, D9, D10 |

### Keypad Layout

```
[ C ] [ ( ] [ ) ] [ ÷ ]
[ 7 ] [ 8 ] [ 9 ] [ × ]
[ 4 ] [ 5 ] [ 6 ] [ - ]
[ 1 ] [ 2 ] [ 3 ] [ + ]
[ d ] [ 0 ] [ ⌫ ] [ = ]
```

## Getting Started

### Prerequisites

Install the [Arduino IDE](https://www.arduino.cc/en/software) (v2.x recommended).

Install the following libraries via **Sketch → Include Library → Manage Libraries**:

- `Adafruit SSD1306`
- `Adafruit GFX` (installed automatically as a dependency)
- `Keypad` by Mark Stanley & Alexander Brevig

### Upload

1. Clone the repo and open `dice_calc/dice_calc.ino` in the Arduino IDE.
2. Connect your Arduino via USB.
3. Select your board under **Tools → Board** (e.g. "Arduino Uno").
4. Select the correct port under **Tools → Port**.
5. Click **Upload** (→ arrow icon).

### Verify Without Hardware

If you don't have the hardware yet, you can click **Verify** (✓ checkmark icon) to confirm the sketch compiles. You can also run it in the [Wokwi online simulator](https://wokwi.com) with a virtual OLED and keypad.

A browser-based prototype is included in the repo at `dice_calc_prototype.html` — open it directly in any browser to test the calculator logic and UI.

## Dice Notation

The calculator supports standard [dice notation](https://en.wikipedia.org/wiki/Dice_notation) inline with arithmetic.

| Input | Meaning |
|-------|---------|
| `d20` | Roll one 20-sided die |
| `3d6` | Roll three 6-sided dice, sum the results |
| `2d8+5` | Roll two 8-sided dice, add 5 |
| `3d12+2` | Roll three 12-sided dice, add 2 |
| `1d20+4+2d6` | Roll 1d20 and 2d6, add 4 to the total |
| `(2d6+2)*3` | Roll 2d6, add 2, then multiply by 3 |

After evaluation, the display shows the total result and the individual rolls for each dice group (e.g. `[4, 11, 2]`).

## Project Structure

```
├── dice_calc/
│   └── dice_calc.ino            # Arduino sketch
├── dice_calc_prototype.html     # Browser-based prototype
└── README.md
```

## License

MIT
