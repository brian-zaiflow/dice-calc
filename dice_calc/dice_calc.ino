// ============================================================
// DICE-CALC RP-20 — Retro Dice Calculator
// ============================================================
// Hardware:
//   - Arduino Uno/Nano (or any ATmega328P board)
//   - SSD1306 128x64 OLED (I2C: SDA=A4, SCL=A5)
//   - 6x6 matrix keypad (26 active keys)
//
// Libraries (install via Arduino Library Manager):
//   - Adafruit SSD1306
//   - Adafruit GFX
//   - Keypad (by Mark Stanley & Alexander Brevig)
//
// Keypad physical layout (6 cols x 6 rows):
//   Row 0:  [d4 ] [d6 ] [d8 ] [d10] [d12] [d20]   <- dice shortcuts
//   Row 1:  [ C ] [ ( ] [ ) ] [ / ] [   ] [   ]
//   Row 2:  [ 7 ] [ 8 ] [ 9 ] [ * ] [   ] [   ]
//   Row 3:  [ 4 ] [ 5 ] [ 6 ] [ - ] [   ] [   ]
//   Row 4:  [ 1 ] [ 2 ] [ 3 ] [ + ] [   ] [   ]
//   Row 5:  [ d ] [ 0 ] [ <- ] [ = ] [   ] [   ]
//
// Pin assignments (adjust to match your wiring):
//   Rows: D2-D6, A0   Cols: D7-D12
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
const byte ROWS = 6;
const byte COLS = 6;

// Dice shortcut keys use chars A-G to distinguish from regular keys:
//   A=d4, B=d6, H=d8, E=d10, F=d12, G=d20
// Unused grid positions are 0 (treated as NO_KEY).
char hexaKeys[ROWS][COLS] = {
  {'A', 'B', 'H', 'E', 'F', 'G'},   // dice shortcuts
  {'C', '(', ')', '/',  0,   0 },
  {'7', '8', '9', '*',  0,   0 },
  {'4', '5', '6', '-',  0,   0 },
  {'1', '2', '3', '+',  0,   0 },
  {'d', '0', '<', '=',  0,   0 }
};

// Adjust these pin numbers to match your wiring
byte rowPins[ROWS] = {2, 3, 4, 5, 6, A0};
byte colPins[COLS] = {7, 8, 9, 10, 11, 12};

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

// --- Dice shortcut state ---
int lastDiceSides = 0;  // tracks repeat-press incrementing

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
// DICE SHORTCUT HELPERS
// ============================================================

// Map keypad char to die sides (0 = not a dice shortcut)
int getDiceSides(char key) {
  switch (key) {
    case 'A': return 4;
    case 'B': return 6;
    case 'H': return 8;
    case 'E': return 10;
    case 'F': return 12;
    case 'G': return 20;
    default:  return 0;
  }
}

// Handle a dice shortcut press.  Mirrors the v2 prototype logic:
//  - Same shortcut again → increment count (d6 → 2d6 → 3d6)
//  - Different shortcut  → auto-insert '+' then append dN
void handleDiceShortcut(int sides) {
  if (hasResult) {
    clearCalc();
  }

  // --- repeat-press: increment count of the trailing NdM ---
  if (lastDiceSides == sides && exprLen > 0) {
    // Walk back past the sides digits
    int pos = exprLen - 1;
    while (pos >= 0 && isDigit(expr[pos])) pos--;

    // Should now be sitting on 'd'
    if (pos >= 0 && expr[pos] == 'd') {
      int dPos = pos;
      pos--;
      // Walk back past the count digits
      while (pos >= 0 && isDigit(expr[pos])) pos--;
      int countStart = pos + 1;

      int count = 1;
      if (countStart < dPos) {
        char buf[6] = {0};
        for (int i = 0; i < dPos - countStart && i < 5; i++)
          buf[i] = expr[countStart + i];
        count = atoi(buf);
      }
      count++;

      // Rebuild tail: <count>d<sides>
      char tail[16];
      char countBuf[6];
      itoa(count, countBuf, 10);
      char sidesBuf[6];
      itoa(sides, sidesBuf, 10);

      int tl = 0;
      for (int i = 0; countBuf[i]; i++) tail[tl++] = countBuf[i];
      tail[tl++] = 'd';
      for (int i = 0; sidesBuf[i]; i++) tail[tl++] = sidesBuf[i];
      tail[tl] = '\0';

      exprLen = countStart;
      for (int i = 0; tail[i] && exprLen < MAX_EXPR; i++)
        expr[exprLen++] = tail[i];
      expr[exprLen] = '\0';

      lastDiceSides = sides;
      return;
    }
  }

  // --- first press or different shortcut ---
  // Auto-insert '+' if the expression doesn't already end with
  // an operator or open paren
  if (exprLen > 0) {
    char last = expr[exprLen - 1];
    if (last != '+' && last != '-' && last != '*' &&
        last != '/' && last != '(') {
      if (exprLen < MAX_EXPR) {
        expr[exprLen++] = '+';
        expr[exprLen] = '\0';
      }
    }
  }

  // Append "dN"
  char sidesBuf[6];
  itoa(sides, sidesBuf, 10);

  if (exprLen < MAX_EXPR) {
    expr[exprLen++] = 'd';
    expr[exprLen] = '\0';
  }
  for (int i = 0; sidesBuf[i] && exprLen < MAX_EXPR; i++) {
    expr[exprLen++] = sidesBuf[i];
  }
  expr[exprLen] = '\0';

  lastDiceSides = sides;
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  char key = keypad.getKey();
  if (!key) return;

  // --- Dice shortcut keys (top row) ---
  int sides = getDiceSides(key);
  if (sides > 0) {
    handleDiceShortcut(sides);
    drawScreen();
    return;
  }

  // Regular key resets dice-shortcut repeat tracking
  lastDiceSides = 0;

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
  lastDiceSides = 0;
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
