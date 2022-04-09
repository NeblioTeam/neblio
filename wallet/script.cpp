// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

using namespace std;
using namespace boost;

#include "bignum.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script.h"
#include "sync.h"
#include "txdb.h"
#include "util.h"

template <typename T>
std::vector<unsigned char> ToByteVector(const T& in)
{
    return std::vector<unsigned char>(in.begin(), in.end());
}

bool CheckSig(vector<unsigned char> vchSig, vector<unsigned char> vchPubKey, CScript scriptCode,
              const CTransaction& txTo, unsigned int nIn, int nHashType);

namespace {

inline bool set_success(ScriptError* ret)
{
    if (ret)
        *ret = SCRIPT_ERR_OK;
    return true;
}

inline bool set_error(ScriptError* ret, const ScriptError serror)
{
    if (ret)
        *ret = serror;
    return false;
}

} // namespace

static const valtype vchFalse(0);
static const valtype vchZero(0);
static const valtype vchTrue(1, 1);
static const CBigNum bnZero(0);
static const CBigNum bnOne(1);
static const CBigNum bnFalse(0);
static const CBigNum bnTrue(1);
static const size_t  nMaxNumSize = 4;

CBigNum CastToBigNum(const valtype& vch)
{
    if (vch.size() > nMaxNumSize)
        throw runtime_error("CastToBigNum() : overflow");
    // Get rid of extra leading zeros
    return CBigNum(CBigNum(vch).getvch());
}

bool CastToBool(const valtype& vch)
{
    for (unsigned int i = 0; i < vch.size(); i++) {
        if (vch[i] != 0) {
            // Can be negative zero
            if (i == vch.size() - 1 && vch[i] == 0x80)
                return false;
            return true;
        }
    }
    return false;
}

//
// WARNING: This does not work as expected for signed integers; the sign-bit
// is left in place as the integer is zero-extended. The correct behavior
// would be to move the most significant bit of the last byte during the
// resize process. MakeSameSize() is currently only used by the disabled
// opcodes OP_AND, OP_OR, and OP_XOR.
//
void MakeSameSize(valtype& vch1, valtype& vch2)
{
    // Lengthen the shorter one
    if (vch1.size() < vch2.size())
        // PATCH:
        // +unsigned char msb = vch1[vch1.size()-1];
        // +vch1[vch1.size()-1] &= 0x7f;
        //  vch1.resize(vch2.size(), 0);
        // +vch1[vch1.size()-1] = msb;
        vch1.resize(vch2.size(), 0);
    if (vch2.size() < vch1.size())
        // PATCH:
        // +unsigned char msb = vch2[vch2.size()-1];
        // +vch2[vch2.size()-1] &= 0x7f;
        //  vch2.resize(vch1.size(), 0);
        // +vch2[vch2.size()-1] = msb;
        vch2.resize(vch1.size(), 0);
}

//
// Script is a stack machine (like Forth) that evaluates a predicate
// returning a bool indicating valid or not.  There are no loops.
//
#define stacktop(i) (stack.at(stack.size() + (i)))
#define altstacktop(i) (altstack.at(altstack.size() + (i)))
static inline void popstack(vector<valtype>& stack)
{
    if (stack.empty())
        throw runtime_error("popstack() : stack empty");
    stack.pop_back();
}

const char* GetTxnOutputType(txnouttype t)
{
    // clang-format off
    switch (t)
    {
    case TX_NONSTANDARD: return "nonstandard";
    case TX_PUBKEY: return "pubkey";
    case TX_PUBKEYHASH: return "pubkeyhash";
    case TX_SCRIPTHASH: return "scripthash";
    case TX_MULTISIG: return "multisig";
    case TX_NULL_DATA: return "nulldata";
    case TX_COLDSTAKE: return "coldstake";
    case TX_POOLCOLDSTAKE: return "poolcoldstake";
    }
    return nullptr;
    // clang-format on
}

// clang-format off
const char* GetOpName(opcodetype opcode)
{
    switch (opcode)
    {
    // push value
    case OP_0                      : return "0";
    case OP_PUSHDATA1              : return "OP_PUSHDATA1";
    case OP_PUSHDATA2              : return "OP_PUSHDATA2";
    case OP_PUSHDATA4              : return "OP_PUSHDATA4";
    case OP_1NEGATE                : return "-1";
    case OP_RESERVED               : return "OP_RESERVED";
    case OP_1                      : return "1";
    case OP_2                      : return "2";
    case OP_3                      : return "3";
    case OP_4                      : return "4";
    case OP_5                      : return "5";
    case OP_6                      : return "6";
    case OP_7                      : return "7";
    case OP_8                      : return "8";
    case OP_9                      : return "9";
    case OP_10                     : return "10";
    case OP_11                     : return "11";
    case OP_12                     : return "12";
    case OP_13                     : return "13";
    case OP_14                     : return "14";
    case OP_15                     : return "15";
    case OP_16                     : return "16";

    // control
    case OP_NOP                    : return "OP_NOP";
    case OP_VER                    : return "OP_VER";
    case OP_IF                     : return "OP_IF";
    case OP_NOTIF                  : return "OP_NOTIF";
    case OP_VERIF                  : return "OP_VERIF";
    case OP_VERNOTIF               : return "OP_VERNOTIF";
    case OP_ELSE                   : return "OP_ELSE";
    case OP_ENDIF                  : return "OP_ENDIF";
    case OP_VERIFY                 : return "OP_VERIFY";
    case OP_RETURN                 : return "OP_RETURN";
    case OP_CHECKLOCKTIMEVERIFY    : return "OP_CHECKLOCKTIMEVERIFY";
    case OP_CHECKSEQUENCEVERIFY    : return "OP_CHECKSEQUENCEVERIFY"; //Currently still treated as NOP3 due to lack of BIP68 implementation

    // stack ops
    case OP_TOALTSTACK             : return "OP_TOALTSTACK";
    case OP_FROMALTSTACK           : return "OP_FROMALTSTACK";
    case OP_2DROP                  : return "OP_2DROP";
    case OP_2DUP                   : return "OP_2DUP";
    case OP_3DUP                   : return "OP_3DUP";
    case OP_2OVER                  : return "OP_2OVER";
    case OP_2ROT                   : return "OP_2ROT";
    case OP_2SWAP                  : return "OP_2SWAP";
    case OP_IFDUP                  : return "OP_IFDUP";
    case OP_DEPTH                  : return "OP_DEPTH";
    case OP_DROP                   : return "OP_DROP";
    case OP_DUP                    : return "OP_DUP";
    case OP_NIP                    : return "OP_NIP";
    case OP_OVER                   : return "OP_OVER";
    case OP_PICK                   : return "OP_PICK";
    case OP_ROLL                   : return "OP_ROLL";
    case OP_ROT                    : return "OP_ROT";
    case OP_SWAP                   : return "OP_SWAP";
    case OP_TUCK                   : return "OP_TUCK";

    // splice ops
    case OP_CAT                    : return "OP_CAT";
    case OP_SUBSTR                 : return "OP_SUBSTR";
    case OP_LEFT                   : return "OP_LEFT";
    case OP_RIGHT                  : return "OP_RIGHT";
    case OP_SIZE                   : return "OP_SIZE";

    // bit logic
    case OP_INVERT                 : return "OP_INVERT";
    case OP_AND                    : return "OP_AND";
    case OP_OR                     : return "OP_OR";
    case OP_XOR                    : return "OP_XOR";
    case OP_EQUAL                  : return "OP_EQUAL";
    case OP_EQUALVERIFY            : return "OP_EQUALVERIFY";
    case OP_RESERVED1              : return "OP_RESERVED1";
    case OP_RESERVED2              : return "OP_RESERVED2";

    // numeric
    case OP_1ADD                   : return "OP_1ADD";
    case OP_1SUB                   : return "OP_1SUB";
    case OP_2MUL                   : return "OP_2MUL";
    case OP_2DIV                   : return "OP_2DIV";
    case OP_NEGATE                 : return "OP_NEGATE";
    case OP_ABS                    : return "OP_ABS";
    case OP_NOT                    : return "OP_NOT";
    case OP_0NOTEQUAL              : return "OP_0NOTEQUAL";
    case OP_ADD                    : return "OP_ADD";
    case OP_SUB                    : return "OP_SUB";
    case OP_MUL                    : return "OP_MUL";
    case OP_DIV                    : return "OP_DIV";
    case OP_MOD                    : return "OP_MOD";
    case OP_LSHIFT                 : return "OP_LSHIFT";
    case OP_RSHIFT                 : return "OP_RSHIFT";
    case OP_BOOLAND                : return "OP_BOOLAND";
    case OP_BOOLOR                 : return "OP_BOOLOR";
    case OP_NUMEQUAL               : return "OP_NUMEQUAL";
    case OP_NUMEQUALVERIFY         : return "OP_NUMEQUALVERIFY";
    case OP_NUMNOTEQUAL            : return "OP_NUMNOTEQUAL";
    case OP_LESSTHAN               : return "OP_LESSTHAN";
    case OP_GREATERTHAN            : return "OP_GREATERTHAN";
    case OP_LESSTHANOREQUAL        : return "OP_LESSTHANOREQUAL";
    case OP_GREATERTHANOREQUAL     : return "OP_GREATERTHANOREQUAL";
    case OP_MIN                    : return "OP_MIN";
    case OP_MAX                    : return "OP_MAX";
    case OP_WITHIN                 : return "OP_WITHIN";

    // crypto
    case OP_RIPEMD160              : return "OP_RIPEMD160";
    case OP_SHA1                   : return "OP_SHA1";
    case OP_SHA256                 : return "OP_SHA256";
    case OP_HASH160                : return "OP_HASH160";
    case OP_HASH256                : return "OP_HASH256";
    case OP_CODESEPARATOR          : return "OP_CODESEPARATOR";
    case OP_CHECKSIG               : return "OP_CHECKSIG";
    case OP_CHECKSIGVERIFY         : return "OP_CHECKSIGVERIFY";
    case OP_CHECKMULTISIG          : return "OP_CHECKMULTISIG";
    case OP_CHECKMULTISIGVERIFY    : return "OP_CHECKMULTISIGVERIFY";

    // expanson
    case OP_NOP1                   : return "OP_NOP1";
    case OP_NOP4                   : return "OP_NOP4";
    case OP_NOP5                   : return "OP_NOP5";
    case OP_NOP6                   : return "OP_NOP6";
    case OP_NOP7                   : return "OP_NOP7";
    case OP_NOP8                   : return "OP_NOP8";
    case OP_NOP9                   : return "OP_NOP9";
    case OP_NOP10                  : return "OP_NOP10";

    case OP_SMALLDATA              : return "OP_SMALLDATA";

    case OP_INVALIDOPCODE          : return "OP_INVALIDOPCODE";
    case OP_CHECKCOLDSTAKEVERIFY   : return "OP_CHECKCOLDSTAKEVERIFY";
    case OP_CHECKPOOLCOLDSTAKEVERIFY   : return "OP_CHECKPOOLCOLDSTAKEVERIFY";

    // Note:
    //  The template matching params etc are defined in opcodetype enum
    //  as kind of implementation hack, they are *NOT* real opcodes.  If found in real
    //  Script, just let the default: case deal with them.

    default:
        return "OP_UNKNOWN";
    }
}
// clang-format on

