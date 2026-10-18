#!/usr/bin/env python3
"""ROM-free regressions for sound queue trace filtering and PC attribution."""
import json
import unittest
from audio_queue_provenance import parse_events, summarize


def event(payload):
    return json.dumps(payload)


class TestSoundQueueProvenance(unittest.TestCase):
    def test_step_join_uses_same_step(self):
        lines = [
            event({"type":"step","step":80,"ip_before":0x1234}),
            event({"type":"memory","step":81,"kind":"write","address":0x504020,
                   "size":4,"bytes":"1f10ae00"}),
            event({"type":"step","step":81,"ip_before":0x438fc}),
        ]
        result = parse_events(lines)
        self.assertEqual((result[0]["step"], result[0]["pc"], result[0]["value"]),
                         (81, 0x438fc, 0x00ae101f))
        self.assertFalse(summarize(result)["gameplay_audio_proven"])

    def test_unknown_pc_is_not_invented(self):
        result = parse_events([event({"type":"memory","step":2,"kind":"write",
                                      "address":0x01c80000,"size":1,"bytes":"ae"})])
        self.assertIsNone(result[0]["pc"])
        self.assertEqual(result[0]["kind"], "uart_byte")

    def test_filter_reads_and_unrelated(self):
        lines = [
            event({"type":"memory","step":2,"kind":"read","address":0x504021,
                   "size":1,"bytes":"01"}),
            event({"type":"memory","step":2,"kind":"write","address":0x500000,
                   "size":1,"bytes":"ff"}),
            event({"type":"memory","step":2,"kind":"write","address":0x504021,
                   "size":1,"bytes":"02"}),
        ]
        self.assertEqual([x["kind"] for x in parse_events(lines)], ["queue_entry"])

    def test_fighter_base_must_be_explicit(self):
        record = event({"type":"memory","step":9,"kind":"write",
                        "address":0x510980+0x624,"size":2,"bytes":"0100"})
        self.assertEqual(parse_events([record]), [])
        self.assertEqual(parse_events([record], [0x510980])[0]["kind"],
                         "fighter_field_624")

    def test_invalid_records_fail_closed(self):
        lines = [
            "junk", "{",
            event({"type":"memory","step":2,"kind":"write","address":0x504020,
                   "size":4,"bytes":"abc"}),
            event({"type":"memory","step":2,"kind":"write","address":0x504020,
                   "size":4,"bytes":"zzzzzzzz"}),
        ]
        self.assertEqual(parse_events(lines), [])

    def test_queue_counter_and_uart_distinct(self):
        lines = [
            event({"type":"memory","step":4,"kind":"write","address":0x504001,
                   "size":1,"bytes":"01"}),
            event({"type":"memory","step":5,"kind":"write","address":0x01c80000,
                   "size":1,"bytes":"9e"}),
        ]
        self.assertEqual(summarize(parse_events(lines))["counts"],
                         {"queue_count": 1, "uart_byte": 1})


if __name__ == "__main__":
    unittest.main()
