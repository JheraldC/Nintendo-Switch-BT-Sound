"""Strict local-only pairing formats. Never print addresses or link keys."""
import configparser
import hashlib
import re
import struct

MAX_RECORDS = 24
RECORD_BYTES = 512


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).digest()


def validate_records(records):
    require(len(records) <= MAX_RECORDS, "record capacity exceeded")
    seen = set()
    for record in records:
        require(len(record) == RECORD_BYTES, "record size invalid")
        address = bytes(record[:6])
        require(address not in (bytes(6), b"\xff" * 6) and address not in seen, "invalid/duplicate device")
        seen.add(address)
        require(b"\0" in record[0xD6:0x1CF], "unterminated device name")
        require(struct.unpack_from("<H", record, 0x46)[0] <= 128, "invalid descriptor length")
        require(struct.unpack_from("<H", record, 0xCA)[0] <= 9, "invalid BRR length")
        require(record[0x39] <= 1, "invalid link-key flag")
    return records


def parse_hack(packet, config_hash):
    require(96 <= len(packet) <= 96 + MAX_RECORDS * RECORD_BYTES, "Hack packet size invalid")
    require(packet[:8] == b"BTHACK1\0", "Hack magic invalid")
    version, count, hos, kind = struct.unpack_from("<4I", packet, 8)
    require((version, hos, kind) == (1, 0x170001, 1) and count <= MAX_RECORDS, "Hack profile invalid")
    require(packet[24:56] == config_hash and len(config_hash) == 32, "Hack config binding invalid")
    require(struct.unpack_from("<Q", packet, 56)[0] == len(packet) == 96 + count * RECORD_BYTES, "Hack length invalid")
    require(digest(packet[:-32]) == packet[-32:], "Hack checksum invalid")
    return validate_records([packet[64 + i * 512:64 + (i + 1) * 512] for i in range(count)])


def parse_source(packet, config_hash, hack_hash):
    require(len(packet) == 12800 and packet[:8] == b"BTORIG1\0", "source packet size/magic invalid")
    version, count, profile, kind = struct.unpack_from("<4I", packet, 8)
    require((version, profile, kind) == (1, 0x220000, 0) and count <= MAX_RECORDS, "source profile invalid")
    require(len(config_hash) == len(hack_hash) == 32 and packet[24:56] == config_hash and packet[56:88] == hack_hash, "source binding invalid")
    require(struct.unpack_from("<Q", packet, 88)[0] == len(packet), "source length invalid")
    require(digest(packet[:-32]) == packet[-32:], "source checksum invalid")
    require(not any(packet[96 + count * 512:-32]), "source padding invalid")
    return validate_records([packet[96 + i * 512:96 + (i + 1) * 512] for i in range(count)])


def name(record):
    return record[0xD6:0x1CF].split(b"\0", 1)[0]


def keyed(record):
    return record[0x39] == 1 and any(record[0x29:0x39])


def controller(record):
    # Generic controllers may have zero VID/PID. Require both HID class and an
    # explicit known controller name, in addition to the user's bound selection.
    return name(record) in (b"Pro Controller", b"Joy-Con (L)", b"Joy-Con (R)") and record[0x27] & 0x1F == 5 and keyed(record)


def merge_selected(destination, source, one_based_slots):
    validate_records(destination)
    validate_records(source)
    require(one_based_slots and len(one_based_slots) <= MAX_RECORDS, "empty/oversized selection")
    require(len(set(one_based_slots)) == len(one_based_slots), "duplicate selection")
    require(all(type(i) is int and 1 <= i <= len(source) for i in one_based_slots), "selection index invalid")
    selected = [source[i - 1] for i in one_based_slots]
    require(all(controller(record) for record in selected), "selection contains audio/unknown/unkeyed device")
    replaced = {record[:6] for record in selected}
    retained = [record for record in destination if record[:6] not in replaced]
    result = retained + selected
    validate_records(result)
    # Preserve nonselected records byte for byte. No eviction or name-based dedup.
    require(result[:len(retained)] == retained, "destination preservation failed")
    require(result[len(retained):] == selected, "selected source preservation failed")
    return result


def encode_external(records):
    validate_records(records)
    return struct.pack("<Q", len(records)) + b"".join(records)


def parse_external(data):
    require(8 <= len(data) <= 8 + MAX_RECORDS * RECORD_BYTES, "external DB size invalid")
    count = struct.unpack_from("<Q", data)[0]
    require(count <= MAX_RECORDS and len(data) == 8 + count * RECORD_BYTES, "external DB count invalid")
    return validate_records([data[8 + i * 512:8 + (i + 1) * 512] for i in range(count)])


def parse_ini(data):
    config = configparser.ConfigParser(interpolation=None, strict=True)
    config.read_string(data.decode("utf-8-sig"))
    return config


def enable_external(data):
    before = parse_ini(data)
    require(before.has_section("atmosphere"), "Atmosphere section missing")
    require(not before.has_option("atmosphere", "enable_external_bluetooth_db"), "external BT already explicitly configured")
    bom = b"\xef\xbb\xbf" if data.startswith(b"\xef\xbb\xbf") else b""
    body = data[len(bom):]
    matches = list(re.finditer(rb"(?m)^[ \t]*\[atmosphere\][ \t]*(?:;[^\r\n]*)?\r?\n", body))
    require(len(matches) == 1, "Atmosphere section layout invalid")
    match = matches[0]
    newline = b"\r\n" if match[0].endswith(b"\r\n") else b"\n"
    addition = b"enable_external_bluetooth_db = u8!0x1" + newline
    result = bom + body[:match.end()] + addition + body[match.end():]
    require(result[:match.end() + len(bom)] + result[match.end() + len(bom) + len(addition):] == data, "unrelated bytes changed")
    after = parse_ini(result)
    expected = {section: dict(before[section]) for section in before.sections()}
    expected["atmosphere"]["enable_external_bluetooth_db"] = "u8!0x1"
    require({section: dict(after[section]) for section in after.sections()} == expected, "unrelated INI options changed")
    return result


def verify_selection_shape(destination, source, slots):
    require((len(destination), len(source), slots) == (3, 7, [3, 6, 7]), "bound selection shape changed")
    require(name(source[2]) == b"Pro Controller" and source[2][:6] == destination[2][:6], "existing Pro identity mismatch")
    require([name(source[i]) for i in (5, 6)] == [b"Joy-Con (R)", b"Joy-Con (L)"], "new generic pair shape changed")
    require(all(source[i][:6] not in {r[:6] for r in destination} for i in (5, 6)), "new pair already exists in destination")
    # The older generic pair has matching source keys; retain Hack metadata too.
    for destination_record in destination[:2]:
        matches = [r for r in source[3:5] if r[:6] == destination_record[:6]]
        require(len(matches) == 1 and matches[0][0x29:0x3A] == destination_record[0x29:0x3A], "existing generic pairing changed")
    result = merge_selected(destination, source, slots)
    require(len(result) == 5 and result[:2] == destination[:2], "final count/preservation mismatch")
    return result