bool IsCanonicalPubKey(const valtype& vchPubKey)
{
    if (vchPubKey.size() < 33)
        return NLog.error("Non-canonical public key: too short");
    if (vchPubKey[0] == 0x04) {
        if (vchPubKey.size() != 65)
            return NLog.error("Non-canonical public key: invalid length for uncompressed key");
    } else if (vchPubKey[0] == 0x02 || vchPubKey[0] == 0x03) {
        if (vchPubKey.size() != 33)
            return NLog.error("Non-canonical public key: invalid length for compressed key");
    } else {
        return NLog.error("Non-canonical public key: compressed nor uncompressed");
    }
    return true;
}

bool IsCanonicalSignature(const valtype& vchSig)
{
    // See https://bitcointalk.org/index.php?topic=8392.msg127623#msg127623
    // A canonical signature exists of: <30> <total len> <02> <len R> <R> <02> <len S> <S> <hashtype>
    // Where R and S are not negative (their first byte has its highest bit not set), and not
    // excessively padded (do not start with a 0 byte, unless an otherwise negative number follows,
    // in which case a single 0 byte is necessary and even required).
    if (vchSig.size() < 9)
        return NLog.error("Non-canonical signature: too short");
    if (vchSig.size() > 73)
        return NLog.error("Non-canonical signature: too long");
    if (vchSig[vchSig.size() - 1] & 0x7C)
        return NLog.error("Non-canonical signature: unknown hashtype byte");
    if (vchSig[0] != 0x30)
        return NLog.error("Non-canonical signature: wrong type");
    if (vchSig[1] != vchSig.size() - 3)
        return NLog.error("Non-canonical signature: wrong length marker");
    unsigned int nLenR = vchSig[3];
    if (5 + nLenR >= vchSig.size())
        return NLog.error("Non-canonical signature: S length misplaced");
    unsigned int nLenS = vchSig[5 + nLenR];
    if ((unsigned long)(nLenR + nLenS + 7) != vchSig.size())
        return NLog.error("Non-canonical signature: R+S length mismatch");

    const unsigned char* R = &vchSig[4];
    if (R[-2] != 0x02)
        return NLog.error("Non-canonical signature: R value type mismatch");
    if (nLenR == 0)
        return NLog.error("Non-canonical signature: R length is zero");
    if (R[0] & 0x80)
        return NLog.error("Non-canonical signature: R value negative");
    if (nLenR > 1 && (R[0] == 0x00) && !(R[1] & 0x80))
        return NLog.error("Non-canonical signature: R value excessively padded");

    const unsigned char* S = &vchSig[6 + nLenR];
    if (S[-2] != 0x02)
        return NLog.error("Non-canonical signature: S value type mismatch");
    if (nLenS == 0)
        return NLog.error("Non-canonical signature: S length is zero");
    if (S[0] & 0x80)
        return NLog.error("Non-canonical signature: S value negative");
    if (nLenS > 1 && (S[0] == 0x00) && !(S[1] & 0x80))
        return NLog.error("Non-canonical signature: S value excessively padded");

    // If the S value is above the order of the curve divided by two, its
    // complement modulo the order could have been used instead, which is
    // one byte shorter when encoded correctly.
    if (!CKey::CheckSignatureElement(S, nLenS, true))
        return NLog.error("Non-canonical signature: S value is unnecessarily high");

    return true;
}

bool CheckLockTime(const int64_t& nLockTime, const CTransaction& txTo, unsigned int nIn)
{
    // There are two kinds of nLockTime: lock-by-blockheight
    // and lock-by-blocktime, distinguished by whether
    // nLockTime < LOCKTIME_THRESHOLD.
    //
    // We want to compare apples to apples, so fail the script
    // unless the type of nLockTime being tested is the same as
    // the nLockTime in the transaction.
    if (!((txTo.nLockTime < LOCKTIME_THRESHOLD && nLockTime < LOCKTIME_THRESHOLD) ||
          (txTo.nLockTime >= LOCKTIME_THRESHOLD && nLockTime >= LOCKTIME_THRESHOLD)))
        return false;

    // Now that we know we're comparing apples-to-apples, the
    // comparison is a simple numeric one.
    if (nLockTime > (int64_t)txTo.nLockTime)
        return false;

    // Finally the nLockTime feature can be disabled and thus
    // CHECKLOCKTIMEVERIFY bypassed if every txin has been
    // finalized by setting nSequence to maxint. The
    // transaction would be allowed into the blockchain, making
    // the opcode ineffective.
    //
    // Testing if this vin is not final is sufficient to
    // prevent this condition. Alternatively we could test all
    // inputs, but testing just this input minimizes the data
    // required to prove correct CHECKLOCKTIMEVERIFY execution.
    if (SEQUENCE_FINAL == txTo.vin[nIn].nSequence)
        return false;

    return true;
}

bool CheckSequence(const int64_t& nSequence, const CTransaction& txTo, unsigned int nIn)
{
    // Function currently not used (NOP) due to lack of BIP68 implementation

    // Relative lock times are supported by comparing the passed
    // in operand to the sequence number of the input.
    const int64_t txToSequence = (int64_t)txTo.vin[nIn].nSequence;

    // Sequence numbers with their most significant bit set are not
    // consensus constrained. Testing that the transaction's sequence
    // number do not have this bit set prevents using this property
    // to get around a CHECKSEQUENCEVERIFY check.
    if (txToSequence & SEQUENCE_LOCKTIME_DISABLE_FLAG)
        return false;

    // Mask off any bits that do not have consensus-enforced meaning
    // before doing the integer comparisons
    const uint32_t nLockTimeMask      = SEQUENCE_LOCKTIME_TYPE_FLAG | SEQUENCE_LOCKTIME_MASK;
    const int64_t  txToSequenceMasked = txToSequence & nLockTimeMask;
    const int64_t  nSequenceMasked    = nSequence & nLockTimeMask;

    // There are two kinds of nSequence: lock-by-blockheight
    // and lock-by-blocktime, distinguished by whether
    // nSequenceMasked < CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG.
    //
    // We want to compare apples to apples, so fail the script
    // unless the type of nSequenceMasked being tested is the same as
    // the nSequenceMasked in the transaction.
    if (!((txToSequenceMasked < SEQUENCE_LOCKTIME_TYPE_FLAG &&
           nSequenceMasked < SEQUENCE_LOCKTIME_TYPE_FLAG) ||
          (txToSequenceMasked >= SEQUENCE_LOCKTIME_TYPE_FLAG &&
           nSequenceMasked >= SEQUENCE_LOCKTIME_TYPE_FLAG))) {
        return false;
    }

    // Now that we know we're comparing apples-to-apples, the
    // comparison is a simple numeric one.
    if (nSequenceMasked > txToSequenceMasked)
        return false;

    return true;
}

