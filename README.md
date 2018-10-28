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

## Windows
We cross compile the windows binary on Linux using MXE (Ubuntu recommended)

### Download our pre-compiled MXE toolchain
```
https://neblio-files.ams3.cdn.digitaloceanspaces.com/dependencies/mxe.tar.gz
tar -xf mxe.tar.gz
sudo mv mxe /
export PATH=/mxe/mxe/usr/bin:$PATH
```

### Build neblio-Qt
```
i686-w64-mingw32.static-qmake-qt5 "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" neblio-wallet.pro
make -B -w -j4
```

## macOS
### Install the following dependencies (homebrew)
```
brew update
brew fetch --retry qt            && brew install qt --force
brew fetch --retry berkeley-db@4 && brew install berkeley-db@4 --force
brew fetch --retry boost@1.60    && brew install boost@1.60 --force
brew fetch --retry miniupnpc     && brew install miniupnpc --force
brew fetch --retry curl          && brew install curl --force
brew fetch --retry openssl       && brew install openssl --force
brew fetch --retry qrencode      && brew install qrencode --force

brew unlink qt            && brew link --force --overwrite qt
brew unlink berkeley-db@4 && brew link --force --overwrite berkeley-db@4
brew unlink boost@1.60    && brew link --force --overwrite boost@1.60
brew unlink miniupnpc     && brew link --force --overwrite miniupnpc
brew unlink curl          && brew link --force --overwrite curl
brew unlink python        && brew link --force --overwrite python
brew unlink openssl       && brew link --force --overwrite openssl
brew unlink qrencode      && brew link --force --overwrite qrencode
```

### Build neblio-Qt
```
qmake "USE_UPNP=1" "USE_QRCODE=1" "RELEASE=1" neblio-wallet.pro
make -B -w -j4
```

### Prepare the .dmg file for release (optional)
```
sudo easy_install appscript
./contrib/macdeploy/macdeployqtplus ./neblio-Qt.app -add-qt-tr da,de,es,hu,ru,uk,zh_CN,zh_TW \
-dmg -fancy ./contrib/macdeploy/fancy.plist -verbose 1 -rpath /usr/local/opt/qt/lib
```
