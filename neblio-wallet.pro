include(neblio-wallet.pri)

TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += src

#NEBLIO_CONFIG += Tests

contains( NEBLIO_CONFIG, Tests ) {
    SUBDIRS += src/test
}
