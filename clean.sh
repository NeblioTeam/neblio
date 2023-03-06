#!/bin/bash

echo "Are you sure you want to delete the neblio data directory?"
read -p "Close neblio app and press enter to continue... "

[ -e ~/.neblio ] && rm -r ~/.neblio
mkdir ~/.neblio

cat <<EOT > ~/.neblio/neblio.conf
regtest=1
server=1
port=9999
rpcport=6326
listenonion=0
rpcuser=user
rpcpassword=password
EOT

read -p "Start neblio and paste your address here: " ADDR

curl --location '127.0.0.1:6326' \
--header 'Authorization: Basic dXNlcjpwYXNzd29yZA==' \
--header 'Content-Type: application/json' \
--data "{
    \"jsonrpc\": \"2.0\",
    \"id\": 2,
    \"method\": \"generatetoaddress\",
    \"params\":[10, \"$ADDR\"]
}"

sleep 1

curl --location '127.0.0.1:6326' \
--header 'Authorization: Basic dXNlcjpwYXNzd29yZA==' \
--header 'Content-Type: application/json' \
--data "{
    \"jsonrpc\": \"2.0\",
    \"id\": 2,
    \"method\": \"generatetoaddress\",
    \"params\":[10, \"$ADDR\"]
}"
