/*
 * Based on LazerTagHost by Alex Faucher
 * https://github.com/afaucher/LazerTagHost
 * Based on IRremote by Ken Shirriff
 * http://arcfn.com
 */

#include <IRremote.h>
#include <FastLED.h>

// ==============
// LED Indicators
// ==============

#define NUM_LEDS 8
#define LED_PIN 6
CRGB leds[NUM_LEDS];

// ==============
// IR State
// ==============

#define RECV_PIN 4
//int RELAY_PIN = 4;
//Note: This pin is hardcoded into IRRemote
#define IR_TX_PIN 3

IRrecv irrecv(RECV_PIN);
IRsend irsend;

decode_results results;

// ==============
// PWM Receiver State
// ==============

//https://create.arduino.cc/projecthub/kelvineyeone/read-pwm-decode-rc-receiver-input-and-apply-fail-safe-6b90eb
#define NUMBER_RC_CHANNELS 1
#define TRIGGER_CHANNEL 0
//I picked this without testing.  This largely depends on receiver
//We will target the 'middle', 0% assuming a three position switch
#define RECEIVER_TRIGGER_TOLERANCE_PERCENT 0.2f
#define TRIGGER_RANGE_VALUE 0.5f
#define RECEIVER_UPDATE_PERIOD_MILLIS 100
//Note: Pin 5 is hardcoded into pwmread_rcfailsafe

struct RcReceiver {
  uint32_t rc_update;
  float RC_in[NUMBER_RC_CHANNELS];                    // an array to store the calibrated input from receiver 
};

RcReceiver rcReceiver = {};

// ==============
// Game State
// ==============

#define MAX_LIFE 16
//TODO - Count need to be switched to millis
#define HIT_COUNTDOWN_COUNT 10000
#define MAX_AMMO 50
#define SHOT_COOLDOWN_MILLIS 1000

struct GameState {
  uint8_t life; //[0-MAX_LIFE] where 0 is dead
  //These are limited to ranges for LTTO/LTX
  uint8_t teamId; //Solo: [0] Team: [1-3]
  uint8_t playerId; //[0-7]
  uint16_t hitCountdown; //If > 0, we have been hit
  uint8_t ammo;
  uint32_t lastShotFiredTimeMillis;
};

GameState gameState = { MAX_LIFE, 0, 0, 0, MAX_AMMO };

// ==============
// Input State
// ==============

struct InputState {
  boolean triggerPulled;
  boolean resetGame;
  boolean hit;
  uint8_t hitDamage;
};

InputState inputState = { false, false, false };

// ==============
// Stats
// ==============

struct Stats {
  uint16_t hits;
  uint16_t hitsIgnoredDuringCountdown;
  uint16_t unknownIrPacket;
  uint16_t ledUpdates;
  uint16_t shotsFired;
};

Stats stats;

// ==============
// Buttons
// ==============

#define HIT_BUTTON_PIN A0
#define TRIGGER_BUTTON_PIN A1

// Dumps out the decode_results structure.
// Call this after IRrecv::decode()
// void * to work around compiler issue
//void dump(void *v) {
//  decode_results *results = (decode_results *)v
void dump(decode_results *results) {
  uint8_t count = results->rawlen;
  if (results->decode_type == UNKNOWN) {
    Serial.println(F("Could not decode message"));
  } 
  else {
    if (results->decode_type == PHOENIX_LTX) {
      Serial.print(F("LTX: "));
    }
    else if (results->decode_type == LAZER_TAG_TEAM_OPS) {
      Serial.print(F("LTTO: "));
    }
    Serial.print(results->value, HEX);
    Serial.print(F(", "));
    Serial.println(results->bits, DEC);
  }
  Serial.print(F("Raw ("));
  Serial.print(count, DEC);
  Serial.print(F("): "));

  for (uint8_t i = 0; i < count; i++) {
    if ((i % 2) == 1) {
      Serial.print(results->rawbuf[i]*USECPERTICK, DEC);
    } 
    else {
      Serial.print(-(int)results->rawbuf[i]*USECPERTICK, DEC);
    }
    Serial.print(F(" "));
  }
  Serial.println();
}

void setup()
{
  //TODO: Cruft from relay - determine if needed
  //pinMode(RELAY_PIN, OUTPUT);
  //pinMode(13, OUTPUT);
  
  //Start IR TX
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);
  
  // Setup Button Input
  pinMode(HIT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIGGER_BUTTON_PIN, INPUT_PULLUP);
  // Start the USB Serial
  Serial.begin(115200);
  Serial.println(F("Start"));
  // Start the IR receiver
  irrecv.enableIRIn();
  // Start the PWM RC receiver
  setup_pwmRead();
  // Setup LEDs
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  setupLedValuesNow();
  //Duemilanove 5v regulator is only rated to 500mA on USB, which also powers the board.
  //Keep a conservative limit here.
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 100); 
  updateLedValuesNow();
}

