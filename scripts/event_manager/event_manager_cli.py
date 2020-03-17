import argparse
from EventManagerInfo import EventManagerInfo
from EventManagerRst import EventManagerRst
import pprint

if __name__ == '__main__':
# parser should only take the input path and output path
    info = EventManagerInfo("/home/mapi/ncs/nrf/applications/nrf_desktop")
    pprint.pprint(info._get_modules_info())
    rst = EventManagerRst(info, "/home/mapi/ncs/nrf/applications/nrf_desktop/doc/")

    rst.save_all_event_propagation_arrays()
    rst.save_all_event_related_modules(7)
