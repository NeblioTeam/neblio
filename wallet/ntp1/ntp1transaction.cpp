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

void NTP1Transaction::readNTP1DataFromTx(
    const CTransaction& tx, const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs)
{
    std::string opReturnArg;
    if (!IsTxNTP1(&tx, &opReturnArg)) {
        ntp1TransactionType = NTP1TxType_NOT_NTP1;
        return;
    }

    // resize NTP1 input size to match normal outputs size and clear tokens for recalculation
    for (auto&& in : vin) {
        in.tokens.clear();
    }
    vin.resize(tx.vin.size());

    // TODO: parts of this may not be needed at all
    // null elements are supposed to be non-NTP1 transactions; invalid inputs throw exceptions
    std::vector<std::shared_ptr<NTP1Script>> inputNTP1Scripts(vin.size());
    for (unsigned i = 0; i < tx.vin.size(); i++) {
        const auto& currInputHash  = tx.vin[i].prevout.hash;
        const auto& currInputIndex = tx.vin[i].prevout.n;

        vin[i].setPrevout(NTP1OutPoint(currInputHash, currInputIndex));

        // find inputs in the list of inputs and parse their OP_RETURN
        auto it = std::find_if(inputsTxs.cbegin(), inputsTxs.cend(),
                               [this, i](const std::pair<CTransaction, NTP1Transaction>& in) {
                                   return in.first.GetHash() == vin[i].getPrevout().getHash();
                               });
        if (it == inputsTxs.end()) {
            throw std::runtime_error("Could not find all relevant inputs in the inputs list");
        }
        std::string opReturnArgInput;

        const CTransaction&    currStdInput  = it->first;
        const NTP1Transaction& currNTP1Input = it->second;
        // if the transaction is not NTP1, continue
        if (!IsTxNTP1(&currStdInput, &opReturnArgInput)) {
            continue;
        }
        inputNTP1Scripts[i] = NTP1Script::ParseScript(opReturnArgInput);
        vin[i].tokens       = currNTP1Input.vout.at(currInputIndex).tokens;
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
        uint64_t totalAmountLeft = scriptPtrD->getAmount();
        if (tx.vin.size() < 1) {
            throw std::runtime_error("Number of inputs is zero for transaction: " +
                                     tx.GetHash().ToString());
        }
        for (long i = 0; i < scriptPtrD->getTransferInstructionsCount(); i++) {
            // TODO: verify fees; do we have to check the fees here?
            NTP1TokenTxData ntp1tokenTxData;
            const auto&     instruction = scriptPtrD->getTransferInstruction(i);
            if (instruction.outputIndex >= static_cast<int>(tx.vout.size())) {
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
        ntp1TransactionType = NTP1TxType_INVALID;
        throw std::runtime_error("Unknown NTP1 transaction type");
    }
}

bool NTP1Transaction::writeToDisk(unsigned int& nFileRet, unsigned int& nTxPosRet, FILE* customFile)
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
