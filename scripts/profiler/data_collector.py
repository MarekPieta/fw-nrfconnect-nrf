#
# Copyright (c) 2018 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from multiprocessing import Process, Event, active_children
import argparse
import time
import logging
import signal
import sys
from stream import Stream
from rtt2stream import Rtt2Stream
from model_creator import ModelCreator

def rtt2stream(stream, event, event_close, log_lvl_number):
    try:
        rtt2s = Rtt2Stream(stream, event_close, log_lvl=log_lvl_number)
        event.wait()
        rtt2s.read_and_transmit_data()
    except Exception as e:
        print("[ERROR] Unhandled exception in Profiler Rtt to stream module: {}".format(e))

def model_creator(stream, event, event_close, dataset_name, log_lvl_number):
    try:
        mc = ModelCreator(stream,
                          event_close,
                          sending_events=False,
                          event_filename=dataset_name + ".csv",
                          event_types_filename=dataset_name + ".json",
                          log_lvl=log_lvl_number)
        event.set()
        mc.start()
    except Exception as e:
        print("[ERROR] Unhandled exception in Profiler model creator module: {}".format(e))

def signal_handler(sig, frame):
    print("sigint")

def main():
    signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGINT})
    parser = argparse.ArgumentParser(
        description='Collecting data from Nordic profiler for given time and saving to files.')
    parser.add_argument('time', type=int, help='Time of collecting data [s]')
    parser.add_argument('dataset_name', help='Name of dataset')
    parser.add_argument('--log', help='Log level')
    args = parser.parse_args()

    if args.log is not None:
        log_lvl_number = int(getattr(logging, args.log.upper(), None))
    else:
        log_lvl_number = logging.INFO

    # Event is made to ensure that ModelCreator class is initialized before Rtt2Stream starts sending data
    event = Event()
    # Setting these events results in closing corresponding modules.
    event_close_rtt2stream = Event()
    event_close_model_creator = Event()

    streams = Stream.create_stream(2)

    processes = []
    processes.append((Process(target=rtt2stream,
                                args=(streams[0], event, event_close_rtt2stream, log_lvl_number),
                                daemon=True),
                        event_close_rtt2stream))
    processes.append((Process(target=model_creator,
                                args=(streams[1], event, event_close_model_creator,
                                    args.dataset_name, log_lvl_number),
                                daemon=True),
                        event_close_model_creator))

    def close_processes():
        for p, event_close in processes:
            if p.is_alive():
                event_close.set()
                # Ensure that we stop processes in order to prevent profiler data drop.
                p.join()


    for p, _ in processes:
        p.start()

    start_time = time.time()
    is_waiting = True
    while is_waiting:
        for p, _ in processes:
            p.join(timeout=0.5)
            # Terminate other processes if one of the processes is not active.
            if len(processes) > len(active_children()):
                is_waiting = False
                break

            # Terminate every process on timeout.
            if (time.time() - start_time) >= args.time:
                is_waiting = False
                break

    close_processes()

if __name__ == "__main__":
    main()
