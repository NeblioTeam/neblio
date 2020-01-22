include(neblio-wallet.pri)

TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += wallet

mac {
	QMAKE_INFO_PLIST = wallet/qt/res/Info.plist
}

#NEBLIO_CONFIG += Tests

contains( NEBLIO_CONFIG, Tests ) {
    SUBDIRS += wallet/test
}

contains( NEBLIO_CONFIG, NoWallet ) {
    SUBDIRS += wallet/test
    SUBDIRS -= wallet
}
