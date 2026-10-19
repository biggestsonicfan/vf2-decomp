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

    def test_verified_90_keyon_is_distinct_not_fighter_hit(self):
        record = frame(d92=0, uart="9e0a7f", nonzero=2)
        record["keyon_90_retired"] = 1
        result = audit(record)
        self.assertEqual(
            result["counts"]["frames_with_verified_90_keyon"], 1)
        self.assertEqual(result["counts"]["frames_with_d92_keyon"], 0)
        self.assertFalse(result["gates"]["natural_fighter_hit_proven"])
        record["d92_keyons_retired"] = 1
        with self.assertRaisesRegex(ValueError, "dispatcher accounting"):
            audit(record)

    def test_pcm_without_queue_does_not_imply_audio_event(self):
        f = frame(before=0, after=0, uart="", d92=0)
        f["entries_filed"] = f["entries_dispatched"] = 0
        result = audit(f)
        self.assertEqual(result["candidates"], [])
        self.assertEqual(result["counts"]["frames_with_nonzero_pcm"], 1)

    def test_original_i960_ring_writer_is_only_bus_proof(self):
        record = frame()
        record["original_i960_ring_writes"] = 1
        record["original_i960_first_ring_slot"] = 0x00504020
        record["original_i960_first_ring_word"] = 0x00AD2342
        record["original_i960_writer_pc_proven"] = False
        result = audit(record)
        self.assertEqual(result["counts"]["frames_with_original_i960_ring_writer"], 1)
        self.assertTrue(result["gates"]["original_i960_ring_write_observed"])
        self.assertFalse(result["gates"]["original_producer_pc_verified"])
        self.assertFalse(result["gates"]["natural_fighter_hit_proven"])
        self.assertEqual(result["original_i960_ring_writers"][0]["first_command"],
                         "0x00ad2342")

    def test_true_original_writer_pc_no_hit_proof(self):
        record = frame()
        record.update({
            "original_i960_ring_writes": 1,
            "original_i960_first_ring_slot": 0x00504020,
            "original_i960_first_ring_word": 0x00AD2342,
            "original_i960_first_writer_pc": 0x00043924,
            "original_i960_writer_pc_proven": True,
        })
        result = audit(record)
        self.assertTrue(result["gates"]["original_producer_pc_verified"])
        self.assertEqual(result["counts"]["frames_with_original_i960_writer_pc"], 1)
        self.assertEqual(
            result["original_i960_ring_writers"][0]["original_instruction_ip"],
            "0x00043924"
        )
        self.assertFalse(result["gates"]["natural_fighter_hit_proven"])
        self.assertFalse(result["gates"]["original_isr_to_m68k_link_verified"])

    def test_orphan_writer_pc_claim_rejected(self):
        record = frame()
        record["original_i960_first_writer_pc"] = 0x00043924
        record["original_i960_writer_pc_proven"] = True
        with self.assertRaisesRegex(ValueError, "without writes"):
            audit(record)

    def test_later_pc_target_after_startup_zeroing(self):
        record = frame(index=0, before=0, after=1, uart="ae101f")
        record.update({
            "original_i960_ring_writes": 17,
            "original_i960_first_ring_slot": 0x00504020,
            "original_i960_first_ring_word": 0,
            "original_i960_first_writer_pc": 0x100,
            "original_i960_writer_pc_proven": True,
            "original_i960_target_writer_observed": True,
            "original_i960_target_writer_pc": 0x43924,
            "original_i960_target_slot": 0x00504020,
            "original_i960_target_word": 0x00AE101F,
        })
        result = audit(record)
        self.assertEqual(result["counts"]["frames_with_selected_i960_writer"], 1)
        self.assertEqual(result["original_i960_selected_writers"][0]["pc"],
                         "0x00043924")
        self.assertEqual(result["original_i960_selected_writers"][0]["word"],
                         "0x00ae101f")
        self.assertFalse(result["gates"]["natural_fighter_hit_proven"])

    def test_invalid_selected_producer_rejected(self):
        record = frame()
        record.update({
            "original_i960_ring_writes": 1,
            "original_i960_first_ring_slot": 0x00504020,
            "original_i960_first_ring_word": 0,
            "original_i960_writer_pc_proven": False,
            "original_i960_target_writer_observed": True,
            "original_i960_target_writer_pc": 0x43924,
            "original_i960_target_slot": 0x00504021,
            "original_i960_target_word": 0xAE101F,
        })
        with self.assertRaisesRegex(ValueError, "invalid selected"):
            audit(record)
        record["original_i960_target_slot"] = 0x00504020
        record["original_i960_target_writer_observed"] = False
        with self.assertRaisesRegex(ValueError, "unobserved target"):
            audit(record)

    def test_forged_or_misaligned_writer_pc_refused(self):
        record = frame()
        record.update({
            "original_i960_ring_writes": 1,
            "original_i960_first_ring_slot": 0x00504020,
            "original_i960_first_ring_word": 0x00AD2342,
            "original_i960_first_writer_pc": 0x00043925,
            "original_i960_writer_pc_proven": True,
        })
        with self.assertRaisesRegex(ValueError, "invalid original"):
            audit(record)
        record["original_i960_first_writer_pc"] = 0x00043924
        record["original_i960_writer_pc_proven"] = False
        with self.assertRaisesRegex(ValueError, "invalid original"):
            audit(record)

    def test_true_writer_pc_requires_real_write(self):
        record = frame()
        record.update({
            "original_i960_ring_writes": 0,
            "original_i960_first_ring_slot": 0,
            "original_i960_first_ring_word": 0,
            "original_i960_first_writer_pc": 0x00043924,
            "original_i960_writer_pc_proven": True,
        })
        with self.assertRaisesRegex(ValueError, "invalid original"):
            audit(record)

    def test_wrong_i960_slot_and_claim_refused(self):
        record = frame()
        record.update({
            "original_i960_ring_writes": 1,
            "original_i960_first_ring_slot": 0x00504021,
            "original_i960_first_ring_word": 0x00AD2342,
            "original_i960_writer_pc_proven": False,
        })
        with self.assertRaisesRegex(ValueError, "invalid original"):
            audit(record)
        record["original_i960_first_ring_slot"] = 0x00504020
        record["original_i960_writer_pc_proven"] = True
        with self.assertRaisesRegex(ValueError, "invalid original"):
            audit(record)

    def test_zero_i960_writes_require_zero_payload(self):
        record = frame()
        record.update({
            "original_i960_ring_writes": 0,
            "original_i960_first_ring_slot": 0x00504020,
            "original_i960_first_ring_word": 0,
            "original_i960_writer_pc_proven": False,
        })
        with self.assertRaisesRegex(ValueError, "invalid original"):
            audit(record)

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
