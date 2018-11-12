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


# Neblio Command Line Parameters
These parameters can be passed to nebliod or neblio-Qt at runtime. They can also be hardcoded in a neblio.conf file located in the data directory.

```
Usage:
  nebliod [options]
  nebliod [options] <command> [params]  Send command to -server or nebliod
  nebliod [options] help                List commands
  nebliod [options] help <command>      Get help for a command

Options:
  -?                     This help message
  -conf=<file>           Specify configuration file (default: neblio.conf)
  -pid=<file>            Specify pid file (default: nebliod.pid)
  -datadir=<dir>         Specify data directory
  -wallet=<dir>          Specify wallet file (within data directory)
  -dbcache=<n>           Set database cache size in megabytes (default: 25)
  -dblogsize=<n>         Set database disk log size in megabytes (default: 100)
  -timeout=<n>           Specify connection timeout in milliseconds (default: 5000)
  -proxy=<ip:port>       Connect through socks proxy
  -socks=<n>             Select the version of socks proxy to use (4-5, default: 5)
  -tor=<ip:port>         Use proxy to reach tor hidden services (default: same as -proxy)
  -dns                   Allow DNS lookups for -addnode, -seednode and -connect
  -port=<port>           Listen for connections on <port> (default: 6325 or testnet: 16325)
  -maxconnections=<n>    Maintain at most <n> connections to peers (default: 125)
  -addnode=<ip>          Add a node to connect to and attempt to keep the connection open
  -connect=<ip>          Connect only to the specified node(s)
  -seednode=<ip>         Connect to a node to retrieve peer addresses, and disconnect
  -externalip=<ip>       Specify your own public address
  -onlynet=<net>         Only connect to nodes in network <net> (IPv4, IPv6 or Tor)
  -discover              Discover own IP address (default: 1 when listening and no -externalip)
  -listen                Accept connections from outside (default: 1 if no -proxy or -connect)
  -bind=<addr>           Bind to given address. Use [host]:port notation for IPv6
  -dnsseed               Find peers using DNS lookup (default: 1)
  -staking               Stake your coins to support network and gain reward (default: 1)
  -synctime              Sync time with other nodes. Disable if time on your system is precise e.g. syncing with Network Time Protocol (default: 1)
  -cppolicy              Sync checkpoints policy (default: strict)
  -banscore=<n>          Threshold for disconnecting misbehaving peers (default: 100)
  -bantime=<n>           Number of seconds to keep misbehaving peers from reconnecting (default: 86400)
  -maxreceivebuffer=<n>  Maximum per-connection receive buffer, <n>*1000 bytes (default: 5000)
  -maxsendbuffer=<n>     Maximum per-connection send buffer, <n>*1000 bytes (default: 1000)
  -upnp                  Use UPnP to map the listening port (default: 1 when listening)
  -paytxfee=<amt>        Fee per KB to add to transactions you send
  -mininput=<amt>        When creating transactions, ignore inputs with value less than this (default: 0.01)
  -daemon                Run in the background as a daemon and accept commands
  -testnet               Use the test network
  -debug                 Output extra debugging information. Implies all other -debug* options
  -debugnet              Output extra network debugging information
  -logtimestamps         Prepend debug output with timestamp
  -shrinkdebugfile       Shrink debug.log file on client startup (default: 1 when no -debug)
  -printtoconsole        Send trace/debug info to console instead of debug.log file
  -rpcuser=<user>        Username for JSON-RPC connections
  -rpcpassword=<pw>      Password for JSON-RPC connections
  -rpcport=<port>        Listen for JSON-RPC connections on <port> (default: 6326 or testnet: 16326)
  -rpcallowip=<ip>       Allow JSON-RPC connections from specified IP address
  -rpcconnect=<ip>       Send commands to node running on <ip> (default: 127.0.0.1)
  -blocknotify=<cmd>     Execute command when the best block changes (%s in cmd is replaced by block hash)
  -walletnotify=<cmd>    Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)
  -confchange            Require a confirmations for change (default: 0)
  -enforcecanonical      Enforce transaction scripts to use canonical PUSH operators (default: 1)
  -alertnotify=<cmd>     Execute command when a relevant alert is received (%s in cmd is replaced by message)
  -upgradewallet         Upgrade wallet to latest format
  -keypool=<n>           Set key pool size to <n> (default: 100)
  -rescan                Rescan the block chain for missing wallet transactions
  -salvagewallet         Attempt to recover private keys from a corrupt wallet.dat
  -checkblocks=<n>       How many blocks to check at startup (default: 2500, 0 = all)
  -checklevel=<n>        How thorough the block verification is (0-6, default: 1)
  -loadblock=<file>      Imports blocks from external blk000?.dat file

Block creation options:
  -blockminsize=<n>      Set minimum block size in bytes (default: 0)
  -blockmaxsize=<n>      Set maximum block size in bytes (default: 8000000)
  -blockprioritysize=<n> Set maximum size of high-priority/low-fee transactions in bytes (default: 27000)

SSL options:
  -rpcssl                                  Use OpenSSL (https) for JSON-RPC connections
  -rpcsslcertificatechainfile=<file.cert>  Server certificate file (default: server.cert)
  -rpcsslprivatekeyfile=<file.pem>         Server private key (default: server.pem)
  -rpcsslciphers=<ciphers>                 Acceptable ciphers (default: TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!AH:!3DES:@STRENGTH)
```


