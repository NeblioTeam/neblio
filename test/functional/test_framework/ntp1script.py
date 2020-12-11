#!/usr/bin/env python3

# from .mininode import CTransaction, CTxOut, sha256, hash256, uint256_from_str, ser_uint256, ser_string
from binascii import hexlify
import hashlib
import sys
from enum import Enum


class NTP1ScriptType(Enum):
    # these numbers are meaningless in the script, they're just internal
    UNDEFINED = 0
    ISSUANCE = 1
    TRANSFER = 2
    BURN = 3

    @staticmethod
    def calcuate_tx_type(op_code):
        if op_code <= 0x0F:
            return NTP1ScriptType.ISSUANCE
        if op_code <= 0x1F:
            return NTP1ScriptType.TRANSFER
        if op_code <= 0x2F:
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
    if len(subscript) < 1:
        raise ValueError("Too short bytes for amount; less than one byte")
    if len(subscript) < amount_size:
        raise ValueError("Too short bytes for amount; less than the provided size ({} < {})".format(len(subscript), amount_size))
    amount_bytes = subscript[0:amount_size]
    amount = decode_ntp1_amount_bytes(amount_bytes)
    return amount


def parse_ntp1v1_transfer_instructions_from_long_enough_bytes(subscript: bytes) -> list:
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


def parse_ntp1v3_transfer_instructions_from_long_enough_bytes(subscript: bytes, max_ti_count: int) -> list:
    result = []
    bytes_ptr = 0
    for _ in range(max_ti_count):
        if len(subscript) - bytes_ptr <= 1:
            raise ValueError("Reached end of bytes before parsing or transfer instructions")
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


def parse_ntp1v3_metadata_from_long_enough_string(subscript: bytes) -> bytes:
    if len(subscript) == 0:
        return b''

    ptr = 0

    if len(subscript) < 4:
        raise ValueError("The data remaining cannot fit metadata start flag, which is 4 bytes, starting from {}".format(subscript.hex()))

    metadata_size = int.from_bytes(subscript[ptr:ptr+4], byteorder='big')

    ptr += 4

    if len(subscript[ptr:]) != metadata_size:
        raise ValueError("Metadata size doesn't match the available data in the script: {}".format(subscript.hex()))

    metadata = subscript[ptr:]
    return metadata




def parse_ntp1v3_issuance_data(subscript: bytes) -> dict:
    result = {}

    ptr = 0

    # parse token symbol
    token_symbol = parse_ntp1_token_symbol(subscript[ptr:ptr+5])
    if not token_symbol.isalnum():
        raise ValueError("Invalid token symbol: {}".format(token_symbol))
    result['token_symbol'] = token_symbol

    ptr += 5

    amount_size = calculate_amount_size(subscript[ptr])

    amount = parse_amount_from_long_enough_bytes(subscript[ptr:ptr+amount_size], amount_size)
    result['amount'] = amount

    ptr += amount_size

    num_of_transfer_instructions = subscript[ptr]
    if num_of_transfer_instructions <= 0:
        raise ValueError("The number of transfer instructions cannot be zero")
    ptr += 1

    result['transfer_instructions'] = parse_ntp1v3_transfer_instructions_from_long_enough_bytes(subscript[ptr:], num_of_transfer_instructions)
    total_ti_size = 0
    for ti in result['transfer_instructions']:
        total_ti_size += ti.total_raw_size
        if ti.skip_input:
            raise ValueError("An issuance script contained a skip transfer instruction: {}" + subscript[ptr:].hex())

    ptr += total_ti_size

    if len(subscript) - ptr < 1:
        raise ValueError("Not enough bytes to parse issuance flags in issuance subscript {}".format(subscript.hex()))
    result['issuance_flags'] = IssuanceFlags.parse_issuance_flags(subscript[ptr])

    ptr += 1

    metadata = parse_ntp1v3_metadata_from_long_enough_string(subscript[ptr:])
    result['metadata'] = metadata
    result['metadata_size'] = len(metadata)

    return result


