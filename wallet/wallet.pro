TEMPLATE = app
TARGET = neblio-qt
VERSION = 1.0.7
INCLUDEPATH += . json qt
DEFINES += QT_GUI BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE
CONFIG += no_include_pwd
CONFIG += thread
QMAKE_CXXFLAGS += -std=c++11
unix:QMAKE_CXXFLAGS += -Wno-attributes

NEBLIO_ROOT = $${PWD}/../

DEFINES += BOOST_BIND_GLOBAL_PLACEHOLDERS

VPATH += $${NEBLIO_ROOT}/wallet/ $${NEBLIO_ROOT}/wallet/json $${NEBLIO_ROOT}/wallet/qt

mac {
	QMAKE_INFO_PLIST = $${NEBLIO_ROOT}/wallet/qt/res/Info.plist
}

# use: qmake "NEBLIO_REST=1"
contains(NEBLIO_REST, 1) {
    DEFINES += NEBLIO_REST
    # restbed
    LIBS += -L"$(CURDIR)/restbed/distribution/library" -lrestbed
    INCLUDEPATH += "$(CURDIR)/restbed/distribution/include/"
    QMAKE_CXXFLAGS += -std=c++11
}

include(wallet.pri)
include(wallet-libs.pri)

RESOURCES += \
    qt/bitcoin.qrc

system($$QMAKE_LRELEASE -silent $$_PRO_FILE_)

DISTFILES +=                                \
    .travis.yml                             \
    ci_scripts/test_linux-daemon.py         \
    ci_scripts/test_linux-gui_wallet.py     \
    ci_scripts/test_win32-gui_wallet.py     \
    ci_scripts/neblio_ci_libs/__init__.py   \
    ci_scripts/neblio_ci_libs/common.py
