#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test staking in neblio
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from test_framework.messages import *


# Create one-input, one-output, no-fee transaction:
class RawTransactionsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 6
        self.extra_args = [[], [], [], [], [], []]

    def setup_network(self, split=False):
        super().setup_network()

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

    def gen_pos_block(self, node_number, max_retries=10, average_block_time=STAKE_TARGET_SPACING, block_time_spread=10):
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

    def create_tx_with_output_amounts(self, available_outputs, addresses_vs_amounts, fee=Decimal('0.1')):
        total_output_amount = fee
        for addr in addresses_vs_amounts:
            total_output_amount += addresses_vs_amounts[addr]
        total_input = 0
        utxos_to_be_used = []
        for input in available_outputs:
            if total_input < total_output_amount:
                if input['confirmations'] > COINBASE_MATURITY:
                    utxos_to_be_used.append(input)
                    total_input += input['amount']
            else:
                break
        if total_input < total_output_amount:
            logger.info("Attempting to reach value: {}".format(total_output_amount))
            logger.info("Available outputs: {}".format(available_outputs))
            raise ValueError("Total input could not reach the required output. Find available outputs above.")
        tx_inputs = []
        for input in utxos_to_be_used:
            tx_inputs.append({"txid": input['txid'], "vout": input['vout']})
        tx_outputs = addresses_vs_amounts
        return self.nodes[0].createrawtransaction(tx_inputs, tx_outputs)

    def run_test(self):
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

        # Create outputs in nodes[1] to stake them
        inputs = [{"txid": genesis_utxo['txid'], "vout": genesis_utxo['vout']}]

        outputs = {}
        outputs_count = 220
        for i in range(outputs_count):
            outputs[self.nodes[1].getnewaddress()] = 500
        rawTx = self.nodes[0].createrawtransaction(inputs, outputs)
        signedRawTx = self.nodes[0].signrawtransaction(rawTx)
        initial_tx_hash = self.nodes[0].sendrawtransaction(signedRawTx['hex'])

        # Create outputs in nodes[2] to stake them
        for i in range(100):  # mine 100 blocks
            hash = self.gen_pow_block(0, average_block_time, block_time_spread)
        self.sync_all()

        # create one transaction to add to the new block
        output_to_spend = 0
        inputs = [{"txid": initial_tx_hash, "vout": output_to_spend}]
        outputs = {self.nodes[2].getnewaddress(): 499}
        rawTx = self.nodes[1].createrawtransaction(inputs, outputs)
        signedRawTx = self.nodes[1].signrawtransaction(rawTx)

        # create the new block through staking
        output_to_stake = 1
        address = self.nodes[2].gettransaction(initial_tx_hash)['vout'][output_to_stake]['scriptPubKey']['addresses'][0]
        privkey = self.nodes[1].dumpprivkey(address)
        block_hex = self.nodes[2].generateblockwithkey(initial_tx_hash, output_to_stake, privkey, [signedRawTx['hex']])
        assert_equal(self.nodes[2].getblockcount(), 200)
        assert_equal(self.nodes[2].submitblock(block_hex), None)
        assert_equal(self.nodes[2].getblockcount(), 201)
        self.progress_mock_time(30)

        block_json = self.nodes[2].getblockbynumber(201)
        assert_equal(len(block_json['tx']), 3)  # stake marker + coinstake + tx we added = 3

        # let's create many more blocks!
        for i in range(100):
            output_to_stake = i + 2
            address = self.nodes[2].gettransaction(initial_tx_hash)['vout'][output_to_stake]['scriptPubKey']['addresses'][0]
            privkey = self.nodes[1].dumpprivkey(address)
            block_hex = self.nodes[2].generateblockwithkey(initial_tx_hash, output_to_stake, privkey, [])
            assert_equal(self.nodes[2].getblockcount(), 200 + i + 1)
            assert_equal(self.nodes[2].submitblock(block_hex), None)
            assert_equal(self.nodes[2].getblockcount(), 200 + i + 2)
            # test the block txs
            block_json = self.nodes[2].getblockbynumber(200 + i + 2)
            assert_equal(len(block_json['tx']), 2)  # stake marker + coinstake = 2

            self.progress_mock_time(30)

        self.sync_all()

if __name__ == '__main__':
    RawTransactionsTest().main()
