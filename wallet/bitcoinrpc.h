// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _BITCOINRPC_H_
#define _BITCOINRPC_H_ 1

#include <list>
#include <map>
#include <string>

class CBlockIndex;

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"

#include "checkpoints.h"
#include "util.h"

#include "ntp1/ntp1sendtokensonerecipientdata.h"
#include "ntp1/ntp1sendtxdata.h"

// HTTP status codes
enum HTTPStatusCode
{
    HTTP_OK                    = 200,
    HTTP_BAD_REQUEST           = 400,
    HTTP_UNAUTHORIZED          = 401,
    HTTP_FORBIDDEN             = 403,
    HTTP_NOT_FOUND             = 404,
    HTTP_INTERNAL_SERVER_ERROR = 500,
};
 
// Bitcoin RPC error codes
enum RPCErrorCode
{
    // Standard JSON-RPC 2.0 errors
    RPC_INVALID_REQUEST  = -32600,
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS   = -32602,
    RPC_INTERNAL_ERROR   = -32603,
    RPC_PARSE_ERROR      = -32700,

    // General application defined errors
    RPC_MISC_ERROR              = -1,  // std::exception thrown in command handling
    RPC_FORBIDDEN_BY_SAFE_MODE  = -2,  // Server is in safe mode, and command is not allowed in safe mode
    RPC_TYPE_ERROR              = -3,  // Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY  = -5,  // Invalid address or key
    RPC_OUT_OF_MEMORY           = -7,  // Ran out of memory during operation
    RPC_INVALID_PARAMETER       = -8,  // Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR          = -20, // Database error
    RPC_DESERIALIZATION_ERROR   = -22, // Error parsing or validating structure in raw format
    RPC_TX_AMEND_FAILED         = -23, // Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR            = -25, // General error during transaction or block submission
    RPC_VERIFY_REJECTED         = -26, //  Transaction or block was rejected by network rules
    RPC_VERIFY_ALREADY_IN_CHAIN = -27, // Transaction already in chain
    RPC_IN_WARMUP               = -28, // Client still warming up

    //! Aliases for backward compatibility
    RPC_TRANSACTION_ERROR            = RPC_VERIFY_ERROR,
    RPC_TRANSACTION_REJECTED         = RPC_VERIFY_REJECTED,
    RPC_TRANSACTION_ALREADY_IN_CHAIN = RPC_VERIFY_ALREADY_IN_CHAIN,

    // P2P client errors
    RPC_CLIENT_NOT_CONNECTED       = -9,  // Bitcoin is not connected
    RPC_CLIENT_IN_INITIAL_DOWNLOAD = -10, // Still downloading initial blocks

    // Wallet errors
    RPC_WALLET_ERROR                = -4,  // Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_INSUFFICIENT_FUNDS   = -6,  // Not enough funds in wallet or account
    RPC_WALLET_INVALID_ACCOUNT_NAME = -11, // Invalid account name
    RPC_WALLET_KEYPOOL_RAN_OUT      = -12, // Keypool ran out, call keypoolrefill first
    RPC_WALLET_UNLOCK_NEEDED        = -13, // Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_PASSPHRASE_INCORRECT = -14, // The wallet passphrase entered was incorrect
    RPC_WALLET_WRONG_ENC_STATE =
        -15, // Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    RPC_WALLET_ENCRYPTION_FAILED = -16, // Failed to encrypt the wallet
    RPC_WALLET_ALREADY_UNLOCKED  = -17, // Wallet is already unlocked

    RPC_CLIENT_NODE_ALREADY_ADDED = -23, //!< Node is already added
    RPC_CLIENT_NODE_NOT_ADDED     = -24, //!< Node has not been added before

    RPC_CLIENT_NODE_NOT_CONNECTED   = -29, //!< Node to disconnect not found in connected nodes
    RPC_CLIENT_INVALID_IP_OR_SUBNET = -30, //!< Invalid IP/Subnet
    RPC_CLIENT_P2P_DISABLED         = -31, //!< No valid connection manager instance found

};

extern boost::atomic_bool fRpcListening;

json_spirit::Object JSONRPCError(int code, const std::string& message);

void ThreadRPCServer();
int  CommandLineRPC(int argc, char* argv[]);

bool IsRPCRunning();

/** Convert parameter values for RPC call from strings to command-specific JSON objects. */
json_spirit::Array RPCConvertValues(const std::string&              strMethod,
                                    const std::vector<std::string>& strParams);

/*
  Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
  the right number of arguments are passed, just that any passed are the correct type.
  Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
*/
void RPCTypeCheck(const json_spirit::Array&                 params,
                  const std::list<json_spirit::Value_type>& typesExpected, bool fAllowNull = false);
