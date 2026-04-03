#!/usr/bin/env python3
"""Build binary fixture files from YAML test matrix and test vectors."""

from __future__ import annotations

import argparse
import os
import struct
from typing import Any, Iterable

import yaml

MATRIX_MAGIC = b"RLNMTX01"
VECTOR_MAGIC = b"RLNVEC01"

MAX_CASES = 64
MAX_STATUSES = 8
MAX_INPUTS = 8
MAX_EFFECTS = 8
MAX_TEXT = 192
MAX_ID = 24
MAX_INVARIANT = 40
MAX_FRAME = 512

STATUS_MAP = {
    "ACCEPT": 1,
    "DISCARD": 2,
    "ERR_INVALID_PACKET": 3,
    "ERR_PAYLOAD_TOO_LARGE": 4,
    "ERR_INVALID_CHANNEL": 5,
    "ERR_INVALID_FLAGS": 6,
    "ERR_INVALID_ACK": 7,
}


def to_int(value: Any) -> int:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise ValueError(f"Cannot convert to int: {value!r}")


def as_list(value: Any) -> list[Any]:
    if value is None:
        return []
    if isinstance(value, list):
        return value
    return [value]


def write_padded_text(fp, text: str, size: int) -> None:
    encoded = (text or "").encode("utf-8")
    if len(encoded) >= size:
        encoded = encoded[: size - 1]
    fp.write(encoded)
    fp.write(b"\x00" * (size - len(encoded)))


def write_u16_le(fp, value: int) -> None:
    fp.write(struct.pack("<H", value & 0xFFFF))


def normalize_statuses(raw_values: Iterable[Any]) -> list[int]:
    statuses: list[int] = []
    for raw in raw_values:
        if isinstance(raw, int):
            statuses.append(raw)
            continue
        key = str(raw).strip()
        if key not in STATUS_MAP:
            raise ValueError(f"Unknown expected status: {raw!r}")
        statuses.append(STATUS_MAP[key])
    return statuses


def write_matrix_case(fp, case: dict[str, Any], section: int) -> None:
    write_padded_text(fp, str(case.get("id", "")), MAX_ID)
    write_padded_text(fp, str(case.get("invariant", "")), MAX_INVARIANT)
    write_padded_text(fp, str(case.get("description", "")), MAX_TEXT)
    write_padded_text(fp, str(case.get("setup", "")), MAX_TEXT)

    fp.write(bytes([section]))

    statuses = normalize_statuses(as_list(case.get("expected_status")))
    status_count = min(len(statuses), MAX_STATUSES)
    fp.write(bytes([status_count]))
    status_slots = [0] * MAX_STATUSES
    for i in range(status_count):
        status_slots[i] = statuses[i] & 0xFF
    fp.write(bytes(status_slots))

    inputs = [str(x) for x in as_list(case.get("input"))]
    input_count = min(len(inputs), MAX_INPUTS)
    fp.write(bytes([input_count]))
    for i in range(MAX_INPUTS):
        item = inputs[i] if i < input_count else ""
        write_padded_text(fp, item, MAX_TEXT)

    effects = [str(x) for x in as_list(case.get("expected_side_effects"))]
    effect_count = min(len(effects), MAX_EFFECTS)
    fp.write(bytes([effect_count]))
    for i in range(MAX_EFFECTS):
        item = effects[i] if i < effect_count else ""
        write_padded_text(fp, item, MAX_TEXT)


def build_matrix_fixture(matrix_yaml: str, out_path: str) -> int:
    with open(matrix_yaml, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}

    tests = data.get("tests", {})
    core_cases = as_list(tests.get("core"))
    recommended_cases = as_list(tests.get("recommended"))

    all_cases: list[tuple[dict[str, Any], int]] = []
    all_cases.extend((c, 1) for c in core_cases if isinstance(c, dict))
    all_cases.extend((c, 2) for c in recommended_cases if isinstance(c, dict))

    if len(all_cases) > MAX_CASES:
        raise ValueError(f"Too many matrix cases ({len(all_cases)}), max is {MAX_CASES}")

    with open(out_path, "wb") as fp:
        fp.write(MATRIX_MAGIC)
        write_u16_le(fp, len(all_cases))
        for case, section in all_cases:
            write_matrix_case(fp, case, section)

    return len(all_cases)


