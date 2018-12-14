#include "ntp1transaction.h"

#include "base58.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1tools.h"
#include "ntp1txin.h"
#include "ntp1txout.h"
#include "txdb.h"
#include "util.h"

#include <boost/algorithm/hex.hpp>

// token id vs max block to take transactions from these tokens
const ThreadSafeHashMap<std::string, int>
ntp1_blacklisted_token_ids(std::unordered_map<std::string, int>{
    {"La77KcJTUj991FnvxNKhrCD1ER8S81T3LgECS6", 300000}, // old QRT mainnet
    {"La4kVcoUAddLWkmQU9tBxrNdFjmSaHQruNJW2K", 300000}, // old TNIBB testnet
    {"La36YNY2G6qgBPj7VSiQDjGCy8aC2GUUsGqtbQ", 300000}, // old TNIBB testnet
    {"La9wLfpkfZTQvRqyiWjaEpgQStUbCSVMWZW2by", 300000}, // TEST3 on testnet
    {"La347xkKhi5VUCNDCqxXU4F1RUu8wPvC3pnQk6", 300000}, // BOT on testnet
    {"La531vUwiu9NnvtJcwPEjV84HrdKCupFCCb6D7", 300000}, // BAUTO on testnet
    {"La5JGnJcSsLCvYWxqqVSyj3VUqsrAcLBjZjbw5", 300000}, // XYZ from Sam on testnet
    {"La86PtvXGftbwdoZ9rVMKsLQU5nPHganJDsCRq", 300000}  // ON
    //    ,{"LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp", 300000}  // NIBBL
});

// list of transactions to be excluded because they're invalid
// this should be a thread-safe hashset, but we don't have one. So we're using the map.
const ThreadSafeHashMap<uint256, int>
excluded_txs_testnet(std::unordered_map<uint256, int>{
    {uint256("826e7b74b24e458e39d779b1033567d325b8d93b507282f983e3c4b3f950fca1"), 0},
    {uint256("c378447562be04c6803fdb9f829c9ba0dda462b269e15bcfc7fac3b3561d2eef"), 0},
    {uint256("a57a3e4746a79dd0d0e32e6a831d4207648ff000c82a4c5e8d9f3b6b0959f8b8"), 0},
    {uint256("7e71508abef696d6c0427cc85073e0d56da9380f3d333354c7dd9370acd422bc"), 0},
    {uint256("adb421a497e25375a88848b17b5c632a8d60db3d02dcc61dbecd397e6c1fb1ca"), 0},
    {uint256("adedc16e0318668e55f08f2a1ea57be8c5a86cfce3c1900346b0337a8f75a390"), 0},
    {uint256("bb8f1a29237e64285b9bd1f2bf1500c0de6205e8eb5e004c3b1ab6671e9c4cb2"), 0},
    {uint256("cc8f8a763677b8015bf79a19c9bcf87837b734d1cb203b30726af27b75f41a48"), 0},
    {uint256("666d81ad74e470ef1c9e74022a8be886e4951a0bec0d27f9b078519a30af71b2"), 0},
    {uint256("27bea35b4e2ac8987441aa7c5ff3d305047664ef7244b822cad54e549b84f50b"), 0},
    {uint256("59cb6e2cc9649d9a9b806f820a91927dcb0e43d1e1e92b0b9d976e921bba1334"), 0},
    {uint256("054cead1a3b498ec845462a1920508698e4f0ab2a71e1f4f8d827d007a43a2f4"), 0},
    {uint256("7d211b98e4796e9375233d935eb8d1262d6fb9d79645b576f15ad1b85427facf"), 0},
    {uint256("ab336eecf51cdaecd3f7444d5da7eca2286462d44e7f3439458ecbe3d7514971"), 0},
    {uint256("95c6f2b978160ab0d51545a13a7ee7b931713a52bd1c9f12807f4cd77ff7536b"), 0}});

const ThreadSafeHashMap<uint256, int> excluded_txs_mainnet = {};

bool IsNTP1TokenBlacklisted(const string& tokenId, int& maxHeight)
{
    return ntp1_blacklisted_token_ids.get(tokenId, maxHeight);
}

bool IsNTP1TokenBlacklisted(const string& tokenId) { return ntp1_blacklisted_token_ids.exists(tokenId); }

