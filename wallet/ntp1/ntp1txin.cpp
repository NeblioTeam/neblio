#include "ntp1txin.h"

void NTP1TxIn::setPrevout(const NTP1OutPoint& value) { prevout = value; }

NTP1OutPoint NTP1TxIn::getPrevout() const { return prevout; }

void NTP1TxIn::setSequence(const uint64_t& value) { nSequence = value; }

NTP1TxIn::NTP1TxIn() { nSequence = std::numeric_limits<unsigned int>::max(); }

void NTP1TxIn::setNull()
{
    prevout.setNull();
    scriptSigHex.clear();
    nSequence = 0;
    tokens.clear();
}

void NTP1TxIn::importJsonData(const json_spirit::Value& parsedData)
{
    try {
        std::string txid_str = NTP1Tools::GetStrField(parsedData.get_obj(), "txid");
        uint256     txid;
        txid.SetHex(txid_str);
        nSequence                 = NTP1Tools::GetUint64Field(parsedData.get_obj(), "sequence");
        unsigned int        index = NTP1Tools::GetUint64Field(parsedData.get_obj(), "vout");
        json_spirit::Object scriptSigJsonObj =
            NTP1Tools::GetObjectField(parsedData.get_obj(), "scriptSig");
        prevout      = NTP1OutPoint(txid, index);
        scriptSigHex = NTP1Tools::GetStrField(scriptSigJsonObj, "hex");
        json_spirit::Array tokens_list;
        if (!json_spirit::find_value(parsedData.get_obj(), "tokens").is_null()) {
            tokens_list = NTP1Tools::GetArrayField(parsedData.get_obj(), "tokens");
        }
        tokens.clear();
        tokens.resize(tokens_list.size());
        for (unsigned long i = 0; i < tokens_list.size(); i++) {
            tokens[i].importJsonData(tokens_list[i]);
        }
    } catch (std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

json_spirit::Value NTP1TxIn::exportDatabaseJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("prevout", prevout.exportDatabaseJsonData()));
    root.push_back(json_spirit::Pair("scriptSig", scriptSigHex));
    root.push_back(json_spirit::Pair("sequence", nSequence));
    json_spirit::Array tokensArray;

    for (long i = 0; i < static_cast<long>(tokens.size()); i++) {
        tokensArray.push_back(tokens[i].exportDatabaseJsonData());
    }
    root.push_back(json_spirit::Pair("tokens", json_spirit::Value(tokensArray)));

    return json_spirit::Value(root);
}

void NTP1TxIn::importDatabaseJsonData(const json_spirit::Value& data)
{
    setNull();

    prevout.importDatabaseJsonData(NTP1Tools::GetObjectField(data.get_obj(), "prevout"));
    scriptSigHex                   = NTP1Tools::GetStrField(data.get_obj(), "scriptSig");
    nSequence                      = NTP1Tools::GetUint64Field(data.get_obj(), "sequence");
    json_spirit::Array tokens_list = NTP1Tools::GetArrayField(data.get_obj(), "tokens");
    tokens.clear();
    tokens.resize(tokens_list.size());
    for (unsigned long i = 0; i < tokens_list.size(); i++) {
        tokens[i].importDatabaseJsonData(tokens_list[i]);
    }
}
void NTP1TxIn::importJsonData(const std::string& data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);
        importJsonData(parsedData);
    } catch (std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

NTP1OutPoint NTP1TxIn::getOutPoint() const { return prevout; }

std::string NTP1TxIn::getScriptSigHex() const { return scriptSigHex; }

void NTP1TxIn::setScriptSigHex(const std::string& s) { scriptSigHex = s; }

uint64_t NTP1TxIn::getSequence() const { return nSequence; }

const NTP1TokenTxData& NTP1TxIn::getToken(unsigned long index) const { return tokens[index]; }

unsigned long NTP1TxIn::getNumOfTokens() const { return tokens.size(); }

void NTP1TxIn::__addToken(const NTP1TokenTxData& token) { tokens.push_back(token); }