/*
  Check for expected keys/value types in an Object.
  Use like: RPCTypeCheck(object, boost::assign::map_list_of("name", str_type)("value", int_type));
*/
void RPCTypeCheck(const json_spirit::Object&                            o,
                  const std::map<std::string, json_spirit::Value_type>& typesExpected,
                  bool                                                  fAllowNull = false);

typedef json_spirit::Value (*rpcfn_type)(const json_spirit::Array& params, bool fHelp);

class CRPCCommand
{
public:
    std::string name;
    rpcfn_type  actor;
    bool        okSafeMode;
    bool        unlocked;
};

/**
 * Bitcoin RPC command dispatcher.
 */
class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;

public:
    CRPCTable();
    const CRPCCommand* operator[](std::string name) const;
    std::string        help(std::string name) const;

    /**
     * Execute a method.
     * @param method   Method to execute
     * @param params   Array of arguments (JSON objects)
     * @returns Result of the call.
     * @throws an exception (json_spirit::Value) when an error happens.
     */
    json_spirit::Value execute(const std::string& method, const json_spirit::Array& params) const;

    /**
     * Returns a list of registered commands
     * @returns List of registered commands.
     */
    std::vector<std::string> listCommands() const;
};

extern const CRPCTable tableRPC;

extern boost::atomic<int64_t> nWalletUnlockTime;
extern CAmount                AmountFromValue(const json_spirit::Value& value);
NTP1Int                       NTP1AmountFromValue(const json_spirit::Value& value);
extern json_spirit::Value     ValueFromAmount(const CAmount& amount);
extern double                 GetDifficulty(const CBlockIndex* blockindex = NULL);

extern double GetPoWMHashPS();
extern double GetPoSKernelPS();

extern std::string HelpRequiringPassphrase();
extern void        EnsureWalletIsUnlocked();

//
// Utilities: convert hex-encoded Values
// (throws error if not hex).
//
extern uint256                    ParseHashV(const json_spirit::Value& v, const std::string& strName);
extern uint256                    ParseHashO(const json_spirit::Object& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const json_spirit::Value& v, std::string strName);
extern std::vector<unsigned char> ParseHexO(const json_spirit::Object& o, std::string strKey);

// in rpcnet.cpp
extern json_spirit::Value getconnectioncount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addnode(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value disconnectnode(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setmocktime(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getpeerinfo(const json_spirit::Array& params, bool fHelp);

// in rpcdump.cpp
extern json_spirit::Value dumpwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value dumpprivkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value dumppubkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importprivkey(const json_spirit::Array& params, bool fHelp);

// in rpcmining.cpp
extern json_spirit::Value getsubsidy(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getmininginfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakinginfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getwork(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getworkex(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblocktemplate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value submitblock(const json_spirit::Array& params, bool fHelp);

// in rpcwallet.cpp
extern json_spirit::Value getnewaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addledgeraddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value verifyledgeraddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaccountaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getledgeraccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressesbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendtoaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendntp1toaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value udtoneblioaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value signmessage(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value verifymessage(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listdelegators(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value delegatoradd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value delegatorremove(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststakingaddresses(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getdelegatedbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getcoldstakingbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getunconfirmedbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getntp1balances(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getntp1balance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value abandontransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value movecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendmany(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addmultisigaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addredeemscript(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listtransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaddressgroupings(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaccounts(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listsinceblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value backupwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value keypoolrefill(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getwalletinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrawchangeaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletpassphrase(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletpassphrasechange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletlock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value encryptwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value validateaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value reservebalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value checkwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value repairwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value resendtx(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value makekeypair(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value validatepubkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnewpubkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value delegatestake(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value rawdelegatestake(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listcoldutxos(const json_spirit::Array& params, bool fHelp);

// in rcprawtransaction.cpp
extern json_spirit::Value getrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decodentp1script(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createrawntp1transaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value issuenewntp1token(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decoderawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decodescript(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getscriptpubkeyfromaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getscriptpubkeyforp2cs(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value signrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendrawtransaction(const json_spirit::Array& params, bool fHelp);

// in rpcblockchain.cpp
extern json_spirit::Value getbestblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value syncwithvalidationinterfacequeue(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockchaininfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockheader(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value generatepos(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value generate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value generatetoaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value generateblockwithkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockcount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getdifficulty(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value settxfee(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrawmempool(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value calculateblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockbynumber(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettxout(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value exportblockchain(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value waitforblockheight(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listvotes(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value castvote(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value cancelallvotesofproposal(const json_spirit::Array& params, bool fHelp);

std::vector<NTP1SendTokensOneRecipientData>
     GetNTP1RecipientsVector(const json_spirit::Value& sendTo, boost::shared_ptr<NTP1Wallet> ntp1wallet,
                             bool getDataStrictlyFromNTP1Wallet = true);
void ScriptPubKeyToJSON(const CScript& scriptPubKey, json_spirit::Object& out, bool fIncludeHex);

#endif
