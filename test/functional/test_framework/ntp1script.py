#!/usr/bin/env python3

# from .mininode import CTransaction, CTxOut, sha256, hash256, uint256_from_str, ser_uint256, ser_string
from binascii import hexlify
import hashlib
import sys
from enum import Enum
import unittest


class NTP1ScriptType(Enum):
    # these numbers are meaningless in the script, they're just internal
    UNDEFINED = 0
    ISSUANCE = 1
    TRANSFER = 2
    BURN = 3

    @staticmethod
    def calcuate_tx_type(op_code):
        if op_code <= 0x1F:
            return NTP1ScriptType.ISSUANCE
        if op_code <= 0x2F:
            return NTP1ScriptType.TRANSFER
        if op_code <= 0x3F:
            return NTP1ScriptType.BURN
        raise ValueError("Unknown op_code for NTP1ScriptType {}".format(op_code))


class AggregationPolicy(Enum):
    # these numbers are meaningless in the script, they're just internal
    Unknown = 0
    Aggregatable = 1
    NonAggregatable = 2


class IssuanceFlags:
    def __init__(self):
        self.aggregation_policy = AggregationPolicy.Unknown
        self.divisibility = None
        self.locked = None

    @staticmethod
    def parse_issuance_flags(issuance_flags_byte: int):
        result = IssuanceFlags()
        result.divisibility = (issuance_flags_byte & 0xe0) >> 5
        result.locked = (issuance_flags_byte & 0x10) >> 4
        aggregable_value = (issuance_flags_byte & 0xC) >> 2
        if aggregable_value == 0:
            result.aggregation_policy = AggregationPolicy.Aggregatable
        elif aggregable_value == 2:
            result.aggregation_policy = AggregationPolicy.NonAggregatable
        else:
            raise ValueError("Unknown aggregation policy with value {}".format(aggregable_value))
        return result


class TransferInstruction:
    def __init__(self):
        self.amount = 0
        # transfer instructions act on inputs in order until they're empty, so instruction 0 will act on
        # input 0, and instruction 1 will act on input 0, etc... until input 0 is empty, or a skip
        # instruction is given to move to the next input
        self.skip_input = False
        self.output_index = None
        self.first_raw_byte = 1
        self.raw_amount = b''
        self.total_raw_size = None


NTP1_HEADER_BYTES = bytes.fromhex('4e54')


def parse_ntp1_token_symbol(symbol_bytes: bytes):
    return symbol_bytes.replace(bytes.fromhex("20"), bytes.fromhex('')).decode('ascii')


def calculate_ntp1v1_metadata_size(op_code_char):
    if op_code_char == 0x01:
        return 52
    elif op_code_char == 0x02:
        return 20
    elif op_code_char == 0x03:
        return 0
    elif op_code_char == 0x04:
        return 20
    elif op_code_char == 0x05:
        return 0
    elif op_code_char == 0x06:
        return 0
    elif op_code_char == 0x10:
        return 52
    elif op_code_char == 0x11:
        return 20
    elif op_code_char == 0x12:
        return 0
    elif op_code_char == 0x13:
        return 20
    elif op_code_char == 0x14:
        return 20
    elif op_code_char == 0x15:
        return 0
    elif op_code_char == 0x20:
        return 52
    elif op_code_char == 0x21:
        return 20
    elif op_code_char == 0x22:
        return 0
    elif op_code_char == 0x23:
        return 20
    elif op_code_char == 0x24:
        return 20
    elif op_code_char == 0x25:
        return 0
    raise ValueError("Unknown OP_CODE (in hex): {}".format(op_code_char))


def calculate_amount_size(first_char):
    s = (first_char >> 5) & 0x7  # take the 3 MSB bits
    if s < 6:
        return s + 1
    else:
        return 7

def clear_msb(value: int, msb_bits_to_clear: int):
    result = value
    for _ in range(msb_bits_to_clear):
        to_sub = 2**(result.bit_length()-2)
        result -= to_sub
    return result


def get_trailing_zeros(number: int) -> int:
    var = number
    result = 0
    while True:
        if var % 10 == 0:
            result += 1
            var /= 10
        else:
            break
    return result


