// ====================================================================
// HANGMAN PLOTTER: CALIBRATION + DRAWING, CONTROLLED OVER SERIAL
// MKS Base V1.6 / RAMPS 1.4 pinouts
// ====================================================================

#define MOTOR_A_STEP    54  // gantry motor A
#define MOTOR_A_DIR     55
#define MOTOR_A_ENABLE  38

#define MOTOR_B_STEP    60  // gantry motor B
#define MOTOR_B_DIR     61
#define MOTOR_B_ENABLE  56

#define Z_STEP          46  // carriage (left-right) motor
#define Z_DIR           48
#define Z_ENABLE        62

#define PEN_STEP        36  // E1 pen lifter
#define PEN_DIR         34
#define PEN_ENABLE      30

// ---------------------- SETTINGS ----------------------
#define STEPS_PER_INCH 2032

const float PAPER_W = 6.0f;   // width  (x) in inches
const float PAPER_H = 5.5f;   // height (y) in inches

// X/Y speed (microseconds per half-pulse). Lower = faster.
const unsigned int CRUISE_DELAY_US = 600;
const unsigned int START_DELAY_US  = 1200;
const long RAMP_STEPS = 300;

// Direction levels for "+" moves (w = +Y, d = +X). Flip if a key moves the wrong way.
const uint8_t GANTRY_POS_LEVEL   = HIGH;
const uint8_t CARRIAGE_POS_LEVEL = HIGH;

// Pen lift
int penLiftSteps = 150;                 // steps from the starting position down to the paper
const int PEN_UP_EXTRA_STEPS = 50;      // CHANGED (new): extra steps the pen rises ABOVE the
                                        // starting position. Bigger = higher pen up.
                                        // The pen-down position is not affected.
const unsigned int PEN_DELAY_US = 1000; // raise to 2000 if the pen motor stalls
const uint8_t PEN_DOWN_LEVEL = LOW;     // flip if the pen goes the wrong way

// Jog sizes in inches, cycled with the 'j' command
const float JOG_SIZES[3] = {1.0f, 0.25f, 0.05f};
int jogIndex = 0;

// ---- Word blanks and letters (inches) ----
const float SLOT_Y        = 1.5f;   // height of the blanks
const float SLOT_CENTER_X = 3.0f;   // the whole row is centered here
const float SLOT_PITCH    = 0.74f;   // distance from one blank to the next
const float SLOT_LEN      = 0.4f;   // length of each blank
const float LETTER_H      = 0.5f;   // height of written letters

// ---------------------- STATE ----------------------
long currX = 0;
long currY = 0;
bool penIsDown = false;
long penPos = 0;          // CHANGED (new): pen position in steps. 0 = starting position,
                          // positive = toward the paper, negative = raised above the start
bool dryRun = false;      // true = pen never lowers (preview)

long cornerX[4];
long cornerY[4];
int calIndex = 0;
bool calibrated = false;

int wordLen = 0;          // number of blanks drawn for the current word

const char* cornerNames[4] = {
  "Corner 1: Bottom-Left  (0, 0)",
  "Corner 2: Bottom-Right (6, 0)",
  "Corner 3: Top-Right    (6, 5.5)",
  "Corner 4: Top-Left     (0, 5.5)"
};

