#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define NUM_BALLS     8
#define BUZZER_PIN    2      // D4 on ESP8266 (GPIO 2)

// Physics settings tuned for 100 Hz sub-stepping
#define DAMPING       0.9975f   // Higher per step because steps are smaller (10ms)
#define RESTITUTION   0.85f
#define REPULSION_STR 1.75f     // Halved force per sub-step
#define GRAVITY_SCALE 0.0375f   // Halved gravity per sub-step

TFT_eSPI tft = TFT_eSPI();
Adafruit_MPU6050 mpu;
bool mpuPresent = false; // Flag to track MPU6050 hardware status

struct Ball {
  float x, y;
  float vx, vy;
  float radius;
  float mass;
  uint16_t color;
};

Ball balls[NUM_BALLS];
Ball prevBalls[NUM_BALLS];
unsigned long toneEndTime = 0;

void playSound(unsigned int frequency, int duration) {
  if(duration > 0){
    //if((int)duration >= toneEndTime - millis()){
      tone(BUZZER_PIN, frequency);
      toneEndTime = millis() + duration;
    //}
  }
}

void updateAudio() {
  if (toneEndTime > 0 && millis() >= toneEndTime) {
    noTone(BUZZER_PIN);
    toneEndTime = 0;
  }
}

uint16_t getRandomColor() {
  uint8_t r = random(60, 256);
  uint8_t g = random(60, 256);
  uint8_t b = random(60, 256);
  return tft.color565(r, g, b);
}

void initBalls() {
  for (int i = 0; i < NUM_BALLS; i++) {
    balls[i].radius = random(7, 16);
    balls[i].mass = balls[i].radius * balls[i].radius;
    
    balls[i].x = random((int)balls[i].radius, SCREEN_WIDTH - (int)balls[i].radius);
    balls[i].y = random((int)balls[i].radius, SCREEN_HEIGHT - (int)balls[i].radius);
    
    balls[i].vx = 0.0f;
    balls[i].vy = 0.0f;
    
    balls[i].color = getRandomColor();
  }
}

