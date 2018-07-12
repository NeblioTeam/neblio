# Input
DEPENDPATH += . json qt
HEADERS += qt/bitcoingui.h \
    qt/transactiontablemodel.h \
    qt/addresstablemodel.h \
    qt/optionsdialog.h \
    qt/coincontroldialog.h \
    qt/coincontroltreewidget.h \
    qt/sendcoinsdialog.h \
    qt/addressbookpage.h \
    qt/signverifymessagedialog.h \
    qt/aboutdialog.h \
    qt/editaddressdialog.h \
    qt/bitcoinaddressvalidator.h \
    alert.h \
    addrman.h \
    base58.h \
    bignum.h \
    checkpoints.h \
    compat.h \
    coincontrol.h \
    sync.h \
    util.h \
    hash.h \
    uint256.h \
    kernel.h \
    scrypt.h \
    pbkdf2.h \
    zerocoin/Accumulator.h \
    zerocoin/AccumulatorProofOfKnowledge.h \
    zerocoin/Coin.h \
    zerocoin/CoinSpend.h \
    zerocoin/Commitment.h \
    zerocoin/ParamGeneration.h \
    zerocoin/Params.h \
    zerocoin/SerialNumberSignatureOfKnowledge.h \
    zerocoin/SpendMetaData.h \
    zerocoin/ZeroTest.h \
    zerocoin/Zerocoin.h \
    serialize.h \
    main.h \
    miner.h \
    net.h \
    key.h \
    db.h \
    txdb.h \
    walletdb.h \
    script.h \
    init.h \
    hash.h \
    bloom.h \
    mruset.h \
    json/json_spirit_writer_template.h \
    json/json_spirit_writer.h \
    json/json_spirit_value.h \
    json/json_spirit_utils.h \
    json/json_spirit_stream_reader.h \
    json/json_spirit_reader_template.h \
    json/json_spirit_reader.h \
    json/json_spirit_error_position.h \
    json/json_spirit.h \
    qt/clientmodel.h \
    qt/guiutil.h \
    qt/transactionrecord.h \
    qt/guiconstants.h \
    qt/optionsmodel.h \
    qt/monitoreddatamapper.h \
    qt/transactiondesc.h \
    qt/transactiondescdialog.h \
    qt/bitcoinamountfield.h \
    wallet.h \
    keystore.h \
    qt/transactionfilterproxy.h \
    qt/transactionview.h \
    qt/walletmodel.h \
    bitcoinrpc.h \
    qt/overviewpage.h \
    qt/ui_overviewpage.h \
    qt/ntp1summary.h \
    qt/ui_ntp1summary.h \
    qt/ui_sendcoinsdialog.h \
    qt/ui_coincontroldialog.h \
    qt/ui_sendcoinsdialog.h \
    qt/ui_addressbookpage.h \
    qt/ui_signverifymessagedialog.h \
    qt/ui_aboutdialog.h \
    qt/ui_editaddressdialog.h \
    qt/ui_transactiondescdialog.h \
    qt/ui_sendcoinsentry.h \
    qt/ui_askpassphrasedialog.h \
    qt/ui_rpcconsole.h \
    qt/ui_optionsdialog.h \
    qt/csvmodelwriter.h \
    crypter.h \
    qt/sendcoinsentry.h \
    qt/qvalidatedlineedit.h \
    qt/bitcoinunits.h \
    qt/qvaluecombobox.h \
    qt/askpassphrasedialog.h \
    qt/neblioupdatedialog.h \
    protocol.h \
    qt/notificator.h \
    qt/qtipcserver.h \
    allocators.h \
    ui_interface.h \
    qt/rpcconsole.h \
    qt/ClickableLabel.h \
    version.h \
    netbase.h \
    clientversion.h \
    threadsafety.h \
    neblioupdater.h \
    neblioversion.h \
    neblioreleaseinfo.h \
    curltools.h \
    qt/messageboxwithtimer.h \
    qt/ntp1/ntp1tokenlistmodel.h \
    qt/ntp1/ntp1tokenlistfilterproxy.h \
    ntp1/ntp1tokenmetadata.h \
    ntp1/ntp1wallet.h \
    qt/ntp1/ntp1tokenlistitemdelegate.h \
    ThreadSafeHashMap.h \
    qt/ntp1senddialog.h \
    qt/ntp1sendsingletokenfields.h \
    ntp1/ntp1sendtokensdata.h \
    qt/ntp1sendtokensfeewidget.h


HEADERS +=                 \
    ntp1/ntp1tools.h       \
    ntp1/ntp1inpoint.h     \
    ntp1/ntp1outpoint.h    \
    ntp1/ntp1transaction.h \
    ntp1/ntp1txin.h        \
    ntp1/ntp1txout.h       \
    ntp1/ntp1tokentxdata.h \
    ntp1/ntp1apicalls.h    \
    ntp1/ntp1sendtokensonerecipientdata.h \
    ntp1/ntp1script.h      \
    ntp1/ntp1script_issuance.h



