// ============================================================
// DICE-CALC RP-20 — Retro Dice Calculator
// ============================================================
// Hardware:
//   - Arduino Uno/Nano (or any ATmega328P board)
//   - SSD1306 128x64 OLED (I2C: SDA=A4, SCL=A5)
//   - 4x5 matrix keypad (20 keys)
//
// Libraries (install via Arduino Library Manager):
//   - Adafruit SSD1306
//   - Adafruit GFX
//   - Keypad (by Mark Stanley & Alexander Brevig)
//
// Keypad physical layout (4 cols x 5 rows):
//   Row 0:  [ C ] [ ( ] [ ) ] [ / ]
//   Row 1:  [ 7 ] [ 8 ] [ 9 ] [ * ]
//   Row 2:  [ 4 ] [ 5 ] [ 6 ] [ - ]
//   Row 3:  [ 1 ] [ 2 ] [ 3 ] [ + ]
//   Row 4:  [ d ] [ 0 ] [ <- ] [ = ]
//
// Pin assignments (adjust to match your wiring):
//   Rows: D2-D6   Cols: D7-D10
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

// --- Display setup ---
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

// --- Keypad setup ---
const byte ROWS = 5;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
  {'C', '(', ')', '/'},
  {'7', '8', '9', '*'},
  {'4', '5', '6', '-'},
  {'1', '2', '3', '+'},
  {'d', '0', '<', '='}
};

// Adjust these pin numbers to match your wiring
byte rowPins[ROWS] = {2, 3, 4, 5, 6};
byte colPins[COLS] = {7, 8, 9, 10};

Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// --- Calculator state ---
#define MAX_EXPR 40
char expr[MAX_EXPR + 1];     // the raw input expression
int  exprLen = 0;

#define MAX_ROLLS 32          // max individual die results we'll track
int  rollResults[MAX_ROLLS];
int  rollCount = 0;

// After resolving dice, we store the arithmetic string here
#define MAX_RESOLVED 80
char resolved[MAX_RESOLVED + 1];

long finalResult = 0;
bool hasResult = false;
bool hasError  = false;

// --- Roll animation ---
#define ROLL_ANIM_FRAMES 6
#define ROLL_ANIM_DELAY  80  // ms per frame

// ============================================================
// SETUP
// ============================================================
void setup() {
  // Seed RNG from floating analog pin — not cryptographic,
  // but plenty random for tabletop dice
  randomSeed(analogRead(A0) ^ (analogRead(A1) << 8));

  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 init failed"));
    for (;;); // halt
  }

  display.clearDisplay();
  drawSplash();
  display.display();
  delay(1500);

  clearCalc();
  drawScreen();
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  char key = keypad.getKey();
  if (!key) return;

  switch (key) {
    case 'C':
      clearCalc();
      break;

    case '<': // backspace
      if (hasResult) {
        clearCalc();
      } else if (exprLen > 0) {
        exprLen--;
        expr[exprLen] = '\0';
      }
      break;

    case '=':
      evaluate();
      break;

    default:
      // If we just showed a result and user starts typing
      // a digit or 'd', start fresh
      if (hasResult && (isDigit(key) || key == 'd')) {
        clearCalc();
      } else if (hasResult) {
        // If typing an operator after a result, keep the
        // result as the start of the new expression
        char buf[12];
        ltoa(finalResult, buf, 10);
        clearCalc();
        for (int i = 0; buf[i] && exprLen < MAX_EXPR; i++) {
          expr[exprLen++] = buf[i];
        }
        expr[exprLen] = '\0';
      }

      if (exprLen < MAX_EXPR) {
        expr[exprLen++] = key;
        expr[exprLen] = '\0';
      }
      break;
  }

  drawScreen();
}

// ============================================================
// CLEAR
// ============================================================
void clearCalc() {
  exprLen = 0;
  expr[0] = '\0';
  rollCount = 0;
  finalResult = 0;
  hasResult = false;
  hasError = false;
}

