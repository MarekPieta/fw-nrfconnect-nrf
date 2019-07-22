#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic

from kivy.app import App
from kivy.config import Config
from kivy.uix.button import Button
from kivy.uix.gridlayout import GridLayout

from configurator_gui import KEYBOARD_PID, MOUSE_PID, NORDIC_VID, get_device_type_from_pid, Device

Config.set('input', 'mouse', 'mouse,multitouch_on_demand')  # turn off right click red dots

GENERIC_DESKTOP_PAGE = 1


class MouseOptions(GridLayout):
    pass


class Gui(App):
    def add_mouse_settings(self):
        mouse_options = MouseOptions()
        self.root.ids.possible_settings_place.add_widget(mouse_options)

    def clear_possible_settings(self):
        print("clear possible settings")
        self.root.ids.possible_settings_place.clear_widgets()

    def setcpi_callback(self, value):
        self.device.setcpi(value)

    def fetchcpi_callback(self):
        returned = self.device.fetchcpi()
        if 100 <= returned <= 12000:
            return returned
        else:
            return 1600

    def initialize_dropdown(self):
        device_list = Device.list_devices()
        for device in device_list:
            if device['usage_page'] == GENERIC_DESKTOP_PAGE and device['vendor_id'] == NORDIC_VID:
                print("add {} to dropdown".format(device['product_id']))
                self.add_dropdown_button(device['product_id'])

    def initialize_device(self, pid):
        self.device = Device.create_instance(get_device_type_from_pid(pid))
        print("initialize device with pid: {}".format(pid))

    def show_possible_settings(self):
        label = self.root.ids.settings_label
        if self.device.pid == KEYBOARD_PID:
            label.text = 'there are no available keyboard options for now'
        elif self.device.pid == MOUSE_PID:
            label.text = 'mouse options'
            self.add_mouse_settings()
        else:
            label.text = 'Unrecognised device'

    def add_dropdown_button(self, pid):
        dropdown = self.root.ids.dropdown
        button = Button()
        button.size_hint_y = None
        button.height = '48dp'
        button.text = get_device_type_from_pid(pid)
        button.bind(on_release=lambda btn: self.show_fwinfo())
        button.bind(on_release=lambda btn: self.show_possible_settings())
        button.bind(on_release=lambda btn: dropdown.select(btn.text))
        button.bind(on_release=lambda btn: self.clear_possible_settings())
        button.bind(on_release=lambda btn: self.initialize_device(pid))
        dropdown.add_widget(button)

    def clear_dropdown(self):
        dropdown = self.root.ids.dropdown
        dropdown.clear_widgets()
        grid_layout = GridLayout()
        dropdown.add_widget(grid_layout)

    def update_device_list(self):
        self.clear_dropdown()
        self.initialize_dropdown()

    def show_fwinfo(self):
        label = self.root.ids.fwinfo_label
        returned = self.device.fwinfo()
        if returned:
            flash_area_id, image_len, ver_major, ver_minor, ver_rev, ver_build_nr = returned
            label.text = ('Firmware info\n'
                          ' FLASH area id: {}\n'
                          ' Image length: {}\n'
                          ' Version: {}.{}.{}.{}').format(flash_area_id,
                                                          image_len,
                                                          ver_major,
                                                          ver_minor,
                                                          ver_rev,
                                                          ver_build_nr)
        else:
            label.text = 'FW info request failed'

    def on_start(self):
        print('Start Hello')


if __name__ == '__main__':
    Gui().run()
