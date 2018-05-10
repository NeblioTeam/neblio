#include "ntp1txout.h"
#include "ntp1tools.h"

NTP1TxOut::NTP1TxOut()
{
    setNull();
}

NTP1TxOut::NTP1TxOut(int64_t nValueIn, const std::string &scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKeyHex = scriptPubKeyIn;
}

void NTP1TxOut::setNull()
{
    nValue = -1;
    scriptPubKeyHex.clear();
    tokens.clear();
}

bool NTP1TxOut::isNull() const
{
    return (nValue == -1);
}

void NTP1TxOut::importJsonData(const std::string &data)
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

void NTP1TxOut::importJsonData(const json_spirit::Value &parsedData)
{
    try {
        nValue = NTP1Tools::GetUint64Field(parsedData.get_obj(), "value");
        json_spirit::Object scriptPubKeyJsonObj = NTP1Tools::GetObjectField(parsedData.get_obj(), "scriptPubKey");
        scriptPubKeyHex = NTP1Tools::GetStrField(scriptPubKeyJsonObj, "hex");
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

int64_t NTP1TxOut::getValue() const
{
    return nValue;
}

const std::string& NTP1TxOut::getScriptPubKeyHex() const
{
    return scriptPubKeyHex;
}

const NTP1TokenTxData &NTP1TxOut::getToken(unsigned long index) const
{
    return tokens[index];
}

unsigned long NTP1TxOut::getNumOfTokens() const
{
    return tokens.size();
}
