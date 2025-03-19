/*
 * Name: HourGlass
 * Author: eggseal
 * Version: 1.0
 * Date: 2024/09/04
 * Description: 
 *   This program emulates the behaviour of an hourglass displayed in two LED matrices, the duration of the hourglass can be changed
 *   from its integrated buttons, the circuit can be flipped, behaving like a real hourglass, the system can be completely shut off
 *   as to preserve battery life.
 */

// Libraries
#include <U8g2lib.h>  // "U8g2" - "Oliver"

// Macros
// #define SIMULATION
// #define DEBUG

// Pin I/O
const uint8_t ROW[] = { 2, 3, 4, 5, 6 };    // The row of LEDs to turn on
const uint8_t COL[] = { 7, 8, 9, 10, 11 };  // The column of LEDs to turn on

#define BY5 34  // HIGH if the time changes by 5s
#define ADD 35  // HIGH if the time is incrementing
#define SUB 36  // HIGH if the time is decrementing
#define ROT 37  // Rotation sensor that says if the clock is upside down
#define MUX 13  // Toggles between both sides of the clock to reuse pins

// Constants
#define N 5                           // Size of the matrix
#define DOTS 22                       // Number of LEDs turned on at once
#define SCREEN_WIDTH 128              // Width in pixels of the OLED display
#define SCREEN_HEIGHT 64              // Height in pixels of the OLED display
#define TIME_POS_Y 38                 // Y coordinates of vertically aligned text
#define OLED_DISPLAY_INTERVAL 100     // Interval to update the OLED display
#define MAX_MIDDLE_FALL_INTERVAL 250  // Interval to update the middle falling dot
#define MAX_DURATION 600000           // Maximum allowed timer length
#define MIN_DURATION 5000             // Minimum allowed timer length

// Variables
bool lastAddState = false;  // The state of the ADD button in the previous iteration
bool lastSubState = false;  // The state of the SUB button in the previous iteration
bool lastRotState = false;  // The state of the rotation in the previous iteration, FALSE == UPRIGHT

unsigned long length = 20000;                               // Countdown timer (ms)
int minutes = 0, seconds = 0, timeWidth = 0, timePosX = 0;  // Values needed to write the length to the display
String timestamp = "";                                      // The time formatted in [m:ss]
int lastCircle = 0;
float secsPerDot = length / DOTS;  // How much time (ms) is represented by one dot
int dotsRemoved = 0;               // Counter of how many dots have been removed from the top

int m = N - 1;                    // The index of the falling dot animation
unsigned long lastMiddleDot = 0;  // Relative time since last falling dot index change
unsigned long lastDisplay = 0;    // Relative time since last OLED update
int dotsPlaced = 0;               // Counter of how many dots have been placed at the bottom
bool reachedSurface = false;      // Flag if the falling dot reached the bottom dots
bool oldMiddleState = false;      // Old state of the overwritten dot in the diagonal

bool sel = 1;        // The current mux toggle
long resetTime = 0;  // Execution time in which the clock was restarted
bool SIDE1[N][N] = {
  // State matrix of the LEDs
  { 0, 0, 1, 1, 1 },
  { 0, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 1 },
};
bool SIDE2[N][N] = {
  // State matrix of the LEDs
  { 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0 },
};
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// Macro subroutines
#ifdef DEBUG
#define PRINT_DURATION(total, interval) \
  do { \
    Serial.print("Duration: "); \
    Serial.print(total); \
    Serial.print(" :: Interval: "); \
    Serial.println(interval); \
  } while (0)
#define PRINT_DOT_STATES(placed, removed, total) \
  do { \
    Serial.print("Dots Removed: "); \
    Serial.print(removed); \
    Serial.print("/"); \
    Serial.print(total); \
    Serial.print(" :: Dots Placed: "); \
    Serial.print(placed); \
    Serial.print("/"); \
    Serial.println(total); \
  } while (0)
#define PRINT_COMPARE_TIME(label, time) \
  do { \
    Serial.print("Current: "); \
    Serial.print(millis()); \
    Serial.print(" :: "); \
    Serial.print(label); \
    Serial.print(": "); \
    Serial.println(time); \
  } while (0)
#define PRINT_TIMESTAMP(label, var) \
  do { \
    Serial.print("["); \
    Serial.print(millis()); \
    Serial.print("]: "); \
    Serial.print(label); \
    Serial.print(" "); \
    Serial.println(var); \
  } while (0)
