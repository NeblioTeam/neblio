// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "NetworkForks.h"
#include "base58.h"
#include "bitcoinrpc.h"
#include "blockmetadata.h"
#include "boost/make_shared.hpp"
#include "coldstakedelegation.h"
#include "globals.h"
#include "init.h"
#include "main.h"
#include "udaddress.h"
#include "wallet.h"
#include "walletdb.h"

using namespace json_spirit;
using namespace std;

int64_t                 nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry,
                     bool ignoreNTP1);

static void accountingDeprecationCheck()
{
    if (!GetBoolArg("-enableaccounts", false))
        throw runtime_error(
            "Accounting API is deprecated and will be removed in future.\n"
            "It can easily result in negative or odd balances if misused or misunderstood, which has "
            "happened in the field.\n"
            "If you still want to enable it, add to your config file enableaccounts=1 and it is HIGHLY "
            "recommended that staking is DISABLED with staking=0\n");
}

std::string HelpRequiringPassphrase()
{
    return pwalletMain->IsCrypted()
               ? "\nrequires wallet passphrase to be set with walletpassphrase first"
               : "";
}

void EnsureWalletIsUnlocked()
{
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED,
                           "Error: Please enter the wallet passphrase with walletpassphrase first.");
    if (fWalletUnlockStakingOnly)
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Wallet is unlocked for staking only.");
}

void WalletTxToJSON(const CWalletTx& wtx, Object& entry)
{
    const CTxDB txdb;

    int confirms = wtx.GetDepthInMainChain(txdb, txdb.GetBestBlockHash());
    entry.push_back(Pair("confirmations", confirms));
    if (wtx.IsCoinBase() || wtx.IsCoinStake())
        entry.push_back(Pair("generated", true));
    if (confirms > 0) {
        entry.push_back(Pair("blockhash", wtx.hashBlock.GetHex()));
        entry.push_back(Pair("blockindex", wtx.nIndex));
        const auto    bi    = txdb.ReadBlockIndex(wtx.hashBlock);
        const int64_t nTime = static_cast<int64_t>(bi ? bi->nTime : 0);
        entry.push_back(Pair("blocktime", nTime));
    }
    entry.push_back(Pair("txid", wtx.GetHash().GetHex()));
    json_spirit::Array conflicts;
    for (const uint256& conflict : wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.push_back(Pair("walletconflicts", conflicts));
    entry.push_back(Pair("time", (int64_t)wtx.GetTxTime()));
    entry.push_back(Pair("timereceived", (int64_t)wtx.nTimeReceived));
    for (const PAIRTYPE(const string, string) & item : wtx.mapValue)
        entry.push_back(Pair(item.first, item.second));
}

string AccountFromValue(const Value& value)
{
    string strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}

Value getinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getinfo\n"
                            "Returns an object containing various state info.");

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    const CTxDB txdb;

    auto bestBlockIndex = txdb.GetBestBlockIndex();
    if (!bestBlockIndex) {
        NLog.write(b_sev::err, "Failed to find best block in RPC call to getinfo");
        throw runtime_error("Failed to find best block in RPC call to getinfo");
    }

    Object obj, diff;
    obj.push_back(Pair("version", FormatFullVersion()));
    obj.push_back(Pair("protocolversion", (int)PROTOCOL_VERSION));
    obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    obj.push_back(Pair("balance", ValueFromAmount(pwalletMain->GetBalance(txdb))));
    obj.push_back(Pair("newmint", ValueFromAmount(pwalletMain->GetNewMint(txdb))));
    obj.push_back(Pair("stake", ValueFromAmount(pwalletMain->GetStake(txdb))));
    obj.push_back(Pair("blocks", (int)bestBlockIndex->nHeight));
    obj.push_back(Pair("timeoffset", (int64_t)GetTimeOffset()));
    const boost::optional<BlockMetadata> blockMetadata =
        txdb.ReadBlockMetadata(bestBlockIndex->GetBlockHash());
    obj.push_back(Pair("moneysupply",
                       blockMetadata ? ValueFromAmount(blockMetadata->getMoneySupply()) : "<ERROR>"));
    obj.push_back(Pair("connections", (int)vNodes.size()));
    obj.push_back(Pair("proxy", (proxy.first.IsValid() ? proxy.first.ToStringIPPort() : string())));
    obj.push_back(Pair("ip", addrSeenByPeer.get().ToStringIP()));

    diff.push_back(Pair("proof-of-work", GetDifficulty()));
    const CBlockIndex bi = GetLastBlockIndex(*bestBlockIndex, true, txdb);
    diff.push_back(Pair("proof-of-stake", GetDifficulty(&bi)));
    obj.push_back(Pair("difficulty", diff));

    obj.push_back(Pair("testnet", Params().NetType() != NetworkType::Mainnet));
    obj.push_back(
        Pair("tachyon", Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, txdb)));
    obj.push_back(Pair("keypoololdest", (int64_t)pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize", (int)pwalletMain->GetKeyPoolSize()));
    obj.push_back(Pair("paytxfee", ValueFromAmount(nTransactionFee)));
    obj.push_back(Pair("mininput", ValueFromAmount(nMinimumInputValue)));
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", (int64_t)nWalletUnlockTime / 1000));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));
    return obj;
}

CWalletTx SubmitColdStakeDelegationTx(CReserveKey& reservekey, CAmount nValue,
                                      const CScript& scriptPubKey, bool fUseDelegated)
{
    // Get NTP1 wallet
    boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();
    ntp1wallet->setRetrieveFullMetadata(false);
    ntp1wallet->update();

    NTP1SendTxData tokenSelector;
    tokenSelector.selectNTP1Tokens(ntp1wallet, std::vector<COutPoint>(),
                                   std::vector<NTP1SendTokensOneRecipientData>(), false);

    const CTxDB txdb;

    const CAmount currBalance =
        pwalletMain->GetBalance(txdb) + (fUseDelegated ? pwalletMain->GetDelegatedBalance(txdb) : 0);

    // Create the transaction
    CAmount     nFeeRequired;
    CWalletTx   wtxNew;
    std::string strError;
    if (!pwalletMain->CreateTransaction(txdb, scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired,
                                        tokenSelector, &strError, RawNTP1MetadataBeforeSend(), false,
                                        nullptr, fUseDelegated)) {
        if (nValue + nFeeRequired > currBalance)
            strError = fmt::format("Error: This transaction requires a transaction fee of at least {} "
                                   "because of its amount, complexity, or use of recently received "
                                   "funds!",
                                   FormatMoney(nFeeRequired));
        fmt::format("{} : {}", FUNCTIONSIG, strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtxNew;
}

RPCErrorCode ColdStakeDelegationRPCErrorCode(const ColdStakeDelegationErrorCode errorCode)
{
    switch (errorCode) {
    case ColdStakingDisabled:
        return RPC_VERIFY_ERROR;
    case InvalidStakerAddress:
        return RPC_INVALID_ADDRESS_OR_KEY;
    case StakerAddressPubKeyHashError:
        return RPC_WALLET_ERROR;
    case InvalidAmount:
        return RPC_INVALID_PARAMETER;
    case InsufficientBalance:
        return RPC_WALLET_INSUFFICIENT_FUNDS;
    case WalletLocked:
        return RPC_WALLET_UNLOCK_NEEDED;
    case InvalidOwnerAddress:
        return RPC_INVALID_ADDRESS_OR_KEY;
    case OwnerAddressPubKeyHashError:
        return RPC_WALLET_ERROR;
    case OwnerAddressNotInWallet:
        return RPC_INVALID_ADDRESS_OR_KEY;
    case KeyPoolEmpty:
        return RPC_WALLET_KEYPOOL_RAN_OUT;
    case GeneratedOwnerAddressPubKeyHashError:
        return RPC_WALLET_ERROR;
    }
    return RPC_MISC_ERROR;
}

struct CreateColdStakeDelegationParsedParams
{
    // the address of the staker
    std::string stakerAddress;

    // the amount to be delegated
    CAmount nValue;

    // owner address is optional, otherwise taken from the wallet
    boost::optional<std::string> ownerAddress;

    // force using an external address that doesn't belong to this wallet
    bool fForceExternalAddr;

    // include already delegated coins
    bool fUseDelegated = false;

    // Check that Cold Staking has been enforced or fForceNotEnabled = true
    bool fForceNotEnabled = false;
};

CreateColdStakeDelegationParsedParams ParseCreateColdStakeDelegationParams(const Array& params)
{
    CreateColdStakeDelegationParsedParams result;

    result.stakerAddress = params[0].get_str();

    result.nValue = AmountFromValue(params[1]);

    // owner address is optional, otherwise taken from the wallet
    if (params.size() > 2 && params[2].type() != Value_type::null_type && !params[2].get_str().empty())
        result.ownerAddress = params[2].get_str();

    // force using an external address that doesn't belong to this wallet
    result.fForceExternalAddr =
        params.size() > 3 && params[3].type() != Value_type::null_type ? params[3].get_bool() : false;

    // include already delegated coins
    result.fUseDelegated = false;
    if (params.size() > 4 && params[4].type() != Value_type::null_type)
        result.fUseDelegated = params[4].get_bool();

    // Check that Cold Staking has been enforced or fForceNotEnabled = true
    result.fForceNotEnabled = false;
    if (params.size() > 5 && params[5].type() != Value_type::null_type)
        result.fForceNotEnabled = params[5].get_bool();

    return result;
}

Value delegatestake(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 6)
        throw std::runtime_error(
            "delegatestake \"stakingaddress\" amount ( \"owneraddress\" fExternalOwner fUseDelegated "
            "fForceNotEnabled )\n"
            "\nDelegate an amount to a given address for cold staking. The amount is a real and is "
            "rounded to the nearest 0.00000001\n" +
            HelpRequiringPassphrase() +
            "\n"

            "\nArguments:\n"
            "1. \"stakingaddress\"      (string, required) The neblio address to delegate.\n"
            "2. \"amount\"              (numeric, required) The amount in nebl to delegate for staking. "
            "eg 100\n"
            "3. \"owneraddress\"        (string, optional) The neblio address corresponding to the key "
            "that will be able to spend the stake. \n"
            "                               If not provided, or empty string, a new wallet address is "
            "generated.\n"
            "4. \"fExternalOwner\"      (boolean, optional, default = false) use the provided "
            "'owneraddress' anyway, even if not present in this wallet.\n"
            "                               WARNING: The owner of the keys to 'owneraddress' will be "
            "the only one allowed to spend these coins.\n"
            "5. \"fUseDelegated\"       (boolean, optional, default = false) include already delegated "
            "inputs if needed."
            "6. \"fForceNotEnabled\"    (boolean, optional, default = false) force the creation even if "
            "SPORK 17 is disabled (for tests)."

            "\nResult:\n"
            "{\n"
            "   \"owner_address\": \"xxx\"   (string) The owner (delegator) owneraddress.\n"
            "   \"staker_address\": \"xxx\"  (string) The cold staker (delegate) stakingaddress.\n"
            "   \"txid\": \"xxx\"            (string) The stake delegation transaction id.\n"
            "}\n"
            "\nExamples:\n" +
            "delegatestake \"TAUWWD4oJ3FM36F9CzUwVofGx17X6XtkBG\" 100\n"
            "delegatestake \"TAUWWD4oJ3FM36F9CzUwVofGx17X6XtkBG\" 1000\n"
            "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg34fk\""
            "delegatestake \"TAUWWD4oJ3FM36F9CzUwVofGx17X6XtkBG\" 1000 "
            "\"DMJRSsuU9zfyrvxVaAEFQqK4MxZg34fk\"");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CReserveKey reservekey(pwalletMain.get());

    const CreateColdStakeDelegationParsedParams pParams = ParseCreateColdStakeDelegationParams(params);

    const auto delegRes = CreateColdStakeDelegation(pParams.stakerAddress, pParams.nValue,
                                                    pParams.ownerAddress, pParams.fForceExternalAddr,
                                                    pParams.fUseDelegated, pParams.fForceNotEnabled);

    if (delegRes.isErr()) {
        throw JSONRPCError(ColdStakeDelegationRPCErrorCode(delegRes.unwrapErr()),
                           ColdStakeDelegationErrorStr(delegRes.unwrapErr()));
    }

    const CoinStakeDelegationResult res = delegRes.unwrap();

    const CWalletTx wtx =
        SubmitColdStakeDelegationTx(reservekey, pParams.nValue, res.scriptPubKey, pParams.fUseDelegated);

    if (!pwalletMain->CommitTransaction(wtx, CTxDB(), reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Error: The transaction was rejected! This might happen if some of the coins "
                           "in your wallet were already spent, such as if you used a copy of wallet.dat "
                           "and coins were spent in the copy but not marked as spent here.");

    Object ret = res.AddressesToJsonObject();

    ret.push_back(Pair("txid", wtx.GetHash().GetHex()));
    return ret;
}

Value rawdelegatestake(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw std::runtime_error(
            "rawdelegatestake \"stakingaddress\" amount ( \"owneraddress\" fExternalOwner fUseDelegated "
            ")\n"
            "\nDelegate an amount to a given address for cold staking. The amount is a real and is "
            "rounded to the nearest 0.00000001\n"
            "\nDelegate transaction is returned as json object."
            "\n"
            "\nArguments:\n"
            "1. \"stakingaddress\"      (string, required) The neblio address to delegate.\n"
            "2. \"amount\"              (numeric, required) The amount in nebl to delegate for "
            "staking. "
            "eg 100\n"
            "3. \"owneraddress\"        (string, optional) The neblio address corresponding to the "
            "key "
            "that will be able to spend the stake. \n"
            "                               If not provided, or empty string, a new wallet address "
            "is "
            "generated.\n"
            "4. \"fExternalOwner\"      (boolean, optional, default = false) use the provided "
            "'owneraddress' anyway, even if not present in this wallet.\n"
            "                               WARNING: The owner of the keys to 'owneraddress' will "
            "be "
            "the only one allowed to spend these coins.\n"
            "5. \"fUseDelegated         (boolean, optional, default = false) include already "
            "delegated "
            "inputs if needed."

            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"id\",        (string) The transaction id (same as provided)\n"
            "  \"version\" : n,          (numeric) The version\n"
            "  \"size\" : n,             (numeric) The serialized transaction size\n"
            "  \"locktime\" : ttt,       (numeric) The lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) The transaction id\n"
            "       \"vout\": n,         (numeric) \n"
            "       \"scriptSig\": {     (json object) The script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"sequence\": n      (numeric) The script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [              (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) The value in btc\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptPubKey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqSigs\" : n,            (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) The type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"neblioaddress\"        (string) neblio address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\",       (string) The serialized, hex-encoded data for 'txid'\n"
            "}\n"

            "\nExamples:\n"
            "rawdelegatestake \"TAUWWD4oJ3FM36F9CzUwVofGx17X6XtkBG\" 100 rawdelegatestake"
            "\"TAUWWD4oJ3FM36F9CzUwVofGx17X6XtkBG\" 1000 \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg34fk\" "
            "rawdelegatestake"
            "\"TAUWWD4oJ3FM36F9CzUwVofGx17X6XtkBG\" 1000 \"DMJRSsuU9zfyrvxVaAEFQqK4MxZg34fk\"");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CReserveKey reservekey(pwalletMain.get());

    const CreateColdStakeDelegationParsedParams pParams = ParseCreateColdStakeDelegationParams(params);

    const auto delegRes = CreateColdStakeDelegation(pParams.stakerAddress, pParams.nValue,
                                                    pParams.ownerAddress, pParams.fForceExternalAddr,
                                                    pParams.fUseDelegated, pParams.fForceNotEnabled);

    if (delegRes.isErr()) {
        throw JSONRPCError(ColdStakeDelegationRPCErrorCode(delegRes.unwrapErr()),
                           ColdStakeDelegationErrorStr(delegRes.unwrapErr()));
    }

    CoinStakeDelegationResult res = delegRes.unwrap();

    CWalletTx wtx =
        SubmitColdStakeDelegationTx(reservekey, pParams.nValue, res.scriptPubKey, pParams.fUseDelegated);

    Object result;
    TxToJSON(wtx, 0, result, true);

    return result;
}

Value getnewpubkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("getnewpubkey [account]\n"
                            "Returns new public key for coinbase generation.");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT,
                           "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBookEntry(keyID, strAccount);
    vector<unsigned char> vchPubKey = newKey.Raw();

    return HexStr(vchPubKey.begin(), vchPubKey.end());
}

Value getnewaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("getnewaddress [account]\n"
                            "Returns a new neblio address for receiving payments.  "
                            "If [account] is specified, it is added to the address book "
                            "so payments received with the address will be credited to [account].");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT,
                           "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBookEntry(keyID, strAccount);

    return CBitcoinAddress(keyID).ToString();
}

Value getrawchangeaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error("getrawchangeaddress\n"
                                 "\nReturns a new neblio address, for receiving change.\n"
                                 "This is for use with raw transactions, NOT normal use.\n"

                                 "\nResult:\n"
                                 "\"address\"    (string) The address\n"

                                 "\nExamples:\n"
                                 "getrawchangeaddress");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    CReserveKey reservekey(pwalletMain.get());
    CPubKey     vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT,
                           "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CKeyID keyID = vchPubKey.GetID();

    return CBitcoinAddress(keyID).ToString();
}

