"""ROM-free tests for conservative frame-level VF2 audio evidence."""
import json
import unittest

from audio_frame_evidence_report import audit_observations


def frame(*, index=0, before=0, after=1, uart="ad2342",
          d92=1, nonzero=2):
    return {
        "type": "vf2_audio_frame", "frame": index, "input": 4,
        "origin": "oracle_frame_shadow_uart_pump",
        "queue_before_step": before, "queue_after_step": after,
        "uart_forwarded_hex": uart, "uart_count": len(uart) // 2,
        "uart_truncated": 0, "bytes_moved": len(uart) // 2,
        "entries_filed": 1, "entries_dispatched": 1,
        "entries_refused": 0, "d92_keyons_retired": d92,
        "pump_status": 0, "drain_status": 0,
        "stream_status": 0, "voice_status": 0, "render_status": 0,
        "pcm_frames": 4, "pcm_nonzero_frames": nonzero, "pcm_peak": 1024,
        "natural_fighter_hit_proven": False,
        "original_uart_irq_proven": False, "hardware_pcm_proven": False,
    }


def audit(*records):
    return audit_observations([json.dumps(e) + "\n" for e in records])


class TestFrameEvidence(unittest.TestCase):
    def test_candidate_never_promoted_to_real_hit(self):
        result = audit(frame())
        self.assertEqual(result["total_frames"], 1)
        self.assertEqual(result["uart_bytes_observed"], 3)
        self.assertTrue(result["gates"]["frame_local_queue_to_shadow_uart_candidate"])
        self.assertFalse(result["gates"]["natural_fighter_hit_proven"])
        self.assertFalse(result["gates"]["original_producer_pc_verified"])

    def test_preexisting_queue_does_not_imply_gameplay_writer(self):
        f = frame(before=2, after=2)
        result = audit(f)
        self.assertEqual(result["candidates"], [])
        self.assertEqual(result["counts"]["frames_with_uart"], 1)

    def test_pcm_without_queue_does_not_imply_audio_event(self):
        f = frame(before=0, after=0, uart="", d92=0)
        f["entries_filed"] = f["entries_dispatched"] = 0
        result = audit(f)
        self.assertEqual(result["candidates"], [])
        self.assertEqual(result["counts"]["frames_with_nonzero_pcm"], 1)

    def test_discontinuous_frame_refused(self):
        with self.assertRaisesRegex(ValueError, "discontinuous"):
            audit(frame(index=0), frame(index=2))

    def test_wrong_uart_count_refused(self):
        f = frame()
        f["uart_count"] = 2
        with self.assertRaisesRegex(ValueError, "UART bytes invalid"):
            audit(f)

    def test_incomplete_transport_refused(self):
        f = frame()
        f["bytes_moved"] = 4
        with self.assertRaisesRegex(ValueError, "captured bytes disagree"):
            audit(f)

    def test_dispatch_count_corruption_refused(self):
        f = frame()
        f["d92_keyons_retired"] = 2
        with self.assertRaisesRegex(ValueError, "dispatcher accounting"):
            audit(f)

    def test_false_evidence_claim_refused(self):
        f = frame()
        f["natural_fighter_hit_proven"] = True
        with self.assertRaisesRegex(ValueError, "unsupported truth claim"):
            audit(f)

    def test_invalid_origin_refused(self):
        f = frame()
        f["origin"] = "native_cpu"
        with self.assertRaisesRegex(ValueError, "not an original"):
            audit(f)

    def test_corrupt_pcm_refused(self):
        f = frame()
        f["pcm_nonzero_frames"] = 9
        with self.assertRaisesRegex(ValueError, "impossible PCM"):
            audit(f)

    def test_capped_candidates_keep_true_total(self):
        a = frame()
        b = frame(index=1)
        result = audit_observations([json.dumps(a), json.dumps(b)],
                                    max_candidates=1)
        self.assertEqual(result["counts"]["temporal_queue_growth_uart_candidates"], 2)
        self.assertEqual(len(result["candidates"]), 1)
        self.assertTrue(result["candidates_truncated"])

    def test_blank_records_and_empty_trace(self):
        result = audit_observations(["\n", "  \n"])
        self.assertEqual(result["total_frames"], 0)
        self.assertFalse(result["gates"]["frame_progression_observed"])


if __name__ == "__main__":
    unittest.main()
