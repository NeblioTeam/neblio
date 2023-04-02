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


if __name__ == '__main__':
    ImportAddress().main()