CBitcoinAddress GetAccountAddress(string strAccount, bool bForceNew = false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    bool bKeyUsed = false;

    // Check if the current key has been used
    if (account.vchPubKey.IsValid()) {
        CScript scriptPubKey;
        scriptPubKey.SetDestination(account.vchPubKey.GetID());
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && account.vchPubKey.IsValid(); ++it) {
            const CWalletTx& wtx = (*it).second;
            for (const CTxOut& txout : wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    bKeyUsed = true;
        }
    }

    // Generate a new key
    if (!account.vchPubKey.IsValid() || bForceNew || bKeyUsed) {
        if (!pwalletMain->GetKeyFromPool(account.vchPubKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT,
                               "Error: Keypool ran out, please call keypoolrefill first");

        pwalletMain->SetAddressBookEntry(account.vchPubKey.GetID(), strAccount);
        walletdb.WriteAccount(strAccount, account);
    }

    return CBitcoinAddress(account.vchPubKey.GetID());
}

Value getaccountaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccountaddress <account>\n"
            "Returns the current neblio address for receiving payments to this account.");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    Value ret;

    ret = GetAccountAddress(strAccount).ToString();

    return ret;
}

Value setaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("setaccount <neblioaddress> <account>\n"
                            "Sets the account associated with the given address.");

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");

    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Detect when changing the account of an address that is the 'unused current key' of another
    // account:
    if (const auto entry = pwalletMain->mapAddressBook.get(address.Get())) {
        string strOldAccount = entry->name;
        if (address == GetAccountAddress(strOldAccount))
            GetAccountAddress(strOldAccount, true);
    }

    pwalletMain->SetAddressBookEntry(address.Get(), strAccount);

    return Value::null;
}

Value getaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getaccount <neblioaddress>\n"
                            "Returns the account associated with the given address.");

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");

    string     strAccount;
    const auto mi = pwalletMain->mapAddressBook.get(address.Get());
    if (mi.is_initialized() && !mi->name.empty())
        strAccount = mi->name;
    return strAccount;
}

Value getaddressesbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getaddressesbyaccount <account>\n"
                            "Returns the list of addresses for the given account.");

    string strAccount = AccountFromValue(params[0]);

    // Find all addresses that have the given account
    Array      ret;
    const auto addrBook = pwalletMain->mapAddressBook.getInternalMap();
    for (const auto& item : addrBook) {
        const CBitcoinAddress& address = item.first;
        const string&          strName = item.second.name;
        if (strName == strAccount)
            ret.push_back(address.ToString());
    }
    return ret;
}

Value sendtoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error("sendtoaddress <neblioaddress> <amount> [comment] [comment-to]\n"
                            "<amount> is a real and is rounded to the nearest 0.000001" +
                            HelpRequiringPassphrase());

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");

    // Amount
    const CAmount nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["to"] = params[3].get_str();

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED,
                           "Error: Please enter the wallet passphrase with walletpassphrase first.");

    string strError = pwalletMain->SendMoneyToDestination(CTxDB(), address.Get(), nAmount, wtx);
    if (strError != "")
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}

