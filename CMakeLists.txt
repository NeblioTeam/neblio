cmake_minimum_required(VERSION 3.2.0)
project(Neblio)

option(NEBLIO_CMAKE "Enable buildig with cmake (to ensure this is for development)" OFF)
if(NOT NEBLIO_CMAKE)
    message(FATAL_ERROR "This cmake file is only for development. Please use qmake to build the project.")
endif()

cmake_host_system_information(RESULT hostname QUERY HOSTNAME)
message("Building on machine: " ${hostname})

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-private-field")

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj")
endif()

option(COMPILE_GUI            "Enable compiling neblio-qt" ON)
option(COMPILE_DAEMON         "Enable compiling nebliod" ON)
option(COMPILE_CURL           "Download and compile libcurl (and OpenSSL) automatically (Not for Windows)" OFF)
option(COMPILE_TESTS          "Build tests" ON)
option(USE_QRCODE             "Enable QRCode" ON)
option(USE_UPNP               "Enable Miniupnpc" OFF)
option(USE_DBUS               "Enable Dbus" ON)
option(USE_CUSTOM_WARNINGS    "Enable custom warnings" OFF)
option(DISABLE_ASSERTS        "Disables asserts" OFF)
option(CONFIGURE_SYSTEM_TESTS "Configure system tests" ON)