def parse_ntp1v1_transfer_data(op_code, subscript: bytes) -> dict:
    result = {}
    ptr = 0

    metadata_size = calculate_ntp1v1_metadata_size(op_code)
    result['metadata_size'] = metadata_size

    if len(subscript[ptr:]) < metadata_size:
        raise ValueError("Invalid metadata size; smaller than expected (expected {}, found {})".format(metadata_size, len(subscript[5:])))
    result['metadata'] = subscript[ptr:ptr+metadata_size]

    ptr += metadata_size

    result['transfer_instructions'] = parse_ntp1v1_transfer_instructions_from_long_enough_bytes(subscript[ptr:])
    total_ti_size = 0
    for ti in result['transfer_instructions']:
        total_ti_size += ti.total_raw_size

    ptr += total_ti_size

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

    result['transfer_instructions'] = parse_ntp1v1_transfer_instructions_from_long_enough_bytes(subscript[ptr:])
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
    op_code = script[3]
    script_type = NTP1ScriptType.calcuate_tx_type(op_code)
    result['script_type'] = script_type

    if script_type == NTP1ScriptType.ISSUANCE:
        if protocol_version == 1:
            result['issuance_data'] = parse_ntp1v1_issuance_data(op_code, script[4:])
        elif protocol_version == 3:
            result['issuance_data'] = parse_ntp1v3_issuance_data(script[4:])
        else:
            raise ValueError("Unknown protocol version ({}) for issuance".format(protocol_version))
    elif script_type == NTP1ScriptType.TRANSFER:
        if protocol_version == 1:
            result['transfer_data'] = parse_ntp1v1_transfer_data(op_code, script[4:])
        elif protocol_version == 3:
            pass
        else:
            raise ValueError("Unknown protocol version ({}) for transfer".format(protocol_version))
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


def run_transfer_ntp1v1_parsing_tests():
    transfer_script = '4e5401150069892a92'
    transfer_script_parse_result = parse_ntp1_script(bytes.fromhex(transfer_script))
    assert_equal(transfer_script_parse_result['header'], bytes.fromhex('4e5401'))
    assert_equal(transfer_script_parse_result['protocol_version'], 1)
    assert_equal(transfer_script_parse_result['transfer_data']['metadata'], b'')
    assert_equal(transfer_script_parse_result['transfer_data']['metadata_size'], 0)
    assert_equal(len(transfer_script_parse_result['transfer_data']['transfer_instructions']), 1)
    assert_equal(transfer_script_parse_result['transfer_data']['transfer_instructions'][0].amount, 999901700)
    assert_equal(transfer_script_parse_result['transfer_data']['transfer_instructions'][0].first_raw_byte, 0)
    assert_equal(transfer_script_parse_result['transfer_data']['transfer_instructions'][0].raw_amount, bytes.fromhex('69892A92'))
    assert_equal(transfer_script_parse_result['transfer_data']['transfer_instructions'][0].skip_input, False)
    assert_equal(transfer_script_parse_result['transfer_data']['transfer_instructions'][0].output_index, 0)
    assert_equal(transfer_script_parse_result['transfer_data']['transfer_instructions'][0].total_raw_size, 5)


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
    assert_equal(nibbl_issuance_parse_result['issuance_data']['transfer_instructions'][0].output_index, 0)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['transfer_instructions'][0].total_raw_size, 3)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['issuance_flags'].aggregation_policy, AggregationPolicy.Aggregatable)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['issuance_flags'].divisibility, 7)
    assert_equal(nibbl_issuance_parse_result['issuance_data']['issuance_flags'].locked, True)


