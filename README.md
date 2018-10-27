![Build Status](https://travis-ci.org/NeblioTeam/neblio.svg?branch=master)

# Open Source Repository for Neblio Nodes & Wallets
More information here: https://nebl.io

Alpha Builds for commits that pass testing can be found here:  
https://neblio-build-staging.ams3.digitaloceanspaces.com/index.html

Pull Requests Welcome

# Building Neblio
## Linux (debian-based)
### Install the following dependencies
```
sudo apt-get update && sudo apt-get install qt5-default qt5-qmake qtbase5-dev-tools \
qttools5-dev-tools build-essential libboost-dev libboost-system-dev libboost-filesystem-dev \
libboost-program-options-dev libboost-thread-dev libboost-regex-dev libssl-dev libdb++-dev \
libminiupnpc-dev libqrencode-dev libcurl4-openssl-dev libldap2-dev libidn11-dev librtmp-dev -y
```

### Build OpenSSL, cURL, and QREncode
```
python ./build_scripts/CompileOpenSSL-Linux.py
python ./build_scripts/CompileCurl-Linux.py
python ./build_scripts/CompileQREncode-Linux.py
export PKG_CONFIG_PATH=$PWD/curl_build/lib/pkgconfig/
export OPENSSL_INCLUDE_PATH=$PWD/openssl_build/include/
export OPENSSL_LIB_PATH=$PWD/openssl_build/lib/
```

### Build nebliod
```
cd wallet
make "STATIC=1" -B -w -f makefile.unix -j4
strip ./nebliod
```

### Build neblio-Qt
```
qmake "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" "OPENSSL_INCLUDE_PATH=$PWD/openssl_build/include/" "OPENSSL_LIB_PATH=$PWD/openssl_build/lib/" "PKG_CONFIG_PATH=$PWD/curl_build/lib/pkgconfig/" neblio-wallet.pro
make -B -w -j4
```