// ============================================================
// DICE RESOLVER
// Scans `expr` for NdM patterns, rolls dice, writes the
// arithmetic-only result into `resolved`.
// Populates rollResults[] with individual die values.
// ============================================================
bool resolveDice() {
  rollCount = 0;
  int ri = 0; // index into resolved[]
  int i = 0;

  while (i < exprLen && ri < MAX_RESOLVED - 1) {
    // Look for a 'd' that's part of a dice expression
    if (expr[i] == 'd' && (i + 1 < exprLen) && isDigit(expr[i + 1])) {
      // Parse count (digits before 'd')
      int count = 0;
      int countDigits = 0;
      int backtrack = ri - 1;

      // Walk back through resolved to find the count digits
      while (backtrack >= 0 && isDigit(resolved[backtrack])) {
        backtrack--;
        countDigits++;
      }

      if (countDigits > 0) {
        // Extract count from resolved
        char countBuf[6] = {0};
        int start = backtrack + 1;
        for (int c = 0; c < countDigits && c < 5; c++) {
          countBuf[c] = resolved[start + c];
        }
        count = atoi(countBuf);
        ri = start; // rewind resolved to overwrite count digits
      } else {
        count = 1;  // bare 'd6' means 1d6
      }

      i++; // skip the 'd'

      // Parse sides
      char sidesBuf[6] = {0};
      int si = 0;
      while (i < exprLen && isDigit(expr[i]) && si < 5) {
        sidesBuf[si++] = expr[i++];
      }
      int sides = atoi(sidesBuf);

      // Validate
      if (count < 1 || count > 99 || sides < 1 || sides > 9999) {
        return false;
      }

      // Roll the dice
      long sum = 0;
      for (int r = 0; r < count; r++) {
        int roll = random(1, sides + 1);
        if (rollCount < MAX_ROLLS) {
          rollResults[rollCount++] = roll;
        }
        sum += roll;
      }

      // Write sum into resolved
      char sumBuf[12];
      ltoa(sum, sumBuf, 10);
      for (int s = 0; sumBuf[s] && ri < MAX_RESOLVED - 1; s++) {
        resolved[ri++] = sumBuf[s];
      }

    } else {
      resolved[ri++] = expr[i++];
    }
  }

  resolved[ri] = '\0';
  return true;
}

// ============================================================
// EXPRESSION EVALUATOR
// Simple recursive-descent parser for +, -, *, /, ()
// Operates on the `resolved` string (dice already replaced)
// ============================================================
int evalPos; // current parse position

long parseExpr();
long parseTerm();
long parseFactor();
long parseNumber();

long parseNumber() {
  // Handle negative numbers
  bool neg = false;
  if (resolved[evalPos] == '-') {
    neg = true;
    evalPos++;
  }

  if (resolved[evalPos] == '(') {
    evalPos++; // skip '('
    long val = parseExpr();
    if (resolved[evalPos] == ')') evalPos++; // skip ')'
    return neg ? -val : val;
  }

  long val = 0;
  bool foundDigit = false;
  while (isDigit(resolved[evalPos])) {
    val = val * 10 + (resolved[evalPos] - '0');
    evalPos++;
    foundDigit = true;
  }

  if (!foundDigit) {
    hasError = true;
    return 0;
  }

  return neg ? -val : val;
}

long parseFactor() {
  return parseNumber();
}

long parseTerm() {
  long left = parseFactor();
  while (resolved[evalPos] == '*' || resolved[evalPos] == '/') {
    char op = resolved[evalPos++];
    long right = parseFactor();
    if (op == '*') left *= right;
    else {
      if (right == 0) { hasError = true; return 0; }
      left /= right;
    }
  }
  return left;
}

long parseExpr() {
  long left = parseTerm();
  while (resolved[evalPos] == '+' || resolved[evalPos] == '-') {
    char op = resolved[evalPos++];
    long right = parseTerm();
    if (op == '+') left += right;
    else left -= right;
  }
  return left;
}

