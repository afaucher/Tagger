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

// ==============
// IR State
// ==============

int RECV_PIN = 11;
//int RELAY_PIN = 4;

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
  byte hitDamage;
};

InputState inputState = { false, false, false };

// ==============
// Stats
// ==============

struct Stats {
  unsigned int hits;
  unsigned int hitsIgnoredDuringCountdown;
  unsigned int unknownIrPacket;
};

Stats stats;

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
  Serial.print("Raw (");
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
  Serial.println("");
}

void setup()
{
  //Cruft from relay - determine if needed
  //pinMode(RELAY_PIN, OUTPUT);
  //pinMode(13, OUTPUT);
  
  
  // Setup Button Input
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // Start the USB Serial
  Serial.begin(115200);
  Serial.println("Start");
  // Start the IR receiver
  irrecv.enableIRIn();
  // Start the PWM RC receiver
  setup_pwmRead();  
  
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  //Duemilanove 5v regulator is only rated to 190mA
  FastLED.setMaxPowerInVoltsAndMilliamps(5,100); 
  
  updateLedValuesNow();
}

void shoot(short team, short player, short damage) {
  short data = ((team & 0x03) << 5)
      | ((player & 0x07) << 2)
      | (damage & 0x02);
  irsend.sendPHOENIX_LTX(data, 7);
}

//Manages led state
void updateLedValues() {
  int healthPerLed = MAX_LIFE / NUM_LEDS;
  for (int i = 0; i < NUM_LEDS; i++) {
    //The order here defines precidence for which rule controls the output
    if (gameState.life == 0) { //Player Death
      leds[i] = CRGB::White;
    } else if (gameState.hitCountdown > 0) { //Hit Indicator
      leds[i] = CRGB::Red;
    } else { //Show Health
      if (i * healthPerLed < gameState.life) {
        //TODO - Partial health can be indicated by blinking the last element
        leds[i] = CRGB::Green;
      } else {
        leds[i] = CRGB::Black;
      }
    }
  }
}

void updateLedValuesNow() {
  updateLedValues();
  FastLED.show();
}

void readInput() {
  //For now, a button simulates getting hit
  //TODO - Pull in a debounce library for cleaning up this input
  //for now the hit countdown hides it if it bounces
  if (!inputState.hit) {
    int buttonValue = digitalRead(BUTTON_PIN);
    inputState.hit = (buttonValue == 0);
    inputState.hitDamage = 1;
  }
}

void updateGame() {
  if (inputState.hit) {
    if (gameState.life > 0 && gameState.hitCountdown == 0) {
      //We must be alive to be hit
      //Only if we have recovered from the countdown can we be hit again
      gameState.life--;
      gameState.hitCountdown = HIT_COUNTDOWN_COUNT;
      stats.hits++;
      Serial.print("Hit, remaining life: ");
      Serial.println(gameState.life, DEC);
      updateLedValuesNow();
    } else {
      stats.hitsIgnoredDuringCountdown++;
    }
    //We reset current input as we expect it to retrigger on the next hit
    inputState.hit = 0;
  }
  if (gameState.hitCountdown > 0) {
    gameState.hitCountdown--;
    if (gameState.hitCountdown == 0) {
      updateLedValuesNow();
    }
  }
}

void readIrInput() {
  if (irrecv.decode(&results)) {
    //dump(&results);
    
    //We only understand shots for now and we can just assume everything is all right
     if (results.decode_type == PHOENIX_LTX && results.bits == 7) {
       //https://executedata.blogspot.com/2010/09/lazertag-team-ops-packet-part-1.html
       //[ 3 bits - zero ] [ 2 bits - team ] [ 2 bits - damage ]
       // Team:
       // 0x00 = Solo
       // 0x01-0x03 = Team 1-3
       // Damage:
       // 0x00-0x03 = Damage 1-4 (3-4 are only usable by LTTO as Mega-Tag)
       byte team = (results.value >> 2) & 0x3;
       byte damage = (results.value >> 0) & 0x3;
       //TODO - Clean up team detection
       if (team == 0 && gameState.teamId == 0) {
         //Solo
         inputState.hit = 1;
         inputState.hitDamage = damage + 1;
       } else if (team != 0 && gameState.teamId != 0 && team != gameState.teamId) {
         //We are not playing solo and not on the same team
         inputState.hit = 1;
         inputState.hitDamage = damage + 1;
       }
     } else {
       stats.unknownIrPacket++;
     }
    
    irrecv.enableIRIn();
    //Serial.print("Value: ");
    //Serial.println(results.value, BIN);
    
    irrecv.resume(); // Receive the next value
    
     
  }
}

void loop() {
//  if (Serial.available() >= 2) {
//    byte high = Serial.read();
//    byte low = Serial.read();
//    
//    byte count = (high >> 1) & 0xf;
//    byte type = (high >> 5) & 0x1;
//    short value = (short)low | (((short)high & 0x1) << 8);
//    
//    /*Serial.print("Recv: ");
//    Serial.print(value, HEX);
//    Serial.print(", ");
//    Serial.println(count, DEC);*/
//    if (type == 0x00) {
//      irsend.sendPHOENIX_LTX(value, count);
//    } else if (type == 0x01) {
//      irsend.sendLTTO(value, count);
//    }
//    
//    irrecv.enableIRIn();
//    irrecv.resume();
//  }
  
  readIrInput();
  //readInput();
  updateGame();
  
  //updateLeds();
}
