#include "googletest/googletest/include/gtest/gtest.h"

#include "util.h"

TEST(base32_tests, base32_testvectors)
{
    static const std::string vstrIn[]  = {"","f","fo","foo","foob","fooba","foobar"};
    static const std::string vstrOut[] = {"","my======","mzxq====","mzxw6===","mzxw6yq=","mzxw6ytb","mzxw6ytboi======"};
    for (unsigned int i=0; i<sizeof(vstrIn)/sizeof(vstrIn[0]); i++)
    {
        std::string strEnc = EncodeBase32(vstrIn[i]);
        EXPECT_TRUE(strEnc == vstrOut[i]);
        std::string strDec = DecodeBase32(vstrOut[i]);
        EXPECT_TRUE(strDec == vstrIn[i]);
    }
}
