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
#define N 5      // Size of the matrix
#define DOTS 22  // Number of LEDs turned on at once

// Variables
bool lastAddState = false;  // The state of the ADD button in the previous iteration
bool lastSubState = false;  // The state of the SUB button in the previous iteration
bool lastRotState = false;  // The state of the rotation in the previous iteration

unsigned int length = 20000;       // Countdown timer (ms)
float secsPerDot = length / DOTS;  // How much time (ms) is represented by one dot
int dotsRemoved = 0;               // Counter of how many dots have been removed from the top

int m = N - 1;                    // The index of the falling dot animation
unsigned long lastMiddleDot = 0;  // Relative time since last falling dot index change
int dotsPlaced = 0;               // Counter of how many dots have been placed at the bottom
bool reachedSurface = false;      // Flag if the falling dot reached the bottom dots
bool oldMiddleState = false;      // Old state of the overwritten dot in the diagonal

bool sel = 1;                 // The current mux toggle
unsigned long resetTime = 0;  // Execution time in which the clock was restarted
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

// Subroutines
/**
 * Draws a state matrix onto the LEDs one row at a time
 * @param side The state matrix that is being drawn
 */
void drawScreen(bool (*side)[N][N]) {
  // Turn HIGH every row one by one while turning LOW every one on the side matrix row
  for (uint8_t r = 0; r < N; r++) {
    digitalWrite(ROW[r], LOW);
    for (uint8_t c = 0; c < N; c++)
      digitalWrite(COL[c], !(*side)[r][c]);
    // Wait 1ms before turning the row LOW to allow the eye to see it
    delay(1);
    digitalWrite(ROW[r], HIGH);
  }
}
#ifdef DEBUG
/**
 * Prints a matrix to the Serial monitor for debugging purposes
 * @param matrix The state matrix that is being drawn
 */
void printMatrix(bool matrix[N][N]) {
  // Iterate through every item of the matrix and print it, separating the rows into new lines
  for (uint8_t r = 0; r < N; r++) {
    for (uint8_t c = 0; c < N; c++) {
      Serial.print(matrix[r][c] ? 1 : 0);
      Serial.print(" ");
    }
    Serial.println();
  }
}
#endif
#ifdef SIMULATION
/**
 * Sends a matrix on a MatLab format on a single line to the Serial port, intended for simulating the LEDs from another program
 * @param matrix The state matrix that's being rendered
 */
void sendClockSerial(bool matrix[N][N]) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      Serial.print((*top)[i][j] ? 1 : 0);
      if (j < N - 1)
        Serial.print(",");
    }
    Serial.print(";");  // Newline after each row
  }
  Serial.println();
  delay(50);  // Delay to avoid flooding the serial port
  for (int i = N - 1; i >= 0; i--) {
    for (int j = N - 1; j >= 0; j--) {
      Serial.print((*bot)[i][j] ? 1 : 0);
      if (j > 0)
        Serial.print(",");
    }
    Serial.print(";");  // Newline after each row
  }
  Serial.println();
  delay(50);  // Delay to avoid flooding the serial port
}
#endif

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
  Serial.begin(9600);
#ifdef DEBUG
  Serial.print("ms/dot: ");
  Serial.println(secsPerDot);
#endif

  // Variable initialization
  lastAddState = digitalRead(ADD);
  lastSubState = digitalRead(SUB);
  lastRotState = digitalRead(ROT);
}

