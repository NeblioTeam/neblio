#include "ntp1txout.h"
#include "ntp1tools.h"

std::string NTP1TxOut::getAddress() const { return address; }

void NTP1TxOut::setAddress(const std::string& Address) { address = Address; }

typename NTP1TxOut::OutputType NTP1TxOut::getType() const
{
    NTP1TxOut::OutputType type;
    if (scriptPubKeyAsm.empty()) {
        type = OutputType::NonStandard;
    } else if (scriptPubKeyAsm.find("OP_RETURN") != std::string::npos) {
        type = OutputType::OPReturn;
    } else {
        type = OutputType::NormalOutput;
    }

    return type;
}

std::string NTP1TxOut::getScriptPubKeyAsm() const { return scriptPubKeyAsm; }

void NTP1TxOut::__manualSet(int64_t NValue, std::string ScriptPubKeyHex, std::string ScriptPubKeyAsm,
                            std::vector<NTP1TokenTxData> Tokens, std::string Address)
{
    nValue          = NValue;
    scriptPubKeyHex = ScriptPubKeyHex;
    scriptPubKeyAsm = ScriptPubKeyAsm;
    tokens          = Tokens;
    address         = Address;
}

void NTP1TxOut::setNValue(const int64_t& value) { nValue = value; }

void NTP1TxOut::setScriptPubKeyHex(const std::string& value) { scriptPubKeyHex = value; }

void NTP1TxOut::setScriptPubKeyAsm(const std::string& value) { scriptPubKeyAsm = value; }

void NTP1TxOut::__addToken(const NTP1TokenTxData& token) { tokens.push_back(token); }

NTP1TxOut::NTP1TxOut() { setNull(); }

NTP1TxOut::NTP1TxOut(int64_t nValueIn, const std::string& scriptPubKeyIn)
{
    nValue          = nValueIn;
    scriptPubKeyHex = scriptPubKeyIn;
}

void NTP1TxOut::setNull()
{
    nValue = -1;
    scriptPubKeyHex.clear();
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
        printf("%s", ex.what());
        throw;
    }
}

void NTP1TxOut::importJsonData(const json_spirit::Value& parsedData)
{
    try {
        nValue = NTP1Tools::GetUint64Field(parsedData.get_obj(), "value");
        json_spirit::Object scriptPubKeyJsonObj =
            NTP1Tools::GetObjectField(parsedData.get_obj(), "scriptPubKey");
        scriptPubKeyHex = NTP1Tools::GetStrField(scriptPubKeyJsonObj, "hex");
        scriptPubKeyAsm = NTP1Tools::GetStrField(scriptPubKeyJsonObj, "asm");
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
        printf("%s", ex.what());
        throw;
    }
}

json_spirit::Value NTP1TxOut::exportDatabaseJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("value", nValue));
    root.push_back(json_spirit::Pair("scriptPubKey", scriptPubKeyHex));
    root.push_back(json_spirit::Pair("scriptPubKeyAsm", scriptPubKeyAsm));
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

    nValue                         = NTP1Tools::GetUint64Field(data.get_obj(), "value");
    scriptPubKeyHex                = NTP1Tools::GetStrField(data.get_obj(), "scriptPubKey");
    scriptPubKeyAsm                = NTP1Tools::GetStrField(data.get_obj(), "scriptPubKeyAsm");
    address                        = NTP1Tools::GetStrField(data.get_obj(), "address");
    json_spirit::Array tokens_list = NTP1Tools::GetArrayField(data.get_obj(), "tokens");
    tokens.clear();
    tokens.resize(tokens_list.size());
    for (unsigned long i = 0; i < tokens_list.size(); i++) {
        tokens[i].importDatabaseJsonData(tokens_list[i]);
    }
}

int64_t NTP1TxOut::getValue() const { return nValue; }

const std::string& NTP1TxOut::getScriptPubKeyHex() const { return scriptPubKeyHex; }

const NTP1TokenTxData& NTP1TxOut::getToken(unsigned long index) const { return tokens[index]; }

NTP1TokenTxData& NTP1TxOut::getToken(unsigned long index) { return tokens[index]; }

unsigned long NTP1TxOut::tokenCount() const { return tokens.size(); }