#define PRINT_MATRIX(matrix, indent) \
  do { \
    for (uint8_t r = 0; r < N; r++) { \
      for (uint8_t i = 0; i < indent; i++) \
        Serial.print("  "); \
      for (uint8_t c = 0; c < N; c++) { \
        Serial.print(matrix[r][c] ? 1 : 0); \
        Serial.print(" "); \
      } \
      Serial.println(); \
    } \
  } while (0)
#else
#define PRINT_DURATION(total, interval)
#define PRINT_DOT_STATES(placed, removed, total)
#define PRINT_COMPARE_TIME(label, time)
#define PRINT_TIMESTAMP(label, var)
#define PRINT_MATRIX(matrix, indent)
#endif

#ifdef SIMULATION
/**
 * Sends a matrix on a MatLab format on a single line to the Serial port, intended for simulating the LEDs from another program
 * @param matrix The state matrix that's being rendered
 */
void sendClockSerial() {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      Serial.print(SIDE1[i][j] ? 1 : 0);
      if (j < N - 1)
        Serial.print(",");
    }
    Serial.print(";");  // Newline after each row
  }
  Serial.println();
  delay(50);  // Delay to avoid flooding the serial port
  for (int i = N - 1; i >= 0; i--) {
    for (int j = N - 1; j >= 0; j--) {
      Serial.print(SIDE2[i][j] ? 1 : 0);
      if (j > 0)
        Serial.print(",");
    }
    Serial.print(";");  // Newline after each row
  }
  Serial.println();
  delay(50);  // Delay to avoid flooding the serial port
}
#else
void sendClockSerial() {}
#endif

// Subroutines
/**
 * Inverts the values of two states whose transition is not immediate
 * @param TOTAL The maximum number of dots
 * @param stateA The amount currently on the state A
 * @param stateB The amount currently on the state B
 */
void swapState(int TOTAL, int& stateA, int& stateB) {
  int prevB = stateB;
  stateB = TOTAL - stateA;
  stateA = TOTAL - prevB;
}
bool turnRemovedOff(int counter) {
  return counter > 0 ? LOW : HIGH;
}
bool turnPlacedOn(int counter) {
  return counter > 0 ? HIGH : LOW;
}
void iterateDiagonally(bool (&side)[N][N], int counter, bool (*execute)(int)) {
  uint8_t i = 0, j = 0;
  // Iterate until it reaches position NxN
  while (i + j <= (N - 1) * 2) {
    // Go diagonally through every diagonal position from bottom left to top right
    for (uint8_t r = i, c = j; r >= j && c <= i; r--, c++) {
      side[r][c] = execute(counter);
      counter--;
    }
    // Move the diagonal one position to the right
    if (i >= N - 1) j++;
    else i++;
  }
}
/**
 * Draws a state matrix onto the LEDs one row at a time
 * @param side The state matrix that is being drawn
 */
void drawMatrix(bool side[N][N], bool mux) {
  digitalWrite(MUX, mux);
  // Turn HIGH every row one by one while turning LOW every one on the side matrix row
  for (uint8_t r = 0; r < N; r++) {
    digitalWrite(ROW[r], LOW);
    for (uint8_t c = 0; c < N; c++)
      digitalWrite(COL[c], !(side)[r][c]);
    // Wait 1ms before turning the row LOW to allow the eye to see it
    delay(10);
    digitalWrite(ROW[r], HIGH);
  }
}

void setup() {
  // Pin definition
  pinMode(BY5, INPUT);
  pinMode(ADD, INPUT);
  pinMode(SUB, INPUT);
  pinMode(ROT, INPUT);
  pinMode(MUX, OUTPUT);
  for (uint8_t i = 0; i < N; i++) {
    pinMode(ROW[i], OUTPUT);
    pinMode(COL[i], OUTPUT);

    // Output clearing
    digitalWrite(ROW[i], HIGH);
    digitalWrite(COL[i], HIGH);
  }
  digitalWrite(MUX, HIGH);

  // Comms
  Serial.begin(115200);

  // Variable initialization
  lastAddState = digitalRead(ADD);
  lastSubState = digitalRead(SUB);
  lastRotState = digitalRead(ROT);

  // Library initialization
  display.begin();
  display.setFont(u8g2_font_t0_22b_tf);
}