def encode_ntp1_amount_bytes(amount: int) -> bytes:
    trailing_zeros_count = get_trailing_zeros(amount)
    # numbers less than 32 can fit in a single byte with no exponent
    # this other condition is provided to match the API server
    if amount >= 32 and len(str(amount // 10**trailing_zeros_count)) <= 12:
        exponent = trailing_zeros_count
    else:
        exponent = 0
    mantissa = amount // 10**exponent

    if mantissa.bit_length() <= 5 and exponent == 0:
        header = int("000", 2)
        header_size = 3
        mantissa_size = 5
        exponent_size = 0
    elif mantissa.bit_length() <= 9 and exponent.bit_length() <= 4:
        header = int("001", 2)
        header_size = 3
        mantissa_size = 9
        exponent_size = 4
    elif mantissa.bit_length() <= 17 and exponent.bit_length() <= 4:
        header = int("010", 2)
        header_size = 3
        mantissa_size = 17
        exponent_size = 4
    elif mantissa.bit_length() <= 25 and exponent.bit_length() <= 4:
        header = int("011", 2)
        header_size = 3
        mantissa_size = 25
        exponent_size = 4
    elif mantissa.bit_length() <= 34 and exponent.bit_length() <= 3:
        header = int("100", 2)
        header_size = 3
        mantissa_size = 34
        exponent_size = 3
    elif mantissa.bit_length() <= 42 and exponent.bit_length() <= 3:
        header = int("101", 2)
        header_size = 3
        mantissa_size = 42
        exponent_size = 3
    elif mantissa.bit_length() <= 54 and exponent == 0:
        header = int("11", 2)
        header_size = 2
        mantissa_size = 54
        exponent_size = 0
    else:
        raise ValueError("Unable to encode the number {} "
                         "to NTP1 amount hex; its mantissa and exponent do not fit in the "
                         "expected binary representation.".format(amount))

    # assert mantissa_size - mantissa.bit_length() >= 0
    # assert exponent_size - exponent.bit_length() <= 0

    # mantissa *= 10**(mantissa_size - mantissa.bit_length())
    # exponent //= 10**(exponent_size - exponent.bit_length())

    total_bit_size = header_size + mantissa_size + exponent_size

    bits = (header << mantissa_size + exponent_size) + \
           (mantissa << exponent_size) + exponent

    return bits.to_bytes(total_bit_size // 8, byteorder='big')


def decode_ntp1_amount_bytes(amount_bytes: bytes) -> int:
    temp = int.from_bytes(amount_bytes, byteorder='big')  # todo: remove
    temp1 = bin(int.from_bytes(amount_bytes, byteorder='big'))  # todo: remove
    bit0 = bool(0x80 & amount_bytes[0])
    bit1 = bool(0x40 & amount_bytes[0])
    bit2 = bool(0x20 & amount_bytes[0])
    header_size = None
    mantissa_size = None
    exponent_size = None
    if bit0 and bit1:
        header_size = 2
        mantissa_size = 54
        exponent_size = 0
    else:
        header_size = 3
        if bit0 == 0 and bit1 == 0 and bit2 == 0:
            mantissa_size = 5
            exponent_size = 0
        elif bit0 == 0 and bit1 == 0 and bit2 == 1:
            mantissa_size = 9
            exponent_size = 4
        elif bit0 == 0 and bit1 == 1 and bit2 == 0:
            mantissa_size = 17
            exponent_size = 4
        elif bit0 == 0 and bit1 == 1 and bit2 == 1:
            mantissa_size = 25
            exponent_size = 4
        elif bit0 == 1 and bit1 == 0 and bit2 == 0:
            mantissa_size = 34
            exponent_size = 3
        elif bit0 == 1 and bit1 == 0 and bit2 == 1:
            mantissa_size = 42
            exponent_size = 3
        else:
            raise ValueError("Unexpected binary structure. This should never happen.")
    total_size = header_size + mantissa_size + exponent_size
    assert total_size % 8 == 0

    unmasked_result = int.from_bytes(amount_bytes, byteorder='big')
    mantissa_mask = ((1 << mantissa_size) - 1) << exponent_size
    exponent_mask = ((1 << exponent_size) - 1)
    mantissa = (unmasked_result & mantissa_mask) >> exponent_size
    exponent = (unmasked_result & exponent_mask)
    result = mantissa * 10**exponent
    return result


def parse_amount_from_long_enough_bytes(subscript: bytes, amount_size):
    if len(subscript) < 2:
        raise ValueError("Too short bytes for amount; less than two bytes")
    if len(subscript) < amount_size:
        raise ValueError("Too short bytes for amount; less than the provided size ({} < {})".format(len(subscript), amount_size))
    amount_bytes = subscript[0:amount_size]
    amount = decode_ntp1_amount_bytes(amount_bytes)
    return amount


def parse_transfer_instructions_from_long_enough_bytes(subscript: bytes) -> list:
    result = []
    bytes_ptr = 0
    while True:
        if len(subscript) - bytes_ptr <= 1:
            break
        ti = TransferInstruction()
        ti.first_raw_byte = subscript[bytes_ptr]
        amount_size = calculate_amount_size(subscript[bytes_ptr + 1])
        ti.raw_amount = subscript[bytes_ptr + 1: bytes_ptr + 1 + amount_size]
        ti.amount = decode_ntp1_amount_bytes(ti.raw_amount)
        ti.skip_input = bool(ti.first_raw_byte & 1)
        ti.output_index = ti.first_raw_byte >> 3
        ti.total_raw_size = 1 + amount_size

        bytes_ptr += 1 + amount_size

        result.append(ti)

    return result


def parse_ntp1v1_issuance_data(op_code, subscript: bytes) -> dict:
    result = {}

    ptr = 0

    # parse token symbol
    token_symbol = parse_ntp1_token_symbol(subscript[ptr:ptr+5])
    if not token_symbol.isalnum():
        raise ValueError("Invalid token symbol: {}".format(token_symbol))
    result['token_symbol'] = token_symbol

    ptr += 5

    metadata_size = calculate_ntp1v1_metadata_size(op_code)
    result['metadata_size'] = metadata_size

    if len(subscript[ptr:]) < metadata_size:
        raise ValueError("Invalid metadata size; smaller than expected (expected {}, found {})".format(metadata_size, len(subscript[5:])))
    result['metadata'] = subscript[ptr:ptr+metadata_size]

    ptr += metadata_size

    amount_size = calculate_amount_size(subscript[ptr])

    amount = parse_amount_from_long_enough_bytes(subscript[ptr:ptr+amount_size], amount_size)
    result['amount'] = amount

    ptr += amount_size

    result['transfer_instructions'] = parse_transfer_instructions_from_long_enough_bytes(subscript[ptr:])
    total_ti_size = 0
    for ti in result['transfer_instructions']:
        total_ti_size += ti.total_raw_size
        if ti.skip_input:
            raise ValueError("An issuance script contained a skip transfer instruction: {}" + subscript[ptr:].hex())

    ptr += total_ti_size

    if len(subscript) - ptr > 1:
        raise ValueError("Unexplained bytes in ntp1script")
    result['issuance_flags'] = IssuanceFlags.parse_issuance_flags(subscript[ptr])

    return result


def parse_ntp1_script(script: bytes):
    result = {}

    # parse header
    if script[0:2] != NTP1_HEADER_BYTES:
        raise ValueError("Invalid NTP1 script header")
    if script[2] != 1 and script[2] != 3:
        raise ValueError("Unknown NTP1 script version {}".format(script))
    result['header'] = script[0:3]
    protocol_version = script[2]
    result['protocol_version'] = protocol_version

    # parse script type
    op_code = script[2]
    script_type = NTP1ScriptType.calcuate_tx_type(op_code)
    result['script_type'] = script_type

    if script_type == NTP1ScriptType.ISSUANCE:
        result['issuance_data'] = parse_ntp1v1_issuance_data(op_code, script[4:])
    elif script_type == NTP1ScriptType.TRANSFER:
        pass
    elif script_type == NTP1ScriptType.BURN:
        pass
    else:
        raise ValueError("Unknown script type {}".format(script_type))
    return result


def assert_equal(first, second):
    if first != second:
        raise AssertionError("{} != {}".format(first, second))


def run_numerics_tests():
    assert_equal(calculate_amount_size(bytes.fromhex("11")[0]), 1)
    assert_equal(calculate_amount_size(bytes.fromhex("69")[0]), 4)
    assert_equal(calculate_amount_size(bytes.fromhex("2012")[0]), 2)
    assert_equal(calculate_amount_size(bytes.fromhex("4bb3c1")[0]), 3)
    assert_equal(calculate_amount_size(bytes.fromhex("68c7e5b3")[0]), 4)
    assert_equal(calculate_amount_size(bytes.fromhex("8029990f1a")[0]), 5)
    assert_equal(calculate_amount_size(bytes.fromhex("a09c47f7b1a1")[0]), 6)
    assert_equal(calculate_amount_size(bytes.fromhex("c0a60eea1aa8fd")[0]), 7)

    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("69892a92")), 999901700)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("c007b60b6f687a")), 8478457292922)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("40ef54")), 38290000)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("201f")), 1000000000000000)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("60b0b460")), 723782)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("5545e1")), 871340)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("c007b60b6f687a")), 8478457292922)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("11")), 17)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("2011")), 10)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("2012")), 100)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("4bb3c1")), 479320)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("68c7e5b3")), 9207387000)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("8029990f1a")), 8723709100)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("a09c47f7b1a1")), 839027891720)
    assert_equal(decode_ntp1_amount_bytes(bytes.fromhex("c0a60eea1aa8fd")), 182582987368701)

    assert_equal(encode_ntp1_amount_bytes(999901700), bytes.fromhex("69892a92"))
    assert_equal(encode_ntp1_amount_bytes(8478457292922), bytes.fromhex("c007b60b6f687a"))
    assert_equal(encode_ntp1_amount_bytes(38290000), bytes.fromhex("40ef54"))
    assert_equal(encode_ntp1_amount_bytes(1000000000000000), bytes.fromhex("201f"))
    assert_equal(encode_ntp1_amount_bytes(723782), bytes.fromhex("60b0b460"))
    assert_equal(encode_ntp1_amount_bytes(871340), bytes.fromhex("5545e1"))
    assert_equal(encode_ntp1_amount_bytes(8478457292922), bytes.fromhex("c007b60b6f687a"))
    assert_equal(encode_ntp1_amount_bytes(17), bytes.fromhex("11"))
    assert_equal(encode_ntp1_amount_bytes(100), bytes.fromhex("2012"))
    assert_equal(encode_ntp1_amount_bytes(479320), bytes.fromhex("4bb3c1"))
    assert_equal(encode_ntp1_amount_bytes(9207387000), bytes.fromhex("68c7e5b3"))
    assert_equal(encode_ntp1_amount_bytes(8723709100), bytes.fromhex("8029990f1a"))
    assert_equal(encode_ntp1_amount_bytes(839027891720), bytes.fromhex("a09c47f7b1a1"))
    assert_equal(encode_ntp1_amount_bytes(182582987368701), bytes.fromhex("c0a60eea1aa8fd"))

    assert_equal(encode_ntp1_amount_bytes(999999999997990), bytes.fromhex("c38d7ea4c67826"))
    assert_equal(encode_ntp1_amount_bytes(276413656646664), bytes.fromhex("c0fb6591d0c408"))
    assert_equal(encode_ntp1_amount_bytes(9731165496688), bytes.fromhex("c008d9b6a9a570"))
    assert_equal(encode_ntp1_amount_bytes(943721684679640), bytes.fromhex("c35a4f53c83bd8"))
    assert_equal(encode_ntp1_amount_bytes(1412849080), bytes.fromhex("80435eb161"))