Result<void, ScriptError> EvalScript(vector<vector<unsigned char>>& stack, const CScript& script,
                                     const CTransaction& txTo, unsigned int nIn, bool fStrictEncodings,
                                     int nHashType, ScriptError* serror)
{
    CAutoBN_CTX             pctx;
    CScript::const_iterator pc             = script.begin();
    CScript::const_iterator pend           = script.end();
    CScript::const_iterator pbegincodehash = script.begin();
    opcodetype              opcode;
    valtype                 vchPushValue;
    vector<bool>            vfExec;
    vector<valtype>         altstack;
    set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);
    if (script.size() > MAX_SCRIPT_SIZE)
        return Err(SCRIPT_ERR_SCRIPT_SIZE);
    int nOpCount = 0;

    try {
        while (pc < pend) {
            bool fExec = !count(vfExec.begin(), vfExec.end(), false);

            //
            // Read instruction
            //
            if (!script.GetOp(pc, opcode, vchPushValue))
                return Err(SCRIPT_ERR_BAD_OPCODE);
            if (vchPushValue.size() > MAX_SCRIPT_ELEMENT_SIZE)
                return Err(SCRIPT_ERR_PUSH_SIZE);
            if (opcode > OP_16 && ++nOpCount > 201)
                return Err(SCRIPT_ERR_OP_COUNT);

            // clang-format off
            if (opcode == OP_CAT ||
                opcode == OP_SUBSTR ||
                opcode == OP_LEFT ||
                opcode == OP_RIGHT ||
                opcode == OP_INVERT ||
                opcode == OP_AND ||
                opcode == OP_OR ||
                opcode == OP_XOR ||
                opcode == OP_2MUL ||
                opcode == OP_2DIV ||
                opcode == OP_MUL ||
                opcode == OP_DIV ||
                opcode == OP_MOD ||
                opcode == OP_LSHIFT ||
                opcode == OP_RSHIFT)
                return Err(SCRIPT_ERR_DISABLED_OPCODE); // Disabled opcodes.
            // clang-format on

            if (fExec && 0 <= opcode && opcode <= OP_PUSHDATA4)
                stack.push_back(vchPushValue);
            else if (fExec || (OP_IF <= opcode && opcode <= OP_ENDIF))
                switch (opcode) {
                //
                // Push value
                //
                case OP_1NEGATE:
                case OP_1:
                case OP_2:
                case OP_3:
                case OP_4:
                case OP_5:
                case OP_6:
                case OP_7:
                case OP_8:
                case OP_9:
                case OP_10:
                case OP_11:
                case OP_12:
                case OP_13:
                case OP_14:
                case OP_15:
                case OP_16: {
                    // ( -- value)
                    CBigNum bn((int)opcode - (int)(OP_1 - 1));
                    stack.push_back(bn.getvch());
                } break;

                //
                // Control
                //
                case OP_NOP:
                case OP_NOP1:
                case OP_NOP4:
                case OP_NOP5:
                case OP_NOP6:
                case OP_NOP7:
                case OP_NOP8:
                case OP_NOP9:
                case OP_NOP10:
                    break;

                case OP_IF:
                case OP_NOTIF: {
                    // <expression> if [statements] [else [statements]] endif
                    bool fValue = false;
                    if (fExec) {
                        if (stack.size() < 1)
                            return Err(SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                        valtype& vch = stacktop(-1);
                        fValue       = CastToBool(vch);
                        if (opcode == OP_NOTIF)
                            fValue = !fValue;
                        popstack(stack);
                    }
                    vfExec.push_back(fValue);
                } break;

                case OP_ELSE: {
                    if (vfExec.empty())
                        return Err(SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                    vfExec.back() = !vfExec.back();
                } break;

                case OP_ENDIF: {
                    if (vfExec.empty())
                        return Err(SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                    vfExec.pop_back();
                } break;

                case OP_VERIFY: {
                    // (true -- ) or
                    // (false -- false) and return
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    bool fValue = CastToBool(stacktop(-1));
                    if (fValue)
                        popstack(stack);
                    else
                        return Err(SCRIPT_ERR_VERIFY);
                } break;

                case OP_RETURN: {
                    return Err(SCRIPT_ERR_OP_RETURN);
                } break;

                case OP_CHECKLOCKTIMEVERIFY: {
                    // CHECKLOCKTIMEVERIFY
                    //
                    // (nLockTime -- nLockTime)
                    if (txTo.nTime < CHECKLOCKTIME_VERIFY_SWITCH_TIME) {
                        // treat as a NOP2 if not enabled yet by timestamp
                        break;
                    }

                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);

                    CBigNum nLockTime = CastToBigNum(stacktop(-1));

                    // In the rare event that the argument may be < 0 due to
                    // some arithmetic being done first, you can always use
                    // 0 MAX CHECKLOCKTIMEVERIFY.
                    if (nLockTime < 0)
                        return Err(SCRIPT_ERR_NEGATIVE_LOCKTIME);

                    // Actually compare the specified lock time with the transaction.
                    if (!CheckLockTime(nLockTime.getuint64(), txTo, nIn))
                        return Err(SCRIPT_ERR_UNSATISFIED_LOCKTIME);

                    break;
                }

                case OP_CHECKSEQUENCEVERIFY: {
                    // Not implemented
                    // treat as a NOP3 for now as BIP68 is not implemented in Neblio
                    break;

                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);

                    // nSequence, like nLockTime, is a 32-bit unsigned integer
                    // field. See the comment in CHECKLOCKTIMEVERIFY regarding
                    // 5-byte numeric operands.
                    CBigNum nSequence = CastToBigNum(stacktop(-1));

                    // In the rare event that the argument may be < 0 due to
                    // some arithmetic being done first, you can always use
                    // 0 MAX CHECKSEQUENCEVERIFY.
                    if (nSequence < 0)
                        return Err(SCRIPT_ERR_NEGATIVE_LOCKTIME);

                    // To provide for future soft-fork extensibility, if the
                    // operand has the disabled lock-time flag set,
                    // CHECKSEQUENCEVERIFY behaves as a NOP.
                    if ((nSequence.getint() & SEQUENCE_LOCKTIME_DISABLE_FLAG) != 0)
                        break;

                    // Compare the specified sequence number with the input.
                    if (!CheckSequence(nSequence.getuint64(), txTo, nIn))
                        return Err(SCRIPT_ERR_UNSATISFIED_LOCKTIME);

                    break;
                }

                //
                // Stack ops
                //
                case OP_TOALTSTACK: {
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    altstack.push_back(stacktop(-1));
                    popstack(stack);
                } break;

                case OP_FROMALTSTACK: {
                    if (altstack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_ALTSTACK_OPERATION);
                    stack.push_back(altstacktop(-1));
                    popstack(altstack);
                } break;

                case OP_2DROP: {
                    // (x1 x2 -- )
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    popstack(stack);
                    popstack(stack);
                } break;

                case OP_2DUP: {
                    // (x1 x2 -- x1 x2 x1 x2)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-2);
                    valtype vch2 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                } break;

                case OP_3DUP: {
                    // (x1 x2 x3 -- x1 x2 x3 x1 x2 x3)
                    if (stack.size() < 3)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-3);
                    valtype vch2 = stacktop(-2);
                    valtype vch3 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                    stack.push_back(vch3);
                } break;

                case OP_2OVER: {
                    // (x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2)
                    if (stack.size() < 4)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-4);
                    valtype vch2 = stacktop(-3);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                } break;

                case OP_2ROT: {
                    // (x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2)
                    if (stack.size() < 6)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-6);
                    valtype vch2 = stacktop(-5);
                    stack.erase(stack.end() - 6, stack.end() - 4);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                } break;

                case OP_2SWAP: {
                    // (x1 x2 x3 x4 -- x3 x4 x1 x2)
                    if (stack.size() < 4)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-4), stacktop(-2));
                    swap(stacktop(-3), stacktop(-1));
                } break;

                case OP_IFDUP: {
                    // (x - 0 | x x)
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    if (CastToBool(vch))
                        stack.push_back(vch);
                } break;

                case OP_DEPTH: {
                    // -- stacksize
                    CBigNum bn(stack.size());
                    stack.push_back(bn.getvch());
                } break;

                case OP_DROP: {
                    // (x -- )
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    popstack(stack);
                } break;

                case OP_DUP: {
                    // (x -- x x)
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    stack.push_back(vch);
                } break;

                case OP_NIP: {
                    // (x1 x2 -- x2)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    stack.erase(stack.end() - 2);
                } break;

                case OP_OVER: {
                    // (x1 x2 -- x1 x2 x1)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-2);
                    stack.push_back(vch);
                } break;

                case OP_PICK:
                case OP_ROLL: {
                    // (xn ... x2 x1 x0 n - xn ... x2 x1 x0 xn)
                    // (xn ... x2 x1 x0 n - ... x2 x1 x0 xn)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    int n = CastToBigNum(stacktop(-1)).getint();
                    popstack(stack);
                    if (n < 0 || n >= (int)stack.size())
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-n - 1);
                    if (opcode == OP_ROLL)
                        stack.erase(stack.end() - n - 1);
                    stack.push_back(vch);
                } break;

                case OP_ROT: {
                    // (x1 x2 x3 -- x2 x3 x1)
                    //  x2 x1 x3  after first swap
                    //  x2 x3 x1  after second swap
                    if (stack.size() < 3)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-3), stacktop(-2));
                    swap(stacktop(-2), stacktop(-1));
                } break;

                case OP_SWAP: {
                    // (x1 x2 -- x2 x1)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-2), stacktop(-1));
                } break;

                case OP_TUCK: {
                    // (x1 x2 -- x2 x1 x2)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    stack.insert(stack.end() - 2, vch);
                } break;

                //
                // Splice ops
                //
                case OP_CAT: {
                    // (x1 x2 -- out)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch1 = stacktop(-2);
                    valtype& vch2 = stacktop(-1);
                    vch1.insert(vch1.end(), vch2.begin(), vch2.end());
                    popstack(stack);
                    if (stacktop(-1).size() > MAX_SCRIPT_ELEMENT_SIZE)
                        return Err(SCRIPT_ERR_MAX_ELEMENT_SIZE);
                } break;

                case OP_SUBSTR: {
                    // (in begin size -- out)
                    if (stack.size() < 3)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch    = stacktop(-3);
                    int      nBegin = CastToBigNum(stacktop(-2)).getint();
                    int      nEnd   = nBegin + CastToBigNum(stacktop(-1)).getint();
                    if (nBegin < 0 || nEnd < nBegin)
                        return Err(SCRIPT_ERR_ELEMENT_SIZE);
                    if (nBegin > (int)vch.size())
                        nBegin = vch.size();
                    if (nEnd > (int)vch.size())
                        nEnd = vch.size();
                    vch.erase(vch.begin() + nEnd, vch.end());
                    vch.erase(vch.begin(), vch.begin() + nBegin);
                    popstack(stack);
                    popstack(stack);
                } break;

                case OP_LEFT:
                case OP_RIGHT: {
                    // (in size -- out)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch   = stacktop(-2);
                    int      nSize = CastToBigNum(stacktop(-1)).getint();
                    if (nSize < 0)
                        return Err(SCRIPT_ERR_ELEMENT_SIZE);
                    if (nSize > (int)vch.size())
                        nSize = vch.size();
                    if (opcode == OP_LEFT)
                        vch.erase(vch.begin() + nSize, vch.end());
                    else
                        vch.erase(vch.begin(), vch.end() - nSize);
                    popstack(stack);
                } break;

                case OP_SIZE: {
                    // (in -- in size)
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CBigNum bn(stacktop(-1).size());
                    stack.push_back(bn.getvch());
                } break;

                //
                // Bitwise logic
                //
                case OP_INVERT: {
                    // (in - out)
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch = stacktop(-1);
                    for (unsigned int i = 0; i < vch.size(); i++)
                        vch[i] = ~vch[i];
                } break;

                //
                // WARNING: These disabled opcodes exhibit unexpected behavior
                // when used on signed integers due to a bug in MakeSameSize()
                // [see definition of MakeSameSize() above].
                //
                case OP_AND:
                case OP_OR:
                case OP_XOR: {
                    // (x1 x2 - out)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch1 = stacktop(-2);
                    valtype& vch2 = stacktop(-1);
                    MakeSameSize(vch1, vch2); // <-- NOT SAFE FOR SIGNED VALUES
                    if (opcode == OP_AND) {
                        for (unsigned int i = 0; i < vch1.size(); i++)
                            vch1[i] &= vch2[i];
                    } else if (opcode == OP_OR) {
                        for (unsigned int i = 0; i < vch1.size(); i++)
                            vch1[i] |= vch2[i];
                    } else if (opcode == OP_XOR) {
                        for (unsigned int i = 0; i < vch1.size(); i++)
                            vch1[i] ^= vch2[i];
                    }
                    popstack(stack);
                } break;

                case OP_EQUAL:
                case OP_EQUALVERIFY:
                    // case OP_NOTEQUAL: // use OP_NUMNOTEQUAL
                    {
                        // (x1 x2 - bool)
                        if (stack.size() < 2)
                            return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                        valtype& vch1   = stacktop(-2);
                        valtype& vch2   = stacktop(-1);
                        bool     fEqual = (vch1 == vch2);
                        // OP_NOTEQUAL is disabled because it would be too easy to say
                        // something like n != 1 and have some wiseguy pass in 1 with extra
                        // zero bytes after it (numerically, 0x01 == 0x0001 == 0x000001)
                        // if (opcode == OP_NOTEQUAL)
                        //    fEqual = !fEqual;
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fEqual ? vchTrue : vchFalse);
                        if (opcode == OP_EQUALVERIFY) {
                            if (fEqual)
                                popstack(stack);
                            else
                                return Err(SCRIPT_ERR_EQUALVERIFY);
                        }
                    }
                    break;

                //
                // Numeric
                //
                case OP_1ADD:
                case OP_1SUB:
                case OP_2MUL:
                case OP_2DIV:
                case OP_NEGATE:
                case OP_ABS:
                case OP_NOT:
                case OP_0NOTEQUAL: {
                    // (in -- out)
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CBigNum bn = CastToBigNum(stacktop(-1));
                    // clang-format off
                    switch (opcode)
                    {
                    case OP_1ADD:       bn += bnOne; break;
                    case OP_1SUB:       bn -= bnOne; break;
                    case OP_2MUL:       bn <<= 1; break;
                    case OP_2DIV:       bn >>= 1; break;
                    case OP_NEGATE:     bn = -bn; break;
                    case OP_ABS:        if (bn < bnZero) bn = -bn; break;
                    case OP_NOT:        bn = (bn == bnZero); break;
                    case OP_0NOTEQUAL:  bn = (bn != bnZero); break;
                    default:            assert(!"invalid opcode"); break;
                    }
                    // clang-format on
                    popstack(stack);
                    stack.push_back(bn.getvch());
                } break;

                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_MOD:
                case OP_LSHIFT:
                case OP_RSHIFT:
                case OP_BOOLAND:
                case OP_BOOLOR:
                case OP_NUMEQUAL:
                case OP_NUMEQUALVERIFY:
                case OP_NUMNOTEQUAL:
                case OP_LESSTHAN:
                case OP_GREATERTHAN:
                case OP_LESSTHANOREQUAL:
                case OP_GREATERTHANOREQUAL:
                case OP_MIN:
                case OP_MAX: {
                    // (x1 x2 -- out)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CBigNum bn1 = CastToBigNum(stacktop(-2));
                    CBigNum bn2 = CastToBigNum(stacktop(-1));
                    CBigNum bn;
                    switch (opcode) {
                    case OP_ADD:
                        bn = bn1 + bn2;
                        break;

                    case OP_SUB:
                        bn = bn1 - bn2;
                        break;

                    case OP_MUL:
                        if (!BN_mul(bn.get_raw(), bn1.get_raw(), bn2.get_raw(), pctx))
                            return Err(SCRIPT_ERR_ARITHMETIC_OP_FAILED);
                        break;

                    case OP_DIV:
                        if (!BN_div(bn.get_raw(), NULL, bn1.get_raw(), bn2.get_raw(), pctx))
                            return Err(SCRIPT_ERR_ARITHMETIC_OP_FAILED);
                        break;

                    case OP_MOD:
                        if (!BN_mod(bn.get_raw(), bn1.get_raw(), bn2.get_raw(), pctx))
                            return Err(SCRIPT_ERR_ARITHMETIC_OP_FAILED);
                        break;

                    case OP_LSHIFT:
                        if (bn2 < bnZero || bn2 > CBigNum(2048))
                            return Err(SCRIPT_ERR_ARITHMETIC_OP_FAILED);
                        bn = bn1 << bn2.getulong();
                        break;

                    case OP_RSHIFT:
                        if (bn2 < bnZero || bn2 > CBigNum(2048))
                            return Err(SCRIPT_ERR_ARITHMETIC_OP_FAILED);
                        bn = bn1 >> bn2.getulong();
                        break;
                    // clang-format off
                    case OP_BOOLAND:             bn = (bn1 != bnZero && bn2 != bnZero); break;
                    case OP_BOOLOR:              bn = (bn1 != bnZero || bn2 != bnZero); break;
                    case OP_NUMEQUAL:            bn = (bn1 == bn2); break;
                    case OP_NUMEQUALVERIFY:      bn = (bn1 == bn2); break;
                    case OP_NUMNOTEQUAL:         bn = (bn1 != bn2); break;
                    case OP_LESSTHAN:            bn = (bn1 < bn2); break;
                    case OP_GREATERTHAN:         bn = (bn1 > bn2); break;
                    case OP_LESSTHANOREQUAL:     bn = (bn1 <= bn2); break;
                    case OP_GREATERTHANOREQUAL:  bn = (bn1 >= bn2); break;
                    case OP_MIN:                 bn = (bn1 < bn2 ? bn1 : bn2); break;
                    case OP_MAX:                 bn = (bn1 > bn2 ? bn1 : bn2); break;
                    // clang-format on
                    default:
                        assert(!"invalid opcode");
                        break;
                    }
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(bn.getvch());

                    if (opcode == OP_NUMEQUALVERIFY) {
                        if (CastToBool(stacktop(-1)))
                            popstack(stack);
                        else
                            return Err(SCRIPT_ERR_NUMEQUALVERIFY);
                    }
                } break;

                case OP_WITHIN: {
                    // (x min max -- out)
                    if (stack.size() < 3)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CBigNum bn1    = CastToBigNum(stacktop(-3));
                    CBigNum bn2    = CastToBigNum(stacktop(-2));
                    CBigNum bn3    = CastToBigNum(stacktop(-1));
                    bool    fValue = (bn2 <= bn1 && bn1 < bn3);
                    popstack(stack);
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fValue ? vchTrue : vchFalse);
                } break;

                //
                // Crypto
                //
                case OP_RIPEMD160:
                case OP_SHA1:
                case OP_SHA256:
                case OP_HASH160:
                case OP_HASH256: {
                    // (in -- hash)
                    if (stack.size() < 1)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch = stacktop(-1);
                    valtype  vchHash(
                        (opcode == OP_RIPEMD160 || opcode == OP_SHA1 || opcode == OP_HASH160) ? 20 : 32);
                    if (opcode == OP_RIPEMD160)
                        RIPEMD160(&vch[0], vch.size(), &vchHash[0]);
                    else if (opcode == OP_SHA1)
                        SHA1(&vch[0], vch.size(), &vchHash[0]);
                    else if (opcode == OP_SHA256)
                        SHA256(&vch[0], vch.size(), &vchHash[0]);
                    else if (opcode == OP_HASH160) {
                        uint160 hash160 = Hash160(vch);
                        memcpy(&vchHash[0], &hash160, sizeof(hash160));
                    } else if (opcode == OP_HASH256) {
                        uint256 hash = Hash(vch.begin(), vch.end());
                        memcpy(&vchHash[0], &hash, sizeof(hash));
                    }
                    popstack(stack);
                    stack.push_back(vchHash);
                } break;

                case OP_CODESEPARATOR: {
                    // Hash starts after the code separator
                    pbegincodehash = pc;
                } break;

                case OP_CHECKSIG:
                case OP_CHECKSIGVERIFY: {
                    // (sig pubkey -- bool)
                    if (stack.size() < 2)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);

                    valtype& vchSig    = stacktop(-2);
                    valtype& vchPubKey = stacktop(-1);

                    // Subset of script starting at the most recent codeseparator
                    CScript scriptCode(pbegincodehash, pend);

                    // Drop the signature, since there's no way for a signature to sign itself
                    scriptCode.FindAndDelete(CScript(vchSig));

                    bool fSuccess = (!fStrictEncodings ||
                                     (IsCanonicalSignature(vchSig) && IsCanonicalPubKey(vchPubKey)));
                    if (fSuccess)
                        fSuccess = CheckSig(vchSig, vchPubKey, scriptCode, txTo, nIn, nHashType);

                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fSuccess ? vchTrue : vchFalse);
                    if (opcode == OP_CHECKSIGVERIFY) {
                        if (fSuccess)
                            popstack(stack);
                        else
                            return Err(SCRIPT_ERR_OP_CHECKSIGVERIFY_FAILED);
                    }
                } break;

                case OP_CHECKMULTISIG:
                case OP_CHECKMULTISIGVERIFY: {
                    // ([sig ...] num_of_signatures [pubkey ...] num_of_pubkeys -- bool)

                    int i = 1;
                    if ((int)stack.size() < i)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);

                    int nKeysCount = CastToBigNum(stacktop(-i)).getint();
                    if (nKeysCount < 0 || nKeysCount > 20)
                        return Err(SCRIPT_ERR_PUBKEY_COUNT);
                    nOpCount += nKeysCount;
                    if (nOpCount > 201)
                        return Err(SCRIPT_ERR_OP_COUNT);
                    int ikey = ++i;
                    i += nKeysCount;
                    if ((int)stack.size() < i)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);

                    int nSigsCount = CastToBigNum(stacktop(-i)).getint();
                    if (nSigsCount < 0 || nSigsCount > nKeysCount)
                        return Err(SCRIPT_ERR_SIG_COUNT);
                    int isig = ++i;
                    i += nSigsCount;
                    if ((int)stack.size() < i)
                        return Err(SCRIPT_ERR_INVALID_STACK_OPERATION);

                    // Subset of script starting at the most recent codeseparator
                    CScript scriptCode(pbegincodehash, pend);

                    // Drop the signatures, since there's no way for a signature to sign itself
                    for (int k = 0; k < nSigsCount; k++) {
                        valtype& vchSig = stacktop(-isig - k);
                        scriptCode.FindAndDelete(CScript(vchSig));
                    }

                    bool fSuccess = true;
                    while (fSuccess && nSigsCount > 0) {
                        valtype& vchSig    = stacktop(-isig);
                        valtype& vchPubKey = stacktop(-ikey);

                        // Check signature
                        bool fOk = (!fStrictEncodings ||
                                    (IsCanonicalSignature(vchSig) && IsCanonicalPubKey(vchPubKey)));
                        if (fOk)
                            fOk = CheckSig(vchSig, vchPubKey, scriptCode, txTo, nIn, nHashType);

                        if (fOk) {
                            isig++;
                            nSigsCount--;
                        }
                        ikey++;
                        nKeysCount--;

                        // If there are more signatures left than keys left,
                        // then too many signatures have failed
                        if (nSigsCount > nKeysCount)
                            fSuccess = false;
                    }

                    while (i-- > 0)
                        popstack(stack);
                    stack.push_back(fSuccess ? vchTrue : vchFalse);

                    if (opcode == OP_CHECKMULTISIGVERIFY) {
                        if (fSuccess)
                            popstack(stack);
                        else
                            return Err(SCRIPT_ERR_CHECKMULTISIGVERIFY);
                    }
                } break;
                case OP_CHECKCOLDSTAKEVERIFY: {
                    // check it is used in a valid cold stake transaction.
                    if (!txTo.CheckColdStake(script)) {
                        return Err(SCRIPT_ERR_CHECKCOLDSTAKEVERIFY);
                    }
                } break;
                case OP_CHECKPOOLCOLDSTAKEVERIFY: {
                    // check it is used in a valid cold stake transaction.
                    if (!txTo.CheckPoolColdStake(script)) {
                        return Err(SCRIPT_ERR_CHECKCOLDSTAKEVERIFY);
                    }
                } break;

                default:
                    return Err(SCRIPT_ERR_BAD_OPCODE);
                }

            // Size limits
            if (stack.size() + altstack.size() > 1000)
                return Err(SCRIPT_ERR_STACK_SIZE);
        }
    } catch (...) {
        return Err(SCRIPT_ERR_UNKNOWN_ERROR);
    }

    if (!vfExec.empty())
        return Err(SCRIPT_ERR_UNBALANCED_CONDITIONAL);

    return Ok();
}

