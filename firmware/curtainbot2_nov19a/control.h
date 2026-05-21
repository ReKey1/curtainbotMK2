// ESP32 Automatic Curtain Controller - Object Oriented
// L298N Motor Driver + Quadrature Encoders
// Two independent motors with stall detection

// ========== PIN DEFINITIONS ==========
const int M1_IN1_PIN = 16;
const int M1_IN2_PIN = 17;
const int M1_EN_PIN  = 5;
const int M1_ENC_A   = 32;
const int M1_ENC_B   = 33;

const int M2_IN1_PIN = 18;
const int M2_IN2_PIN = 19;
const int M2_EN_PIN  = 4;
const int M2_ENC_A   = 25;
const int M2_ENC_B   = 26;

const int BTN1_PIN = 21;    // Button for motor 1
const int BTN2_PIN = 22;    // Button for motor 2
const int LED1_PIN = 23;    // LED indicator for motor 1
const int LED2_PIN = 27;    // LED indicator for motor 2

// ========== MOTOR CLASS ==========
class Motor {
public:
  enum State {
    STOPPED,
    MOVING_OPEN,
    MOVING_CLOSED,
    OPEN,      // Stalled while opening (reached open limit)
    CLOSED     // Stalled while closing (reached closed limit)
  };

private:
  // Pin assignments
  int in1Pin, in2Pin, enPin;
  int encAPin, encBPin;
  int pwmChannel;
  
  // Encoder
  volatile long* encCountPtr;
  
  // PWM settings
  static const int PWM_FREQ = 20000;
  static const int PWM_RES = 8;
  
  // Movement settings (PUBLIC via setters)
  int targetSpeed;              // 0-255
  int rampTimeMs;               // Time to ramp from 0 to targetSpeed
  
  // Stall detection settings
  unsigned long checkIntervalMs;
  int minTicksPerInterval;
  unsigned long stallTimeoutMs;
  
  // State variables
  State currentState;
  int currentPWM;
  long currentPosition;  // tracks encoder position (updated on stall)
  
  // Ramping
  unsigned long rampStartTime;
  bool isRamping;
  
  // Stall detection
  unsigned long lastCheckTime;
  long lastCheckCount;
  unsigned long stallStartTime;
  
  // Direction mapping (CHANGE THESE to swap open/close direction)
  int openDirection;   // 1 or -1
  int closeDirection;  // 1 or -1

public:
  Motor(int _in1, int _in2, int _en, int _encA, int _encB, int _pwmCh, volatile long* _encPtr)
    : in1Pin(_in1), in2Pin(_in2), enPin(_en), 
      encAPin(_encA), encBPin(_encB), pwmChannel(_pwmCh),
      encCountPtr(_encPtr),
      targetSpeed(180),           // Default speed
      rampTimeMs(1000),           // Default 1000ms ramp
      checkIntervalMs(150),
      minTicksPerInterval(2),
      stallTimeoutMs(500),
      currentState(STOPPED),
      currentPWM(0),
      currentPosition(0),
      isRamping(false),
      lastCheckTime(0),
      lastCheckCount(0),
      stallStartTime(0),
      openDirection(1),           // Positive encoder = open (CHANGE if needed)
      closeDirection(-1)          // Negative encoder = close (CHANGE if needed)
  {}
  
  void begin() {
    // Setup pins
    pinMode(in1Pin, OUTPUT);
    pinMode(in2Pin, OUTPUT);
    pinMode(enPin, OUTPUT);
    pinMode(encAPin, INPUT_PULLUP);
    pinMode(encBPin, INPUT_PULLUP);
    
    // Setup PWM (compatible with ESP32 Arduino core 3.x+)
    ledcAttach(enPin, PWM_FREQ, PWM_RES);
    
    // Initialize stopped
    setMotorPWM(0);
    
    // Initialize position from encoder
    currentPosition = *encCountPtr;
  }
  
  // ===== Configuration Setters =====
  void setSpeed(int speed) { targetSpeed = constrain(speed, 0, 255); }
  void setRampTime(int ms) { rampTimeMs = ms; }
  void setStallCheckInterval(int ms) { checkIntervalMs = ms; }
  void setMinTicksPerInterval(int ticks) { minTicksPerInterval = ticks; }
  void setStallTimeout(int ms) { stallTimeoutMs = ms; }
  
  // Swap directions if motors are wired backwards
  void reverseOpenDirection() { openDirection = -openDirection; }
  void reverseCloseDirection() { closeDirection = -closeDirection; }
  
  // ===== Movement Commands =====
  void moveOpen() {
    if (currentState == MOVING_OPEN) return; // Already moving open
    startMovement(openDirection);
    currentState = MOVING_OPEN;
  }
  
  void moveClosed() {
    if (currentState == MOVING_CLOSED) return;
    startMovement(closeDirection);
    currentState = MOVING_CLOSED;
  }
  
