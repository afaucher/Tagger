/*
 * IRremote
 * Version 0.11 August, 2009
 * Copyright 2009 Ken Shirriff
 * For details, see http://arcfn.com/2009/08/multi-protocol-infrared-remote-library.html
 *
 * Interrupt code based on NECIRrcv by Joe Knapp
 * http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1210243556
 * Also influenced by http://zovirl.com/2008/11/12/building-a-universal-remote-with-an-arduino/
 */

#include "IRremote.h"
#include "IRremoteInt.h"

// Provides ISR
#include <avr/interrupt.h>

volatile irparams_t irparams;

// These versions of MATCH, MATCH_MARK, and MATCH_SPACE are only for debugging.
// To use them, set DEBUG in IRremoteInt.h
// Normally macros are used for efficiency
#ifdef DEBUG
int MATCH(int measured, int desired) {
  Serial.print("Testing: ");
  Serial.print(TICKS_LOW(desired), DEC);
  Serial.print(" <= ");
  Serial.print(measured, DEC);
  Serial.print(" <= ");
  Serial.println(TICKS_HIGH(desired), DEC);
  return measured >= TICKS_LOW(desired) && measured <= TICKS_HIGH(desired);
}

int MATCH_MARK(int measured_ticks, int desired_us) {
  Serial.print("Testing mark ");
  Serial.print(USECPERTICK);
  Serial.print(" USECPERTICK ");
  Serial.print(measured_ticks, DEC);
  Serial.print(" ticks vs ");
  Serial.print(desired_us, DEC);
  Serial.print(" us: ");
  Serial.print(TICKS_LOW(desired_us + MARK_EXCESS), DEC);
  Serial.print(" <= ");
  Serial.print(measured_ticks, DEC);
  Serial.print(" <= ");
  Serial.println(TICKS_HIGH(desired_us + MARK_EXCESS), DEC);
  return measured_ticks >= TICKS_LOW(desired_us + MARK_EXCESS) && measured_ticks <= TICKS_HIGH(desired_us + MARK_EXCESS);
}

int MATCH_SPACE(int measured_ticks, int desired_us) {
  Serial.print("Testing space ");
  Serial.print(measured_ticks * USECPERTICK, DEC);
  Serial.print(" vs ");
  Serial.print(desired_us, DEC);
  Serial.print(": ");
  Serial.print(TICKS_LOW(desired_us - MARK_EXCESS), DEC);
  Serial.print(" <= ");
  Serial.print(measured_ticks, DEC);
  Serial.print(" <= ");
  Serial.println(TICKS_HIGH(desired_us - MARK_EXCESS), DEC);
  return measured_ticks >= TICKS_LOW(desired_us - MARK_EXCESS) && measured_ticks <= TICKS_HIGH(desired_us - MARK_EXCESS);
}
#endif

void IRsend::sendRaw(unsigned int buf[], int len, int hz)
{
  enableIROut(hz);
  for (int i = 0; i < len; i++) {
    if (i & 1) {
      space(buf[i]);
    } 
    else {
      mark(buf[i]);
    }
  }
  space(0); // Just to be sure
}

void IRsend::sendPHOENIX_LTX(unsigned long data, int nbits) {
  enableIROut(36);
  //HEAD
  mark(PHOENIX_HDR_MARK);
  space(PHOENIX_HDR_SPACE);
  mark(PHOENIX_HDR_MARK);
  
  int i = 0;
  for (i = 0; i < nbits; i++) {
    space(PHOENIX_LTX_SPACE);
    byte current_bit = (data >> (nbits - i - 1)) & 0x1;
    if (current_bit) {
      mark(PHOENIX_LTX_ONE_MARK);
    } else {
      mark(PHOENIX_LTX_ZERO_MARK);    
    }
  }
}

void IRsend::sendLTTO(unsigned long data, int nbits) {
  enableIROut(36);
  //HEAD
  mark(LTTO_HDR_MARK_ONE);
  space(LTTO_HDR_SPACE);
  mark(LTTO_HDR_MARK_TWO);
  
  int i = 0;
  for (i = 0; i < nbits; i++) {
    space(LTTO_SPACE);
    byte current_bit = (data >> (nbits - i - 1)) & 0x1;
    if (current_bit) {
      mark(LTTO_ONE_MARK);
    } else {
      mark(LTTO_ZERO_MARK);    
    }
  }
}

void IRsend::mark(int time) {
  // Sends an IR mark for the specified number of microseconds.
  // The mark output is modulated at the PWM frequency.
  TCCR2A |= _BV(COM2B1); // Enable pin 3 PWM output
  delayMicroseconds(time);
}

/* Leave pin off for time (given in microseconds) */
void IRsend::space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
  TCCR2A &= ~(_BV(COM2B1)); // Disable pin 3 PWM output
  delayMicroseconds(time);
}