bool IsNTP1TxExcluded(const uint256& txHash)
{
    return excluded_txs_testnet.exists(txHash) || excluded_txs_mainnet.exists(txHash);
}

NTP1Transaction::NTP1Transaction() { setNull(); }

void NTP1Transaction::setNull()
{
    nVersion = NTP1Transaction::CURRENT_VERSION;
    nTime    = GetAdjustedTime() * 1000;
    vin.clear();
    vout.clear();
    nLockTime = 0;
}

bool NTP1Transaction::isNull() const noexcept { return (vin.empty() && vout.empty()); }

void NTP1Transaction::importJsonData(const std::string& data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);

        setHex(NTP1Tools::GetStrField(parsedData.get_obj(), "hex"));
        std::string hash = NTP1Tools::GetStrField(parsedData.get_obj(), "txid");
#ifdef DEBUG__INCLUDE_STR_HASH
        strHash = hash;
#endif
        txHash.SetHex(hash);
        nLockTime                   = NTP1Tools::GetUint64Field(parsedData.get_obj(), "locktime");
        nTime                       = NTP1Tools::GetUint64Field(parsedData.get_obj(), "time");
        nVersion                    = NTP1Tools::GetUint64Field(parsedData.get_obj(), "version");
        json_spirit::Array vin_list = NTP1Tools::GetArrayField(parsedData.get_obj(), "vin");
        vin.clear();
        vin.resize(vin_list.size());
        for (unsigned long i = 0; i < vin_list.size(); i++) {
            vin[i].importJsonData(vin_list[i]);
        }
        json_spirit::Array vout_list = NTP1Tools::GetArrayField(parsedData.get_obj(), "vout");
        vout.clear();
        vout.resize(vout_list.size());
        for (unsigned long i = 0; i < vout_list.size(); i++) {
            vout[i].importJsonData(vout_list[i]);
        }
    } catch (std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

json_spirit::Value NTP1Transaction::exportDatabaseJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("version", nVersion));
    root.push_back(json_spirit::Pair("txid", txHash.GetHex()));
    root.push_back(json_spirit::Pair("locktime", nLockTime));
    root.push_back(json_spirit::Pair("time", nTime));
    root.push_back(json_spirit::Pair("hex", getHex()));

    json_spirit::Array vinArray;
    for (long i = 0; i < static_cast<long>(vin.size()); i++) {
        vinArray.push_back(vin[i].exportDatabaseJsonData());
    }
    root.push_back(json_spirit::Pair("vin", json_spirit::Value(vinArray)));

    json_spirit::Array voutArray;
    for (long i = 0; i < static_cast<long>(vout.size()); i++) {
        voutArray.push_back(vout[i].exportDatabaseJsonData());
    }
    root.push_back(json_spirit::Pair("vout", json_spirit::Value(voutArray)));

    return json_spirit::Value(root);
}

void NTP1Transaction::importDatabaseJsonData(const json_spirit::Value& data)
{
    setNull();

    nVersion = NTP1Tools::GetUint64Field(data.get_obj(), "version");
    txHash.SetHex(NTP1Tools::GetStrField(data.get_obj(), "txid"));
#ifdef DEBUG__INCLUDE_STR_HASH
    strHash = NTP1Tools::GetStrField(data.get_obj(), "txid");
#endif
    nLockTime = NTP1Tools::GetUint64Field(data.get_obj(), "locktime");
    nTime     = NTP1Tools::GetUint64Field(data.get_obj(), "time");
    setHex(NTP1Tools::GetStrField(data.get_obj(), "hex"));

    json_spirit::Array vin_list = NTP1Tools::GetArrayField(data.get_obj(), "vin");
    vin.clear();
    vin.resize(vin_list.size());
    for (unsigned long i = 0; i < vin_list.size(); i++) {
        vin[i].importDatabaseJsonData(vin_list[i]);
    }

    json_spirit::Array vout_list = NTP1Tools::GetArrayField(data.get_obj(), "vout");
    vout.clear();
    vout.resize(vout_list.size());
    for (unsigned long i = 0; i < vout_list.size(); i++) {
        vout[i].importDatabaseJsonData(vout_list[i]);
    }
}

