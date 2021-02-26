include_directories(../..)

add_library(mocks_lib
    mocks/mtxdb.h
    mocks/mntp1wallet.h
    )

add_executable(neblio-tests
    accounting_tests.cpp
    allocator_tests.cpp
    base32_tests.cpp
    base58_tests.cpp
    base64_tests.cpp
    bignum_tests.cpp
    blockindexlru_tests.cpp
    bloom_tests.cpp
    canonical_tests.cpp
    compress_tests.cpp
    checkpoints_tests.cpp
    crypter_tests.cpp
    db_tests.cpp
    fixedpoint_tests.cpp
    getarg_tests.cpp
    hash_tests.cpp
    key_tests.cpp
    merkle_tests.cpp
    miner_tests.cpp
    mruset_tests.cpp
    netbase_tests.cpp
    ntp1_tests.cpp
    ntp1_selection_tests.cpp
    pmt_tests.cpp
    pos_tests.cpp
    result_tests.cpp
    rpc_tests.cpp
    script_tests.cpp
    serialize_tests.cpp
    sigopcount_tests.cpp
    transaction_tests.cpp
    uint160_tests.cpp
    uint256_tests.cpp
    util_tests.cpp
    wallet_tests.cpp
    environment.cpp
    ${GTEST_PATH}/src/gtest_main.cc
    ${GMOCK_PATH}/src/gmock-all.cc
    # sources that depend on target as they have defs inside them, these are not tests
    ${CMAKE_SOURCE_DIR}/wallet/wallet.cpp
    ${CMAKE_SOURCE_DIR}/wallet/init.cpp
    )

target_link_libraries(neblio-tests
    gtest
    core_lib
    logging_lib
    ntp1_lib
    curltools_lib
    json_spirit_lib
    txdb_lib
    mocks_lib
    db_lib
    -lpthread
    -lrt
    -ldl
    Boost::system
    Boost::filesystem
    Boost::thread
    Boost::regex
    Boost::program_options
    Boost::iostreams
    Boost::atomic
    ${BERKELEY_DB_LIBRARIES}
    ${CURL_LIBS}
    ${OPENSSL_LIBS}
    ${ZLIB_LIBRARIES}
    )

set_target_properties(neblio-tests PROPERTIES COMPILE_FLAGS -Wno-unused-function)

target_compile_definitions(neblio-tests PRIVATE
    TEST_ROOT_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\"
    NEBLIO_UNITTESTS
    )

option(NEBLIO_TEST_ALL_TXS "[Unit-tests]: Enable downloading and testing all transactions by comparing it with the API server" ON)
option(NEBLIO_RECREATE_TX_DATA "[Unit-tests]: Enable downloading and recreating transaction data to be used in tests.
                                              If this is OFF and the option NEBLIO_TEST_ALL_TXS, the data will be downloaded directly from the neblio server." OFF)
option(NEBLIO_REDOWNLOAD_TXID_LIST "[Unit-tests]: If NEBLIO_RECREATE_TX_DATA is enabled, enabling this will redownload the TXids list to recreate the database from them" ON)
option(NEBLIO_FORCE_DISABLE_PREMADE_DATA_DOWNLOAD "[Unit-tests]: If NEBLIO_RECREATE_TX_DATA is disabled, this can be used to prevent data from being downloaded" OFF)

if(NEBLIO_TEST_ALL_TXS)
    target_compile_definitions(neblio-tests PRIVATE UNITTEST_RUN_NTP1_PARSE_TESTS)
endif()

if(NEBLIO_RECREATE_TX_DATA)
    target_compile_definitions(neblio-tests PRIVATE UNITTEST_DOWNLOAD_AND_CREATE_TX_DATA)
endif()

if(NEBLIO_REDOWNLOAD_TXID_LIST)
    target_compile_definitions(neblio-tests PRIVATE UNITTEST_REDOWNLOAD_TXID_LIST)
endif()

if(NEBLIO_FORCE_DISABLE_PREMADE_DATA_DOWNLOAD)
    target_compile_definitions(neblio-tests PRIVATE UNITTEST_FORCE_DISABLE_PREMADE_DATA_DOWNLOAD)
endif()

add_test(
    NAME neblio-core-tests
    COMMAND neblio-core-tests
    --datadir ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