void IRsend::enableIROut(int khz) {
  // Enables IR output.  The khz value controls the modulation frequency in kilohertz.
  // The IR output will be on pin 3 (OC2B).
  // This routine is designed for 36-40KHz; if you use it for other values, it's up to you
  // to make sure it gives reasonable results.  (Watch out for overflow / underflow / rounding.)
  // TIMER2 is used in phase-correct PWM mode, with OCR2A controlling the frequency and OCR2B
  // controlling the duty cycle.
  // There is no prescaling, so the output frequency is 16MHz / (2 * OCR2A)
  // To turn the output on and off, we leave the PWM running, but connect and disconnect the output pin.
  // A few hours staring at the ATmega documentation and this will all make sense.
  // See my Secrets of Arduino PWM at http://arcfn.com/2009/07/secrets-of-arduino-pwm.html for details.

  
  // Disable the Timer2 Interrupt (which is used for receiving IR)
  TIMSK2 &= ~_BV(TOIE2); //Timer2 Overflow Interrupt
  
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW); // When not sending PWM, we want it low
  
  // COM2A = 00: disconnect OC2A
  // COM2B = 00: disconnect OC2B; to send signal set to 10: OC2B non-inverted
  // WGM2 = 101: phase-correct PWM with OCRA as top
  // CS2 = 000: no prescaling
  TCCR2A = _BV(WGM20);
  TCCR2B = _BV(WGM22) | _BV(CS20);

  // The top value for the timer.  The modulation frequency will be SYSCLOCK / 2 / OCR2A.
  OCR2A = SYSCLOCK / 2 / khz / 1000;
  OCR2B = OCR2A / 3; // 33% duty cycle
}

IRrecv::IRrecv(int recvpin)
{
  irparams.recvpin = recvpin;
  irparams.blinkflag = 0;
}

// initialization
void IRrecv::enableIRIn() {
  // setup pulse clock timer interrupt
  TCCR2A = 0;  // normal mode

  //Prescale /8 (16M/8 = 0.5 microseconds per tick)
  // Therefore, the timer interval can range from 0.5 to 128 microseconds
  // depending on the reset value (255 to 0)
  cbi(TCCR2B,CS22);
  sbi(TCCR2B,CS21);
  cbi(TCCR2B,CS20);

  //Timer2 Overflow Interrupt Enable
  sbi(TIMSK2,TOIE2);

  RESET_TIMER2;

  sei();  // enable interrupts

  // initialize state machine variables
  irparams.rcvstate = STATE_IDLE;
  irparams.rawlen = 0;


  // set pin modes
  pinMode(irparams.recvpin, INPUT);
}

// enable/disable blinking of pin 13 on IR processing
void IRrecv::blink13(int blinkflag)
{
  irparams.blinkflag = blinkflag;
  if (blinkflag)
    pinMode(BLINKLED, OUTPUT);
}

// TIMER2 interrupt code to collect raw data.
// Widths of alternating SPACE, MARK are recorded in rawbuf.
// Recorded in ticks of 50 microseconds.
// rawlen counts the number of entries recorded so far.
// First entry is the SPACE between transmissions.
// As soon as a SPACE gets long, ready is set, state switches to IDLE, timing of SPACE continues.
// As soon as first MARK arrives, gap width is recorded, ready is cleared, and new logging starts
ISR(TIMER2_OVF_vect)
{
  RESET_TIMER2;

  uint8_t irdata = (uint8_t)digitalRead(irparams.recvpin);

  irparams.timer++; // One more 50us tick
  if (irparams.rawlen >= RAWBUF) {
    // Buffer overflow
    irparams.rcvstate = STATE_STOP;
  }
  switch(irparams.rcvstate) {
  case STATE_IDLE: // In the middle of a gap
    if (irdata == MARK) {
      if (irparams.timer < GAP_TICKS) {
        // Not big enough to be a gap.
        irparams.timer = 0;
      } 
      else {
        // gap just ended, record duration and start recording transmission
        irparams.rawlen = 0;
        irparams.rawbuf[irparams.rawlen++] = irparams.timer;
        irparams.timer = 0;
        irparams.rcvstate = STATE_MARK;
      }
    }
    break;
  case STATE_MARK: // timing MARK
    if (irdata == SPACE) {   // MARK ended, record time
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_SPACE;
    }
    break;
  case STATE_SPACE: // timing SPACE
    if (irdata == MARK) { // SPACE just ended, record it
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_MARK;
    } 
    else { // SPACE
      if (irparams.timer > GAP_TICKS) {
        // big SPACE, indicates gap between codes
        // Mark current code as ready for processing
        // Switch to STOP
        // Don't reset timer; keep counting space width
        irparams.rcvstate = STATE_STOP;
      } 
    }
    break;
  case STATE_STOP: // waiting, measuring gap
    if (irdata == MARK) { // reset gap timer
      irparams.timer = 0;
    }
    break;
  }

  if (irparams.blinkflag) {
    if (irdata == MARK) {
      PORTB |= B00100000;  // turn pin 13 LED on
    } 
    else {
      PORTB &= B11011111;  // turn pin 13 LED off
    }
  }
}

