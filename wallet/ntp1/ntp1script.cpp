#include "ntp1script.h"

#include <bitset>
#include <boost/algorithm/hex.hpp>
#include <numeric>
#include <stdexcept>
#include <util.h>

#include "ntp1script_burn.h"
#include "ntp1script_issuance.h"
#include "ntp1script_transfer.h"

std::string NTP1Script::getHeader() const { return headerBin; }

std::string NTP1Script::getOpCodeBin() const { return opCodeBin; }

NTP1Script::TxType NTP1Script::getTxType() const { return txType; }

std::string NTP1Script::getParsedScript() const { return parsedScript; }

void NTP1Script::setCommonParams(std::string Header, int ProtocolVersion, std::string OpCodeBin)
{
    headerBin       = Header;
    protocolVersion = ProtocolVersion;
    opCodeBin       = OpCodeBin;
    txType          = CalculateTxType(opCodeBin);
}

std::string NTP1Script::TransferInstructionToBinScript(const NTP1Script::TransferInstruction& inst)
{
    std::string result;

    std::string    skipBitStr = (inst.skipInput ? "1" : "0");
    std::bitset<5> outputIndexBits(inst.outputIndex);
    std::string    amountStr = NumberToHexNTP1Amount(inst.amount);

    std::string allStr = skipBitStr + "00" + outputIndexBits.to_string();
    if (allStr.size() != 8) {
        throw std::string("Unable to correctly construct transfer instruction with parameters: skip (" +
                          ToString(inst.skipInput) + "), output index (" + ToString(inst.outputIndex) +
                          ")");
    }
    uint8_t allUChar = static_cast<uint8_t>(std::bitset<8>(allStr).to_ulong());
    result += *reinterpret_cast<char*>(&allUChar);

    result += boost::algorithm::unhex(amountStr);

    return result;
}

uint64_t NTP1Script::CalculateMetadataSize(const std::string& op_code_bin)
{
    if (op_code_bin.size() > 1) {
        throw std::runtime_error("Too large op_code");
    }
    const uint8_t char1 = *reinterpret_cast<const uint8_t*>(&op_code_bin[0]);
    if (char1 == 0x01) {
        return 52;
    } else if (char1 == 0x02) {
        return 20;
    } else if (char1 == 0x03) {
        return 0;
    } else if (char1 == 0x04) {
        return 20;
    } else if (char1 == 0x05) {
        return 0;
    } else if (char1 == 0x06) {
        return 0;
    } else if (char1 == 0x10) {
        return 52;
    } else if (char1 == 0x11) {
        return 20;
    } else if (char1 == 0x12) {
        return 0;
    } else if (char1 == 0x13) {
        return 20;
    } else if (char1 == 0x14) {
        return 20;
    } else if (char1 == 0x15) {
        return 0;
    } else if (char1 == 0x20) {
        return 52;
    } else if (char1 == 0x21) {
        return 20;
    } else if (char1 == 0x22) {
        return 0;
    } else if (char1 == 0x23) {
        return 20;
    } else if (char1 == 0x24) {
        return 20;
    } else if (char1 == 0x25) {
        return 0;
    }
    throw std::runtime_error("Unknown OP_CODE (in hex): " + boost::algorithm::hex(op_code_bin));
}

NTP1Script::TxType NTP1Script::CalculateTxType(const std::string& op_code_bin)
{
    uint8_t char1 = *reinterpret_cast<const uint8_t*>(&op_code_bin[0]);
    if (char1 <= 0x0F) {
        return TxType::TxType_Issuance;
    } else if (char1 <= 0x1F) {
        return TxType::TxType_Transfer;
    } else if (char1 <= 0x2F) {
        return TxType::TxType_Burn;
    } else {
        throw std::runtime_error("Unable to parse transaction type with unknown opcode (in hex): " +
                                 boost::algorithm::hex(op_code_bin));
    }
}