//////////////////////////

uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType)
{
    if (nIn >= txTo.vin.size()) {
        NLog.write(b_sev::err, "ERROR: SignatureHash() : nIn={} out of range", nIn);
        return 1;
    }
    CTransaction txTmp(txTo);

    // In case concatenating two scripts ends up with two codeseparators,
    // or an extra one at the end, this prevents all those possible incompatibilities.
    scriptCode.FindAndDelete(CScript(OP_CODESEPARATOR));

    // Blank out other inputs' signatures
    for (unsigned int i = 0; i < txTmp.vin.size(); i++)
        txTmp.vin[i].scriptSig = CScript();
    txTmp.vin[nIn].scriptSig = scriptCode;

    // Blank out some of the outputs
    if ((nHashType & 0x1f) == SIGHASH_NONE) {
        // Wildcard payee
        txTmp.vout.clear();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            if (i != nIn)
                txTmp.vin[i].nSequence = 0;
    } else if ((nHashType & 0x1f) == SIGHASH_SINGLE) {
        // Only lock-in the txout payee at same index as txin
        unsigned int nOut = nIn;
        if (nOut >= txTmp.vout.size()) {
            NLog.write(b_sev::err, "ERROR: SignatureHash() : nOut={} out of range", nOut);
            return 1;
        }
        txTmp.vout.resize(nOut + 1);
        for (unsigned int i = 0; i < nOut; i++)
            txTmp.vout[i].SetNull();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            if (i != nIn)
                txTmp.vin[i].nSequence = 0;
    }

    // Blank out other inputs completely, not recommended for open transactions
    if (nHashType & SIGHASH_ANYONECANPAY) {
        txTmp.vin[0] = txTmp.vin[nIn];
        txTmp.vin.resize(1);
    }

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
}

