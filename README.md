[![Travis](https://img.shields.io/travis/NeblioTeam/neblio?label=Travis&logo=Travis%20CI&logoColor=fff&style=for-the-badge)](https://travis-ci.org/NeblioTeam/neblio/builds)
[![GitHub Actions](https://img.shields.io/github/workflow/status/NeblioTeam/neblio/CICD?label=GitHub%20Actions&logo=Github&style=for-the-badge)](https://github.com/NeblioTeam/neblio/actions)
[![Latest Release](https://img.shields.io/github/v/release/NeblioTeam/neblio?label=Latest%20Release&style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyBpZD0iTGF5ZXJfMSIgZGF0YS1uYW1lPSJMYXllciAxIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAxMC44IDE0LjY1Ij48ZGVmcz48c3R5bGU+LmNscy0xe2ZvbnQtc2l6ZToxMnB4O2ZpbGw6I2ZmZjtmb250LWZhbWlseTpPcGVuU2Fucy1FeHRyYUJvbGQsIE9wZW4gU2Fucztmb250LXdlaWdodDo4MDA7fTwvc3R5bGU+PC9kZWZzPjx0aXRsZT52PC90aXRsZT48dGV4dCB4PSItMjk2LjUiIHk9Ii0zNjUuNDYiLz48dGV4dCB4PSItMjk2LjUiIHk9Ii0zNjUuNDYiLz48dGV4dCBjbGFzcz0iY2xzLTEiIHRyYW5zZm9ybT0idHJhbnNsYXRlKDAgMTEuMTQpIj52LjwvdGV4dD48L3N2Zz4=)](https://github.com/NeblioTeam/neblio/releases/latest)

![Downloads](https://img.shields.io/github/downloads/NeblioTeam/neblio/total?style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyMjguNDkgMjU2LjgiPjxkZWZzPjxzdHlsZT4uY2xzLTF7ZmlsbDojNGY4OGNlO30uY2xzLTJ7ZmlsbDojM2JhM2QwO30uY2xzLTN7ZmlsbDojNDg5MWNmO30uY2xzLTR7ZmlsbDojMTljZWQ0O30uY2xzLTV7ZmlsbDojNmI2NWNiO30uY2xzLTZ7ZmlsbDojMWZjNmQzO30uY2xzLTd7ZmlsbDojNzE1ZGNhO30uY2xzLTh7ZmlsbDojNDI5YWNmO30uY2xzLTl7ZmlsbDojNzg1NGNhO30uY2xzLTEwe2ZpbGw6IzdmNGJjOTt9LmNscy0xMXtmaWxsOiM2NDZlY2M7fS5jbHMtMTJ7ZmlsbDojNTY4MGNkO30uY2xzLTEze2ZpbGw6IzJkYjRkMTt9LmNscy0xNHtmaWxsOiMxMmQ3ZDQ7fS5jbHMtMTV7ZmlsbDojMGJlMGQ1O30uY2xzLTE2e2ZpbGw6IzI2YmRkMjt9LmNscy0xN3tmaWxsOiMzNGFiZDE7fS5jbHMtMTh7ZmlsbDojNWQ3N2NkO308L3N0eWxlPjwvZGVmcz48dGl0bGU+TWFya19Db2xvcjwvdGl0bGU+PGcgaWQ9IkxheWVyXzIiIGRhdGEtbmFtZT0iTGF5ZXIgMiI+PGcgaWQ9IkxheWVyXzEtMiIgZGF0YS1uYW1lPSJMYXllciAxIj48cG9seWdvbiBjbGFzcz0iY2xzLTEiIHBvaW50cz0iMTE0LjI1IDE4NC45NCAxNjMuMjEgMTU2LjY3IDE2My4yMSAxMDAuMTMgMTE0LjI1IDEyOC40IDExNC4yNSAxODQuOTQiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0yMjUuNDQsNjQuMmEyMi43MSwyMi43MSwwLDAsMC04LjMzLTguMzNMMTcxLjM3LDI5LjQ2LDEyNS42MiwzLjA1YTIyLjc0LDIyLjc0LDAsMCwwLTIyLjc1LDBMNTcuMTIsMjkuNDZoMGwtMzQuNTcsMjBMMTEuMzgsNTUuODdBMjIuNzYsMjIuNzYsMCwwLDAsMCw3NS41N1YxMjguNEgwdjUyLjgzYTIyLjc2LDIyLjc2LDAsMCwwLDExLjM4LDE5LjdsNDUuNzQsMjYuNDFoMGw0NS43NSwyNi40MWEyMi43NCwyMi43NCwwLDAsMCwyMi43NSwwbDQ1Ljc1LTI2LjQxLDQ1Ljc0LTI2LjQxYTIyLjc2LDIyLjc2LDAsMCwwLDExLjM4LTE5LjcxVjEyOC40aDBWNzUuNThBMjIuNjcsMjIuNjcsMCwwLDAsMjI1LjQ0LDY0LjJabS01NC4wNyw3MC4xdjI3LjA4bC01Ny4xMiwzMy01Ny4xMy0zM2gwdi02Nmw0Ny4zLTI3LjMxLDkuODMtNS42Nyw1Ny4xMiwzM1oiLz48cG9seWdvbiBjbGFzcz0iY2xzLTIiIHBvaW50cz0iMTYzLjIxIDEwMC4xMyAxNjMuMjEgMTU2LjY3IDExNC4yNSAxMjguNCAxNjMuMjEgMTAwLjEzIi8+PHBvbHlnb24gY2xhc3M9ImNscy0zIiBwb2ludHM9IjE2My4yMSAxNTYuNjcgMTE0LjI1IDE4NC45NCAxMTQuMjUgMTI4LjQgMTYzLjIxIDE1Ni42NyIvPjxwb2x5Z29uIGNsYXNzPSJjbHMtNCIgcG9pbnRzPSIxNzEuMzcgOTUuNDIgMTcxLjM3IDE2MS4zOCAyMjguNDkgMTI4LjQgMTcxLjM3IDk1LjQyIi8+PHBvbHlnb24gY2xhc3M9ImNscy0zIiBwb2ludHM9IjU3LjEyIDE2MS4zOCA1Ny4xMiAyMjcuMzQgMTE0LjI1IDE5NC4zNiA1Ny4xMiAxNjEuMzgiLz48cG9seWdvbiBjbGFzcz0iY2xzLTUiIHBvaW50cz0iNTcuMTIgMjkuNDYgNTcuMTIgOTUuNDIgMTE0LjI1IDYyLjQ0IDU3LjEyIDI5LjQ2Ii8+PHBvbHlnb24gY2xhc3M9ImNscy02IiBwb2ludHM9IjE3MS4zNyAxNjEuMzggMTE0LjI1IDE5NC4zNiAxNzEuMzcgMjI3LjM0IDE3MS4zNyAxNjEuMzgiLz48cG9seWdvbiBjbGFzcz0iY2xzLTciIHBvaW50cz0iNTcuMTIgOTUuNDIgMCAxMjguNCA1Ny4xMiAxNjEuMzggNTcuMTIgOTUuNDIiLz48cG9seWdvbiBjbGFzcz0iY2xzLTgiIHBvaW50cz0iMTcxLjM3IDI5LjQ2IDExNC4yNSA2Mi40NCAxNzEuMzcgOTUuNDIgMTcxLjM3IDI5LjQ2Ii8+PHBhdGggY2xhc3M9ImNscy05IiBkPSJNNTcuMTIsMjkuNDZsLTM0LjU3LDIwTDExLjM4LDU1Ljg3QTIyLjcxLDIyLjcxLDAsMCwwLDMuMDUsNjQuMkw1Ny4xMiw5NS40MloiLz48cGF0aCBjbGFzcz0iY2xzLTEwIiBkPSJNNTcuMTIsOTUuNDIsMy4wNSw2NC4yQTIyLjY2LDIyLjY2LDAsMCwwLDAsNzUuNTdWMTI4LjRaIi8+PHBhdGggY2xhc3M9ImNscy0xMSIgZD0iTTAsMTI4LjR2NTIuODNBMjIuNjYsMjIuNjYsMCwwLDAsMy4wNSwxOTIuNmw1NC4wNy0zMS4yMloiLz48cGF0aCBjbGFzcz0iY2xzLTEyIiBkPSJNNTcuMTIsMTYxLjM4LDMuMDUsMTkyLjZhMjIuNzksMjIuNzksMCwwLDAsOC4zMyw4LjMzbDQ1Ljc0LDI2LjQxWiIvPjxwYXRoIGNsYXNzPSJjbHMtMiIgZD0iTTU3LjEyLDIyNy4zNGw0NS43NSwyNi40MWEyMi43NywyMi43NywwLDAsMCwxMS4zOCwzLjA1VjE5NC4zNloiLz48cGF0aCBjbGFzcz0iY2xzLTEzIiBkPSJNMTE0LjI1LDI1Ni44YTIyLjc2LDIyLjc2LDAsMCwwLDExLjM3LTMuMDVsNDUuNzUtMjYuNDEtNTcuMTItMzNaIi8+PHBhdGggY2xhc3M9ImNscy0xNCIgZD0iTTE3MS4zNywxNjEuMzh2NjZsNDUuNzQtMjYuNDFhMjIuNzksMjIuNzksMCwwLDAsOC4zMy04LjMzWiIvPjxwYXRoIGNsYXNzPSJjbHMtMTUiIGQ9Ik0yMjUuNDQsMTkyLjZhMjIuNjcsMjIuNjcsMCwwLDAsMy4wNS0xMS4zOFYxMjguNGwtNTcuMTIsMzNaIi8+PHBhdGggY2xhc3M9ImNscy0xNiIgZD0iTTIyOC40OSwxMjguNFY3NS41OGEyMi42NywyMi42NywwLDAsMC0zLjA1LTExLjM4TDE3MS4zNyw5NS40MloiLz48cGF0aCBjbGFzcz0iY2xzLTE3IiBkPSJNMjI1LjQ0LDY0LjJhMjIuNzEsMjIuNzEsMCwwLDAtOC4zMy04LjMzTDE3MS4zNywyOS40NnY2NloiLz48cGF0aCBjbGFzcz0iY2xzLTEiIGQ9Ik0xNzEuMzcsMjkuNDYsMTI1LjYyLDMuMDVhMjIuNzYsMjIuNzYsMCwwLDAtMTEuMzctM1Y2Mi40NFoiLz48cGF0aCBjbGFzcz0iY2xzLTE4IiBkPSJNMTE0LjI1LDYyLjQ0VjBhMjIuNzcsMjIuNzcsMCwwLDAtMTEuMzgsMy4wNUw1Ny4xMiwyOS40NloiLz48L2c+PC9nPjwvc3ZnPg==)
[![Discord](https://img.shields.io/discord/403302085749112834?label=Discord&logo=Discord&style=for-the-badge)](https://discord.gg/mrrj45a)

# Open Source Repository for Neblio Nodes & Wallets
More information here: https://nebl.io

Alpha Builds for commits that pass testing can be found here:
https://builds.nebl.io

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
https://files.nebl.io/dependencies/mxe.tar.gz
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
./contrib/macdeploy/macdeployqtplus ./neblio-Qt.app -add-qt-tr da,de,es,hu,ru,uk,zh_CN,zh_TW -verbose 1 -rpath /usr/local/opt/qt/lib
appdmg ./contrib/macdeploy/appdmg.json ./neblio-Qt.dmg
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
  -noquicksync           If enabled, do not use quicksync, instead use regular sync
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
createrawntp1transaction [{"txid":txid,"vout":n},...] {address:{tokenid/tokenName:tokenAmount},address:neblAmount,...} '{"userData":{"meta":[{"K1":"V1"},{},...]}}' [encrypt-metadata=false]
createrawtransaction [{"txid":txid,"vout":n},...] {address:amount,...}
decoderawtransaction <hex string> [ignoreNTP1=false]
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
getblock <hash> [verbose=true] [showtxns=false] [ignoreNTP1=false]
getblockbynumber <number> [txinfo] [ignoreNTP1=false]
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
getrawtransaction <txid> [verbose=0] [ignoreNTP1=false]
getreceivedbyaccount <account> [minconf=1]
getreceivedbyaddress <neblioaddress> [minconf=1]
getstakinginfo
getsubsidy [nTarget]
gettransaction <txid> [ignoreNTP1=false]
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
sendntp1toaddress <neblioaddress> <amount> <tokenId/tokenName> '{"userData":{"meta":[{"K1":"V1"},{},...]}}' [encrypt-metadata=false] [comment] [comment-to]
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


# Responsible Vulnerability Disclosure
If you believe you've found a security issue in our product or service, we encourage you to notify us. We welcome working with skilled security researchers across the globe to resolve any issues promptly.

This Bug Bounty program is an open offer to external individuals to receive compensation for reporting Neblio bugs, specifically related to security of the core functionality of the network.

## Submissions
Submissions should be made through the following [contact page](https://nebl.io/contact)

Include following in your report:
1. Severity - Your opinion on the severity of the issue (e.g. high, moderate, low)
2. Summary - Add summary of the vulnerability
3. Description - Any additional details about this vulnerability
4. Steps - Steps to reproduce
5. Supporting Material/References - Source code to replicate, list any additional material (e.g. screenshots, logs, etc.)
6. Impact - What security impact could an attacker achieve?
7. Your name and country.

Please be available to cooperate with the Neblio Team to provide further information on the bug if needed.

## Rewards
Rewards are at the discretion of Neblio, and we will not be awarding significant bounties for low severity bugs.

## Examples of eligible bugs
### Critical
- bugs which can take full control of Neblio nodes.
- bugs which can lead to private key leakage.
- bugs which can lead to unauthorised transfer or generation of coins/NTP1 tokens.
### High
- bugs which can incur Denial of Service (DoS) in the Neblio network through P2P network.
### Medium
- bugs which can incur Denial of Service (DoS) in the Neblio network through the RPC-API.
- bugs allowing unauthorized operations on user accounts.

## Program Rules
- Vulnerabilities relating to this repository, the core software running the Neblio Network, are eligible for this program. Out-of-scope vulnerabilities impacting other software we release may be considered under this program at Neblio's discretion.
- As this is a private program, please do not discuss this program or any vulnerabilities (even resolved ones) outside of the program without express consent from the organization.
- Please provide detailed reports with reproducible steps. If the report is not detailed enough to reproduce the issue, the issue will not be eligible for a reward.
- Submit vulnerabilities only for the latest release, vulnerabilities submitted for older versions are not eligible for a reward.

Not following these rules will disqualify you from receiving any rewards.
