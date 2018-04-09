#include <vector>
#include "googletest/googletest/include/gtest/gtest.h"
#include <boost/foreach.hpp>

#include "script.h"
#include "key.h"

using namespace std;

// Helpers:
static std::vector<unsigned char>
Serialize(const CScript& s)
{
    std::vector<unsigned char> sSerialized(s);
    return sSerialized;
}

TEST(sigopcount_tests, GetSigOpCount)
{
    // Test CScript::GetSigOpCount()
    CScript s1;
    EXPECT_EQ(s1.GetSigOpCount(false), (unsigned)0);
    EXPECT_EQ(s1.GetSigOpCount(true), (unsigned)0);

    uint160 dummy;
    s1 << OP_1 << dummy << dummy << OP_2 << OP_CHECKMULTISIG;
    EXPECT_EQ(s1.GetSigOpCount(true), (unsigned)2);
    s1 << OP_IF << OP_CHECKSIG << OP_ENDIF;
    EXPECT_EQ(s1.GetSigOpCount(true), (unsigned)3);
    EXPECT_EQ(s1.GetSigOpCount(false), (unsigned)21);

    CScript p2sh;
    p2sh.SetDestination(s1.GetID());
    CScript scriptSig;
    scriptSig << OP_0 << Serialize(s1);
    EXPECT_EQ(p2sh.GetSigOpCount(scriptSig), (unsigned)3);

    std::vector<CKey> keys;
    for (int i = 0; i < 3; i++)
    {
        CKey k;
        k.MakeNewKey(true);
        keys.push_back(k);
    }
    CScript s2;
    s2.SetMultisig(1, keys);
    EXPECT_EQ(s2.GetSigOpCount(true), (unsigned)3);
    EXPECT_EQ(s2.GetSigOpCount(false), (unsigned)20);

    p2sh.SetDestination(s2.GetID());
    EXPECT_EQ(p2sh.GetSigOpCount(true), (unsigned)0);
    EXPECT_EQ(p2sh.GetSigOpCount(false), (unsigned)0);
    CScript scriptSig2;
    scriptSig2 << OP_1 << dummy << dummy << Serialize(s2);
    EXPECT_EQ(p2sh.GetSigOpCount(scriptSig2), (unsigned)3);
}