// Valid signature cache, to avoid doing expensive ECDSA signature checking
// twice for every transaction (once when accepted into memory pool, and
// again when accepted into the block chain)

class CSignatureCache
{
private:
    // sigdata_type is (signature hash, signature, public key):
    typedef boost::tuple<uint256, std::vector<unsigned char>, std::vector<unsigned char>> sigdata_type;
    std::set<sigdata_type>                                                                setValid;
    CCriticalSection                                                                      cs_sigcache;

public:
    bool Get(uint256 hash, const std::vector<unsigned char>& vchSig,
             const std::vector<unsigned char>& pubKey)
    {
        LOCK(cs_sigcache);

        sigdata_type                     k(hash, vchSig, pubKey);
        std::set<sigdata_type>::iterator mi = setValid.find(k);
        if (mi != setValid.end())
            return true;
        return false;
    }

    void Set(uint256 hash, const std::vector<unsigned char>& vchSig,
             const std::vector<unsigned char>& pubKey)
    {
        // DoS prevention: limit cache size to less than 10MB
        // (~200 bytes per cache entry times 50,000 entries)
        // Since there are a maximum of 20,000 signature operations per block
        // 50,000 is a reasonable default.
        int64_t nMaxCacheSize = GetArg("-maxsigcachesize", 50000);
        if (nMaxCacheSize <= 0)
            return;

        LOCK(cs_sigcache);

        while (static_cast<int64_t>(setValid.size()) > nMaxCacheSize) {
            // Evict a random entry. Random because that helps
            // foil would-be DoS attackers who might try to pre-generate
            // and re-use a set of valid signatures just-slightly-greater
            // than our cache size.
            uint256                          randomHash = GetRandHash();
            std::vector<unsigned char>       unused;
            std::set<sigdata_type>::iterator it =
                setValid.lower_bound(sigdata_type(randomHash, unused, unused));
            if (it == setValid.end())
                it = setValid.begin();
            setValid.erase(*it);
        }

        sigdata_type k(hash, vchSig, pubKey);
        setValid.insert(k);
    }
};

bool CheckSig(vector<unsigned char> vchSig, vector<unsigned char> vchPubKey, CScript scriptCode,
              const CTransaction& txTo, unsigned int nIn, int nHashType)
{
    static CSignatureCache signatureCache;

    // Hash type is one byte tacked on to the end of the signature
    if (vchSig.empty())
        return false;
    if (nHashType == 0)
        nHashType = vchSig.back();
    else if (nHashType != vchSig.back())
        return false;
    vchSig.pop_back();

    uint256 sighash = SignatureHash(scriptCode, txTo, nIn, nHashType);

    if (signatureCache.Get(sighash, vchSig, vchPubKey))
        return true;

    CKey key;
    if (!key.SetPubKey(vchPubKey))
        return false;

    if (!key.Verify(sighash, vchSig))
        return false;

    signatureCache.Set(sighash, vchSig, vchPubKey);
    return true;
}

//
// Return public keys or hashes from scriptPubKey, for 'standard' transaction types.
//
bool Solver(const ITxDB& txdb, const CScript& scriptPubKey, txnouttype& typeRet,
            vector<vector<unsigned char>>& vSolutionsRet)
{
    // Templates
    static multimap<txnouttype, CScript> mTemplates;
    if (mTemplates.empty()) {
        // Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(make_pair(TX_PUBKEY, CScript() << OP_PUBKEY << OP_CHECKSIG));

        // Bitcoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mTemplates.insert(make_pair(TX_PUBKEYHASH, CScript() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH
                                                             << OP_EQUALVERIFY << OP_CHECKSIG));

        // Sender provides N pubkeys, receivers provides M signatures
        mTemplates.insert(make_pair(TX_MULTISIG, CScript() << OP_SMALLINTEGER << OP_PUBKEYS
                                                           << OP_SMALLINTEGER << OP_CHECKMULTISIG));

        // Empty, provably prunable, data-carrying output
        mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN << OP_SMALLDATA));

        // Cold Staking: sender provides P2CS scripts, receiver provides signature, staking-flag and
        // pubkey
        mTemplates.insert(std::make_pair(
            TX_COLDSTAKE, CScript() << OP_DUP << OP_HASH160 << OP_ROT << OP_IF << OP_CHECKCOLDSTAKEVERIFY
                                    << OP_PUBKEYHASH << OP_ELSE << OP_PUBKEYHASH << OP_ENDIF
                                    << OP_EQUALVERIFY << OP_CHECKSIG));

        // Cold Staking: sender provides P2CSP scripts, receiver provides signature, staking-flag and
        // pubkey
        mTemplates.insert(std::make_pair(
            TX_POOLCOLDSTAKE, CScript() << OP_DUP << OP_HASH160 << OP_ROT << OP_IF
                                        << OP_CHECKPOOLCOLDSTAKEVERIFY << OP_PUBKEYHASH << OP_ELSE
                                        << OP_PUBKEYHASH << OP_ENDIF << OP_EQUALVERIFY << OP_CHECKSIG));
    }

    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (scriptPubKey.IsPayToScriptHash()) {
        typeRet = TX_SCRIPTHASH;
        vector<unsigned char> hashBytes(scriptPubKey.begin() + 2, scriptPubKey.begin() + 22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }

    // Scan templates
    const CScript& script1 = scriptPubKey;
    for (const PAIRTYPE(const txnouttype, CScript) & tplate : mTemplates) {
        const CScript& script2 = tplate.second;
        vSolutionsRet.clear();

        opcodetype            opcode1, opcode2;
        vector<unsigned char> vch1, vch2;

        // Compare
        CScript::const_iterator pc1 = script1.begin();
        CScript::const_iterator pc2 = script2.begin();
        while (true) {
            if (pc1 == script1.end() && pc2 == script2.end()) {
                // Found a match
                typeRet = tplate.first;
                if (typeRet == TX_MULTISIG) {
                    // Additional checks for TX_MULTISIG:
                    unsigned char m = vSolutionsRet.front()[0];
                    unsigned char n = vSolutionsRet.back()[0];
                    if (m < 1 || n < 1 || m > n || vSolutionsRet.size() - 2 != n)
                        return false;
                }
                return true;
            }
            if (!script1.GetOp(pc1, opcode1, vch1))
                break;
            if (!script2.GetOp(pc2, opcode2, vch2))
                break;

            // Template matching opcodes:
            if (opcode2 == OP_PUBKEYS) {
                while (vch1.size() >= 33 && vch1.size() <= 120) {
                    vSolutionsRet.push_back(vch1);
                    if (!script1.GetOp(pc1, opcode1, vch1))
                        break;
                }
                if (!script2.GetOp(pc2, opcode2, vch2))
                    break;
                // Normal situation is to fall through
                // to other if/else statements
            }

            if (opcode2 == OP_PUBKEY) {
                if (vch1.size() < 33 || vch1.size() > 120)
                    break;
                vSolutionsRet.push_back(vch1);
            } else if (opcode2 == OP_PUBKEYHASH) {
                if (vch1.size() != sizeof(uint160))
                    break;
                vSolutionsRet.push_back(vch1);
            } else if (opcode2 == OP_SMALLINTEGER) { // Single-byte small integer pushed onto vSolutions
                if (opcode1 == OP_0 || (opcode1 >= OP_1 && opcode1 <= OP_16)) {
                    char n = (char)CScript::DecodeOP_N(opcode1);
                    vSolutionsRet.push_back(valtype(1, n));
                } else
                    break;
            } else if (opcode2 == OP_SMALLDATA) {
                // small pushdata, <= 4096 bytes after hard fork, 80 before
                if (vch1.size() > Params().OpReturnMaxSize(txdb))
                    break;
            } else if (opcode1 != opcode2 || vch1 != vch2) {
                // Others must match exactly
                break;
            }
        }
    }

    vSolutionsRet.clear();
    typeRet = TX_NONSTANDARD;
    return false;
}