Value sendntp1toaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 7)
        throw runtime_error("sendntp1toaddress <neblioaddress> <amount> <tokenId/tokenName> [NTP1 "
                            "metadata=\"\"] [encrypt-meta-data=false] [comment] [comment-to]\n" +
                            HelpRequiringPassphrase());

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");

    // Amount
    const NTP1Int nAmount = NTP1AmountFromValue(params[1]);

    // Get NTP1 wallet
    boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();
    ntp1wallet->setRetrieveFullMetadata(false);
    ntp1wallet->update();

    // figure out token id from either the id or the name
    std::string tokenId;
    std::string providedId = params[2].get_str();

    const std::unordered_map<std::string, NTP1TokenMetaData> tokenMetadataMap =
        ntp1wallet->getTokenMetadataMap();
    // token id was not found
    if (tokenMetadataMap.find(providedId) == tokenMetadataMap.end()) {
        int nameCount = 0; // number of tokens that have that name
        // try to find whether the name of the token meatches with what's provided
        for (const auto& tokenMetadata : tokenMetadataMap) {
            if (tokenMetadata.second.getTokenName() == providedId) {
                tokenId = tokenMetadata.second.getTokenId();
                nameCount++;
            }
        }
        if (tokenId == "") {
            throw std::runtime_error("Failed to find token by the id/name: " + providedId);
        }
        if (nameCount > 1) {
            throw std::runtime_error("Found multiple tokens by the name " + providedId);
        }
    } else {
        tokenId = params[2].get_str();
    }

    // Wallet comments
    CWalletTx                 wtx;
    RawNTP1MetadataBeforeSend ntp1metadata("", false);
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        ntp1metadata.metadata = params[3].get_str();
    if (params.size() > 4 && params[4].type() != null_type)
        ntp1metadata.encrypt = params[4].get_bool();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["comment"] = params[5].get_str();
    if (params.size() > 6 && params[6].type() != null_type && !params[6].get_str().empty())
        wtx.mapValue["to"] = params[6].get_str();

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED,
                           "Error: Please enter the wallet passphrase with walletpassphrase first.");

    string strError = pwalletMain->SendNTP1ToDestination(CTxDB(), address.Get(), nAmount, tokenId, wtx,
                                                         ntp1wallet, ntp1metadata);
    if (strError != "")
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}

std::unordered_map<std::string, std::unordered_map<std::string, std::pair<std::string, NTP1Int>>>
GetNTP1AddressVsTokenBalances()
{
    boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();
    ntp1wallet->setRetrieveFullMetadata(false);
    ntp1wallet->update();

    // map<address, map<tokenId, pair<name,balance>>>
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<std::string, NTP1Int>>>
        addressBalances;

    const std::unordered_map<NTP1OutPoint, NTP1Transaction>& ntp1outputs =
        ntp1wallet->getWalletOutputsWithTokens();

    for (const auto& outPair : ntp1outputs) {
        const NTP1Transaction& ntp1tx  = outPair.second;
        const NTP1TxOut&       ntp1out = ntp1tx.getTxOut(outPair.first.getIndex());
        const std::string      addr    = ntp1out.getAddress();
        if (addr.empty()) {
            continue;
        }
        if (addressBalances.find(addr) == addressBalances.end()) {
            addressBalances[addr] = std::unordered_map<std::string, std::pair<std::string, NTP1Int>>();
        }
        for (int i = 0; i < (int)ntp1out.tokenCount(); i++) {
            std::string tokenId   = ntp1out.getToken(i).getTokenId();
            std::string tokenName = ntp1out.getToken(i).getTokenSymbol();
            if (addressBalances[addr].find(tokenId) == addressBalances[addr].end()) {
                addressBalances[addr][tokenId] = std::make_pair(tokenName, 0);
            }
            addressBalances[addr][tokenId].second += ntp1out.getToken(i).getAmount();
        }
    }

    // print all content to cout for testing
    //    for (const auto& a : addressBalances) {
    //        for (const auto& t : a.second) {
    //            std::cout << a.first << "\t" << t.first << "\t" << t.second << std::endl;
    //        }
    //    }
    return addressBalances;
}

Value listaddressgroupings(const Array& /*params*/, bool fHelp)
{
    if (fHelp)
        throw runtime_error("listaddressgroupings\n"
                            "Lists groups of addresses which have had their common ownership\n"
                            "made public by common use as inputs or as the resulting change\n"
                            "in past transactions");

    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<std::string, NTP1Int>>>
        ntp1AddressVsTokenBalances = GetNTP1AddressVsTokenBalances();

    const CTxDB txdb;

    Array                        jsonGroupings;
    map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances(txdb);
    for (set<CTxDestination> grouping : pwalletMain->GetAddressGroupings(txdb)) {
        Array jsonGrouping;
        for (CTxDestination address : grouping) {
            Array       addressInfo;
            std::string addrStr = CBitcoinAddress(address).ToString();
            addressInfo.push_back(addrStr);
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                if (const auto entry = pwalletMain->mapAddressBook.get(CBitcoinAddress(address).Get())) {
                    addressInfo.push_back(entry->name);
                }
            }
            // add NTP1 tokens
            {
                Array ntp1SingleTokenBalance;
                if (ntp1AddressVsTokenBalances.find(addrStr) != ntp1AddressVsTokenBalances.end()) {
                    for (const std::pair<const std::string, std::pair<std::string, NTP1Int>>&
                             tokenBalance : ntp1AddressVsTokenBalances[addrStr]) {
                        Array inner;
                        inner.push_back(tokenBalance.second.first);            // token name
                        inner.push_back(ToString(tokenBalance.second.second)); // balance
                        inner.push_back(tokenBalance.first);                   // token-id
                        ntp1SingleTokenBalance.push_back(inner);
                    }
                }
                addressInfo.push_back(ntp1SingleTokenBalance);
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

Value udtoneblioaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "udtoneblioaddress <unstoppable domain address>\n"
            "Returns the neblio address associated with the provided unstoppable domain");

    string strUDDomain;
    if (params.size() > 0)
        strUDDomain = params[0].get_str();

    if (!IsUDAddressSyntaxValid(strUDDomain)) {
        throw std::runtime_error("Invalid syntax for unstoppable domains");
    }

    auto neblAddress = GetNeblioAddressFromUDAddress(strUDDomain);
    if (!neblAddress) {
        throw std::runtime_error("Failed to get address from domain. Either the address is invalid or "
                                 "internet connectivity has a problem.");
    }

    return *neblAddress;
}

Value signmessage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("signmessage <neblioaddress> <message>\n"
                            "Sign a message with the private key of an address");

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strMessage = params[1].get_str();

    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

Value verifymessage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error("verifymessage <neblioaddress> <signature> <message>\n"
                            "Verify a signed message");

    string strAddress = params[0].get_str();
    string strSign    = params[1].get_str();
    string strMessage = params[2].get_str();

    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    bool                  fInvalid = false;
    vector<unsigned char> vchSig   = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CKey key;
    if (!key.SetCompactSignature(ss.GetHash(), vchSig))
        return false;

    return (key.GetPubKey().GetID() == keyID);
}

Value getreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("getreceivedbyaddress <neblioaddress> [minconf=1]\n"
                            "Returns the total amount received by <neblioaddress> in transactions with "
                            "at least [minconf] confirmations.");

    // Bitcoin address
    const CTxDB     txdb;
    CBitcoinAddress address = CBitcoinAddress(params[0].get_str());
    CScript         scriptPubKey;
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");
    scriptPubKey.SetDestination(address.Get());

    if (IsMine(*pwalletMain, scriptPubKey) == isminetype::ISMINE_NO)
        throw JSONRPCError(RPC_WALLET_ERROR, "Address not found in wallet");

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    CAmount       nAmount       = 0;
    const uint256 bestBlockHash = txdb.GetBestBlockHash();
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
         it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || wtx.IsCoinStake() || !IsFinalTx(wtx, txdb))
            continue;

        for (const CTxOut& txout : wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain(txdb, bestBlockHash) >= nMinDepth)
                    nAmount += txout.nValue;
    }

    return ValueFromAmount(nAmount);
}

void GetAccountAddresses(string strAccount, set<CTxDestination>& setAddress)
{
    const auto addrBook = pwalletMain->mapAddressBook.getInternalMap();
    for (const auto& item : addrBook) {
        const CTxDestination& address = item.first;
        const string&         strName = item.second.name;
        if (strName == strAccount)
            setAddress.insert(address);
    }
}

Value getreceivedbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("getreceivedbyaccount <account> [minconf=1]\n"
                            "Returns the total amount received by addresses with <account> in "
                            "transactions with at least [minconf] confirmations.");

    accountingDeprecationCheck();

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Get the set of pub keys assigned to account
    string              strAccount = AccountFromValue(params[0]);
    set<CTxDestination> setAddress;
    GetAccountAddresses(strAccount, setAddress);

    // Tally
    CAmount       nAmount = 0;
    const CTxDB   txdb;
    const uint256 bestBlockHash = txdb.GetBestBlockHash();
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
         it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || wtx.IsCoinStake() || !IsFinalTx(wtx, txdb))
            continue;

        for (const CTxOut& txout : wtx.vout) {
            CTxDestination address;
            if (ExtractDestination(CTxDB(), txout.scriptPubKey, address) &&
                IsMine(*pwalletMain, address) != isminetype::ISMINE_NO && setAddress.count(address))
                if (wtx.GetDepthInMainChain(txdb, bestBlockHash) >= nMinDepth)
                    nAmount += txout.nValue;
        }
    }

    return (double)nAmount / (double)COIN;
}

Value ListaddressesForPurpose(const std::string& strPurpose)
{
    Array ret;
    {
        const auto addrBook = pwalletMain->mapAddressBook.getInternalMap();
        for (const auto& addr : addrBook) {
            if (addr.second.purpose != strPurpose)
                continue;
            Object entry;
            entry.push_back(Pair("label", addr.second.name));
            entry.push_back(Pair("address", CBitcoinAddress(addr.first).ToString()));
            ret.push_back(entry);
        }
    }

    return ret;
}

Value listdelegators(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "listdelegators ( fBlacklist )\n"
            "\nShows the list of allowed delegator addresses for cold staking.\n"

            "\nArguments:\n"
            "1. fBlacklist             (boolean, optional, default = false) Show addresses removed\n"
            "                          from the delegators whitelist\n"

            "\nResult:\n"
            "[\n"
            "   {\n"
            "   \"label\": \"yyy\",    (string) account label\n"
            "   \"address\": \"xxx\",  (string) neblio address string\n"
            "   }\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            "listdelegators");

    const bool fBlacklist = (params.size() > 0 ? params[0].get_bool() : false);
    return (fBlacklist ? ListaddressesForPurpose(AddressBook::AddressBookPurpose::DELEGABLE)
                       : ListaddressesForPurpose(AddressBook::AddressBookPurpose::DELEGATOR));
}

