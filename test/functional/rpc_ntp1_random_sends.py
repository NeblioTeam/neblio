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
import copy


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

    def make_few_ntp1_tokens_data(self, count: int):
        resulting_coins = []
        symbols = {}  # used to ensure no duplicates exist
        while len(resulting_coins) < count:
            metadata_len = random.randint(0, 5)
            metadata = bytes.fromhex(''.join(random.choices(string.hexdigits + string.digits,
                                                            k=2*metadata_len)))
            amount = str(random.randint(1, 100)*1000)
            token_symbol_length = random.randint(1, 5)
            token_symbol = ''.join(random.choices(string.ascii_letters + string.digits,
                                                  k=token_symbol_length))
            # ensure no token symbol duplicates will ever exist
            if token_symbol.lower() in [s.lower() for s in symbols]:
                continue
            symbols[token_symbol] = 0
            resulting_coins.append({"metadata": metadata,
                                    "amount": amount,
                                    "symbol": token_symbol})
        print("Issued coins:", resulting_coins)
        return resulting_coins

    def issue_ntp1_token(self, node_id: int, inputs: list, token_name: str, amount: str, destination: str, metadata: bytes):
        issue_raw_tx = self.nodes[node_id].issuenewntp1token(inputs, token_name, amount, destination, metadata.hex())
        signed_issue_raw_tx = self.nodes[node_id].signrawtransaction(issue_raw_tx)
        assert signed_issue_raw_tx['complete'] is True
        signed_issue_raw_tx_hash = self.nodes[node_id].sendrawtransaction(signed_issue_raw_tx['hex'])
        return signed_issue_raw_tx_hash

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
                result[tokenID] = copy.deepcopy(current_balances[tokenID])
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

    def create_expected_balances(self, source_node_id, initial_balances_of_nodes, transfers):
        new_balances = copy.deepcopy(initial_balances_of_nodes)
        for node_id in transfers:
            for transfer in transfers[node_id]:
                if node_id == source_node_id:
                    # if we transfer from and to the same node, do nothing
                    continue
                tokenID = transfer['TokenId']
                balance_transferred = transfer['Balance']
                balance_transferred_int = int(balance_transferred)
                current_source_balance = new_balances[source_node_id][tokenID]['Balance']
                current_source_balance_int = int(current_source_balance)
                # the target node may or may not have that token before
                # if not, just make its balance in that zero
                if tokenID not in new_balances[node_id]:
                    new_balances[node_id][tokenID] = copy.deepcopy(new_balances[source_node_id][tokenID])
                    new_balances[node_id][tokenID]['Balance'] = '0'
                current_target_balance = new_balances[node_id][tokenID]['Balance']
                current_target_balance_int = int(current_target_balance)
                new_source_balance = str(current_source_balance_int - balance_transferred_int)
                new_target_balance = str(current_target_balance_int + balance_transferred_int)
                new_balances[source_node_id][tokenID]['Balance'] = new_source_balance
                new_balances[node_id][tokenID]['Balance'] = new_target_balance
        return new_balances

    def get_current_ntp1_balances_per_node(self):
        balances = []
        for node in self.nodes:
            balances.append(node.getntp1balances())
        return balances

    @staticmethod
    def compare_balances(current, expected):
        assert_equal(len(current), len(expected))
        for i in range(len(current)):
            # we loop both ways to ensure that an element in first should be in second
            # or should be zero
            for tokenID in current[i]:
                current_b = current[i][tokenID]['Balance']
                if tokenID not in expected[i]:
                    assert assert_equal(current_b, '0')
                else:
                    expected_b = expected[i][tokenID]['Balance']
                    assert_equal(current_b, expected_b)

            for tokenID in expected[i]:
                expected_b = expected[i][tokenID]['Balance']
                if tokenID not in current[i]:
                    assert assert_equal(expected_b, '0')
                else:
                    current_b = current[i][tokenID]['Balance']
                    assert_equal(current_b, expected_b)

    def run_test(self):
        n1s.run_all_local_tests()
        # prepare some coins
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

        # test random sending of tokens
        tokens_to_issue_count = 5
        tokens_to_issue = self.make_few_ntp1_tokens_data(tokens_to_issue_count)
        assert len(tokens_to_issue) == tokens_to_issue_count
        issued_tokens = {}
        for i in range(len(tokens_to_issue)):
            output_index = 2+i  # 2 were used, + the index we're in now
            node_id = 1
            txid = self.issue_ntp1_token(node_id,
                                         [{'txid': spendable_tx[0], 'vout': spendable_tx[1][node_id][output_index]}],
                                         tokens_to_issue[i]['symbol'],
                                         tokens_to_issue[i]['amount'],
                                         self.nodes[node_id].getnewaddress(),
                                         tokens_to_issue[i]['metadata']
                                         )
            symbol = tokens_to_issue[i]['symbol']
            issued_tokens[symbol] = (txid, tokens_to_issue[i])

        node_to_send_from = 1
        self.nodes[0].generate(5)
        sync_blocks(self.nodes)

        balances_before_send = self.get_current_ntp1_balances_per_node()

        for token in tokens_to_issue:
            symbol = token['symbol']
            metadata = token['metadata']
            amount = token['amount']
            assert symbol in issued_tokens
            assert symbol == issued_tokens[symbol][1]['symbol']
            assert metadata == issued_tokens[symbol][1]['metadata']
            assert amount == issued_tokens[symbol][1]['amount']

            # test the result from getntp1balances()
            found = False
            for tokenID in balances_before_send[node_to_send_from]:
                if balances_before_send[node_to_send_from][tokenID]['Name'] == symbol:
                    found = True
                    assert balances_before_send[node_to_send_from][tokenID]['Balance'] == amount
            assert found

        # for multiple rounds we transfer randomly from one node to others
        rounds_count = 8
        for i in range(rounds_count):
            print("Round {} in transfers:".format(i))

            balances_before_send = self.get_current_ntp1_balances_per_node()
            print("Balances before transfer", balances_before_send)

            transfers = self.pick_random_amounts_and_send(balances_before_send[node_to_send_from], 0.7)
            print("Planned transfers:", transfers)

            transferred = self.send_transactions_for_token_transfers(node_to_send_from, transfers)
            print("Transferred that happened:", transferred)

            # generate blocks to ensure sends are in the blockchain
            for node in self.nodes:
                node.generate(1)
                sync_blocks(self.nodes)

            new_expected_balances = self.create_expected_balances(node_to_send_from, balances_before_send, transferred)
            print("New expected balances:", new_expected_balances)

            new_balances = self.get_current_ntp1_balances_per_node()

            self.compare_balances(new_balances, new_expected_balances)
            print("Test for round {} succeeded".format(i))

            # source node for nexst round
            node_to_send_from = random.randint(0, self.num_nodes - 1)


if __name__ == '__main__':
    RawTransactionsTest().main()