  void stop() {
    setMotorPWM(0);
    currentState = STOPPED;
    isRamping = false;
    stallStartTime = 0;
    currentPosition = *encCountPtr; // Update position on manual stop
  }
  
  // ===== Status Getters =====
  State getState() const { return currentState; }
  bool isMoving() const { return currentState == MOVING_OPEN || currentState == MOVING_CLOSED; }
  bool isOpen() const { return currentState == OPEN; }
  bool isClosed() const { return currentState == CLOSED; }
  long getPosition() const { return currentPosition; }
  long getEncoderCount() const { return *encCountPtr; }
  
  // ===== Main Update (call in loop) =====
  void update() {
    if (!isMoving()) return;
    
    unsigned long now = millis();
    
    // Handle ramping
    if (isRamping) {
      unsigned long elapsed = now - rampStartTime;
      if (elapsed >= rampTimeMs) {
        // Ramp complete
        currentPWM = targetSpeed;
        isRamping = false;
        lastCheckTime = now;
        lastCheckCount = *encCountPtr;
        Serial.println("Ramp complete, stall detection active");
      } else {
        // Still ramping - linear interpolation
        currentPWM = map(elapsed, 0, rampTimeMs, 0, targetSpeed);
      }
      setMotorPWM(currentPWM * ((currentState == MOVING_OPEN) ? openDirection : closeDirection));
      return; // No stall detection during ramp
    }
    
    // Stall detection (only after ramp)
    if (now - lastCheckTime >= checkIntervalMs) {
      long currentCount = *encCountPtr;
      long delta = abs(currentCount - lastCheckCount);
      
      if (delta < minTicksPerInterval) {
        // Low movement detected
        if (stallStartTime == 0) {
          stallStartTime = now;
        } else if (now - stallStartTime >= stallTimeoutMs) {
          // Stall confirmed - set state based on direction
          setMotorPWM(0);
          if (currentState == MOVING_OPEN) {
            currentState = OPEN;
            Serial.println("OPEN limit reached");
          } else if (currentState == MOVING_CLOSED) {
            currentState = CLOSED;
            Serial.println("CLOSED limit reached");
          }
          currentPosition = *encCountPtr; // Update position
          return;
        }
      } else {
        // Movement detected, reset stall timer
        stallStartTime = 0;
      }
      
      lastCheckCount = currentCount;
      lastCheckTime = now;
    }
  }
  
private:
  void startMovement(int direction) {
    isRamping = true;
    rampStartTime = millis();
    currentPWM = 0;
    stallStartTime = 0;
    lastCheckTime = millis();
    lastCheckCount = *encCountPtr;
    
    // Set motor direction
    int pwmSigned = direction; // Just for direction, will ramp PWM magnitude
    setMotorPWM(pwmSigned);
  }
  
  void setMotorPWM(int speedSigned) {
    // speedSigned: -255 to 255
    int pwm = abs(speedSigned);
    if (pwm > 255) pwm = 255;
    
    if (speedSigned > 0) {
      digitalWrite(in1Pin, HIGH);
      digitalWrite(in2Pin, LOW);
    } else if (speedSigned < 0) {
      digitalWrite(in1Pin, LOW);
      digitalWrite(in2Pin, HIGH);
    } else {
      digitalWrite(in1Pin, LOW);
      digitalWrite(in2Pin, LOW);
    }
    
    ledcWrite(enPin, pwm);
  }
};

// ========== GLOBAL ENCODER VARIABLES ==========
volatile long encCount1 = 0;
volatile long encCount2 = 0;

// ========== ENCODER ISRs ==========
void IRAM_ATTR isrEnc1() {
  bool b = digitalRead(M1_ENC_B);
  if (b) encCount1++; else encCount1--;
}

void IRAM_ATTR isrEnc2() {
  bool b = digitalRead(M2_ENC_B);
  if (b) encCount2++; else encCount2--;
}

// ========== MOTOR INSTANCES ==========
Motor motor1(M1_IN1_PIN, M1_IN2_PIN, M1_EN_PIN, M1_ENC_A, M1_ENC_B, 0, &encCount1);
Motor motor2(M2_IN1_PIN, M2_IN2_PIN, M2_EN_PIN, M2_ENC_A, M2_ENC_B, 1, &encCount2);