Value delegatoradd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "delegatoradd \"addr\" ( \"label\" )\n"
            "\nAdd the provided address <addr> into the allowed delegators AddressBook.\n"
            "This enables the staking of coins delegated to this wallet, owned by <addr>\n"

            "\nArguments:\n"
            "1. \"addr\"        (string, required) The address to whitelist\n"
            "2. \"label\"       (string, optional) A label for the address to whitelist\n"

            "\nResult:\n"
            "true|false           (boolean) true if successful.\n"

            "\nExamples:\n"
            "delegatoradd <address>\n");

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");

    const std::string strLabel = (params.size() > 1 ? params[1].get_str() : "");

    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get KeyID from neblio address");

    return pwalletMain->SetAddressBookEntry(keyID, strLabel, AddressBook::AddressBookPurpose::DELEGATOR);
}

Value delegatorremove(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "delegatorremove \"addr\"\n"
            "\nUpdates the provided address <addr> from the allowed delegators keystore to a "
            "\"delegable\" status.\n"
            "This disables the staking of coins delegated to this wallet, owned by <addr>\n"

            "\nArguments:\n"
            "1. \"addr\"        (string, required) The address to blacklist\n"

            "\nResult:\n"
            "true|false           (boolean) true if successful.\n"

            "\nExamples:\n"
            "delegatorremove <address>");

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");

    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get KeyID from neblio address");

    if (!pwalletMain->HasAddressBookEntry(keyID))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get neblio address from addressBook");

    std::string label = "";
    {
        const auto mi = pwalletMain->mapAddressBook.get(address.Get());
        if (mi.is_initialized()) {
            label = mi->name;
        }
    }

    return pwalletMain->SetAddressBookEntry(keyID, label, AddressBook::AddressBookPurpose::DELEGABLE);
}

Value liststakingaddresses(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error("liststakingaddresses \"addr\"\n"
                                 "\nShows the list of staking addresses for this wallet.\n"

                                 "\nResult:\n"
                                 "[\n"
                                 "   {\n"
                                 "   \"label\": \"yyy\",  (string) account label\n"
                                 "   \"address\": \"xxx\",  (string) neblio address string\n"
                                 "   }\n"
                                 "  ...\n"
                                 "]\n"
                                 "\nExamples:\n"
                                 "liststakingaddresses");

    return ListaddressesForPurpose(AddressBook::AddressBookPurpose::COLD_STAKING);
}

CAmount GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth,
                          const isminefilter& filter, const ITxDB& txdb)
{
    CAmount nBalance = 0;

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    // Tally wallet transactions
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
         it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;

        bool fConflicted = false;
        int  depth       = wtx.GetDepthAndMempool(fConflicted, txdb, bestBlockHash);

        const CTxDB txdb;
        if (!IsFinalTx(wtx, txdb) || wtx.GetBlocksToMaturity(txdb, bestBlockHash) > 0 || depth < 0 ||
            fConflicted)
            continue;

        CAmount nReceived, nSent, nFee;
        wtx.GetAccountAmounts(txdb, strAccount, nReceived, nSent, nFee, filter);

        if (nReceived != 0 && wtx.GetDepthInMainChain(txdb, bestBlockHash) >= nMinDepth &&
            wtx.GetBlocksToMaturity(txdb, bestBlockHash) == 0)
            nBalance += nReceived;
        nBalance -= nSent + nFee;
    }

    // Tally internal accounting entries
    nBalance += walletdb.GetAccountCreditDebit(strAccount);

    return nBalance;
}

CAmount GetAccountBalance(const string& strAccount, int nMinDepth, const isminefilter& filter,
                          const ITxDB& txdb)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter, txdb);
}

Value getcoldstakingbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "getcoldstakingbalance ( \"account\" )\n"
            "\nIf account is not specified, returns the server's total available cold balance.\n"
            "If account is specified (DEPRECATED), returns the cold balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, optional) DEPRECATED. The selected account, or \"*\" for "
            "entire wallet. It may be the default account using \"\".\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in nebl received for this account in P2CS "
            "contracts.\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            "getcoldstakingbalance"
            "\nAs a json rpc call\n"
            "getcoldstakingbalance \"*\"");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 0)
        return ValueFromAmount(pwalletMain->GetColdStakingBalance(CTxDB()));

    std::string strAccount = params[0].get_str();
    return ValueFromAmount(GetAccountBalance(strAccount, /*nMinDepth*/ 1, ISMINE_COLD, CTxDB()));
}

Value getdelegatedbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "getdelegatedbalance ( \"account\" )\n"
            "\nIf account is not specified, returns the server's total available delegated balance (sum "
            "of all utxos delegated\n"
            "to a cold staking address to stake on behalf of addresses of this wallet).\n"
            "If account is specified (DEPRECATED), returns the cold balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, optional) DEPRECATED. The selected account, or \"*\" for "
            "entire wallet. It may be the default account using \"\".\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in nebl received for this account in P2CS "
            "contracts.\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            "getdelegatedbalance \nAs a json rpc call\n"
            "getdelegatedbalance \"*\"");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 0)
        return ValueFromAmount(pwalletMain->GetDelegatedBalance(CTxDB()));

    std::string strAccount = params[0].get_str();
    return ValueFromAmount(
        GetAccountBalance(strAccount, /*nMinDepth*/ 1, ISMINE_SPENDABLE_DELEGATED, CTxDB()));
}

Value getbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getbalance [account] [minconf=1] [includeWatchonly=false] [includeDelegated=true]\n"
            "\n"
            "If [account] is not specified, returns the server's total available balance.\n"
            "If [account] is specified, returns the balance in the account."
            "\n"
            "\nArguments:\n"
            "1. \"account\"      (string, optional) DEPRECATED. The selected account, "
            "or \"*\" for entire wallet. It may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include "
            "transactions confirmed at least this many times.\n"
            "3. includeWatchonly (bool, optional, default=false) Also include balance "
            "in watchonly addresses (see 'importaddress')\n"
            "4. includeDelegated (bool, optional, default=true) Also include balance "
            "delegated to cold stakers\n");

    const CTxDB txdb;

    if (params.size() == 0)
        return ValueFromAmount(pwalletMain->GetBalance(txdb));

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    isminefilter filter = static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE);
    if (params.size() > 2 && params[2].get_bool())
        filter = filter | static_cast<isminefilter>(isminetype::ISMINE_WATCH_ONLY);
    if (!(params.size() > 3) || params[3].get_bool())
        filter = filter | static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE_DELEGATED);

    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and getbalance '*' 0 should return the same number.
        CAmount nBalance = 0;
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end(); ++it) {
            const CWalletTx& wtx = (*it).second;
            if (!wtx.IsTrusted(txdb, bestBlockHash))
                continue;

            CAmount                             allFee;
            string                              strSentAccount;
            list<pair<CTxDestination, CAmount>> listReceived;
            list<pair<CTxDestination, CAmount>> listSent;
            wtx.GetAmounts(txdb, listReceived, listSent, allFee, strSentAccount, filter);
            if (wtx.GetDepthInMainChain(txdb, bestBlockHash) >= nMinDepth &&
                wtx.GetBlocksToMaturity(txdb, bestBlockHash) == 0) {
                for (const PAIRTYPE(CTxDestination, CAmount) & r : listReceived)
                    nBalance += r.second;
            }
            for (const PAIRTYPE(CTxDestination, CAmount) & r : listSent)
                nBalance -= r.second;
            nBalance -= allFee;
        }
        return ValueFromAmount(nBalance);
    }

    accountingDeprecationCheck();

    string strAccount = AccountFromValue(params[0]);

    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, filter, txdb);

    return ValueFromAmount(nBalance);
}

Value getunconfirmedbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw std::runtime_error("getunconfirmedbalance\n"
                                 "Returns the server's total unconfirmed balance\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ValueFromAmount(pwalletMain->GetUnconfirmedBalance(CTxDB()));
}

Value getntp1balances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("getntp1balances [minconf=1]\n");

    boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();

    int nMinDepth = 0;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    ntp1wallet->setRetrieveFullMetadata(false);
    ntp1wallet->setMinMaxConfirmations(nMinDepth);
    ntp1wallet->update();

    int tokenCount = ntp1wallet->getNumberOfTokens();

    json_spirit::Object root;

    for (int i = 0; i < tokenCount; i++) {
        std::string tokenId   = ntp1wallet->getTokenID(i);
        std::string tokenName = ntp1wallet->getTokenName(tokenId);
        NTP1Int     balance   = ntp1wallet->getTokenBalance(tokenId);

        json_spirit::Object tokenJsonData;
        tokenJsonData.push_back(json_spirit::Pair("Name", tokenName));
        tokenJsonData.push_back(json_spirit::Pair("TokenId", tokenId));
        tokenJsonData.push_back(json_spirit::Pair("Balance", ToString(balance)));

        root.push_back(json_spirit::Pair(tokenId, tokenJsonData));
    }

    return json_spirit::Value(root);
}

Value getntp1balance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("getntp1balance <tokenId/name> [minconf=1]\n");

    boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();

    int    nMinDepth = 0;
    string requestedToken;
    string requestedTokenLowerCase;
    if (params.size() > 0)
        requestedToken = params[0].get_str();
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    std::transform(requestedToken.cbegin(), requestedToken.cend(),
                   std::back_inserter(requestedTokenLowerCase), ::tolower);

    ntp1wallet->setRetrieveFullMetadata(false);
    ntp1wallet->setMinMaxConfirmations(nMinDepth);
    ntp1wallet->update();

    int tokenCount = ntp1wallet->getNumberOfTokens();

    json_spirit::Object root;

    for (int i = 0; i < tokenCount; i++) {
        std::string tokenId   = ntp1wallet->getTokenID(i);
        std::string tokenName = ntp1wallet->getTokenName(tokenId);
        std::transform(tokenName.begin(), tokenName.end(), tokenName.begin(), ::tolower);
        if (tokenId != requestedToken && tokenName != requestedTokenLowerCase) {
            continue;
        }

        NTP1Int balance = ntp1wallet->getTokenBalance(tokenId);

        return json_spirit::Value(ToString(balance));
    }

    return json_spirit::Value(root);
}

