import os
from EventManagerInfo import EventManagerInfo
from collections import namedtuple


class EventManagerRst():
    def __init__(self, em_app_info, doc_dir):
        self.em_app_info = em_app_info
        assert isinstance(em_app_info, EventManagerInfo), \
               'em_app_info must be instance of EventManagerInfo'

        self.out_dir = doc_dir
        if not os.path.exists(self.out_dir):
            os.mkdir(self.out_dir)

    @staticmethod
    def _gen_underline(text_below):
        if len(text_below.replace(' ', '')) != 0:
            sign = '-'
        else:
            sign = ' '

        return sign * (len(text_below) + 2)

    @staticmethod
    def _columns_to_rst_array(columns):
        record_cnt = 0
        for c in columns:
            if len(columns[c]) == 0:
                columns[c].append('')

            record_cnt = len(columns[c])

        assert all(len(columns[col]) == record_cnt for col in columns)

        max_width = {col : max(max(list(map(lambda x: len(x), columns[col]))), len(col))
                     for col in columns}

        for c in columns:
            col = columns[c]
            columns[c] = list(map(lambda x: x + ' ' * (max_width[c] - len(x)),
                                  col))

        lines = []

        col_names = list(map(lambda x: x + ' ' * (max_width[x] - len(x)), columns))
        lines.append('+' + ('+').join(list(map(lambda x: '-' * (len(x) + 2), col_names))) + '+')
        lines.append('| ' + ' | '.join(col_names) + ' |')
        lines.append('+' + ('+').join(list(map(lambda x: '=' * (len(x) + 2), col_names))) + '+')

        values_prev = list(map(lambda x: columns[x][0], columns))
        for i in range(1, record_cnt):
            lines.append('| ' + (' | ').join(values_prev) + ' |')

            values = list(map(lambda x: columns[x][i], columns))

            underlines = list(map(EventManagerRst._gen_underline, values))
            underline = '|' + ('|').join(underlines) + '|'
            underline = underline.replace('-|', '-+').replace('|-', '+-')
            lines.append(underline)

            values_prev = values

        lines.append('| ' + (' | ').join(values_prev) + ' |')
        lines.append('+' + ('+').join(list(map(lambda x: '-' * (len(x) + 2), values_prev))) + '+')

        return '\n'.join(lines)

    @staticmethod
    def _decorate_item_no_ref(item):
        return "``{}``".format(item)

    @staticmethod
    def _decorate_item_ref(prefix, item):
        return ":ref:`{}_{}`".format(prefix, item)

    @staticmethod
    def _gen_event_propagation_columns(listener, in_events, out_events, SUBSCRIPTION_TYPES):
        COLUMN_NAMES = ('Source Module', 'Input Event', 'This Module',
                        'Output Event', 'Sink Module')

        columns = {k : [] for k in COLUMN_NAMES}

        columns['This Module'].append(listener)

        for s_t in SUBSCRIPTION_TYPES:
            for in_evt in in_events[s_t]:
                in_evt_appended = False
                for src in in_events[s_t][in_evt]:
                    columns['Source Module'].append(src)
                    if not in_evt_appended:
                        columns['Input Event'].append(in_evt)
                        in_evt_appended = True
                    else:
                        columns['Input Event'].append('')

        for out_evt in out_events:
            out_evt_appended = False
            for s_t in SUBSCRIPTION_TYPES:
                for dest in out_events[out_evt][s_t]:
                    columns['Sink Module'].append(dest)
                    if not out_evt_appended:
                        columns['Output Event'].append(out_evt)
                        out_evt_appended = True
                    else:
                        columns['Output Event'].append('')
            # Submitting event that has no subscribers
            if not out_evt_appended:
                columns['Sink Module'].append('None')
                columns['Output Event'].append(out_evt)
                out_event_appended = True


        # Add padding to make every column the same length
        req_len = len(columns['Source Module']) + len(columns['Sink Module'])

        assert len(columns['Source Module']) == len(columns['Input Event'])
        keys = ('Source Module', 'Input Event', 'This Module')
        for k in keys:
            columns[k] += [''] * (req_len - len(columns[k]))

        assert len(columns['Output Event']) == len(columns['Sink Module'])
        keys = ('Output Event', 'Sink Module')
        for k in keys:
            columns[k] = [''] * (req_len - len(columns[k])) + columns[k]

        return columns

    @staticmethod
    def _collapse_sources(event_name, sources_list):
        if len(sources_list) < 7:
            return sources_list
        else:
            return ["{}_sources".format(event_name)]

    @staticmethod
    def _collapse_sinks(event_name, sink_dict):
        listified = []
        for k, v in sink_dict.items():
            listified += v

        if len(listified) < 7:
            return sink_dict
        else:
            res = {}
            for k in sink_dict:
                res[k] = []

            res[""].append("{}_sinks".format(event_name))
            return res

    @staticmethod
    def _add_missing_line_rst_array(array):
        last_line_idx = array.rfind('\n| :ref:')
        last_line_idx += 2
        start_idx = array[last_line_idx:].find('\n|')
        if start_idx == -1:
            return array
        else:
            start_idx += last_line_idx

        array = array[:start_idx] + array[start_idx:].replace('|', '+', 2)
        end_idx = array[start_idx:].find('|')
        array = array[:start_idx] + array[start_idx:].replace(' ', '-',
                                                              end_idx - 3)
        array = array[:start_idx] + array[start_idx:].replace('|', '+', 1)

        return array


    @staticmethod
    def _gen_event_propagation_array(em_info, listener):
        SUBSCRIPTION_TYPES = em_info.get_subscription_types()
        PROJECT_NAME = em_info.get_project_name()

        in_events = em_info.get_in_events(listener)
        # Hotfix for case when module has multiple implementations and
        # implementations have various priority level for given event.
        in_events[''] = list(set(in_events['']) - set(in_events['EARLY']))
        in_events[''].sort()

        in_events['FINAL'] = list(set(in_events['FINAL']) - set(in_events['EARLY']))
        in_events['FINAL'] = list(set(in_events['FINAL']) - set(in_events['']))
        in_events['FINAL'].sort()

        for t in in_events:
            in_events[t] = {EventManagerRst._decorate_item_no_ref(ev) :
                            EventManagerRst._collapse_sources(ev, em_info.get_event_sources(ev))
                            for ev in in_events[t]}
            for evt in in_events[t]:
                in_events[t][evt] = [EventManagerRst._decorate_item_ref(PROJECT_NAME, src)
                                     for src in in_events[t][evt]]

        out_events = em_info.get_out_events(listener)

        out_events = {EventManagerRst._decorate_item_no_ref(ev) :
                      EventManagerRst._collapse_sinks(ev, em_info.get_event_subscribers(ev))
                        for ev in out_events}

        for k in out_events:
            for t in out_events[k]:
                out_events[k][t] = [EventManagerRst._decorate_item_ref(PROJECT_NAME, sub)
                                    for sub in out_events[k][t]]

        listener = EventManagerRst._decorate_item_no_ref(listener)
        columns = EventManagerRst._gen_event_propagation_columns(listener, in_events, out_events,
                                                                 SUBSCRIPTION_TYPES)

        array = EventManagerRst._columns_to_rst_array(columns)

        return EventManagerRst._add_missing_line_rst_array(array)

    def save_all_event_propagation_arrays(self):
        evt_prop_out_dir = self.out_dir

        listeners = self.em_app_info.get_all_listeners()
        outfile = open(os.path.join(evt_prop_out_dir, 'event_propagation.rst'), 'w+')
        outfile.write(':orphan:\n\n')
        outfile.write('nRF Desktop event propagation\n')
        outfile.write('#############################\n\n')

        for l in listeners:
            arr = EventManagerRst._gen_event_propagation_array(self.em_app_info, l)

            outfile.write('.. table_{}_start'.format(l))
            outfile.write('\n\n')
            outfile.write(arr)
            outfile.write('\n\n')
            outfile.write('.. table_{}_end'.format(l))
            outfile.write('\n\n\n')

        # Remove trailing empty lines from file
        outfile.seek(outfile.tell() - 2, os.SEEK_SET)
        outfile.truncate()
        outfile.close()

    def save_all_event_related_modules(self, module_limit=None):
        evt_rel_modules_out_dir = self.out_dir

        events = self.em_app_info.get_all_events()
        PROJECT_NAME = self.em_app_info.get_project_name()

        outfile = open(os.path.join(evt_rel_modules_out_dir, 'event_rel_modules.rst'), 'w+')

        outfile.write('.. _nrf_desktop_event_rel_modules:\n\n')
        outfile.write('Source and sink module lists\n')
        outfile.write('############################\n\n')

        outfile.write(".. contents::\n")
        outfile.write("   :local:\n")
        outfile.write("   :depth: 2\n\n")

        outfile.write('This page includes lists of source and sink modules for events that have many listeners or sources.\n')
        outfile.write('These were gathered on a single page to simplify the event propagation tables.\n\n')

        for evt in events:
            sources = self.em_app_info.get_event_sources(evt)
            sinks_dict = self.em_app_info.get_event_subscribers(evt)

            # Hotfix for case when module is twice in the dict (various
            # module implementations have various priority for given event).
            sinks_dict[''] = list(set(sinks_dict['']) - set(sinks_dict['EARLY']))
            sinks_dict[''].sort()
            sinks_dict['FINAL'] = list(set(sinks_dict['FINAL']) - set(sinks_dict['EARLY']))
            sinks_dict['FINAL'] = list(set(sinks_dict['FINAL']) - set(sinks_dict['']))
            sinks_dict['FINAL'].sort()

            sinks = []
            for v in sinks_dict.values():
                sinks.extend(v)

            if module_limit is not None and \
               len(sources) < module_limit and \
               len(sinks) < module_limit:
                continue

            source_mod_header = 'Source modules for {}\n'.format(evt)
            source_mod_header += '=' * (len(source_mod_header) - 1) + '\n\n'

            outfile.write('.. _{}_{}_{}:\n\n'.format(PROJECT_NAME, evt, 'sources'))
            outfile.write(source_mod_header)
            for m in sources:
                outfile.write('* :ref:`{}_{}`\n'.format(PROJECT_NAME, m))

            sink_mod_header = 'Sink modules for {}\n'.format(evt)
            sink_mod_header += '=' * (len(sink_mod_header) - 1) + '\n\n'

            outfile.write('\n')
            outfile.write('.. _{}_{}_{}:\n\n'.format(PROJECT_NAME, evt, 'sinks'))
            outfile.write(sink_mod_header)
            for m in sinks:
                outfile.write('* :ref:`{}_{}`\n'.format(PROJECT_NAME, m))
            outfile.write('\n\n')

        # Remove trailing empty lines from file
        outfile.seek(outfile.tell() - 2, os.SEEK_SET)
        outfile.truncate()
        outfile.close()