bool Sign1(const CKeyID& address, const CKeyStore& keystore, uint256 hash, int nHashType,
           CScript& scriptSigRet)
{
    CKey key;
    if (!keystore.GetKey(address, key))
        return false;

    vector<unsigned char> vchSig;
    if (!key.Sign(hash, vchSig))
        return false;
    vchSig.push_back((unsigned char)nHashType);
    scriptSigRet << vchSig;

    return true;
}

bool SignN(const vector<valtype>& multisigdata, const CKeyStore& keystore, uint256 hash, int nHashType,
           CScript& scriptSigRet)
{
    int nSigned   = 0;
    int nRequired = multisigdata.front()[0];
    for (unsigned int i = 1; i < multisigdata.size() - 1 && nSigned < nRequired; i++) {
        const valtype& pubkey = multisigdata[i];
        CKeyID         keyID  = CPubKey(pubkey).GetID();
        if (Sign1(keyID, keystore, hash, nHashType, scriptSigRet))
            ++nSigned;
    }
    return nSigned == nRequired;
}

//
// Sign scriptPubKey with private keys stored in keystore, given transaction hash and hash type.
// Signatures are returned in scriptSigRet (or returns false if scriptPubKey can't be signed),
// unless whichTypeRet is TX_SCRIPTHASH, in which case scriptSigRet is the redemption script.
// Returns false if scriptPubKey could not be completely satisfied.
//
bool Solver(const ITxDB& txdb, const CKeyStore& keystore, const CScript& scriptPubKey, uint256 hash,
            int nHashType, CScript& scriptSigRet, txnouttype& whichTypeRet, bool fColdStake = false)
{
    scriptSigRet.clear();

    vector<valtype> vSolutions;
    if (!Solver(txdb, scriptPubKey, whichTypeRet, vSolutions))
        return false;

    CKeyID keyID;
    switch (whichTypeRet) {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return false;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        return Sign1(keyID, keystore, hash, nHashType, scriptSigRet);
    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (!Sign1(keyID, keystore, hash, nHashType, scriptSigRet))
            return false;
        else {
            CPubKey vch;
            keystore.GetPubKey(keyID, vch);
            scriptSigRet << vch;
        }
        return true;
    case TX_SCRIPTHASH:
        return keystore.GetCScript(uint160(vSolutions[0]), scriptSigRet);

    case TX_MULTISIG:
        scriptSigRet << OP_0; // workaround CHECKMULTISIG bug
        return (SignN(vSolutions, keystore, hash, nHashType, scriptSigRet));

    case TX_COLDSTAKE:
    case TX_POOLCOLDSTAKE:
        if (fColdStake) {
            // sign with the cold staker key
            keyID = CKeyID(uint160(vSolutions[0]));
        } else {
            // sign with the owner key
            keyID = CKeyID(uint160(vSolutions[1]));
        }
        if (!Sign1(keyID, keystore, hash, nHashType, scriptSigRet))
            return NLog.error("*** {}: failed to sign with the {} key.", FUNCTIONSIG,
                              fColdStake ? "cold staker" : "owner");
        CPubKey pubKey;
        if (!keystore.GetPubKey(keyID, pubKey))
            return NLog.error("{} : Unable to get public key from keyID", FUNCTIONSIG);
        std::vector<unsigned char> vch = pubKey.Raw();
        scriptSigRet << (fColdStake ? (int)OP_TRUE : OP_FALSE) << vch;
        return true;
    }
    return false;
}

int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char>>& vSolutions)
{
    switch (t) {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return -1;
    case TX_PUBKEY:
        return 1;
    case TX_PUBKEYHASH:
        return 2;
    case TX_MULTISIG:
        if (vSolutions.size() < 1 || vSolutions[0].size() < 1)
            return -1;
        return vSolutions[0][0] + 1;
    case TX_SCRIPTHASH:
        return 1; // doesn't include args needed by the script
    case TX_COLDSTAKE:
    case TX_POOLCOLDSTAKE:
        return 3;
    }
    return -1;
}

bool IsStandard(const ITxDB& txdb, const CScript& scriptPubKey, txnouttype& whichType)
{
    vector<valtype> vSolutions;
    if (!Solver(txdb, scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_MULTISIG) {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
        // Support up to x-of-3 multisig txns as standard
        if (n < 1 || n > 3)
            return false;
        if (m < 1 || m > n)
            return false;
    }

    return whichType != TX_NONSTANDARD;
}

unsigned int HaveKeys(const vector<valtype>& pubkeys, const CKeyStore& keystore)
{
    unsigned int nResult = 0;
    for (const valtype& pubkey : pubkeys) {
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (keystore.HaveKey(keyID))
            ++nResult;
    }
    return nResult;
}

class CKeyStoreIsMineVisitor : public boost::static_visitor<isminetype>
{
private:
    const CKeyStore* keystore;

public:
    CKeyStoreIsMineVisitor(const CKeyStore* keystoreIn) : keystore(keystoreIn) {}
    isminetype operator()(const CNoDestination& /*dest*/) const { return isminetype::ISMINE_NO; }
    isminetype operator()(const CKeyID& keyID) const
    {
        return keystore->HaveKey(keyID) ? isminetype::ISMINE_SPENDABLE : isminetype::ISMINE_NO;
    }
    isminetype operator()(const CScriptID& scriptID) const
    {
        return keystore->HaveCScript(scriptID) ? isminetype::ISMINE_SPENDABLE : isminetype::ISMINE_NO;
    }
};

isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest)
{
    return boost::apply_visitor(CKeyStoreIsMineVisitor(&keystore), dest);
}

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey)
{
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    if (!Solver(CTxDB(), scriptPubKey, whichType, vSolutions))
        return isminetype::ISMINE_NO;

    CKeyID keyID;
    switch (whichType) {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return isminetype::ISMINE_NO;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        if (keystore.HaveKey(keyID))
            return isminetype::ISMINE_SPENDABLE;
        break;
    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (keystore.HaveKey(keyID))
            return isminetype::ISMINE_SPENDABLE;
        break;
    case TX_SCRIPTHASH: {
        CScript subscript;
        if (!keystore.GetCScript(CScriptID(uint160(vSolutions[0])), subscript))
            return isminetype::ISMINE_NO;
        return IsMine(keystore, subscript);
    }
    case TX_MULTISIG: {
        // Only consider transactions "mine" if we own ALL the
        // keys involved. multi-signature transactions that are
        // partially owned (somebody else has a key that can spend
        // them) enable spend-out-from-under-you attacks, especially
        // in shared-wallet situations.
        std::vector<valtype> keys(vSolutions.begin() + 1, vSolutions.begin() + vSolutions.size() - 1);
        if (HaveKeys(keys, keystore) == keys.size())
            return isminetype::ISMINE_SPENDABLE;
        break;
    }
    case TX_COLDSTAKE:
    case TX_POOLCOLDSTAKE: {
        CKeyID stakeKeyID     = CKeyID(uint160(vSolutions[0]));
        bool   stakeKeyIsMine = keystore.HaveKey(stakeKeyID);
        CKeyID ownerKeyID     = CKeyID(uint160(vSolutions[1]));
        bool   spendKeyIsMine = keystore.HaveKey(ownerKeyID);

        if (spendKeyIsMine && stakeKeyIsMine)
            return isminetype::ISMINE_SPENDABLE_STAKEABLE;
        else if (stakeKeyIsMine)
            return isminetype::ISMINE_COLD;
        else if (spendKeyIsMine)
            return isminetype::ISMINE_SPENDABLE_DELEGATED;
        break;
    }
    }
    return isminetype::ISMINE_NO;
}