Value abandontransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "abandontransaction \"txid\"\n"
            "\nMark in-wallet transaction <txid> as abandoned\n"
            "This will mark this transaction and all its in-wallet descendants as abandoned which will "
            "allow\n"
            "for their inputs to be respent.  It can be used to replace \"stuck\" or evicted "
            "transactions.\n"
            "It only works on transactions which are not included in a block and are not currently in "
            "the mempool.\n"
            "It has no effect on transactions which are already conflicted or abandoned.\n"
            "\nArguments:\n"
            "1. \"txid\" (string, required) The transaction id\n"
            "\nResult:\n"
            "\nExamples:\n"
            "abandontransaction "
            "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"");

    EnsureWalletIsUnlocked();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());

    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    if (!pwalletMain->AbandonTransaction(CTxDB(), hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not eligible for abandonment");

    return Value();
}

Value movecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error("move <fromaccount> <toaccount> <amount> [minconf=1] [comment]\n"
                            "Move from one account in your wallet to another.");

    accountingDeprecationCheck();

    string  strFrom = AccountFromValue(params[0]);
    string  strTo   = AccountFromValue(params[1]);
    CAmount nAmount = AmountFromValue(params[2]);

    if (params.size() > 3)
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)params[3].get_int();
    string strComment;
    if (params.size() > 4)
        strComment = params[4].get_str();

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.TxnBegin())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos       = pwalletMain->IncOrderPosNext(&walletdb);
    debit.strAccount      = strFrom;
    debit.nCreditDebit    = -nAmount;
    debit.nTime           = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment      = strComment;
    pwalletMain->AddAccountingEntry(debit, walletdb);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos       = pwalletMain->IncOrderPosNext(&walletdb);
    credit.strAccount      = strTo;
    credit.nCreditDebit    = nAmount;
    credit.nTime           = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment      = strComment;
    pwalletMain->AddAccountingEntry(credit, walletdb);

    if (!walletdb.TxnCommit())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    return true;
}

Value sendfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error(
            "sendfrom <fromaccount> <toneblioaddress> <amount> [minconf=1] [comment] [comment-to]\n"
            "<amount> is a real and is rounded to the nearest 0.000001" +
            HelpRequiringPassphrase());

    string          strAccount = AccountFromValue(params[0]);
    CBitcoinAddress address(params[1].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");
    CAmount nAmount = AmountFromValue(params[2]);

    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();

    const CTxDB txdb;

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["to"] = params[5].get_str();

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(
        strAccount, nMinDepth, static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE_ALL), txdb);
    if (nAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    // Send
    string strError = pwalletMain->SendMoneyToDestination(txdb, address.Get(), nAmount, wtx);
    if (strError != "")
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}

Value sendmany(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendmany <fromaccount (must be empty, unsupported)> {address:amount,...} or \n"
            "sendmany <fromaccount (must be empty, unsupported)> [{address:amount},...]\n"
            "[comment]\n"
            "amounts are double-precision floating point numbers" +
            HelpRequiringPassphrase());

    string strAccount = params[0].get_str();
    if (!strAccount.empty()) {
        throw std::runtime_error("Accounts are not supported anymore. The account field must be empty");
    }
    Object sendTo = params[1].get_obj();

    const CTxDB txdb;

    // Get NTP1 wallet
    boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();
    ntp1wallet->setRetrieveFullMetadata(false);
    ntp1wallet->update();

    CWalletTx wtx;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();

    vector<pair<CScript, CAmount>> vecSend;

    CAmount totalAmount = 0;
    for (const Pair& s : sendTo) {
        CBitcoinAddress address(s.name_);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid neblio address: ") + s.name_);

        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());
        CAmount nAmount = AmountFromValue(s.value_);

        totalAmount += nAmount;

        vecSend.push_back(make_pair(scriptPubKey, nAmount));
    }

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = pwalletMain->GetBalance(txdb);
    if (totalAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    std::vector<NTP1SendTokensOneRecipientData> ntp1recipients =
        GetNTP1RecipientsVector(sendTo, ntp1wallet);

    // initial selection of NTP1 tokens
    NTP1SendTxData tokenSelector;
    tokenSelector.selectNTP1Tokens(ntp1wallet, std::vector<COutPoint>(), ntp1recipients, false);

    // Send
    CReserveKey keyChange(pwalletMain.get());
    CAmount     nFeeRequired = 0;

    bool fCreated =
        pwalletMain->CreateTransaction(txdb, vecSend, wtx, keyChange, nFeeRequired, tokenSelector);
    if (!fCreated) {
        if (totalAmount + nFeeRequired > pwalletMain->GetBalance(txdb))
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction creation failed");
    }

    // verify the NTP1 transaction before commiting
    try {
        std::vector<std::pair<CTransaction, NTP1Transaction>> inputsTxs =
            NTP1Transaction::GetAllNTP1InputsOfTx(wtx, txdb, false);
        NTP1Transaction ntp1tx;
        ntp1tx.readNTP1DataFromTx(CTxDB(), wtx, inputsTxs);
    } catch (std::exception& ex) {
        NLog.write(b_sev::info, "An invalid NTP1 transaction was created; an exception was thrown: {}",
                   ex.what());
        throw std::runtime_error(
            "Unable to create the transaction. The transaction created would result in an invalid "
            "transaction. Please report your transaction details to the Neblio team. The "
            "error is: " +
            std::string(ex.what()));
    }
    if (!pwalletMain->CommitTransaction(wtx, CTxDB(), keyChange))
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");

    return wtx.GetHash().GetHex();
}

Value addmultisigaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3) {
        string msg = "addmultisigaddress <nrequired> <'[\"key\",\"key\"]'> [account]\n"
                     "Add a nrequired-to-sign multisignature address to the wallet\"\n"
                     "each key is a neblio address or hex-encoded public key\n"
                     "If [account] is specified, assign address to [account].";
        throw runtime_error(msg);
    }

    int          nRequired = params[0].get_int();
    const Array& keys      = params[1].get_array();
    string       strAccount;
    if (params.size() > 2)
        strAccount = AccountFromValue(params[2]);

    // Gather public keys
    if (nRequired < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if ((int)keys.size() < nRequired)
        throw runtime_error(fmt::format("not enough keys supplied "
                                        "(got {} keys, but need at least {} to redeem)",
                                        keys.size(), nRequired));
    std::vector<CKey> pubkeys;
    pubkeys.resize(keys.size());
    for (unsigned int i = 0; i < keys.size(); i++) {
        const std::string& ks = keys[i].get_str();

        // Case 1: Bitcoin address and we have full public key:
        CBitcoinAddress address(ks);
        if (address.IsValid()) {
            CKeyID keyID;
            if (!address.GetKeyID(keyID))
                throw runtime_error(fmt::format("{} does not refer to a key", ks));
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                throw runtime_error(fmt::format("no full public key for address {}", ks));
            if (!vchPubKey.IsValid() || !pubkeys[i].SetPubKey(vchPubKey))
                throw runtime_error(" Invalid public key: " + ks);
        }

        // Case 2: hex public key
        else if (IsHex(ks)) {
            CPubKey vchPubKey(ParseHex(ks));
            if (!vchPubKey.IsValid() || !pubkeys[i].SetPubKey(vchPubKey))
                throw runtime_error(" Invalid public key: " + ks);
        } else {
            throw runtime_error(" Invalid public key: " + ks);
        }
    }

    // Construct using pay-to-script-hash:
    CScript inner;
    inner.SetMultisig(nRequired, pubkeys);
    CScriptID innerID = inner.GetID();
    if (!pwalletMain->AddCScript(inner))
        throw runtime_error("AddCScript() failed");

    pwalletMain->SetAddressBookEntry(innerID, strAccount);
    return CBitcoinAddress(innerID).ToString();
}

Value addredeemscript(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2) {
        string msg = "addredeemscript <redeemScript> [account]\n"
                     "Add a P2SH address with a specified redeemScript to the wallet.\n"
                     "If [account] is specified, assign address to [account].";
        throw runtime_error(msg);
    }

    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Construct using pay-to-script-hash:
    vector<unsigned char> innerData = ParseHexV(params[0], "redeemScript");
    CScript               inner(innerData.begin(), innerData.end());
    CScriptID             innerID = inner.GetID();
    if (!pwalletMain->AddCScript(inner))
        throw runtime_error("AddCScript() failed");

    pwalletMain->SetAddressBookEntry(innerID, strAccount);
    return CBitcoinAddress(innerID).ToString();
}

struct tallyitem
{
    CAmount              nAmount;
    int                  nConf;
    std::vector<uint256> txids;
    tallyitem()
    {
        nAmount = 0;
        nConf   = std::numeric_limits<int>::max();
    }
};

