spaduino
========

spaduino is a hot tub controller implemented using an arduino UNO compatible controller. It was used as a drop-in MCU replacement for a Spa Quip SP500AMKI, manufactured by Spa Quip/Davey in New Zealand.

I really can't stress enough how much anyone reading this should regard this project as **INFORMATIONAL ONLY**, and should not under **ANY** circumstances attempt anything remotely similar. This project involves rediculous quantities of both high pressure water and high current electricity in potentially **LETHAL** proximity.

The project consists of three applications:

* [**SpaController**][sc] is the audino sketch for the core firmware. The [rpi-ios branch][sc-rpi-ios] is the version used during the development phase, which used a serial protocol for interactivity and an RTC source. The [master branch][sc] is the final self-contained version providing user interaction and display via the unit's original front-panel interface, and an integrated module as an RTC source.

* [**SpaServer**][ss] is a simple server which in the first phase was run on a raspberry-pi running Arch Linux, connected to the arduino via RS232. It primarily acted as a Wi-Fi enabled console host and relay on top of the arduino's serial protocol, as well as providing an RTC source via a chrony NTP daemon.

* [**Slimer**][sl] is an iOS frontend also used during the first phase. It connected to the SpaServer using Google Protocol Buffers over TCP using NSStream, and provided an interface for system interaction and status display.

Schematics I used to grok the original design are available separately:

* gEDA/gaf sources: [tinybrain/spaduino-schematics][sch]

* Combined PDF: [sp500-tinybrain-combined.pdf][schpdf].

SpaController
-------------

### Overview

1. **Test Mode**

  The controller can be started in a pin-assignment mode which basically flips each IO pin in sequence each second. The mode is set with a magic number in EEPROM, `23` for test mode, and `42` for normal operation.

2. **Peripherals**

  * Control Panel - the unit's original panel providing:

    Error LED `ERR_LED`
    Piezo alarm `PAD_PZ`
    Three digit 7 segment LED display via external shift
    registers `SHR_LATCH` `SHR_DATA` `SHR_CLK`.
    Navigation buttons `PAD_LEFT` `PAD_RIGHT` `PAD_UP` `PAD_DOWN`

  * RTC - [Maxim DS3232 I2C RTC][maxim-ds3232] on a [module][ft-rtc] `I2C_SDA` `I2C_SCL`.

3. **Sensors**

  * Water Level - IR LED transmitter and photodiode receiver `WL_OUT, WL_IN`.

  * High Limit Sense - H11AA1 optocoupler for sensing current through the heating element `HL_IN`.

  * Digital Temperature - Dallas Semiconductor DS1820 `ONE_WIRE`.

4. **Power Relays**

  Each of the four power relays uses a +5V GPIO line connected to a ULN2803 darlington array. The array outputs a high current +12V signal which drives the relay coils.

  * `RLY_SAFETY`

    A master relay supplying the other relays. When entering an error state, driving this line provides additional protection against electrical failures where the other power relays fail to open.

  * `RLY_PUMP`

    Supplies the main pump induction motor (1.6kW).

  * `RLY_HEAT`

    Supplies the heating element (2kW), and is wired to the output of `RLY_PUMP` (see `Pump.Heat` 5.2).

  * `RLY_AUX`

    Supplies an auxiliary blower induction motor (700W).

5. **Finite State Machines**

  5.1 `Mode`

    The controller's primary and "high level" state, corresponding to the various user selectable modes and error states.

    * `ERR`:`Error`

      A fatal error produced by sensor failure or limits. The displayed error codes are:

      `___`:`Unknown`
      `CLK`:`Clock` (i.e. RTC)
      `TPS`:`TemperatureSensor`
      `H20`:`WaterLevelSensor`
      `CUR`:`HighLimit`

    * `INI`:`Init`

      Entry state, during which sensors and peripherals are configured.

    * `OFF`:`Off`

      Idle state with all sensors peripherals initialised.

    * `AUT`:`Autoheat`

      The default idle state, during which operation is determined via the scheduler and thermal control.

    * `RPD`:`Rapidheat`

      A 24hr cycle during which thermal regulation takes priority over any scheduling rules. This is for bringing the water back to temperature after changing out the water.

    * `ON_`:`Soak`

      The whole point of the exercise. A 1hr cycle during which automatic scheduling is suspended and the pump is enabled.

  5.2 `Pump`

    Controls the pump motor relay `RLY_PUMP` and heating element relay `RLY_HEAT`. These are combined into a single state as the heating element must only be supplied when there is sufficient water flow and is therefore dependent on the pump.

    The `Pump` FSM also enforces the dependancy by all relay states - including the independent `Aux` FSM - on the master relay `RLY_SAFETY`.

    * `Error`

      Set when entering `Mode.Error`. `RLY_SAFETY` `RLY_PUMP` `RLY_HEAT` are cleared, as well as `RLY_AUX` by setting `Aux.Off`.

    * `Off`

      `RLY_SAFETY` enabled.

    * `On`

      `RLY_SAFETY` `RLY_PUMP` enabled.

    * `Heat`

      `RLY_SAFETY` `RLY_PUMP` `RLY_HEAT` enabled.

  5.3 `Aux`

    Provides independent control of the auxiliary relay `RLY_AUX`. It is a separate state machine as its operation does not depend on the state of the pump. It is however dependent on the state of the heater, as well as the master relay `RLY_SAFETY`.

    * `Off`

      `RLY_AUX` is cleared.

    * `On`

      `RLY_AUX` enabled. If supplying a blower which causes the total load to exceed the rating of the mains circuit, it must ensure the heater is disabled by switching from `Pump.Heat` to `Pump.On`

  5.4 `Menu`

    Manages the handlers and output relevant to the control panel.

    * `Error`

      Set from `Mode.Error`, displaying the relevant error code.

    * `Mode`

      Displays the mode when entering any mode during normal operation.

    * `NN.N`:`Setpoint`

      Allows adjustment of the thermal control setpoint in 0.1°C increments.

    * `---`:`Timer`

      Displays a countdown or other value relevant to the current scheduler mode.

    * `BL._`:`Blower`

      Allows selection between `Aux.Off` and `Aux.On`

    * `FT._`:`Footwell`
      `ST._`:`Steps`
      `HD._`:`Head`
      `PT._`:`Path`

      Allows switching of low voltage auxiliary lighting circuits.

    * `RST`:`SoftReset`

      Executes `asm volatile ("  jmp 0")`

