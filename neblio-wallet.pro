include(neblio-wallet.pri)

TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += wallet

#NEBLIO_CONFIG += Tests

contains( NEBLIO_CONFIG, Tests ) {
    SUBDIRS += wallet/test
}

contains( NEBLIO_CONFIG, Only_Tests ) {
    SUBDIRS += wallet/test
    SUBDIRS -= wallet
}
