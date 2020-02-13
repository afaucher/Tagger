/*
 * Based on IRremote by Ken Shirriff
 * http://arcfn.com
 */

#include <IRremote.h>
#include <FastLED.h>

// ==============
// LED Indicators
// ==============

#define NUM_LEDS 8
CRGB leds[NUM_LEDS];
#define LED_PIN 6
int ledCounter = 0;
int dutyCycleCounter = 0;
#define DUTY_CYCLE_COUNT 4
#define DUTY_CYCLE_THRESHOLD 1
#define LED_UPDATE_COUNT_PERIOD 50

// ==============
// IR State
// ==============

int RECV_PIN = 11;
int RELAY_PIN = 4;

IRrecv irrecv(RECV_PIN);
IRsend irsend;

decode_results results;

// ==============
// PWM Receiver State
// ==============

//https://create.arduino.cc/projecthub/kelvineyeone/read-pwm-decode-rc-receiver-input-and-apply-fail-safe-6b90eb
//unsigned long now;                        // timing variables to update data at a regular interval                  
//unsigned long rc_update;
//const int channels = 1;                   // specify the number of receiver channels
//float RC_in[channels];                    // an array to store the calibrated input from receiver 


// ==============
// Game State
// ==============

#define MAX_LIFE 16
#define HIT_COUNTDOWN_COUNT 10000

struct GameState {
  unsigned int life; //[0-MAX_LIFE]
  //These are limited to ranges for LTTO/LTX
  byte teamId; //Solo: [0] Team: [1-3]
  byte playerId; //[0-7]
  unsigned int hitCountdown; //If > 0, we have been hit
};

GameState gameState = { MAX_LIFE, 0, 0, 0 };

// ==============
// Input State
// ==============

struct InputState {
  boolean triggerPulled;
  boolean resetGame;
  boolean hit;
};

InputState inputState = { false, false, false };

// ==============
// Stats
// ==============

// ==============
// Buttons
// ==============

#define BUTTON_PIN 7

// Dumps out the decode_results structure.
// Call this after IRrecv::decode()
// void * to work around compiler issue
//void dump(void *v) {
//  decode_results *results = (decode_results *)v
void dump(decode_results *results) {
  int count = results->rawlen;
  if (results->decode_type == UNKNOWN) {
    Serial.println("Could not decode message");
  } 
  else {
    if (results->decode_type == PHOENIX_LTX) {
      Serial.print("LTX: ");
    }
    else if (results->decode_type == LAZER_TAG_TEAM_OPS) {
      Serial.print("LTTO: ");
    }
    Serial.print(results->value, HEX);
    Serial.print(", ");
    Serial.println(results->bits, DEC);
  }
  /*Serial.print("Raw (");
  Serial.print(count, DEC);
  Serial.print("): ");

  for (int i = 0; i < count; i++) {
    if ((i % 2) == 1) {
      Serial.print(results->rawbuf[i]*USECPERTICK, DEC);
    } 
    else {
      Serial.print(-(int)results->rawbuf[i]*USECPERTICK, DEC);
    }
    Serial.print(" ");
  }
  Serial.println("");*/
}

void setup()
{
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); //Button
  Serial.begin(115200);
  Serial.println("Start");
  // Start the IR receiver
  irrecv.enableIRIn();
  // Start the PWM RC receiver
  setup_pwmRead();  
  
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
}

void shoot(short team, short player, short damage) {
  short data = ((team & 0x03) << 5)
      | ((player & 0x07) << 2)
      | (damage & 0x02);
  irsend.sendPHOENIX_LTX(data, 7);
}



void updateLeds() {
  ledCounter += 1;
  if (ledCounter > LED_UPDATE_COUNT_PERIOD) {
    ledCounter = 0;
    
    dutyCycleCounter++;
    if (dutyCycleCounter > DUTY_CYCLE_COUNT) {
      dutyCycleCounter = 0;
    }
    boolean dutyCycle = dutyCycleCounter <= DUTY_CYCLE_THRESHOLD;
    int healthPerLed = MAX_LIFE / NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++) {
      if (!dutyCycle) {
        leds[i] = CRGB::Black;
        continue;
      }
      if (gameState.hitCountdown > 0) {
        leds[i] = CRGB::Red;
      } else if (i * healthPerLed < gameState.life) {
        leds[i] = CRGB::Green;
      } else {
        leds[i] = CRGB::Black;
      }
    }
    FastLED.show();
  }
}

void readInput() {
  //For now, a button simulates getting hit
  int buttonValue = digitalRead(BUTTON_PIN);
  inputState.hit = (buttonValue == 0);
}

void updateGame() {
  if (inputState.hit) {
    if (gameState.life > 0 && gameState.hitCountdown == 0) {
      //We must be alive to be hit
      //Only if we have recovered from the countdown can we be hit again
      gameState.life--;
      gameState.hitCountdown = HIT_COUNTDOWN_COUNT;
      Serial.print("Hit, life: ");
      Serial.println(gameState.life, DEC);
    }
  }
  if (gameState.hitCountdown > 0) {
    gameState.hitCountdown--;
  }
}

void loop() {
  if (irrecv.decode(&results)) {
    //dump(&results);
    
    
    irrecv.enableIRIn();
    //Serial.print("Value: ");
    //Serial.println(results.value, BIN);
    
    irrecv.resume(); // Receive the next value
  } else if (Serial.available() >= 2) {
    byte high = Serial.read();
    byte low = Serial.read();
    
    byte count = (high >> 1) & 0xf;
    byte type = (high >> 5) & 0x1;
    short value = (short)low | (((short)high & 0x1) << 8);
    
    /*Serial.print("Recv: ");
    Serial.print(value, HEX);
    Serial.print(", ");
    Serial.println(count, DEC);*/
    if (type == 0x00) {
      irsend.sendPHOENIX_LTX(value, count);
    } else if (type == 0x01) {
      irsend.sendLTTO(value, count);
    }
    
    irrecv.enableIRIn();
    irrecv.resume();
  }
  
  readInput();
  updateGame();
  
  updateLeds();
}
