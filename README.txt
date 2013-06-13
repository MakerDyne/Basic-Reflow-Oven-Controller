==============================================================================
======================== Basic Reflow Oven Controller ========================
==============================================================================

Author:	Richard Leszczynski
Email: 	richard@makerdyne.com
Web:	www.MakerDyne.com

WARNING: DO NOT LEAVE UNATTENDED WHEN IN USE

Notes on the hardware:

This code has been written to control a tabletop "toaster oven" for the
purpose of more accurately using it to carry out the reflow soldering
of PCBs with SMT components.

The code runs on an Arduino microcontroller (tested on an Uno) and outputs
information on the reflow process via a serial connection in a comma-separated
format that is suitable for later plotting.

A minimum of external components are required and all can easily be
accommodated on a small breadboard with the exception of the device that is
used to switch the oven on and off.

A Powerswitch Tail is used to safely switch the oven's mains voltage on/off.
This approach means that all mains wiring connections are contained within
the Powerswitch Tail's enclosure:- none are brought out to any switching
mechanism on the breadboard and none are exposed.

A k-type thermocouple is used to monitor the temperature inside the oven.

There are two switches specified below. It may seem wasteful to use two
different SPST-NO "Start" and "Stop" switches, but this approach allows
the user to replace the simple, small SPST-NO Stop switch with a larger,
latching "E-Stop" style stop switch for increased safety if they prefer.

There are a total of 5 LEDs for visual monitoring of the process control.
2 LEDs provide an indication of reflow process on/off, oven heat on/off
3 LEDs provide a "Goldilocks" view of the temperature control:- The oven
is too hot, just right, or too cold.

You will require:
- 1 toaster oven
- 1 small breadboard + jumper wires
- 1 Arduino microcontroller
- 2 tactile SPST-NO switches (which make use of the MCU's internal pullups)
- 1 green LED + resistor to indicate if the reflow process is in progress.
- 1 red LED + resistor to indicate if the oven element is on or not.
- 1 blue LED + resistor to indicate that the oven is too hot
- 1 green LED + resistor to indicate that the oven is "just right"
- 1 blue LED + resistor to indicate that the oven is too cold
- 1 Adafruit MAX31855 thermocouple amplifier breakout board
  http://www.adafruit.com/products/269
- 1 K-type thermocouple
  http://www.adafruit.com/products/270
- 1 Powerswitch Tail for safely switching the oven's mains voltage on/off
  http://www.powerswitchtail.com/Pages/PSTKKit.aspx
  
Notes on the software:

There is no fancy PID control here. It's a simpler approach that applies
"Oven is too hot, turn it off!" and "Oven is too cold, turn it on!" logic
at discrete intervals.

There are five reflow stages in total:
1. Ramp to soak
2. Soak
3. Ramp to reflow
4. Reflow
5. Cooling

There are four parameters that need to be set for each reflow stage:
1. Target temperature
2. Permitted temperature +- error
3. Stage duration
4. Stage control interval (monitor the process every X seconds)

During cooling, it may be necessary to manually open and close the oven 
door according to the state of the three Goldilocks LEDs.