void updatePhysics(uint16_t touchX, uint16_t touchY, bool touched, float gravX, float gravY) {
  bool hitWallThisFrame = false;

  for (int i = 0; i < NUM_BALLS; i++) {
    // 1. Apply MPU6050 Gravity Tilt
    balls[i].vx += gravX * GRAVITY_SCALE;
    balls[i].vy += gravY * GRAVITY_SCALE;

    // 2. Touch Repulsion ("Whitehole")
    if (touched) {
      float dx = balls[i].x - touchX;
      float dy = balls[i].y - touchY;
      float distSq = dx * dx + dy * dy;
      float minDist = 80.0f;

      if (distSq < minDist * minDist && distSq > 0.1f) {
        float dist = sqrtf(distSq);
        float force = (1.0f - (dist / minDist)) * REPULSION_STR;
        balls[i].vx += (dx / dist) * force;
        balls[i].vy += (dy / dist) * force;
      }
    }

    // 3. Velocity & Damping Update
    balls[i].vx *= DAMPING;
    balls[i].vy *= DAMPING;
    balls[i].x += balls[i].vx;
    balls[i].y += balls[i].vy;

    float avgRadius = balls[i].radius;
    unsigned int freq = map((long)avgRadius, 6, 17, 240, 60);
        
    // 4. Boundary Collisions
    if (balls[i].x - balls[i].radius < 0) {
      balls[i].x = balls[i].radius;
      balls[i].vx = -balls[i].vx * RESTITUTION;
      playSound(freq, abs(balls[i].vx));
    } else if (balls[i].x + balls[i].radius >= SCREEN_WIDTH) {
      balls[i].x = SCREEN_WIDTH - 1 - balls[i].radius;
      balls[i].vx = -balls[i].vx * RESTITUTION;
      playSound(freq, abs(balls[i].vx));
    }

    if (balls[i].y - balls[i].radius < 0) {
      balls[i].y = balls[i].radius;
      balls[i].vy = -balls[i].vy * RESTITUTION;
      playSound(freq, abs(balls[i].vy));
    } else if (balls[i].y + balls[i].radius >= SCREEN_HEIGHT) {
      balls[i].y = SCREEN_HEIGHT - 1 - balls[i].radius;
      balls[i].vy = -balls[i].vy * RESTITUTION;
      playSound(freq, abs(balls[i].vy));
    }
  }

  // Play wall bump sound (Low pitch pop)
  if (hitWallThisFrame) {
    //playSound(180, 12);
  }

  // 5. Elastic Ball-to-Ball Collisions
  for (int i = 0; i < NUM_BALLS; i++) {
    for (int j = i + 1; j < NUM_BALLS; j++) {
      float dx = balls[j].x - balls[i].x;
      float dy = balls[j].y - balls[i].y;
      float distSq = dx * dx + dy * dy;
      float minDist = balls[i].radius + balls[j].radius;

      if (distSq < minDist * minDist && distSq > 0.0001f) {
        float dist = sqrtf(distSq);
        float overlap = 0.5f * (minDist - dist);

        float nx = dx / dist;
        float ny = dy / dist;
        balls[i].x -= nx * overlap;
        balls[i].y -= ny * overlap;
        balls[j].x += nx * overlap;
        balls[j].y += ny * overlap;

        float kx = balls[i].vx - balls[j].vx;
        float ky = balls[i].vy - balls[j].vy;
        float p = 2.0f * (nx * kx + ny * ky) / (balls[i].mass + balls[j].mass);

        balls[i].vx -= p * balls[j].mass * nx * RESTITUTION;
        balls[i].vy -= p * balls[j].mass * ny * RESTITUTION;
        balls[j].vx += p * balls[i].mass * nx * RESTITUTION;
        balls[j].vy += p * balls[i].mass * ny * RESTITUTION;

        // Dynamic pitch based on ball size (Smaller = Higher Pitch)
        // And duration based on velocity (Stronger = Longer Duration)
        // IM NOT USING REAL VELOCITY DATA THERE, only track based on
        // position changes, bcz sometimes velocity high when balls are
        // squeezed together, i dont want that ugly sound.
        int strength = 1 * sqrtf (
          (( balls[i].x - prevBalls[i].x ) - ( balls[j].x - prevBalls[j].x)) *
          (( balls[i].x - prevBalls[i].x ) - ( balls[j].x - prevBalls[j].x)) +
          (( balls[i].y - prevBalls[i].y ) - ( balls[j].y - prevBalls[j].y)) *
          (( balls[i].y - prevBalls[i].y ) - ( balls[j].y - prevBalls[j].y)) );

        float avgRadius = (balls[i].radius + balls[j].radius) * 0.5f;
        unsigned int freq = map((long)avgRadius, 6, 17, 4800, 1200);
          playSound(freq, strength);
      }
    }
  }
}

long signed int lastMillis;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  Wire.begin(4, 5); // SDA = GPIO4 (D2), SCL = GPIO5 (D1)
  
  // Safely check if MPU6050 is connected
  if (mpu.begin()) {
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpuPresent = true;
  } else {
    mpuPresent = false; // Fallback to touch-only simulation
  }

  randomSeed(analogRead(A0));
  initBalls();

  lastMillis = millis();
}

