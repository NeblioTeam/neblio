#include "googletest/googletest/include/gtest/gtest.h"

#include <boost/algorithm/hex.hpp>
#include <string>
#include <vector>

#include "serialize.h"

TEST(serialize_tests, varints)
{
    // encode

    CDataStream            ss(SER_DISK, 0);
    CDataStream::size_type size = 0;
    for (int i = 0; i < 100000; i++) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        EXPECT_TRUE(size == ss.size());
    }

    for (uint64_t i = 0; i < 100000000000ULL; i += 999999937) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        EXPECT_TRUE(size == ss.size());
    }

    // decode
    for (int i = 0; i < 100000; i++) {
        int j = 0;
        ss >> VARINT(j);
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }

    for (uint64_t i = 0; i < 100000000000ULL; i += 999999937) {
        uint64_t j = 0;
        ss >> VARINT(j);
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }
}

    /**
     * Classes that use serialization:
     * CAddrInfo
    CAddrMan
    CUnsignedAlert
    CAlert
    CBloomFilter
    CUnsignedSyncCheckpoint
    CSyncCheckpoint
    CMasterKey
    CPubKey
    CDiskTxPos
    COutPoint
    CTxIn
    CTxOut
    CTransaction
    CTxOutCompressor
    CTxUndo
    CMerkleTx
    CTxIndex
    CPartialMerkleTree
    CBlock
    CDiskBlockIndex
    CBlockLocator
    CMerkleBlock
    CNetAddr
    CService
    NTP1OutPoint
    NTP1TokenTxData
    NTP1Transaction
    NTP1TxIn
    NTP1TxOut
    CMessageHeader
    CAddress
    CInv
    CKeyPool
    CWalletTx
    CWalletKey
    CAccount
    CAccountingEntry
    CKeyMetadata
    Accumulator
    AccumulatorProofOfKnowledge
    PublicCoin
    PrivateCoin
    CoinSpend
    Commitment
    CommitmentProofOfKnowledge
    IntegerGroupParams
    AccumulatorAndProofParams
    Params
    SerialNumberSignatureOfKnowledge
    SpendMetaData
     */

#include "addrman.h"
#include "alert.h"
#include "bloom.h"
#include "checkpoints.h"
#include "crypter.h"
#include "main.h"
#include "ntp1/ntp1outpoint.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1tokentxdata.h"
#include "ntp1/ntp1transaction.h"
#include "ntp1/ntp1txin.h"
#include "ntp1/ntp1txout.h"
#include "protocol.h"
#include "wallet.h"
#include "zerocoin/Accumulator.h"
#include "zerocoin/AccumulatorProofOfKnowledge.h"