void loop() {
  // Handle length changes depending on which button just got pressed
  int diff = digitalRead(BY5) ? 5000 : 1000;
  if (!lastAddState && digitalRead(ADD)) length = min(length + diff, MAX_DURATION);
  else if (!lastSubState && digitalRead(SUB)) length = max(length - diff, MIN_DURATION);
  lastAddState = digitalRead(ADD);
  lastSubState = digitalRead(SUB);

  // Update the interval depending on the new duration
  if (secsPerDot != length / DOTS) {
    secsPerDot = length / DOTS;
    PRINT_DURATION(length, secsPerDot);
  }

  // If the clock's rotation changed, swap the top and bottom sides and restart the clock at the current elapsed time
  if (lastRotState != digitalRead(ROT)) {
    PRINT_DOT_STATES(dotsPlaced, dotsRemoved, DOTS);
    swapState(DOTS, dotsPlaced, dotsRemoved);
    PRINT_DOT_STATES(dotsPlaced, dotsRemoved, DOTS);
    resetTime = millis() - dotsRemoved * secsPerDot;  // Current time minus the time it would take for the already removed dots to be removed
    PRINT_COMPARE_TIME("Reset", resetTime);
  }

  // Swap bottom and top depending on the rotation of the clock
  lastRotState = digitalRead(ROT);
  bool(&top)[N][N] = lastRotState ? SIDE2 : SIDE1;
  bool(&bot)[N][N] = lastRotState ? SIDE1 : SIDE2;

  // Draws the top side if there's still dots to be removed and the time has surpassed the next dot time step
  if (dotsRemoved < DOTS && millis() > resetTime + (dotsRemoved + 1) * secsPerDot) {
    dotsRemoved++;                               // Add the removed dot to the counter
    int toTurnOff = N * N - DOTS + dotsRemoved;  // The always empty dots plus the dots that have been removed
    iterateDiagonally(top, toTurnOff, turnRemovedOff);
    PRINT_TIMESTAMP("Removed Dots", dotsRemoved);
  }

  // Draw the bottom side if there's a removed dot that hasn't been placed and the falling dot reached the surface of the bottom
  if (dotsPlaced < dotsRemoved && reachedSurface) {
    reachedSurface = false;  // Reset the surface flag
    dotsPlaced = dotsRemoved;
    iterateDiagonally(bot, dotsPlaced, turnPlacedOn);
    PRINT_TIMESTAMP("Placed Dots", dotsPlaced);
    PRINT_MATRIX(SIDE1, 0);
    PRINT_MATRIX(SIDE2, N);
    PRINT_COMPARE_TIME("Current Clock", millis() - resetTime);
  }

  // Change the index of the diagonal dot if all the dots haven't been placed on every interval
  if (dotsPlaced < DOTS && millis() - lastMiddleDot > min(secsPerDot / N, MAX_MIDDLE_FALL_INTERVAL)) {
    (bot)[m][m] = m == 0 ? 1 : oldMiddleState;
    m = (m - 1 + N) % N;  // Change to the next diagonal position

    reachedSurface = !oldMiddleState && ((bot)[m][m] || m == 0);  // It reached the surface if previously the dot was placed on an off LED and now it's on an on LED or at the bottom of the clock
    oldMiddleState = (bot)[m][m];                                 // Save the current state of the LED for the next iteration
    (bot)[m][m] = 1;                                              // Turn on the falling dot
    lastMiddleDot = millis();                                     // Update relative time
    PRINT_TIMESTAMP("Middle Index", m);
  }

  // Draw to screen
  drawMatrix(SIDE1, HIGH);
  drawMatrix(SIDE2, LOW);
  sendClockSerial();

  // Draw to OLED screen
  if (millis() - lastDisplay > OLED_DISPLAY_INTERVAL) {
    // Extract minutes and seconds from the length and format it in [m:ss]
    minutes = length / 1000 / 60;
    seconds = length / 1000 % 60;
    timestamp = String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
    const char* timestampChar = timestamp.c_str();  // Convert the timestamp to char* for operations

    // Calculate position in display matrix to center the timestamp
    timeWidth = display.getStrWidth(timestampChar);
    timePosX = (SCREEN_WIDTH - timeWidth) / 2;

    // Write the time to the buffer
    display.clearBuffer();
    display.drawStr(timePosX, TIME_POS_Y, timestampChar);
    display.drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, (SCREEN_HEIGHT - 1) / 2);
    display.drawLine((SCREEN_WIDTH - timeWidth) / 2, 40, (SCREEN_WIDTH + timeWidth) / 2, 40);
    display.sendBuffer();
    lastDisplay = millis();
  }
}