6. **Thermal Control**

  Because of the latencies inherent in the system, a complicated PID controller isn't required, and the thermal regulation is fairly basic.

  A setpoint is adjustable from the control panel in 0.1°C increments, and set with `Thermal::setSetPoint` which limits the value within the range of 20°C and 40°C.

  The thermal controller is updated by `Thermal::update`, called by a timer with an interval defined using the `TS_INTERVAL` macro, which calculates the sensor's response time based on `TS_RESOLUTION`.

  To adjust for noise in local temperature measurements, an average temperature value is continuously updated by smoothing it with the incoming sensor values.

  A trigger state is then evaluated by comparing the averaged temperature against the setpoint using a sort of poor man's binary hysteresis. The trigger state is set to `tsHigh` when the setpoint is reached, and to `tsLow` when the temperature drops at least 1°C below.

  The combination of the smoothing and the trigger results in sensibly conservative durations of heater operation, at the same time barely reaching the duty cycle limits set by the scheduler, while sufficiently aggressive in maintaining a comfortable temperature during use.

7. **Scheduler**

  The scheduler is a bit ad-hoc and lacks any interactivity features. My requirements were pretty straight forward, requiring temperature maintenance to be predominantly performed during off-peak periods, and either disabling or restricting operation during shoulder and peak periods. I also wanted to define exception periods such as ensuring the temperature was correct at the end of the week.

  To allow for flexible tweaking, the scheduler implemented a system of ordered schedule items, which could be enabled during any given period of time on one or more days of the week, and system operation quotas defined in terms of an "ideal duty cycle", specified as a minimum and maximum permitted durations over a given period.

  Minimum quotas are required to ensure that the system performs complete filtration of the water on a regular basis.

  The schedule is defined as an array of schedule items, each consisting

  * `Days`

    The days of the week are enumerated bit masks which are used in some useful combinations:

    ``` C++
    enum eWeekDay
    {
      Sun = 0x02,
      Mon = 0x04,
      Tue = 0x08,
      Wed = 0x10,
      Thu = 0x20,
      Fri = 0x40,
      Sat = 0x80
    };

    #define Weekdays  Mon | Tue | Wed | Thu | Fri
    #define Weekend   Sun | Sat
    #define AllWeek   Weekdays | Weekend
    ```

  * `Start` & `End`

    Times between which the schedule item is in effect on the specified days, specified as the number of seconds since midnight. Schedule items are defined within a 24hr period, and so `Start` must be less than `End`.

  * `Period`

    Defines the duration within which duty cycle quotas must be met.

  * `MinDuty` & `MaxDuty`

    The minimum and maximum durations of pump and/or heating operation for each `Period`.

[sc]: https://github.com/tinybrain/spaduino/tree/master/SpaController
[sc-rpi-ios]: https://github.com/tinybrain/spaduino/tree/rpi-ios/SpaController
[ss]: https://github.com/tinybrain/spaduino/tree/master/SpaServer
[sl]: https://github.com/tinybrain/spaduino/tree/master/Slimer
[sch]: https://github.com/tinybrain/spaduino-schematics/
[schpdf]: https://github.com/tinybrain/spaduino-schematics/raw/master/sp500-tinybrain-combined.pdf

[maxim-ds3232]: https://www.maximintegrated.com/en/products/digital/real-time-clocks/DS3232.html
[ft-rtc]: http://www.freetronics.com.au/collections/modules/products/real-time-clock-rtc-module