Value ListReceived(const Array& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    isminefilter filter = static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE_ALL);
    if (params.size() > 2)
        if (params[2].get_bool())
            filter = filter | static_cast<isminefilter>(isminetype::ISMINE_WATCH_ONLY);

    const CTxDB txdb;

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    // Tally
    map<CBitcoinAddress, tallyitem> mapTally;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
         it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;

        if (wtx.IsCoinBase() || wtx.IsCoinStake() || !IsFinalTx(wtx, txdb))
            continue;

        int nDepth = wtx.GetDepthInMainChain(txdb, bestBlockHash);
        if (nDepth < nMinDepth)
            continue;

        for (const CTxOut& txout : wtx.vout) {
            CTxDestination address;
            if (!ExtractDestination(txdb, txout.scriptPubKey, address) ||
                !IsMineCheck(IsMine(*pwalletMain, address), static_cast<isminetype>(filter)))
                continue;

            tallyitem& item = mapTally[address];
            item.nAmount += txout.nValue;
            item.nConf = min(item.nConf, nDepth);
            item.txids.push_back(wtx.GetHash());
        }
    }

    // Reply
    Array                  ret;
    map<string, tallyitem> mapAccountTally;
    const auto             addrBook = pwalletMain->mapAddressBook.getInternalMap();
    for (const auto& item : addrBook) {
        const CBitcoinAddress&                    address    = item.first;
        const string&                             strAccount = item.second.name;
        map<CBitcoinAddress, tallyitem>::iterator it         = mapTally.find(address);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        CAmount nAmount = 0;
        int     nConf   = std::numeric_limits<int>::max();
        if (it != mapTally.end()) {
            nAmount = (*it).second.nAmount;
            nConf   = (*it).second.nConf;
        }

        if (fByAccounts) {
            tallyitem& item = mapAccountTally[strAccount];
            item.nAmount += nAmount;
            item.nConf = min(item.nConf, nConf);
        } else {
            Object obj;
            obj.push_back(Pair("address", address.ToString()));
            obj.push_back(Pair("account", strAccount));
            obj.push_back(Pair("amount", ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            json_spirit::Array transactions;
            if (it != mapTally.end()) {
                for (const uint256& _item : (*it).second.txids) {
                    transactions.push_back(_item.GetHex());
                }
            }
            obj.push_back(Pair("txids", transactions));
            ret.push_back(obj);
        }
    }

    if (fByAccounts) {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end();
             ++it) {
            CAmount nAmount = (*it).second.nAmount;
            int     nConf   = (*it).second.nConf;
            Object  obj;
            obj.push_back(Pair("account", (*it).first));
            obj.push_back(Pair("amount", ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

Value listreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listreceivedbyaddress [minconf=1] [includeempty=false] [includeWatchonly=false]\n"
            "[minconf] is the minimum number of confirmations before payments are included.\n"
            "[includeempty] whether to include addresses that haven't received any payments.\n"
            "Returns an array of objects containing:\n"
            "  \"address\" : receiving address\n"
            "  \"account\" : the account of the receiving address\n"
            "  \"amount\" : total amount received by the address\n"
            "  \"confirmations\" : number of confirmations of the most recent transaction included");

    return ListReceived(params, false);
}

Value listreceivedbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listreceivedbyaccount [minconf=1] [includeempty=false] [includeWatchonly=false]\n"
            "[minconf] is the minimum number of confirmations before payments are included.\n"
            "[includeempty] whether to include accounts that haven't received any payments.\n"
            "Returns an array of objects containing:\n"
            "  \"account\" : the account of the receiving addresses\n"
            "  \"amount\" : total amount received by addresses with this account\n"
            "  \"confirmations\" : number of confirmations of the most recent transaction included");

    accountingDeprecationCheck();

    return ListReceived(params, true);
}

static void MaybePushAddress(Object& entry, const CTxDestination& dest)
{
    CBitcoinAddress addr;
    if (addr.Set(dest))
        entry.push_back(Pair("address", addr.ToString()));
}

void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong,
                      const isminefilter& filter, Array& ret)
{
    CAmount                             nFee;
    string                              strSentAccount;
    list<pair<CTxDestination, CAmount>> listReceived;
    list<pair<CTxDestination, CAmount>> listSent;

    const CTxDB txdb;

    wtx.GetAmounts(txdb, listReceived, listSent, nFee, strSentAccount, filter);

    bool fAllAccounts = (strAccount == string("*"));

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    // Sent
    if ((!wtx.IsCoinStake()) && (!listSent.empty() || nFee != 0) &&
        (fAllAccounts || strAccount == strSentAccount)) {
        for (const PAIRTYPE(CTxDestination, CAmount) & s : listSent) {
            Object entry;
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.first);
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.second)));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            entry.push_back(Pair("blockheight", txdb.GetBestChainHeight().value_or(0) -
                                                    wtx.GetDepthInMainChain(txdb, bestBlockHash)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            ret.push_back(entry);
        }
    }

    // Received
    int depthInMainChain = wtx.GetDepthInMainChain(txdb, bestBlockHash);
    if (listReceived.size() > 0 && depthInMainChain >= nMinDepth) {
        bool stop = false;
        for (const PAIRTYPE(CTxDestination, CAmount) & r : listReceived) {
            string account;
            if (const auto entry = pwalletMain->mapAddressBook.get(r.first))
                account = entry->name;
            if (fAllAccounts || (account == strAccount)) {
                Object entry;
                entry.push_back(Pair("account", account));
                MaybePushAddress(entry, r.first);
                if (wtx.IsCoinBase() || wtx.IsCoinStake()) {
                    if (wtx.GetDepthInMainChain(txdb, bestBlockHash) < 1)
                        entry.push_back(Pair("category", "orphan"));
                    else if (wtx.GetBlocksToMaturity(txdb, bestBlockHash) > 0)
                        entry.push_back(Pair("category", "immature"));
                    else
                        entry.push_back(Pair("category", "generate"));
                } else {
                    entry.push_back(Pair("category", "receive"));
                }
                if (!wtx.IsCoinStake())
                    entry.push_back(Pair("amount", ValueFromAmount(r.second)));
                else {
                    entry.push_back(Pair("amount", ValueFromAmount(-nFee)));
                    stop = true; // only one coinstake output
                }
                entry.push_back(Pair("blockheight", 1 + txdb.GetBestChainHeight().value_or(0) -
                                                        wtx.GetDepthInMainChain(txdb, bestBlockHash)));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
            if (stop)
                break;
        }
    }
}

void AcentryToJSON(const CAccountingEntry& acentry, const string& strAccount, Array& ret)
{
    bool fAllAccounts = (strAccount == string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount) {
        Object entry;
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", (int64_t)acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}

Value listtransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error("listtransactions [account] [count=10] [from=0] [includeWatchonly=false] "
                            "[includeDelegated=true] [includeCold=true]\n"
                            "Returns up to [count] most recent transactions skipping the first [from] "
                            "transactions for account [account].");

    string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();

    isminefilter filter = static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE);
    if (params.size() > 3 && params[3].get_bool())
        filter = filter | static_cast<isminefilter>(isminetype::ISMINE_WATCH_ONLY);
    if (!(params.size() > 4) || params[4].get_bool())
        filter = filter | static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE_DELEGATED);
    if (!(params.size() > 5) || params[5].get_bool())
        filter = filter | static_cast<isminefilter>(isminetype::ISMINE_COLD);

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    Array ret;

    std::list<CAccountingEntry> acentries;
    CWallet::TxItems            txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
        CWalletTx* const pwtx = (*it).second.first;
        if (pwtx != 0)
            ListTransactions(*pwtx, strAccount, 0, true, filter, ret);
        CAccountingEntry* const pacentry = (*it).second.second;
        if (pacentry != 0)
            AcentryToJSON(*pacentry, strAccount, ret);

        if ((int)ret.size() >= (nCount + nFrom))
            break;
    }
    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;
    Array::iterator first = ret.begin();
    std::advance(first, nFrom);
    Array::iterator last = ret.begin();
    std::advance(last, nFrom + nCount);

    if (last != ret.end())
        ret.erase(last, ret.end());
    if (first != ret.begin())
        ret.erase(ret.begin(), first);

    std::reverse(ret.begin(), ret.end()); // Return oldest to newest

    return ret;
}

Value listaccounts(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "listaccounts [minconf=1] [includeWatchonly=false]\n"
            "Returns Object that has account names as keys, account balances as values.");

    accountingDeprecationCheck();

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    isminefilter includeWatchonly = static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE);
    if (params.size() > 1 && params[1].get_bool())
        includeWatchonly = includeWatchonly | static_cast<isminefilter>(isminetype::ISMINE_WATCH_ONLY);

    map<string, CAmount> mapAccountBalances;
    const auto           addrBook = pwalletMain->mapAddressBook.getInternalMap();
    for (const auto& entry : addrBook) {
        if (IsMineCheck(IsMine(*pwalletMain, entry.first),
                        static_cast<isminetype>(includeWatchonly))) // This address belongs to me
            mapAccountBalances[entry.second.name] = 0;
    }

    const CTxDB txdb;

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
         it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx&                    wtx = (*it).second;
        CAmount                             nFee;
        string                              strSentAccount;
        list<pair<CTxDestination, CAmount>> listReceived;
        list<pair<CTxDestination, CAmount>> listSent;
        bool                                fConflicted = false;
        int nDepth = wtx.GetDepthAndMempool(fConflicted, txdb, bestBlockHash);
        if (wtx.GetBlocksToMaturity(txdb, bestBlockHash) > 0 || nDepth < 0 || fConflicted)
            continue;
        wtx.GetAmounts(txdb, listReceived, listSent, nFee, strSentAccount, includeWatchonly);
        mapAccountBalances[strSentAccount] -= nFee;
        for (const PAIRTYPE(CTxDestination, CAmount) & s : listSent)
            mapAccountBalances[strSentAccount] -= s.second;
        if (nDepth >= nMinDepth && wtx.GetBlocksToMaturity(txdb, bestBlockHash) == 0) {
            for (const PAIRTYPE(CTxDestination, CAmount) & r : listReceived)
                if (const auto en = pwalletMain->mapAddressBook.get(r.first))
                    mapAccountBalances[en->name] += r.second;
                else
                    mapAccountBalances[""] += r.second;
        }
    }

    list<CAccountingEntry> acentries;
    CWalletDB(pwalletMain->strWalletFile).ListAccountCreditDebit("*", acentries);
    for (const CAccountingEntry& entry : acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    Object ret;
    for (const PAIRTYPE(const string, CAmount) & accountBalance : mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

Value listsinceblock(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "listsinceblock [blockhash] [target-confirmations] [include-removed=true] "
            "[include-watchonly=false]\n"
            "Get all transactions in blocks since block [blockhash], or all transactions if omitted. If "
            "include-removed is true, transactions in orphans will be included.");

    LOCK(cs_main);

    const CTxDB txdb;

    boost::optional<CBlockIndex> index;
    int                          target_confirms = 1;
    isminefilter                 filter = static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE_ALL) |
                          static_cast<isminefilter>(isminetype::ISMINE_COLD);

    std::vector<uint256> nonMainChain;

    if (params.size() > 0 && !params[0].get_str().empty()) {
        uint256 blockId = 0;

        blockId.SetHex(params[0].get_str());
        boost::optional<CBlockIndex> index = txdb.ReadBlockIndex(blockId);
        if (!index) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        // find the common ancestor if this block is not in mainchain
        while (index && !index->IsInMainChain(txdb) && index->getPrev(txdb)) {
            nonMainChain.push_back(index->blockHash);
            index = index->getPrev(txdb);
        }
    }

    if (params.size() > 1) {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
    }

    int depth = index ? (1 + txdb.GetBestChainHeight().value_or(0) - index->nHeight) : -1;

    Array transactions;
    Array removed;

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
         it != pwalletMain->mapWallet.end(); it++) {
        CWalletTx tx = (*it).second;

        if (depth == -1 || tx.GetDepthInMainChain(txdb, bestBlockHash) < depth)
            ListTransactions(tx, "*", 0, true, filter, transactions);
    }

    bool includeRemoved = true;
    if (params.size() > 2) {
        includeRemoved = params[2].get_bool();
    }

    if (includeRemoved) {
        for (const uint256& h : nonMainChain) {
            CBlock block;
            if (txdb.ReadBlock(h, block, true)) {
                for (const CTransaction& tx : block.vtx) {
                    auto it = pwalletMain->mapWallet.find(tx.GetHash());
                    if (it != pwalletMain->mapWallet.cend()) {
                        // We want all transactions regardless of confirmation count to appear here,
                        // even negative confirmation ones, hence the big negative.
                        ListTransactions(it->second, "*", -100000000, true, filter, removed);
                    }
                }
            }
        }
    }

    uint256 lastblock;

    if (target_confirms == 1) {
        lastblock = txdb.GetBestBlockHash();
    } else {
        int target_height = txdb.GetBestChainHeight().value_or(0) + 1 - target_confirms;

        boost::optional<CBlockIndex> block;
        for (block = txdb.GetBestBlockIndex(); block && block->nHeight > target_height;) {
            block = block->getPrev(txdb);
        }

        lastblock = block ? block->GetBlockHash() : 0;
    }

    Object ret;
    ret.push_back(Pair("transactions", transactions));
    if (includeRemoved) {
        ret.push_back(Pair("removed", removed));
    }
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;
}

