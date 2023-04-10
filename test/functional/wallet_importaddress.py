#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet keypool and interaction with wallet encryption/locking."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

class ImportAddress(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [["-enableaccounts=1"], ["-enableaccounts=1"]]
    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,1)
    def run_test(self):
        node0 = self.nodes[0]
        node1 = self.nodes[1]

        node0_addr = node0.getnewaddress()
        # Create a wallet in node 1 retrieve one addresss an import it as watchonly inside the second one
        addresses_in_node1 = node1.getaddressesbyaccount("")
        assert_equal(node0_addr not in addresses_in_node1, True)

        # Import the address
        node1.importaddress(node0_addr)
        addresses_in_node1 = node1.getaddressesbyaccount("")
        assert_equal(node0_addr in addresses_in_node1, True)

        addr_with_label = node0.getnewaddress()
        node1.importaddress(addr_with_label, "test")
        addresses_in_node1 = node1.getaddressesbyaccount("test")
        assert_equal(addr_with_label in addresses_in_node1, True)
        
        # Test importing script
        node0_addr2 = node0.getnewaddress()
        node0_script = node0.getscriptpubkeyfromaddress(node0_addr2)

        node1.importaddress(node0_script)
        addresses_in_node1 = node1.getaddressesbyaccount("")
        assert_equal(node0_addr2 in addresses_in_node1, True)

        # P2SH case
        public_key_hash = '11695b6cd891484c2d49ec5aa738ec2b2f897777'
        push_public_key_hash = '14' + public_key_hash

        rpc_result = node0.decodescript('a9' + push_public_key_hash + '87')
        assert_equal('OP_HASH160 ' + public_key_hash + ' OP_EQUAL', rpc_result['asm'])
        node1.importaddress(rpc_result["hex"])

        addresses = rpc_result["addresses"]

        addresses_in_node1 = node1.getaddressesbyaccount("")
        for a in addresses:
            assert_equal(a in addresses_in_node1, True)

        # Close the wallet and reopen it and make sure the addresses are still there
        self.stop_node(1)
        self.start_node(1, extra_args=["-enableaccounts=1"])
        connect_nodes_bi(self.nodes, 0, 1)

        addresses_in_node1 = node1.getaddressesbyaccount("")
        assert_equal(node0_addr in addresses_in_node1, True)
        assert_equal(node0_addr2 in addresses_in_node1, True)
        assert_equal(addresses[0] in addresses_in_node1, True)

        # Make some tx to that address then generate 10 blocks for example and import address see if rescan was triggered
        # Make sure that after a rescan the balance/tx from the added account are updated
        node0_addr3 = node0.getnewaddress()
        txid = node0.sendtoaddress(node0_addr3, 153)
        node0.generate(1)
        self.sync_all()

        node1.importaddress(node0_addr3, "label-rescan", False) # Import without rescan
        balance_without_rescan = node1.getbalance("label-rescan", 1, True)
        assert_equal(balance_without_rescan, 0)

        # node1.importaddress(node0_addr3, "label-rescan", True) # Import now with rescan
        # balance_with_rescan = node1.getbalance("label-rescan", 1, True)
        # assert_equal(balance_with_rescan, 153)

        # Test the getbalance() RPC
        node0.generate(1)
        self.sync_all()
        node1_balance_with_watch_before_tx = node1.getbalance("", 1, True)
        node1_balance_without_watch_before_tx = node1.getbalance("", 1, False)
        amount = 1
        node0.sendtoaddress(node0_addr, amount, "", "")
        node0.generate(16)
        self.sync_all()
        node1_balance_with_watch_after_tx = node1.getbalance("", 1, True)
        node1_balance_without_watch_after_tx = node1.getbalance("", 1, False)

        assert_equal(node1_balance_with_watch_after_tx, node1_balance_with_watch_before_tx + amount)
        assert_equal(node1_balance_without_watch_after_tx, node1_balance_without_watch_before_tx)

        # Test on address with label
        txid = node0.sendtoaddress(addr_with_label, 100, "", "")
        node0.generate(15)
        self.sync_all()
        balance_addr_with_watch_and_label = node1.getbalance("test", 1, True)
        balance_addr_without_watch_label = node1.getbalance("test", 1, False)

        assert_equal(balance_addr_with_watch_and_label, 100)
        assert_equal(balance_addr_without_watch_label, 0)

        # Test listaccounts
        accounts_node1_with_watch = node1.listaccounts(1, True)
        accounts_node1_without_watch = node1.listaccounts(1, False)
        assert_greater_than(len(accounts_node1_with_watch), len(accounts_node1_without_watch))
        assert_greater_than_or_equal(accounts_node1_with_watch["test"], 0)

        # Test validateaddress
        validate_result = node1.validateaddress(addr_with_label)
        assert_equal(validate_result["iswatchonly"], True)

        validate_result2 = node1.validateaddress(node1.getnewaddress())
        assert_equal(validate_result2["iswatchonly"], False)

        # Test Validatepubkey
        validate_pub_result = node1.validatepubkey(validate_result2["pubkey"])
        assert_equal(validate_pub_result["iswatchonly"], False)

        validate_pub_result2 = node1.validatepubkey(node0.validateaddress(addr_with_label)["pubkey"])
        assert_equal(validate_pub_result2["iswatchonly"], True)

        # Test getwatchonlybalance on account test
        watch_balance = node1.getwatchonlybalance("test")
        assert_equal(watch_balance, 100)

        # Test gettransaction
        txs = node1.gettransaction(txid, False, True)
        assert_equal(txs['details'][0]['account'], "test")
        assert_equal(txs['details'][0]['involvesWatchonly'], True)

        # Test listtransactions
        txlist_with_watch = node1.listtransactions("test", 10, 0, True)
        assert_equal(len(txlist_with_watch), 1)
        assert_equal(txlist_with_watch[0]["involvesWatchonly"], True)
        txlist_without_watch = node1.listtransactions("test", 10, 0, False)
        assert_equal(len(txlist_without_watch), 0)

        txlist_with_watch2 = node0.listtransactions("", 10, 0, True) # On node 0
        txlist_without_watch2 = node0.listtransactions("", 10, 0, False) # On node 0
        assert_equal(txlist_with_watch2, txlist_without_watch2) # Node 0 has no watch only tx/addresses

        for tx in txlist_with_watch2:
            if "involvesWatchonly" in tx:
                assert_equal(tx["involvesWatchonly"], False)

        # Import an address on node0 then send some tx to that addr and check if listtransactions has involvesWatchonly
        node1_addr = node1.getnewaddress()
        node0.importaddress(node1_addr)
        node1.sendtoaddress(node1_addr, 10)

        node1.generate(10)
        self.sync_all()
        tx_list_with_watch = node0.listtransactions("", 10, 0, True)

        found_watch_tx = None
        for tx in tx_list_with_watch:
            if "involvesWatchonly" in tx:
                found_watch_tx = tx

        assert found_watch_tx != None
        assert_equal(found_watch_tx["involvesWatchonly"], True)

        # Test listsinceblock
        list_since_block = node0.listsinceblock("", 1, False, True)
        # There should be at least one watchonly tx
        found_watch_tx_in_last_block = False
        for txs in list_since_block["transactions"]:
            for tx in txs:
                if "involvesWatchonly" in tx:
                    found_watch_tx_in_last_block = True

        assert_equal(found_watch_tx_in_last_block, True)
        # Test listreceivedbyaddress
        list_received_addr = node0.listreceivedbyaddress(1, False, True)
        list_received_addr2 = node1.listreceivedbyaddress(1, False, True)

        found_watch_list_received = False
        found_watch_list_received2 = False

        for addr in list_received_addr:
            if addr["involvesWatchonly"] == True:
                found_watch_list_received = True
                break

        for addr in list_received_addr2:
            if addr["involvesWatchonly"] == True:
                found_watch_list_received2 = True
                break

        assert_equal(found_watch_list_received, True)
        assert_equal(found_watch_list_received2, True)


if __name__ == '__main__':
    ImportAddress().main()