uint64_t NTP1Script::CalculateAmountSize(uint8_t firstChar)
{
    std::bitset<8> bits(firstChar);
    std::bitset<3> bits3(bits.to_string().substr(0, 3));
    unsigned long  s = bits3.to_ulong();
    if (s < 6) {
        return s + 1;
    } else {
        return 7;
    }
}

uint64_t NTP1Script::ParseAmountFromLongEnoughString(const std::string& BinAmountStartsAtByte0,
                                                     int&               rawSize)
{
    if (BinAmountStartsAtByte0.size() < 2) {
        throw std::runtime_error("Too short a string to be parsed " +
                                 boost::algorithm::hex(BinAmountStartsAtByte0));
    }
    int amountSize = CalculateAmountSize(static_cast<uint8_t>(BinAmountStartsAtByte0[0]));
    if ((int)BinAmountStartsAtByte0.size() < amountSize) {
        throw std::runtime_error("Error parsing script: " + BinAmountStartsAtByte0 +
                                 "; the amount size is longer than what is available in the script");
    }
    rawSize = amountSize;
    return NTP1AmountHexToNumber<uint64_t>(
        boost::algorithm::hex(BinAmountStartsAtByte0.substr(0, amountSize)));
}

std::string NTP1Script::ParseOpCodeFromLongEnoughString(const std::string& BinOpCodeStartsAtByte0)
{
    std::string result;
    for (unsigned i = 0; BinOpCodeStartsAtByte0.size(); i++) {
        const auto&   c  = BinOpCodeStartsAtByte0[i];
        const uint8_t uc = *reinterpret_cast<const uint8_t*>(&c);
        result.push_back(c);
        // byte value 0xFF means that more OP_CODE bytes are required
        if (uc != 255) {
            break;
        } else {
            if (i + 1 == BinOpCodeStartsAtByte0.size()) {
                throw std::runtime_error("OpCode's last byte 0xFF indicates that there's more, but "
                                         "there's no more characters in the string.");
            }
        }
    }
    return result;
}

std::string NTP1Script::ParseMetadataFromLongEnoughString(const std::string& BinMetadataStartsAtByte0,
                                                          const std::string& op_code_bin,
                                                          const std::string& wholeScriptHex)
{
    int metadataSize = CalculateMetadataSize(op_code_bin);
    if ((int)BinMetadataStartsAtByte0.size() < metadataSize) {
        throw std::runtime_error("Error parsing script" +
                                 (wholeScriptHex.size() > 0 ? ": " + wholeScriptHex : "") +
                                 "; the metadata size is longer than what is available in the script");
    }
    return BinMetadataStartsAtByte0.substr(0, metadataSize);
}

std::string
NTP1Script::ParseTokenSymbolFromLongEnoughString(const std::string& BinTokenSymbolStartsAtByte0)
{
    if ((int)BinTokenSymbolStartsAtByte0.size() < 0) {
        throw std::runtime_error(
            "Error parsing script (starting at this point a symbol is expected). " +
            (BinTokenSymbolStartsAtByte0.size() > 0 ? ": " + BinTokenSymbolStartsAtByte0 : "") +
            "; the token symbol size is longer than what is available in the script");
    }
    const std::string& ScriptBin = BinTokenSymbolStartsAtByte0;
    std::string        result;
    result = ScriptBin.substr(0, 5);
    // drop 0x01 chars from the beginning
    result.erase(std::remove_if(result.begin(), result.end(),
                                [](char c) { return *reinterpret_cast<uint8_t*>(&(c)) == 0x01; }),
                 result.end());

    if (result.size() == 0) {
        throw std::runtime_error("Invalid token symbol; it cannot be empty.");
    }
    return result;
}

