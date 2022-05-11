.. _ug_bt_fast_pair:

Using Google Fast Pair with the |NCS|
#####################################

.. contents::
   :local:
   :depth: 2

Google Fast Pair is a standard for pairing Bluetooth® and Bluetooth Low Energy (LE) devices with as little user interaction required as possible.
Google also provides additional features built upon the Fast Pair standard.
For detailed information about supported functionalities see `Fast Pair`_ documentation.

.. note::
   The Fast Pair support in the |NCS| is experimental.
   The implementation is not yet ready for production and extensions are not supported.

   The implementation does not pass end-to-end integration tests in the Fast Pair Validator.
   The procedure triggered in Android settings is successful (tested with Android 10).

Integrating Fast Pair
*********************

Fast Pair standard integration in the |NCS| consists of the following steps:

1. :ref:`Provisioning the device <ug_bt_fast_pair_provisioning>`
#. :ref:`Setting up Bluetooth LE advertising <ug_bt_fast_pair_advertising>`
#. :ref:`Setting up GATT service <ug_bt_fast_pair_gatt_service>`

These steps are described in the following sections.
For an integration example, see the :ref:`peripheral_fast_pair` sample.

.. rst-class:: numbered-step

.. _ug_bt_fast_pair_provisioning:

Provisioning the device
***********************

A device model must be registered with Google to work as a Fast Pair Provider.
The data is used for procedures defined by the Fast Pair standard.

Registering Fast Pair Provider
------------------------------

See `Fast Pair Model Registration`_ for information how to register the device and obtain the Model ID and Anti-Spoofing Public/Private Key pair.

Provisioning registration data onto device
------------------------------------------

Fast Pair standard requires provisioning the device with Model ID and Anti-Spoofing Private Key obtained during device model registration.
In the |NCS| the provisioning data is generated as :file:`hex` file using :ref:`bt_fast_pair_provision_script`.
User must provide the Model ID, Anti-Spoofing Private Key and address of Fast Pair provisioning partition start.
The address must match start address of the ``bt_fast_pair`` partition defined by the :ref:`partition_manager`.

Once generated, the :file:`hex` file must be manually flashed to device.
For example, you can use nrfjprog to program the device:

.. parsed-literal::
    :class: highlight

    nrfjprog --program provision.hex

The :file:`provision.hex` is name of the generated file containing Fast Pair provisioning data.

.. rst-class:: numbered-step

.. _ug_bt_fast_pair_advertising:

Setting up Bluetooth LE advertising
***********************************

The Fast Pair Provider must include Fast Pair service advertising data in the advertising payload.
The Fast Pair service implementation provides API to generate the advertising data for both discoverable and not-discoverable advertising:

* :c:func:`bt_fast_pair_adv_data_size`, :c:func:`bt_fast_pair_adv_data_fill`
  These functions are used to check buffer size required for the advertising data and fill the buffer with data.
  Managing memory used for the advertising packets is application's responsibility.
  Application shall also make sure that these functions are called from cooperative context to ensure that not-discoverable advertising data generation would not be preempted by an Account Key write from a connected Fast Pair Seeker.
  Account Keys are used to generate not-discoverable advertising data.
* :c:func:`bt_fast_pair_set_pairing_mode`
  The function shall be used to set pairing mode before advertising is started.

Since advertising is controlled by the user, it is user responsibility to use advertising parameters consistent with the specification.
The Bluetooth privacy is selected by the Fast Pair service, but user needs to make sure that private address rotation is synchronized with advertising payload update during not-discoverable advertising.

See `Fast Pair Advertising`_ for detailed information about the requirements related to discoverable and not-discoverable advertising.
See :file:`bluetooth/peripheral_fast_pair/src/bt_adv_helper.c` for an example of the implementation.

.. rst-class:: numbered-step

.. _ug_bt_fast_pair_gatt_service:

Setting up GATT service
***********************

The Fast Pair GATT service is implemented by the :ref:`bt_fast_pair_readme`.
The service implements functionalities required by the `Fast Pair Procedure`_.
The procedure is initiated by the Fast Pair Seeker after Bluetooth LE connection is established.
No application interaction is required.

The Fast Pair GATT service modifies default values of related Kconfig options to follow Fast Pair requirements.
The service also enables the needed functionalities using Kconfig select statement.
See the :ref:`bt_fast_pair_readme` for details.