bool ExtractDestination(const ITxDB& txdb, const CScript& scriptPubKey, CTxDestination& addressRet,
                        bool fColdStake)
{
    vector<valtype> vSolutions;
    txnouttype      whichType;
    if (!Solver(txdb, scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY) {
        addressRet = CPubKey(vSolutions[0]).GetID();
        return true;
    } else if (whichType == TX_PUBKEYHASH) {
        addressRet = CKeyID(uint160(vSolutions[0]));
        return true;
    } else if (whichType == TX_SCRIPTHASH) {
        addressRet = CScriptID(uint160(vSolutions[0]));
        return true;
    } else if (whichType == TX_COLDSTAKE) {
        addressRet = CKeyID(uint160(vSolutions[!fColdStake]));
        return true;
    }
    // Multisig txns have more than one address...
    return false;
}

class CAffectedKeysVisitor : public boost::static_visitor<void>
{
private:
    const ITxDB&         txdb;
    const CKeyStore&     keystore;
    std::vector<CKeyID>& vKeys;

public:
    CAffectedKeysVisitor(const ITxDB& txdbIn, const CKeyStore& keystoreIn, std::vector<CKeyID>& vKeysIn)
        : txdb(txdbIn), keystore(keystoreIn), vKeys(vKeysIn)
    {
    }

    void Process(const CScript& script)
    {
        txnouttype                  type;
        std::vector<CTxDestination> vDest;
        int                         nRequired;
        if (ExtractDestinations(txdb, script, type, vDest, nRequired)) {
            for (const CTxDestination& dest : vDest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const CKeyID& keyId)
    {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID& scriptId)
    {
        CScript script;
        if (keystore.GetCScript(scriptId, script))
            Process(script);
    }

    void operator()(const CNoDestination& /*none*/) {}
};

void ExtractAffectedKeys(const ITxDB& txdb, const CKeyStore& keystore, const CScript& scriptPubKey,
                         std::vector<CKeyID>& vKeys)
{
    CAffectedKeysVisitor(txdb, keystore, vKeys).Process(scriptPubKey);
}

bool ExtractDestinations(const ITxDB& txdb, const CScript& scriptPubKey, txnouttype& typeRet,
                         vector<CTxDestination>& addressRet, int& nRequiredRet)
{
    addressRet.clear();
    typeRet = TX_NONSTANDARD;
    vector<valtype> vSolutions;
    if (!Solver(txdb, scriptPubKey, typeRet, vSolutions))
        return false;
    if (typeRet == TX_NULL_DATA) {
        // This is data, not addresses
        return false;
    }

    if (typeRet == TX_MULTISIG) {
        nRequiredRet = vSolutions.front()[0];
        for (unsigned int i = 1; i < vSolutions.size() - 1; i++) {
            CTxDestination address = CPubKey(vSolutions[i]).GetID();
            addressRet.push_back(address);
        }
    } else if (typeRet == TX_COLDSTAKE) {
        if (vSolutions.size() < 2)
            return false;
        nRequiredRet = 2;
        addressRet.push_back(CKeyID(uint160(vSolutions[0])));
        addressRet.push_back(CKeyID(uint160(vSolutions[1])));
        return true;

    } else {
        nRequiredRet = 1;
        CTxDestination address;
        if (!ExtractDestination(txdb, scriptPubKey, address))
            return false;
        addressRet.push_back(address);
    }

    return true;
}

Result<void, ScriptError> VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey,
                                       const CTransaction& txTo, unsigned int nIn,
                                       bool fValidatePayToScriptHash, bool fStrictEncodings,
                                       int nHashType)
{

    vector<vector<unsigned char>> stack, stackCopy;

    TRYV(EvalScript(stack, scriptSig, txTo, nIn, fStrictEncodings, nHashType));

    if (fValidatePayToScriptHash)
        stackCopy = stack;

    TRYV(EvalScript(stack, scriptPubKey, txTo, nIn, fStrictEncodings, nHashType));

    if (stack.empty())
        return Err(SCRIPT_ERR_EVAL_FALSE);

    if (CastToBool(stack.back()) == false)
        return Err(SCRIPT_ERR_EVAL_FALSE);

    // Additional validation for spend-to-script-hash transactions:
    if (scriptPubKey.IsPayToScriptHash()) {
        if (!scriptSig.IsPushOnly()) // scriptSig must be literals-only
            return Err(SCRIPT_ERR_SIG_PUSHONLY);

        if (stackCopy.empty()) // Check to make sure that the stackCopy has elements too
            return Err(SCRIPT_ERR_UNKNOWN_ERROR);

        const valtype& pubKeySerialized = stackCopy.back();
        CScript        pubKey2(pubKeySerialized.begin(), pubKeySerialized.end());
        popstack(stackCopy);

        TRYV(EvalScript(stackCopy, pubKey2, txTo, nIn, fStrictEncodings, nHashType));

        if (stackCopy.empty())
            return Err(SCRIPT_ERR_EVAL_FALSE);
        if (!CastToBool(stackCopy.back()))
            return Err(SCRIPT_ERR_EVAL_FALSE);
        else
            return Ok();
    }

    return Ok();
}

SignatureState SignSignature(const CKeyStore& keystore, const CScript& fromPubKey, CTransaction& txTo,
                             unsigned int nIn, int nHashType, bool fColdStake)
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];

    // Leave out the signature from the hash, since a signature can't sign itself.
    // The checksig op will also drop the signatures from its hash.
    uint256 hash = SignatureHash(fromPubKey, txTo, nIn, nHashType);

    const CTxDB txdb;

    txnouttype whichType;
    if (!Solver(txdb, keystore, fromPubKey, hash, nHashType, txin.scriptSig, whichType, fColdStake))
        return SignatureState::Failed;

    if (whichType == TX_SCRIPTHASH) {
        // Solver returns the subscript that need to be evaluated;
        // the final scriptSig is the signatures from that
        // and then the serialized subscript:
        CScript subscript = txin.scriptSig;

        // Recompute txn hash using subscript in place of scriptPubKey:
        uint256 hash2 = SignatureHash(subscript, txTo, nIn, nHashType);

        txnouttype subType;
        bool fSolved = Solver(txdb, keystore, subscript, hash2, nHashType, txin.scriptSig, subType) &&
                       subType != TX_SCRIPTHASH;
        // Append serialized subscript whether or not it is completely signed:
        txin.scriptSig << static_cast<valtype>(subscript);
        if (!fSolved)
            return SignatureState::Failed;
    }

    // Test solution
    if (!fColdStake && VerifyScript(txin.scriptSig, fromPubKey, txTo, nIn, true, true, 0).isOk()) {
        return SignatureState::Verified;
    }
    // we don't verify cold stakes because the verification is transaction dependent, not input dependent
    if (fColdStake) {
        return SignatureState::Unverified;
    }
    return SignatureState::Failed;
}

SignatureState SignSignature(const CKeyStore& keystore, const CTransaction& txFrom, CTransaction& txTo,
                             unsigned int nIn, int nHashType, bool fColdStake)
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    return SignSignature(keystore, txout.scriptPubKey, txTo, nIn, nHashType, fColdStake);
}

Result<void, ScriptError> VerifySignature(const CTransaction& txFrom, const CTransaction& txTo,
                                          unsigned int nIn, bool fValidatePayToScriptHash,
                                          bool fStrictEncodings, int nHashType)
{
    assert(nIn < txTo.vin.size());
    const CTxIn& txin = txTo.vin[nIn];
    if (txin.prevout.n >= txFrom.vout.size())
        return Err(ScriptError::SCRIPT_ERR_UNKNOWN_ERROR);
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    if (txin.prevout.hash != txFrom.GetHash())
        return Err(ScriptError::SCRIPT_ERR_UNKNOWN_ERROR);

    TRYV(VerifyScript(txin.scriptSig, txout.scriptPubKey, txTo, nIn, fValidatePayToScriptHash,
                      fStrictEncodings, nHashType));
    return Ok();
}

static CScript PushAll(const vector<valtype>& values)
{
    CScript result;
    for (const valtype& v : values)
        result << v;
    return result;
}

static CScript CombineMultisig(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                               const vector<valtype>& vSolutions, vector<valtype>& sigs1,
                               vector<valtype>& sigs2)
{
    // Combine all the signatures we've got:
    set<valtype> allsigs;
    for (const valtype& v : sigs1) {
        if (!v.empty())
            allsigs.insert(v);
    }
    for (const valtype& v : sigs2) {
        if (!v.empty())
            allsigs.insert(v);
    }

    // Build a map of pubkey -> signature by matching sigs to pubkeys:
    assert(vSolutions.size() > 1);
    unsigned int          nSigsRequired = vSolutions.front()[0];
    unsigned int          nPubKeys      = vSolutions.size() - 2;
    map<valtype, valtype> sigs;
    for (const valtype& sig : allsigs) {
        for (unsigned int i = 0; i < nPubKeys; i++) {
            const valtype& pubkey = vSolutions[i + 1];
            if (sigs.count(pubkey))
                continue; // Already got a sig for this pubkey

            if (CheckSig(sig, pubkey, scriptPubKey, txTo, nIn, 0)) {
                sigs[pubkey] = sig;
                break;
            }
        }
    }
    // Now build a merged CScript:
    unsigned int nSigsHave = 0;
    CScript      result;
    result << OP_0; // pop-one-too-many workaround
    for (unsigned int i = 0; i < nPubKeys && nSigsHave < nSigsRequired; i++) {
        if (sigs.count(vSolutions[i + 1])) {
            result << sigs[vSolutions[i + 1]];
            ++nSigsHave;
        }
    }
    // Fill any missing with OP_0:
    for (unsigned int i = nSigsHave; i < nSigsRequired; i++)
        result << OP_0;

    return result;
}

static CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                                 const txnouttype txType, const vector<valtype>& vSolutions,
                                 vector<valtype>& sigs1, vector<valtype>& sigs2)
{
    switch (txType) {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        // Don't know anything about this, assume bigger one is correct:
        if (sigs1.size() >= sigs2.size())
            return PushAll(sigs1);
        return PushAll(sigs2);
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
    case TX_COLDSTAKE:
    case TX_POOLCOLDSTAKE:
        // Signatures are bigger than placeholders or empty scripts:
        if (sigs1.empty() || sigs1[0].empty())
            return PushAll(sigs2);
        return PushAll(sigs1);
    case TX_SCRIPTHASH:
        if (sigs1.empty() || sigs1.back().empty())
            return PushAll(sigs2);
        else if (sigs2.empty() || sigs2.back().empty())
            return PushAll(sigs1);
        else {
            // Recur to combine:
            valtype spk = sigs1.back();
            CScript pubKey2(spk.begin(), spk.end());

            txnouttype                    txType2;
            vector<vector<unsigned char>> vSolutions2;
            Solver(CTxDB(), pubKey2, txType2, vSolutions2);
            sigs1.pop_back();
            sigs2.pop_back();
            CScript result = CombineSignatures(pubKey2, txTo, nIn, txType2, vSolutions2, sigs1, sigs2);
            result << spk;
            return result;
        }
    case TX_MULTISIG:
        return CombineMultisig(scriptPubKey, txTo, nIn, vSolutions, sigs1, sigs2);
    }

    return CScript();
}

CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                          const CScript& scriptSig1, const CScript& scriptSig2)
{
    txnouttype                    txType;
    vector<vector<unsigned char>> vSolutions;
    Solver(CTxDB(), scriptPubKey, txType, vSolutions);

    vector<valtype> stack1;
    EvalScript(stack1, scriptSig1, CTransaction(), 0, true, 0);
    vector<valtype> stack2;
    EvalScript(stack2, scriptSig2, CTransaction(), 0, true, 0);

    return CombineSignatures(scriptPubKey, txTo, nIn, txType, vSolutions, stack1, stack2);
}

unsigned int CScript::GetSigOpCount(bool fAccurate) const
{
    unsigned int   n          = 0;
    const_iterator pc         = begin();
    opcodetype     lastOpcode = OP_INVALIDOPCODE;
    while (pc < end()) {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            break;
        if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY)
            n++;
        else if (opcode == OP_CHECKMULTISIG || opcode == OP_CHECKMULTISIGVERIFY) {
            if (fAccurate && lastOpcode >= OP_1 && lastOpcode <= OP_16)
                n += DecodeOP_N(lastOpcode);
            else
                n += 20;
        }
        lastOpcode = opcode;
    }
    return n;
}

