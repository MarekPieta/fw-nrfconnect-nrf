.. _bt_fast_pair_readme:

Google Fast Pair Service (GFPS)
###############################

.. contents::
   :local:
   :depth: 2

The Google Fast Pair Service implements a Bluetooth® LE GATT Service required when :ref:`ug_bt_fast_pair`.

Service UUID
************

The Fast Pair service uses UUID of ``0xFE2C``.

Characteristics
***************

The Fast Pair GATT characteristics are described in detail in the `Fast Pair GATT Characteristics`_ documentation.
The implementaion available in the |NCS| follows these requirements.

.. note::
   The Additional Data characteristic is not supported yet.

Configuration
*************

Set the :kconfig:option:`CONFIG_BT_FAST_PAIR` to enable the module.

The following Kconfig options are also available for this module:

* :kconfig:option:`CONFIG_BT_FAST_PAIR_STORAGE_ACCOUNT_KEY_MAX` - The option configures maximum number of stored Account Keys.
* :kconfig:option:`CONFIG_BT_FAST_PAIR_CRYPTO_TINYCRYPT`, :kconfig:option:`CONFIG_BT_FAST_PAIR_CRYPTO_MBEDTLS` - These options are used to select cryptographic backend for Fast Pair.
  MbedTLS is used by default.

See the Kconfig help for details.

Override of default Kconfig values
==================================

To simplify configuration, the GFPS modifies default values of related Kconfig options to follow Fast Pair requirements.
The service also enables some of the functionalities using Kconfig select statement.

Bluetooth privacy
-----------------

The service selects Bluetooth privacy (:kconfig:option:`CONFIG_BT_PRIVACY`).
The service sets the default value of :kconfig:option:`CONFIG_BT_RPA_TIMEOUT` to 3600 seconds.
During not-discoverable advertising, the RPA address rotation shall be done together with Fast Pair payload update.
Because of this, the RPA address cannot be rotated by Zephyr in the background.

Firmware Revision characteristic
--------------------------------

The Fast Pair specification requires enabling GATT Device Information Service and the Firmware Revision characteristic.
The default values of :kconfig:option:`CONFIG_BT_DIS` and :kconfig:option:`CONFIG_BT_DIS_FW_REV` are set to enabled.
The default value of :kconfig:option:`CONFIG_BT_DIS_FW_REV_STR` is set to :kconfig:option:`CONFIG_MCUBOOT_IMAGE_VERSION` if :kconfig:option:`CONFIG_BOOTLOADER_MCUBOOT` is enabled.
Otherwise, it is set to ``0.0.0+0``.

MTU configuration
-----------------

The Fast Pair specification suggests using ATT maximum transmission unit (MTU) value of ``83`` if possible.
Because of this requirement, the default values of the following Kconfig options are modified by the GFPS Kconfig:

* :kconfig:option:`CONFIG_BT_L2CAP_TX_MTU`
* :kconfig:option:`CONFIG_BT_BUF_ACL_TX_SIZE`
* :kconfig:option:`CONFIG_BT_BUF_ACL_RX_SIZE`
* :kconfig:option:`CONFIG_BT_CTLR_DATA_LENGTH_MAX`

.. tip::
   In case of the nRF53, this part of the configuration cannot be automatically updated for the network core and it must be manually aligned by the user.
   These options on the network core shall be set to default values specified by GFPS Kconfg.

Partition Manager
-----------------

The Fast Pair provisioning data is preprogrammed to a dedicated flash memory partition.
The GFPS selects :kconfig:option:`CONFIG_PM_SINGLE_IMAGE` to enable the :ref:`partiton_manager`.

Settings
--------

The GFPS uses Zephyr's :ref:`zephyr:settings_api` to store Account Keys.
Because of this, the GFPS selects :kconfig:option:`CONFIG_SETTINGS`.

Implementation details
**********************

The implementation uses :c:macro:`BT_GATT_SERVICE_DEFINE` to statically define and register the Fast Pair GATT service.
The service automatically handles all of the requests received from the Fast Pair Seeker.
No application input is required to handle the requests.

Bluetooth authentication
------------------------

The service overrides the set of Zephyr's Bluetooth authentication callbacks (:c:struct:`bt_conn_auth_cb`) while handling Key-Based Pairing request.
The callbacks are updated to report proper device capabilities for Bluetooth LE pairing request/response packets.
Overriding callbacks allows the GFPS to take over Bluetooth authentication during `Fast Pair Procedure`_ and perform all of the required operations without application interaction.
After the procedure is finished, the previously used callbacks are assigned back.

The Fast Pair Provider can still bond using a normal Bluetooth LE bonding procedures, without using Fast Pair.
The normal BLE bonding procedure can be used, for example, if the connected Bluetooth Central does not support Fast Pair.
In that case Bluetooth authentication callbacks registered by the application are used.

.. note::
   Zephyr supports only one set of Bluetooth authentication callbacks.
   Because of this limitation, it is not possible to simultaneously handle bonding using Fast Pair procedure and normal Bluetooth LE bonding.
   In that case improper device capabilities would be reported.

   It's user responsibility to make sure that the normal BLE bonding procedure and Fast Pair procedure do no overlap with each other.
   The simples approach is to avoid handling more than one bonding procedure at a time.

API documentation
*****************

| Header file: :file:`include/bluetooth/services/fast_pair.h`
| Source files: :file:`subsys/bluetooth/services/fast_pair`

.. doxygengroup:: bt_fast_pair
   :project: nrf
   :members:
