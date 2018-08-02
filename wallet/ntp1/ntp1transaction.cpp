#include "ntp1transaction.h"

#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1tools.h"
#include "ntp1txin.h"
#include "ntp1txout.h"
#include "util.h"

#include <boost/algorithm/hex.hpp>

unsigned int DiskNTP1TxPos::nCurrentNTP1TxsFile = 1;

NTP1Transaction::NTP1Transaction() { setNull(); }

void NTP1Transaction::setNull()
{
    nVersion = NTP1Transaction::CURRENT_VERSION;
    nTime    = GetAdjustedTime() * 1000;
    vin.clear();
    vout.clear();
    nLockTime = 0;
}

bool NTP1Transaction::isNull() const { return (vin.empty() && vout.empty()); }

void NTP1Transaction::importJsonData(const std::string& data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);

        setHex(NTP1Tools::GetStrField(parsedData.get_obj(), "hex"));
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
        if (ntp1InTx1.getTxOut(tx.vin[i].prevout.n).getNumOfTokens() == 0) {
            for (int j = i + 1; j < (int)tx.vin.size(); j++) {
                auto it2 = GetPrevInputIt(tx, tx.vin[j].prevout.hash, inputsTxs);

                const NTP1Transaction& ntp1InTx2 = it2->second;

                if (ntp1InTx2.getTxOut(tx.vin[j].prevout.n).getNumOfTokens() != 0) {
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

        if (in.prevout.n + 1 >= ntp1InTx.getTxOutCount()) {
            throw std::runtime_error("Failed at retrieving the number of tokens from transaction " +
                                     tx.GetHash().ToString() + " at input " +
                                     neblInTx.GetHash().ToString() +
                                     "; input: " + ToString(in.prevout.n) + " is out of range.");
        }

        result += ntp1InTx.getTxOut(in.prevout.n).getNumOfTokens();
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

void NTP1Transaction::AmendStdTxWithNTP1(CTransaction& tx)
{
    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs = GetAllNTP1InputsOfTx(tx);

    AmendStdTxWithNTP1(tx, inputs);
}

void NTP1Transaction::AmendStdTxWithNTP1(
    CTransaction& tx, const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputs)
{
    // temp copy to avoid changing the original if the operation fails
    CTransaction tx_ = tx;

    EnsureInputsHashesMatch(inputs);

    EnsureInputTokensRelateToTx(tx_, inputs);

    unsigned inputTokenKinds = CountTokenKindsInInputs(tx_, inputs);

    std::string opReturnArg;
    bool        txIsNTP1 = IsTxNTP1(&tx_, &opReturnArg);

    // if no inputs contain NTP1 AND no OP_RETURN argument exists, then this is a pure NEBL transaction
    // with no NTP1
    if (!txIsNTP1 && inputTokenKinds == 0) {
        return;
    }

    if (txIsNTP1) {
        throw std::runtime_error("Cannot NTP1-amend transaction " + tx_.GetHash().ToString() +
                                 " because it already has an OP_RETURN");
    }

    ReorderTokenInputsToGoFirst(tx_, inputs);

    if (!txIsNTP1 && inputTokenKinds > 0) {
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

            for (int j = 0; j < (int)inputTxNTP1.vout.at(inIndex).getNumOfTokens(); j++) {
                if (inputTxNTP1.vout.at(inIndex).getToken(j).getAmount() == 0) {
                    if (j == 0) {
                        throw std::runtime_error("While amending a native neblio transactions, the "
                                                 "first input is empty. This basically cannot be "
                                                 "amended. Inputs must be reordered to have tokens in "
                                                 "first inputs");
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

                CScript outputScript;
                outputScript.SetDestination(currentTokenAddress);
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

        std::shared_ptr<NTP1Script_Transfer> scriptPtrT = NTP1Script_Transfer::CreateScript(TIs, "");

        std::string script    = scriptPtrT->calculateScriptBin();
        std::string scriptHex = boost::algorithm::hex(script);

        tx_.vout[opRetIdx].scriptPubKey = CScript() << OP_RETURN << ParseHex(scriptHex);
        tx_.vout[opRetIdx].nValue       = MIN_TX_FEE;
    }
    tx = tx_;
}

void NTP1Transaction::__manualSet(int NVersion, uint256 TxHash, std::vector<unsigned char> TxSerialized,
                                  std::vector<NTP1TxIn> Vin, std::vector<NTP1TxOut> Vout,
                                  uint64_t NLockTime, uint64_t NTime,
                                  NTP1TransactionType Ntp1TransactionType)
{
    nVersion            = NVersion;
    txHash              = TxHash;
    txSerialized        = TxSerialized;
    vin                 = Vin;
    vout                = Vout;
    nLockTime           = NLockTime;
    nTime               = NTime;
    ntp1TransactionType = Ntp1TransactionType;
}

void NTP1Transaction::readNTP1DataFromTx_minimal(const CTransaction& tx)
{
    txHash = tx.GetHash();
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
    }
    ntp1TransactionType = NTP1TxType_NOT_NTP1;
}

void NTP1Transaction::readNTP1DataFromTx(
    const CTransaction& tx, const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{
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

    // resize NTP1 input size to match normal outputs size and clear tokens for recalculation
    for (auto&& in : vin) {
        in.tokens.clear();
    }
    vin.resize(tx.vin.size());

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

    // resize NTP1 output size to match normal outputs size and clear tokens for recalculation
    for (auto&& out : vout) {
        out.tokens.clear();
    }
    vout.resize(tx.vout.size());

    txHash = tx.GetHash();

    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScript(opReturnArg);
    if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Issuance) {
        ntp1TransactionType = NTP1TxType_ISSUANCE;

        std::shared_ptr<NTP1Script_Issuance> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Issuance>(scriptPtr);

        if (static_cast<int64_t>(totalInput) - static_cast<int64_t>(totalOutput) <
            static_cast<int64_t>(IssuanceFee)) {
            throw std::runtime_error("Issuance fee is less than 10 nebls");
        }

        uint64_t totalAmountLeft = scriptPtrD->getAmount();
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
            uint64_t currentAmount = instruction.amount;

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

            for (int i = 0; i < (int)input.second.vout[currIndex].getNumOfTokens(); i++) {
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
            throw std::runtime_error("Casting script point to transfer type failed: " + opReturnArg);
        }

        __TransferTokens<NTP1Script_Transfer>(scriptPtrD, tx, inputsTxs, false);

    } else if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Burn) {
        ntp1TransactionType = NTP1TxType_BURN;
        std::shared_ptr<NTP1Script_Burn> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Burn>(scriptPtr);

        if (!scriptPtrD) {
            throw std::runtime_error("Casting script point to burn type failed: " + opReturnArg);
        }

        __TransferTokens<NTP1Script_Burn>(scriptPtrD, tx, inputsTxs, true);

    } else {
        ntp1TransactionType = NTP1TxType_UNKNOWN;
        throw std::runtime_error("Unknown NTP1 transaction type");
    }
}

bool NTP1Transaction::writeToDisk(unsigned int& nFileRet, unsigned int& nTxPosRet,
                                  FILE* customFile) const
{
    // Open history file to append
    CAutoFile fileout =
        CAutoFile((customFile == nullptr ? DiskNTP1TxPos::AppendNTP1TxsFile(nFileRet) : customFile),
                  SER_DISK, CLIENT_VERSION);
    if (!fileout)
        return error("NTP1Transaction::WriteToDisk() : AppendNTP1TxsFile failed");

    // Write tx
    long fileOutPos = ftell(fileout);
    if (fileOutPos < 0)
        return error("NTP1Transaction::WriteToDisk() : ftell failed");
    nTxPosRet = fileOutPos;
    fileout << *this;

    // Flush stdio buffers and commit to disk before returning
    fflush(fileout);
    FileCommit(fileout);

    return true;
}

bool NTP1Transaction::readFromDisk(DiskNTP1TxPos pos, FILE** pfileRet, FILE* customFile)
{
    CAutoFile filein = CAutoFile(
        (customFile == nullptr ? DiskNTP1TxPos::OpenNTP1TxsFile(pos.nFile, 0, pfileRet ? "rb+" : "rb")
                               : customFile),
        SER_DISK, CLIENT_VERSION);
    if (!filein)
        return error("NTP1Transaction::ReadFromDisk() : OpenNTP1TxsFile failed");

    // Read transaction
    if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
        return error("NTP1Transaction::ReadFromDisk() : fseek failed");

    try {
        filein >> *this;
    } catch (std::exception& e) {
        return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
    }

    // Return file pointer
    if (pfileRet) {
        if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
            return error("NTP1Transaction::ReadFromDisk() : second fseek failed");
        *pfileRet = filein.release();
    }
    return true;
}

FILE* DiskNTP1TxPos::OpenNTP1TxsFile(unsigned int nFile, unsigned int nTxPos, const char* pszMode)
{
    if ((nFile < 1) || (nFile == (unsigned int)-1))
        return NULL;
    FILE* file = fopen(DiskNTP1TxPos::NTP1TxsFilePath(nFile).string().c_str(), pszMode);
    if (!file)
        return NULL;
    if (nTxPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w')) {
        if (fseek(file, nTxPos, SEEK_SET) != 0) {
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* DiskNTP1TxPos::AppendNTP1TxsFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    while (true) {
        FILE* file = DiskNTP1TxPos::OpenNTP1TxsFile(DiskNTP1TxPos::nCurrentNTP1TxsFile, 0, "ab");
        if (!file)
            return NULL;
        if (fseek(file, 0, SEEK_END) != 0)
            return NULL;
        // FAT32 file size max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < (long)(0x7F000000 - MAX_SIZE)) {
            nFileRet = DiskNTP1TxPos::nCurrentNTP1TxsFile;
            return file;
        }
        fclose(file);
        DiskNTP1TxPos::nCurrentNTP1TxsFile++;
    }
}

boost::filesystem::path DiskNTP1TxPos::NTP1TxsFilePath(unsigned int nFile)
{
    string strNTP1TxsFn = strprintf("ntp1txs%04u.dat", nFile);
    return GetDataDir() / strNTP1TxsFn;
}
