#include "ntp1txin.h"

NTP1TxIn::NTP1TxIn()
{
    nSequence = std::numeric_limits<unsigned int>::max();
}

void NTP1TxIn::importJsonData(const json_spirit::Value& parsedData)
{
    try {
        std::string txid_str = NTP1Tools::GetStrField(parsedData.get_obj(), "txid");
        uint256 txid;
        txid.SetHex(txid_str);
        nSequence = NTP1Tools::GetUint64Field(parsedData.get_obj(), "sequence");
        unsigned int index = NTP1Tools::GetUint64Field(parsedData.get_obj(), "vout");
        json_spirit::Object scriptSigJsonObj = NTP1Tools::GetObjectField(parsedData.get_obj(), "scriptSig");
        prevout = NTP1OutPoint(txid, index);
        scriptSigHex = NTP1Tools::GetStrField(scriptSigJsonObj, "hex");
        json_spirit::Array tokens_list = NTP1Tools::GetArrayField(parsedData.get_obj(), "tokens");
        tokens.clear();
        tokens.resize(tokens_list.size());
        for(unsigned long i = 0; i < tokens_list.size(); i++) {
            tokens[i].importJsonData(tokens_list[i]);
        }
    } catch(std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}
void NTP1TxIn::importJsonData(const std::string &data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);
        importJsonData(parsedData);
    } catch(std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

NTP1OutPoint NTP1TxIn::getOutPoint() const
{
    return prevout;
}

std::string NTP1TxIn::getScriptSigHex() const
{
    return scriptSigHex;
}

uint64_t NTP1TxIn::getSequence() const
{
    return nSequence;
}

const NTP1TokenTxData &NTP1TxIn::getToken(unsigned long index) const
{
    return tokens[index];
}

unsigned long NTP1TxIn::getNumOfTokens() const
{
    return tokens.size();
}