def run_issuance_ntp1v3_parsing_tests():
    issuance_script = '4e540301524f4d415001010001f000000965789c8d58db72db3812fd15ac1fa6762ab2123b9924cecb96622b89666ccb9194496d6dcd03488212c624c000a0146e2a55f98daddafdb97cc99e6e9014e5785dfb648b22fa72fa74f781be1c6532c8a3575f8e82bd55e65a96eae8d5d1627e35b9391a1d65caa74e57415b83a7d72a29b4150b2bb35256f85a7b5f2bd77fb352b2c4d3da15fee8d53fbe1c99684ca738cd8ff1611342e55f3d7e6c4275724cdff8b12cfdd371a6d73ac8c2a64a1a5fc954f9716acbc7cf5fbc3c4b7ef9e549fee2e4997afa4cbd4cce5ecae72a7bf1e4797a76969f9dbe789ee74a9e8d2bb3868f52976ad554ecb5946bf5981e7ffd03cebd72176da2a5a2bf88ef563578f18d4ac4e993931738be95454d67571b254ae96e55107e63775e486194ca446e9df0baac0a2512847a9b6ea436c2dba226843c7f9fd45e1be571c66422c090ce941436172d7ada8bc43a3316aff716f0cc2b6584a4534297a5325e158da07864026f41a51b630bbb6e0e7c289c2b6551b0af42bab51a8b7776a7b6cab1eb606d1103d9c728b752176c543a7e4300e7aad0a90c6d863b442c6466b9ece32eee9d8c81e388489c36eb1f91384ea4878d8157e5b61aa5a433618f293e654e6ff74e081e8e779f2650a1273d9a3beb8a6c8c1a8558de65a0188ebe8eba329ef820dec922275bef9fdeade81efcaac67f29c095c6d8daa42a1b0f4ad33fdcbf47905028b9767011c0b0b178fb6106408a02c9c82a82d713e0a3361938331257329d2f470cc4a536f567067c0f3fd2ce64833fb047846b83f818cd7a5bbb54a136a805c555783b087d68e4ad0eefeae4c0c6805ad72a00ba5bb1b6284341a0db882cfb8eb077498376e121884f91c9ff8678927eaa35325cd94aac64015b54e8d7b52e3231af037b6ae33bb778ef02342d6c454e19a379a55c0b230d123f161fd5f76fffc29b1bcd7c633e281fdf0605d79b409f4ae0cd0cfbd322dfc4868d0076400e27b33b3eecde47883e2678abb1353adc2ba351bef688a23e80a94649e72961f519cfb4024cc4cdf3478f46627c3d5d8dc44d1336d68c04ceca6c2b993c939b99e0ee20cfd1e1dfc41cc62d3bdbc9084edb0f1459a6a838609aab07ac676c456cabba6a2b37183d8386e124900cf1447a0f97686821135b8791d86d34c84244202039310058149a0249ed317da31c629ce53030427ba28835cdbd02817080daa4459d31584e38e5eb52c12e5508f327615082add30dce5bf397ffb7519fdd65d1b450293028d132083f3603d1df7671db3cd7a99685981844a933f193d0f3a5985415aab9008f334ad11e12eec75e8d89d515e520aa42f25c2b24da6023ba18da79b66b834086ef9f8dfb6fc5c1b7a3d8a2b7c6ee788e7ffff6ef823268bffefeed3f235139bbe5d98aba824a811b305146e51ae66d37b57d00ded26587c10f1c7701018a02bb5894047bdbcbc63a6c047285b9eb63db4864de4150d6801f34ac2b4a980634ea1edf0110ae36dde44d0b5b672350f4b6edbd52e4d207e546f446294d2352491ba8a446f32aad9d8ac38ec329b83d1d4d9b26b66359d5f4005ee1859a1bf5a4d545b0813f117baeddfd153e18b46183a1f8a9960e26318ef5960e520b405104678b88267bee678ec6b9bf5e4f5f5ffe8c309bdd46b9c86c7c8853bd7d33e53739b8843a88f62f87e8bb31e254aad0ad1965b7d52ed4c0b611b4761be252c01a281f6a818341fa430b2c43c4bc233f2d9585f455a21cecdf680ee102338052ef6074aa5028079127f6f8e030cf44aae90f564870b4a6040b26b00c4d4675633ac1606df4a75a0d0404bb5bd32669f1226de5e322493448df30b31087a287dab580b6d3ab3d64dabd84a7183276775c41b7380e4a71fd09eb0e4b3288396c5aab4eedd01e3ece83a10b7a409d0f821966125a1c89100b829399a604c0abbe7fc8f7cdb9c8eba2880cb8642fcc689e9c3d9c141794e03eb396d4e49588f4d0da7c7f42157e39a8f062ba5cc129ad8958a199018f651afae93c40aadde05cea045b02348bfdba172568cb8abab236b41aba5d59d90010a99180c1d0e25079c6a228f25f39ed55145a2439783fc51a0ef70c58a6631b9378af4b562fced6eb1834c840ee689d06fa07f4e10ab6dbd71cb7c2e600026d0e6722c6f1ba061bd1afdd767d8badf42bd4d4a24e9a6ef3fe0a1d3412d79048e33ffdcf4355cc5a384a50d6c7b14564a533f469a6aac23660e0e770bc26d53d1c2c872cbd4748151a981896d889cae3ec533d9083ddac52eb1b54a67c901ba777b971c59a806a782ecb4aeab511973c1ad19afb2a95fd5b69f756ea54dc6cbdbce6db87d14c81a1d46e93bb5f47dc15deca60c259c3da1043148432b5ec2c61e83b6a548ff60a6aad55ac65165b8ddbd66c208928d0dca635b12af62e91a237d3ea20ccb058858dae625fec7979ccb792fb6e5d0fc2fbf42ebc030cdfcec56a2eae268bdfa62b31b9be10b41b6673a2e4efa7e2f574351197930fd7e7efc662965127e54dbcd411b982303addf0254afa6ef976f71beace1492a2dded2277b68cab9d44a1ce3b0d42ebffbe22a0d49f25c947dfcbae774a16619392c2c3f6b1042176307a840a3312cb1a246ec4391be21d8869029aae2daacfbe287280a033fe1837b535ac82346787d6db5b3c680adc55f70a94a6fd718edb7022d3db38aff79032ef100044862d1589694fd4748aaf0664d893cce09b27a3b8d7f541f63bac1dd92d4d494b2f6bd252dbd3b1b87071d7e7b5e33da57177d9c60c49fdefd184020a368512b883395f1efa3b7a77cb7d8843cfee726876757339bd9a5eaf26abd9fc9aa97331bd9cfd3e5dfc5dccdf0c6824961f66aba9009bdecc170207a68b9bc56c3915e71f96abf9d574b11c8b37dc169ddaa0344ad689c8f15ef4747bbfe973a820820c6f60ea360f1936ac485f8a7e00f08f0db204b97c41ed8ebe757cf767d35d470f4cb45a528107b3c0f7a7ad6aa5f1f07ec5d391f348e394a001c48b81162950dcc7d707e561cce7b4f8daca1c96730fc53e5dba7a14518d1371782c0c56476c41e0c4f28e81bafbfbc45eca74825c1b7a1b391c5ebc1ebc0b3f39397b34e444870c76799fc24ff8902157d71c7f243f93fe4715baac47cddcdefe52453d5be87f2240f519e19a35378236fda41cd188cfe24ac77da31b033c92e8b73b1e43f1c6c8bf3490b8ce713ae3df64188b28d59266b8e33a91d5aa1f6e2e9a57a44ad938b15ff83a8d2488125f7bd21c9e6f4f5c739fca226e9cf626db746d3c60d260cd24aab1301487685bb451cf460a02ff5bc88212d5d6814620ecb13889abc0569585f2a6f5a6da0b4cae0d0bf1f8f3d4fdebadff250aca2d6076dfb79dfff8faf5eb7f01d62e81a9'
    issuance_parse_result = parse_ntp1_script(bytes.fromhex(issuance_script))
    assert_equal(issuance_parse_result['header'], bytes.fromhex('4e5403'))
    assert_equal(issuance_parse_result['protocol_version'], 3)
    assert_equal(issuance_parse_result['script_type'], NTP1ScriptType.ISSUANCE)
    assert_equal(issuance_parse_result['issuance_data']['token_symbol'], 'ROMAP')
    assert_equal(issuance_parse_result['issuance_data']['amount'], 1)
    assert_equal(issuance_parse_result['issuance_data']['metadata_size'], 2405)
    assert_equal(issuance_parse_result['issuance_data']['metadata'], bytes.fromhex("789C8D58DB72DB3812FD15AC1FA6762AB2123B9924CECB96622B89666CCB9194496D6DCD03488212C624C000A0146E2A55F98DADDAFDB97CC99E6E9014E5785DFB648B22FA72FA74F781BE1C6532C8A3575F8E82BD55E65A96EAE8D5D1627E35B9391A1D65CAA74E57415B83A7D72A29B4150B2BB35256F85A7B5F2BD77FB352B2C4D3DA15FEE8D53FBE1C99684CA738CD8FF1611342E55F3D7E6C4275724CDFF8B12CFDD371A6D73AC8C2A64A1A5FC954F9716ACBC7CF5FBC3C4B7EF9E549FEE2E4997AFA4CBD4CCE5ECAE72A7BF1E4797A76969F9DBE789EE74A9E8D2BB3868F52976AD554ECB5946BF5981E7FFD03CEBD72176DA2A5A2BF88EF563578F18D4AC4E993931738BE95454D67571B254AE96E55107E63775E486194CA446E9DF0BAAC0A2512847A9B6EA436C2DBA226843C7F9FD45E1BE571C66422C090CE941436172D7ADA8BC43A3316AFF716F0CC2B6584A4534297A5325E158DA07864026F41A51B630BBB6E0E7C289C2B6551B0AF42BAB51A8B7776A7B6CAB1EB606D1103D9C728B752176C543A7E4300E7AAD0A90C6D863B442C6466B9ECE32EEE9D8C81E388489C36EB1F91384EA4878D8157E5B61AA5A433618F293E654E6FF74E081E8E779F2650A1273D9A3BEB8A6C8C1A8558DE65A0188EBE8EBA329EF820DEC922275BEF9FDEADE81EFCAAC67F29C095C6D8DAA42A1B0F4AD33FDCBF47905028B9767011C0B0B178FB6106408A02C9C82A82D713E0A3361938331257329D2F470CC4A536F567067C0F3FD2CE64833FB047846B83F818CD7A5BBB54A136A805C555783B087D68E4AD0EEFEAE4C0C6805AD72A00BA5BB1B6284341A0DB882CFB8EB077498376E121884F91C9FF8678927EAA35325CD94AAC64015B54E8D7B52E3231AF037B6AE33BB778EF02342D6C454E19A379A55C0B230D123F161FD5F76FFFC29B1BCD7C633E281FDF0605D79B409F4AE0CD0CFBD322DFC4868D0076400E27B33B3EECDE47883E2678ABB1353ADC2BA351BEF688A23E80A94649E72961F519CFB4024CC4CDF3478F46627C3D5D8DC44D1336D68C04CECA6C2B993C939B99E0EE20CFD1E1DFC41CC62D3BDBC9084EDB0F1459A6A838609AAB07AC676C456CABBA6A2B37183D8386E124900CF1447A0F97686821135B8791D86D34C84244202039310058149A0249ED317DA31C629CE53030427BA28835CDBD02817080DAA4459D31584E38E5EB52C12E5508F327615082ADD30DCE5BF397FFB7519FDD65D1B450293028D132083F3603D1DF7671DB3CD7A99685981844A933F193D0F3A5985415AAB9008F334AD11E12EEC75E8D89D515E520AA42F25C2B24DA6023BA18DA79B66B834086EF9F8DFB6FC5C1B7A3D8A2B7C6EE788E7FFFF6EF823268BFFEFEED3F235139BBE5D98ABA824A811B305146E51AE66D37B57D00DED26587C10F1C7701018A02BB5894047BDBCBC63A6C047285B9EB63DB4864DE4150D6801F34AC2B4A980634EA1EDF0110AE36DDE44D0B5B672350F4B6EDBD52E4D207E546F446294D2352491BA8A446F32AAD9D8AC38EC329B83D1D4D9B26B66359D5F4005EE1859A1BF5A4D545B0813F117BAEDDFD153E18B46183A1F8A9960E26318EF5960E520B405104678B88267BEE678EC6B9BF5E4F5F5FFE8C309BDD46B9C86C7C8853BD7D33E53739B8843A88F62F87E8BB31E254AAD0AD1965B7D52ED4C0B611B4761BE252C01A281F6A818341FA430B2C43C4BC233F2D9585F455A21CECDF680EE102338052EF6074AA5028079127F6F8E030CF44AAE90F564870B4A6040B26B00C4D4675633AC1606DF4A75A0D0404BB5BD32669F1226DE5E322493448DF30B31087A287DAB580B6D3AB3D64DABD84A7183276775C41B7380E4A71FD09EB0E4B3288396C5AAB4EEDD01E3ECE83A10B7A409D0F821966125A1C89100B829399A604C0ABBE7FC8F7CDB9C8EBA2880CB8642FCC689E9C3D9C141794E03EB396D4E49588F4D0DA7C7F42157E39A8F062BA5CC129AD8958A199018F651AFAE93C40AADDE05CEA045B02348BFDBA172568CB8ABAB236B41ABA5D59D90010A99180C1D0E25079C6A228F25F39ED55145A2439783FC51A0EF70C58A6631B9378AF4B562FCED6EB1834C840EE689D06FA07F4E10AB6DBD71CB7C2E600026D0E6722C6F1BA061BD1AFDD767D8BADF42BD4D4A24E9A6EF3FE0A1D3412D79048E33FFDCF4355CC5A384A50D6C7B14564A533F469A6AAC23660E0E770BC26D53D1C2C872CBD4748151A981896D889CAE3EC533D9083DDAC52EB1B54A67C901BA777B971C59A806A782ECB4AEAB511973C1AD19AFB2A95FD5B69F756EA54DC6CBDBCE6DB87D14C81A1D46E93BB5F47DC15DECA60C259C3DA1043148432B5EC2C61E83B6A548FF60A6AAD55AC65165B8DDBD66C208928D0DCA635B12AF62E91A237D3EA20CCB058858DAE625FEC7979CCB792FB6E5D0FC2FBF42EBC030CDFCEC56A2EAE268BDFA62B31B9BE10B41B6673A2E4EFA7E2F574351197930FD7E7EFC662965127E54DBCD411B982303ADDF0254AFA6EF976F71BEACE1492A2DDED2277B68CAB9D44A1CE3B0D42EBFFBE22A0D49F25C947DFCBAE774A16619392C2C3F6B1042176307A840A3312CB1A246EC4391BE21D8869029AAE2DAACFBE287280A033FE1837B535AC82346787D6DB5B3C680ADC55F70A94A6FD718EDB7022D3DB38AFF79032EF100044862D1589694FD4748AAF0664D893CCE09B27A3B8D7F541F63BAC1DD92D4D494B2F6BD252DBD3B1B87071D7E7B5E33DA57177D9C60C49FDEFD184020A368512B883395F1EFA3B7A77CB7D8843CFEE726876757339BD9A5EAF26ABD9FC9AA97331BD9CFD3E5DFC5DCCDF0C6824961F66ABA9009BDECC170207A68B9BC56C3915E71F96ABF9D574B11C8B37DC169DDAA0344AD689C8F15EF4747BBFE973A820820C6F60EA360F1936AC485F8A7E00F08F0DB204B97C41ED8EBE757CF767D35D470F4CB45A528107B3C0F7A7AD6AA5F1F07EC5D391F348E394A001C48B81162950DCC7D707E561CCE7B4F8DACA1C96730FC53E5DBA7A14518D1371782C0C56476C41E0C4F28E81BAFBFBC45ECA74825C1B7A1B391C5EBC1EBC0B3F39397B34E444870C76799FC24FF8902157D71C7F243F93FE4715BAAC47CDDCDEFE52453D5BE87F2240F519E19A35378236FDA41CD188CFE24AC77DA31B033C92E8B73B1E43F1C6C8BF3490B8CE713AE3DF64188B28D59266B8E33A91D5AA1F6E2E9A57A44AD938B15FF83A8D2488125F7BD21C9E6F4F5C739FCA226E9CF626DB746D3C60D260CD24AAB1301487685BB451CF460A02FF5BC88212D5D6814620ECB13889ABC0569585F2A6F5A6DA0B4CAE0D0BF1F8F3D4FDEBADFF250ACA2D6076DFB79DFFF8FAF5EB7F01D62E81A9"))
    assert_equal(len(issuance_parse_result['issuance_data']['transfer_instructions']), 1)
    assert_equal(issuance_parse_result['issuance_data']['transfer_instructions'][0].amount, 1)
    assert_equal(issuance_parse_result['issuance_data']['transfer_instructions'][0].first_raw_byte, 0)
    assert_equal(issuance_parse_result['issuance_data']['transfer_instructions'][0].raw_amount, bytes.fromhex('01'))
    assert_equal(issuance_parse_result['issuance_data']['transfer_instructions'][0].skip_input, False)
    assert_equal(issuance_parse_result['issuance_data']['transfer_instructions'][0].output_index, 0)
    assert_equal(issuance_parse_result['issuance_data']['transfer_instructions'][0].total_raw_size, 2)
    assert_equal(issuance_parse_result['issuance_data']['issuance_flags'].aggregation_policy, AggregationPolicy.Aggregatable)
    assert_equal(issuance_parse_result['issuance_data']['issuance_flags'].divisibility, 7)
    assert_equal(issuance_parse_result['issuance_data']['issuance_flags'].locked, True)


def run_all_local_tests():
    run_numerics_tests()
    run_issuance_ntp1v1_parsing_tests()
    run_issuance_ntp1v3_parsing_tests()
    run_transfer_ntp1v1_parsing_tests()


# to test this file
if __name__ == "__main__":
    run_all_local_tests()