void loop() {
  static uint16_t prevTouchX = 0, prevTouchY = 0;
  static bool wasTouched = false;
  
  // Track previous frame acceleration for inertia force transfer
  static float prevAccX = 0.0f;
  static float prevAccY = 0.0f;

  updateAudio(); 

  float gravX = 0.0f;
  float gravY = 0.0f;

// 1. Read MPU6050 only if sensor was detected in setup()
  if (mpuPresent) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float currentAccX =  a.acceleration.x;
    float currentAccY = -a.acceleration.y;

    // 2. Calculate Instantaneous Inertial Force (Jerk / Acceleration Delta)
    // When board moves fast, balls experience an opposite reactive force!
    float forceX = (currentAccX - prevAccX) * 0.8f; // Scale factor for shake impulse
    float forceY = (currentAccY - prevAccY) * 0.8f;

    // Cache current readings for next frame
    prevAccX = currentAccX;
    prevAccY = currentAccY;

    // Apply inertial impulse directly to ball velocities
    for (int i = 0; i < NUM_BALLS; i++) {
      balls[i].vx += forceX;
      balls[i].vy += forceY;
    }

    // Tilt baseline (Static gravity vector)
    gravX = currentAccX;
    gravY = currentAccY;
  }

  // 3. Read Touch
  uint16_t touchX = 0, touchY = 0;
  bool touched = tft.getTouch(&touchX, &touchY);

  // 4. Physics Step
  updatePhysics(touchX, touchY, touched, gravX, gravY);
  updatePhysics(touchX, touchY, touched, gravX, gravY);

  if (wasTouched && (!touched || prevTouchX != touchX || prevTouchY != touchY)) {
    tft.fillCircle(prevTouchX, prevTouchY, 36, TFT_BLACK);
  }

  if (touched) {
    tft.fillCircle(touchX, touchY, 36, 0b0010100101100101);
    tft.fillCircle(touchX, touchY, 24, 0b0101001010001010);
    tft.fillCircle(touchX, touchY, 16, 0b1011110111110111);
    tft.fillCircle(touchX, touchY, 10, 0b1111111111111111);
    prevTouchX = touchX;
    prevTouchY = touchY;
  }
  wasTouched = touched;

  // 5. Interleaved Draw Pass (Minimal Flicker)
  for (int i = 0; i < NUM_BALLS; i++) {
    int ballpx = (int)prevBalls[i].x;
    int ballpy = (int)prevBalls[i].y;
    int ballpr = (int)prevBalls[i].radius;
    int ball_x = (int)balls[i].x;
    int ball_y = (int)balls[i].y;
    int ball_r = (int)balls[i].radius;

    if (ballpx != ball_x || ballpy != ball_y) {
      int ball_s = ball_r >> 2;
      // Default highlight offset if MPU is missing (upper-left light source)
      int ball_ox = mpuPresent ? (int)(-gravX * ball_r) >> 4 : (ball_r >> 1);
      int ball_oy = mpuPresent ? (int)( gravY * ball_r) >> 4 : (ball_r >> 1);

      // 1. Clear old ball position immediately before drawing new one
      tft.fillCircle(ballpx, ballpy, ballpr, TFT_BLACK);

      // 2. Draw new ball body + shiny highlight
      tft.fillCircle(ball_x, ball_y, ball_r, balls[i].color);
      if ( ball_ox * ball_ox + ball_oy * ball_oy < ( ball_r - ball_s ) * ( ball_r - ball_s ))
        tft.fillCircle(ball_x + ball_ox, ball_y - ball_oy, ball_s, 0xFFFF);

      // 3. Fix Overlap Clipping: If this erase pass accidentally chopped 
      // an already-drawn ball (j < i), re-draw that neighbor quickly!
      for (int j = 0; j < i; j++) {
        int dx = (int)balls[j].x - ballpx;
        int dy = (int)balls[j].y - ballpy;
        int distSq = dx * dx + dy * dy;
        int minDist = (int)balls[j].radius + ballpr;

        if (distSq <= minDist * minDist) {
          int rd = (int)balls[j].radius;
          int ox = mpuPresent ? (int)(-gravX * rd) >> 4 : (rd >> 1);
          int oy = mpuPresent ? (int)( gravY * rd) >> 4 : (rd >> 1);
          int sj = rd >> 2 ;
          tft.fillCircle((int)balls[j].x, (int)balls[j].y, rd, balls[j].color);
          if ( ( ox * ox ) + ( oy * oy )  < ( rd - sj ) * ( rd - sj ) )
            tft.fillCircle((int)balls[j].x + ox, (int)balls[j].y - oy, sj, 0xFFFF);
        }
      }
    }
    prevBalls[i] = balls[i];
  }

  while ( (int)lastMillis-(int)millis() >= 0 ) delay(1);
  while ( (int)lastMillis-(int)millis() <  0 ) lastMillis += 20;
  
}
