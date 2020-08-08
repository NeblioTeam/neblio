// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>

#include "base58.h"
#include "bitcoinrpc.h"
#include "boost/make_shared.hpp"
#include "climits"
#include "init.h"
#include "main.h"
#include "net.h"
#include "ntp1/ntp1transaction.h"
#include "txdb.h"
#include "wallet.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

template <typename T>
json_spirit::Value GetNTP1TxMetadata(const CTransaction& tx) noexcept
{
    static_assert(std::is_same<T, NTP1Script_Issuance>::value ||
                      std::is_same<T, NTP1Script_Transfer>::value ||
                      std::is_same<T, NTP1Script_Burn>::value,
                  "Unexpected type. Type should be one of the ones in the assert statement.");

    std::string opRet;
    bool        isNTP1 = NTP1Transaction::IsTxNTP1(&tx, &opRet);
    if (isNTP1) {
        std::shared_ptr<NTP1Script> s  = NTP1Script::ParseScript(opRet);
        std::shared_ptr<T>          sd = std::dynamic_pointer_cast<T>(s);
        if (sd) {
            return NTP1Script::GetMetadataAsJson(sd.get(), tx);
        } else {
            return json_spirit::Value();
        }
    }
    return json_spirit::Value();
}

void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry, bool ignoreNTP1 = false)
{
    std::pair<CTransaction, NTP1Transaction> pair;
    std::string                              opRet;
    bool                                     isNTP1 = NTP1Transaction::IsTxNTP1(&tx, &opRet);

    if (isNTP1 && !ignoreNTP1) {
        CTxDB txdb("r");
        pair = std::make_pair(CTransaction::FetchTxFromDisk(tx.GetHash()), NTP1Transaction());
        FetchNTP1TxFromDisk(pair, txdb, false);
        if (pair.second.isNull()) {
            isNTP1 = false;
        }
    }
    entry.push_back(Pair("size", (int)::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)));
    entry.push_back(Pair("txid", tx.GetHash().GetHex()));
    entry.push_back(Pair("version", tx.nVersion));
    entry.push_back(Pair("time", (int64_t)tx.nTime));
    entry.push_back(Pair("locktime", (int64_t)tx.nLockTime));
    entry.push_back(Pair("ntp1", isNTP1));
    Array vin;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxIn& txin = tx.vin[i];
        Object       in;
        Array        tokens;
        if (tx.IsCoinBase())
            in.push_back(Pair("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end())));
        else {
            in.push_back(Pair("txid", txin.prevout.hash.GetHex()));
            in.push_back(Pair("vout", (int64_t)txin.prevout.n));
            Object o;
            o.push_back(Pair("asm", txin.scriptSig.ToString()));
            o.push_back(Pair("hex", HexStr(txin.scriptSig.begin(), txin.scriptSig.end())));
            in.push_back(Pair("scriptSig", o));
            if (isNTP1 && !ignoreNTP1) {
                for (unsigned int t = 0; t < pair.second.getTxIn(i).getNumOfTokens(); t++) {
                    json_spirit::Value n = pair.second.getTxIn(i).getToken(t).exportDatabaseJsonData();
                    uint256            issuanceTxid = pair.second.getTxIn(i).getToken(t).getIssueTxId();
                    json_spirit::Value issuanceJson =
                        NTP1Transaction::GetNTP1IssuanceMetadata(issuanceTxid);
                    n.get_obj().push_back(json_spirit::Pair("metadataOfIssuance", issuanceJson));
                    tokens.push_back(n);
                }
            }
        }
        in.push_back(Pair("sequence", (int64_t)txin.nSequence));
        if (isNTP1 && !ignoreNTP1) {
            in.push_back(Pair("tokens", tokens));
        }
        vin.push_back(in);
    }
    entry.push_back(Pair("vin", vin));
    Array vout;
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& txout = tx.vout[i];
        Object        out;
        Array         tokens;
        out.push_back(Pair("value", ValueFromAmount(txout.nValue)));
        out.push_back(Pair("n", (int64_t)i));
        Object o;
        ScriptPubKeyToJSON(txout.scriptPubKey, o, false);
        out.push_back(Pair("scriptPubKey", o));
        if (isNTP1 && !ignoreNTP1) {
            for (unsigned int t = 0; t < pair.second.getTxOut(i).tokenCount(); t++) {
                json_spirit::Value n = pair.second.getTxOut(i).getToken(t).exportDatabaseJsonData();
                uint256            issuanceTxid = pair.second.getTxOut(i).getToken(t).getIssueTxId();
                json_spirit::Value issuanceJson = NTP1Transaction::GetNTP1IssuanceMetadata(issuanceTxid);
                n.get_obj().push_back(json_spirit::Pair("metadataOfIssuance", issuanceJson));
                tokens.push_back(n);
            }
            out.push_back(Pair("tokens", tokens));
        }
        vout.push_back(out);
    }
    entry.push_back(Pair("vout", vout));

    {
        if (isNTP1 && !ignoreNTP1) {
            std::shared_ptr<NTP1Script> s = NTP1Script::ParseScript(opRet);
            if (s && s->getProtocolVersion() >= 3) {
                if (s->getTxType() == NTP1Script::TxType_Issuance) {
                    entry.push_back(json_spirit::Pair("metadataOfUtxos",
                                                      GetNTP1TxMetadata<NTP1Script_Issuance>(tx)));
                } else if (s->getTxType() == NTP1Script::TxType_Transfer) {
                    entry.push_back(json_spirit::Pair("metadataOfUtxos",
                                                      GetNTP1TxMetadata<NTP1Script_Transfer>(tx)));
                } else if (s->getTxType() == NTP1Script::TxType_Burn) {
                    entry.push_back(
                        json_spirit::Pair("metadataOfUtxos", GetNTP1TxMetadata<NTP1Script_Burn>(tx)));
                }
            }
        }
    }

    if (hashBlock != 0) {
        entry.push_back(Pair("blockhash", hashBlock.GetHex()));
        BlockIndexMapType::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
            if (pindex->IsInMainChain()) {
                entry.push_back(Pair("confirmations", 1 + nBestHeight - pindex->nHeight));
                entry.push_back(Pair("time", (int64_t)pindex->nTime));
                entry.push_back(Pair("blocktime", (int64_t)pindex->nTime));
            } else
                entry.push_back(Pair("confirmations", 0));
        }
    }
}

Value getrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error("getrawtransaction <txid> [verbose=0] [ignoreNTP1=false] [blockhash=null]\n"
                            "If verbose=0, returns a string that is\n"
                            "serialized, hex-encoded data for <txid>.\n"
                            "If verbose is non-zero, returns an Object\n"
                            "with information about <txid>. "
                            "If blockhash is provided, the transaction will be sought only in that block"
                            "Not ignoring NTP1 will try to retireve NTP1 "
                            "data from the database. This won't work if the transaction is not in the "
                            "blockchain.");

    LOCK(cs_main);

    uint256      hash            = ParseHashV(params[0], "parameter 1");
    bool         in_active_chain = true;
    CBlockIndex* blockindex      = nullptr;

    if (hash == Params().GenesisBlock().hashMerkleRoot) {
        // Special exception for the genesis block coinbase transaction
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "The genesis block coinbase is not considered an "
                                                       "ordinary transaction and cannot be retrieved");
    }

    bool fVerbose = false;
    // TODO: verbose should be changed to boolean
    if (params.size() > 1) {
        fVerbose = (params[1].get_int() != 0);
    }

    CTransaction tx;
    uint256      hashBlock = 0;

    if (params.size() > 3) {
        // if a specific block was mentioned, get it from the database and look for the tx in it
        {
            uint256                     blockhash = ParseHashV(params[3], "parameter 3");
            BlockIndexMapType::iterator it        = mapBlockIndex.find(blockhash);
            if (it == mapBlockIndex.end()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block hash not found");
            }
            blockindex      = it->second.get();
            in_active_chain = blockindex->IsInMainChain();
        }

        hashBlock = ParseHashV(params[3], "blockhash");
        CTxDB  txdb;
        CBlock block;
        if (!txdb.ReadBlock(hashBlock, block, true)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block hash not found");
        }
        auto txIt = std::find_if(block.vtx.cbegin(), block.vtx.cend(),
                                 [&hash](const CTransaction& t) { return t.GetHash() == hash; });
        if (txIt == block.vtx.cend()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "No such transaction found in the provided block. Use gettransaction for "
                               "wallet transactions.");
        }
        tx = *txIt;
    } else {
        // if no specific block was mentioned, then get the tx from the mempool, or the main chain
        if (!GetTransaction(hash, tx, hashBlock))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
    }

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;
    string strHex = HexStr(ssTx.begin(), ssTx.end());

    if (!fVerbose)
        return strHex;

    bool fIgnoreNTP1 = false;
    if (params.size() > 2)
        fIgnoreNTP1 = params[2].get_bool();

    Object result;
    result.push_back(Pair("hex", strHex));
    if (blockindex) {
        result.push_back(Pair("in_active_chain", in_active_chain));
    }
    TxToJSON(tx, hashBlock, result, fIgnoreNTP1);
    return result;
}

Value listunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error("listunspent [minconf=1] [maxconf=9999999]  [\"address\",...]\n"
                            "Returns array of unspent transaction outputs\n"
                            "with between minconf and maxconf (inclusive) confirmations.\n"
                            "Optionally filtered to only include txouts paid to specified addresses.\n"
                            "Results are an array of Objects, each of which has:\n"
                            "{txid, vout, scriptPubKey, amount, confirmations}");

    RPCTypeCheck(params, list_of(int_type)(int_type)(array_type));

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    set<CBitcoinAddress> setAddress;
    if (params.size() > 2) {
        Array inputs = params[2].get_array();
        for (Value& input : inputs) {
            CBitcoinAddress address(input.get_str());
            if (!address.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                   string("Invalid neblio address: ") + input.get_str());
            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                                   string("Invalid parameter, duplicated address: ") + input.get_str());
            setAddress.insert(address);
        }
    }

    Array           results;
    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, false);
    for (const COutput& out : vecOutputs) {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
            continue;

        std::vector<std::pair<CTransaction, NTP1Transaction>> ntp1inputs =
            NTP1Transaction::GetAllNTP1InputsOfTx(static_cast<CTransaction>(*out.tx), false);
        NTP1Transaction ntp1tx;
        ntp1tx.readNTP1DataFromTx(static_cast<CTransaction>(*out.tx), ntp1inputs);

        if (setAddress.size()) {
            CTxDestination address;
            if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
                continue;

            if (!setAddress.count(address))
                continue;
        }

        CAmount        nValue = out.tx->vout[out.i].nValue;
        const CScript& pk     = out.tx->vout[out.i].scriptPubKey;
        Object         entry;
        entry.push_back(Pair("txid", out.tx->GetHash().GetHex()));
        entry.push_back(Pair("vout", out.i));
        CTxDestination address;
        if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
            entry.push_back(Pair("address", CBitcoinAddress(address).ToString()));
            if (pwalletMain->mapAddressBook.count(address))
                entry.push_back(Pair("account", pwalletMain->mapAddressBook[address]));
        }
        entry.push_back(Pair("scriptPubKey", HexStr(pk.begin(), pk.end())));
        entry.push_back(Pair("amount", ValueFromAmount(nValue)));
        entry.push_back(Pair("confirmations", out.nDepth));
        json_spirit::Array tokensRoot;
        for (int i = 0; i < (int)ntp1tx.getTxOut(out.i).tokenCount(); i++) {
            tokensRoot.push_back(ntp1tx.getTxOut(out.i).getToken(i).exportDatabaseJsonData());
        }
        entry.push_back(Pair("tokens", Value(tokensRoot)));
        results.push_back(entry);
    }

    return results;
}

Value createrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "createrawtransaction [{\"txid\":txid,\"vout\":n},...] {address:amount,...}\n"
            "Create a transaction spending given inputs\n"
            "(array of objects containing transaction id and output number),\n"
            "sending to given address(es).\n"
            "Returns hex-encoded raw transaction.\n"
            "Note that the transaction's inputs are not signed, and\n"
            "it is not stored in the wallet or transmitted to the network.");

    RPCTypeCheck(params, list_of(array_type)(obj_type));

    Array  inputs = params[0].get_array();
    Object sendTo = params[1].get_obj();

    CTransaction rawTx;

    for (Value& input : inputs) {
        const Object& o = input.get_obj();

        const Value& txid_v = find_value(o, "txid");
        if (txid_v.type() != str_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing txid key");
        string txid = txid_v.get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        const Value& vout_v = find_value(o, "vout");
        if (vout_v.type() != int_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        CTxIn in(COutPoint(uint256(txid), nOutput));
        rawTx.vin.push_back(in);
    }

    set<CBitcoinAddress> setAddress;
    for (const Pair& s : sendTo) {
        CBitcoinAddress address(s.name_);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid neblio address: ") + s.name_);

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               string("Invalid parameter, duplicated address: ") + s.name_);
        setAddress.insert(address);

        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());
        CAmount nAmount = AmountFromValue(s.value_);

        CTxOut out(nAmount, scriptPubKey);
        rawTx.vout.push_back(out);
    }

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;
    return HexStr(ss.begin(), ss.end());
}

