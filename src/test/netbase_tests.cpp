#include "googletest/googletest/include/gtest/gtest.h"

#include <string>
#include <vector>

#include "netbase.h"

using namespace std;

TEST(netbase_tests, netbase_networks)
{
    EXPECT_TRUE(CNetAddr("127.0.0.1").GetNetwork()                              == NET_UNROUTABLE);
    EXPECT_TRUE(CNetAddr("::1").GetNetwork()                                    == NET_UNROUTABLE);
    EXPECT_TRUE(CNetAddr("8.8.8.8").GetNetwork()                                == NET_IPV4);
    EXPECT_TRUE(CNetAddr("2001::8888").GetNetwork()                             == NET_IPV6);
    EXPECT_TRUE(CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetNetwork() == NET_TOR);
}

TEST(netbase_tests, netbase_properties)
{
    EXPECT_TRUE(CNetAddr("127.0.0.1").IsIPv4());
    EXPECT_TRUE(CNetAddr("::FFFF:192.168.1.1").IsIPv4());
    EXPECT_TRUE(CNetAddr("::1").IsIPv6());
    EXPECT_TRUE(CNetAddr("10.0.0.1").IsRFC1918());
    EXPECT_TRUE(CNetAddr("192.168.1.1").IsRFC1918());
    EXPECT_TRUE(CNetAddr("172.31.255.255").IsRFC1918());
    EXPECT_TRUE(CNetAddr("2001:0DB8::").IsRFC3849());
    EXPECT_TRUE(CNetAddr("169.254.1.1").IsRFC3927());
    EXPECT_TRUE(CNetAddr("2002::1").IsRFC3964());
    EXPECT_TRUE(CNetAddr("FC00::").IsRFC4193());
    EXPECT_TRUE(CNetAddr("2001::2").IsRFC4380());
    EXPECT_TRUE(CNetAddr("2001:10::").IsRFC4843());
    EXPECT_TRUE(CNetAddr("FE80::").IsRFC4862());
    EXPECT_TRUE(CNetAddr("64:FF9B::").IsRFC6052());
    EXPECT_TRUE(CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").IsTor());
    EXPECT_TRUE(CNetAddr("127.0.0.1").IsLocal());
    EXPECT_TRUE(CNetAddr("::1").IsLocal());
    EXPECT_TRUE(CNetAddr("8.8.8.8").IsRoutable());
    EXPECT_TRUE(CNetAddr("2001::1").IsRoutable());
    EXPECT_TRUE(CNetAddr("127.0.0.1").IsValid());
}

bool static TestSplitHost(string test, string host, int port)
{
    string hostOut;
    int portOut = -1;
    SplitHostPort(test, portOut, hostOut);
    return hostOut == host && port == portOut;
}

TEST(netbase_tests, netbase_splithost)
{
    EXPECT_TRUE(TestSplitHost("www.bitcoin.org", "www.bitcoin.org", -1));
    EXPECT_TRUE(TestSplitHost("[www.bitcoin.org]", "www.bitcoin.org", -1));
    EXPECT_TRUE(TestSplitHost("www.bitcoin.org:80", "www.bitcoin.org", 80));
    EXPECT_TRUE(TestSplitHost("[www.bitcoin.org]:80", "www.bitcoin.org", 80));
    EXPECT_TRUE(TestSplitHost("127.0.0.1", "127.0.0.1", -1));
    EXPECT_TRUE(TestSplitHost("127.0.0.1:8333", "127.0.0.1", 8333));
    EXPECT_TRUE(TestSplitHost("[127.0.0.1]", "127.0.0.1", -1));
    EXPECT_TRUE(TestSplitHost("[127.0.0.1]:8333", "127.0.0.1", 8333));
    EXPECT_TRUE(TestSplitHost("::ffff:127.0.0.1", "::ffff:127.0.0.1", -1));
    EXPECT_TRUE(TestSplitHost("[::ffff:127.0.0.1]:8333", "::ffff:127.0.0.1", 8333));
    EXPECT_TRUE(TestSplitHost("[::]:8333", "::", 8333));
    EXPECT_TRUE(TestSplitHost("::8333", "::8333", -1));
    EXPECT_TRUE(TestSplitHost(":8333", "", 8333));
    EXPECT_TRUE(TestSplitHost("[]:8333", "", 8333));
    EXPECT_TRUE(TestSplitHost("", "", -1));
}

bool static TestParse(string src, string canon)
{
    CService addr;
    if (!LookupNumeric(src.c_str(), addr, 65535))
        return canon == "";
    return canon == addr.ToString();
}

TEST(netbase_tests, netbase_lookupnumeric)
{
    EXPECT_TRUE(TestParse("127.0.0.1", "127.0.0.1:65535"));
    EXPECT_TRUE(TestParse("127.0.0.1:8333", "127.0.0.1:8333"));
    EXPECT_TRUE(TestParse("::ffff:127.0.0.1", "127.0.0.1:65535"));
    EXPECT_TRUE(TestParse("::", "[::]:65535"));
    EXPECT_TRUE(TestParse("[::]:8333", "[::]:8333"));
    EXPECT_TRUE(TestParse("[127.0.0.1]", "127.0.0.1:65535"));
    EXPECT_TRUE(TestParse(":::", ""));
}

TEST(netbase_tests, onioncat_test)
{
    // values from http://www.cypherpunk.at/onioncat/wiki/OnionCat
    CNetAddr addr1("5wyqrzbvrdsumnok.onion");
    CNetAddr addr2("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca");
    EXPECT_TRUE(addr1 == addr2);
    EXPECT_TRUE(addr1.IsTor());
    EXPECT_TRUE(addr1.ToStringIP() == "5wyqrzbvrdsumnok.onion");
    EXPECT_TRUE(addr1.IsRoutable());
}
