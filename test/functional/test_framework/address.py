#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Encode and decode BASE58, P2PKH and P2SH addresses."""

from .script import hash256, hash160, sha256, CScript, OP_0
from .util import bytes_to_hex_str, hex_str_to_bytes

from . import segwit_addr

## --- Base58Check encoding
__b58chars = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'
__b58base = len(__b58chars)
b58chars = __b58chars

long = int
_bchr = lambda x: bytes([x])
_bord = lambda x: x

WIF_PREFIX = 0xb5
TESTNET_WIF_PREFIX = 0xc1

def byte_to_base58(b, version):
    result = ''
    str = bytes_to_hex_str(b)
    str = bytes_to_hex_str(chr(version).encode('latin-1')) + str
    checksum = bytes_to_hex_str(hash256(hex_str_to_bytes(str)))
    str += checksum[:8]
    value = int('0x'+str,0)
    while value > 0:
        result = __b58chars[value % 58] + result
        value //= 58
    while (str[:2] == '00'):
        result = __b58chars[0] + result
        str = str[2:]
    return result

def b58encode(v):
    """
    encode v, which is a string of bytes, to base58.
    """
    long_value = 0
    for (i, c) in enumerate(v[::-1]):
        long_value += (256**i) * _bord(c)

    result = ''
    while long_value >= __b58base:
        div, mod = divmod(long_value, __b58base)
        result = __b58chars[mod] + result
        long_value = div
    result = __b58chars[long_value] + result
    # Bitcoin does a little leading-zero-compression:
    # leading 0-bytes in the input become leading-1s
    nPad = 0
    for c in v:
        #        if c == '\0': nPad += 1
        if c == 0:
            nPad += 1
        else:
            break

    return (__b58chars[0] * nPad) + result

def b58decode(v, length=None):
    """ decode v into a string of len bytes
    """
    long_value = 0
    for (i, c) in enumerate(v[::-1]):
        long_value += __b58chars.find(c) * (__b58base**i)

    result = bytes()
    while long_value >= 256:
        div, mod = divmod(long_value, 256)
        result = _bchr(mod) + result
        long_value = div
    result = _bchr(long_value) + result

    nPad = 0
    for c in v:
        if c == __b58chars[0]:
            nPad += 1
        else:
            break

    result = _bchr(0) * nPad + result
    if length is not None and len(result) != length:
        return None

    return result

def wif_to_privkey(secretString):
    wif_compressed = 52 == len(secretString)
    pvkeyencoded = b58decode(secretString).hex()
    wifversion = pvkeyencoded[:2]
    checksum = pvkeyencoded[-8:]
    vs = bytes.fromhex(pvkeyencoded[:-8])
    check = hash256(vs)[0:4]

    if (wifversion == WIF_PREFIX.to_bytes(1, byteorder='big').hex() and checksum == check.hex()) \
            or (wifversion == TESTNET_WIF_PREFIX.to_bytes(1, byteorder='big').hex() and checksum == check.hex()):

        if wif_compressed:
            privkey = pvkeyencoded[2:-10]

        else:
            privkey = pvkeyencoded[2:-8]

        return privkey, wif_compressed

    else:
        return None

def keyhash_to_p2pkh(hash, main = False):
    assert (len(hash) == 20)
    version = 53 if main else 65
    return byte_to_base58(hash, version)

def scripthash_to_p2sh(hash, main = False):
    assert (len(hash) == 20)
    version = 112 if main else 127
    return byte_to_base58(hash, version)

def key_to_p2pkh(key, main = False):
    key = check_key(key)
    return keyhash_to_p2pkh(hash160(key), main)

def script_to_p2sh(script, main = False):
    script = check_script(script)
    return scripthash_to_p2sh(hash160(script), main)

def key_to_p2sh_p2wpkh(key, main = False):
    key = check_key(key)
    p2shscript = CScript([OP_0, hash160(key)])
    return script_to_p2sh(p2shscript, main)

def program_to_witness(version, program, main = False):
    if (type(program) is str):
        program = hex_str_to_bytes(program)
    assert 0 <= version <= 16
    assert 2 <= len(program) <= 40
    assert version > 0 or len(program) in [20, 32]
    return segwit_addr.encode("bc" if main else "bcrt", version, program)

def script_to_p2wsh(script, main = False):
    script = check_script(script)
    return program_to_witness(0, sha256(script), main)

def key_to_p2wpkh(key, main = False):
    key = check_key(key)
    return program_to_witness(0, hash160(key), main)

def script_to_p2sh_p2wsh(script, main = False):
    script = check_script(script)
    p2shscript = CScript([OP_0, sha256(script)])
    return script_to_p2sh(p2shscript, main)

def check_key(key):
    if (type(key) is str):
        key = hex_str_to_bytes(key) # Assuming this is hex string
    if (type(key) is bytes and (len(key) == 33 or len(key) == 65)):
        return key
    assert(False)

def check_script(script):
    if (type(script) is str):
        script = hex_str_to_bytes(script) # Assuming this is hex string
    if (type(script) is bytes or type(script) is CScript):
        return script
    assert(False)