Value gettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "gettransaction <txid> [ignoreNTP1=false] [includeWatchonly=false]\n"
            "Get detailed information about <txid>. Not ignoring NTP1 will try to retireve "
            "NTP1 data from the database. This won't work if the transaction is not in the blockchain.");

    uint256 hash;
    hash.SetHex(params[0].get_str());

    const CTxDB txdb;

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    Object entry;

    bool fIgnoreNTP1 = false;
    if (params.size() > 1)
        fIgnoreNTP1 = params[1].get_bool();

    isminefilter filter = static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE_ALL) |
                          static_cast<isminefilter>(isminetype::ISMINE_COLD);
    if (params.size() > 2 && params[2].get_bool())
        filter = filter | static_cast<isminefilter>(isminetype::ISMINE_WATCH_ONLY);

    if (pwalletMain->mapWallet.count(hash)) {
        const CWalletTx& wtx = pwalletMain->mapWallet[hash];

        TxToJSON(wtx, 0, entry, fIgnoreNTP1);

        CAmount nCredit = wtx.GetCredit(bestBlockHash, txdb, filter);
        CAmount nDebit  = wtx.GetDebit(filter);
        CAmount nNet    = nCredit - nDebit;
        CAmount nFee    = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

        entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
        if (wtx.IsFromMe(filter))
            entry.push_back(Pair("fee", ValueFromAmount(nFee)));

        WalletTxToJSON(wtx, entry);

        Array details;
        ListTransactions(pwalletMain->mapWallet[hash], "*", 0, false, filter, details);
        entry.push_back(Pair("details", details));

        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << wtx;
        std::string txHex = HexStr(ssTx.begin(), ssTx.end());
        entry.push_back(Pair("hex", txHex));
    } else {
        CTransaction tx;
        uint256      hashBlock = 0;
        if (GetTransaction(hash, tx, hashBlock)) {
            TxToJSON(tx, 0, entry, fIgnoreNTP1);
            if (hashBlock == 0)
                entry.push_back(Pair("confirmations", 0));
            else {
                entry.push_back(Pair("blockhash", hashBlock.GetHex()));
                const auto bi = txdb.ReadBlockIndex(hashBlock);
                if (bi) {
                    if (bi->IsInMainChain(txdb))
                        entry.push_back(Pair("confirmations",
                                             1 + txdb.GetBestChainHeight().value_or(0) - bi->nHeight));
                    else
                        entry.push_back(Pair("confirmations", 0));
                }
            }
        } else
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
    }

    return entry;
}

Value backupwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("backupwallet <destination>\n"
                            "Safely copies wallet.dat to destination, which can be a directory or a "
                            "path with filename.");

    string strDest = params[0].get_str();
    if (!BackupWallet(*pwalletMain, strDest))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet backup failed!");

    return Value::null;
}

Value keypoolrefill(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("keypoolrefill [new-size]\n"
                            "Fills the keypool." +
                            HelpRequiringPassphrase());

    unsigned int nSize = max(GetArg("-keypool", 100), (int64_t)0);
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size");
        nSize = (unsigned int)params[0].get_int();
    }

    EnsureWalletIsUnlocked();

    pwalletMain->TopUpKeyPool(nSize);

    if (pwalletMain->GetKeyPoolSize() < nSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return Value::null;
}

Value getwalletinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getwalletinfo\n"
            "Returns an object containing various wallet state info.\n"

            "\nResult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,                  (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,                      (numeric) the total nebl balance of the "
            "wallet "
            "(cold balance excluded)\n"
            //            "  \"delegated_balance\": xxxxx,              (numeric) the neblio balance held
            //            in P2CS (cold " "staking) contracts\n" "  \"cold_staking_balance\": xx,
            //            (numeric) the neblio balance held in cold " "staking addresses\n"
            "  \"unconfirmed_balance\": xxx,              (numeric) the total unconfirmed balance of "
            "the wallet in nebls\n"
            "  \"immature_delegated_balance\": xxxxxx,    (numeric) the delegated immature balance of "
            "the wallet in nebls\n"
            "  \"immature_cold_staking_balance\": xxxxxx, (numeric) the cold-staking immature balance "
            "of the wallet in nebls\n"
            //            "  \"immature_balance\": xxxxxx,              (numeric) the total immature
            //            balance of the " "wallet in nebls\n"
            "  \"txcount\": xxxxxxx,                      (numeric) the total number of transactions in "
            "the wallet\n"
            "  \"keypoololdest\": xxxxxx,                 (numeric) the timestamp (seconds since GMT "
            "epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,                     (numeric) how many new keys are "
            "pre-generated\n"
            "  \"unlocked_until\": ttt,                   (numeric) the timestamp in seconds since "
            "epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the "
            "wallet is locked\n"
            //            "  \"paytxfee\": x.xxxx,                      (numeric) the transaction fee
            //            configuration, " "set in nebl/kB\n"
            "}\n"
            "\nExamples:\n"
            "getwalletinfo");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const CTxDB txdb;

    json_spirit::Object obj;
    obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    obj.push_back(Pair("balance", ValueFromAmount(pwalletMain->GetBalance(txdb))));
    obj.push_back(Pair("delegated_balance", ValueFromAmount(pwalletMain->GetDelegatedBalance(txdb))));
    obj.push_back(
        Pair("cold_staking_balance", ValueFromAmount(pwalletMain->GetColdStakingBalance(txdb))));
    obj.push_back(
        Pair("unconfirmed_balance", ValueFromAmount(pwalletMain->GetUnconfirmedBalance(txdb))));
    obj.push_back(Pair("immature_balance", ValueFromAmount(pwalletMain->GetImmatureBalance(txdb))));
    obj.push_back(Pair("immature_delegated_balance",
                       ValueFromAmount(pwalletMain->GetImmatureDelegatedBalance(txdb))));
    obj.push_back(Pair("immature_cold_staking_balance",
                       ValueFromAmount(pwalletMain->GetImmatureColdStakingBalance(txdb))));
    obj.push_back(Pair("txcount", (int)pwalletMain->mapWallet.size()));
    obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize", (int)pwalletMain->GetKeyPoolSize()));
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    //    obj.push_back(Pair("paytxfee", ValueFromAmount(payTxFee.GetFeePerK())));
    return obj;
}

void ThreadTopUpKeyPool(void* /*parg*/)
{
    // Make this thread recognisable as the key-topping-up thread
    RenameThread("neblio-key-top");

    pwalletMain->TopUpKeyPool();
}

void ThreadCleanWalletPassphrase(void* parg)
{
    // Make this thread recognisable as the wallet relocking thread
    RenameThread("neblio-lock-wa");

    int64_t nMyWakeTime = GetTimeMillis() + *((int64_t*)parg) * 1000;

    ENTER_CRITICAL_SECTION(cs_nWalletUnlockTime);

    if (nWalletUnlockTime == 0) {
        nWalletUnlockTime = nMyWakeTime;

        do {
            if (nWalletUnlockTime == 0)
                break;
            int64_t nToSleep = nWalletUnlockTime - GetTimeMillis();
            if (nToSleep <= 0)
                break;

            LEAVE_CRITICAL_SECTION(cs_nWalletUnlockTime);
            MilliSleep(nToSleep);
            ENTER_CRITICAL_SECTION(cs_nWalletUnlockTime);

        } while (1);

        if (nWalletUnlockTime) {
            nWalletUnlockTime = 0;
            pwalletMain->Lock();
        }
    } else {
        if (nWalletUnlockTime < nMyWakeTime)
            nWalletUnlockTime = nMyWakeTime;
    }

    LEAVE_CRITICAL_SECTION(cs_nWalletUnlockTime);

    delete (int64_t*)parg;
}

Value walletpassphrase(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() < 2 || params.size() > 3))
        throw runtime_error("walletpassphrase <passphrase> <timeout> [stakingonly]\n"
                            "Stores the wallet decryption key in memory for <timeout> seconds.\n"
                            "if [stakingonly] is true sending functions are disabled.");
    if (params.size() >= 2) {
        if (params[1].get_int64() > 99999999 || params[1].get_int64() < 0) {
            throw runtime_error("walletpassphrase <passphrase> <timeout> [stakingonly]\n"
                                "Maximum timeout value is 99999999 use timeout of 0 to never re-lock, "
                                "negative values not allowed");
        }
    }
    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(
            RPC_WALLET_WRONG_ENC_STATE,
            "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    if (!pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_ALREADY_UNLOCKED, "Error: Wallet is already unlocked, use "
                                                        "walletlock first if need to change unlock "
                                                        "settings.");
    if (params[1].get_int64() < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Error: Negative timeout or int64 overflow (range: 0-99999999).");

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() > 0) {
        if (!pwalletMain->Unlock(strWalletPass))
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT,
                               "Error: The wallet passphrase entered was incorrect.");
    } else
        throw runtime_error("walletpassphrase <passphrase> <timeout>\n"
                            "Stores the wallet decryption key in memory for <timeout> seconds.");

    NewThread(ThreadTopUpKeyPool, NULL);
    if (params.size() >= 2) {
        int64_t* pnSleepTime = new int64_t(params[1].get_int64());

        if (*pnSleepTime > static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
            *pnSleepTime = static_cast<int64_t>(std::numeric_limits<int32_t>::max());

        if (params[1].get_int64() > 0) {
            NewThread(ThreadCleanWalletPassphrase, pnSleepTime);
        }
    }

    // ppcoin: if user OS account compromised prevent trivial sendmoney commands
    if (params.size() > 2)
        fWalletUnlockStakingOnly = params[2].get_bool();
    else
        fWalletUnlockStakingOnly = false;

    return Value::null;
}

