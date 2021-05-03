TEMPLATE = app
TARGET = neblio-tests
DEFINES += QT_GUI BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE
CONFIG += no_include_pwd
CONFIG += thread
QMAKE_CXXFLAGS += -std=c++11

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
}

DEFINES += "UNITTEST_RUN_NTP1_PARSE_TESTS"
#DEFINES += "UNITTEST_DOWNLOAD_AND_CREATE_TX_DATA"
DEFINES += "UNITTEST_REDOWNLOAD_TXID_LIST"

NEBLIO_TEST += TRUE
NEBLIO_ROOT = $${PWD}/../..
TEST_ROOT = $${NEBLIO_ROOT}/wallet/test/
DEFINES += "TEST_ROOT_PATH=\"\\\"$${TEST_ROOT}\\\"\""
VPATH       += $${NEBLIO_ROOT}/wallet $${NEBLIO_ROOT}/wallet/json $${NEBLIO_ROOT}/wallet/qt
INCLUDEPATH += $${NEBLIO_ROOT}/wallet $${NEBLIO_ROOT}/wallet/json $${NEBLIO_ROOT}/wallet/qt

INCLUDEPATH += googletest/googletest googletest/googletest/include
INCLUDEPATH += googletest/googlemock googletest/googlemock/include

# since this is for tests, unused functions can exist
unix:QMAKE_CXXFLAGS += -Wno-unused-function

include($${NEBLIO_ROOT}/wallet/wallet.pri)
include($${NEBLIO_ROOT}/wallet/wallet-libs.pri)

SOURCES += \
    googletest/googletest/src/gtest-all.cc \
    googletest/googletest/src/gtest_main.cc \
    googletest/googlemock/src/gmock-all.cc \

SOURCES += \
    accounting_tests.cpp  \
    allocator_tests.cpp   \
    base32_tests.cpp      \
    base58_tests.cpp      \
    base64_tests.cpp      \
    bignum_tests.cpp      \
    bloom_tests.cpp       \
    blockindexlru_tests.cpp \
    canonical_tests.cpp   \
    checkpoints_tests.cpp \
    compress_tests.cpp    \
    crypter_tests.cpp     \
    db_tests.cpp          \
    fixedpoint_tests.cpp  \
    getarg_tests.cpp      \
    hash_tests.cpp        \
    key_tests.cpp         \
    merkle_tests.cpp      \
    miner_tests.cpp       \
    mruset_tests.cpp      \
    netbase_tests.cpp     \
    ntp1_selection_tests.cpp \
    ntp1_tests.cpp        \
    pmt_tests.cpp         \
    pos_tests.cpp         \
    rpc_tests.cpp         \
    result_tests.cpp      \
    script_tests.cpp      \
    serialize_tests.cpp   \
    sigopcount_tests.cpp  \
    transaction_tests.cpp \
    uint160_tests.cpp     \
    uint256_tests.cpp     \
    util_tests.cpp        \
    wallet_tests.cpp      \
    environment.cpp

DEFINES += BITCOIN_QT_TEST
