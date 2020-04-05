#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the rawtransaction RPCs.

Test the following RPCs:
   - createrawtransaction
   - signrawtransaction
   - sendrawtransaction
   - decoderawtransaction
   - getrawtransaction
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *


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
        self.extra_args = [["-addresstype=legacy"], ["-addresstype=legacy"], ["-addresstype=legacy"]]

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,2)

    def progress_mock_time(self, by_how_many_seconds):
        assert self.curr_time is not None
        self.curr_time += by_how_many_seconds
        for n in self.nodes:
            n.setmocktime(self.curr_time)

    def reset_mock_time(self, value=None):
        for n in self.nodes:
            if value is None:
                self.curr_time = int(time.time())
                n.setmocktime(self.curr_time)
            else:
                assert type(value) == int
                self.curr_time = value
                n.setmocktime(self.curr_time)

    def gen_pos_block(self, node_number, max_retries=10, average_block_time=30, block_time_spread=10):
        r = random.randrange(-block_time_spread, block_time_spread + 1)
        self.progress_mock_time(average_block_time - self.last_random_time_offset + r)
        self.last_random_time_offset = r
        balance = self.nodes[node_number].getbalance()
        if balance == 0:
            raise ValueError("Node has no balance to stake")
        for i in range(max_retries):
            hashes = self.nodes[node_number].generatepos(1)
            if len(hashes) > 0:
                return hashes[0]
            else:
                # we progress in time to provide some window for nSearchTime vs nLastCoinStakeSearchTime
                self.progress_mock_time(1)
        raise CalledProcessError("Failed to stake. Max tries limit reached.")

    def gen_pow_block(self, node_number, average_block_time, block_time_spread):
        hashes = self.nodes[node_number].generate(1)
        assert_equal(len(hashes), 1)
        r = random.randrange(-block_time_spread, block_time_spread + 1)
        self.progress_mock_time(average_block_time - self.last_random_time_offset + r)
        self.last_random_time_offset = r
        return hashes[0]

    def run_test(self):

        #prepare some coins for multiple *rawtransaction commands

        self.sync_all()
        self.reset_mock_time()
        self.last_random_time_offset = 0
        block_time_spread = 10
        average_block_time = 30
        for i in range(100):  # mine 100 blocks, we reduce the amount per call to avoid timing out
            hash = self.gen_pow_block(0, average_block_time, block_time_spread)
        # find the output that has the genesis block reward
        listunspent = self.nodes[0].listunspent()
        genesis_utxo = None
        for utxo_data in listunspent:
            if utxo_data['amount'] == Decimal('124000000.00000000'):
                genesis_utxo = utxo_data
                break
        assert genesis_utxo is not None

        # send the genesis reward in chunks to nodes[1] for staking later
        inputs = [{"txid": genesis_utxo['txid'], "vout": genesis_utxo['vout']}]
        outputs = {}
        outputs_count = 220
        for i in range(outputs_count):
            outputs[self.nodes[1].getnewaddress()] = 500
        rawTx = self.nodes[0].createrawtransaction(inputs, outputs)
        signedRawTx = self.nodes[0].signrawtransaction(rawTx)
        self.nodes[0].sendrawtransaction(signedRawTx['hex'])

        for i in range(900):  # mine 900 blocks, we reduce the amount per call to avoid timing out
            hash = self.gen_pow_block(0, average_block_time, block_time_spread)
        self.sync_all()

        # move to the future to make coins stakable (this is not needed because 10 minutes is too short)
        # self.progress_mock_time(60*10)

        block_count_to_stake = 220
        # We can't stake more than the outputs we have
        assert block_count_to_stake <= outputs_count
        assert_equal(len(self.nodes[1].generatepos(1)), 0)
        for i in range(block_count_to_stake):
            hash = self.gen_pos_block(1)

        self.sync_all()

        # check that the total number of blocks is the PoW mined + PoS mined
        for n in self.nodes:
            assert_equal(n.getblockcount(), 1000 + block_count_to_stake)


if __name__ == '__main__':
    RawTransactionsTest().main()