std::string NTP1Transaction::getHex() const
{
    std::string out;
    boost::algorithm::hex(txSerialized.begin(), txSerialized.end(), std::back_inserter(out));
    return out;
}

void NTP1Transaction::setHex(const std::string& Hex)
{
    txSerialized.clear();
    boost::algorithm::unhex(Hex.begin(), Hex.end(), std::back_inserter(txSerialized));
}

uint256 NTP1Transaction::getTxHash() const { return txHash; }

uint64_t NTP1Transaction::getLockTime() const { return nLockTime; }

uint64_t NTP1Transaction::getTime() const { return nTime; }

unsigned long NTP1Transaction::getTxInCount() const { return vin.size(); }

const NTP1TxIn& NTP1Transaction::getTxIn(unsigned long index) const { return vin[index]; }

unsigned long NTP1Transaction::getTxOutCount() const { return vout.size(); }

const NTP1TxOut& NTP1Transaction::getTxOut(unsigned long index) const { return vout[index]; }

NTP1TransactionType NTP1Transaction::getTxType() const { return ntp1TransactionType; }

string NTP1Transaction::getTokenSymbolIfIssuance() const
{
    std::string                 script    = getNTP1OpReturnScriptHex();
    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScript(script);
    if (scriptPtr->getTxType() != NTP1Script::TxType_Issuance) {
        throw std::runtime_error(
            "Attempted to get the token symbol of a non-issuance transaction. Txid: " +
            this->getTxHash().ToString() + "; and current tx type: " + ToString(scriptPtr->getTxType()));
    }
    std::shared_ptr<NTP1Script_Issuance> scriptPtrD =
        std::dynamic_pointer_cast<NTP1Script_Issuance>(scriptPtr);

    if (!scriptPtrD) {
        throw std::runtime_error("While getting token symbol for issuance tx, casting script pointer to "
                                 "transfer type failed: " +
                                 script);
    }
    return scriptPtrD->getTokenSymbol();
}

string NTP1Transaction::getTokenIdIfIssuance(string input0txid, unsigned int input0index) const
{
    std::string                 script    = getNTP1OpReturnScriptHex();
    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScript(script);
    if (scriptPtr->getTxType() != NTP1Script::TxType_Issuance) {
        throw std::runtime_error("Attempted to get the token id of a non-issuance transaction. Txid: " +
                                 this->getTxHash().ToString() +
                                 "; and current tx type: " + ToString(scriptPtr->getTxType()));
    }
    std::shared_ptr<NTP1Script_Issuance> scriptPtrD =
        std::dynamic_pointer_cast<NTP1Script_Issuance>(scriptPtr);

    if (!scriptPtrD) {
        throw std::runtime_error("While getting token id for issuance tx, casting script pointer to "
                                 "transfer type failed: " +
                                 script);
    }
    return scriptPtrD->getTokenID(input0txid, input0index);
}

void NTP1Transaction::updateDebugStrHash()
{
#ifdef DEBUG__INCLUDE_STR_HASH
    strHash = txHash.ToString();
#endif
}

std::unordered_map<string, TokenMinimalData>
NTP1Transaction::CalculateTotalInputTokens(const NTP1Transaction& ntp1tx)
{
    std::unordered_map<string, TokenMinimalData> result;
    for (const NTP1TxIn& in : ntp1tx.vin) {
        for (const NTP1TokenTxData& token : in.tokens) {
            const std::string& tokenId = token.getTokenId();
            if (result.find(tokenId) == result.end()) {
                TokenMinimalData tokenData;
                tokenData.amount    = token.getAmount();
                tokenData.tokenId   = token.getTokenId();
                tokenData.tokenName = token.getTokenSymbol();
                result[tokenId]     = tokenData;
            } else {
                result[tokenId].amount += token.getAmount();
            }
        }
    }
    return result;
}

std::unordered_map<string, TokenMinimalData>
NTP1Transaction::CalculateTotalOutputTokens(const NTP1Transaction& ntp1tx)
{
    std::unordered_map<string, TokenMinimalData> result;
    for (const NTP1TxOut& in : ntp1tx.vout) {
        for (const NTP1TokenTxData& token : in.tokens) {
            const std::string& tokenId = token.getTokenId();
            if (result.find(tokenId) == result.end()) {
                TokenMinimalData tokenData;
                tokenData.amount    = token.getAmount();
                tokenData.tokenId   = token.getTokenId();
                tokenData.tokenName = token.getTokenSymbol();
                result[tokenId]     = tokenData;
            } else {
                result[tokenId].amount += token.getAmount();
            }
        }
    }
    return result;
}

