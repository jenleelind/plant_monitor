Arduino Plant Monitor
=====================

Read temperature, humidity, pressure, and light for my indoor plants, and log it to https://data.sparkfun.com/jenleelind_plants

Takes a reading every second, and keeps a running average for 5 minutes. Every 5 minutes, logs average reading to web.

**Requires:**

- Hardware: https://www.sparkfun.com/wish_lists/99075
- https://github.com/sparkfun/SFE_CC3000_Library
- https://github.com/sparkfun/HTU21D_Breakout
- https://github.com/sparkfun/MPL3115A2_Breakout

**Problems:**

- **Innacurate readings.** Temperature consistently reads too high and humidity usually reads too low. Partially due to heat from the CC3000, which I have somewhat mitigated with insulative foam. Perhaps also because the temperature readings come from pressure and humidity sensors that were meant to measure the temperature of the part, not ambient temperature.

- **WiFi Dropout?** Data will randomly stop showing up online, even though the "taking a reading" LED is stil flashing. Perhaps it's falling off wifi? Will attempt to connect on every write to see if that fixes the issue. (Connect method just returns flase if already connected.)