// ========== SETUP ==========
void controlSetup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Curtain Controller Starting...");
  
  // Setup buttons (active LOW with internal pullup)
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  
  // Setup LEDs
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  
  // Initialize motors
  motor1.begin();
  motor2.begin();
  
  // Motor 1 direction reversal (from your config)
  motor1.reverseOpenDirection();
  motor1.reverseCloseDirection();
  motor2.reverseOpenDirection();
  motor2.reverseCloseDirection();  
  
  // Configure motor parameters
  motor1.setSpeed(180);
  motor2.setSpeed(180);
  motor1.setRampTime(1000);
  motor2.setRampTime(1000);
  
  // Stall detection settings
  motor1.setStallCheckInterval(30);
  motor1.setMinTicksPerInterval(15);
  motor1.setStallTimeout(5);
  
  motor2.setStallCheckInterval(30);
  motor2.setMinTicksPerInterval(15);
  motor2.setStallTimeout(5);
  
  // Attach encoder interrupts
  attachInterrupt(digitalPinToInterrupt(M1_ENC_A), isrEnc1, RISING);
  attachInterrupt(digitalPinToInterrupt(M2_ENC_A), isrEnc2, RISING);
  
  Serial.println("Ready!");
  Serial.println("Press buttons to control curtains");
}

// ========== MAIN LOOP ==========
void controlLoop() {
  // Update motors (handles ramping and stall detection)
  motor1.update();
  motor2.update();
  
  // Update LED indicators - turn on when motor is moving
  digitalWrite(LED1_PIN, motor1.isMoving() ? HIGH : LOW);
  digitalWrite(LED2_PIN, motor2.isMoving() ? HIGH : LOW);
  
  // ===== Button Control with Debouncing =====
  static bool btn1LastStableState = HIGH;
  static bool btn2LastStableState = HIGH;
  static bool btn1Reading = HIGH;
  static bool btn2Reading = HIGH;
  static unsigned long btn1LastChangeTime = 0;
  static unsigned long btn2LastChangeTime = 0;
  static bool btn1PressDetected = false;
  static bool btn2PressDetected = false;
  const unsigned long debounceDelay = 20; // Reduced to 20ms for faster response
  
  bool btn1Current = digitalRead(BTN1_PIN);
  bool btn2Current = digitalRead(BTN2_PIN);
  unsigned long now = millis();
  
  // Button 1 debouncing
  if (btn1Current != btn1Reading) {
    btn1LastChangeTime = now;
    btn1Reading = btn1Current;
  }
  
  if ((now - btn1LastChangeTime) > debounceDelay) {
    // Button state has been stable
    if (btn1Reading != btn1LastStableState) {
      btn1LastStableState = btn1Reading;
      
      // Detect button press (LOW)
      if (btn1LastStableState == LOW) {
        btn1PressDetected = true;
      }
      
      // Detect button release (HIGH) - trigger action if press was detected
      if (btn1LastStableState == HIGH && btn1PressDetected) {
        btn1PressDetected = false;
        if (motor1.isMoving()) {
          motor1.stop();
          Serial.println("Motor 1: Stopped by button");
        } else {
          if (motor1.isClosed() || motor1.getState() == Motor::STOPPED) {
            motor1.moveOpen();
            Serial.println("Motor 1: Opening");
          } else {
            motor1.moveClosed();
            Serial.println("Motor 1: Closing");
          }
        }
      }
    }
  }
  
  // Button 2 debouncing
  if (btn2Current != btn2Reading) {
    btn2LastChangeTime = now;
    btn2Reading = btn2Current;
  }
  
  if ((now - btn2LastChangeTime) > debounceDelay) {
    if (btn2Reading != btn2LastStableState) {
      btn2LastStableState = btn2Reading;
      
      if (btn2LastStableState == LOW) {
        btn2PressDetected = true;
      }
      
      if (btn2LastStableState == HIGH && btn2PressDetected) {
        btn2PressDetected = false;
        if (motor2.isMoving()) {
          motor2.stop();
          Serial.println("Motor 2: Stopped by button");
        } else {
          if (motor2.isClosed() || motor2.getState() == Motor::STOPPED) {
            motor2.moveOpen();
            Serial.println("Motor 2: Opening");
          } else {
            motor2.moveClosed();
            Serial.println("Motor 2: Closing");
          }
        }
      }
    }
  }
}

// ===== Status Monitoring =====
String getStatusString() {
  String status = "M1: ";
  
  switch(motor1.getState()) {
    case Motor::STOPPED: status += "STOPPED"; break;
    case Motor::MOVING_OPEN: status += "MOVING_OPEN"; break;
    case Motor::MOVING_CLOSED: status += "MOVING_CLOSED"; break;
    case Motor::OPEN: status += "OPEN"; break;
    case Motor::CLOSED: status += "CLOSED"; break;
  }
  
  status += " Enc:";
  status += String(motor1.getEncoderCount());
  
  status += " | M2: ";
  
  switch(motor2.getState()) {
    case Motor::STOPPED: status += "STOPPED"; break;
    case Motor::MOVING_OPEN: status += "MOVING_OPEN"; break;
    case Motor::MOVING_CLOSED: status += "MOVING_CLOSED"; break;
    case Motor::OPEN: status += "OPEN"; break;
    case Motor::CLOSED: status += "CLOSED"; break;
  }
  
  status += " Enc:";
  status += String(motor2.getEncoderCount());
  
  return status;
}