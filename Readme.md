# Tagger

A prototype for a laser tag system for use in RC.

Based on: https://github.com/afaucher/LazerTagHost

“Tagger” Copyright 2020 Alexander Faucher

Tagger is compatible but not affiliated with Nerf and Tiger brand Phoenix LTX guns and Laser Tag Team Ops (LTTO) guns.

# Build

The wiring is documented in the Fritzing project file.

![Breadboard](/Fritzing/Fritzing%20Tagger%20Project_bb.png)

Things to note on the build:
* There are two 100Ω resistors that are identical in the layout.  This is intentional if these need to be adjusted independently later.  In particular, a 100Ω resistor is higher then ideal for the IR LED.
* PCB has not been manufactured yet so take it as-is.
* Headers are intended to be Servo 'J' connectors using right angle headers.
* '3 pin mystery part' is actually TSOP4138 38kHz Infrared Receiver (4.5-5.5V)
* IR led is TSOP6100 10 Degree Infrared Emitters

# Installation

This project has not been tested on an RC plane yet.  The intention is the following layout:

![Layout](/documentation/Component%20Mounting.png)

# Current Status

What works:
* Reading hits and taking damage.
* Tracking health.
* Player 'death'.
* Displaying hits (flash red), health (% green) & death (stay on white) on LED strip.
* Shooting
* Ammo tracking

What doesn't work yet:
* Dual LEDs strips, one for each wing.
* IR LED power amplification
* Configuring player / team via switches
* Powering via onboard battery
* All the physical parts for mounting on a plane.

# Bill of Materials

1. 1x Arduino Uno
  * https://www.sparkfun.com/products/11224
  * $22.95 each
1. 1x Capacitor (10uF, 16 volts)
  * TDK Multilayer Ceramic Capacitors (MLCC) – Leade
  * Mfg P/N: FK20X7R1C106K, Mouser P/N: 81—FK20X7R1C106K
  * $0.54 each
1. 1x 10 Degree 170mW Infrared Emitters
  * Vishay Infrared Emitters
  * Mfg P/N: TSAL6100, Mouser P/N: 782-TSAL6100
  * $0.44 each
1. 3x 38kHz Infrared Receiver (4.5-5.5V)
  * Vishay Infrared Receiver
  * Mfg P/N: TSOP4138, Mouser P/N: 782-TSOP4138
  * $1.16 each
1. 1x Tagger Board (rev1)
  * https://oshpark.com/shared_projects/CIAWogtW
  * $9.45 each
1. 1x Resistor (100ohms, ¼ watt, 5%)
  * Vishay Metal Film Resistors – Through Hole
  * Mfg P/N: CCF07100RJKE36, Mouser P/N: 71-CCF07100RJKE36
  * $0.04 each
1. 1x Resistor (47ohms, TBD)
  * TBD
1. 1x Resistor (1kohms, TBD)
  * TBD
1. 1x Transistor - NPN, 50V 800mA
  * https://www.sparkfun.com/products/13689
  * $0.50 each
1. 1x Breakaway right angle header
  * https://www.sparkfun.com/products/553
  * TBD
  * $1.95 each
1. 2x WS2812 5050 RGB Addressable
  * https://www.readymaderc.com/products/details/rmrc-fire-leds-ws2812-5050-rgb-addressable
  * $3.99 each
1. 1x Double Convex 50mm Lens, 100mm Focal Length
  * https://www.amazon.com/gp/product/B01HH8ECYC
  * $9.99 each
1. 1x Dip Switch - 8 Position
  * https://www.sparkfun.com/products/8034
  * $1.50

# References

Additional Thanks:
* Jim from LaserTagParts.com for his awesome public documentation: https://web.archive.org/web/20180706033641/http://www.lasertagparts.com/
* Ken Shirriff for his IR Remote code: http://www.righto.com/
* Kelvin Nelson for his PWM decoding code: https://create.arduino.cc/projecthub/kelvineyeone/read-pwm-decode-rc-receiver-input-and-apply-fail-safe-6b90eb
* FAST LED project
* Sparkfun for their great tutorials & good selection of Fritzing parts
* Ssaggers on Thingiverse for the inspiration on the lens mounting: [Thingiverse](https://www.thingiverse.com/thing:454862), [Blog](https://teslaandi.wordpress.com/2014/09/29/databullets-2-laser-tagger-handgun-mark-1/)



