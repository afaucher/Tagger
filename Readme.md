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

# References

Additional Thanks:
* Jim from LaserTagParts.com for his awesome public documentation: https://web.archive.org/web/20180706033641/http://www.lasertagparts.com/
* Ken Shirriff for his IR Remote code: http://www.righto.com/
* Kelvin Nelson for his PWM decoding code: https://create.arduino.cc/projecthub/kelvineyeone/read-pwm-decode-rc-receiver-input-and-apply-fail-safe-6b90eb
* FAST LED project
* Sparkfun for their great tutorials & good selection of Fritzing parts