// ---------------------- STROKE FONT ----------------------
// Each letter is a list of x,y points on an 8 wide x 12 tall grid.
// 255 = lift the pen and start a new stroke, 254 = end of letter.
const uint8_t F_A[] PROGMEM = {0,0, 4,12, 8,0, 255, 2,6, 6,6, 254};
const uint8_t F_B[] PROGMEM = {0,0, 0,12, 6,12, 8,10, 8,8, 6,6, 0,6, 255, 6,6, 8,4, 8,2, 6,0, 0,0, 254};
const uint8_t F_C[] PROGMEM = {8,10, 6,12, 2,12, 0,10, 0,2, 2,0, 6,0, 8,2, 254};
const uint8_t F_D[] PROGMEM = {0,0, 0,12, 5,12, 8,9, 8,3, 5,0, 0,0, 254};
const uint8_t F_E[] PROGMEM = {8,12, 0,12, 0,0, 8,0, 255, 0,6, 6,6, 254};
const uint8_t F_F[] PROGMEM = {8,12, 0,12, 0,0, 255, 0,6, 6,6, 254};
const uint8_t F_G[] PROGMEM = {8,10, 6,12, 2,12, 0,10, 0,2, 2,0, 6,0, 8,2, 8,6, 4,6, 254};
const uint8_t F_H[] PROGMEM = {0,0, 0,12, 255, 8,0, 8,12, 255, 0,6, 8,6, 254};
const uint8_t F_I[] PROGMEM = {2,12, 6,12, 255, 4,12, 4,0, 255, 2,0, 6,0, 254};
const uint8_t F_J[] PROGMEM = {8,12, 8,2, 6,0, 2,0, 0,2, 254};
const uint8_t F_K[] PROGMEM = {0,0, 0,12, 255, 8,12, 0,6, 8,0, 254};
const uint8_t F_L[] PROGMEM = {0,12, 0,0, 8,0, 254};
const uint8_t F_M[] PROGMEM = {0,0, 0,12, 4,6, 8,12, 8,0, 254};
const uint8_t F_N[] PROGMEM = {0,0, 0,12, 8,0, 8,12, 254};
const uint8_t F_O[] PROGMEM = {2,0, 0,2, 0,10, 2,12, 6,12, 8,10, 8,2, 6,0, 2,0, 254};
const uint8_t F_P[] PROGMEM = {0,0, 0,12, 6,12, 8,10, 8,8, 6,6, 0,6, 254};
const uint8_t F_Q[] PROGMEM = {2,0, 0,2, 0,10, 2,12, 6,12, 8,10, 8,2, 6,0, 2,0, 255, 5,4, 8,0, 254};
const uint8_t F_R[] PROGMEM = {0,0, 0,12, 6,12, 8,10, 8,8, 6,6, 0,6, 255, 4,6, 8,0, 254};
const uint8_t F_S[] PROGMEM = {8,10, 6,12, 2,12, 0,10, 0,8, 2,6, 6,6, 8,4, 8,2, 6,0, 2,0, 0,2, 254};
const uint8_t F_T[] PROGMEM = {0,12, 8,12, 255, 4,12, 4,0, 254};
const uint8_t F_U[] PROGMEM = {0,12, 0,2, 2,0, 6,0, 8,2, 8,12, 254};
const uint8_t F_V[] PROGMEM = {0,12, 4,0, 8,12, 254};
const uint8_t F_W[] PROGMEM = {0,12, 2,0, 4,6, 6,0, 8,12, 254};
const uint8_t F_X[] PROGMEM = {0,12, 8,0, 255, 8,12, 0,0, 254};
const uint8_t F_Y[] PROGMEM = {0,12, 4,6, 8,12, 255, 4,6, 4,0, 254};
const uint8_t F_Z[] PROGMEM = {0,12, 8,12, 0,0, 8,0, 254};

const uint8_t* const FONT[26] PROGMEM = {
  F_A, F_B, F_C, F_D, F_E, F_F, F_G, F_H, F_I, F_J, F_K, F_L, F_M,
  F_N, F_O, F_P, F_Q, F_R, F_S, F_T, F_U, F_V, F_W, F_X, F_Y, F_Z
};

