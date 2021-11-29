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
        self.nodes[0].sendrawtransaction(signedRawTx['hex'])

        # Create outputs in nodes[2] to stake them
        for i in range(100):  # mine 100 blocks
            hash = self.gen_pow_block(0, average_block_time, block_time_spread)
        self.sync_all()

        # Here we create the outputs in nodes[2] that should be combined
        n2_addr = self.nodes[2].getnewaddress()
        n2_utxos_to_combine_in_stake = 10
        n2_amount_per_address = Decimal('110')
        # the condition for combination; utxos will be added until we reach 'STAKE_COMBINE_THRESHOLD' nebls
        # note: The outcome can be > STAKE_COMBINE_THRESHOLD
        assert (n2_utxos_to_combine_in_stake - 1) * n2_amount_per_address <= STAKE_COMBINE_THRESHOLD
        for i in range(n2_utxos_to_combine_in_stake):
            addresses_vs_amounts_node2 = {n2_addr: n2_amount_per_address}
            tx_for_n2 = self.create_tx_with_output_amounts(self.nodes[0].listunspent(), addresses_vs_amounts_node2)
            signed_tx_for_n2 = self.nodes[0].signrawtransaction(tx_for_n2)
            self.nodes[0].sendrawtransaction(signed_tx_for_n2['hex'])

        # Here we create the outputs in nodes[2] that should be combined, except for one output
        n3_addr = self.nodes[3].getnewaddress()
        n3_utxos_to_combine_in_stake = 10  # the amount we expect to be combined
        n3_amount_per_address = Decimal('120')
        # the condition for combination; utxos will be added until we reach 'STAKE_COMBINE_THRESHOLD' nebls
        # note: The outcome can be > STAKE_COMBINE_THRESHOLD
        assert (n3_utxos_to_combine_in_stake - 1) * n3_amount_per_address > STAKE_COMBINE_THRESHOLD
        for i in range(n3_utxos_to_combine_in_stake):
            addresses_vs_amounts_node3 = {n3_addr: n3_amount_per_address}
            tx_for_n3 = self.create_tx_with_output_amounts(self.nodes[0].listunspent(), addresses_vs_amounts_node3)
            signed_tx_for_n3 = self.nodes[0].signrawtransaction(tx_for_n3)
            self.nodes[0].sendrawtransaction(signed_tx_for_n3['hex'])

        for i in range(800):  # mine 800 blocks
            hash = self.gen_pow_block(0, average_block_time, block_time_spread)
        self.sync_all()

        n1_balance_before = self.nodes[1].getbalance()
        n2_balance_before = self.nodes[2].getbalance()
        n3_balance_before = self.nodes[3].getbalance()

        # move to the future to make coins stakable (this is not needed because 10 minutes is too short)
        # self.progress_mock_time(60*10)

        LAST_POW_BLOCK = 1000

        votes = self.nodes[0].listvotes()
        assert_equal(len(votes), 0)

        block_count_to_stake = 30
        # We can't stake more than the outputs we have
        assert block_count_to_stake <= outputs_count
        for i in range(block_count_to_stake):
            hash = self.gen_pos_block(1)

        self.sync_all()

        # check that the total number of blocks is the PoW mined + PoS mined
        for n in self.nodes:
            assert_equal(n.getblockcount(), LAST_POW_BLOCK + block_count_to_stake)

        # check that votes are empty
        for i in range(block_count_to_stake):
            blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + i + 1)
            assert_equal(blk_data['votevalue'], None)

        self.nodes[0].castvote(LAST_POW_BLOCK + block_count_to_stake + 5,
                               LAST_POW_BLOCK + block_count_to_stake + 10,
                               999,
                               11)
        self.nodes[0].castvote(LAST_POW_BLOCK + 5,
                               LAST_POW_BLOCK + 10,
                               888,
                               15)
        self.nodes[0].castvote(LAST_POW_BLOCK + block_count_to_stake + 500,
                               LAST_POW_BLOCK + block_count_to_stake + 510,
                               777,
                               33)

        next_block_count_to_stake = 15
        for i in range(next_block_count_to_stake):
            hash = self.gen_pos_block(0)

        # now we ensure that only the correct block range has the vote with the correct values
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 1)
        assert_equal(blk_data['votevalue'], None)
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 2)
        assert_equal(blk_data['votevalue'], None)
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 3)
        assert_equal(blk_data['votevalue'], None)
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 4)
        assert_equal(blk_data['votevalue'], None)
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 5)
        assert_equal(blk_data['votevalue'], {'ProposalID': 999, 'VoteValue': 11})
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 6)
        assert_equal(blk_data['votevalue'], {'VoteValue': 11, 'ProposalID': 999})
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 7)
        assert_equal(blk_data['votevalue'], {'ProposalID': 999, 'VoteValue': 11})
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 8)
        assert_equal(blk_data['votevalue'], {'VoteValue': 11, 'ProposalID': 999})
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 9)
        assert_equal(blk_data['votevalue'], {'ProposalID': 999, 'VoteValue': 11})
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 10)
        assert_equal(blk_data['votevalue'], {'VoteValue': 11, 'ProposalID': 999})
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 11)
        assert_equal(blk_data['votevalue'], None)
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 12)
        assert_equal(blk_data['votevalue'], None)
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 13)
        assert_equal(blk_data['votevalue'], None)
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 14)
        assert_equal(blk_data['votevalue'], None)
        blk_data = self.nodes[0].getblockbynumber(LAST_POW_BLOCK + block_count_to_stake + 15)
        assert_equal(blk_data['votevalue'], None)


if __name__ == '__main__':
    RawTransactionsTest().main()
