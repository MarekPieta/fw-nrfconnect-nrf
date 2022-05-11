.. _peripheral_fast_pair:

Bluetooth: Fast Pair
####################

.. contents::
   :local:
   :depth: 2

The Bluetooth® Fast Pair sample demonstrates :ref:`ug_bt_fast_pair`.
Google Fast Pair Service (GFPS) is a standard for pairing Bluetooth and Bluetooth LE devices with as little user interaction required as possible.
Google also provides additional features built upon the Fast Pair standard.
For detailed information about supported functionalities, see `Fast Pair`_ documentation.

.. note::
   The Fast Pair support in the |NCS| is experimental.
   See :ref:`ug_bt_fast_pair` for details.

Overview
********

The sample works as a Fast Pair Provider and a simple HID multimedia controller.
Two buttons are used to control audio volume of the connected Bluetooth Central.

The device can be used to bond with the following devices:

* Fast Pair Seeker - For example, an Android device.
  The bonding follows `Fast Pair Procedure`_ with Bluetooth man-in-the-middle (MITM) protection.
  The device is linked with the user's Google account.
* Bluetooth Central which is not a Fast Pair Seeker - Normal Bluetooth LE bonding is used in this scenario and there is no Bluetooth MITM protection.

The sample supports only one simultaneous Bluetooth connection, but it can be bonded with multiple Bluetooth Centrals.

The sample supports both the discoverable and not-discoverable Fast Pair advertising.
The device keeps the Bluetooth advertising active until a connection is established.
While maintaining the connection, the Bluetooth advertising is disabled.
The advertising is restarted after disconnection.

See `Fast Pair Advertising`_ for detailed information about discoverable and not-discoverable advertising.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

.. include:: /includes/hci_rpmsg_overlay.txt

Fast Pair device registration
=============================

Before a device can be used as a Fast Pair Provider, you must register the device model with Google.
This is required to obtain Model ID and Anti-Spoofing Private Key.

See :ref:`ug_bt_fast_pair_provisioning` for details.

.. tip::
   The sample provides TX power in the Bluetooth advertising data.
   There is no need to provide the TX power value during device model registration.
   The device is Bluetooth LE only so :guilabel:`Skip connecting audio profiles (e.g. A2DP, HFP)` should be selected.

Seeker device needed
====================

A Fast Pair Seeker device is required to test the Fast Pair procedure.

For example, an Android device can be used as the Seeker device.
To test with a debug mode Model ID, the Android device must be configured to include debug results while displaying nearby Fast Pair Providers.
For details, see `Verifying Fast Pair`_ in the GFPS documentation.

Not-discoverable advertising requirements
   Testing not-discoverable advertising requires using at least two Android devices registered to the same Google account.
   The first one needs to be bonded with Fast Pair Provider before not-discoverable advertising can be detected by the second one.

User interface
**************

The sample supports a simple user interface.
You can control the sample using predefined buttons, while LEDs are used to display information.

Buttons
=======

Button 1:
   Toggles between Fast Pair discoverable and not-discoverable advertising.
   Keep in mind that advertising is disabled while connected to a Bluetooth Central.

Button 2:
   Increases audio volume of the connected Bluetooth Central.

Button 4:
   Decreases audio volume of the connected Bluetooth Central.

LEDs
====

LED 1:
   Keeps blinking with constant interval to indicate that firmware is running.

LED 2:
   Depending on the Bluetooth Central connection status:

   * On if the Central is connected over Bluetooth.
   * Off if there is no Central connected.

LED 3:
   Depending on the discoverability setting:

   * On if the device is Fast Pair discoverable.
   * Off if the device is Fast Pair not-discoverable.

Configuration
*************

|config|

Building and running
********************

.. |sample path| replace:: :file:`samples/bluetooth/peripheral_fast_pair`

.. include:: /includes/build_and_run.txt

Before running the sample, you must complete `Fast Pair device registration`_  and provision the device with the obtained Fast Pair provisioning data that contains Model ID and related Anti-Spoofing Private Key.
The address of the ``bt_fast_pair`` partition can be checked in the :file:`partitions.yml` file generated in the build directory after building the sample.
See :ref:`ug_bt_fast_pair_provisioning` for detailed guide.

.. note::
   The sample cannot be used without the Fast Pair provisioning data.
   Programming device with the sample firmware without providing the proper Fast Pair provisioning data will result in assertion failure during boot.

