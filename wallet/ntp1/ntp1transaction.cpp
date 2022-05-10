#include "ntp1transaction.h"

#include "base58.h"
#include "block.h"
#include "main.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1/ntp1v1_issuance_static_data.h"
#include "ntp1tools.h"
#include "ntp1txin.h"
#include "ntp1txout.h"
#include "txdb.h"
#include "txindex.h"
#include "txmempool.h"
#include "util.h"

#include <boost/algorithm/hex.hpp>

const std::array<std::array<uint8_t, 3>, 2> NTP1ScriptPrefixes{
    std::array<uint8_t, 3>({0x4e, 0x54, 0x03}), std::array<uint8_t, 3>({0x4e, 0x54, 0x01})};

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

        std::string hash = NTP1Tools::GetStrField(parsedData.get_obj(), "txid");
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
        NLog.write(b_sev::err, "{}", ex.what());
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
    nLockTime = NTP1Tools::GetUint64Field(data.get_obj(), "locktime");
    nTime     = NTP1Tools::GetUint64Field(data.get_obj(), "time");

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

uint256 NTP1Transaction::getTxHash() const { return txHash; }

uint64_t NTP1Transaction::getLockTime() const { return nLockTime; }

uint64_t NTP1Transaction::getTime() const { return nTime; }

unsigned long NTP1Transaction::getTxInCount() const { return vin.size(); }

const NTP1TxIn& NTP1Transaction::getTxIn(unsigned long index) const { return vin[index]; }

unsigned long NTP1Transaction::getTxOutCount() const { return vout.size(); }

const NTP1TxOut& NTP1Transaction::getTxOut(unsigned long index) const { return vout[index]; }

NTP1TransactionType NTP1Transaction::getTxType() const { return ntp1TransactionType; }

