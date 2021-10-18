import re
import os
import copy
from collections import OrderedDict


class EventManagerInfo():
    def __init__(self, root_path, used_caf_module_src):
        self.SUBSCRIPTION_TYPES = ("EARLY", "", "FINAL")

        self.root_path = os.path.abspath(root_path)
        assert os.path.isdir(self.root_path)
        # Project name is base directory name
        self.PROJECT_NAME = os.path.basename(self.root_path)

        self.src_dir_path = os.path.join(root_path, "src")
        assert os.path.isdir(self.src_dir_path)

        # Find all relevant application-specific sources
        c_src_app = EventManagerInfo._parse_src_code(self.src_dir_path, postfix=".c")
        h_src_app = EventManagerInfo._parse_src_code(self.src_dir_path, postfix=".h")

        c_src_caf, h_src_caf = EventManagerInfo._get_caf_files(used_caf_module_src)

        c_src = dict(c_src_app)
        c_src.update(c_src_caf)

        h_src = dict(h_src_app)
        h_src.update(h_src_caf)

        evt_submit_c = EventManagerInfo._parse_external_event_submits(c_src.values())
        evt_submit_h = EventManagerInfo._parse_external_event_submits(h_src.values())

        assert len(evt_submit_c.keys() & evt_submit_h.keys()) == 0,\
            'Function or macro with given name can be defined only once in the application.'

        self.evt_submit_calls = {**evt_submit_c, **evt_submit_h}
        self.modules_info = EventManagerInfo._parse_listeners(c_src,
                                                          self.SUBSCRIPTION_TYPES,
                                                          self.evt_submit_calls)

    @staticmethod
    #TODO Improve to automatically parse CMakeLists and Kconfig files ?
    def _filter_caf_files(c_src_caf_all, h_src_caf_all, used_caf_module_src):
        for m in used_caf_module_src:
            assert m in c_src_caf_all.keys()

        # Inclue all CAF events and selected CAF modules
        c_src_caf = {k: v for k, v in c_src_caf_all.items() if
                          (("event" in k) or (k in used_caf_module_src))}
        # Include all CAF headers
        h_src_caf = dict(h_src_caf_all)

        return c_src_caf, h_src_caf

    @staticmethod
    def _get_caf_files(used_caf_module_src):
        # Find CAF sources
        ZEPHYR_DIR = os.getenv("ZEPHYR_BASE")
        assert ZEPHYR_DIR is not None
        assert os.path.exists(ZEPHYR_DIR)

        NCS_DIR = os.path.dirname(ZEPHYR_DIR)
        NRF_DIR = os.path.join(NCS_DIR, "nrf")
        assert os.path.exists(NRF_DIR)

        CAF_SUBSYS_DIR = os.path.join(NRF_DIR, "subsys")
        CAF_SUBSYS_DIR = os.path.join(CAF_SUBSYS_DIR, "caf")
        CAF_INCLUDE_DIR = os.path.join(NRF_DIR, "include")
        CAF_INCLUDE_DIR = os.path.join(CAF_INCLUDE_DIR, "caf")
        assert os.path.exists(CAF_SUBSYS_DIR)
        assert os.path.exists(CAF_INCLUDE_DIR)

        c_src_caf_all = EventManagerInfo._parse_src_code(CAF_SUBSYS_DIR, postfix=".c")
        assert len(EventManagerInfo._parse_src_code(CAF_SUBSYS_DIR, postfix=".h")) == 0
        h_src_caf_all = EventManagerInfo._parse_src_code(CAF_INCLUDE_DIR, postfix=".h")
        assert len(EventManagerInfo._parse_src_code(CAF_INCLUDE_DIR, postfix=".c")) == 0

        return EventManagerInfo._filter_caf_files(c_src_caf_all, h_src_caf_all, used_caf_module_src)

    @staticmethod
    def _parse_src_code(root_path, postfix = None, prefix = None):
        code_files = {}

        for root, dirs, files in os.walk(root_path):
            for f in files:
                if (prefix is not None) and (not f.startswith(prefix)):
                        pass
                elif (postfix is not None) and (not f.endswith(postfix)):
                        pass
                else:
                     filepath = os.path.join(root, f)
                     with open(filepath) as found_file:
                         code_files[f] = found_file.read()

        return code_files

    @staticmethod
    def _is_event_listener(mod_src_file):
        return 'EVENT_LISTENER' in mod_src_file

    @staticmethod
    def _parse_module_name(mod_src_file):
        pattern = re.findall('EVENT_LISTENER\(.+?,.+?\)', mod_src_file)
        assert len(pattern) == 1, \
            'Only one listener should be defined in single source file'

        listener_name = pattern[0].split("(")[1].split(',')[0]

        # TODO: Check properly if definition is before creating listener
        pattern = re.findall('#define {} .+'.format(listener_name),
                             mod_src_file)

        if len(pattern) > 0:
            assert len(pattern) == 1
            listener_name = pattern[0].split(' ')[-1]

        return listener_name

    @staticmethod
    def _parse_in_events(mod_src_file, event_order):
        res = {}
        base = 'EVENT_SUBSCRIBE'

        for o in event_order:
            if o == '':
                separator = ''
            else:
                separator = '_'

            patterns = re.findall(base + separator + o + '\(.+\)', mod_src_file)
            res[o] = set(map(lambda x: x.split(',')[-1].split(')')[0].replace(' ', ''),
                         patterns))

        return res

    @staticmethod
    def _is_function_called(mod_src_file, f_name):
        patterns = re.findall('{}\(.*?\)'.format(f_name), mod_src_file, flags=re.DOTALL)
        return len(patterns) > 0

    @staticmethod
    def _parse_out_events(mod_src_file, evt_submit_calls):
        # Directly submitted events
        patterns = re.findall('new_[a-zA-Z_]+_event', mod_src_file)
        directly = set(map(lambda x: x[x.find('_') + 1:], patterns))

        non_directly = set()
        for f in evt_submit_calls:
            if EventManagerInfo._is_function_called(mod_src_file, f):
                non_directly.update(evt_submit_calls[f])

        directly.update(non_directly)
        return directly

    @staticmethod
    def _parse_def_files(mod_src_file):
        patterns = re.findall('#include .+_def.h', mod_src_file)

        return set(map(lambda x: x.split('"')[-1], patterns))

    @staticmethod
    def _parse_listener_info(src_file, SUBSCRIPTION_TYPES, evt_submit_calls):
        if not EventManagerInfo._is_event_listener(src_file):
            return None, None

        info = {}
        mod_name = EventManagerInfo._parse_module_name(src_file)
        info['in_events'] = EventManagerInfo._parse_in_events(src_file, SUBSCRIPTION_TYPES)
        info['out_events'] = EventManagerInfo._parse_out_events(src_file, evt_submit_calls)
        info['def_files'] = EventManagerInfo._parse_def_files(src_file)

        return mod_name, info

    @staticmethod
    def _parse_listeners(c_src, SUBSCRIPTION_TYPES, evt_submit_calls):
        listeners = {}
        for f in c_src:
            mod_name, info = EventManagerInfo._parse_listener_info(c_src[f], SUBSCRIPTION_TYPES, evt_submit_calls)
            if mod_name is not None:
                if mod_name not in listeners:
                    listeners[mod_name] = info
                else:
                    listeners[mod_name]['def_files'] |= info['def_files']
                    listeners[mod_name]['out_events'] |= info['out_events']
                    for o in SUBSCRIPTION_TYPES:
                        listeners[mod_name]['in_events'][o] |= info['in_events'][o]

        return listeners

    @staticmethod
    def _parse_event_submitting_functions(src_file):
        functions = re.findall('[a-zA-Z_]+?\([^;]+?\)\n{.+?\n}', src_file,
                               flags=re.DOTALL)

        f_names = list(map(lambda x: x.split('(')[0], functions))
        new_events = list(map(lambda x: re.findall('new_[a-zA-Z_]+_event', x),
                              functions))

        for i in range(0, len(new_events)):
            new_events[i] = list(map(lambda x: x.replace('new_', ''), new_events[i]))

        res = dict(zip(f_names, new_events))
        res = dict(filter(lambda x: len(x[1]) > 0, res.items()))

        return res

    @staticmethod
    def _parse_event_submitting_macros(src_file):
        macros = re.findall('#define [a-zA-Z_]+?.+?[^\\\\]\n', src_file,
                            flags=re.DOTALL)

        m_names = list(map(lambda x: x.replace('#define ', '').split('(')[0], macros))
        new_events = list(map(lambda x: re.findall('new_[a-zA-Z_]+_event', x),
                              macros))

        for i in range(0, len(new_events)):
            new_events[i] = list(map(lambda x: x.replace('new_', ''), new_events[i]))

        res = dict(zip(m_names, new_events))
        res = dict(filter(lambda x: len(x[1]) > 0, res.items()))

        return res

    @staticmethod
    def _parse_submitted_events(src_file):
        functions = EventManagerInfo._parse_event_submitting_functions(src_file)
        macros = EventManagerInfo._parse_event_submitting_macros(src_file)

        assert len(functions.keys() & macros.keys()) == 0, \
            'Function or macro with given name can be defined only once in the application.'
        return {**functions, **macros}

    @staticmethod
    def _parse_external_event_submits(c_src):
        res = {}
        for f in c_src:
            if EventManagerInfo._is_event_listener(f):
                continue

            new_dict = EventManagerInfo._parse_submitted_events(f)
            assert len(res.keys() & new_dict.keys()) == 0, \
                'Function or macro with given name can be defined only once in the application.'
            res.update(new_dict)

        return res

    # This function returns internal data representation.
    # The data representation can change at any time.
    def _get_modules_info(self):
        return copy.deepcopy(self.modules_info)

    def get_project_name(self):
        return self.PROJECT_NAME

    def get_all_listeners(self):
        res = list(self.modules_info.keys())
        res.sort()
        return res

    def get_all_events(self):
        all_events = set()
        for m in self.modules_info:
            for s_t in self.SUBSCRIPTION_TYPES:
                all_events = all_events.union(self.modules_info[m]['in_events'][s_t])

            all_events = all_events.union(self.modules_info[m]['out_events'])

        res = list(all_events)
        res.sort()
        return res

    def get_in_events(self, module):
        res = copy.deepcopy(self.modules_info[module]['in_events'])
        for k in res:
            res[k] = list(res[k])
            res[k].sort()

        return res

    def get_out_events(self, module):
        res = copy.deepcopy(self.modules_info[module]['out_events'])
        res = list(res)
        res.sort()
        return res

    def get_subscription_types(self):
        return self.SUBSCRIPTION_TYPES

    def get_event_subscribers(self, event):
        res = {}

        for t in self.SUBSCRIPTION_TYPES:
            res[t] = list(filter(lambda x: event in self.modules_info[x]['in_events'][t],
                          self.modules_info))
            res[t].sort()

        return res

    def get_event_sources(self, event):
        res = list(filter(lambda x: event in self.modules_info[x]['out_events'],
                          self.modules_info))
        res.sort()
        return res