SOURCES += qt/bitcoin.cpp \
    qt/bitcoingui.cpp \
    qt/transactiontablemodel.cpp \
    qt/addresstablemodel.cpp \
    qt/optionsdialog.cpp \
    qt/sendcoinsdialog.cpp \
    qt/coincontroldialog.cpp \
    qt/coincontroltreewidget.cpp \
    qt/addressbookpage.cpp \
    qt/signverifymessagedialog.cpp \
    qt/aboutdialog.cpp \
    qt/editaddressdialog.cpp \
    qt/bitcoinaddressvalidator.cpp \
    alert.cpp \
    version.cpp \
    sync.cpp \
    util.cpp \
    hash.cpp \
    netbase.cpp \
    key.cpp \
    script.cpp \
    main.cpp \
    miner.cpp \
    init.cpp \
    net.cpp \
    bloom.cpp \
    checkpoints.cpp \
    addrman.cpp \
    db.cpp \
    walletdb.cpp \
    qt/clientmodel.cpp \
    qt/guiutil.cpp \
    qt/transactionrecord.cpp \
    qt/optionsmodel.cpp \
    qt/monitoreddatamapper.cpp \
    qt/transactiondesc.cpp \
    qt/transactiondescdialog.cpp \
    qt/bitcoinstrings.cpp \
    qt/bitcoinamountfield.cpp \
    wallet.cpp \
    keystore.cpp \
    qt/transactionfilterproxy.cpp \
    qt/transactionview.cpp \
    qt/walletmodel.cpp \
    bitcoinrpc.cpp \
    rpcdump.cpp \
    rpcnet.cpp \
    rpcmining.cpp \
    rpcwallet.cpp \
    rpcblockchain.cpp \
    rpcrawtransaction.cpp \
    qt/overviewpage.cpp \
    qt/ntp1summary.cpp \
    qt/csvmodelwriter.cpp \
    crypter.cpp \
    qt/sendcoinsentry.cpp \
    qt/qvalidatedlineedit.cpp \
    qt/bitcoinunits.cpp \
    qt/qvaluecombobox.cpp \
    qt/askpassphrasedialog.cpp \
    protocol.cpp \
    qt/notificator.cpp \
    qt/qtipcserver.cpp \
    qt/rpcconsole.cpp \
    qt/ClickableLabel.cpp \
    qt/neblioupdatedialog.cpp \
    qt/messageboxwithtimer.cpp \
    noui.cpp \
    kernel.cpp \
    scrypt-arm.S \
    scrypt-x86.S \
    scrypt-x86_64.S \
    scrypt.cpp \
    pbkdf2.cpp \
    zerocoin/Accumulator.cpp \
    zerocoin/AccumulatorProofOfKnowledge.cpp \
    zerocoin/Coin.cpp \
    zerocoin/CoinSpend.cpp \
    zerocoin/Commitment.cpp \
    zerocoin/ParamGeneration.cpp \
    zerocoin/Params.cpp \
    zerocoin/SerialNumberSignatureOfKnowledge.cpp \
    zerocoin/SpendMetaData.cpp \
    neblioupdater.cpp \
    neblioversion.cpp \
    json/json_spirit_value.cpp \
    json/json_spirit_reader.cpp \
    json/json_spirit_writer.cpp \
    neblioreleaseinfo.cpp \
    zerocoin/ZeroTest.cpp \
    curltools.cpp \
    qt/ntp1/ntp1tokenlistmodel.cpp \
    qt/ntp1/ntp1tokenlistfilterproxy.cpp \
    ntp1/ntp1tokenmetadata.cpp \
    ntp1/ntp1wallet.cpp \
    qt/ntp1/ntp1tokenlistitemdelegate.cpp \
    ThreadSafeHashMap.cpp \
    qt/ntp1senddialog.cpp \
    qt/ntp1sendsingletokenfields.cpp \
    ntp1/ntp1sendtokensdata.cpp \
    ntp1/ntp1sendtokensonerecipientdata.cpp \
    qt/ntp1sendtokensfeewidget.cpp

SOURCES +=                   \
    ntp1/ntp1tools.cpp       \
    ntp1/ntp1inpoint.cpp     \
    ntp1/ntp1outpoint.cpp    \
    ntp1/ntp1transaction.cpp \
    ntp1/ntp1txin.cpp        \
    ntp1/ntp1txout.cpp       \
    ntp1/ntp1tokentxdata.cpp \
    ntp1/ntp1apicalls.cpp    \
    ntp1/ntp1script.cpp      \
    ntp1/ntp1script_issuance.cpp


contains(NEBLIO_REST, 1) {
    HEADERS += nebliorest.h
    SOURCES += nebliorest.cpp
}

contains(USE_QRCODE, 1) {
HEADERS += qt/qrcodedialog.h
SOURCES += qt/qrcodedialog.cpp
FORMS += qt/forms/qrcodedialog.ui
}