def build_vector_frame(case: dict[str, Any]) -> bytes:
    version = 0x01
    mode = to_int(case.get("mode", 0)) & 0x0F
    ptype = to_int(case.get("type", 0)) & 0x0F
    flags = to_int(case.get("flags", 0)) & 0x0F
    seq_id = to_int(case.get("seq_id", 0)) & 0xFFFF
    ack_id = to_int(case.get("ack_id", 0)) & 0xFFFF
    channel_id = to_int(case.get("channel_id", 0)) & 0xFF

    payload_hex = str(case.get("payload_hex", "")).strip()
    payload = bytes.fromhex(payload_hex) if payload_hex else b""
    payload_len = len(payload)

    header = bytearray(9)
    header[0] = ((version & 0x0F) << 4) | mode
    header[1] = ((ptype & 0x0F) << 4) | flags
    header[2] = (seq_id >> 8) & 0xFF
    header[3] = seq_id & 0xFF
    header[4] = (ack_id >> 8) & 0xFF
    header[5] = ack_id & 0xFF
    header[6] = channel_id
    header[7] = (payload_len >> 8) & 0xFF
    header[8] = payload_len & 0xFF

    return bytes(header) + payload


def write_vector_case(fp, case: dict[str, Any]) -> None:
    frame = build_vector_frame(case)
    frame_len = len(frame)
    if frame_len > MAX_FRAME:
        raise ValueError(f"Frame too large ({frame_len}), max is {MAX_FRAME}")

    write_padded_text(fp, str(case.get("id", "")), MAX_ID)
    fp.write(bytes([1 if bool(case.get("expect_valid", False)) else 0]))

    fp.write(bytes([to_int(case.get("mode", 0)) & 0xFF]))
    fp.write(bytes([to_int(case.get("type", 0)) & 0xFF]))
    fp.write(bytes([to_int(case.get("flags", 0)) & 0xFF]))

    write_u16_le(fp, to_int(case.get("seq_id", 0)))
    write_u16_le(fp, to_int(case.get("ack_id", 0)))
    fp.write(bytes([to_int(case.get("channel_id", 0)) & 0xFF]))

    payload_len = max(0, frame_len - 9)
    write_u16_le(fp, payload_len)
    write_u16_le(fp, frame_len)

    fp.write(frame)
    fp.write(b"\x00" * (MAX_FRAME - frame_len))


def build_vector_fixture(vectors_yaml: str, out_path: str) -> int:
    with open(vectors_yaml, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}

    vectors = [v for v in as_list(data.get("vectors")) if isinstance(v, dict)]
    if len(vectors) > MAX_CASES:
        raise ValueError(f"Too many vector cases ({len(vectors)}), max is {MAX_CASES}")

    with open(out_path, "wb") as fp:
        fp.write(VECTOR_MAGIC)
        write_u16_le(fp, len(vectors))
        for case in vectors:
            write_vector_case(fp, case)

    return len(vectors)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build ReliNow binary fixtures from YAML")
    parser.add_argument("--matrix-yaml", required=True, help="Path to TEST_MATRIX.yaml")
    parser.add_argument("--vector-yaml", required=True, help="Path to TEST_VECTORS.yaml")
    parser.add_argument("--out-dir", required=True, help="Output directory for .bin fixtures")
    args = parser.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)

    matrix_out = os.path.join(args.out_dir, "matrix_fixture.bin")
    vector_out = os.path.join(args.out_dir, "vector_fixture.bin")

    matrix_count = build_matrix_fixture(args.matrix_yaml, matrix_out)
    vector_count = build_vector_fixture(args.vector_yaml, vector_out)

    print(f"Wrote {matrix_out} with {matrix_count} cases")
    print(f"Wrote {vector_out} with {vector_count} cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