if(NOT DISABLE_ASSERTS)
    # this removes the definition NDEBUG from release mode and release mode with debug info
    # for both gcc/clang and Visual Studio
    string( REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string( REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string( REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string( REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

option(SANITIZE_THREAD "Enable clang thread-sanitizer (only for clang-compiler)" OFF)
if(SANITIZE_THREAD)
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS}        -g -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    -fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
#    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS}           -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/sanitizer-blacklist.txt")
endif()

option(SANITIZE_UNDEFINED "Enable clang undefined-behavior-sanitizer (only for clang-compiler)" OFF)
if(SANITIZE_UNDEFINED)
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -g -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
endif()

option(SANITIZE_ADDRESS "Enable clang address-sanitizer (only for clang-compiler)" OFF)
if(SANITIZE_ADDRESS)
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -g -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

option(SANITIZE_LEAK "Enable clang leak-sanitizer (only for clang-compiler)" OFF)
if(SANITIZE_LEAK)
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -g -fsanitize=leak")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=leak")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=leak")
endif()

option(SANITIZE_MEMORY "Enable clang memory-sanitizer (only for clang-compiler)" OFF)
if(SANITIZE_MEMORY)
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -g -fsanitize=memory")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=memory")
endif()

set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(INSERT CMAKE_MODULE_PATH 0
     "${CMAKE_SOURCE_DIR}/cmake")

add_definitions("-DQT_STATICPLUGIN")

foreach(plugin ${Qt5Widgets_PLUGINS})
  get_target_property(_loc ${plugin} LOCATION)
  message("Plugin ${plugin} is at location ${_loc}")
endforeach()

set(Boost_USE_STATIC_LIBS 1)
set(Boost_USE_STATIC_RUNTIME 1)

find_package(Qt5Widgets       REQUIRED)
find_package(Qt5Core          REQUIRED)
find_package(Qt5LinguistTools REQUIRED)
find_package(BerkeleyDB       REQUIRED)
find_package(PkgConfig        REQUIRED)
find_package(Boost 1.55       COMPONENTS system random filesystem thread regex program_options iostreams atomic REQUIRED)
find_package(Threads          REQUIRED)
find_package(sodium           REQUIRED)

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
find_package(ZLIB       REQUIRED)


if(COMPILE_CURL AND NOT WIN32)
    include(FindPythonInterp)
    if( !PYTHONINTERP_FOUND )
        message( FATAL_ERROR "Could not find a Python interpreter. A python interpreter is requirered to compile curl." )
    endif()

    if(IS_SYMLINK "${CMAKE_BINARY_DIR}/openssl_build" OR EXISTS "${CMAKE_BINARY_DIR}/openssl_build")
    else()
        message("Compiling OpenSSL...")
        execute_process(
            COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/build_scripts/CompileOpenSSL-Linux.py"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            OUTPUT_VARIABLE COMPILE_OPENSSL_OUTPUT
            ERROR_VARIABLE COMPILE_OPENSSL_OUTPUT
            RESULT_VARIABLE COMPILE_OPENSSL_RETURN_VALUE
            )

        if (NOT COMPILE_OPENSSL_RETURN_VALUE EQUAL 0)
            message(FATAL_ERROR "Failed to compile OpenSSL: ${COMPILE_OPENSSL_OUTPUT}")
        endif()
        message("Done compiling OpenSSL.")
    endif()

    if(IS_SYMLINK "${CMAKE_BINARY_DIR}/curl_build" OR EXISTS "${CMAKE_BINARY_DIR}/curl_build")
    else()
        message("Compiling libcurl...")
        execute_process(
            COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/build_scripts/CompileCurl-Linux.py"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            OUTPUT_VARIABLE COMPILE_CURL_OUTPUT
            ERROR_VARIABLE COMPILE_CURL_OUTPUT
            RESULT_VARIABLE COMPILE_CURL_RETURN_VALUE
            )

        if (NOT COMPILE_CURL_RETURN_VALUE EQUAL 0)
            message(FATAL_ERROR "Failed to compile OpenSSL: ${COMPILE_CURL_OUTPUT}")
        endif()
        message("Done compiling libcurl.")
    endif()
    SET(ENV{PKG_CONFIG_PATH} "${CMAKE_BINARY_DIR}/curl_build/lib/pkgconfig/:$ENV{PKG_CONFIG_PATH}")
    SET(ENV{PKG_CONFIG_PATH} ":${CMAKE_BINARY_DIR}/openssl_build/lib/pkgconfig/:$ENV{PKG_CONFIG_PATH}")
endif()

############################## CURL and OpenSSL PkgConfig
execute_process(
    COMMAND "${PKG_CONFIG_EXECUTABLE}" "libcurl" "--libs"
    OUTPUT_VARIABLE CURL_LIBS
    ERROR_VARIABLE CURL_LIBS
    RESULT_VARIABLE CURL_LIBS_RETURN_VALUE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
if (NOT CURL_LIBS_RETURN_VALUE EQUAL 0)
    message(FATAL_ERROR "Failed to retrieve libs for libcurl with pkg-config: ${CURL_LIBS} ")
endif()
execute_process(
    COMMAND "${PKG_CONFIG_EXECUTABLE}" "libcurl" "--cflags"
    OUTPUT_VARIABLE CURL_INCLUDES
    ERROR_VARIABLE CURL_INCLUDES
    RESULT_VARIABLE CURL_INCLUDES_RETURN_VALUE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

if (NOT CURL_INCLUDES_RETURN_VALUE EQUAL 0)
    message(FATAL_ERROR "Failed to retrieve libs for libcurl with pkg-config: ${CURL_INCLUDES}")
endif()
execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} openssl --libs
    OUTPUT_VARIABLE OPENSSL_LIBS
    ERROR_VARIABLE OPENSSL_LIBS
    RESULT_VARIABLE OPENSSL_LIBS_RETURN_VALUE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
if (NOT OPENSSL_LIBS_RETURN_VALUE EQUAL 0)
    message(FATAL_ERROR "Failed to retrieve libs for openssl with pkg-config: ${OPENSSL_LIBS}")
endif()
execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} openssl --cflags
    OUTPUT_VARIABLE OPENSSL_INCLUDES
    ERROR_VARIABLE OPENSSL_INCLUDES
    RESULT_VARIABLE OPENSSL_INCLUDES_RETURN_VALUE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
if (NOT OPENSSL_INCLUDES_RETURN_VALUE EQUAL 0)
    message(FATAL_ERROR "Failed to retrieve libs for openssl with pkg-config: ${OPENSSL_INCLUDES}")
endif()
add_definitions(-DCURL_STATICLIB)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPENSSL_INCLUDES} ${CURL_INCLUDES}")
################################################

