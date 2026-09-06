#!/usr/bin/env python3
"""Decode a Faba diagnostic capture. Times are MCU observations, not RF timings."""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct

HEADER = struct.Struct("<8sHHIIIIBBBB H2s10s2s24s")
EVENT = struct.Struct("<IIHBB12s")
KINDS = {1: "FIELD_ON", 2: "FIELD_OFF", 3: "RX", 4: "TX_AUTO_CRC",
         5: "TX_BYTES", 6: "TX_BITS", 7: "TX_DONE", 8: "ERROR", 9: "RX_RESULT", 10: "RX_ERROR",
         11: "TX_START", 12: "TIMING_CONFIG"}
STATES = {0: "IDLE", 1: "READY", 2: "ACTIVE", 3: "HALTED"}


def unwrap_rx(bits, data):
    """faba1 logs raw LSB-first DMA bits, including every ninth parity bit."""
    available = min(bits, len(data) * 8)
    if bits < 9:
        return data[:1], None
    stream = [(data[i // 8] >> (i % 8)) & 1 for i in range(available)]
    decoded = bytearray()
    parity_ok = True
    for start in range(0, available, 9):
        group = stream[start:start + 9]
        if len(group) < 8:
            break
        decoded.append(sum(bit << i for i, bit in enumerate(group[:8])))
        if len(group) == 9:
            parity_ok &= sum(group) % 2 == 1
    return bytes(decoded), parity_ok


def command(bits, data):
    if not data:
        return ""
    if bits == 7:
        return {0x26: "REQA", 0x52: "WUPA"}.get(data[0], "7-bit request")
    if len(data) >= 2 and data[0] in (0x93, 0x95, 0x97):
        return f"{'SELECT' if data[1] == 0x70 else 'ANTICOLLISION'} CL{(data[0] - 0x93) // 2 + 1}"
    return {0x30: "READ", 0x3A: "FAST_READ", 0x60: "GET_VERSION", 0x50: "HALT",
            0x39: "READ_CNT", 0x3C: "READ_SIG", 0x1B: "PWD_AUTH", 0xA2: "WRITE",
            0xA0: "COMPAT_WRITE"}.get(data[0], f"command 0x{data[0]:02x}")


def decode(raw):
    if len(raw) < HEADER.size:
        raise ValueError("Truncated trace header")
    (magic, version, event_size, count, dropped, cycle_hz, rtc_hz, reader, slot,
     uid_size, sak, tag_type, atqa, uid, reserved, firmware) = HEADER.unpack_from(raw)
    if magic != b"FABATRC1" or version != 1 or event_size != EVENT.size:
        raise ValueError("Unsupported trace format")
    if count > 128 or uid_size > 10 or reader > 1 or not cycle_hz or not rtc_hz:
        raise ValueError("Invalid trace header")
    if len(raw) != HEADER.size + count * EVENT.size:
        raise ValueError("Truncated trace or unexpected trailing bytes")
    result = {"reader": ["Faba", "phone"][reader], "slot": slot + 1,
              "firmware": firmware.split(b"\0", 1)[0].decode("ascii", errors="replace"),
              "uid": uid[:uid_size].hex(":"), "tag_type": tag_type,
              "atqa_storage": atqa.hex(), "sak": sak, "dropped": dropped,
              "hardware_timer_hz": 16000000 if reserved[0] == 1 else None,
              "retention": "first32+last96" if reserved[1] == 1 else "first128",
              "timing_note": "MCU interrupt observations; capture adds overhead. RTC intervals wrap after 512 s at 32768 Hz.",
              "events": []}
    previous = None
    elapsed = 0.0
    for i in range(count):
        cycles, rtc, bits, kind, state, payload = EVENT.unpack_from(raw, HEADER.size + i * EVENT.size)
        if reserved[1] == 1 and dropped and i == 32:
            result["omitted_middle_events"] = dropped
        if kind not in KINDS:
            raise ValueError(f"Unknown event kind {kind}")
        if previous:
            delta_cycles = (cycles - previous[0]) & 0xFFFFFFFF
            rtc_seconds = ((rtc - previous[1]) & 0xFFFFFF) / rtc_hz
            # DWT stops while the CPU sleeps; RTC remains useful across gaps.
            elapsed += rtc_seconds
            cycle_us = delta_cycles * 1e6 / cycle_hz
        else:
            cycle_us = 0.0
        previous = (cycles, rtc)
        data = payload[:min((bits + 7) // 8, len(payload))]
        event = {"index": i, "rtc_elapsed_ms": round(elapsed * 1000, 4),
                 "cpu_delta_us_modulo": round(cycle_us, 4), "kind": KINDS[kind],
                 "state": STATES.get(state, str(state)), "bits": bits,
                 "data": data.hex(" "), "truncated": bits > len(payload) * 8}
        if kind == 3:
            # Firmware rejects zero lengths and lengths above its 257-byte buffer.
            event["valid_rx_length"] = 0 < bits <= 257 * 8
            if event["valid_rx_length"]:
                decoded, parity_ok = unwrap_rx(bits, data)
                event["decoded_data"] = decoded.hex(" ")
                event["captured_parity_ok"] = parity_ok
                event["command"] = command(bits - bits // 9, decoded)
            else:
                event["command"] = "INVALID_RX_LENGTH"
        if kind == 9 and data:
            event["response_scheduled"] = bool(data[0])
        if kind == 8 and len(data) == 4:
            event["error_reason"] = int.from_bytes(data, "little")
        if kind == 10 and len(data) == 4:
            event["rx_status"] = int.from_bytes(data, "little")
        if result["hardware_timer_hz"] and kind in (7, 11, 12) and bits == 96:
            a, b, c = struct.unpack("<III", data)
            if kind == 12:
                event["frame_delay_min_carrier_cycles"] = a
                event["frame_delay_max_carrier_cycles"] = b
                event["frame_delay_mode"] = {0: "FreeRun", 1: "Window", 2: "ExactVal", 3: "WindowGrid"}.get(c, str(c))
            else:
                event["rx_end_to_tx_start_us"] = ((b - a) & 0xffffffff) / 16
                if kind == 11:
                    event["tx_start_irq_delay_us"] = ((c - b) & 0xffffffff) / 16
                else:
                    event["tx_duration_us"] = ((c - b) & 0xffffffff) / 16
        result["events"].append(event)
    result["counts"] = dict(Counter(e["kind"] for e in result["events"]))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    try:
        result = decode(args.trace.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    if args.json:
        print(json.dumps(result, indent=2))
        return
    print(f"{result['reader']} / slot {result['slot']:02} / {result['firmware']} / UID {result['uid']}")
    print(f"Counts: {result['counts']}; dropped: {result['dropped']}")
    print(result["timing_note"])
    print("Retention:", result["retention"])
    for event in result["events"]:
        if event["index"] == 32 and result.get("omitted_middle_events"):
            print(f"... {result['omitted_middle_events']} middle events omitted ...")
        extra = event.get("command", "")
        if "decoded_data" in event:
            extra += f" decoded=[{event['decoded_data']}]"
        if "response_scheduled" in event:
            extra = f"response scheduled={event['response_scheduled']}"
        if "error_reason" in event:
            extra = "frame-delay timeout" if event["error_reason"] == 0 else f"reason={event['error_reason']}"
        if "rx_status" in event:
            extra = f"rx_status=0x{event['rx_status']:08x}"
        if "rx_end_to_tx_start_us" in event:
            extra = f"HW RXend→TXstart={event['rx_end_to_tx_start_us']:.4f}us"
            if "tx_start_irq_delay_us" in event:
                extra += f" IRQ-delay={event['tx_start_irq_delay_us']:.4f}us"
            if "tx_duration_us" in event:
                extra += f" TX-duration={event['tx_duration_us']:.4f}us"
        if "frame_delay_mode" in event:
            extra = f"{event['frame_delay_mode']} min={event['frame_delay_min_carrier_cycles']} max={event['frame_delay_max_carrier_cycles']} carrier cycles"
        print(f"{event['rtc_elapsed_ms']:10.4f} ms  {event['kind']:12} {event['state']:6} "
              f"{event['bits']:4}b  {event['data']:<35} {extra}")


if __name__ == "__main__":
    main()