void IRrecv::resume() {
  irparams.rcvstate = STATE_IDLE;
  irparams.rawlen = 0;
}



// Decodes the received IR message
// Returns 0 if no data ready, 1 if data ready.
// Results of decoding are stored in results
int IRrecv::decode(decode_results *results) {
  results->rawbuf = irparams.rawbuf;
  results->rawlen = irparams.rawlen;
  if (irparams.rcvstate != STATE_STOP) {
    return ERR;
  }
#ifdef DEBUG
  Serial.println("Attempting PHOENIX_LTX decode");
#endif
  if (decodePHOENIX_LTX(results)) {
    return DECODED;
  }
  if (decodeLAZER_TAG_TEAM_OPS(results)) {
    return DECODED;  
  }
  if (results->rawlen >= 6) {
    // Only return raw buffer if at least 6 bits
    results->decode_type = UNKNOWN;
    results->bits = 0;
    results->value = 0;
    return DECODED;
  }
  // Throw away and start over
  resume();
  return ERR;
}

long IRrecv::decodePHOENIX_LTX(decode_results *results) {
  unsigned long data = 0;
  int offset = 1;
  int i = 0;
  
  results->bits = 0;
  
  //HEAD
  if (results->rawlen < 18) {
    return ERR;
  }
  if (!MATCH_MARK(results->rawbuf[offset], PHOENIX_HDR_MARK)) {
    return ERR;
  }
  offset++;
  if (!MATCH_SPACE(results->rawbuf[offset], PHOENIX_HDR_SPACE)) {
    return ERR;
  }
  offset++;
  if (!MATCH_MARK(results->rawbuf[offset], PHOENIX_HDR_MARK)) {
    return ERR;
  }
  offset++;
  //DATA
  for (i = 0; i < (results->rawlen - 4); i++) {
    if (i % 2 == 1) {
	    if (MATCH_MARK(results->rawbuf[offset], PHOENIX_LTX_ONE_MARK)) {
		    data <<= 1;
		    data |= 1;
		    results->bits++;
	    } else if (MATCH_MARK(results->rawbuf[offset], PHOENIX_LTX_ZERO_MARK)) {
		    data <<= 1;
		    data |= 0;
		    results->bits++;
	    } else {
	      Serial.println(i, DEC);
	      return ERR;
	    }
	  } else {
	    if (!MATCH_SPACE(results->rawbuf[offset], PHOENIX_LTX_SPACE)) {
		    Serial.println(i, DEC);
	      return ERR;
	    }
	  }
	  offset++;
  }
  
  // Success
  results->value = data;
  results->decode_type = PHOENIX_LTX;
  return DECODED;
}

long IRrecv::decodeLAZER_TAG_TEAM_OPS(decode_results *results) {
  unsigned long data = 0;
  int offset = 1;
  int i = 0;
  
  results->bits = 0;
  
  //HEAD
  if (results->rawlen < 14) {
    return ERR;
  }
  if (!MATCH_MARK(results->rawbuf[offset], LTTO_HDR_MARK_ONE)) {
    return ERR;
  }
  offset++;
  if (!MATCH_SPACE(results->rawbuf[offset], LTTO_HDR_SPACE)) {
    return ERR;
  }
  offset++;
  if (!MATCH_MARK(results->rawbuf[offset], LTTO_HDR_MARK_TWO)) {
    return ERR;
  }
  offset++;
  //DATA
  for (i = 0; i < (results->rawlen - 4); i++) {
    if (i % 2 == 1) {
	    if (MATCH_MARK(results->rawbuf[offset], LTTO_ONE_MARK)) {
		    data <<= 1;
		    data |= 1;
		    results->bits++;
	    } else if (MATCH_MARK(results->rawbuf[offset], LTTO_ZERO_MARK)) {
		    data <<= 1;
		    data |= 0;
		    results->bits++;
	    } else {
	      Serial.println(i, DEC);
	      return ERR;
	    }
	  } else {
	    if (!MATCH_SPACE(results->rawbuf[offset], LTTO_SPACE)) {
		    Serial.println(i, DEC);
	      return ERR;
	    }
	  }
	  offset++;
  }
  
  // Success
  results->value = data;
  results->decode_type = LAZER_TAG_TEAM_OPS;
  return DECODED;
}