Testing
=======

After programming the sample to your development kit, test it by performing the following steps:

1. |connect_terminal_specific|
   The sample provides Fast Pair debug logs to inform about state of the Fast Pair procedure.
#. Reset the kit.
#. Observe that **LED 1** is blinking (firmware is running) and **LED 3** is turned on (device is Fast Pair discoverable).
   This means that the device is now working as Fast Pair Provider and is advertising.
#. On the Android device, go to :guilabel:`Settings` > :guilabel:`Google` > :guilabel:`Devices & sharing` or :guilabel:`Device connections` > :guilabel:`Devices`.
#. Move the Andoid device close to the advertising Fast Pair Provider.
#. Wait for Android device's notification about the detected Fast Pair Provider.
   The notification is similar to the following one:

   .. figure:: /images/bt_fast_pair_discoverable_notification.png
      :alt: Fast Pair discoverable advertising Android notification

   The device model name and displayed logo depend on the data provided during the device model registration.
#. Tap the :guilabel:`Connect` button to initiate the connection and trigger the Fast Pair procedure.
   After the procedure is finished, the pop-up is updated to inform about successfully completed Fast Pair procedure.
   **LED 2** turns on to indicate that the device is connected with the Bluetooth Central.

   .. note::
      Some Android devices might disconnect right after the Fast Pair procedure is finished.
      Go to :guilabel:`Settings` > :guilabel:`Bluetooth` and tap on the bonded Fast Pair Provider to reconnect.

   The connected Fast Pair Provider can now be used to control audio volume of the Bluetooth Central.
#. Press **Button 2** to increase the audio volume.
#. Press **Button 4** to decrease the audio volume.

Not-discoverable advertising
----------------------------

Testing not-discoverable advertising requires using a second Android device that is registered to the same Google account as the first Android device.

Test not-discoverable advertising by completing `Testing`_ and performing the following additional steps:

#. Disconnect the Android device that was used during the default `Testing`_:

   a. Go to :guilabel:`Settings` > :guilabel:`Bluetooth`
   #. Tap on the connected device name to trigger disconnection.

      .. note::
         Do not remove Bluetooth bond information related to the Fast Pair Provider.

#. Make sure that the Fast Pair Provider is added to :guilabel:`Saved devices` on the Android device that was used for `Testing`_:

   a. Go to :guilabel:`Settings` > :guilabel:`Google` > :guilabel:`Devices & sharing` or :guilabel:`Device connections` > :guilabel:`Devices` > :guilabel:`Saved devices`.
   #. Verify that the paired device is appearing on the list.

#. Press **Button 1** to switch to the Fast Pair not-discoverable advertising.
   **LED 3** turns off.
#. Wait until the Fast Pair Provider is added to :guilabel:`Saved devices` on the second Android device:

   a. Go to :guilabel:`Settings` > :guilabel:`Google` > :guilabel:`Devices & sharing` or :guilabel:`Device connections` > :guilabel:`Devices` > :guilabel:`Saved devices`.
      The paired device should appear on the list.
   #. If the device does not appear on the list, wait until the data is synced between phones.

#. Move the second Android device close to the Fast Pair Provider.
   A notification similar to the following one appears:

   .. figure:: /images/bt_fast_pair_not_discoverable_notification.png
      :alt: Fast Pair not-discoverable advertising Android notification

#. Tap on the notification to trigger the Fast Pair procedure.
#. Wait for the notification about successful Fast Pair procedure.
   **LED 2** is turned on to inform that the device is connected with the Bluetooth Central.

   .. note::
      Some Android devices might disconnect right after Fast Pair procedure is finished.
      Go to :guilabel:`Settings` > :guilabel:`Bluetooth` and tap on the bonded Fast Pair Provider to reconnect.

   The connected Fast Pair Provider can now be used to control the audio volume of the Bluetooth Central.
#. Press **Button 2** to increase the audio volume.
#. Press **Button 4** to decrease the audio volume.

Dependencies
************

This sample uses the :ref:`bt_fast_pair_readme` and its dependencies.
The sample is configured to meet the requirements of Fast Pair standard.
See :ref:`ug_bt_fast_pair` for details about integrating Fast Pair in the |NCS|.

The :ref:`bt_fast_pair_provision_script` is used to generate :file:`hex` file containing Fast Pair Model ID and Anti Spoofing Private key.