TEST(serialize_tests, cross_platform_consistency)
{
    {
        char a = 0x12;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)1);
    }
    {
        short a = 0x1234;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)2);
    }
    {
        int a = 0x12345678;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)4);
    }
    {
        long a = 0x12345678;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)8);
    }
    {
        long long a = 0x1234567824681357;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)8);
    }
    {
        unsigned char a = 0x12;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)1);
    }
    {
        unsigned short a = 0x1234;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)2);
    }
    {
        unsigned int a = 0x12345678;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)4);
    }
    {
        unsigned long a = 0x12345678;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)8);
    }
    {
        unsigned long long a = 0x1234567824681357;
        EXPECT_EQ(GetSerializeSize(a, 0, 0), (unsigned)8);
    }

    {
        char        a = 0x12;
        CDataStream ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "12");
    }

    {
        short       a = 0x1234;
        CDataStream ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "3412");
    }

    {
        int         a = 0x12345678;
        CDataStream ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "78563412");
    }

    {
        long        a = 0x12345678;
        CDataStream ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "7856341200000000");
    }

    {
        long long   a = 0x1234567813572468;
        CDataStream ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "6824571378563412");
    }

    {
        unsigned char a = 0x12;
        CDataStream   ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "12");
    }

    {
        unsigned short a = 0x1234;
        CDataStream    ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "3412");
    }

    {
        unsigned int a = 0x12345678;
        CDataStream  ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "78563412");
    }

    {
        unsigned long a = 0x12345678;
        CDataStream   ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "7856341200000000");
    }

    {
        unsigned long long a = 0x1234567813572468;
        CDataStream        ss(SER_DISK, 0);
        ss << a;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "6824571378563412");
    }

    in_addr addr;
    addr.s_addr = 0x12345678;

    CNetAddr cNetAddr(addr);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cNetAddr;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "00000000000000000000FFFF78563412");
    }

    CService cService(cNetAddr, 0x1234);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cService;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "00000000000000000000FFFF785634121234");
    }

    CAddress cAddress(cService);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cAddress;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0000000000E1F505010000000000000000000000000000000000FFFF785634121234");
    }

    CAddrInfo cAddrInfo(cAddress, cNetAddr);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cAddrInfo;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "0000000000E1F505010000000000000000000000000000000000"
                                                   "FFFF78563412123400000000000000000000FFFF785634120000"
                                                   "00000000000000000000");
    }

    CUnsignedAlert cUnsignedAlert;
    cUnsignedAlert.nVersion     = 0x12345678;
    cUnsignedAlert.nRelayUntil  = 0x1234567824681357;
    cUnsignedAlert.nExpiration  = 0x1234567824681357;
    cUnsignedAlert.nID          = 0x12345678;
    cUnsignedAlert.nCancel      = 0x12345678;
    cUnsignedAlert.setCancel    = {0x12345678, 0x13572468};
    cUnsignedAlert.nMinVer      = 0x12345678;
    cUnsignedAlert.nMaxVer      = 0x12345678;
    cUnsignedAlert.setSubVer    = {"ABCDEFG", "XXYYZZ"};
    cUnsignedAlert.nPriority    = 0x12345678;
    cUnsignedAlert.strComment   = "ABCDEFG";
    cUnsignedAlert.strStatusBar = "ABCDEFG";
    cUnsignedAlert.strReserved  = "ABCDEFG";
    {
        CDataStream ss(SER_DISK, 0);
        ss << cUnsignedAlert;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "7856341257136824785634125713682478563412785634127856"
                                                   "3412027856341268245713785634127856341202074142434445"
                                                   "464706585859595A5A7856341207414243444546470741424344"
                                                   "4546470741424344454647");
    }

    CAlert cAlert;
    cAlert.vchMsg = {'a', 'b', 'c', 'd', 'e', 'f'};
    cAlert.vchSig = {'a', 'b', 'c', 'd', 'e', 'f'};
    {
        CDataStream ss(SER_DISK, 0);
        ss << cAlert;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "0661626364656606616263646566");
    }

    CBloomFilter cBloomFilter(0x7, 0.24133253664357, 0x3, 0x5);
    cBloomFilter.insert(uint256("12345678135724681122334455667788123456781357246812345678135724"));
    {
        CDataStream ss(SER_DISK, 0);
        ss << cBloomFilter;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), R"(020010010000000300000005)");
    }

    CUnsignedSyncCheckpoint cUnsignedSyncCheckpoint;
    cUnsignedSyncCheckpoint.nVersion = 0x12345678;
    cUnsignedSyncCheckpoint.hashCheckpoint =
        uint256("12345678135724681122334455667788123456781357246812345678135724");
    {
        CDataStream ss(SER_DISK, 0);
        ss << cUnsignedSyncCheckpoint;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "785634122457137856341268245713785634128877665544332211682457137856341200");
    }

    CSyncCheckpoint cSyncCheckpoint;
    cSyncCheckpoint.vchMsg = {'a', 'b', 'c', 'd', 'e', 'f'};
    cSyncCheckpoint.vchSig = {'a', 'b', 'c', 'd', 'e', 'f'};
    {
        CDataStream ss(SER_DISK, 0);
        ss << cSyncCheckpoint;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "0661626364656606616263646566");
    }

    CMasterKey cMasterKey;
    cMasterKey.vchCryptedKey                = {'a', 'b', 'c', 'd', 'e', 'f'};
    cMasterKey.vchSalt                      = {'a', 'b', 'c', 'd', 'e', 'f'};
    cMasterKey.nDerivationMethod            = 0x12345678;
    cMasterKey.nDeriveIterations            = 0x12345678;
    cMasterKey.vchOtherDerivationParameters = {'a', 'b', 'c', 'd', 'e', 'f'};
    {
        CDataStream ss(SER_DISK, 0);
        ss << cMasterKey;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0661626364656606616263646566785634127856341206616263646566");
    }

    CPubKey cPubKey({'a', 'b', 'c', 'd', 'e', 'f'});
    {
        CDataStream ss(SER_DISK, 0);
        ss << cPubKey;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "06616263646566");
    }

    CDiskTxPos cDiskTxPos(uint256("12345678135724681122334455667788123456781357246812345678135724"),
                          0x12345678);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cDiskTxPos;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "245713785634126824571378563412887766554433221168245713785634120078563412");
    }

    COutPoint cOutPoint;
    cOutPoint.hash = uint256("12345678135724681122334455667788123456781357246812345678135724");
    cOutPoint.n    = 0x12345678;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cOutPoint;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "245713785634126824571378563412887766554433221168245713785634120078563412");
    }

    std::vector<char> cScriptPubKey({'a', 'b', 'c', 'd', 'e', 'f'});

    CTxIn cTxIn;
    cTxIn.prevout = cOutPoint;
    std::copy(cScriptPubKey.cbegin(), cScriptPubKey.cend(), std::back_inserter(cTxIn.scriptSig));
    cTxIn.nSequence = 0x12345678;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cTxIn;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "2457137856341268245713785634128877665544332211682457"
                                                   "137856341200785634120661626364656678563412");
    }

    CTxOut cTxOut;
    cTxOut.nValue = 0x1234567813572468;
    std::copy(cScriptPubKey.cbegin(), cScriptPubKey.cend(), std::back_inserter(cTxOut.scriptPubKey));
    {
        CDataStream ss(SER_DISK, 0);
        ss << cTxOut;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "682457137856341206616263646566");
    }

    CTransaction cTransaction;
    cTransaction.nVersion = 0x12345678;
    cTransaction.nTime    = 0x12345678;
    cTransaction.vin.push_back(cTxIn);
    cTransaction.vout.push_back(cTxOut);
    cTransaction.nLockTime = 0x12345678;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cTransaction;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "7856341278563412012457137856341268245713785634128877"
                                                   "6655443322116824571378563412007856341206616263646566"
                                                   "785634120168245713785634120661626364656678563412");
    }

    CTxOutCompressor cTxOutCompressor(cTxOut);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cTxOutCompressor;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "80A2EAC1C689EFC08E210C616263646566");
    }

    CTxInUndo cTxInUndo;
    cTxInUndo.txout     = cTxOut;
    cTxInUndo.fCoinBase = 0;
    cTxInUndo.nHeight   = 0x12345678;
    cTxInUndo.nVersion  = 0x12345678;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cTxInUndo;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "81A2A1D8708090D0AB7880A2EAC1C689EFC08E210C616263646566");
    }

    CTxUndo cTxUndo;
    cTxUndo.vprevout.push_back(cTxInUndo);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cTxUndo;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0181A2A1D8708090D0AB7880A2EAC1C689EFC08E210C616263646566");
    }

    CMerkleTx cMerkleTx     = cTransaction;
    cMerkleTx.hashBlock     = uint256("12345678135724681122334455667788123456781357246812345678135724");
    cMerkleTx.vMerkleBranch = {
        uint256("12345678135724681122334455667788123456781357246812345678135724"),
        uint256("12345678135724681122424455667788123456781357246812343535135724"),
        uint256("12345678137654322242334455667788123456781357246812345678135724"),
        uint256("12345678135724687654567890987654321111234357246812345678135724")};
    cMerkleTx.nIndex          = 0x12345678;
    cMerkleTx.fMerkleVerified = 0;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cMerkleTx;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "7856341278563412012457137856341268245713785634128877665544332211682457137856341200785"
                  "6341206616263646566785634120168245713785634120661626364656678563412245713785634126824"
                  "5713785634128877665544332211682457137856341200042457137856341268245713785634128877665"
                  "5443322116824571378563412002457133535341268245713785634128877665544422211682457137856"
                  "3412002457137856341268245713785634128877665544334222325476137856341200245713785634126"
                  "824574323111132547698907856547668245713785634120078563412");
    }

    CTxIndex cTxIndex;
    cTxIndex.pos    = cDiskTxPos;
    cTxIndex.vSpent = {cDiskTxPos};
    {
        CDataStream ss(SER_DISK, 0);
        ss << cTxIndex;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "0000000024571378563412682457137856341288776655443322"
                                                   "1168245713785634120078563412012457137856341268245713"
                                                   "78563412887766554433221168245713785634120078563412");
    }

    CPartialMerkleTree cPartialMerkleTree(
        {uint256("12345678135724681122334455667788123456781357246812345678135724"),
         uint256("12345678135724681122424455667788123456781357246812343535135724"),
         uint256("12345678137654322242334455667788123456781357246812345678135724"),
         uint256("12345678135724687654567890987654321111234357246812345678135724")},
        {1, 1, 1, 0});
    {
        CDataStream ss(SER_DISK, 0);
        ss << cPartialMerkleTree;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0400000004245713785634126824571378563412887766554433221168245713785634120024571335353"
                  "4126824571378563412887766554442221168245713785634120024571378563412682457137856341288"
                  "7766554433422232547613785634120024571378563412682457432311113254769890785654766824571"
                  "37856341200013F");
    }

    CBlock cBlock;
    cBlock.nVersion       = 0x12345678;
    cBlock.hashPrevBlock  = uint256("12345678135724681122334455667788123456781357246812345678135724");
    cBlock.hashMerkleRoot = uint256("12345678135724681122424455667788123456781357246812343535135724");
    cBlock.nTime          = 0x12345678;
    cBlock.nBits          = 0x12345678;
    cBlock.nNonce         = 0x12345678;
    cBlock.vtx            = {cTransaction};
    cBlock.vchBlockSig    = {'a', 'b', 'c', 'd', 'e', 'f'};
    {
        CDataStream ss(SER_DISK, 0);
        ss << cBlock;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "7856341224571378563412682457137856341288776655443322116824571378563412002457133535341"
                  "2682457137856341288776655444222116824571378563412007856341278563412785634120178563412"
                  "7856341201245713785634126824571378563412887766554433221168245713785634120078563412066"
                  "1626364656678563412016824571378563412066162636465667856341206616263646566");
    }

    uint256 uint256v("12345678135724681122424455667788123456781357246812343535135724");

    CDiskBlockIndex cDiskBlockIndex;
    cDiskBlockIndex.phashBlock             = &uint256v;            // const uint256*
    cDiskBlockIndex.pprev                  = &cDiskBlockIndex;     // CBlockIndex*
    cDiskBlockIndex.pnext                  = &cDiskBlockIndex;     // CBlockIndex*
    cDiskBlockIndex.blockKeyInDB           = uint256v + 12345678;  // uint256
    cDiskBlockIndex.nChainTrust            = uint256v + 22233456;  // uint256
    cDiskBlockIndex.nHeight                = 0x12345678;           // int
    cDiskBlockIndex.nMint                  = 0x1234567813572468;   // int64_t
    cDiskBlockIndex.nMoneySupply           = 0x1234567813572468;   // int64_t
    cDiskBlockIndex.nFlags                 = 0x12345678;           // unsigned int
    cDiskBlockIndex.nStakeModifier         = 0x1234567813572468;   // uint64_t
    cDiskBlockIndex.nStakeModifierChecksum = 0x12345678;           // unsigned int
    cDiskBlockIndex.prevoutStake           = cOutPoint;            // COutPoint
    cDiskBlockIndex.nStakeTime             = 0x12345678;           // unsigned int
    cDiskBlockIndex.hashProof              = uint256v + 13131313;  // uint256
    cDiskBlockIndex.nVersion               = 0x12345679;           // int
    cDiskBlockIndex.hashMerkleRoot         = uint256v - 242536447; // uint256
    cDiskBlockIndex.nTime                  = 0x12345678;           // unsigned int
    cDiskBlockIndex.nBits                  = 0x12345622;           // unsigned int
    cDiskBlockIndex.nNonce                 = 0x12345655;           // unsigned int
    {
        CDataStream ss(SER_DISK, 0);
        ss << cDiskBlockIndex;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "00000000000000000000000000000000000000000000000000000000000000000000000072B8CF3535341"
                  "2682457137856341288776655444222116824571378563412007856341268245713785634126824571378"
                  "56341278563412682457137856341255B5DB3535341268245713785634128877665544422211682457137"
                  "8563412007956341200000000000000000000000000000000000000000000000000000000000000002587"
                  "9E26353412682457137856341288776655444222116824571378563412007856341222563412555634120"
                  "000000000000000000000000000000000000000000000000000000000000000");
    }

    struct CBlockLocatorDer : public CBlockLocator
    {
        void add(uint256 v) { vHave.push_back(v); }
    } cBlockLocator;
    cBlockLocator.add(uint256v);
    cBlockLocator.add(uint256v + 1582728274);

    {
        CDataStream ss(SER_DISK, 0);
        ss << cBlockLocator;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "0000000002245713353534126824571378563412887766554442"
                                                   "221168245713785634120076DB69933534126824571378563412"
                                                   "8877665544422211682457137856341200");
    }

    CMerkleBlock cMerkleBlock(cBlock, cBloomFilter);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cMerkleBlock;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "7856341224571378563412682457137856341288776655443322116824571378563412002457133535341"
                  "2682457137856341288776655444222116824571378563412007856341278563412785634120000010000"
                  "000107A1544C6FEB385BFF14176CD20041BD864AFEBEB579A9BCEB815943B73D183C0100");
    }

    NTP1OutPoint ntp1OutPoint(uint256v, 0x12345678);
    {
        CDataStream ss(SER_DISK, 0);
        ss << ntp1OutPoint;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "245713353534126824571378563412887766554442221168245713785634120078563412");
    }

    NTP1TokenTxData ntp1TokenTxData;
    ntp1TokenTxData.setTokenId("abcdefg");                // std::string
    ntp1TokenTxData.setAmount(0x12345678);                // NTP1Int
    ntp1TokenTxData.setIssueTxIdHex(uint256v.ToString()); // uint256
    ntp1TokenTxData.setDivisibility(0x1234567813572468);  // uint64_t
    ntp1TokenTxData.setLockStatus(false);                 // int
    ntp1TokenTxData.setAggregationPolicy("abbcddeefg");   // std::string
    ntp1TokenTxData.setTokenSymbol("ABCDE");              // std::string
    {
        CDataStream ss(SER_DISK, 0);
        ss << ntp1TokenTxData;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0761626364656667093330353431393839362457133535341268245713785634128877665544422211682"
                  "4571378563412006824571378563412000000000A61626263646465656667054142434445");
    }
    NTP1TxIn ntp1TxIn;
    ntp1TxIn.setPrevout(ntp1OutPoint);                    // NTP1OutPoint
    ntp1TxIn.setScriptSigHex(cTxIn.scriptSig.ToString()); // std::string
    ntp1TxIn.setSequence(0x1234567813572468);             // uint64_t
    ntp1TxIn.__addToken(ntp1TokenTxData);                 // std::vector<NTP1TokenTxData>
    {
        CDataStream ss(SER_DISK, 0);
        ss << ntp1TxIn;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "245713353534126824571378563412887766554442221168245713785634120078563412314F505F4E4F5"
                  "0204F505F564552204F505F4946204F505F4E4F544946204F505F5645524946204F505F5645524E4F5449"
                  "4668245713785634120107616263646566670933303534313938393624571335353412682457137856341"
                  "288776655444222116824571378563412006824571378563412000000000A616262636464656566670541"
                  "42434445");
    }

    NTP1TxOut ntp1TxOut;
    ntp1TxOut.setNValue(0x12345678);          // int64_t
    ntp1TxOut.setScriptPubKeyAsm("abcdefg");  // std::string
    ntp1TxOut.setScriptPubKeyHex("deadbeef"); // std::string
    ntp1TxOut.__addToken(ntp1TokenTxData);    // std::vector<NTP1TokenTxData>
    ntp1TxOut.setAddress("ABCDEF");           // std::string

    {
        CDataStream ss(SER_DISK, 0);
        ss << ntp1TxOut;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "7856341200000000086465616462656566076162636465666701076162636465666709333035343139383"
                  "9362457133535341268245713785634128877665544422211682457137856341200682457137856341200"
                  "0000000A6162626364646565666705414243444506414243444546");
    }

    NTP1Transaction ntp1Transaction;
    ntp1Transaction.__manualSet(0x12345678, uint256v, {'a', 'b', 'c', 'd', 'e', 'f'}, {ntp1TxIn},
                                {ntp1TxOut}, 0x1234567813572468, 0x1234567813572468,
                                NTP1TxType_TRANSFER);
    {
        CDataStream ss(SER_DISK, 0);
        ss << ntp1Transaction;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "7856341268245713785634122457133535341268245713785634128877665544422211682457137856341"
                  "20001245713353534126824571378563412887766554442221168245713785634120078563412314F505F"
                  "4E4F50204F505F564552204F505F4946204F505F4E4F544946204F505F5645524946204F505F5645524E4"
                  "F544946682457137856341201076162636465666709333035343139383936245713353534126824571378"
                  "56341288776655444222116824571378563412006824571378563412000000000A6162626364646565666"
                  "7054142434445017856341200000000086465616462656566076162636465666701076162636465666709"
                  "3330353431393839362457133535341268245713785634128877665544422211682457137856341200682"
                  "4571378563412000000000A61626263646465656667054142434445064142434445466824571378563412"
                  "03000000");
    }

    CMessageHeader cMessageHeader;
    std::string    pchMessageStart = "ABCD";
    std::string    pchCommand      = "AABBCDEFGHIJ";
    ASSERT_EQ(pchMessageStart.size(), 4u);
    ASSERT_EQ(pchCommand.size(), 12u);
    std::copy(pchMessageStart.cbegin(), pchMessageStart.cend(), cMessageHeader.pchMessageStart);
    std::copy(pchCommand.cbegin(), pchCommand.cend(), cMessageHeader.pchCommand);
    cMessageHeader.nMessageSize = 0x12345678;
    cMessageHeader.nChecksum    = 0x12345678;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cMessageHeader;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "4142434441414242434445464748494A7856341278563412");
    }

    CInv cInv(0x12345678, uint256v);
    {
        CDataStream ss(SER_DISK, 0);
        ss << cInv;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "785634122457133535341268245713785634128877665544422211682457137856341200");
    }

    CKeyPool cKeyPool;
    cKeyPool.nTime     = 0x1234567813572244;
    cKeyPool.vchPubKey = cPubKey;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cKeyPool;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "00000000442257137856341206616263646566");
    }

    CWalletTx cWalletTx(nullptr);
    cWalletTx.nTime = 0x12345678;
    cWalletTx.vtxPrev.push_back(cMerkleTx); // std::vector<CMerkleTx>
    cWalletTx.mapValue.insert({"abc", "XY"});
    cWalletTx.mapValue.insert({"XXY", "Z"}); // mapValue_t
    cWalletTx.vOrderForm             = {{"aXc", "vXY"},
                            {"XaY", "sZ"}}; // std::vector<std::pair<std::string, std::string>>
    cWalletTx.fTimeReceivedIsTxTime  = 0x12345678;      // unsigned int
    cWalletTx.nTimeReceived          = 0x12345678;      // unsigned int
    cWalletTx.nTimeSmart             = 0x12345678;      // unsigned int
    cWalletTx.fFromMe                = 0x26;            // char
    cWalletTx.strFromAccount         = "XXAASQWERYHGFD";               // std::string
    cWalletTx.vfSpent                = {'A', 'B', 'C', 'D', 'E', 'G'}; // std::vector<char>
    cWalletTx.nOrderPos              = 0x1234567813572467;             // int64_t
    cWalletTx.fDebitCached           = 1;                              // mutable bool
    cWalletTx.fCreditCached          = 0;                              // mutable bool
    cWalletTx.fAvailableCreditCached = 0;                              // mutable bool
    cWalletTx.fChangeCached          = 0;                              // mutable bool
    cWalletTx.nDebitCached           = 0x1234567813572467;             // mutable int64_t
    cWalletTx.nCreditCached          = 0x1234567813572267;             // mutable int64_t
    cWalletTx.nAvailableCreditCached = 0x1234562813572467;             // mutable int64_t
    cWalletTx.nChangeCached          = 0x1234567813512467;             // mutable int64_t
    {
        CDataStream ss(SER_DISK, 0);
        ss << cWalletTx;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0100000078563412000000000000000000000000000000000000000000000000000000000000000000000"
                  "000000000FFFFFFFF01785634127856341201245713785634126824571378563412887766554433221168"
                  "2457137856341200785634120661626364656678563412016824571378563412066162636465667856341"
                  "2245713785634126824571378563412887766554433221168245713785634120004245713785634126824"
                  "5713785634128877665544332211682457137856341200245713353534126824571378563412887766554"
                  "4422211682457137856341200245713785634126824571378563412887766554433422232547613785634"
                  "1200245713785634126824574323111132547698907856547668245713785634120078563412060358585"
                  "9015A036162630258590B66726F6D6163636F756E740E5858414153515745525948474644016E13313331"
                  "31373638343635313932313939323731057370656E74063131313131310974696D65736D6172740933303"
                  "53431393839360203615863037658590358615902735A78563412785634122601");
    }

    CWalletKey  cWalletKey;
    std::string key = "abcdefghijklmnopqrstuvwxyz";
    std::copy(key.cbegin(), key.cend(), std::back_inserter(cWalletKey.vchPrivKey));
    cWalletKey.nTimeCreated = 0x1234562813572467;
    cWalletKey.nTimeExpires = 0x1233452813572467;
    cWalletKey.strComment   = "CommentyCommentttttttt";
    {
        CDataStream ss(SER_DISK, 0);
        ss << cWalletKey;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "000000001A6162636465666768696A6B6C6D6E6F707172737475"
                                                   "767778797A6724571328563412672457132845331216436F6D6D"
                                                   "656E7479436F6D6D656E7474747474747474");
    }

    CAccount cAccount;
    cAccount.vchPubKey = cPubKey;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cAccount;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "0000000006616263646566");
    }

    CAccountingEntry cAccountingEntry;
    cAccountingEntry.strAccount      = "ABcdEFg";              // std::string
    cAccountingEntry.nCreditDebit    = 0x1234562813572467;     // int64_t
    cAccountingEntry.nTime           = 0x1234562813572467;     // int64_t
    cAccountingEntry.strOtherAccount = "xyzXYZabc";            // std::string
    cAccountingEntry.strComment      = "CommentyCommentttttt"; // std::string
    cAccountingEntry.mapValue.insert({"abc", "XY"});
    cAccountingEntry.mapValue.insert({"XXY", "Z"});  // mapValue_t
    cAccountingEntry.nOrderPos = 0x1234562813572467; // int64_t
    cAccountingEntry.nEntryNo  = 0x1234562813572467; // uint64_t
    {
        CDataStream ss(SER_DISK, 0);
        ss << cAccountingEntry;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "00000000672457132856341267245713285634120978797A5859"
                                                   "5A61626339436F6D6D656E7479436F6D6D656E74747474747400"
                                                   "0303585859015A03616263025859016E13313331313736383132"
                                                   "31353934383135353931");
    }

    CKeyMetadata cKeyMetadata;
    cKeyMetadata.nVersion    = 0x12345678;
    cKeyMetadata.nCreateTime = 0x1234567813572468;
    {
        CDataStream ss(SER_DISK, 0);
        ss << cKeyMetadata;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "785634126824571378563412");
    }

    libzerocoin::IntegerGroupParams integerGroupParams;
    integerGroupParams.initialized = 1;
    integerGroupParams.g           = 0x1222367813579;
    integerGroupParams.h           = 0x123557813579;
    integerGroupParams.modulus     = 0x12345678122579;
    integerGroupParams.groupOrder  = 0x12345678122589;
    {
        CDataStream ss(SER_DISK, 0);
        ss << integerGroupParams;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0107793581672322010679358157351207792512785634120789251278563412");
    }

    libzerocoin::AccumulatorAndProofParams accumulatorAndProofParams;
    accumulatorAndProofParams.initialized                   = 1;
    accumulatorAndProofParams.accumulatorModulus            = 0x1234567813579;
    accumulatorAndProofParams.accumulatorBase               = 0x1234569813579;
    accumulatorAndProofParams.accumulatorPoKCommitmentGroup = integerGroupParams;
    accumulatorAndProofParams.accumulatorQRNCommitmentGroup = integerGroupParams;
    accumulatorAndProofParams.minCoinValue                  = 0x123456777813579;
    accumulatorAndProofParams.maxCoinValue                  = 0x12333367813579;
    accumulatorAndProofParams.k_prime                       = 0x12345678;
    accumulatorAndProofParams.k_dprime                      = 0x13574372;
    {
        CDataStream ss(SER_DISK, 0);
        ss << accumulatorAndProofParams;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0107793581674523010779358169452301010779358167232201067935815735120779251278563412078"
                  "9251278563412010779358167232201067935815735120779251278563412078925127856341208793581"
                  "776745230107793581673333127856341272435713");
    }

    CBigNum bg(0x1234567892468);
    // number must be > 1023 bits
    bg = bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg * bg *
         bg * bg * bg * bg * bg;
    libzerocoin::Params params(bg);
    params.initialized                    = 1;
    params.accumulatorParams              = accumulatorAndProofParams;
    params.coinCommitmentGroup            = integerGroupParams;
    params.serialNumberSoKCommitmentGroup = integerGroupParams;
    params.zkp_iterations                 = 0x12345678;
    params.zkp_hash_len                   = 0x13595258;
    {
        CDataStream ss(SER_DISK, 0);
        ss << params;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "0101077935816745230107793581694523010107793581672322010679358157351207792512785634120"
                  "7892512785634120107793581672322010679358157351207792512785634120789251278563412087935"
                  "8177674523010779358167333312785634127243571301077935816723220106793581573512077925127"
                  "8563412078925127856341201077935816723220106793581573512077925127856341207892512785634"
                  "127856341258525913");
    }

    libzerocoin::PublicCoin publicCoin(&params);
    {
        CDataStream ss(SER_DISK, 0);
        ss << publicCoin;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "0032000000");
    }

    libzerocoin::PublicCoin privateCoin(&params);
    {
        CDataStream ss(SER_DISK, 0);
        ss << privateCoin;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "0032000000");
    }

    libzerocoin::SpendMetaData spendMetaData(uint256v, uint256v + 0x2425464757325336);
    {
        CDataStream ss(SER_DISK, 0);
        ss << spendMetaData;
        EXPECT_EQ(boost::algorithm::hex(ss.str()), "2457133535341268245713785634128877665544422211682457"
                                                   "1378563412005AAA458C7C7A378C245713785634128877665544"
                                                   "422211682457137856341200");
    }

    {
        CDataStream ss(SER_DISK, 0);
        ss << bg;
        EXPECT_EQ(boost::algorithm::hex(ss.str()),
                  "9100000000000000000061587366EAD04904DD36E83CBD2DC82F29C64C80CB1F5B7156C133DBF8FC0576E"
                  "A05B7DC8B02357D614546334134FD9D42BE947DF3232180E38E69E4F6D133409A0F3D2D89EE747E7C60C0"
                  "BE3969F0BE4DE12A14D45B935FD7E3A00DD751209BC7B16D12796E977D24D6E954C04C161E86A5977795C"
                  "F1BA1BCC32465C32D1E14F47E38C41D772616");
    }
}
