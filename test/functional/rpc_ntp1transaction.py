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

    def run_test(self):

        #prepare some coins for multiple *rawtransaction commands
        self.nodes[2].generate(1)
        self.sync_all()
        for i in range(3):  # mine 30 blocks, we reduce the amount per call to avoid timing out
            self.nodes[0].generate(10)
        self.sync_all()
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 1.5)
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 1.0)
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 5.0)
        output1 = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 11)
        self.nodes[0].generate(20)
        self.sync_all()

        # Test getrawtransaction on genesis block coinbase returns an error
        block = self.nodes[0].getblock(self.nodes[0].getblockhash(0))
        assert_raises_rpc_error(-5, "The genesis block coinbase is not considered an ordinary transaction", self.nodes[0].getrawtransaction, block['merkleroot'])

        txid = output1
        inputs  = [ {'txid': txid, 'vout': 0}]
        issue_raw_tx = self.nodes[0].issuenewntp1token(inputs, "XyZ", "10000", self.nodes[0].getnewaddress())
        issue_tx = self.nodes[0].decoderawtransaction(issue_raw_tx, True)
        scriptPubKey = issue_tx['vout'][0]['scriptPubKey']['asm']
        ntp1_issue_script = scriptPubKey.lstrip("OP_RETURN ")
        parsed_script = n1s.parse_ntp1_script(bytes.fromhex(ntp1_issue_script))
        assert_equal(parsed_script['protocol_version'], 3)
        assert_equal(parsed_script['script_type'], n1s.NTP1ScriptType.ISSUANCE)
        assert_equal(parsed_script['issuance_data']['token_symbol'], "XyZ")
        assert_equal(parsed_script['issuance_data']['amount'], 10000)
        assert_equal(parsed_script['issuance_data']['metadata'], b'')
        assert_equal(len(parsed_script['issuance_data']['transfer_instructions']), 1)
        assert_equal(parsed_script['issuance_data']['transfer_instructions'][0].skip_input, False)
        assert_equal(parsed_script['issuance_data']['transfer_instructions'][0].output_index, 1)
        assert_equal(parsed_script['issuance_data']['transfer_instructions'][0].amount, 10000)


        # TODO: more input tests
        # Test `issuenewntp1token` required parameters
        assert_raises_rpc_error(-1, "issuenewntp1token", self.nodes[0].issuenewntp1token)
        assert_raises_rpc_error(-1, "issuenewntp1token", self.nodes[0].issuenewntp1token, [])

        # Test `issuenewntp1token` invalid extra parameters
        assert_raises_rpc_error(-1, "value is type obj, expected str", self.nodes[0].issuenewntp1token, [], {}, 0, False, 'foo')


if __name__ == '__main__':
    RawTransactionsTest().main()