std::string NTP1Transaction::getTokenSymbolIfIssuance() const
{
    std::string                 script    = getNTP1OpReturnScriptHex();
    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScriptHex(script);
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

std::string NTP1Transaction::getTokenIdIfIssuance(std::string input0txid, unsigned int input0index) const
{
    std::string                 script    = getNTP1OpReturnScriptHex();
    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScriptHex(script);
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

std::unordered_map<std::string, TokenMinimalData>
NTP1Transaction::CalculateTotalInputTokens(const NTP1Transaction& ntp1tx)
{
    std::unordered_map<std::string, TokenMinimalData> result;
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

std::unordered_map<std::string, TokenMinimalData>
NTP1Transaction::CalculateTotalOutputTokens(const NTP1Transaction& ntp1tx)
{
    std::unordered_map<std::string, TokenMinimalData> result;
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
            "While reading/parsing NTP1 transction: Could not find input related to transaction: " +
            tx.GetHash().ToString() + " with a prevout hash: " + inputTxHash.ToString());
    }

    return it;
}

void NTP1Transaction::AmendStdTxWithNTP1(const ITxDB& txdb, CTransaction& tx, int changeIndex)
{
    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs = GetAllNTP1InputsOfTx(tx, txdb, false);

    AmendStdTxWithNTP1(txdb.GetBestChainHeight(), tx, inputs, changeIndex);
}

void NTP1Transaction::AmendStdTxWithNTP1(
    const int blockHeight, CTransaction& tx,
    const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputs, int changeIndex)
{
    // temp copy to avoid changing the original if the operation fails
    CTransaction tx_ = tx;

    EnsureInputsHashesMatch(inputs);

    EnsureInputTokensRelateToTx(tx_, inputs);

    unsigned inputTokenKinds = CountTokenKindsInInputs(tx_, inputs);

    bool txContainsOpReturn = tx_.ContainsOpReturn();

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
                if (!ExtractDestination(blockHeight, inputTxNebl.vout.at(inIndex).scriptPubKey,
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

void NTP1Transaction::__manualSet(int NVersion, uint256 TxHash, std::vector<NTP1TxIn> Vin,
                                  std::vector<NTP1TxOut> Vout, uint64_t NLockTime, uint64_t NTime,
                                  NTP1TransactionType Ntp1TransactionType)
{
    nVersion            = NVersion;
    txHash              = TxHash;
    vin                 = Vin;
    vout                = Vout;
    nLockTime           = NLockTime;
    nTime               = NTime;
    ntp1TransactionType = Ntp1TransactionType;
}

bool NTP1Transaction::IsScriptNTP1(const std::vector<uint8_t>& opReturnScript)
{
    for (const std::array<uint8_t, 3>& prefix : NTP1ScriptPrefixes) {
        if (opReturnScript.size() >= prefix.size() &&
            std::equal(prefix.begin(), prefix.end(), opReturnScript.begin())) {
            return true;
        }
    }
    return false;
}

std::vector<uint8_t> NTP1Transaction::getNTP1OpReturnScript() const
{
    for (unsigned long j = 0; j < vout.size(); j++) {
        const CScript& scriptPubKey = vout[j].scriptPubKey;
        if (scriptPubKey.size() > 2 && scriptPubKey.at(0) == OP_RETURN) {
            const std::vector<uint8_t> opReturnScript = CTransaction::ExtractOpRetData(scriptPubKey);
            if (IsScriptNTP1(opReturnScript)) {
                return opReturnScript;
            }
            break; // only one script is allowed per transaction
        }
    }
    throw std::runtime_error("Could not extract NTP1 script from OP_RETURN for transaction " +
                             txHash.ToString());
}

std::string NTP1Transaction::getNTP1OpReturnScriptHex() const { return ToHex(getNTP1OpReturnScript()); }

void NTP1Transaction::readNTP1DataFromTx_minimal(const int blockHeight, const CTransaction& tx)
{
    txHash = tx.GetHash();
    vin.clear();
    vin.resize(tx.vin.size());
    for (int i = 0; i < (int)tx.vin.size(); i++) {
        vin[i].setNull();
        vin[i].setPrevout(NTP1OutPoint(tx.vin[i].prevout.hash, tx.vin[i].prevout.n));
        std::string scriptSig;
        boost::algorithm::hex(tx.vin[i].scriptSig.begin(), tx.vin[i].scriptSig.end(),
                              std::back_inserter(scriptSig));
        vin[i].setScriptSigHex(scriptSig);
    }
    vout.clear();
    vout.resize(tx.vout.size());
    for (int i = 0; i < (int)tx.vout.size(); i++) {
        vout[i].nValue       = tx.vout[i].nValue;
        vout[i].scriptPubKey = tx.vout[i].scriptPubKey;
        CTxDestination dest;
        if (ExtractDestination(blockHeight, tx.vout[i].scriptPubKey, dest)) {
            vout[i].setAddress(CBitcoinAddress(dest).ToString());
        } else {
            vout[i].setAddress("");
        }
    }
    ntp1TransactionType = NTP1TxType_NOT_NTP1;
}

void NTP1Transaction::readNTP1DataFromTx(
    const int blockHeight, const CTransaction& tx,
    const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{
    if (Params().IsNTP1TxExcluded(tx.GetHash())) {
        return;
    }

    readNTP1DataFromTx_minimal(blockHeight, tx);

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

    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScriptHex(opReturnArg);
    if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Issuance) {
        ntp1TransactionType = NTP1TxType_ISSUANCE;

        std::shared_ptr<NTP1Script_Issuance> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Issuance>(scriptPtr);

        if (!scriptPtrD) {
            throw std::runtime_error(
                "While parsing NTP1Transaction, casting script pointer to issuance type failed: " +
                opReturnArg);
        }

        int64_t feeProvided = static_cast<int64_t>(totalInput) - static_cast<int64_t>(totalOutput);
        if (feeProvided < static_cast<int64_t>(IssuanceFee)) {
            throw std::runtime_error("Issuance fee is less than 10 nebls. You provided only: " +
                                     FormatMoney(feeProvided));
        }

        NTP1Int totalAmountLeft = scriptPtrD->getAmount();
        if (tx.vin.size() < 1) {
            throw std::runtime_error("Number of inputs is zero for transaction: " +
                                     tx.GetHash().ToString());
        }
        for (unsigned i = 0; i < scriptPtrD->getTransferInstructionsCount(); i++) {
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

        __TransferTokens<NTP1Script_Transfer>(blockHeight, scriptPtrD, tx, inputsTxs, false);

    } else if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Burn) {
        ntp1TransactionType = NTP1TxType_BURN;
        std::shared_ptr<NTP1Script_Burn> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Burn>(scriptPtr);

        if (!scriptPtrD) {
            throw std::runtime_error(
                "While parsing NTP1Transaction, casting script pointer to burn type failed: " +
                opReturnArg);
        }

        __TransferTokens<NTP1Script_Burn>(blockHeight, scriptPtrD, tx, inputsTxs, true);

    } else {
        ntp1TransactionType = NTP1TxType_UNKNOWN;
        throw std::runtime_error("Unknown NTP1 transaction type");
    }
}

json_spirit::Value NTP1Transaction::GetNTP1IssuanceMetadata(const int      blockHeight,
                                                            const uint256& issuanceTxid)
{
    CTransaction    tx = CTransaction::FetchTxFromDisk(issuanceTxid, CTxDB());
    NTP1Transaction ntp1tx;
    ntp1tx.readNTP1DataFromTx_minimal(blockHeight, tx);
    std::string opRet;
    bool        isNTP1 = IsTxNTP1(&tx, &opRet);
    if (!isNTP1) {
        return json_spirit::Value();
    }

    std::shared_ptr<NTP1Script>          s  = NTP1Script::ParseScriptHex(opRet);
    std::shared_ptr<NTP1Script_Issuance> sd = std::dynamic_pointer_cast<NTP1Script_Issuance>(s);
    if (!sd || s->getTxType() != NTP1Script::TxType_Issuance) {
        return json_spirit::Value();
    }
    if (s->getProtocolVersion() == 1) {
        if (tx.vin.empty()) {
            return json_spirit::Value();
        }
        const auto& prevout0 = tx.vin[0].prevout;
        try {
            std::string tokenId = ntp1tx.getTokenIdIfIssuance(prevout0.hash.ToString(), prevout0.n);
            return GetNTP1v1IssuanceMetadataNode(tokenId);
        } catch (std::exception& ex) {
            return json_spirit::Value();
        }
    } else if (s->getProtocolVersion() == 3) {
        return NTP1Script::GetMetadataAsJson(sd.get(), tx);
    } else {
        return json_spirit::Value();
    }
}

NTP1TokenMetaData NTP1Transaction::GetFullNTP1IssuanceMetadata(const CTransaction&    issuanceTx,
                                                               const NTP1Transaction& ntp1IssuanceTx)
{
    if (issuanceTx.GetHash() != ntp1IssuanceTx.getTxHash()) {
        throw std::runtime_error("For GetFullNTP1IssuanceMetadata(), the NTP1 transaction doesn't match "
                                 "the standard transaction");
    }
    uint256     issuanceTxid = issuanceTx.GetHash();
    std::string opRet;
    bool        isNTP1 = IsTxNTP1(&issuanceTx, &opRet);
    if (!isNTP1) {
        throw std::runtime_error("A non-NTP1 transaction was proided (txid: " + issuanceTxid.ToString() +
                                 ") to get NTP1 issuance metadata");
    }

    std::shared_ptr<NTP1Script>          s  = NTP1Script::ParseScriptHex(opRet);
    std::shared_ptr<NTP1Script_Issuance> sd = std::dynamic_pointer_cast<NTP1Script_Issuance>(s);
    if (!sd || s->getTxType() != NTP1Script::TxType_Issuance) {
        throw std::runtime_error("A non-issuance NTP1 transaction was provided (txid: " +
                                 issuanceTxid.ToString() + ") to retrieve issuance metadata");
    }
    const auto& prevout0 = issuanceTx.vin[0].prevout;
    std::string tokenId  = ntp1IssuanceTx.getTokenIdIfIssuance(prevout0.hash.ToString(), prevout0.n);
    if (s->getProtocolVersion() == 1) {
        if (issuanceTx.vin.empty()) {
            throw std::runtime_error(
                "An invalid NTP1 transaction was provided (txid: " + issuanceTxid.ToString() +
                ") to retrieve issuance metadata. The transaction has zero inputs.");
        }
        try {
            NTP1TokenMetaData result;
            result.readSomeDataFromStandardJsonFormat(GetNTP1v1IssuanceMetadataNode(tokenId));
            result.readSomeDataFromNTP1IssuanceScript(sd.get());
            result.setTokenId(tokenId);
            result.setIssuanceTxId(issuanceTxid);
            return result;
        } catch (std::exception& ex) {
            throw std::runtime_error("Failed to get NTP1 transaction metadata for txid: " +
                                     issuanceTxid.ToString() + " . Error: " + std::string(ex.what()));
        }
    } else if (s->getProtocolVersion() == 3) {
        NTP1TokenMetaData result;
        result.readSomeDataFromStandardJsonFormat(NTP1Script::GetMetadataAsJson(sd.get(), issuanceTx));
        result.readSomeDataFromNTP1IssuanceScript(sd.get());
        result.setTokenId(tokenId);
        result.setIssuanceTxId(issuanceTxid);
        return result;
    } else {
        throw std::runtime_error("Failed to get NTP1 transaction metadata for txid: " +
                                 issuanceTxid.ToString() + " . Unknown NTP1 protocol version");
    }
}

NTP1TokenMetaData NTP1Transaction::GetFullNTP1IssuanceMetadata(const int      blockHeight,
                                                               const uint256& issuanceTxid)
{
    CTransaction    tx = CTransaction::FetchTxFromDisk(issuanceTxid, CTxDB());
    NTP1Transaction ntp1tx;
    ntp1tx.readNTP1DataFromTx_minimal(blockHeight, tx);
    CDataStream ds1(SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ds2(SER_NETWORK, PROTOCOL_VERSION);
    return GetFullNTP1IssuanceMetadata(tx, ntp1tx);
}

std::vector<std::pair<CTransaction, NTP1Transaction>>
NTP1Transaction::GetAllNTP1InputsOfTx(CTransaction tx, const ITxDB& txdb, bool recoverProtection,
                                      int recursionCount)
{
    return GetAllNTP1InputsOfTx(
        tx, txdb, recoverProtection,
        std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>(),
        std::map<uint256, CTxIndex>(), recursionCount);
}

std::vector<std::pair<CTransaction, NTP1Transaction>> NTP1Transaction::GetAllNTP1InputsOfTx(
    CTransaction tx, const ITxDB& txdb, bool recoverProtection,
    const std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>& mapQueuedNTP1Inputs,
    const std::map<uint256, CTxIndex>& queuedAcceptedTxs, int recursionCount)
{
    // rertrieve standard transaction inputs (NOT NTP1)
    MapPrevTx mapInputs;
    bool      fInvalid = false;
    if (!tx.FetchInputs(txdb, queuedAcceptedTxs, true, false, mapInputs, fInvalid)) {
        if (fInvalid) {
            NLog.write(b_sev::err, "Error: For GetAllNTP1InputsOfTx, FetchInputs found invalid tx {}",
                       tx.GetHash().ToString());
            throw std::runtime_error("Error: For NTP1, FetchInputs found invalid tx " +
                                     tx.GetHash().ToString());
        }
    }

    std::vector<std::pair<CTransaction, NTP1Transaction>> result = StdFetchedInputTxsToNTP1(
        tx, mapInputs, txdb, recoverProtection, mapQueuedNTP1Inputs, queuedAcceptedTxs, recursionCount);
    return result;
}

std::vector<std::pair<CTransaction, NTP1Transaction>> NTP1Transaction::StdFetchedInputTxsToNTP1(
    const CTransaction& tx, const MapPrevTx& mapInputs, const ITxDB& txdb, bool recoverProtection,
    const std::map<uint256, CTxIndex>& queuedAcceptedTxs, int recursionCount)
{
    // It's not possible to use default parameter here because NTP1Transaction is an incomplete type in
    // main.h, and including NTP1Transaction header file is not possible due to circular dependency
    return StdFetchedInputTxsToNTP1(
        tx, mapInputs, txdb, recoverProtection,
        std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>(), queuedAcceptedTxs,
        recursionCount);
}

std::vector<std::pair<CTransaction, NTP1Transaction>> NTP1Transaction::StdFetchedInputTxsToNTP1(
    const CTransaction& tx, const MapPrevTx& mapInputs, const ITxDB& txdb, bool recoverProtection,
    const std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>& mapQueuedNTP1Inputs,
    const std::map<uint256, CTxIndex>& queuedAcceptedTxs, int recursionCount)
{
    if (recursionCount >= 32) {
        throw std::runtime_error("Reached maximum recursion in StdFetchedInputTxsToNTP1");
    }

    if (tx.IsCoinBase()) {
        return {};
    }

    const int currentBlockHeight = txdb.GetBestChainHeight();

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputsWithNTP1;
    // put the input transactions in a vector with their corresponding NTP1 transactions
    {
        inputsWithNTP1.clear();
        std::transform(tx.vin.begin(), tx.vin.end(), std::back_inserter(inputsWithNTP1),
                       [&mapInputs, &tx, &currentBlockHeight](const CTxIn& in) {
                           if (mapInputs.count(in.prevout.hash) == 0) {
                               throw std::runtime_error("Could not find input after having fetched it "
                                                        "(for NTP1 database storage); for tx: " +
                                                        tx.GetHash().ToString());
                           }
                           auto result = std::make_pair(mapInputs.find(in.prevout.hash)->second.second,
                                                        NTP1Transaction());
                           result.second.readNTP1DataFromTx_minimal(currentBlockHeight, result.first);
                           return result;
                       });
    }

    // NTP1 transaction data is either in the test pool OR in the database; no third option here
    auto it = mapQueuedNTP1Inputs.find(tx.GetHash());
    if (it == mapQueuedNTP1Inputs.end()) {
        for (auto&& inTx : inputsWithNTP1) {
            if (IsTxNTP1(&inTx.first)) {
                if (txdb.ContainsNTP1Tx(inTx.first.GetHash())) {
                    // if the transaction is in the database, get it
                    FetchNTP1TxFromDisk(inTx, txdb, recoverProtection);
                } else if (queuedAcceptedTxs.find(inTx.first.GetHash()) != queuedAcceptedTxs.end()) {
                    // otherwise, if the transaction is already in the test pool, use it to read it

                    std::vector<std::pair<CTransaction, NTP1Transaction>> inputsOfInput =
                        GetAllNTP1InputsOfTx(inTx.first, txdb, recoverProtection, mapQueuedNTP1Inputs,
                                             queuedAcceptedTxs, recursionCount + 1);

                    NTP1Transaction ntp1tx;
                    inTx.second.readNTP1DataFromTx(currentBlockHeight, inTx.first, inputsOfInput);
                } else {
                    // read NTP1 transaction inputs. If they fail, that's OK, because they will
                    // fail later if they're necessary
                    FetchNTP1TxFromDisk(inTx, txdb, recoverProtection);
                }
            }
        }
    } else {
        inputsWithNTP1 = it->second;
    }
    return inputsWithNTP1;
}

int NTP1Transaction::GetCurrentBlockHeight(const ITxDB& txdb) { return txdb.GetBestChainHeight(); }

bool NTP1Transaction::IsTxNTP1(const CTransaction* tx, std::string* opReturnArg)
{
    if (!tx) {
        return false;
    }

    if (Params().IsNTP1TxExcluded(tx->GetHash())) {
        return false;
    }

    boost::smatch opReturnArgMatch;

    for (unsigned long j = 0; j < tx->vout.size(); j++) {
        const CScript& scriptPubKey = tx->vout[j].scriptPubKey;
        if (scriptPubKey.size() > 2 && scriptPubKey.at(0) == OP_RETURN) {
            const std::vector<uint8_t> ntp1Script = CTransaction::ExtractOpRetData(scriptPubKey);
            if (IsScriptNTP1(ntp1Script)) {
                if (opReturnArg) {
                    *opReturnArg = ToHex(ntp1Script);
                }
                return true;
            }
        }
    }
    return false;
}

bool AreTokenSymbolsEquivalent(std::string lhs, std::string rhs)
{
    std::transform(lhs.begin(), lhs.end(), lhs.begin(), ::toupper);
    std::transform(rhs.begin(), rhs.end(), rhs.begin(), ::toupper);
    return lhs == rhs;
}

// Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock
bool GetTransaction(const uint256& hash, CTransaction& tx, uint256& hashBlock)
{
    {
        if (mempool.lookup(hash, tx)) {
            return true;
        }
        const CTxDB txdb;
        CTxIndex    txindex;
        if (tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex)) {
            CBlock block;
            if (block.ReadFromDisk(txindex.pos.nBlockPos, txdb, false))
                hashBlock = block.GetHash();
            return true;
        }
    }
    return false;
}

void FetchNTP1TxFromDisk(std::pair<CTransaction, NTP1Transaction>& txPair, const ITxDB& txdb,
                         bool /*recoverProtection*/, unsigned /*recurseDepth*/)
{
    if (!NTP1Transaction::IsTxNTP1(&txPair.first)) {
        return;
    }
    if (!txdb.ReadNTP1Tx(txPair.first.GetHash(), txPair.second)) {
        NLog.write(b_sev::err, "Failed to fetch NTP1 transaction {}", txPair.first.GetHash().ToString());
        return;
    }
}

template <typename ScriptType>
void NTP1Transaction::__TransferTokens(
    int blockHeight, const std::shared_ptr<ScriptType>& scriptPtrD, const CTransaction& tx,
    const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs, bool burnOutput31)
{
    static_assert(std::is_same<ScriptType, NTP1Script_Transfer>::value ||
                      std::is_same<ScriptType, NTP1Script_Burn>::value,
                  "Script types can only be Transfer and Burn in this function");

    int currentInputIndex = 0;

    EnsureInputsHashesMatch(inputsTxs);
    EnsureInputTokensRelateToTx(tx, inputsTxs);

    // calculate total tokens in inputs
    std::vector<std::vector<NTP1Int>>         totalTokensLeftInInputs(tx.vin.size());
    std::vector<std::vector<NTP1TokenTxData>> tokensKindsInInputs(tx.vin.size());
    for (unsigned i = 0; i < tx.vin.size(); i++) {
        const auto& n    = tx.vin[i].prevout.n;
        const auto& hash = tx.vin[i].prevout.hash;

        auto it = GetPrevInputIt(tx, hash, inputsTxs);

        const NTP1Transaction& ntp1tx = it->second;
        // this array keeps track of all tokens left
        totalTokensLeftInInputs[i].resize(ntp1tx.getTxOut(n).tokenCount());
        // this object relates tokens in the last list to their corresponding information
        tokensKindsInInputs[i].resize(ntp1tx.getTxOut(n).tokenCount());
        // loop over tokens of a single input (outputs from previous transactions)
        for (int j = 0; j < (int)ntp1tx.getTxOut(n).tokenCount(); j++) {
            const auto& tokenObj          = ntp1tx.getTxOut(n).getToken(j);
            totalTokensLeftInInputs[i][j] = tokenObj.getAmount();
            tokensKindsInInputs[i][j]     = tokenObj;
        }
    }

    std::vector<NTP1Script::TransferInstruction> TIs = scriptPtrD->getTransferInstructions();

    // the operation is invalid if input 0 has no tokens; this artificial failure is forced for this to
    // be compliant with the API
    bool invalid = false;
    if (totalTokensLeftInInputs.size() == 0) {
        invalid = true;
    } else {
        NTP1Int totalTokensInInput0 = std::accumulate(totalTokensLeftInInputs[0].begin(),
                                                      totalTokensLeftInInputs[0].end(), NTP1Int(0));

        invalid = !totalTokensInInput0;
    }

    for (int i = 0; i < (int)scriptPtrD->getTransferInstructionsCount(); i++) {
        // if the transaction is invalid, break here and go straight to change calculations
        if (invalid) {
            break;
        }

        if (currentInputIndex >= static_cast<int>(vin.size())) {
            throw std::runtime_error(
                "An input of transfer instruction is outside the available "
                "range of inputs in NTP1 OP_RETURN argument: " +
                scriptPtrD->getParsedScriptHex() + ", where the number of available inputs is " +
                ::ToString(tx.vin.size()) + " in transaction " + tx.GetHash().ToString());
        }

        const int outputIndex    = TIs[i].outputIndex;
        bool      burnThisOutput = (burnOutput31 && outputIndex == 31);

        if (outputIndex >= static_cast<int>(vout.size()) && !burnThisOutput) {
            throw std::runtime_error(
                "An output of transfer instruction is outside the available "
                "range of outputs in NTP1 OP_RETURN argument: " +
                scriptPtrD->getParsedScriptHex() + ", where the number of available outputs is " +
                ::ToString(tx.vout.size()) + " in transaction " + tx.GetHash().ToString());
        }

        // loop over the kinds of tokens in the input and distribute them over outputs
        // note: there's no way to switch from one token to the next unless its content depletes
        NTP1Int currentOutputAmount = TIs[i].amount;

        //  token index at which to start subtraction, helps in skipping empty tokens when
        // subtracting spent amount
        int startTokenIndex = 0;

        // if input is empty, just move to the next one since empty inputs don't break adjacency
        bool    stopInstructions = false;
        NTP1Int totalTokensInCurrentInput =
            std::accumulate(totalTokensLeftInInputs[currentInputIndex].begin(),
                            totalTokensLeftInInputs[currentInputIndex].end(), NTP1Int(0));
        while (totalTokensLeftInInputs[currentInputIndex].size() == 0 ||
               totalTokensInCurrentInput == 0) {
            currentInputIndex++;
            if (currentInputIndex >= static_cast<int>(vin.size())) {
                stopInstructions = true;
                break;
            }
            totalTokensInCurrentInput =
                std::accumulate(totalTokensLeftInInputs[currentInputIndex].begin(),
                                totalTokensLeftInInputs[currentInputIndex].end(), NTP1Int(0));
        }

        if (stopInstructions) {
            break;
        }

        for (int j = 0; j < (int)totalTokensLeftInInputs[currentInputIndex].size(); j++) {

            // calculate the total number of available tokens for spending
            NTP1Int     totalAdjacentTokensOfOneKind = 0;
            std::string currentTokenId;
            bool        inputDone = false;
            for (int k = currentInputIndex; k < (int)totalTokensLeftInInputs.size(); k++) {
                // an empty input in between doesn't break adjacency
                //                if (totalTokensLeftInInputs[k].size() == 0) {
                //                    break;
                //                }
                for (int l = (k == currentInputIndex ? j : 0);
                     l < (int)totalTokensLeftInInputs[k].size(); l++) {
                    // if the amount of tokens currently is zero, it's simply skipped and the token ID is
                    // changed to the next adjacent kind
                    if (tokensKindsInInputs[k][l].getTokenId() != currentTokenId &&
                        totalAdjacentTokensOfOneKind > 0) {
                        inputDone = true;
                        break;
                    } else {
                        if (totalAdjacentTokensOfOneKind == 0) {
                            startTokenIndex = l;
                            currentTokenId  = tokensKindsInInputs[currentInputIndex][l].getTokenId();
                        }
                        totalAdjacentTokensOfOneKind += totalTokensLeftInInputs[k][l];
                    }
                }
                if (inputDone) {
                    break;
                }
            }

            // check if the token is blacklisted
            int blacklistHeight = 0;
            if (Params().IsNTP1TokenBlacklisted(currentTokenId, blacklistHeight)) {
                const int currentHeight = blockHeight;
                if (currentHeight >= blacklistHeight) {
                    throw std::runtime_error("The NTP1 token " + currentTokenId +
                                             " is blacklisted and cannot be transferred or burned.");
                }
            }

            // ensure that gaps won't create a problem; an empty input is automatically skipped here
            if (totalAdjacentTokensOfOneKind == 0) {
                if (j + 1 >= (int)totalTokensLeftInInputs[currentInputIndex].size()) {
                    currentInputIndex++;
                    j = -1; // resets j to 0 in the next iteration
                }
                continue;
            }

            if (currentOutputAmount > totalAdjacentTokensOfOneKind) {
                throw std::runtime_error(
                    "Insufficient tokens for transaction from inputs for transaction: " +
                    tx.GetHash().ToString() + " from input: " + ::ToString(currentInputIndex) +
                    ". Required output amount: " + ::ToString(currentOutputAmount) +
                    "; and the total available amount in all (possibly adjacent) inputs: " +
                    ::ToString(totalAdjacentTokensOfOneKind) +
                    "; in transfer instruction number: " + ::ToString(i) + ". Inputs in order are: " +
                    std::accumulate(tx.vin.begin(), tx.vin.end(), std::string(),
                                    [](const std::string& currRes, const CTxIn& inp) {
                                        return currRes + " - " + inp.prevout.hash.ToString() + ":" +
                                               ::ToString(inp.prevout.n);
                                    }) +
                    "; and OP_RETURN script: " + scriptPtrD->getParsedScriptHex());
            }

            const auto&   currentTokenObj = tokensKindsInInputs[currentInputIndex][startTokenIndex];
            const NTP1Int amountToCredit  = std::min(totalAdjacentTokensOfOneKind, currentOutputAmount);

            if (!burnThisOutput) {
                // create the token object that will be added to the output
                NTP1TokenTxData ntp1tokenTxData;
                ntp1tokenTxData.setAmount(amountToCredit);
                ntp1tokenTxData.setTokenId(currentTokenObj.getTokenId());
                ntp1tokenTxData.setAggregationPolicy(currentTokenObj.getAggregationPolicy());
                ntp1tokenTxData.setDivisibility(currentTokenObj.getDivisibility());
                ntp1tokenTxData.setTokenSymbol(currentTokenObj.getTokenSymbol());
                ntp1tokenTxData.setLockStatus(currentTokenObj.getLockStatus());
                ntp1tokenTxData.setIssueTxIdHex(currentTokenObj.getIssueTxId().ToString());

                // add the token to the output
                vout[outputIndex].tokens.push_back(ntp1tokenTxData);
            }

            // reduce the available output amount
            if (amountToCredit > currentOutputAmount) {
                currentOutputAmount = 0;
            } else {
                currentOutputAmount -= amountToCredit;
            }

            // reduce the available balance from the array that tracks all available inputs
            NTP1Int amountLeftToSubtract = amountToCredit;
            for (int k = currentInputIndex; k < (int)totalTokensLeftInInputs.size(); k++) {
                // an empty input in between means inputs are not adjacent
                for (int l = (k == currentInputIndex ? startTokenIndex : 0);
                     l < (int)totalTokensLeftInInputs[k].size(); l++) {
                    if (tokensKindsInInputs[k][l].getTokenId() == currentTokenId) {
                        if (totalTokensLeftInInputs[k][l] >= amountLeftToSubtract) {
                            totalTokensLeftInInputs[k][l] -= amountLeftToSubtract;
                            amountLeftToSubtract = 0;
                        } else {
                            amountLeftToSubtract -= totalTokensLeftInInputs[k][l];
                            totalTokensLeftInInputs[k][l] = 0;
                        }
                    } else {
                        if (amountLeftToSubtract == 0) {
                            break;
                        } else {
                            throw std::runtime_error(
                                "Unable to decredit the available balances from inputs");
                        }
                    }
                    if (amountLeftToSubtract == 0) {
                        break;
                    }
                }
                if (amountLeftToSubtract == 0) {
                    break;
                }
            }
            if (amountLeftToSubtract > 0) {
                throw std::runtime_error("Unable to decredit the available balances from inputs");
            }

            // if skip, move on to the next input
            if (TIs[i].skipInput) {
                currentInputIndex++;
            }

            NTP1Int totalTokensLeftInCurrentInput =
                std::accumulate(totalTokensLeftInInputs[currentInputIndex].begin(),
                                totalTokensLeftInInputs[currentInputIndex].end(), NTP1Int(0));
            if (totalTokensLeftInCurrentInput == 0) {
                // avoid incrementing twice
                if (!TIs[i].skipInput) {
                    currentInputIndex++;
                }
            }

            // all required output amount is spent. The rest will be redirected as change
            if (currentOutputAmount == 0) {
                break;
            }
        }
    }

    for (int i = 0; i < (int)totalTokensLeftInInputs.size(); i++) {
        for (int j = 0; j < (int)totalTokensLeftInInputs[i].size(); j++) {
            if (totalTokensLeftInInputs[i][j] == 0) {
                continue;
            }

            const auto& currentTokenObj = tokensKindsInInputs[i][j];
            NTP1Int     amountToCredit  = totalTokensLeftInInputs[i][j];

            // create the token object that will be added to the output
            NTP1TokenTxData ntp1tokenTxData;
            ntp1tokenTxData.setAmount(amountToCredit);
            ntp1tokenTxData.setTokenId(currentTokenObj.getTokenId());
            ntp1tokenTxData.setAggregationPolicy(currentTokenObj.getAggregationPolicy());
            ntp1tokenTxData.setDivisibility(currentTokenObj.getDivisibility());
            ntp1tokenTxData.setTokenSymbol(currentTokenObj.getTokenSymbol());
            ntp1tokenTxData.setLockStatus(currentTokenObj.getLockStatus());
            ntp1tokenTxData.setIssueTxIdHex(currentTokenObj.getIssueTxId().ToString());

            if (vout.size() == 0) {
                throw std::runtime_error("No outputs in transaction: " + tx.GetHash().ToString());
            }

            // reduce the available balance
            totalTokensLeftInInputs[i][j] -= amountToCredit;
            amountToCredit = 0;

            if (ntp1tokenTxData.getAggregationPolicy() ==
                NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str) {
                // aggregate coins from next inputs
                bool stopLooping = false;
                for (int k = i; k < (int)totalTokensLeftInInputs.size(); k++) {
                    for (int l = (k == i ? j : 0); l < (int)totalTokensLeftInInputs[k].size(); l++) {
                        if (k == i && l == j) {
                            continue; // ignore the balance that was already credited
                        }
                        if (tokensKindsInInputs[i][j].getTokenId() !=
                            tokensKindsInInputs[k][l].getTokenId()) {
                            stopLooping = true;
                            break;
                        }
                        amountToCredit                = totalTokensLeftInInputs[k][l];
                        totalTokensLeftInInputs[k][l] = 0;
                        ntp1tokenTxData.setAmount(ntp1tokenTxData.getAmount() + amountToCredit);
                    }

                    // stop the outer loop
                    if (stopLooping) {
                        break;
                    }
                }
            }

            // add the token to the last output
            vout.back().tokens.push_back(ntp1tokenTxData);
        }
    }

    // delete empty token slots
    for (int i = 0; i < (int)vout.size(); i++) {
        for (int j = 0; j < (int)vout[i].tokenCount(); j++) {
            if (vout[i].getToken(j).getAmount() == 0) {
                vout[i].tokens.erase(vout[i].tokens.begin() + j);
                j--;
            }
        }
    }
}