std::vector<NTP1Script::TransferInstruction> NTP1Script::ParseTransferInstructionsFromLongEnoughString(
    const std::string& BinInstructionsStartFromByte0, int& totalRawSize)
{
    std::string                      toParse = BinInstructionsStartFromByte0;
    std::vector<TransferInstruction> result;
    totalRawSize = 0;
    for (int i = 0;; i++) {
        if (toParse.size() <= 1) {
            break;
        }

        // one byte of flags, and then N bytes for the amount
        TransferInstruction transferInst;
        transferInst.firstRawByte = static_cast<unsigned char>(toParse[0]);
        transferInst.rawAmount    = toParse.substr(1, CalculateAmountSize(toParse[1]));
        int currentSize           = 1 + transferInst.rawAmount.size();
        totalRawSize += currentSize;
        toParse.erase(toParse.begin(), toParse.begin() + currentSize);

        // parse data from raw
        std::bitset<8> rawByte(transferInst.firstRawByte);
        std::bitset<5> outputIndex(rawByte.to_string().substr(3, 5));
        transferInst.skipInput   = rawByte.test(7); // first big-endian bit (is the last one in bitset)
        transferInst.outputIndex = static_cast<int>(outputIndex.to_ulong());

        transferInst.amount = NTP1AmountHexToNumber<decltype(transferInst.amount)>(
            boost::algorithm::hex(transferInst.rawAmount));

        // push to the vector
        result.push_back(transferInst);
    }
    return result;
}

std::shared_ptr<NTP1Script> NTP1Script::ParseScript(const std::string& scriptHex)
{
    try {
        std::string scriptBin = boost::algorithm::unhex(scriptHex);

        if (scriptBin.size() < 3) {
            throw std::runtime_error("Too short script");
        }
        std::string header = scriptBin.substr(0, 3);
        int         protocolVersion =
            static_cast<decltype(protocolVersion)>(*reinterpret_cast<uint8_t*>(&header[2]));

        // drop header bytes
        scriptBin.erase(scriptBin.begin(), scriptBin.begin() + 3);

        std::string opCodeBin = ParseOpCodeFromLongEnoughString(scriptBin);
        TxType      txType    = CalculateTxType(opCodeBin);

        // drop the OP_CODE parsed part
        scriptBin.erase(scriptBin.begin(), scriptBin.begin() + opCodeBin.size());

        std::shared_ptr<NTP1Script> result_;

        if (txType == TxType::TxType_Issuance) {
            result_ = NTP1Script_Issuance::ParseIssuancePostHeaderData(scriptBin, opCodeBin);
        } else if (txType == TxType::TxType_Transfer) {
            result_ = NTP1Script_Transfer::ParseTransferPostHeaderData(scriptBin, opCodeBin);
        } else if (txType == TxType::TxType_Burn) {
            result_ = NTP1Script_Burn::ParseBurnPostHeaderData(scriptBin, opCodeBin);
        } else {
            throw std::runtime_error("Unknown transaction type to parse");
        }
        result_->setCommonParams(header, protocolVersion, opCodeBin);

        result_->parsedScript = scriptHex;

        return result_;

    } catch (std::exception& ex) {
        throw std::runtime_error("Unable to parse hex script: " + scriptHex + "; reason: " + ex.what());
    }
}

NTP1Script::IssuanceFlags NTP1Script::IssuanceFlags::ParseIssuanceFlag(uint8_t flags)
{
    IssuanceFlags  result;
    std::bitset<8> bits(flags);
    // first 3 bits
    result.divisibility = static_cast<decltype(result.divisibility)>(
        std::bitset<3>(bits.to_string().substr(0, 3)).to_ulong());
    result.locked = bits.test(4); // 4th bit (3rd bit from the lsb, 7-(4-1)=3)
    // 5th + 6th bits
    int aggrPolicy = static_cast<decltype(result.divisibility)>(
        std::bitset<2>(bits.to_string().substr(4, 2)).to_ulong());
    switch (aggrPolicy) {
    case 0: // 00
        result.aggregationPolicy = AggregationPolicy::AggregationPolicy_Aggregatable;
        break;
    case 2: // 10
        result.aggregationPolicy = AggregationPolicy::AggregationPolicy_NonAggregatable;
        break;
    default: // everything else is not known (01 and 11)
        throw std::runtime_error("Unknown aggregation policy: " + std::to_string(aggrPolicy));
    }
    return result;
}