void loop() {
  // Handle length changes depending on which button just got pressed
  int diff = digitalRead(BY5) ? 5 : 1;
  if (!lastAddState && digitalRead(ADD)) length += diff * 1000;
  else if (!lastSubState && digitalRead(SUB)) length -= diff * 1000;
  lastAddState = digitalRead(ADD);
  lastSubState = digitalRead(SUB);

  // If the clock's rotation changed, swap the top and bottom sides and restart the clock at the current elapsed time
  if (lastRotState != digitalRead(ROT)) {
#ifdef DEBUG
    Serial.print("Reset: R: ");
    Serial.print(dotsRemoved);
    Serial.print(", P: ");
    Serial.println(dotsPlaced);
#endif
    dotsRemoved = DOTS - dotsPlaced;                    // The dots removed become the one's that haven't been placed
    dotsPlaced = DOTS - dotsRemoved;                    // The dots placed become the one's that haven't been removed
    resetTime = millis() - (dotsRemoved * secsPerDot);  // Current time minus the time it would take for the already removed dots to be removed
#ifdef DEBUG
    Serial.print("Millis: ");
    Serial.print(millis());
    Serial.print("Reset: ");
    Serial.println(resetTime);
#endif
  }
  lastRotState = digitalRead(ROT);

  // Swap bottom and top depending on the rotation of the clock
  lastRotState = digitalRead(ROT);
  bool(*top)[N][N] = lastRotState ? &SIDE2 : &SIDE1;
  bool(*bot)[N][N] = lastRotState ? &SIDE1 : &SIDE2;

  // Draws the top side if there's still dots to be removed and the time has surpassed the next dot time step
  if (dotsRemoved < DOTS && millis() > resetTime + (dotsRemoved + 1) * secsPerDot) {
    dotsRemoved++;  // Add the removed dot to the counter
#ifdef DEBUG
    Serial.print(millis());
    Serial.print(": Removed Dot #");
    Serial.println(dotsRemoved);
#endif
    int toTurnOff = N * N - DOTS + dotsRemoved;  // The always empty dots plus the dots that have been removed
    uint8_t i = 0, j = 0;
    // Iterate until it reaches position NxN
    while (i + j <= N * 2 - 2) {
      // Go diagonally through every diagonal position from bottom left to top right
      for (uint8_t r = i, c = j; r >= j && c <= i; r--, c++) {
        (*top)[r][c] = toTurnOff <= 0 ? HIGH : LOW;
        toTurnOff--;
      }
      // Move the diagonal one position to the right
      if (i >= N - 1) j++;
      else i++;
    }
  }

  // Draw the bottom side if there's a removed dot that hasn't been placed and the falling dot reached the surface of the bottom
  if (dotsPlaced < dotsRemoved && reachedSurface) {
    reachedSurface = false;  // Reset the surface flag
    dotsPlaced = 0;          // Reset the placing counter
    uint8_t i = 0, j = 0;
    // Iterate until it reaches position NxN
    while (i + j <= N * 2 - 2) {
      // Go diagonally through every diagonal position from bottom left to top right
      for (uint8_t r = i, c = j; r >= j && c <= i; r--, c++) {
        (*bot)[r][c] = dotsPlaced >= dotsRemoved ? LOW : HIGH;
        dotsPlaced += (*bot)[r][c];
      }
      // Move the diagonal one position to the right
      if (i >= N - 1) j++;
      else i++;
    }
#ifdef DEBUG
    Serial.print(millis());
    Serial.print(": Placed: Dot #");
    Serial.println(dotsPlaced);
    printMatrix(SIDE1);
    printMatrix(SIDE2);
    Serial.println(millis() - resetTime);
#endif
  }

  // Change the index of the diagonal dot if all the dots haven't been placed on every interval
  if (dotsPlaced < DOTS && millis() - lastMiddleDot > min(secsPerDot / N, 250)) {
    (*bot)[m][m] = m == 0 && dotsPlaced > 0 ? 1 : oldMiddleState;
    m = (m - 1 + N) % N;  // Change to the next diagonal position

    reachedSurface = !oldMiddleState && ((*bot)[m][m] || m == 0);  // It reached the surface if previously the dot was placed on an off LED and now it's on an on LED or at the bottom of the clock
    oldMiddleState = (*bot)[m][m];                                 // Save the current state of the LED for the next iteration
    (*bot)[m][m] = 1;                                              // Turn on the falling dot
    lastMiddleDot = millis();                                      // Update relative time
#ifdef DEBUG
    Serial.print("Mid: ");
    Serial.println(m);
#endif
  }

  // Draw to screen
  digitalWrite(MUX, HIGH);
  drawScreen(&SIDE1);
  digitalWrite(MUX, LOW);
  drawScreen(&SIDE2);

#ifdef SIMULATION
  sendClockSerial(top);
  sendClockSerial(bot);
#endif
}