if(USE_DBUS)
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} dbus-1 --libs
        OUTPUT_VARIABLE DBUS_LIBRARY_DIRS
        ERROR_VARIABLE DBUS_LIBRARY_DIRS
        RESULT_VARIABLE DBUS_LIBS_RETURN_VALUE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    if (NOT DBUS_LIBS_RETURN_VALUE EQUAL 0)
        message(FATAL_ERROR "Failed to retrieve libs for dbus with pkg-config: ${DBUS_LIBRARY_DIRS}")
    endif()
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} dbus-1 --cflags
        OUTPUT_VARIABLE DBUS_INCLUDE_DIRS
        ERROR_VARIABLE DBUS_INCLUDE_DIRS
        RESULT_VARIABLE DBUS_INCLUDES_RETURN_VALUE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    if (NOT DBUS_INCLUDES_RETURN_VALUE EQUAL 0)
        message(FATAL_ERROR "Failed to retrieve libs for dbus with pkg-config: ${DBUS_INCLUDE_DIRS}")
    endif()
    set(CMAKE_CXX_FLAGS "-Wno-unused-command-line-argument")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DBUS_LIBRARY_DIRS}")
endif()

# copy "res", the resources dir to build dir
file(COPY "${CMAKE_SOURCE_DIR}/wallet/qt/res" DESTINATION "${CMAKE_BINARY_DIR}")
# read, modify then write the qrc file, remove "locale/" directory as it's not necessary when translation files are generated in build dir
file(READ ${CMAKE_SOURCE_DIR}/wallet/qt/bitcoin.qrc resources_file_data)
STRING(REGEX REPLACE "locale\/(bitcoin[a-zA-Z_]+\.qm)" "\\1" resources_file_data_mod "${resources_file_data}" )
FILE(WRITE ${CMAKE_BINARY_DIR}/bitcoin.qrc "${resources_file_data_mod}")
set(RESOURCE ${CMAKE_BINARY_DIR}/bitcoin.qrc)
qt5_add_resources(RESOURCE_ADDED ${RESOURCE})

add_subdirectory(test)

add_definitions(-DBOOST_SPIRIT_THREADSAFE)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.12 -arch x86_64 -Wno-nullability-completeness -Wno-unused-command-line-argument")
    add_library(mac_libs
        qt/macdockiconhandler.h
        qt/macnotificationhandler.h
        qt/macdockiconhandler.mm
        qt/macnotificationhandler.mm
        )
    target_link_libraries(mac_libs
        -framework Foundation -framework ApplicationServices -framework AppKit
        )
    add_definitions(-DMAC_OSX MSG_NOSIGNAL=0)

    # NOTE: Don't include the path in MACOSX_BUNDLE_ICON_FILE -- this is
    # the property added to Info.plist
    set(MACOSX_BUNDLE_ICON_FILE qt/res/icons/bitcoin.icns)

    # And this part tells CMake where to find and install the file itself
    set(myApp_ICON ${CMAKE_CURRENT_SOURCE_DIR}/images/myAppImage.icns)
    set_source_files_properties(${myApp_ICON} PROPERTIES
           MACOSX_PACKAGE_LOCATION "Resources")
endif()

if(UNIX AND NOT APPLE)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-Bstatic")
    add_definitions(-DLINUX)
endif()

if(NOT WIN32)
    # for extra security against potential buffer overflows: enable GCCs Stack Smashing Protection
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all -Wstack-protector")
    # We need to exclude this for Windows cross compile with MinGW 4.2.x, as it will result in a non-working executable!
    # This can be enabled for Windows, when we switch to MinGW >= 4.4.x.
endif()

# for extra security (see: https://wiki.debian.org/Hardening)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2")
if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,relro -Wl,-z,now")
endif()

if(WIN32)
    # for extra security on Windows: enable ASLR and DEP via GCC linker flags
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -Wl,--dynamicbase -Wl,--nxcompat")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -Wl,--large-address-aware -static")
    add_definitions(-DWIN32)
    add_definitions(-D_MT -DBOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN)
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -lmingwthrd -mthreads")
    set(CMAKE_CXX_FLAGS "-Wno-unused-variable")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-Bstatic")
endif()

# TODO
#contains(BITCOIN_NEED_QT_PLUGINS, 1) {
#    DEFINES += BITCOIN_NEED_QT_PLUGINS
#    QTPLUGIN += qcncodecs qjpcodecs qtwcodecs qkrcodecs qtaccessiblewidgets
#}

#############################################################
# lmdb
# Check whether we're on a 32-bit or 64-bit system
if(CMAKE_SIZEOF_VOID_P EQUAL "8")
  set(DEFAULT_LMDB64 ON)
