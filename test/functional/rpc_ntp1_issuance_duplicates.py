#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the rawtransaction RPCs.

Test the following RPCs:
   - issuenewntp1token
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
import test_framework.ntp1script as n1s
import zlib
import itertools
import string


class multidict(dict):
    """Dictionary that allows duplicate keys.

    Constructed with a list of (key, value) tuples. When dumped by the json module,
    will output invalid json with repeated keys, eg:
    >>> json.dumps(multidict([(1,2),(1,2)])
    '{"1": 2, "1": 2}'

    Used to test calls to rpc methods with repeated keys in the json object."""

    def __init__(self, x):
        dict.__init__(self, x)
        self.x = x

    def items(self):
        return self.x


# Create one-input, one-output, no-fee transaction:
class RawTransactionsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        node_args = ["-addresstype=legacy",
                     "-forksheight=first,10",
                     "-forksheight=confs_changed,20",
                     "-forksheight=tachyon,25",
                     "-forksheight=retarget_correction,30"]
        self.extra_args = [node_args] * self.num_nodes

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,2)

    def get_spendable_outputs(self):
        self.nodes[0].generate(30)
        unspent_outputs = self.nodes[0].listunspent()

        # Create outputs in nodes[1] to stake them
        inputs = []
        for unspent in unspent_outputs:
            inputs.append({"txid": unspent['txid'], "vout": unspent['vout']})

        # we map node ids to output indices in this transaction
        node_to_output = {}
        outputs = {}
        outputs_count = 100
        for node_id in range(self.num_nodes):
            node_to_output[node_id] = []
            for i in range(outputs_count):
                node_to_output[node_id].append(len(outputs))
                outputs[self.nodes[node_id].getnewaddress()] = 11
        rawTx = self.nodes[0].createrawtransaction(inputs, outputs)
        signedRawTx = self.nodes[0].signrawtransaction(rawTx)
        return (self.nodes[0].sendrawtransaction(signedRawTx['hex']), node_to_output)

    '''
    Given a list of balances, create a random list of transfers
    '''
    def pick_random_amounts_and_send(self, current_balances, probability_to_pick):
        assert probability_to_pick >= 0
        assert probability_to_pick <= 1
        result = {}
        for tokenID in current_balances:
            if random.uniform(0, 1) < probability_to_pick:
                result[tokenID] = current_balances[tokenID]
                # pick a random amount to transfer
                bal = result[tokenID]['Balance']
                result[tokenID]['Balance'] = str(random.randint(1, int(bal)))
        return result

    '''
    Given a list of tokens to transfer from a source node, do these transfers,
    and return a list of what transfers were done (node in dict key vs list tokens transferred)
    '''
    def send_transactions_for_token_transfers(self, source_node_id, tokens_to_transfer: dict):
        # get the list of available nodes
        nodes_ids = list(range(0, self.num_nodes))
        assert source_node_id in nodes_ids
        node_vs_received = {}
        for transferTID in tokens_to_transfer:
            target_node = random.choice(nodes_ids)
            txid = self.nodes[source_node_id].sendntp1toaddress(
                self.nodes[target_node].getnewaddress(),
                int(tokens_to_transfer[transferTID]['Balance']),
                transferTID)
            if target_node not in node_vs_received:
                node_vs_received[target_node] = []
            node_vs_received[target_node].append(tokens_to_transfer[transferTID])
        return node_vs_received

    def run_test(self):
        n1s.run_all_local_tests()
        # prepare some coins for multiple *rawtransaction commands
        self.nodes[0].generate(1)
        spendable_tx = self.get_spendable_outputs()
        self.nodes[2].generate(1)
        self.sync_all()
        for i in range(3):  # mine 30 blocks, we reduce the amount per call to avoid timing out
            self.nodes[0].generate(10)
        self.sync_all()

        # Test getrawtransaction on genesis block coinbase returns an error
        block = self.nodes[0].getblock(self.nodes[0].getblockhash(0))
        assert_raises_rpc_error(-5, "The genesis block coinbase is not considered an ordinary transaction", self.nodes[0].getrawtransaction, block['merkleroot'])

        # we issue the same token few times, but with different balances to ensure different transactions
        inputs1 = [{'txid': spendable_tx[0], 'vout': spendable_tx[1][0][0]}]
        inputs2 = [{'txid': spendable_tx[0], 'vout': spendable_tx[1][0][1]}]
        inputs3 = [{'txid': spendable_tx[0], 'vout': spendable_tx[1][0][2]}]
        issue_tx_dest = self.nodes[0].getnewaddress()
        issue_raw_tx1 = self.nodes[0].issuenewntp1token(inputs1, "XyZ", "10001", issue_tx_dest, "MyMetadata".encode("ascii").hex())
        issue_raw_tx2 = self.nodes[0].issuenewntp1token(inputs2, "xyz", "10002", issue_tx_dest, "MyMetadata".encode("ascii").hex())
        issue_raw_tx3 = self.nodes[0].issuenewntp1token(inputs3, "XyZ", "10003", issue_tx_dest, "MyMetadata".encode("ascii").hex())
        signed_issue_raw_tx1 = self.nodes[0].signrawtransaction(issue_raw_tx1)
        signed_issue_raw_tx2 = self.nodes[0].signrawtransaction(issue_raw_tx2)
        signed_issue_raw_tx3 = self.nodes[0].signrawtransaction(issue_raw_tx3)

        # only the first transaction should be accepted to the mempool
        signed_issue_raw_tx_hash = self.nodes[0].sendrawtransaction(signed_issue_raw_tx1['hex'])
        assert_raises_rpc_error(-26, "txn-ntp1-mempool-conflict", self.nodes[0].sendrawtransaction, signed_issue_raw_tx2['hex'])
        assert_raises_rpc_error(-26, "txn-ntp1-mempool-conflict", self.nodes[0].sendrawtransaction, signed_issue_raw_tx3['hex'])

        # generate some blocks to get this issuance mined
        self.nodes[0].generate(10)

        # get the balance (with 1 confirmation), and ensure that only one issuance happened
        ntp1balances = self.nodes[0].getntp1balances(1)

        # only one token should be there
        assert_equal(len(ntp1balances), 1)
        # the symbol of that token is XyZ
        assert_equal(list(ntp1balances.items())[0][1]['Name'], 'XyZ')
        # one of the balance values should be in there, based on which transaction made it in first
        assert_equal(list(ntp1balances.items())[0][1]['Balance'], '10001')

        self.nodes[0].generate(10)
        sync_blocks(self.nodes)

        ntp1balances = self.nodes[0].getntp1balances(1)
        assert_equal(len(ntp1balances), 1)


if __name__ == '__main__':
    RawTransactionsTest().main()
