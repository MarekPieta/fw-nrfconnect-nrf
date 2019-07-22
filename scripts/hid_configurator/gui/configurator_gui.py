#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic

import logging
import time
import hid
import struct

from configurator import get_device_pid, open_device, NORDIC_VID, EVENT_GROUP_DFU, GROUP_FIELD_POS, DFU_IMGINFO, \
    TYPE_FIELD_POS, exchange_feature_report, EVENT_DATA_LEN_MAX, DFU_REBOOT, SENSOR_OPTIONS, SETUP_MODULE_SENSOR, \
    EVENT_GROUP_SETUP, MOD_FIELD_POS, OPT_FIELD_POS, check_range, KEYBOARD_PID, MOUSE_PID, DONGLE_PID


def get_device_type_from_pid(pid):
    default = 'wrong pid'
    return {
        KEYBOARD_PID: 'keyboard',
        MOUSE_PID: 'mouse',
        DONGLE_PID: 'dongle'
    }.get(pid, default)


class Device:
    @staticmethod
    def create_instance(device_type):
        device = Device()
        device.device_type = device_type
        device.pid = get_device_pid(device_type)
        device.dev = open_device(device_type)
        return device

    @staticmethod
    def list_devices():
        device_list = hid.enumerate(NORDIC_VID, 0)

        for x in device_list:
            print(x)
            if x['usage_page'] == 1:
                if x['product_id'] == 21213:
                    print('Keyboard')
                elif x['product_id'] == 21214:
                    print('Mouse')
        return device_list

    def fwinfo(self):
        event_id = (EVENT_GROUP_DFU << GROUP_FIELD_POS) | (DFU_IMGINFO << TYPE_FIELD_POS)
        recipient = self.pid
        dev = self.dev

        try:
            success, fetched_data = exchange_feature_report(dev, recipient, event_id, None, True)
        except:
            success = False

        if success and fetched_data:
            fmt = '<BIBBHI'
            assert (struct.calcsize(fmt) <= EVENT_DATA_LEN_MAX)

            (flash_area_id, image_len, ver_major, ver_minor, ver_rev, ver_build_nr) = struct.unpack(fmt, fetched_data)
            print(('Firmware info\n'
                   '\tFLASH area id: {}\n'
                   '\tImage length: {}\n'
                   '\tVersion: {}.{}.{}.{}').format(flash_area_id,
                                                    image_len,
                                                    ver_major,
                                                    ver_minor,
                                                    ver_rev,
                                                    ver_build_nr))
            return flash_area_id, image_len, ver_major, ver_minor, ver_rev, ver_build_nr
        else:
            print('FW info request failed')
            return False

    def fwreboot(self):
        event_id = (EVENT_GROUP_DFU << GROUP_FIELD_POS) | (DFU_REBOOT << TYPE_FIELD_POS)
        recipient = self.pid
        dev = self.dev
        try:
            success, fetched_data = exchange_feature_report(dev, recipient, event_id, None, True)
        except:
            success = False

        if success:
            print('Firmware rebooted')

        else:
            print('FW reboot request failed')

        self.dev.close()
        time.sleep(9)
        self.dev = open_device(self.device_type)

    def setcpi(self, value):
        config_name = 'cpi'
        options = SENSOR_OPTIONS
        config_opts = options[config_name]
        module_id = SETUP_MODULE_SENSOR
        opt_id = config_opts.event_id

        value_range = config_opts.range

        recipient = self.pid
        event_id = (EVENT_GROUP_SETUP << GROUP_FIELD_POS) | (module_id << MOD_FIELD_POS) | (opt_id << OPT_FIELD_POS)

        dev = self.dev

        config_value = int(value)
        logging.debug('Send request to update {}: {}'.format(config_name, config_value))
        if not check_range(config_value, value_range):
            print('Failed. Config value for {} must be in range {}'.format(config_name, value_range))
            return "Must be in range {}".format(value_range)

        event_data = struct.pack('<I', config_value)
        try:
            success = exchange_feature_report(dev, recipient, event_id, event_data, False)
        except:
            success = False

        if success:
            print('{} set to {}'.format(config_name, config_value))
            return config_value
        else:
            print('Failed to set {}'.format(config_name))
            return False

    def fetchcpi(self):
        config_name = 'cpi'
        options = SENSOR_OPTIONS
        config_opts = options[config_name]
        module_id = SETUP_MODULE_SENSOR
        opt_id = config_opts.event_id

        value_range = config_opts.range

        recipient = self.pid
        event_id = (EVENT_GROUP_SETUP << GROUP_FIELD_POS) | (module_id << MOD_FIELD_POS) | (opt_id << OPT_FIELD_POS)

        dev = self.dev

        logging.debug('Fetch the current value of {} from the firmware'.format(config_name))
        try:
            success, fetched_data = exchange_feature_report(dev, recipient, event_id, None, True)
        except:
            success = False

        if success and fetched_data:
            val = int.from_bytes(fetched_data, byteorder='little')
            print('Fetched {}: {}'.format(config_name, val))
            return val
        else:
            print('Failed to fetch {}'.format(config_name))
            return False


def test():
    print("Configurator GUI test")
    Device.list_devices()


if __name__ == '__main__':
    try:
        test()
    except KeyboardInterrupt:
        pass