unsigned int CScript::GetSigOpCount(const CScript& scriptSig) const
{
    if (!IsPayToScriptHash())
        return GetSigOpCount(true);

    // This is a pay-to-script-hash scriptPubKey;
    // get the last item that the scriptSig
    // pushes onto the stack:
    const_iterator        pc = scriptSig.begin();
    vector<unsigned char> data;
    while (pc < scriptSig.end()) {
        opcodetype opcode;
        if (!scriptSig.GetOp(pc, opcode, data))
            return 0;
        if (opcode > OP_16)
            return 0;
    }

    /// ... and return its opcount:
    CScript subscript(data.begin(), data.end());
    return subscript.GetSigOpCount(true);
}

// clang-format off
bool CScript::IsPayToScriptHash() const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 &&
            this->at(0) == OP_HASH160 &&
            this->at(1) == 0x14 &&
            this->at(22) == OP_EQUAL);
}
// clang-format off

// clang-format off
bool CScript::IsPayToColdStaking() const
{
    // Extra-fast test for pay-to-cold-staking CScripts:
    return (this->size() == 51 &&
            this->at(2) == OP_ROT &&
            this->at(4) == OP_CHECKCOLDSTAKEVERIFY &&
            this->at(5) == 0x14 &&
            this->at(27) == 0x14 &&
            this->at(49) == OP_EQUALVERIFY &&
            this->at(50) == OP_CHECKSIG);
}

bool CScript::IsPayToColdStakingForPool() const
{
    // Extra-fast test for pay-to-cold-staking CScripts:
    return (this->size() == 51 &&
            this->at(2) == OP_ROT &&
            this->at(4) == OP_CHECKPOOLCOLDSTAKEVERIFY &&
            this->at(5) == 0x14 &&
            this->at(27) == 0x14 &&
            this->at(49) == OP_EQUALVERIFY &&
            this->at(50) == OP_CHECKSIG);
}
// clang-format on

boost::optional<std::vector<uint8_t>> CScript::GetPubKeyOfP2CSScriptSig() const
{

    CScript::const_iterator           pc = cbegin();
    opcodetype                        opcode;
    valtype                           vchValue;
    static const std::vector<uint8_t> TRUE_VEC({OP_TRUE});

    if (!GetOp(pc, opcode, vchValue)) {
        // sig is expected here
        return boost::none;
    }

    // check that the signature is canonical and valid
    if (!IsCanonicalSignature(vchValue)) {
        return boost::none;
    }

    if (!GetOp(pc, opcode, vchValue)) {
        // we expect a OP_TRUE here
        return boost::none;
    }
    if (vchValue != TRUE_VEC) {
        // if this is false, it's a delegation revocation, it's not what we expect
        return boost::none;
    }
    if (!GetOp(pc, opcode, vchValue)) {
        // we expect the public key here
        return boost::none;
    }

    // check that public key is valid
    if (!IsCanonicalPubKey(vchValue)) {
        return boost::none;
    }
    // we extract the public key
    auto pubKey = boost::make_optional(std::move(vchValue));
    if (GetOp(pc, opcode, vchValue)) {
        // there should be nothing left, otherwise this is not scriptSig of P2CS
        return boost::none;
    }
    return pubKey;
}

bool CScript::HasCanonicalPushes() const
{
    const_iterator pc = begin();
    while (pc < end()) {
        opcodetype                 opcode;
        std::vector<unsigned char> data;
        if (!GetOp(pc, opcode, data))
            return false;
        if (opcode > OP_16)
            continue;
        if (opcode < OP_PUSHDATA1 && opcode > OP_0 && (data.size() == 1 && data[0] <= 16))
            // Could have used an OP_n code, rather than a 1-byte push.
            return false;
        if (opcode == OP_PUSHDATA1 && data.size() < OP_PUSHDATA1)
            // Could have used a normal n-byte push, rather than OP_PUSHDATA1.
            return false;
        if (opcode == OP_PUSHDATA2 && data.size() <= 0xFF)
            // Could have used an OP_PUSHDATA1.
            return false;
        if (opcode == OP_PUSHDATA4 && data.size() <= 0xFFFF)
            // Could have used an OP_PUSHDATA2.
            return false;
    }
    return true;
}

class CScriptVisitor : public boost::static_visitor<bool>
{
private:
    CScript* script;

public:
    CScriptVisitor(CScript* scriptin) { script = scriptin; }

    bool operator()(const CNoDestination& /*dest*/) const
    {
        script->clear();
        return false;
    }

    bool operator()(const CKeyID& keyID) const
    {
        script->clear();
        *script << OP_DUP << OP_HASH160 << keyID << OP_EQUALVERIFY << OP_CHECKSIG;
        return true;
    }

    bool operator()(const CScriptID& scriptID) const
    {
        script->clear();
        *script << OP_HASH160 << scriptID << OP_EQUAL;
        return true;
    }
};

void CScript::SetDestination(const CTxDestination& dest)
{
    boost::apply_visitor(CScriptVisitor(this), dest);
}

void CScript::SetMultisig(int nRequired, const std::vector<CKey>& keys)
{
    this->clear();

    *this << EncodeOP_N(nRequired);
    for (const CKey& key : keys)
        *this << key.GetPubKey();
    *this << EncodeOP_N(keys.size()) << OP_CHECKMULTISIG;
}

bool CScriptCompressor::IsToKeyID(CKeyID& hash) const
{
    if (script.size() == 25 && script[0] == OP_DUP && script[1] == OP_HASH160 && script[2] == 20 &&
        script[23] == OP_EQUALVERIFY && script[24] == OP_CHECKSIG) {
        memcpy(&hash, &script[3], 20);
        return true;
    }
    return false;
}

bool CScriptCompressor::IsToScriptID(CScriptID& hash) const
{
    if (script.size() == 23 && script[0] == OP_HASH160 && script[1] == 20 && script[22] == OP_EQUAL) {
        memcpy(&hash, &script[2], 20);
        return true;
    }
    return false;
}

bool CScriptCompressor::IsToPubKey(std::vector<unsigned char>& pubkey) const
{
    if (script.size() == 35 && script[0] == 33 && script[34] == OP_CHECKSIG &&
        (script[1] == 0x02 || script[1] == 0x03)) {
        pubkey.resize(33);
        memcpy(&pubkey[0], &script[1], 33);
        return true;
    }
    if (script.size() == 67 && script[0] == 65 && script[66] == OP_CHECKSIG && script[1] == 0x04) {
        pubkey.resize(65);
        memcpy(&pubkey[0], &script[1], 65);
        CKey key;
        return (key.SetPubKey(CPubKey(pubkey))); // SetPubKey fails if this is not a valid public key, a
                                                 // case that would not be compressible
    }
    return false;
}

bool CScriptCompressor::Compress(std::vector<unsigned char>& out) const
{
    CKeyID keyID;
    if (IsToKeyID(keyID)) {
        out.resize(21);
        out[0] = 0x00;
        memcpy(&out[1], &keyID, 20);
        return true;
    }
    CScriptID scriptID;
    if (IsToScriptID(scriptID)) {
        out.resize(21);
        out[0] = 0x01;
        memcpy(&out[1], &scriptID, 20);
        return true;
    }
    std::vector<unsigned char> pubkey;
    if (IsToPubKey(pubkey)) {
        out.resize(33);
        memcpy(&out[1], &pubkey[1], 32);
        if (pubkey[0] == 0x02 || pubkey[0] == 0x03) {
            out[0] = pubkey[0];
            return true;
        } else if (pubkey[0] == 0x04) {
            out[0] = 0x04 | (pubkey[64] & 0x01);
            return true;
        }
    }
    return false;
}

unsigned int CScriptCompressor::GetSpecialSize(unsigned int nSize) const
{
    if (nSize == 0 || nSize == 1)
        return 20;
    if (nSize == 2 || nSize == 3 || nSize == 4 || nSize == 5)
        return 32;
    return 0;
}

bool CScriptCompressor::Decompress(unsigned int nSize, const std::vector<unsigned char>& in)
{
    switch (nSize) {
    case 0x00:
        script.resize(25);
        script[0] = OP_DUP;
        script[1] = OP_HASH160;
        script[2] = 20;
        memcpy(&script[3], &in[0], 20);
        script[23] = OP_EQUALVERIFY;
        script[24] = OP_CHECKSIG;
        return true;
    case 0x01:
        script.resize(23);
        script[0] = OP_HASH160;
        script[1] = 20;
        memcpy(&script[2], &in[0], 20);
        script[22] = OP_EQUAL;
        return true;
    case 0x02:
    case 0x03:
        script.resize(35);
        script[0] = 33;
        script[1] = nSize;
        memcpy(&script[2], &in[0], 32);
        script[34] = OP_CHECKSIG;
        return true;
    case 0x04:
    case 0x05:
        std::vector<unsigned char> vch(33, 0x00);
        vch[0] = nSize - 2;
        memcpy(&vch[1], &in[0], 32);
        CKey key;
        if (!key.SetPubKey(CPubKey(vch)))
            return false;
        key.SetCompressedPubKey(false); // Decompress public key
        CPubKey pubkey = key.GetPubKey();
        script.resize(67);
        script[0] = 65;
        memcpy(&script[1], &pubkey.Raw()[0], 65);
        script[66] = OP_CHECKSIG;
        return true;
    }
    return false;
}

CScript GetScriptForDestination(const CTxDestination& dest)
{
    CScript script;

    boost::apply_visitor(CScriptVisitor(&script), dest);
    return script;
}

CScript GetScriptForStakeDelegation(const CKeyID& stakingKey, const CKeyID& spendingKey)
{
    CScript script;
    script << OP_DUP << OP_HASH160 << OP_ROT << OP_IF << OP_CHECKCOLDSTAKEVERIFY
           << ToByteVector(stakingKey) << OP_ELSE << ToByteVector(spendingKey) << OP_ENDIF
           << OP_EQUALVERIFY << OP_CHECKSIG;
    return script;
}

CScript GetScriptForStakeDelegationForPool(const CKeyID& stakingKey, const CKeyID& spendingKey)
{
    CScript script;
    script << OP_DUP << OP_HASH160 << OP_ROT << OP_IF << OP_CHECKPOOLCOLDSTAKEVERIFY
           << ToByteVector(stakingKey) << OP_ELSE << ToByteVector(spendingKey) << OP_ENDIF
           << OP_EQUALVERIFY << OP_CHECKSIG;
    return script;
}