// ============================================================
// EVALUATE — orchestrates resolve + parse
// ============================================================
void evaluate() {
  if (exprLen == 0) return;

  hasError = false;
  hasResult = false;

  // Animate the roll if dice are involved
  bool hasDice = false;
  for (int i = 0; i < exprLen; i++) {
    if (expr[i] == 'd') { hasDice = true; break; }
  }

  if (hasDice) {
    animateRoll();
  }

  if (!resolveDice()) {
    hasError = true;
    hasResult = true;
    return;
  }

  evalPos = 0;
  finalResult = parseExpr();

  if (resolved[evalPos] != '\0') {
    // Leftover characters means malformed expression
    hasError = true;
  }

  hasResult = true;
}

// ============================================================
// ROLL ANIMATION — flickers random numbers briefly
// ============================================================
void animateRoll() {
  for (int frame = 0; frame < ROLL_ANIM_FRAMES; frame++) {
    display.clearDisplay();

    // Expression at top
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    drawExprText();

    // Flickering random number
    display.setTextSize(3);
    long fakeNum = random(1, 100);
    char buf[8];
    ltoa(fakeNum, buf, 10);
    int tw = strlen(buf) * 18;
    display.setCursor(SCREEN_W - tw, 20);
    display.print(buf);

    // "Rolling..." text at bottom
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print(F("rolling"));
    for (int dot = 0; dot <= (frame % 3); dot++) {
      display.print('.');
    }

    display.display();
    delay(ROLL_ANIM_DELAY);
  }
}

// ============================================================
// DISPLAY RENDERING
// ============================================================

// Pretty-print the expression (replace * with x, / with a
// division symbol for that retro calculator feel)
void drawExprText() {
  for (int i = 0; i < exprLen; i++) {
    if (expr[i] == '*') display.print((char)0xF8); // '×' approx
    else if (expr[i] == '/') display.print((char)0xFD); // '÷' approx
    else display.print(expr[i]);
  }
}

void drawScreen() {
  display.clearDisplay();

  // --- Zone 1: Expression (top, small font) ---
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Right-align: estimate width, position cursor
  int exprPixelW = exprLen * 6; // each char ~6px at size 1
  int exprX = max(0, SCREEN_W - exprPixelW);
  display.setCursor(exprX, 0);
  drawExprText();

  // --- Zone 2: Result (middle, big font) ---
  if (hasResult) {
    display.setTextSize(3);
    char resultBuf[14];

    if (hasError) {
      strcpy(resultBuf, "ERR");
    } else {
      ltoa(finalResult, resultBuf, 10);
    }

    int rw = strlen(resultBuf) * 18;
    display.setCursor(max(0, SCREEN_W - rw), 16);
    display.print(resultBuf);
  } else {
    // Show "0" when idle
    display.setTextSize(3);
    display.setCursor(SCREEN_W - 18, 16);
    display.print('0');
  }

  // --- Zone 3: Individual rolls (bottom, small font) ---
  if (hasResult && !hasError && rollCount > 0) {
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.print('[');
    for (int i = 0; i < rollCount; i++) {
      if (i > 0) display.print(',');
      display.print(rollResults[i]);
    }
    display.print(']');

    // If the text overflows, it just clips — acceptable
    // for very large rolls. Could add scrolling later.
  }

  // --- Divider line above rolls zone ---
  if (hasResult && rollCount > 0) {
    display.drawFastHLine(0, 49, SCREEN_W, SSD1306_WHITE);
  }

  display.display();
}

// ============================================================
// SPLASH SCREEN
// ============================================================
void drawSplash() {
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 8);
  display.print(F("DICE-CALC"));

  display.setTextSize(1);
  display.setCursor(40, 32);
  display.print(F("RP-20"));

  display.setCursor(20, 52);
  display.print(F("roll for init..."));
}
