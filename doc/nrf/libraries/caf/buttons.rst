.. _caf_buttons:

CAF: Buttons module
###################

.. contents::
   :local:
   :depth: 2

The buttons module of the Common Application Framework is responsible for generating events related to key presses.
The source of events are changes to GPIO pins.

Configuration
*************

The module is enabled with the :option:`CONFIG_CAF_BUTTONS` define.

The module can handle both matrix keyboard and buttons connected directly to GPIO pins.

When defining how buttons are connected, two arrays are used:

* The first array contains pins associated with matrix rows.
* The second array contains pins associated with columns, and it can be left empty (buttons will be assumed to be directly connected to row pins, one button per pin).

Both arrays are defined in the configuration file.
Its location must be specified by :option:`CONFIG_CAF_BUTTONS_DEF_PATH`.

By default, a button press is indicated by the pin switch from the low to the high state.
You can change this with :option:`CONFIG_CAF_BUTTONS_POLARITY_INVERSED`, which will cause the application to react to an opposite pin change (from the high to the low state).

Implementation details
**********************

The module can be in the following states:

* ``STATE_IDLE``
* ``STATE_ACTIVE``
* ``STATE_SCANNING``
* ``STATE_SUSPENDING``

After initialization, the module starts in ``STATE_ACTIVE``.
In this state, the module enables the GPIO interrupts and waits for the pin state to change.
When a button is pressed, the module switches to ``STATE_SCANNING``.

When the switch occurs, the module submits a work with a delay set to :option:`CONFIG_CAF_BUTTONS_DEBOUNCE_INTERVAL`.
The work scans the keyboard matrix for changes to button states and sends the related events.
If the button is kept pressed while the scanning is performed, the work will be re-submitted with a delay set to :option:`CONFIG_CAF_BUTTONS_SCAN_INTERVAL`.
If no button is pressed, the module switches back to ``STATE_ACTIVE``.

If :option:`CONFIG_CAF_BUTTONS_PM_EVENTS` is enabled module can create and react to power management events.
If the ``wakup_up_event`` is created within the system buttons module enters low-power state.
The module state changes to ``STATE_IDLE``, in which it waits for GPIO interrupts that indicate a change to button states.
When an interrupt is triggered, the module will issue a system wake-up event.

If a request for power off comes while the button is pressed, the module switches to ``STATE_SUSPENDING`` and remains in this state until no button is pressed.
Then, it will switch to ``STATE_IDLE``.