// ---------------------- SETUP ----------------------
void setup() {
  Serial.begin(115200);

  // Drivers off before pins become outputs
  digitalWrite(MOTOR_A_ENABLE, HIGH);
  digitalWrite(MOTOR_B_ENABLE, HIGH);
  digitalWrite(Z_ENABLE, HIGH);
  digitalWrite(PEN_ENABLE, HIGH);

  pinMode(MOTOR_A_STEP, OUTPUT); pinMode(MOTOR_A_DIR, OUTPUT); pinMode(MOTOR_A_ENABLE, OUTPUT);
  pinMode(MOTOR_B_STEP, OUTPUT); pinMode(MOTOR_B_DIR, OUTPUT); pinMode(MOTOR_B_ENABLE, OUTPUT);
  pinMode(Z_STEP, OUTPUT);       pinMode(Z_DIR, OUTPUT);       pinMode(Z_ENABLE, OUTPUT);
  pinMode(PEN_STEP, OUTPUT);     pinMode(PEN_DIR, OUTPUT);     pinMode(PEN_ENABLE, OUTPUT);

  digitalWrite(MOTOR_A_STEP, LOW);
  digitalWrite(MOTOR_B_STEP, LOW);
  digitalWrite(Z_STEP, LOW);
  digitalWrite(PEN_STEP, LOW);

  digitalWrite(MOTOR_A_ENABLE, LOW);
  digitalWrite(MOTOR_B_ENABLE, LOW);
  digitalWrite(Z_ENABLE, LOW);
  digitalWrite(PEN_ENABLE, LOW);

  setGlobalHome();
  Serial.println("READY");
}

// ---------------------- MAIN LOOP ----------------------
// Every command is answered with "OK" when finished, or "ERR <reason>".
void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;
    if (processCommand(input)) Serial.println("OK");
  }
}

