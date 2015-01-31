# Furnace_blower_controller
Arduino project to help distribute heated and cooled air throughout my house

NOTE:  This project needs a schematic, which I will upload soon.

USE AT YOUR OWN RISK!

The purpose of this project is to help heat and cool my two-story home.  The problem that I have is that the thermostat is
downstairs, and the hot air mostly stays upstairs.  In the summer, the upstairs gets hot long before the thermostat notices.
In the winter, when the heat first comes on in the morning, it gets too hot upstairs.

Both of these problems are helped by leaving the blower always running, but that's hard on the motor, and seems wasteful.
This project inserts an arduino-controlled relay between my thermostat and the furnace's fan input.  The arduino monitors the various
thermostat signals (heat, cool, fan) and decides when to send the fan signal to the furnace.

The rules are:
  * run when the heat, cool, or fan input is on
  * run for an extra few minutes after the thermostat stops asking
  * run for x out of every y minutes in summer
  * run for j out of every k minutes in winter
  
(Summer is when cooling has been requested in the last week)

An obvious complication is that the signals between the thermostat and the furnace are 24VAC.  Be sure to see the circuit diagram for how this is handled.