# Neblio RPC Commands
RPC commands are used to interact with a running instance of nebliod or neblio-Qt. They are used via the command line with nebliod, or via the neblio-Qt Debug Console.

```
addmultisigaddress <nrequired> <'["key","key"]'> [account]
addredeemscript <redeemScript> [account]
backupwallet <destination>
checkwallet
createrawntp1transaction [{"txid":txid,"vout":n},...] {address:{tokenid/tokenName:tokenAmount}},{address:neblAmount,...}
createrawtransaction [{"txid":txid,"vout":n},...] {address:amount,...}
decoderawtransaction <hex string>
decodescript <hex string>
dumpprivkey <neblioaddress>
dumpwallet <filename>
encryptwallet <passphrase>
exportblockchain <path-dir>
getaccount <neblioaddress>
getaccountaddress <account>
getaddressesbyaccount <account>
getbalance [account] [minconf=1]
getbestblockhash
getblock <hash> [verbose=true]
getblockbynumber <number> [txinfo]
getblockcount
getblockhash <index>
getblocktemplate [params]
getcheckpoint
getconnectioncount
getdifficulty
getinfo
getmininginfo
getnewaddress [account]
getnewpubkey [account]
getntp1balances [minconf=1]
getpeerinfo
getrawmempool
getrawtransaction <txid> [verbose=0]
getreceivedbyaccount <account> [minconf=1]
getreceivedbyaddress <neblioaddress> [minconf=1]
getstakinginfo
getsubsidy [nTarget]
gettransaction <txid>
getwork [data]
getworkex [data, coinbase]
help [command]
importprivkey <neblioprivkey> [label]
importwallet <filename>
keypoolrefill [new-size]
listaccounts [minconf=1]
listaddressgroupings
listreceivedbyaccount [minconf=1] [includeempty=false]
listreceivedbyaddress [minconf=1] [includeempty=false]
listsinceblock [blockhash] [target-confirmations]
listtransactions [account] [count=10] [from=0]
listunspent [minconf=1] [maxconf=9999999] ["address",...]
makekeypair [prefix]
move <fromaccount> <toaccount> <amount> [minconf=1] [comment]
repairwallet
resendtx
reservebalance [<reserve> [amount]]
sendalert <message> <privatekey> <minver> <maxver> <priority> <id> [cancelupto]
sendfrom <fromaccount> <toneblioaddress> <amount> [minconf=1] [comment] [comment-to]
sendmany <fromaccount (must be empty, unsupported)> {address:amount,...} [comment]
sendntp1toaddress <neblioaddress> <amount> <tokenId/tokenName> [comment] [comment-to]
sendrawtransaction <hex string>
sendtoaddress <neblioaddress> <amount> [comment] [comment-to]
setaccount <neblioaddress> <account>
settxfee <amount>
signmessage <neblioaddress> <message>
signrawtransaction <hex string> [{"txid":txid,"vout":n,"scriptPubKey":hex},...] [<privatekey1>,...] [sighashtype="ALL"]
stop
submitblock <hex data> [optional-params-obj]
validateaddress <neblioaddress>
validatepubkey <nebliopubkey>
verifymessage <neblioaddress> <signature> <message>
```