Value walletpassphrasechange(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error("walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
                            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");
    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(
            RPC_WALLET_WRONG_ENC_STATE,
            "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw runtime_error("walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
                            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT,
                           "Error: The wallet passphrase entered was incorrect.");

    return Value::null;
}

Value walletlock(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error(
            "walletlock\n"
            "Removes the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.");
    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE,
                           "Error: running with an unencrypted wallet, but walletlock was called.");

    {
        LOCK(cs_nWalletUnlockTime);
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return Value::null;
}

Value encryptwallet(const Array& params, bool fHelp)
{
    if (!pwalletMain->IsCrypted() && (fHelp || params.size() != 1))
        throw runtime_error("encryptwallet <passphrase>\n"
                            "Encrypts the wallet with <passphrase>.");
    if (fHelp)
        return true;
    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE,
                           "Error: running with an encrypted wallet, but encryptwallet was called.");

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw runtime_error("encryptwallet <passphrase>\n"
                            "Encrypts the wallet with <passphrase>.");

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; neblio server stopping, restart to run with encrypted wallet.  The "
           "keypool has been flushed, you need to make a new backup.";
}

class DescribeAddressVisitor : public boost::static_visitor<Object>
{
public:
    Object operator()(const CNoDestination& /*dest*/) const { return Object(); }

    Object operator()(const CKeyID& keyID) const
    {
        Object  obj;
        CPubKey vchPubKey;
        pwalletMain->GetPubKey(keyID, vchPubKey);
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("pubkey", HexStr(vchPubKey.Raw())));
        obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        return obj;
    }

    Object operator()(const CScriptID& scriptID) const
    {
        Object obj;
        obj.push_back(Pair("isscript", true));
        CScript subscript;
        pwalletMain->GetCScript(scriptID, subscript);
        std::vector<CTxDestination> addresses;
        txnouttype                  whichType;
        int                         nRequired;
        ExtractDestinations(CTxDB(), subscript, whichType, addresses, nRequired);
        obj.push_back(Pair("script", GetTxnOutputType(whichType)));
        obj.push_back(Pair("hex", HexStr(subscript.begin(), subscript.end())));
        Array a;
        for (const CTxDestination& addr : addresses)
            a.push_back(CBitcoinAddress(addr).ToString());
        obj.push_back(Pair("addresses", a));
        if (whichType == TX_MULTISIG)
            obj.push_back(Pair("sigsrequired", nRequired));
        return obj;
    }
};

Value validateaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("validateaddress <neblio-address>\n"
                            "Return information about <neblioaddress>.");

    CBitcoinAddress address(params[0].get_str());
    bool            isValid = address.IsValid();

    Object ret;
    ret.push_back(Pair("isvalid", isValid));
    if (isValid) {
        CTxDestination dest = address.Get();

        string currentAddress = address.ToString();
        ret.push_back(Pair("address", currentAddress));

        CScript scriptPubKey = GetScriptForDestination(dest);
        ret.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end())));

        isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : isminetype::ISMINE_NO;
        ret.push_back(Pair("ismine", IsMineCheck(mine, isminetype::ISMINE_SPENDABLE_ALL) ||
                                         IsMineCheck(mine, isminetype::ISMINE_COLD)));
        if (mine != isminetype::ISMINE_NO) {
            Object detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
            ret.insert(ret.end(), detail.begin(), detail.end());
        }
        if (const auto en = pwalletMain->mapAddressBook.get(dest))
            ret.push_back(Pair("account", en->name));
    }
    return ret;
}

Value validatepubkey(const Array& params, bool fHelp)
{
    if (fHelp || !params.size() || params.size() > 2)
        throw runtime_error("validatepubkey <nebliopubkey>\n"
                            "Return information about <nebliopubkey>.");

    std::vector<unsigned char> vchPubKey = ParseHex(params[0].get_str());
    CPubKey                    pubKey(vchPubKey);

    bool   isValid      = pubKey.IsValid();
    bool   isCompressed = pubKey.IsCompressed();
    CKeyID keyID        = pubKey.GetID();

    CBitcoinAddress address;
    address.Set(keyID);

    Object ret;
    ret.push_back(Pair("isvalid", isValid));
    if (isValid) {
        CTxDestination dest           = address.Get();
        string         currentAddress = address.ToString();
        ret.push_back(Pair("address", currentAddress));
        bool fMine = IsMineCheck(IsMine(*pwalletMain, dest), isminetype::ISMINE_SPENDABLE_ALL) ||
                     IsMineCheck(IsMine(*pwalletMain, dest), isminetype::ISMINE_COLD);
        ret.push_back(Pair("ismine", fMine));
        ret.push_back(Pair("iscompressed", isCompressed));
        if (fMine) {
            Object detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
            ret.insert(ret.end(), detail.begin(), detail.end());
        }
        if (const auto en = pwalletMain->mapAddressBook.get(dest))
            ret.push_back(Pair("account", en->name));
    }
    return ret;
}

// ppcoin: reserve balance from being staked for network protection
Value reservebalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("reservebalance [<reserve> [amount]]\n"
                            "<reserve> is true or false to turn balance reserve on or off.\n"
                            "<amount> is a real and rounded to cent.\n"
                            "Set reserve amount not participating in network protection.\n"
                            "If no parameters provided current setting is printed.\n");

    if (params.size() > 0) {
        bool fReserve = params[0].get_bool();
        if (fReserve) {
            if (params.size() == 1)
                throw runtime_error("must provide amount to reserve balance.\n");
            CAmount nAmount = AmountFromValue(params[1]);
            nAmount         = (nAmount / CENT) * CENT; // round to cent
            if (nAmount < 0)
                throw runtime_error("amount cannot be negative.\n");
            nReserveBalance = nAmount;
        } else {
            if (params.size() > 1)
                throw runtime_error("cannot specify amount to turn off reserve.\n");
            nReserveBalance = 0;
        }
    }

    Object result;
    result.push_back(Pair("reserve", (nReserveBalance > 0)));
    result.push_back(Pair("amount", ValueFromAmount(nReserveBalance)));
    return result;
}

// NovaCoin: resend unconfirmed wallet transactions
Value resendtx(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("resendtx\n"
                            "Re-send unconfirmed transactions.\n");

    ResendWalletTransactions(true);

    return Value::null;
}

// ppcoin: make a public-private key pair
Value makekeypair(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("makekeypair [prefix]\n"
                            "Make a public/private key pair.\n"
                            "[prefix] is optional preferred prefix for the public key.\n");

    string strPrefix = "";
    if (params.size() > 0)
        strPrefix = params[0].get_str();

    CKey key;
    key.MakeNewKey(false);

    CPrivKey vchPrivKey = key.GetPrivKey();
    Object   result;
    result.push_back(
        Pair("PrivateKey", HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end())));
    result.push_back(Pair("PublicKey", HexStr(key.GetPubKey().Raw())));
    return result;
}

Value listcoldutxos(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "listcoldutxos ( nonWhitelistedOnly )\n"
            "\nList P2CS unspent outputs received by this wallet as cold-staker-\n"

            "\nArguments:\n"
            "1. nonWhitelistedOnly   (boolean, optional, default=false) Whether to exclude P2CS from "
            "whitelisted delegators.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"true\",            (string) The transaction id of the P2CS utxo\n"
            "    \"txidn\" : \"accountname\",    (string) The output number of the P2CS utxo\n"
            "    \"amount\" : x.xxx,             (numeric) The amount of the P2CS utxo\n"
            "    \"confirmations\" : n           (numeric) The number of confirmations of the P2CS "
            "utxo\n"
            "    \"cold-staker\" : n             (string) The cold-staker address of the P2CS utxo\n"
            "    \"coin-owner\" : n              (string) The coin-owner address of the P2CS utxo\n"
            "    \"whitelisted\" : n             (string) \"true\"/\"false\" coin-owner in delegator "
            "whitelist\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "listcoldutxos\n"
            "listcoldutxos true");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    bool fExcludeWhitelisted = false;
    if (params.size() > 0)
        fExcludeWhitelisted = params[0].get_bool();
    Array results;

    const CTxDB txdb;

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    for (std::map<uint256, CWalletTx>::const_iterator it = pwalletMain->mapWallet.begin();
         it != pwalletMain->mapWallet.end(); ++it) {
        const uint256&   wtxid = it->first;
        const CWalletTx* pcoin = &(*it).second;
        if (!IsFinalTx(*pcoin, txdb) || !pcoin->IsTrusted(txdb, bestBlockHash))
            continue;

        // if this tx has no unspent P2CS outputs for us, skip it
        if (pcoin->GetColdStakingCredit(bestBlockHash, txdb) == 0 &&
            pcoin->GetStakeDelegationCredit(bestBlockHash, txdb) == 0)
            continue;

        for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
            const CTxOut& out  = pcoin->vout[i];
            isminetype    mine = pwalletMain->IsMine(out);
            if (!bool(mine & ISMINE_COLD) && !bool(mine & ISMINE_SPENDABLE_DELEGATED))
                continue;
            txnouttype                  type;
            std::vector<CTxDestination> addresses;
            int                         nRequired;
            if (!ExtractDestinations(CTxDB(), out.scriptPubKey, type, addresses, nRequired))
                continue;
            const bool fWhitelisted = pwalletMain->mapAddressBook.exists(addresses[1]) > 0;
            if (fExcludeWhitelisted && fWhitelisted)
                continue;
            Object entry;
            entry.push_back(Pair("txid", wtxid.GetHex()));
            entry.push_back(Pair("txidn", (int)i));
            entry.push_back(Pair("amount", ValueFromAmount(out.nValue)));
            entry.push_back(Pair("confirmations", pcoin->GetDepthInMainChain(txdb, bestBlockHash)));
            entry.push_back(Pair("cold-staker", CBitcoinAddress(addresses[0]).ToString()));
            entry.push_back(Pair("coin-owner", CBitcoinAddress(addresses[1]).ToString()));
            entry.push_back(Pair("whitelisted", fWhitelisted ? "true" : "false"));
            results.push_back(entry);
        }
    }

    return results;
}