bool processCommand(String input) {
  char cmd = tolower(input.charAt(0));

  // Jogging: w/a/s/d, optionally followed by a count (e.g. d6 = six jogs right)
  if (cmd == 'w' || cmd == 'a' || cmd == 's' || cmd == 'd') {
    int n = 1;
    if (input.length() > 1) {
      n = input.substring(1).toInt();
      if (n < 1) n = 1;
    }
    long jog = lroundf(JOG_SIZES[jogIndex] * STEPS_PER_INCH) * n;
    if (cmd == 'w') moveSteps(0, jog);
    if (cmd == 's') moveSteps(0, -jog);
    if (cmd == 'a') moveSteps(-jog, 0);
    if (cmd == 'd') moveSteps(jog, 0);
    printPosition();
    return true;
  }

  if (cmd == 'q') { setPen(false); Serial.println(">> Pen UP"); return true; }
  if (cmd == 'e') { setPen(true);  Serial.println(">> Pen DOWN"); return true; }

  if (cmd == 'j') {
    jogIndex = (jogIndex + 1) % 3;
    Serial.print(">> Jog size is now ");
    Serial.print(JOG_SIZES[jogIndex], 2);
    Serial.println(" inch per jog.");
    return true;
  }

  // Set Home and restart calibration
  if (cmd == '0') {
    setGlobalHome();
    calIndex = 0;
    calibrated = false;
    wordLen = 0;
    Serial.println(">> Home registered at (0,0). Calibration reset.");
    Serial.println("CAL 0");
    return true;
  }

  // Record the next corner
  if (cmd == 'c') {
    if (calIndex >= 4) {
      Serial.println("ERR already calibrated (press Set Home to start over)");
      return false;
    }
    cornerX[calIndex] = currX;
    cornerY[calIndex] = currY;
    Serial.print(">> RECORDED "); Serial.print(cornerNames[calIndex]);
    Serial.print("  at steps X: "); Serial.print(currX);
    Serial.print(", Y: "); Serial.println(currY);
    calIndex++;
    if (calIndex == 4) calibrated = true;
    Serial.print("CAL "); Serial.println(calIndex);
    return true;
  }

  // Trace the four corners with the pen up
  if (cmd == 't') {
    if (!calibrated) { Serial.println("ERR calibrate all 4 corners first"); return false; }
    setPen(false);
    for (int i = 0; i < 4; i++) moveTo(cornerX[i], cornerY[i]);
    moveTo(cornerX[0], cornerY[0]);
    Serial.println(">> Trace done.");
    return true;
  }

  // Preview the whole drawing with the pen held up
  if (cmd == 'v') {
    if (!calibrated) { Serial.println("ERR calibrate all 4 corners first"); return false; }
    dryRun = true;
    drawHangman();
    dryRun = false;
    return true;
  }

  // Draw the whole hangman in one go
  if (cmd == 'g') {
    if (!calibrated) { Serial.println("ERR calibrate all 4 corners first"); return false; }
    dryRun = false;
    drawHangman();
    return true;
  }

  // Draw ONE body part: b1 head, b2 body, b3 left leg, b4 right leg, b5 left arm, b6 right arm
  if (cmd == 'b') {
    if (!calibrated) { Serial.println("ERR calibrate all 4 corners first"); return false; }
    int n = input.substring(1).toInt();
    if (n < 1 || n > 6) { Serial.println("ERR part must be 1-6"); return false; }
    dryRun = false;
    drawPart(n);
    Serial.print(">> Drew part "); Serial.println(n);
    return true;
  }

  // Draw the blanks for a new word: n5 ... n8
  if (cmd == 'n') {
    if (!calibrated) { Serial.println("ERR calibrate all 4 corners first"); return false; }
    int n = input.substring(1).toInt();
    if (n < 1 || n > 8) { Serial.println("ERR word length must be 1-8"); return false; }
    wordLen = n;
    dryRun = false;
    drawBlanks(n);
    Serial.print(">> Drew "); Serial.print(n); Serial.println(" blanks");
    return true;
  }

  // Write a letter in its blank(s): l<LETTER><slots>, e.g. "lE25" = E in slots 2 and 5
  if (cmd == 'l') {
    if (!calibrated) { Serial.println("ERR calibrate all 4 corners first"); return false; }
    if (wordLen == 0) { Serial.println("ERR draw the blanks first"); return false; }
    char ch = toupper(input.charAt(1));
    if (ch < 'A' || ch > 'Z') { Serial.println("ERR bad letter"); return false; }
    dryRun = false;
    for (unsigned int i = 2; i < input.length(); i++) {
      char d = input.charAt(i);
      if (d >= '0' && d <= '7') {
        int slot = d - '0';
        if (slot < wordLen) writeLetterInSlot(ch, slot);
      }
    }
    Serial.print(">> Wrote "); Serial.println(ch);
    return true;
  }

  // Face in the head: f1 = smiley (win), f0 = frowny (loss)
  if (cmd == 'f') {
    if (!calibrated) { Serial.println("ERR calibrate all 4 corners first"); return false; }
    int mood = input.substring(1).toInt();
    dryRun = false;
    drawFace(mood == 1);
    Serial.println(">> Drew face");
    return true;
  }

  if (cmd == 'h') { returnToHome(); return true; }
  if (cmd == 'p') { printPosition(); return true; }

  if (cmd == 'k') {
    Serial.print("STATUS "); Serial.print(calIndex);
    Serial.print(" "); Serial.print(penIsDown ? 1 : 0);
    Serial.print(" "); Serial.print(currX);
    Serial.print(" "); Serial.println(currY);
    return true;
  }

  Serial.println("ERR unknown command");
  return false;
}

// ---------------------- COORDINATE MAPPING ----------------------
// Paper coordinates (inches) -> motor steps, using the 4 calibrated corners.
void paperToSteps(float x, float y, long &sx, long &sy) {
  float u = constrain(x / PAPER_W, 0.0f, 1.0f);
  float v = constrain(y / PAPER_H, 0.0f, 1.0f);

  float w0 = (1.0f - u) * (1.0f - v);   // bottom-left
  float w1 = u * (1.0f - v);            // bottom-right
  float w2 = u * v;                     // top-right
  float w3 = (1.0f - u) * v;            // top-left

  sx = lroundf(w0 * cornerX[0] + w1 * cornerX[1] + w2 * cornerX[2] + w3 * cornerX[3]);
  sy = lroundf(w0 * cornerY[0] + w1 * cornerY[1] + w2 * cornerY[2] + w3 * cornerY[3]);
}

void goToPaper(float x, float y) {
  long sx, sy;
  paperToSteps(x, y, sx, sy);
  moveTo(sx, sy);
}

