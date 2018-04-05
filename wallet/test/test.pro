TEMPLATE = app
TARGET = neblio-tests
DEFINES += QT_GUI BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE
CONFIG += no_include_pwd
CONFIG += thread

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
}

NEBLIO_ROOT = $${PWD}/../..
TEST_ROOT = $${NEBLIO_ROOT}/wallet/test/
DEFINES += "TEST_ROOT_PATH=\"\\\"$${TEST_ROOT}\\\"\""
VPATH       += $${NEBLIO_ROOT}/wallet $${NEBLIO_ROOT}/wallet/json $${NEBLIO_ROOT}/wallet/qt
INCLUDEPATH += $${NEBLIO_ROOT}/wallet $${NEBLIO_ROOT}/wallet/json $${NEBLIO_ROOT}/wallet/qt

INCLUDEPATH += googletest/googletest googletest/googletest/include

include($${NEBLIO_ROOT}/wallet/wallet.pri)
include($${NEBLIO_ROOT}/wallet/neblio-qt-libs.pri)

SOURCES += \
    googletest/googletest/src/gtest-all.cc \
    googletest/googletest/src/gtest_main.cc

SOURCES += \
    accounting_tests.cpp  \
    allocator_tests.cpp   \
    base32_tests.cpp      \
    base58_tests.cpp      \
    base64_tests.cpp      \
    bignum_tests.cpp      \
    bloom_tests.cpp       \
    canonical_tests.cpp   \
    compress_tests.cpp    \
    getarg_tests.cpp      \
    key_tests.cpp         \
    mruset_tests.cpp      \
    netbase_tests.cpp     \
    pmt_tests.cpp         \
    rpc_tests.cpp         \
    script_tests.cpp      \
    serialize_tests.cpp   \
    sigopcount_tests.cpp  \
    transaction_tests.cpp \
    uint160_tests.cpp     \
    uint256_tests.cpp     \
    util_tests.cpp        \
    wallet_tests.cpp

DEFINES += BITCOIN_QT_TEST