void shoot(uint16_t team, uint16_t player, uint16_t damage) {
  //Damage is encoded 0-3 for damage range [1-4]
  damage--;
  uint16_t data = ((team & 0x03) << 5)
      | ((player & 0x07) << 2)
      | (damage & 0x02);
  irsend.sendPHOENIX_LTX(data, 7);
  irrecv.enableIRIn();
  irrecv.resume();
//  Serial.println(F("Fire"));
}

//Manages led state
void updateLedValues() {
  uint8_t healthPerLed = MAX_LIFE / NUM_LEDS;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    //The order here defines precidence for which rule controls the output
    if (gameState.life == 0) { //Player Death
      leds[i] = CRGB::White;
    } else if (gameState.hitCountdown > 0) { //Hit Indicator
      leds[i] = CRGB::Red;
    } else if (gameState.lastShotFiredTimeMillis > 0) { //Shot Indicator
        //Give an indication while firing
        //TODO - This should move over to the 'ammo' LED once we define that
        leds[i] = CRGB(0,0,100);
    } else { //Show Health
      if (i * healthPerLed < gameState.life) {
        //Note: Partial health can be indicated by blinking the last element
        leds[i] = CRGB::Green;
      } else {
        leds[i] = CRGB::Black;
      }
    }
  }
}

void setupLedValuesNow() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Grey;
  }
  FastLED.show();
  stats.ledUpdates++;
}

void updateLedValuesNow() {
  updateLedValues();
  FastLED.show();
  stats.ledUpdates++;
}

void readButtonInput() {
  //For now, a button simulates getting hit
  //TODO - Pull in a debounce library for cleaning up this input
  //for now the hit countdown hides it if it bounces
  if (!inputState.hit) {
    int buttonValue = digitalRead(HIT_BUTTON_PIN);
    inputState.hit = (buttonValue == 0);
    inputState.hitDamage = 1;
  }
  if (!inputState.triggerPulled) {
    int buttonValue = digitalRead(TRIGGER_BUTTON_PIN);
    inputState.triggerPulled = (buttonValue == 0);
  }
}

void updateGame(uint32_t now) {
  boolean requiresUpdate = false;
  if (inputState.hit) {
    if (gameState.life > 0 && gameState.hitCountdown == 0) {
      //We must be alive to be hit
      //Only if we have recovered from the countdown can we be hit again
      gameState.life--;
      gameState.hitCountdown = HIT_COUNTDOWN_COUNT;
      stats.hits++;
      //Serial.print(F("Hit, remaining life: "));
      //Serial.println(gameState.life, DEC);
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
      requiresUpdate = true;
    }
  }
  if (inputState.triggerPulled) {
    //While the trigger is pulled, we will not fire faster then SHOT_COOLDOWN_MILLIS
    if (gameState.lastShotFiredTimeMillis < now - SHOT_COOLDOWN_MILLIS
        && gameState.ammo > 0) {
      shoot(gameState.playerId, gameState.teamId, 1);
      gameState.lastShotFiredTimeMillis = now;
      gameState.ammo--;
      stats.shotsFired++;
      requiresUpdate = true;
      inputState.triggerPulled = false;
    }
  } else if (gameState.lastShotFiredTimeMillis > 0) {
    //Trigger has been released.  If the trigger is pulled again you can fire again.
    //The main reason to do this is to detect when firing stops so the LED indication can be updated.
    gameState.lastShotFiredTimeMillis = 0;
    requiresUpdate = true;
  }
  if (requiresUpdate) {
    updateLedValuesNow();
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
       uint8_t team = (results.value >> 2) & 0x3;
       uint8_t damage = (results.value >> 0) & 0x3;
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
    //Serial.print(F("Value: "));
    //Serial.println(results.value, BIN);
    
    irrecv.resume(); // Receive the next value
    
     
  }
}

void readRcInput(uint32_t now) {
  if(RC_avail() || now - rcReceiver.rc_update > RECEIVER_UPDATE_PERIOD_MILLIS) {   // if RC data is available or 25ms has passed since last update (adjust to be equal or greater than the frame rate of receiver)
      
      rcReceiver.rc_update = now;                           
      
      //Note: This printing can break uploading somehow?  Uncommenting it causes upload to fail.
      //print_RCpwm();                        // uncommment to print raw data from receiver to serial
      
      for (uint8_t i = 0; i < NUMBER_RC_CHANNELS; i++){       // run through each RC channel
        uint8_t ch = i + 1;
        
        rcReceiver.RC_in[i] = RC_decode(ch);             // decode receiver channel and apply failsafe
        
        //Serial.println(rcReceiver.RC_in[i]);   // uncomment to print calibrated receiver input (+-100%) to serial       
      }
      //Serial.println();                       // uncomment when printing calibrated receiver input to serial.
    }
    inputState.triggerPulled = TRIGGER_RANGE_VALUE - RECEIVER_TRIGGER_TOLERANCE_PERCENT < rcReceiver.RC_in[TRIGGER_CHANNEL]
        && rcReceiver.RC_in[TRIGGER_CHANNEL] < TRIGGER_RANGE_VALUE + RECEIVER_TRIGGER_TOLERANCE_PERCENT;
}

void loop() {
  uint32_t now = millis();
  readIrInput();
  //readButtonInput();
  readRcInput(now);
  updateGame(now);
}