// ---------------------- DRAWING ----------------------
void drawSegment(float x0, float y0, float x1, float y1) {
  setPen(false);
  goToPaper(x0, y0);
  setPen(true);
  goToPaper(x1, y1);
  setPen(false);
}

// Parts in order: 1 head, 2 body, 3 left leg, 4 right leg, 5 left arm, 6 right arm
void drawPart(int n) {
  if (n == 1) {
    // Head: square (2.5,4) -> (3.5,4) -> (3.5,5) -> (2.5,5) -> (2.5,4)
    setPen(false);
    goToPaper(2.5f, 4.0f);
    setPen(true);
    goToPaper(3.5f, 4.0f);
    goToPaper(3.5f, 5.0f);
    goToPaper(2.5f, 5.0f);
    goToPaper(2.5f, 4.0f);
    setPen(false);
  }
  else if (n == 2) drawSegment(3.0f, 4.0f, 3.0f, 3.0f);   // body
  else if (n == 3) drawSegment(3.0f, 3.0f, 2.5f, 2.5f);   // left leg
  else if (n == 4) drawSegment(3.0f, 3.0f, 3.5f, 2.5f);   // right leg
  else if (n == 5) drawSegment(3.0f, 4.0f, 3.5f, 3.5f);   // left arm
  else if (n == 6) drawSegment(3.0f, 4.0f, 2.5f, 3.5f);   // right arm
}

void drawHangman() {
  for (int n = 1; n <= 6; n++) drawPart(n);
  returnToHome();
}

// Left edge of blank number i in a row of n blanks, centered on SLOT_CENTER_X
float slotLeft(int i, int n) {
  float total = n * SLOT_PITCH - (SLOT_PITCH - SLOT_LEN);
  return SLOT_CENTER_X - total / 2.0f + i * SLOT_PITCH;
}

void drawBlanks(int n) {
  for (int i = 0; i < n; i++) {
    float x0 = slotLeft(i, n);
    drawSegment(x0, SLOT_Y, x0 + SLOT_LEN, SLOT_Y);
  }
}

// Draw one letter with its bottom-left corner at x,y and height h (inches)
void drawLetter(char ch, float x, float y, float h) {
  if (ch >= 'a' && ch <= 'z') ch -= 32;
  if (ch < 'A' || ch > 'Z') return;

  const uint8_t* p = (const uint8_t*)pgm_read_word(&FONT[ch - 'A']);
  float s = h / 12.0f;
  bool newStroke = true;

  while (true) {
    uint8_t a = pgm_read_byte(p);
    p++;
    if (a == 254) break;
    if (a == 255) { setPen(false); newStroke = true; continue; }
    uint8_t b = pgm_read_byte(p);
    p++;

    float px = x + a * s;
    float py = y + b * s;
    if (newStroke) {
      setPen(false);
      goToPaper(px, py);
      setPen(true);
      newStroke = false;
    } else {
      goToPaper(px, py);
    }
  }
  setPen(false);
}

// Write a letter just above its blank
void writeLetterInSlot(char ch, int slot) {
  float letterW = LETTER_H * (8.0f / 12.0f);
  float x = slotLeft(slot, wordLen) + (SLOT_LEN - letterW) / 2.0f;
  drawLetter(ch, x, SLOT_Y + 0.08f, LETTER_H);
}

// ---- Face inside the head square (2.5,4)-(3.5,5), center (3, 4.5) ----
void drawMouth(bool smile) {
  setPen(false);
  const int N = 8;
  for (int i = 0; i <= N; i++) {
    float t = -1.0f + 2.0f * i / N;
    float x = 3.0f + 0.3f * t;
    float y = smile ? (4.20f + 0.15f * t * t) : (4.35f - 0.15f * t * t);
    if (i == 0) {
      goToPaper(x, y);
      setPen(true);
    } else {
      goToPaper(x, y);
    }
  }
  setPen(false);
}