Value createrawntp1transaction(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 2 && params.size() != 3 && params.size() != 4))
        throw runtime_error(
            "createrawntp1transaction [{\"txid\":txid,\"vout\":n},...] "
            "{address:{tokenid/tokenName:tokenAmount},{address:neblAmount,...}} [NTP1 Metadata=\"\"] "
            "[Encrypt-metadata=false]\n"
            "Create a transaction spending given inputs\n"
            "(array of objects containing transaction id and output number),\n"
            "sending to given address(es).\n"
            "Returns hex-encoded raw transaction.\n"
            "Note that the transaction is not stored in the wallet or transmitted to the network.");

    RPCTypeCheck(params, list_of(array_type)(obj_type));

    Array                     inputs = params[0].get_array();
    Object                    sendTo = params[1].get_obj();
    std::string               processedMetadata;
    RawNTP1MetadataBeforeSend rawNTP1Data("", false);
    if (params.size() > 2) {
        rawNTP1Data.metadata = params[2].get_str();
    }
    if (params.size() > 3) {
        rawNTP1Data.encrypt = params[3].get_bool();
    }

    CTransaction rawTx;

    boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();
    ntp1wallet->setRetrieveFullMetadata(false);
    ntp1wallet->update();

    // create the list of recipients that's compatible with NTP1 token selector
    std::vector<NTP1SendTokensOneRecipientData> ntp1recipients =
        GetNTP1RecipientsVector(sendTo, ntp1wallet);

    // create inputs' vector
    std::vector<COutPoint> cinputs;
    for (Value& input : inputs) {
        const Object& o = input.get_obj();

        const Value& txid_v = find_value(o, "txid");
        if (txid_v.type() != str_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing txid key");
        string txid = txid_v.get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        const Value& vout_v = find_value(o, "vout");
        if (vout_v.type() != int_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        cinputs.push_back(COutPoint(uint256(txid), nOutput));
    }

    NTP1SendTxData tokenSelector;
    tokenSelector.selectNTP1Tokens(ntp1wallet, cinputs, ntp1recipients, false);

    const std::map<std::string, NTP1Int>& changeMap    = tokenSelector.getChangeTokens();
    NTP1Int                               changeTokens = std::accumulate(
        changeMap.begin(), changeMap.end(), NTP1Int(0),
        [](NTP1Int n, const std::pair<std::string, NTP1Int>& p1) { return n + p1.second; });

    if (changeTokens > 0) {
        std::string except_msg;
        try {
            // safety
            if (changeMap.size() > 0) {
                std::string tokenId      = changeMap.begin()->first;
                NTP1Int     changeAmount = changeMap.begin()->second;

                std::string tokenName = ntp1wallet->getTokenMetadataMap().at(tokenId).getTokenName();

                except_msg =
                    "The transaction has NTP1 tokens change. Please spend all NTP1 tokens in the "
                    "transaction. Token with name " +
                    tokenName + " and ID " + tokenId +
                    " has the following amount unspent: " + ::ToString(changeAmount);
            }
        } catch (std::exception&) {
            throw std::runtime_error("The transaction has NTP1 tokens change. Please spend all NTP1 "
                                     "tokens in the transaction");
        }

        throw std::runtime_error(except_msg);
    }

    std::vector<NTP1OutPoint> usedInputs = tokenSelector.getUsedInputs();

    for (const NTP1OutPoint& in : usedInputs) {
        rawTx.vin.push_back(CTxIn(in.getHash(), in.getIndex()));
    }

    // Pre-check input data for validity
    for (const NTP1SendTokensOneRecipientData& rcp : ntp1recipients) {
        CScript scriptPubKey;
        scriptPubKey.SetDestination(CBitcoinAddress(rcp.destination).Get());
        // here we add only nebls. NTP1 tokens will be added later
        if (rcp.tokenId == NTP1SendTxData::NEBL_TOKEN_ID) {
            using NeblInt = int64_t;
            NeblInt val   = 0;
            if (rcp.amount > NTP1Int(std::numeric_limits<NeblInt>::max())) {
                val = std::numeric_limits<NeblInt>::max();
            } else if (rcp.amount < 0) {
                val = 0;
            } else {
                val = rcp.amount.convert_to<NeblInt>();
            }
            rawTx.vout.push_back(CTxOut(val, scriptPubKey));
        }
    }

    // add NTP1 outputs to tx
    int tokenOutputOffset = -1;
    tokenOutputOffset     = CWallet::AddNTP1TokenOutputsToTx(rawTx, tokenSelector);

    // add NTP1 inputs to tx
    std::vector<NTP1Script::TransferInstruction> TIs;
    TIs = CWallet::AddNTP1TokenInputsToTx(rawTx, tokenSelector, tokenOutputOffset);

    processedMetadata =
        rawNTP1Data.applyMetadataEncryption(rawTx, tokenSelector.getNTP1TokenRecipientsList());
    CWallet::SetTxNTP1OpRet(rawTx, TIs, processedMetadata);

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;
    return HexStr(ss.begin(), ss.end());
}

Value decoderawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("decoderawtransaction <hex string> [ignoreNTP1=false]\n"
                            "Return a JSON object representing the serialized, hex-encoded transaction. "
                            "Not ignoring NTP1 will try to retireve NTP1 data from the database. This "
                            "won't work if the transaction is not in the blockchain.");

    RPCTypeCheck(params, list_of(str_type));

    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream           ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction          tx;
    try {
        ssData >> tx;
    } catch (std::exception& e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    bool fIgnoreNTP1 = false;
    if (params.size() > 1)
        fIgnoreNTP1 = params[1].get_bool();

    Object result;
    TxToJSON(tx, 0, result, fIgnoreNTP1);

    return result;
}

Value decodescript(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("decodescript <hex string>\n"
                            "Decode a hex-encoded script.");

    RPCTypeCheck(params, list_of(str_type));

    Object  r;
    CScript script;
    if (params[0].get_str().size() > 0) {
        vector<unsigned char> scriptData(ParseHexV(params[0], "argument"));
        script = CScript(scriptData.begin(), scriptData.end());
    } else {
        // Empty scripts are valid
    }
    ScriptPubKeyToJSON(script, r, false);

    r.push_back(Pair("p2sh", CBitcoinAddress(script.GetID()).ToString()));
    return r;
}

Value signrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "signrawtransaction <hex string> [{\"txid\":txid,\"vout\":n,\"scriptPubKey\":hex},...] "
            "[<privatekey1>,...] [sighashtype=\"ALL\"]\n"
            "Sign inputs for raw transaction (serialized, hex-encoded).\n"
            "Second optional argument (may be null) is an array of previous transaction outputs that\n"
            "this transaction depends on but may not yet be in the blockchain.\n"
            "Third optional argument (may be null) is an array of base58-encoded private\n"
            "keys that, if given, will be the only keys used to sign the transaction.\n"
            "Fourth optional argument is a string that is one of six values; ALL, NONE, SINGLE or\n"
            "ALL|ANYONECANPAY, NONE|ANYONECANPAY, SINGLE|ANYONECANPAY.\n"
            "Returns json object with keys:\n"
            "  hex : raw transaction with signature(s) (hex-encoded string)\n"
            "  complete : 1 if transaction has a complete set of signature (0 if not)" +
            HelpRequiringPassphrase());

    RPCTypeCheck(params, list_of(str_type)(array_type)(array_type)(str_type), true);

    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream           ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    vector<CTransaction>  txVariants;
    while (!ssData.empty()) {
        try {
            CTransaction tx;
            ssData >> tx;
            txVariants.push_back(tx);
        } catch (std::exception& e) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
        }
    }

    if (txVariants.empty())
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transaction");

    // mergedTx will end up with all the signatures; it
    // starts as a clone of the rawtx:
    CTransaction mergedTx(txVariants[0]);
    bool         fComplete = true;

    // Fetch previous transactions (inputs):
    map<COutPoint, CScript> mapPrevOut;
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++) {
        CTransaction           tempTx;
        MapPrevTx              mapPrevTx;
        CTxDB                  txdb("r");
        map<uint256, CTxIndex> unused;
        bool                   fInvalid;

        // FetchInputs aborts on failure, so we go one at a time.
        tempTx.vin.push_back(mergedTx.vin[i]);
        tempTx.FetchInputs(txdb, unused, false, false, mapPrevTx, fInvalid);

        // Copy results into mapPrevOut:
        for (const CTxIn& txin : tempTx.vin) {
            const uint256& prevHash = txin.prevout.hash;
            if (mapPrevTx.count(prevHash) && mapPrevTx[prevHash].second.vout.size() > txin.prevout.n)
                mapPrevOut[txin.prevout] = mapPrevTx[prevHash].second.vout[txin.prevout.n].scriptPubKey;
        }
    }

    // Add previous txouts given in the RPC call:
    if (params.size() > 1 && params[1].type() != null_type) {
        Array prevTxs = params[1].get_array();
        for (Value& p : prevTxs) {
            if (p.type() != obj_type)
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                                   "expected object with {\"txid'\",\"vout\",\"scriptPubKey\"}");

            Object prevOut = p.get_obj();

            RPCTypeCheck(prevOut,
                         map_list_of("txid", str_type)("vout", int_type)("scriptPubKey", str_type));

            string txidHex = find_value(prevOut, "txid").get_str();
            if (!IsHex(txidHex))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "txid must be hexadecimal");
            uint256 txid;
            txid.SetHex(txidHex);

            int nOut = find_value(prevOut, "vout").get_int();
            if (nOut < 0)
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "vout must be positive");

            string pkHex = find_value(prevOut, "scriptPubKey").get_str();
            if (!IsHex(pkHex))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "scriptPubKey must be hexadecimal");
            vector<unsigned char> pkData(ParseHex(pkHex));
            CScript               scriptPubKey(pkData.begin(), pkData.end());

            COutPoint outpoint(txid, nOut);
            if (mapPrevOut.count(outpoint)) {
                // Complain if scriptPubKey doesn't match
                if (mapPrevOut[outpoint] != scriptPubKey) {
                    string err("Previous output scriptPubKey mismatch:\n");
                    err = err + mapPrevOut[outpoint].ToString() + "\nvs:\n" + scriptPubKey.ToString();
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, err);
                }
            } else
                mapPrevOut[outpoint] = scriptPubKey;
        }
    }

    bool           fGivenKeys = false;
    CBasicKeyStore tempKeystore;
    if (params.size() > 2 && params[2].type() != null_type) {
        fGivenKeys = true;
        Array keys = params[2].get_array();
        for (Value k : keys) {
            CBitcoinSecret vchSecret;
            bool           fGood = vchSecret.SetString(k.get_str());
            if (!fGood)
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
            CKey    key;
            bool    fCompressed;
            CSecret secret = vchSecret.GetSecret(fCompressed);
            key.SetSecret(secret, fCompressed);
            tempKeystore.AddKey(key);
        }
    } else
        EnsureWalletIsUnlocked();

    const CKeyStore& keystore = (fGivenKeys ? tempKeystore : *pwalletMain);

    int nHashType = SIGHASH_ALL;
    if (params.size() > 3 && params[3].type() != null_type) {
        static map<string, int> mapSigHashValues =
            boost::assign::map_list_of(string("ALL"), int(SIGHASH_ALL))(
                string("ALL|ANYONECANPAY"), int(SIGHASH_ALL | SIGHASH_ANYONECANPAY))(
                string("NONE"), int(SIGHASH_NONE))(string("NONE|ANYONECANPAY"),
                                                   int(SIGHASH_NONE | SIGHASH_ANYONECANPAY))(
                string("SINGLE"), int(SIGHASH_SINGLE))(string("SINGLE|ANYONECANPAY"),
                                                       int(SIGHASH_SINGLE | SIGHASH_ANYONECANPAY));
        string strHashType = params[3].get_str();
        if (mapSigHashValues.count(strHashType))
            nHashType = mapSigHashValues[strHashType];
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid sighash param");
    }

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

    // Sign what we can:
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++) {
        CTxIn& txin = mergedTx.vin[i];
        if (mapPrevOut.count(txin.prevout) == 0) {
            fComplete = false;
            continue;
        }
        const CScript& prevPubKey = mapPrevOut[txin.prevout];

        txin.scriptSig.clear();
        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < mergedTx.vout.size()))
            SignSignature(keystore, prevPubKey, mergedTx, i, nHashType);

        // ... and merge in other signatures:
        for (const CTransaction& txv : txVariants) {
            txin.scriptSig =
                CombineSignatures(prevPubKey, mergedTx, i, txin.scriptSig, txv.vin[i].scriptSig);
        }
        if (!VerifyScript(txin.scriptSig, prevPubKey, mergedTx, i, true, true, 0))
            fComplete = false;
    }

    Object      result;
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << mergedTx;
    result.push_back(Pair("hex", HexStr(ssTx.begin(), ssTx.end())));
    result.push_back(Pair("complete", fComplete));

    return result;
}

Value sendrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "sendrawtransaction <hex string>\n"
            "Submits raw transaction (serialized, hex-encoded) to local node and network.");

    RPCTypeCheck(params, list_of(str_type));

    // parse hex string from parameter
    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream           ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction          tx;

    // deserialize binary data stream
    try {
        ssData >> tx;
    } catch (std::exception& e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }
    uint256 hashTx = tx.GetHash();

    // See if the transaction is already in a block
    // or in the memory pool:
    CTransaction existingTx;
    uint256      hashBlock = 0;
    if (GetTransaction(hashTx, existingTx, hashBlock)) {
        if (hashBlock != 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               string("transaction already in block ") + hashBlock.GetHex());
        // Not in block, but already in the memory pool; will drop
        // through to re-relay it.
    } else {
        // push to local node
        bool fMissingInputs = false;
        if (!AcceptToMemoryPool(mempool, tx, &fMissingInputs)) {
            if (fMissingInputs) {
                throw JSONRPCError(RPC_TRANSACTION_ERROR, "Missing inputs");
            }
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX rejected");
        }

        SyncWithWallets(tx, NULL, true);
    }
    RelayTransaction(tx, hashTx);

    return hashTx.GetHex();
}