void NTP1Transaction::ReorderTokenInputsToGoFirst(
    CTransaction& tx, const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{

    EnsureInputTokensRelateToTx(tx, inputsTxs);
    EnsureInputsHashesMatch(inputsTxs);

    if (CountTokenKindsInInputs(tx, inputsTxs) == 0) {
        return;
    }

    // loop over vin's with no tokens, and swap them with ones that do to make them first
    for (int i = 0; i < (int)tx.vin.size(); i++) {
        auto it1 = GetPrevInputIt(tx, tx.vin[i].prevout.hash, inputsTxs);

        const NTP1Transaction& ntp1InTx1 = it1->second;

        // if there are no tokens in this instance, find next ones that do, and move tokens here
        if (ntp1InTx1.getTxOut(tx.vin[i].prevout.n).tokenCount() == 0) {
            for (int j = i + 1; j < (int)tx.vin.size(); j++) {
                auto it2 = GetPrevInputIt(tx, tx.vin[j].prevout.hash, inputsTxs);

                const NTP1Transaction& ntp1InTx2 = it2->second;

                if (ntp1InTx2.getTxOut(tx.vin[j].prevout.n).tokenCount() != 0) {
                    std::swap(tx.vin[i], tx.vin[j]);
                    break;
                }
            }
        }
    }
}

unsigned int NTP1Transaction::CountTokenKindsInInputs(
    const CTransaction& tx, const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{
    unsigned result = 0;

    for (const auto& in : tx.vin) {
        auto it = GetPrevInputIt(tx, in.prevout.hash, inputsTxs);

        const CTransaction&    neblInTx = it->first;
        const NTP1Transaction& ntp1InTx = it->second;

        if (in.prevout.n + 1 > ntp1InTx.getTxOutCount()) {
            throw std::runtime_error("Failed at retrieving the number of tokens from transaction " +
                                     tx.GetHash().ToString() + " at input " +
                                     neblInTx.GetHash().ToString() +
                                     "; input: " + ToString(in.prevout.n) + " is out of range.");
        }

        result += ntp1InTx.getTxOut(in.prevout.n).tokenCount();
    }

    return result;
}

void NTP1Transaction::EnsureInputsHashesMatch(
    const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{
    // ensure that input pairs match
    for (const auto& in : inputsTxs) {
        if (in.first.GetHash() != in.second.getTxHash()) {
            throw std::runtime_error(
                "Input transactions in the NTP1 parser do not have matching hashes.");
        }
    }
}

void NTP1Transaction::EnsureInputTokensRelateToTx(
    const CTransaction& tx, const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{
    // ensure that all inputs are relevant to this transaction (to protect from double-spending
    // tokens)
    for (unsigned i = 0; i < inputsTxs.size(); i++) {
        uint256 currentHash = inputsTxs[i].first.GetHash(); // the tx-hash of the input
        auto    it = std::find_if(tx.vin.begin(), tx.vin.end(), [&currentHash](const CTxIn& in) {
            return in.prevout.hash == currentHash;
        });
        if (it == tx.vin.end()) {
            throw std::runtime_error("An input was included in NTP1 transaction parser while it was not "
                                     "being spent by the spending transaction. This is not allowed.");
        }
    }
}

std::vector<std::pair<CTransaction, NTP1Transaction>>::const_iterator
NTP1Transaction::GetPrevInputIt(const CTransaction& tx, const uint256& inputTxHash,
                                const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{
    auto it = std::find_if(inputsTxs.cbegin(), inputsTxs.cend(),
                           [&inputTxHash](const std::pair<CTransaction, NTP1Transaction>& inPair) {
                               return inPair.first.GetHash() == inputTxHash;
                           });
    if (it == inputsTxs.end()) {
        throw std::runtime_error(
            "Could not find input related to transaction: " + tx.GetHash().ToString() +
            " with a prevout hash: " + inputTxHash.ToString());
    }

    return it;
}

void NTP1Transaction::AmendStdTxWithNTP1(CTransaction& tx, int changeIndex)
{
    CTxDB                                                 txdb;
    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs = GetAllNTP1InputsOfTx(tx, txdb, false);

    AmendStdTxWithNTP1(tx, inputs, changeIndex);
}

void NTP1Transaction::AmendStdTxWithNTP1(
    CTransaction& tx, const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputs,
    int changeIndex)
{
    // temp copy to avoid changing the original if the operation fails
    CTransaction tx_ = tx;

    EnsureInputsHashesMatch(inputs);

    EnsureInputTokensRelateToTx(tx_, inputs);

    unsigned inputTokenKinds = CountTokenKindsInInputs(tx_, inputs);

    bool txContainsOpReturn = TxContainsOpReturn(&tx_);

    // if no inputs contain NTP1 AND no OP_RETURN argument exists, then this is a pure NEBL transaction
    // with no NTP1
    if (inputTokenKinds == 0) {
        return;
    }

    // there are tokens, but there is already OP_RETURN
    if (txContainsOpReturn) {
        throw std::runtime_error("Cannot NTP1-amend transaction " + tx_.GetHash().ToString() +
                                 " because it already has an OP_RETURN output");
    }

    ReorderTokenInputsToGoFirst(tx_, inputs);

    if (!txContainsOpReturn && inputTokenKinds > 0) {
        // no OP_RETURN output, but there are input tokens to be diverted to output
        tx_.vout.push_back(CTxOut(0, CScript())); // pushed now, but will be filled later
        unsigned opRetIdx = tx_.vout.size() - 1;

        std::vector<NTP1Script::TransferInstruction> TIs;

        for (int i = 0; i < (int)tx_.vin.size(); i++) {
            const auto& inHash  = tx_.vin[i].prevout.hash;
            const auto& inIndex = tx_.vin[i].prevout.n;
            auto        it      = GetPrevInputIt(tx_, inHash, inputs);

            const CTransaction&    inputTxNebl = it->first;
            const NTP1Transaction& inputTxNTP1 = it->second;

            for (int j = 0; j < (int)inputTxNTP1.vout.at(inIndex).tokenCount(); j++) {
                if (inputTxNTP1.vout.at(inIndex).getToken(j).getAmount() == 0) {
                    if (i == 0) {
                        throw std::runtime_error("While amending a native neblio transactions, the "
                                                 "first input is empty. This basically cannot be "
                                                 "amended. Inputs must be reordered to have tokens in "
                                                 "first inputs, and it seems that reordering failed");
                    }
                    continue;
                }

                // prepare native Neblio output
                CTxDestination currentTokenAddress;
                // get the current address where the token is
                if (!ExtractDestination(inputTxNebl.vout.at(inIndex).scriptPubKey,
                                        currentTokenAddress)) {
                    throw std::runtime_error("Unable to extract address from previous output; tx: " +
                                             tx_.GetHash().ToString() + " and prevout: " +
                                             inHash.ToString() + ":" + ToString(inIndex));
                }

                if (changeIndex + 1 > (int)tx_.vout.size()) {
                    throw std::runtime_error("Invalid change index provided to fund moving NTP1 tokens");
                }

                if (changeIndex < 0) {
                    throw std::runtime_error(
                        "Could not find a change output from which NTP1 token moving can be funded");
                }

                if (tx_.vout.at(changeIndex).nValue < MIN_TX_FEE) {
                    throw std::runtime_error(
                        "Insufficient balance in change to create fund moving an NTP1 token");
                }

                CScript outputScript;
                outputScript.SetDestination(currentTokenAddress);
                // create a new output
                tx_.vout[changeIndex].nValue -= MIN_TX_FEE; // subtract
                tx_.vout.push_back(CTxOut(MIN_TX_FEE, outputScript));

                // create the transfer instruction
                NTP1Script::TransferInstruction ti;
                ti.amount    = inputTxNTP1.vout.at(inIndex).getToken(j).getAmount();
                ti.skipInput = false;
                // set the output index based on the number of outputs (since we added the last output)
                ti.outputIndex = tx_.vout.size() - 1;

                // push the transfer instruction
                TIs.push_back(ti);
            }
        }

        if (changeIndex + 1 > (int)tx_.vout.size()) {
            throw std::runtime_error("Invalid change index provided to fund moving NTP1 tokens");
        }

        if (changeIndex < 0) {
            throw std::runtime_error(
                "Could not find a change output from which NTP1 token moving can be funded");
        }

        if (tx_.vout.at(changeIndex).nValue < MIN_TX_FEE) {
            throw std::runtime_error(
                "Insufficient balance in change to create fund moving an NTP1 token");
        }

        // if the change left equals exactly the amount required to create OP_RETURN, make it there
        bool setOpRetAtChangeOutput = false;
        if (tx_.vout[changeIndex].nValue == MIN_TX_FEE) {
            setOpRetAtChangeOutput = true;
        }

        std::shared_ptr<NTP1Script_Transfer> scriptPtrT = NTP1Script_Transfer::CreateScript(TIs, "");

        std::string script    = scriptPtrT->calculateScriptBin();
        std::string scriptHex = boost::algorithm::hex(script);

        CScript outputScript = CScript() << OP_RETURN << ParseHex(scriptHex);
        if (setOpRetAtChangeOutput) {
            // use the change output to move the tokens there
            tx_.vout[changeIndex] = CTxOut(MIN_TX_FEE, outputScript);
            // delete the unused index since change index was replaced
            tx_.vout.erase(tx_.vout.begin() + opRetIdx);
        } else {
            tx_.vout[opRetIdx] = CTxOut(MIN_TX_FEE, outputScript);
            tx_.vout[changeIndex].nValue -= MIN_TX_FEE; // subtract
        }
    }

    // copy the result to the input
    tx = tx_;
}

void NTP1Transaction::__manualSet(int NVersion, uint256 TxHash, std::vector<unsigned char> TxSerialized,
                                  std::vector<NTP1TxIn> Vin, std::vector<NTP1TxOut> Vout,
                                  uint64_t NLockTime, uint64_t NTime,
                                  NTP1TransactionType Ntp1TransactionType)
{
    nVersion = NVersion;
    txHash   = TxHash;
#ifdef DEBUG__INCLUDE_STR_HASH
    strHash = TxHash.ToString();
#endif
    txSerialized        = TxSerialized;
    vin                 = Vin;
    vout                = Vout;
    nLockTime           = NLockTime;
    nTime               = NTime;
    ntp1TransactionType = Ntp1TransactionType;
}

string NTP1Transaction::getNTP1OpReturnScriptHex() const
{
    const static std::string NTP1OpReturnRegexStr = R"(^OP_RETURN\s+(4e5401[a-fA-F0-9]*)$)";
    const static std::regex  NTP1OpReturnRegex(NTP1OpReturnRegexStr);

    std::smatch opReturnArgMatch;
    std::string opReturnArg;

    for (unsigned long j = 0; j < vout.size(); j++) {
        std::string scriptPubKeyStr = vout[j].scriptPubKeyAsm;
        if (std::regex_match(scriptPubKeyStr, opReturnArgMatch, NTP1OpReturnRegex)) {
            if (opReturnArgMatch[1].matched) {
                opReturnArg = std::string(opReturnArgMatch[1]);
                return opReturnArg;
            }
        }
    }
    throw std::runtime_error("Could not extract NTP1 script from OP_RETURN for transaction " +
                             txHash.ToString());
}

void NTP1Transaction::readNTP1DataFromTx_minimal(const CTransaction& tx)
{
    txHash = tx.GetHash();
#ifdef DEBUG__INCLUDE_STR_HASH
    strHash = tx.GetHash().ToString();
#endif
    vin.clear();
    vin.resize(tx.vin.size());
    for (int i = 0; i < (int)tx.vin.size(); i++) {
        vin[i].setNull();
        vin[i].setPrevout(NTP1OutPoint(tx.vin[i].prevout.hash, tx.vin[i].prevout.n));
    }
    vout.clear();
    vout.resize(tx.vout.size());
    for (int i = 0; i < (int)tx.vout.size(); i++) {
        vout[i].nValue = tx.vout[i].nValue;
        vout[i].scriptPubKeyHex.clear();
        boost::algorithm::hex(tx.vout[i].scriptPubKey.begin(), tx.vout[i].scriptPubKey.end(),
                              std::back_inserter(vout[i].scriptPubKeyHex));
        vout[i].scriptPubKeyAsm = tx.vout[i].scriptPubKey.ToString();
        CTxDestination dest;
        if (ExtractDestination(tx.vout[i].scriptPubKey, dest)) {
            vout[i].setAddress(CBitcoinAddress(dest).ToString());
        } else {
            vout[i].setAddress("");
        }
    }
    ntp1TransactionType = NTP1TxType_NOT_NTP1;
}

void NTP1Transaction::readNTP1DataFromTx(
    const CTransaction& tx, const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{
    if (IsNTP1TxExcluded(tx.GetHash())) {
        return;
    }

    readNTP1DataFromTx_minimal(tx);

    std::string opReturnArg;
    if (!IsTxNTP1(&tx, &opReturnArg)) {
        ntp1TransactionType = NTP1TxType_NOT_NTP1;
        return;
    }

    if (tx.vin.size() != inputsTxs.size()) {
        throw std::runtime_error("The number of input transactions must match the number of inputs in "
                                 "the provided transaction. Error in tx: " +
                                 tx.GetHash().ToString());
    }

    uint64_t totalOutput = tx.GetValueOut();
    uint64_t totalInput  = 0;

    for (unsigned i = 0; i < tx.vin.size(); i++) {
        const auto& currInputHash  = tx.vin[i].prevout.hash;
        const auto& currInputIndex = tx.vin[i].prevout.n;

        vin[i].setPrevout(NTP1OutPoint(currInputHash, currInputIndex));

        // find inputs in the list of inputs and parse their OP_RETURN
        auto it = GetPrevInputIt(tx, vin[i].getPrevout().getHash(), inputsTxs);

        std::string opReturnArgInput;

        // The transaction that has an input that matches currInputHash
        const CTransaction&    currStdInput  = it->first;
        const NTP1Transaction& currNTP1Input = it->second;

        totalInput += currStdInput.vout.at(currInputIndex).nValue;

        // if the transaction is not NTP1, continue
        if (!IsTxNTP1(&currStdInput, &opReturnArgInput)) {
            continue;
        }

        // if the input is not an NTP1 transaction, then currNTP1Input.vout.size() is zero
        if (currNTP1Input.vout.size() > 0) {
            vin[i].tokens = currNTP1Input.vout.at(currInputIndex).tokens;
        }
    }

    EnsureInputsHashesMatch(inputsTxs);

    EnsureInputTokensRelateToTx(tx, inputsTxs);

    if (totalInput == 0) {
        throw std::runtime_error("Total input is zero; that's invalid; in transaction: " +
                                 tx.GetHash().ToString());
    }

    this->nTime     = tx.nTime;
    this->nLockTime = tx.nLockTime;

    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScript(opReturnArg);
    if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Issuance) {
        ntp1TransactionType = NTP1TxType_ISSUANCE;

        std::shared_ptr<NTP1Script_Issuance> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Issuance>(scriptPtr);

        if (!scriptPtrD) {
            throw std::runtime_error(
                "While parsing NTP1Transaction, casting script pointer to transfer type failed: " +
                opReturnArg);
        }

        if (static_cast<int64_t>(totalInput) - static_cast<int64_t>(totalOutput) <
            static_cast<int64_t>(IssuanceFee)) {
            throw std::runtime_error("Issuance fee is less than 10 nebls");
        }

        NTP1Int totalAmountLeft = scriptPtrD->getAmount();
        if (tx.vin.size() < 1) {
            throw std::runtime_error("Number of inputs is zero for transaction: " +
                                     tx.GetHash().ToString());
        }
        for (long i = 0; i < scriptPtrD->getTransferInstructionsCount(); i++) {
            NTP1TokenTxData ntp1tokenTxData;
            const auto&     instruction = scriptPtrD->getTransferInstruction(i);
            if (instruction.outputIndex >= tx.vout.size()) {
                throw std::runtime_error("An output of issuance is outside the available range of "
                                         "outputs in NTP1 OP_RETURN argument: " +
                                         opReturnArg + ", where the number of available outputs is " +
                                         ::ToString(tx.vout.size()) + " in transaction " +
                                         tx.GetHash().ToString());
            }
            NTP1Int currentAmount = instruction.amount;

            // ensure the output is larger than input
            if (totalAmountLeft < currentAmount) {
                throw std::runtime_error("The amount targeted to outputs in bigger than the amount "
                                         "issued in NTP1 OP_RETURN argument: " +
                                         opReturnArg);
            }

            totalAmountLeft -= currentAmount;
            ntp1tokenTxData.setAmount(currentAmount);
            ntp1tokenTxData.setAggregationPolicy(scriptPtrD->getAggregationPolicyStr());
            ntp1tokenTxData.setDivisibility(scriptPtrD->getDivisibility());
            ntp1tokenTxData.setTokenSymbol(scriptPtrD->getTokenSymbol());
            ntp1tokenTxData.setLockStatus(scriptPtrD->isLocked());
            ntp1tokenTxData.setIssueTxIdHex(tx.GetHash().ToString());
            ntp1tokenTxData.setTokenId(
                scriptPtrD->getTokenID(tx.vin[0].prevout.hash.ToString(), tx.vin[0].prevout.n));
            vout[instruction.outputIndex].tokens.push_back(ntp1tokenTxData);
        }

        // distribute the remainder of the issued tokens
        if (totalAmountLeft > 0) {
            if (vout.size() > 0) {
                NTP1TokenTxData ntp1tokenTxData;

                ntp1tokenTxData.setAmount(totalAmountLeft);
                totalAmountLeft = 0;
                ntp1tokenTxData.setAggregationPolicy(scriptPtrD->getAggregationPolicyStr());
                ntp1tokenTxData.setDivisibility(scriptPtrD->getDivisibility());
                ntp1tokenTxData.setTokenSymbol(scriptPtrD->getTokenSymbol());
                ntp1tokenTxData.setLockStatus(scriptPtrD->isLocked());
                ntp1tokenTxData.setIssueTxIdHex(tx.GetHash().ToString());
                ntp1tokenTxData.setTokenId(
                    scriptPtrD->getTokenID(tx.vin[0].prevout.hash.ToString(), tx.vin[0].prevout.n));
                vout.back().tokens.push_back(ntp1tokenTxData);
            } else {
                throw std::runtime_error(
                    "Unable to send token change to the last output; the number of outputs is zero.");
            }
        }

        // loop over all inputs and add their tokens to the last output
        for (const auto& in : tx.vin) {
            const uint256&      currHash  = in.prevout.hash;
            const unsigned int& currIndex = in.prevout.n;
            // find the input tx from the list of inputs that matches the input hash from the tx in
            // question
            auto it = GetPrevInputIt(tx, currHash, inputsTxs);

            const std::pair<CTransaction, NTP1Transaction>& input = *it;

            for (int i = 0; i < (int)input.second.vout[currIndex].tokenCount(); i++) {
                if (vout.size() > 0) {
                    vout.back().tokens.push_back(input.second.vout[currIndex].getToken(i));
                } else {
                    throw std::runtime_error("Unable to send token change to the last output; the "
                                             "number of outputs is zero.");
                }
            }
        }
    } else if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Transfer) {
        ntp1TransactionType = NTP1TxType_TRANSFER;
        std::shared_ptr<NTP1Script_Transfer> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);

        if (!scriptPtrD) {
            throw std::runtime_error(
                "While parsing NTP1Transaction, casting script pointer to transfer type failed: " +
                opReturnArg);
        }

        __TransferTokens<NTP1Script_Transfer>(scriptPtrD, tx, inputsTxs, false);

    } else if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Burn) {
        ntp1TransactionType = NTP1TxType_BURN;
        std::shared_ptr<NTP1Script_Burn> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Burn>(scriptPtr);

        if (!scriptPtrD) {
            throw std::runtime_error(
                "While parsing NTP1Transaction, casting script pointer to burn type failed: " +
                opReturnArg);
        }

        __TransferTokens<NTP1Script_Burn>(scriptPtrD, tx, inputsTxs, true);

    } else {
        ntp1TransactionType = NTP1TxType_UNKNOWN;
        throw std::runtime_error("Unknown NTP1 transaction type");
    }
}
