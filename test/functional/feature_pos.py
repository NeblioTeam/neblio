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

        block_count_to_stake = 30
        # We can't stake more than the outputs we have
        assert block_count_to_stake <= outputs_count
        assert_equal(len(self.nodes[1].generatepos(1)), 0)
        for i in range(block_count_to_stake):
            hash = self.gen_pos_block(1)

        self.sync_all()

        # check that the total number of blocks is the PoW mined + PoS mined
        for n in self.nodes:
            assert_equal(n.getblockcount(), 1000 + block_count_to_stake)

        # test that combining stakes below the threshold STAKE_COMBINE_THRESHOLD will combine them
        balance_before = self.nodes[2].getbalance()
        assert_equal(balance_before, n2_utxos_to_combine_in_stake * n2_amount_per_address)
        hash_n2 = self.gen_pos_block(2)

        self.sync_all()

        # we expect all inputs to be joined in one stake, so the remaining confirmed amount is zero
        balance_after = self.nodes[2].getbalance()
        assert_equal(balance_after, Decimal('0'))

        staked_block_in_n2 = self.nodes[2].getblock(hash_n2, True, True)
        # in the staked block, transaction 1 has 'n3_amount_per_address' inputs,
        # which are all combined to create this stake
        # Combined because the age > STAKE_SPLIT_AGE and the total amount is < STAKE_COMBINE_THRESHOLD
        assert_equal(len(staked_block_in_n2['tx'][1]['vin']), n2_utxos_to_combine_in_stake)

        #
        # test that combining stakes above the threshold STAKE_COMBINE_THRESHOLD will combine them up to that threshold
        balance_before = self.nodes[3].getbalance()
        assert_equal(balance_before, n3_utxos_to_combine_in_stake * n3_amount_per_address)
        hash_n3 = self.gen_pos_block(3)

        self.sync_all()

        # we expect all inputs to be joined in one stake, except for one output
        balance_after = self.nodes[3].getbalance()
        # since we're a tick over the threshold, we expect one utxo to be left unspent in staking
        assert_equal(balance_after, n3_amount_per_address)

        staked_block_in_n3 = self.nodes[3].getblock(hash_n3, True, True)
        # in the staked block, transaction 1 has 'n3_amount_per_address' inputs,
        # which are all combined to create this stake
        # Combined because the age > STAKE_SPLIT_AGE and the total amount is < STAKE_COMBINE_THRESHOLD
        assert_equal(len(staked_block_in_n3['tx'][1]['vin']), n3_utxos_to_combine_in_stake - 1)

        # ensure that the desired output has value in it
        assert staked_block_in_n3['tx'][1]['vout'][1]['value'] > 0

        # attempt to send the staked nebls before they mature (to nodes[0])
        inputs = [{"txid": staked_block_in_n3['tx'][1]['txid'], "vout": 1}]
        outputs = {self.nodes[0].getnewaddress(): 20}
        test_maturity_rawtx = self.nodes[3].createrawtransaction(inputs, outputs)
        test_maturity_signed_rawtx = self.nodes[3].signrawtransaction(test_maturity_rawtx)
        for node in self.nodes:  # spending stake before maturity should be rejected in all nodes
            assert_raises_rpc_error(-26, "input-connect-error", node.sendrawtransaction, test_maturity_signed_rawtx['hex'])

        # we stake blocks that total to 'COINBASE_MATURITY' blocks, and the staked block to mature
        for i in range(COINBASE_MATURITY):
            # it should not be possible to submit the transaction until the maturity is reached
            assert_raises_rpc_error(-26, "input-connect-error", self.nodes[0].sendrawtransaction,
                                    test_maturity_signed_rawtx['hex'])
            hash = self.gen_pos_block(0)
        self.sync_all()

        for node in self.nodes:  # spending that stake should be accepted in all nodes after maturity
            node.sendrawtransaction(test_maturity_signed_rawtx['hex'])
        self.sync_all()

        n4_addr = self.nodes[4].getnewaddress()
        n4_utxos_to_split_in_stake = 1  # the amount we expect to be combined
        n4_amount_per_address = Decimal('2000')
        # the condition for combination; utxos will be added until we reach 'STAKE_COMBINE_THRESHOLD' nebls
        # note: The outcome can be > STAKE_COMBINE_THRESHOLD
        for i in range(n4_utxos_to_split_in_stake):
            addresses_vs_amounts_node3 = {n4_addr: n4_amount_per_address}
            tx_for_n4 = self.create_tx_with_output_amounts(self.nodes[0].listunspent(), addresses_vs_amounts_node3)
            signed_tx_for_n4 = self.nodes[0].signrawtransaction(tx_for_n4)
            self.nodes[0].sendrawtransaction(signed_tx_for_n4['hex'])

        # we stake a few blocks in nodes[0] to reach block maturity before
        blocks_to_stake = 40 * 2
        assert block_count_to_stake * STAKE_TARGET_SPACING < STAKE_SPLIT_AGE
        for _ in range(blocks_to_stake):  # 80 blocks = 80 * 30 = 40 minutes
            hash_n0 = self.gen_pos_block(0)
        self.sync_all()
        n4_balance_to_stake = Decimal('2000')
        assert_equal(self.nodes[4].getbalance(), n4_balance_to_stake)

        hash_n4 = self.gen_pos_block(4)
        staked_block_in_n4 = self.nodes[4].getblock(hash_n4, True, True)
        # 1 input should be split into two outputs because stake time is less than nStakeSplitAge
        assert_equal(len(staked_block_in_n4['tx'][1]['vin']), 1)
        assert_equal(len(staked_block_in_n4['tx'][1]['vout']), 3)  # 1 empty output + 2 split outputs
        assert_equal(staked_block_in_n4['tx'][1]['vout'][0]['value'], Decimal('0'))
        assert_equal(staked_block_in_n4['tx'][1]['vout'][1]['value'], n4_balance_to_stake/2)
        assert Decimal('1000') < staked_block_in_n4['tx'][1]['vout'][2]['value'] < Decimal('1001')

        self.sync_all()

        assert n2_balance_before < self.nodes[2].getbalance()

        # TODO: determine rewards while staking
        # the balance can never be determined because times are random
        # assert_equal(self.nodes[1].getbalance(), Decimal("110001.30160111"))
        # assert_equal(self.nodes[2].getbalance(), Decimal("1100.08679167"))
        # assert_equal(self.nodes[3].getbalance(), Decimal("120"))


if __name__ == '__main__':
    RawTransactionsTest().main()