else()
  set(DEFAULT_LMDB64 OFF)
endif()
option(USE_LMDB64 "Build LMDB for 64-bit? 'OFF' builds for 32-bit." ${DEFAULT_LMDB64})

add_subdirectory(wallet/liblmdb)
include_directories(${LMDB_INCLUDE})
#############################################################

###################
# generate build.h
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/build")
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/build/build.h
    COMMAND /bin/sh ${CMAKE_SOURCE_DIR}/share/genbuild.sh ${CMAKE_BINARY_DIR}/build/build.h
)
set_property(SOURCE ${CMAKE_BINARY_DIR}/build/build.h PROPERTY SKIP_AUTOMOC ON)
###################

if(CMAKE_SIZEOF_VOID_P EQUAL "4") # 32-bit compiler
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -msse2")
endif()

if(USE_CUSTOM_WARNINGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-show-option -Wall -Wextra -Wno-ignored-qualifiers -Wformat -Wformat-security -Wno-unused-parameter -Wstack-protector")
endif()

# TODO: from qmake. Is this necessary?
# CODECFORTR = UTF-8

qt5_add_translation(QM_FILES
    wallet/qt/locale/bitcoin_af_ZA.ts
    wallet/qt/locale/bitcoin_ar.ts
    wallet/qt/locale/bitcoin_be_BY.ts
    wallet/qt/locale/bitcoin_bg.ts
    wallet/qt/locale/bitcoin_bs.ts
    wallet/qt/locale/bitcoin_ca.ts
    wallet/qt/locale/bitcoin_ca@valencia.ts
    wallet/qt/locale/bitcoin_ca_ES.ts
    wallet/qt/locale/bitcoin_cs.ts
    wallet/qt/locale/bitcoin_cy.ts
    wallet/qt/locale/bitcoin_da.ts
    wallet/qt/locale/bitcoin_de.ts
    wallet/qt/locale/bitcoin_el_GR.ts
    wallet/qt/locale/bitcoin_en.ts
    wallet/qt/locale/bitcoin_eo.ts
    wallet/qt/locale/bitcoin_es.ts
    wallet/qt/locale/bitcoin_es_CL.ts
    wallet/qt/locale/bitcoin_es_DO.ts
    wallet/qt/locale/bitcoin_es_MX.ts
    wallet/qt/locale/bitcoin_es_UY.ts
    wallet/qt/locale/bitcoin_et.ts
    wallet/qt/locale/bitcoin_eu_ES.ts
    wallet/qt/locale/bitcoin_fa.ts
    wallet/qt/locale/bitcoin_fa_IR.ts
    wallet/qt/locale/bitcoin_fi.ts
    wallet/qt/locale/bitcoin_fr.ts
    wallet/qt/locale/bitcoin_fr_CA.ts
    wallet/qt/locale/bitcoin_gl.ts
    wallet/qt/locale/bitcoin_he.ts
    wallet/qt/locale/bitcoin_hi_IN.ts
    wallet/qt/locale/bitcoin_hr.ts
    wallet/qt/locale/bitcoin_hu.ts
    wallet/qt/locale/bitcoin_id_ID.ts
    wallet/qt/locale/bitcoin_it.ts
    wallet/qt/locale/bitcoin_ja.ts
    wallet/qt/locale/bitcoin_ka.ts
    wallet/qt/locale/bitcoin_kk_KZ.ts
    wallet/qt/locale/bitcoin_ko_KR.ts
    wallet/qt/locale/bitcoin_ky.ts
    wallet/qt/locale/bitcoin_la.ts
    wallet/qt/locale/bitcoin_lt.ts
    wallet/qt/locale/bitcoin_lv_LV.ts
    wallet/qt/locale/bitcoin_ms_MY.ts
    wallet/qt/locale/bitcoin_nb.ts
    wallet/qt/locale/bitcoin_nl.ts
    wallet/qt/locale/bitcoin_pam.ts
    wallet/qt/locale/bitcoin_pl.ts
    wallet/qt/locale/bitcoin_pt_BR.ts
    wallet/qt/locale/bitcoin_pt_PT.ts
    wallet/qt/locale/bitcoin_ro_RO.ts
    wallet/qt/locale/bitcoin_ru.ts
    wallet/qt/locale/bitcoin_sah.ts
    wallet/qt/locale/bitcoin_sk.ts
    wallet/qt/locale/bitcoin_sl_SI.ts
    wallet/qt/locale/bitcoin_sq.ts
    wallet/qt/locale/bitcoin_sr.ts
    wallet/qt/locale/bitcoin_sv.ts
    wallet/qt/locale/bitcoin_th_TH.ts
    wallet/qt/locale/bitcoin_tr.ts
    wallet/qt/locale/bitcoin_uk.ts
    wallet/qt/locale/bitcoin_ur_PK.ts
    wallet/qt/locale/bitcoin_vi.ts
    wallet/qt/locale/bitcoin_vi_VN.ts
    wallet/qt/locale/bitcoin_zh_CN.ts
    wallet/qt/locale/bitcoin_zh_TW.ts
    )

if(USE_QRCODE AND COMPILE_GUI)
    include(FindPythonInterp)
    if( !PYTHONINTERP_FOUND )
        message( FATAL_ERROR "Could not find a Python interpreter. A python interpreter is requirered to compile qrencode." )
    endif()

    if(NOT WIN32)
        if(IS_SYMLINK "${CMAKE_BINARY_DIR}/qrencode_build" OR EXISTS "${CMAKE_BINARY_DIR}/qrencode_build")
        else()
            message("Compiling qrencode...")
            execute_process(
                COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/build_scripts/CompileQREncode-Linux.py"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                OUTPUT_VARIABLE COMPILE_QRENCODE_OUTPUT
                ERROR_VARIABLE COMPILE_QRENCODE_OUTPUT
                RESULT_VARIABLE COMPILE_QRENCODE_RETURN_VALUE
                )
            if (NOT COMPILE_QRENCODE_RETURN_VALUE EQUAL 0)
                message(FATAL_ERROR "Failed to compile OpenSSL: ${COMPILE_QRENCODE_OUTPUT}")
            endif()
            message("Done compiling qrencode.")
        endif()
    endif()
    SET(ENV{PKG_CONFIG_PATH} "${CMAKE_BINARY_DIR}/qrencode_build/lib/pkgconfig/:$ENV{PKG_CONFIG_PATH}")
    execute_process(
        COMMAND "${PKG_CONFIG_EXECUTABLE}" "libqrencode" "--libs"
        OUTPUT_VARIABLE QRENCODE_LIBS
        ERROR_VARIABLE QRENCODE_LIBS
        RESULT_VARIABLE QRENCODE_LIBS_RETURN_VALUE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    if (NOT QRENCODE_LIBS_RETURN_VALUE EQUAL 0)
        message(FATAL_ERROR "Failed to retrieve libs for libqrencode with pkg-config: ${QRENCODE_LIBS} ")
    endif()
    execute_process(
        COMMAND "${PKG_CONFIG_EXECUTABLE}" "libqrencode" "--cflags"
        OUTPUT_VARIABLE QRENCODE_INCLUDES
        ERROR_VARIABLE QRENCODE_INCLUDES
        RESULT_VARIABLE QRENCODE_INCLUDES_RETURN_VALUE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    set(QRCODE_SOURCES "wallet/qt/qrcodedialog.cpp" "wallet/qt/ui_qrcodedialog.h")
    add_definitions(-DUSE_QRCODE)
endif()

add_library(gui_lib STATIC
    wallet/qt/bitcoingui.cpp
    wallet/qt/transactiontablemodel.cpp
    wallet/qt/addresstablemodel.cpp
    wallet/qt/optionsdialog.cpp
    wallet/qt/sendcoinsdialog.cpp
    wallet/qt/coincontroldialog.cpp
    wallet/qt/coincontroltreewidget.cpp
    wallet/qt/addressbookpage.cpp
    wallet/qt/signverifymessagedialog.cpp
    wallet/qt/aboutdialog.cpp
    wallet/qt/editaddressdialog.cpp
    wallet/qt/bitcoinaddressvalidator.cpp
    wallet/qt/clientmodel.cpp
    wallet/qt/guiutil.cpp
    wallet/qt/transactionrecord.cpp
    wallet/qt/optionsmodel.cpp
    wallet/qt/monitoreddatamapper.cpp
    wallet/qt/transactiondesc.cpp
    wallet/qt/transactiondescdialog.cpp
    wallet/qt/bitcoinstrings.cpp
    wallet/qt/bitcoinamountfield.cpp
    wallet/qt/transactionfilterproxy.cpp
    wallet/qt/transactionview.cpp
    wallet/qt/walletmodel.cpp
    wallet/qt/overviewpage.cpp
    wallet/qt/csvmodelwriter.cpp
    wallet/qt/sendcoinsentry.cpp
    wallet/qt/qvalidatedlineedit.cpp
    wallet/qt/bitcoinunits.cpp
    wallet/qt/qvaluecombobox.cpp
    wallet/qt/askpassphrasedialog.cpp
    wallet/qt/notificator.cpp
    wallet/qt/qtipcserver.cpp
    wallet/qt/rpcconsole.cpp
    wallet/qt/ClickableLabel.cpp
    wallet/qt/neblioupdatedialog.cpp
    wallet/qt/messageboxwithtimer.cpp
    ${QRCODE_SOURCES}
    )

if(USE_QRCODE)
    target_compile_options(gui_lib PRIVATE ${QRENCODE_INCLUDES})
endif()

target_link_libraries(gui_lib
    Qt5::Core
    Qt5::Widgets
    ${QRENCODE_LIBS}
    ui_lib
    )

add_library(ui_lib STATIC
    wallet/qt/ui_aboutdialog.h
    wallet/qt/ui_addressbookpage.h
    wallet/qt/ui_askpassphrasedialog.h
    wallet/qt/ui_coincontroldialog.h
    wallet/qt/ui_editaddressdialog.h
    wallet/qt/ui_ntp1summary.h
    wallet/qt/ui_optionsdialog.h
    wallet/qt/ui_overviewpage.h
    wallet/qt/ui_qrcodedialog.h
    wallet/qt/ui_rpcconsole.h
    wallet/qt/ui_sendcoinsdialog.h
    wallet/qt/ui_sendcoinsentry.h
    wallet/qt/ui_signverifymessagedialog.h
    wallet/qt/ui_transactiondescdialog.h
    )

target_link_libraries(ui_lib
    Qt5::Core
    Qt5::Widgets
    )

add_library(ntp1_gui_lib STATIC
    wallet/qt/ntp1summary.cpp
    wallet/qt/ntp1/ntp1tokenlistmodel.cpp
    wallet/qt/ntp1/ntp1tokenlistfilterproxy.cpp
    wallet/qt/ntp1/ntp1tokenlistitemdelegate.cpp
    wallet/qt/ntp1/ntp1createmetadatadialog.cpp
    wallet/qt/ntp1/ntp1metadatapairwidget.cpp
    wallet/qt/ntp1/ntp1custommetadatawidget.cpp
    wallet/qt/ntp1/ntp1metadatapairswidget.cpp
    wallet/qt/ntp1sendtokensfeewidget.cpp
    wallet/qt/json/AbstractTreeNode.cpp
    wallet/qt/json/JsonNewNodeDialog.cpp
    wallet/qt/json/JsonTreeModel.cpp
    wallet/qt/json/JsonTreeNode.cpp
    wallet/qt/json/JsonTreeView.cpp
    wallet/qt/json/NTP1MetadataViewer.cpp
    wallet/qt/ntp1/issuenewntp1tokendialog.cpp
    )

target_link_libraries(ntp1_gui_lib
    Qt5::Core
    Qt5::Widgets
    ui_lib
    )

add_library(core_lib STATIC
    ${CMAKE_BINARY_DIR}/build/build.h
    wallet/alert.cpp
    wallet/version.cpp
    wallet/sync.cpp
    wallet/util.cpp
    wallet/hash.cpp
    wallet/netbase.cpp
    wallet/key.cpp
    wallet/script.cpp
    wallet/main.cpp
    wallet/miner.cpp
    wallet/net.cpp
    wallet/bloom.cpp
    wallet/checkpoints.cpp
    wallet/addrman.cpp
    wallet/db.cpp
    wallet/walletdb.cpp
    wallet/keystore.cpp
    wallet/bitcoinrpc.cpp
    wallet/rpcdump.cpp
    wallet/rpcnet.cpp
    wallet/rpcmining.cpp
    wallet/rpcwallet.cpp
    wallet/rpcblockchain.cpp
    wallet/rpcrawtransaction.cpp
    wallet/crypter.cpp
    wallet/protocol.cpp
    wallet/noui.cpp
    wallet/kernel.cpp
    wallet/scrypt-arm.S
    wallet/scrypt-x86.S
    wallet/scrypt-x86_64.S
    wallet/scrypt.cpp
    wallet/pbkdf2.cpp
    wallet/neblioupdater.cpp
    wallet/neblioversion.cpp
    wallet/neblioreleaseinfo.cpp
    wallet/ThreadSafeMap.cpp
    wallet/ThreadSafeHashMap.cpp
    wallet/NetworkForks.cpp
    wallet/blockindexcatalog.cpp
    wallet/blockindex.cpp
    wallet/outpoint.cpp
    wallet/inpoint.cpp
    wallet/block.cpp
    wallet/transaction.cpp
    wallet/globals.cpp
    wallet/diskblockindex.cpp
    wallet/disktxpos.cpp
    wallet/txindex.cpp
    wallet/txin.cpp
    wallet/txout.cpp
    wallet/txmempool.cpp
    wallet/merkletx.cpp
    wallet/blocklocator.cpp
    wallet/crypto_highlevel.cpp
    wallet/chainparamsbase.cpp
    wallet/chainparams.cpp
    wallet/consensus_params.cpp
    wallet/amount.h
    wallet/CustomTypes.cpp
    wallet/scheduler.cpp
    )

target_link_libraries(core_lib
    ntp1_lib
    curltools_lib
    sodium
    )

add_library(curltools_lib STATIC
    wallet/curltools.cpp
    )

if(USE_UPNP)
    find_package(Miniupnpc REQUIRED)
    message(Building without UPNP support)
    target_link_libraries(core_lib -lminiupnpc)
    target_compile_definitions(core_lib PRIVATE -DUSE_UPNP=1)
endif()

add_library(zerocoin_lib STATIC
    wallet/zerocoin/Accumulator.cpp
    wallet/zerocoin/AccumulatorProofOfKnowledge.cpp
    wallet/zerocoin/Coin.cpp
    wallet/zerocoin/CoinSpend.cpp
    wallet/zerocoin/Commitment.cpp
    wallet/zerocoin/ParamGeneration.cpp
    wallet/zerocoin/Params.cpp
    wallet/zerocoin/SerialNumberSignatureOfKnowledge.cpp
    wallet/zerocoin/SpendMetaData.cpp
    wallet/zerocoin/ZeroTest.cpp
    )

add_library(ntp1_lib STATIC
    wallet/ntp1/ntp1sendtokensonerecipientdata.cpp
    wallet/ntp1/ntp1script_burn.cpp
    wallet/ntp1/ntp1tokenminimalmetadata.cpp
    wallet/ntp1/ntp1sendtxdata.cpp
    wallet/ntp1/ntp1tokenmetadata.cpp
    wallet/ntp1/ntp1wallet.cpp
    wallet/ntp1/ntp1tools.cpp
    wallet/ntp1/ntp1inpoint.cpp
    wallet/ntp1/ntp1outpoint.cpp
    wallet/ntp1/ntp1transaction.cpp
    wallet/ntp1/ntp1txin.cpp
    wallet/ntp1/ntp1txout.cpp
    wallet/ntp1/ntp1tokentxdata.cpp
    wallet/ntp1/ntp1apicalls.cpp
    wallet/ntp1/ntp1script.cpp
    wallet/ntp1/ntp1script_issuance.cpp
    wallet/ntp1/ntp1script_transfer.cpp
    wallet/ntp1/ntp1v1_issuance_static_data.cpp
    )

target_link_libraries(ntp1_lib
    curltools_lib
    )

add_library(json_spirit_lib STATIC
    wallet/json/json_spirit_value.cpp
    wallet/json/json_spirit_reader.cpp
    wallet/json/json_spirit_writer.cpp
    )

if(WIN32)
    # this solves a problem of big string tables on Windows when building in debug mode
    set_property(TARGET json_spirit_lib PROPERTY COMPILE_FLAGS "-O2")
endif()

add_library(txdb_lib STATIC
    wallet/txdb-lmdb.cpp
    wallet/SerializationTester.cpp
    wallet/diskblockindex.cpp
    )

target_link_libraries(txdb_lib
    lmdb
    )

target_link_libraries(curltools_lib
    ${CURL_LIBS}
    ${OPENSSL_LIBS}
)

include_directories(wallet)
include_directories(wallet/json)
target_include_directories(gui_lib PRIVATE wallet/qt)
target_include_directories(ntp1_gui_lib PRIVATE wallet/qt)

include_directories(${ZLIB_INCLUDE_DIRS})

include_directories(${sodium_INCLUDE_DIR})

# the following from here https://github.com/owncloud/client/blob/master/src/gui/CMakeLists.txt
target_compile_definitions(gui_lib PRIVATE "QT_DISABLE_DEPRECATED_BEFORE=0")

if(COMPILE_TESTS)
    # GTEST ##############
    set(GTEST_PATH ${CMAKE_SOURCE_DIR}/wallet/test/googletest/googletest)
    include_directories(${GTEST_PATH}/include)
    include_directories(${GTEST_PATH}/)
    add_library(gtest ${GTEST_PATH}/src/gtest-all.cc)
    target_link_libraries(gtest Threads::Threads)
    ######################

    enable_testing()
    add_subdirectory(wallet/test)
endif()

if(WIN32)
    if(COMPILE_GUI)
        add_executable(
            neblio-qt
            ${RESOURCE_ADDED}
            ${QM_FILES}
            # sources that depend on target as they have defs inside them
            wallet/qt/bitcoin.cpp
            wallet/wallet.cpp
            wallet/init.cpp
            )

        target_link_libraries(neblio-qt
            gui_lib
            ui_lib
            ntp1_gui_lib
            curltools_lib
            zerocoin_lib
            ntp1_lib
            core_lib
            json_spirit_lib
            txdb_lib
            Threads::Threads
            ### win32 libs
            -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32
            ##############
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
        target_compile_definitions(neblio-qt PRIVATE
            QT_GUI
            )
        target_link_libraries(neblio-qt Qt5::QWindowsIntegrationPlugin)
    endif()
    if(COMPILE_DAEMON)
        add_executable(
            # sources that depend on target as they have defs inside them
            nebliod
            wallet/wallet.cpp
            wallet/init.cpp
            )

        target_link_libraries(nebliod
            core_lib
            zerocoin_lib
            ntp1_lib
            curltools_lib
            json_spirit_lib
            txdb_lib
            Threads::Threads
            ### win32 libs
            -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32
            ##############
            Boost::system
            Boost::filesystem
            Boost::thread
            Boost::regex
            Boost::program_options
            ${BERKELEY_DB_LIBRARIES}
            ${CURL_LIBS}
            ${OPENSSL_LIBS}
            )
    endif()
elseif(APPLE)
    add_executable(
        neblio-qt
        MACOSX_BUNDLE ${myApp_ICON}
        )
else()
    if(COMPILE_GUI)
        add_executable(
            neblio-qt
            ${RESOURCE_ADDED}
            ${QM_FILES}
            # sources that depend on target as they have defs inside them
            wallet/qt/bitcoin.cpp
            wallet/wallet.cpp
            wallet/init.cpp
            )

        target_link_libraries(neblio-qt
            gui_lib
            ui_lib
            ntp1_gui_lib
            curltools_lib
            zerocoin_lib
            ntp1_lib
            core_lib
            json_spirit_lib
            txdb_lib
            Threads::Threads
            -lrt
            -ldl
            Boost::system
            Boost::filesystem
            Boost::thread
            Boost::regex
            Boost::program_options
            Boost::iostreams
            ${BERKELEY_DB_LIBRARIES}
            ${CURL_LIBS}
            ${OPENSSL_LIBS}
            ${ZLIB_LIBRARIES}
            )
        target_compile_definitions(neblio-qt PRIVATE
            QT_GUI
            )
    endif()

    if(COMPILE_DAEMON)
        add_executable(
            nebliod
            # sources that depend on target as they have defs inside them
            wallet/wallet.cpp
            wallet/init.cpp
            )

        target_link_libraries(nebliod
            core_lib
            zerocoin_lib
            ntp1_lib
            curltools_lib
            json_spirit_lib
            txdb_lib
            Threads::Threads
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
    endif()
endif()
