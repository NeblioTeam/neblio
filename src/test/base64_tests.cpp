#include "googletest/googletest/include/gtest/gtest.h"

#include "main.h"
#include "wallet.h"
#include "util.h"

TEST(base64_tests, base64_testvectors)
{
    static const std::string vstrIn[]  = {"","f","fo","foo","foob","fooba","foobar"};
    static const std::string vstrOut[] = {"","Zg==","Zm8=","Zm9v","Zm9vYg==","Zm9vYmE=","Zm9vYmFy"};
    for (unsigned int i=0; i<sizeof(vstrIn)/sizeof(vstrIn[0]); i++)
    {
        std::string strEnc = EncodeBase64(vstrIn[i]);
        EXPECT_TRUE(strEnc == vstrOut[i]);
        std::string strDec = DecodeBase64(strEnc);
        EXPECT_TRUE(strDec == vstrIn[i]);
    }
}
