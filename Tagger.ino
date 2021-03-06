/*
   Based on LazerTagHost by Alex Faucher
   https://github.com/afaucher/LazerTagHost
   Based on IRremote by Ken Shirriff
   http://arcfn.com
*/

#include <IRremote.h>
#include <FastLED.h>
// https://github.com/thomasfredericks/Bounce2
#include <Bounce2.h>


// ==============
// LED Indicators
// ==============

#define NUM_LEDS_PER_STRIP 8
#define HEALTH_LED_PIN 6
#define AMMO_LED_PIN 7
CRGB leds[NUM_LEDS_PER_STRIP * 2];

// ==============
// IR State
// ==============

#define RECV_PIN 4
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
//Setup: You must adjust the range/tolerance value for the channel you choose.
#define RECEIVER_TRIGGER_TOLERANCE_PERCENT 0.2f
#define TRIGGER_RANGE_VALUE 0.5f
#define RECEIVER_UPDATE_PERIOD_MILLIS 100
//Note: Pin 5 is hardcoded in pwmread_rcfailsafe

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
  uint32_t lastShotFiredTimeMillis; //Zero if not in cooldown
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
#define DEBOUNCE_INTERVAL_MS 100

struct Buttons {
  Bounce hitButton;
  Bounce triggerButton;
};

Buttons buttons;

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
  //Start IR TX
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);

  // Setup Button Input
  buttons.hitButton.attach(HIT_BUTTON_PIN, INPUT_PULLUP);
  buttons.hitButton.interval(DEBOUNCE_INTERVAL_MS);
  buttons.triggerButton.attach(TRIGGER_BUTTON_PIN, INPUT_PULLUP);
  buttons.triggerButton.interval(DEBOUNCE_INTERVAL_MS);

  // Start the USB Serial
  Serial.begin(115200);
  Serial.println(F("Start"));
  // Start the IR receiver
  irrecv.enableIRIn();
  // Start the PWM RC receiver
  setup_pwmRead();
  // Setup LEDs
  // Note: The decision to seperate these was based on the wiring being easier.  This can be
  // refactored to support one long continous strip if needed.
  FastLED.addLeds<NEOPIXEL, HEALTH_LED_PIN>(leds, 0, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<NEOPIXEL, AMMO_LED_PIN>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP);
  setupLedValuesNow();
  // Duemilanove 5v regulator is only rated to 500mA on USB, which also powers the board.
  // Keep a conservative limit here.  Adjust if you know your board can drive higher current.
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
}

//Update the LED state from the current game state
//Does not refresh the LEDs
void updateLedValues() {
  //HEALTH STRIP
  uint8_t healthPerLed = MAX_LIFE / NUM_LEDS_PER_STRIP;
  for (uint8_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
    //The order here defines precidence for which rule controls the output
    if (gameState.life == 0) { //Player Death
      leds[i] = CRGB::White;
    } else if (gameState.hitCountdown > 0) { //Hit Indicator
      leds[i] = CRGB::Red;
    } else { //Show Health
      if (i * healthPerLed < gameState.life) {
        //Note: Partial health can be indicated by blinking the last element
        leds[i] = CRGB::Green;
      } else {
        leds[i] = CRGB::Black;
      }
    }
  }
  //AMMO STRIP
  uint8_t ammoPerLed = MAX_AMMO / NUM_LEDS_PER_STRIP;
  for (uint8_t i = NUM_LEDS_PER_STRIP; i < NUM_LEDS_PER_STRIP * 2; i++) {
    //The order here defines precidence for which rule controls the output
    if (gameState.life == 0) { //Player Death
      leds[i] = CRGB::White;
    } else if (gameState.hitCountdown > 0) { //Hit Indicator
      leds[i] = CRGB::Red;
    } else { //Show Ammo & Shot indicator
      if ((i - NUM_LEDS_PER_STRIP) * ammoPerLed < gameState.ammo) {
        if (gameState.lastShotFiredTimeMillis > 0 && gameState.ammo > 0) { //Shot Indicator
          //Give an indication while firing by making ammo orange
          leds[i] = CRGB::Orange;
        } else { //Ammo indicator
          leds[i] = CRGB::Blue;
        }
      } else {
        leds[i] = CRGB::Black;
      }
    }
  }
}

//Initialize LEDs to a neutral value to diagnose any initialization issues
void setupLedValuesNow() {
  for (uint8_t i = 0; i < NUM_LEDS_PER_STRIP * 2; i++) {
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
  //A button can simulate getting hit for debugging
  if (!inputState.hit) {
    buttons.hitButton.update();
    inputState.hit |= !buttons.hitButton.read();
    inputState.hitDamage = 1;
  }
  if (!inputState.triggerPulled) {
    buttons.triggerButton.update();
    inputState.triggerPulled |= !buttons.triggerButton.read();
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
      requiresUpdate = true;
    } else {
      stats.hitsIgnoredDuringCountdown++;
    }
    //We reset current input as we expect it to retrigger on the next hit
    inputState.hit = false;
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
        inputState.hit |= true;
        inputState.hitDamage = damage + 1;
      } else if (team != 0 && gameState.teamId != 0 && team != gameState.teamId) {
        //We are not playing solo and not on the same team
        inputState.hit |= true;
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
  if (RC_avail() || now - rcReceiver.rc_update > RECEIVER_UPDATE_PERIOD_MILLIS) {  // if RC data is available or 25ms has passed since last update (adjust to be equal or greater than the frame rate of receiver)

    rcReceiver.rc_update = now;

    //print_RCpwm();                        // uncommment to print raw data from receiver to serial

    for (uint8_t i = 0; i < NUMBER_RC_CHANNELS; i++) {      // run through each RC channel
      uint8_t ch = i + 1;

      rcReceiver.RC_in[i] = RC_decode(ch);             // decode receiver channel and apply failsafe

      //Serial.println(rcReceiver.RC_in[i]);   // uncomment to print calibrated receiver input (+-100%) to serial
    }
    //Serial.println();                       // uncomment when printing calibrated receiver input to serial.
  }
  inputState.triggerPulled |= TRIGGER_RANGE_VALUE - RECEIVER_TRIGGER_TOLERANCE_PERCENT < rcReceiver.RC_in[TRIGGER_CHANNEL]
                              && rcReceiver.RC_in[TRIGGER_CHANNEL] < TRIGGER_RANGE_VALUE + RECEIVER_TRIGGER_TOLERANCE_PERCENT;
}

void loop() {
  uint32_t now = millis();
  readIrInput();
  readButtonInput();
  readRcInput(now);
  updateGame(now);
}
