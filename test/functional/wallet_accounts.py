#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test account RPCs.

RPCs tested are:
    - getaccountaddress
    - getaddressesbyaccount
    - listaddressgroupings
    - setaccount
    - sendfrom (with account arguments)
    - move (with account arguments)
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

class WalletAccountsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[],[]]

    def run_test(self):
        node0 = self.nodes[0]
        node1 = self.nodes[1]
        # Check that there's no UTXO on any of the nodes
        assert_equal(len(node0.listunspent()), 0)

        node1.generate(10)  # generate the first block to give the reward to nodes[1]
        self.sync_all()

        # Note each time we call generate, all generated coins go into
        # the same address, so we call twice to get two addresses
        node0.generate(1)
        node0.generate(1)
        self.sync_all()
        # now we have 2 addresses, each with 2000 nebls

        # we generate 10 blocks from the other nodes, then sync,
        # in order to achieve block maturity in node0
        node1.generate(10)
        self.sync_all()

        # after maturity, node0 balance
        assert_equal(node0.getbalance(), 4000)

        # there should be 2 address groups
        # each with 1 address with a balance of 2000 nebls
        address_groups = node0.listaddressgroupings()
        assert_equal(len(address_groups), 2)

        # the addresses aren't linked now, but will be after we send to the
        # common address
        linked_addresses = set()
        for address_group in address_groups:
            assert_equal(len(address_group), 1)
            assert_equal(len(address_group[0]), 4)  # address + value + "account/address info" + NTP1 info
            assert_equal(address_group[0][1],2000)
            linked_addresses.add(address_group[0][0])

        expected_fee = 0.0002

        # send 2000 nebls from each address to a third address not in this wallet
        # There's some fee that will come back to us when the miner reward
        # matures.
        common_address = "TC2KcoGc4CCKJ2asMHA25Wbgkww8VoBF1V"
        txid = node0.sendmany(
            "",
            {common_address: 4000-expected_fee}
        )
        tx_details = node0.gettransaction(txid)
        fee = -tx_details['details'][0]['fee']

        # there should be 1 address group, with the previously
        # unlinked addresses now linked (they both have 0 balance)
        address_groups = node0.listaddressgroupings()
        assert_equal(len(address_groups), 1)
        assert_equal(len(address_groups[0]), 2)
        assert_equal(set([a[0] for a in address_groups[0]]), linked_addresses)
        assert_equal([a[1] for a in address_groups[0]], [0, 0])


if __name__ == '__main__':
    WalletAccountsTest().main()
