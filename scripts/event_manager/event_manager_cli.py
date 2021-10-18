import argparse
from EventManagerInfo import EventManagerInfo
from EventManagerRst import EventManagerRst
import pprint

if __name__ == '__main__':
    NRF_DESKTOP_PATH = "/home/mapi/ncs/nrf/applications/nrf_desktop"
    NRF_DESKTOP_OUT_PATH = "/home/mapi/ncs/nrf/applications/nrf_desktop/doc"
    NRF_DESKTOP_CAF_MODULES_SRC = (
        "ble_adv.c",
        "ble_smp.c",
        "ble_state.c",
        "ble_state_pm.c",
        "buttons.c",
        "click_detector.c",
        "leds.c",
        "power_manager.c",
        "settings_loader.c"
    )

    info = EventManagerInfo(NRF_DESKTOP_PATH, NRF_DESKTOP_CAF_MODULES_SRC)
    pprint.pprint(info._get_modules_info())
    rst = EventManagerRst(info, NRF_DESKTOP_OUT_PATH)

    rst.save_all_event_propagation_arrays()
    rst.save_all_event_related_modules(7)
