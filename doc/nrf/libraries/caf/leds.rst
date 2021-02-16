.. _caf_leds:

CAF: LEDs module
################

.. contents::
   :local:
   :depth: 2

The Common Application Framework's LEDs module is responsible for controlling LEDs, in response to LED effect set by a ``led_event``.
The source of events could be any other module.

LED effect
**********

The LED effect defines the LED behavior over time for the LED diodes by setting their brightness level periodically.
This allows for different RGB or monochromatic colors.
An example may be an LED that is blinking or breathing with a given color.
Such LED behavior is referred to as *LED effect*.

The LED effect (:c:struct:`led_effect`) is described by the following characteristics:

* Pointer to array of LED steps (:c:member:`led_effect.steps`).
* Size of the array (:c:member:`led_effect.step_count`).
* Flag indicating if the sequence should start over after it finishes (:c:member:`led_effect.loop_forever`).

To achieve the desired LED effect, the LED color is updated periodically based on LED steps defined for the given LED effect, which in turn are divided in multiple smaller updates called *substeps*.

During every substep, the next LED color is calculated using a linear approximation between the current LED color and the :c:member:`led_effect_step.color` described in the next LED step.
A single LED step also defines the number of substeps for color change between the given LED step and the previous one (:c:member:`led_effect_step.substep_count`), as well as the period of time between color updates (:c:member:`led_effect_step.substep_time`).
After achieving the color described in the next step, the index of the next step is updated.

After the last step, the sequence restarts if the :c:member:`led_effect.loop_forever` flag is set for the given LED effect.
If the flag is not set, the sequence stops and the given LED effect ends.

Module events
*************

.. include:: event_propagation.rst
    :start-after: table_leds_start
    :end-before: table_leds_end

.. note::
    |nrf_desktop_module_event_note|

Configuration
*************

The module uses Zephyr's :ref:`zephyr:pwm_api` driver for setting the LED color (that is, the brightness of the diodes).
To use the module, set the following configuration options:

* :option:`CONFIG_PWM`

The PWM ports used by the application needs to be enabled in the ``dts`` file, specific for the board or by :file:`dts.overlay` file.
This shall be done by setting "okay" status for relevant PWM ports.
Make sure to configure all PWM ports that are used by the application.

Symbols from Zephyr's :ref:`zephyr:dt-guide` are used to define the mapping of the PWM channels to pin numbers.

The module is enabled using :option:`CONFIG_CAF_LEDS`.

You can use the following additional configuration options:

* :option:`CONFIG_CAF_LEDS_DEF_PATH` - Location of the configuration file.
* :option:`CONFIG_CAF_LEDS_PM_EVENTS` - Enable reaction on power management events.
* :option:`CONFIG_CAF_LEDS_COUNT` - Number of LEDs in use.
* :option:`CONFIG_CAF_LEDS_COLOR_COUNT` - Number of color channels per diode.
* :option:`CONFIG_CAF_LEDS_BRIGHTNESS_MAX` - Maximum value of LED brightness.

The configuration for the board is written in the file specified by :option:`CONFIG_CAF_LEDS_DEF_PATH`.
When defining LEDs configuration array of arrays is used.
It contains PWM port and pin configuration for each LED used in the application.

Matching `dts` file must be created in order to configure PWM ports accordingly to configuration file.

Implementation details
**********************

The LED color is achieved by setting the proper pulse widths for the PWM signals.
To achieve the desired LED effect, colors for the given LED are periodically updated using work (:c:struct:`k_delayed_work`).
One work automatically updates the color of a single LED.

This module turns off all LEDs when the application goes to the power down state.
In such case, the PWM drivers are set to the suspended state to reduce the power consumption.
If the application goes to the error state, the LEDs are used to indicate error.
