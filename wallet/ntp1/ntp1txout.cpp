#include "ntp1txout.h"
#include "logging/logger.h"
#include "ntp1tools.h"

std::string NTP1TxOut::getAddress() const { return address; }

void NTP1TxOut::setAddress(const std::string& Address) { address = Address; }

typename NTP1TxOut::OutputType NTP1TxOut::getType() const
{
    NTP1TxOut::OutputType type;
    if (scriptPubKey.empty()) {
        type = OutputType::NonStandard;
    } else if (scriptPubKey[0] == OP_RETURN) {
        type = OutputType::OPReturn;
    } else {
        type = OutputType::NormalOutput;
    }

    return type;
}

std::string NTP1TxOut::getScriptPubKeyAsm() const { return scriptPubKey.ToString(); }

void NTP1TxOut::__manualSet(int64_t NValue, const CScript& ScriptPubKey,
                            const std::vector<NTP1TokenTxData>& Tokens, const std::string& Address)
{
    nValue       = NValue;
    scriptPubKey = ScriptPubKey;
    tokens       = Tokens;
    address      = Address;
}

void NTP1TxOut::__manualSet(int64_t NValue, const std::string& ScriptPubKeyHex,
                            const std::vector<NTP1TokenTxData>& Tokens, const std::string& Address)
{
    const std::string scriptPubKeyBin = boost::algorithm::unhex(ScriptPubKeyHex);
    scriptPubKey.clear();
    std::copy(scriptPubKeyBin.begin(), scriptPubKeyBin.end(), std::back_inserter(scriptPubKey));

    nValue  = NValue;
    tokens  = Tokens;
    address = Address;
}

void NTP1TxOut::setNValue(const int64_t& value) { nValue = value; }

void NTP1TxOut::setScriptPubKey(const CScript& value) { scriptPubKey = value; }

void NTP1TxOut::__addToken(const NTP1TokenTxData& token) { tokens.push_back(token); }

NTP1TxOut::NTP1TxOut() { setNull(); }

NTP1TxOut::NTP1TxOut(int64_t nValueIn, const CScript& scriptPubKeyIn)
{
    nValue       = nValueIn;
    scriptPubKey = scriptPubKeyIn;
}

void NTP1TxOut::setNull()
{
    nValue = -1;
    scriptPubKey.clear();
    tokens.clear();
}

bool NTP1TxOut::isNull() const { return (nValue == -1); }

void NTP1TxOut::importJsonData(const std::string& data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);
        importJsonData(parsedData);
    } catch (std::exception& ex) {
        NLog.write(b_sev::info, "{}", ex.what());
        throw;
    }
}

void NTP1TxOut::importJsonData(const json_spirit::Value& parsedData)
{
    try {
        nValue = NTP1Tools::GetUint64Field(parsedData.get_obj(), "value");
        json_spirit::Object scriptPubKeyJsonObj =
            NTP1Tools::GetObjectField(parsedData.get_obj(), "scriptPubKey");

        std::string scriptPubKeyBin =
            boost::algorithm::unhex(NTP1Tools::GetStrField(scriptPubKeyJsonObj, "hex"));
        scriptPubKey.clear();
        std::copy(scriptPubKeyBin.begin(), scriptPubKeyBin.end(), std::back_inserter(scriptPubKey));

        if (getType() == OutputType::NormalOutput) {
            json_spirit::Array addresses = NTP1Tools::GetArrayField(scriptPubKeyJsonObj, "addresses");
            if (addresses.size() != 1) {
                throw std::runtime_error(
                    "Addresses field in scriptPubKey has a size != 1 for normal outputs");
            }
            address = addresses[0].get_str();
        }
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
        NLog.write(b_sev::info, "{}", ex.what());
        throw;
    }
}

json_spirit::Value NTP1TxOut::exportDatabaseJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("value", nValue));
    root.push_back(json_spirit::Pair("scriptPubKey", ToHex(scriptPubKey)));
    root.push_back(json_spirit::Pair("scriptPubKeyAsm", scriptPubKey.ToString()));
    root.push_back(json_spirit::Pair("address", address));
    json_spirit::Array tokensArray;
    for (long i = 0; i < static_cast<long>(tokens.size()); i++) {
        tokensArray.push_back(tokens[i].exportDatabaseJsonData());
    }
    root.push_back(json_spirit::Pair("tokens", json_spirit::Value(tokensArray)));

    return json_spirit::Value(root);
}

void NTP1TxOut::importDatabaseJsonData(const json_spirit::Value& data)
{
    setNull();

    nValue = NTP1Tools::GetUint64Field(data.get_obj(), "value");

    std::string scriptPubKeyBin =
        boost::algorithm::unhex(NTP1Tools::GetStrField(data.get_obj(), "scriptPubKey"));
    scriptPubKey.clear();
    std::copy(scriptPubKeyBin.begin(), scriptPubKeyBin.end(), std::back_inserter(scriptPubKey));

    address                        = NTP1Tools::GetStrField(data.get_obj(), "address");
    json_spirit::Array tokens_list = NTP1Tools::GetArrayField(data.get_obj(), "tokens");
    tokens.clear();
    tokens.resize(tokens_list.size());
    for (unsigned long i = 0; i < tokens_list.size(); i++) {
        tokens[i].importDatabaseJsonData(tokens_list[i]);
    }
}

int64_t NTP1TxOut::getValue() const { return nValue; }

std::string NTP1TxOut::getScriptPubKeyHex() const { return ToHex(scriptPubKey); }

const NTP1TokenTxData& NTP1TxOut::getToken(unsigned long index) const { return tokens[index]; }

NTP1TokenTxData& NTP1TxOut::getToken(unsigned long index) { return tokens[index]; }

unsigned long NTP1TxOut::tokenCount() const { return tokens.size(); }