def run_issuance_ntp1v1_parsing_tests():
    nibbl_issuance_script = '4e5401014e4942424cab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef215620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0'
    nibbl_issuance_parse_result = parse_ntp1_script(bytes.fromhex(nibbl_issuance_script))
    assert_equal(nibbl_issuance_parse_result['header'], bytes.fromhex('4e5401'))
    assert_equal(nibbl_issuance_parse_result['protocol_version'], 1)
    assert_equal(nibbl_issuance_parse_result['script_type'], NTP1ScriptType.ISSUANCE)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['token_symbol'], 'NIBBL')
    assert_equal(nibbl_issuance_parse_result['issuance_data']['metadata_size'], 52)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['metadata'], bytes.fromhex("AB10C04E20E0AEC73D58C8FBF2A9C26A6DC3ED666C7B80FEF215620C817703B1E5D8B1870211CE7CDF50718B4789245FB80F5899"))
    assert_equal(nibbl_issuance_parse_result['issuance_data']['amount'], 1000000000)
    assert_equal(len(nibbl_issuance_parse_result['issuance_data']['transfer_instructions']), 1)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['transfer_instructions'][0].amount, 1000000000)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['transfer_instructions'][0].first_raw_byte, 0)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['transfer_instructions'][0].raw_amount, bytes.fromhex('2019'))
    assert_equal(nibbl_issuance_parse_result['issuance_data']['transfer_instructions'][0].skip_input, False)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['transfer_instructions'][0].total_raw_size, 3)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['issuance_flags'].aggregation_policy, AggregationPolicy.Aggregatable)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['issuance_flags'].divisibility, 7)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['issuance_flags'].locked, True)


def run_all_local_tests():
    run_numerics_tests()
    run_issuance_ntp1v1_parsing_tests()


# to test this file
if __name__ == "__main__":
    run_all_local_tests()