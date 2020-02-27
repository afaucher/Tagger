# Tagger

A prototype for a laser tag system for use in RC.

Based on: https://github.com/afaucher/LazerTagHost

“Tagger” Copyright 2020 Alexander Faucher

Tagger is compatible but not affiliated with Nerf and Tiger brand Phoenix LTX guns and Laser Tag Team Ops (LTTO) guns.

# Build

The wiring is documented in the Fritzing project file.

![Breadboard](/Fritzing/Fritzing%20Tagger%20Project_bb.png)

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

Additional Thanks
* Jim from LaserTagParts.com for his awesome public documentation: https://web.archive.org/web/20180706033641/http://www.lasertagparts.com/
* Ken Shirriff for his IR Remote code: http://www.righto.com/
* Kelvin Nelson for his PWM decoding code: https://create.arduino.cc/projecthub/kelvineyeone/read-pwm-decode-rc-receiver-input-and-apply-fail-safe-6b90eb
* FAST LED project