void drawFace(bool smile) {
  drawSegment(2.75f, 4.65f, 2.75f, 4.75f);   // left eye
  drawSegment(3.25f, 4.65f, 3.25f, 4.75f);   // right eye
  drawMouth(smile);
}

// ---------------------- MOTION ----------------------
unsigned int rampDelay(long i, long total) {
  long fromEnd = total - 1 - i;
  long d = min(i, fromEnd);
  if (d >= RAMP_STEPS) return CRUISE_DELAY_US;
  return CRUISE_DELAY_US +
         (long)(START_DELAY_US - CRUISE_DELAY_US) * (RAMP_STEPS - d) / RAMP_STEPS;
}

// Relative move; X and Y step together so diagonals are straight.
// +dx = right (carriage), +dy = forward (gantry).
void moveSteps(long dx, long dy) {
  long ax = labs(dx);
  long ay = labs(dy);
  if (ax == 0 && ay == 0) return;

  uint8_t gantryDir   = (dy > 0) ? GANTRY_POS_LEVEL   : !GANTRY_POS_LEVEL;
  uint8_t carriageDir = (dx > 0) ? CARRIAGE_POS_LEVEL : !CARRIAGE_POS_LEVEL;

  digitalWrite(MOTOR_A_DIR, gantryDir);
  digitalWrite(MOTOR_B_DIR, gantryDir);
  digitalWrite(Z_DIR, carriageDir);
  delay(2);

  long total = max(ax, ay);
  long xAcc = 0;
  long yAcc = 0;

  for (long i = 0; i < total; i++) {
    unsigned int dly = rampDelay(i, total);

    xAcc += ax;
    yAcc += ay;
    bool stepX = (xAcc >= total);
    bool stepY = (yAcc >= total);
    if (stepX) xAcc -= total;
    if (stepY) yAcc -= total;

    if (stepX) digitalWrite(Z_STEP, HIGH);
    if (stepY) {
      digitalWrite(MOTOR_A_STEP, HIGH);
      digitalWrite(MOTOR_B_STEP, HIGH);
    }

    delayMicroseconds(dly);

    if (stepX) digitalWrite(Z_STEP, LOW);
    if (stepY) {
      digitalWrite(MOTOR_A_STEP, LOW);
      digitalWrite(MOTOR_B_STEP, LOW);
    }

    delayMicroseconds(dly);
  }

  currX += dx;
  currY += dy;
}

void moveTo(long tx, long ty) {
  moveSteps(tx - currX, ty - currY);
}

// CHANGED: pen up now goes PEN_UP_EXTRA_STEPS above the starting position.
// Pen down goes penLiftSteps below the starting position (same as before).
void setPen(bool down) {
  if (dryRun && down) return;      // preview never lowers the pen

  long target = down ? (long)penLiftSteps : -(long)PEN_UP_EXTRA_STEPS;
  long delta = target - penPos;

  if (delta != 0) {
    digitalWrite(PEN_ENABLE, LOW);
    digitalWrite(PEN_DIR, (delta > 0) ? PEN_DOWN_LEVEL : !PEN_DOWN_LEVEL);
    delay(2);

    long n = labs(delta);
    for (long i = 0; i < n; i++) {
      digitalWrite(PEN_STEP, HIGH);
      delayMicroseconds(PEN_DELAY_US);
      digitalWrite(PEN_STEP, LOW);
      delayMicroseconds(PEN_DELAY_US);
    }
    penPos = target;
    delay(100);
  }
  penIsDown = down;
}

void setGlobalHome() {
  currX = 0;
  currY = 0;
  penIsDown = false;
}

void returnToHome() {
  setPen(false);
  moveTo(0, 0);
  Serial.println(">> Returned to Home (0,0).");
}

void printPosition() {
  Serial.print(">> Position from home: X = ");
  Serial.print((float)currX / STEPS_PER_INCH, 2);
  Serial.print(" in, Y = ");
  Serial.print((float)currY / STEPS_PER_INCH, 2);
  Serial.print(" in   (jog size ");
  Serial.print(JOG_SIZES[jogIndex], 2);
  Serial.println(" in)");
}
