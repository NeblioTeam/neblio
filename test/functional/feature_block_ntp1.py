#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test block processing.

This reimplements tests from the bitcoinj/FullBlockTestGenerator used
by the pull-tester.

We use the testing framework in which we expect a particular answer from
each test.
"""
from io import BytesIO

from test_framework.test_framework import ComparisonTestFramework
from test_framework.util import *
from test_framework.comptool import TestManager, TestInstance, RejectResult
from test_framework.blocktools import *
import time
from test_framework.key import CECKey
from test_framework.script import *
from test_framework.mininode import network_thread_start
import struct
import test_framework.ntp1script as n1s
import zlib

class PreviousSpendableOutput():
    def __init__(self, tx = CTransaction(), n = -1):
        self.tx = tx
        self.n = n  # the output we're spending

#  Use this class for tests that require behavior other than normal "mininode" behavior.
#  For now, it is used to serialize a bloated varint (b64).
class CBrokenBlock(CBlock):
    def __init__(self, header=None):
        super(CBrokenBlock, self).__init__(header)

    def initialize(self, base_block):
        self.vtx = copy.deepcopy(base_block.vtx)
        self.hashMerkleRoot = self.calc_merkle_root()

    def serialize(self, with_witness=False):
        r = b""
        r += super(CBlock, self).serialize()
        r += struct.pack("<BQ", 255, len(self.vtx))
        for tx in self.vtx:
            if with_witness:
                r += tx.serialize_with_witness()
            else:
                r += tx.serialize_without_witness()
        r += ser_string(self.vchBlockSig)
        return r

    def normal_serialize(self):
        r = b""
        r += super(CBrokenBlock, self).serialize()
        return r


fee = min_fee

class FullBlockTest(ComparisonTestFramework):
    # Can either run this test as 1 node with expected answers, or two and compare them.
    # Change the "outcome" variable from each TestInstance object to only do the comparison.
    def set_test_params(self):
        self.num_nodes = 1
        node_args = ["-forksheight=first,1",
                     "-forksheight=confs_changed,2",
                     "-forksheight=tachyon,3",
                     "-forksheight=retarget_correction,4"]
        self.extra_args = [node_args] * self.num_nodes
        self.setup_clean_chain = True
        self.block_heights = {}
        self.coinbase_key = CECKey()
        self.coinbase_key.set_secretbytes(b"horsebattery")
        self.coinbase_pubkey = self.coinbase_key.get_pubkey()
        self.tip = None
        self.blocks = {}

    def add_options(self, parser):
        super().add_options(parser)
        parser.add_option("--runbarelyexpensive", dest="runbarelyexpensive", default=True)

    def run_test(self):
        self.test = TestManager(self, self.options.tmpdir)
        self.test.add_all_connections(self.nodes)
        network_thread_start()
        self.test.run()

    def add_transactions_to_block(self, block, tx_list):
        [ tx.rehash() for tx in tx_list ]
        block.vtx.extend(tx_list)

    # this is a little handier to use than the version in blocktools.py
    def create_tx(self, spend_tx, n, value, script=CScript([OP_TRUE, OP_DROP] * 15 + [OP_TRUE])):
        tx = create_transaction(spend_tx, n, b"", value, script)
        return tx

    # sign a transaction, using the key we know about
    # this signs input 0 in tx, which is assumed to be spending output n in spend_tx
    def sign_tx(self, tx, spend_tx, n):
        scriptPubKey = bytearray(spend_tx.vout[n].scriptPubKey)
        if (scriptPubKey[0] == OP_TRUE):  # an anyone-can-spend
            tx.vin[0].scriptSig = CScript()
            return
        (sighash, err) = SignatureHash(spend_tx.vout[n].scriptPubKey, tx, 0, SIGHASH_ALL)
        tx.vin[0].scriptSig = CScript([self.coinbase_key.sign(sighash) + bytes(bytearray([SIGHASH_ALL]))])

    def create_and_sign_transaction(self, spend_tx, n, value, script=CScript([OP_TRUE])):
        tx = self.create_tx(spend_tx, n, value, script)
        self.sign_tx(tx, spend_tx, n)
        tx.rehash()
        return tx

    def next_block(self, number, spend=None, additional_coinbase_value=0, script=CScript([OP_TRUE]), solve=True):
        logger.info("Creating block:".format(number))  # useful marker for debugging, marks the last block that was created
        if self.tip is None:
            base_block_hash = self.genesis_hash
            block_time = int(time.time()) + 1
        else:
            base_block_hash = self.tip.sha256
            block_time = self.tip.nTime + 1
        # First create the coinbase
        height = self.block_heights[base_block_hash] + 1
        coinbase = create_coinbase(height, self.coinbase_pubkey)
        coinbase.vout[0].nValue += additional_coinbase_value
        coinbase.rehash()
        if spend is None:
            block = create_block(base_block_hash, coinbase, block_time)
        else:
            coinbase.vout[0].nValue += fee  # add the fee to coinbase
            coinbase.rehash()
            block = create_block(base_block_hash, coinbase, block_time)
            # create a new transaction, but remove the fee from it
            amount_to_send = spend.tx.vout[spend.n].nValue - fee
            tx = create_transaction(spend.tx, spend.n, b"", amount_to_send, script)  # spend 10000000 satoshi
            self.sign_tx(tx, spend.tx, spend.n)
            self.add_transactions_to_block(block, [tx])
            block.hashMerkleRoot = block.calc_merkle_root()

        # fix block time to never be younger than transactions
        block.fix_time_then_resolve(False)

        if solve:
            # add neblio signature
            block.solve()
            block.vchBlockSig = self.coinbase_key.sign(bytes.fromhex(block.hash)[::-1])
        else:
            block.rehash()
        logger.info("Created block number {} with hash {}".format(number, block.hash))
        self.tip = block
        self.block_heights[block.sha256] = height
        assert number not in self.blocks
        self.blocks[number] = block
        return block

    def format_hash_to_str(self, hashPrevBlock):
        return hex(hashPrevBlock).replace("0x", "").zfill(len(hex(self.genesis_hash).replace("0x", "")))

    def format_block_to_string(self, block, extra_label):
        truncate_len = 12
        return self.format_hash_to_str(block.sha256)[:truncate_len] + " " + str(self.block_heights[block.sha256]) + " " + str(extra_label)

    def format_prevblock_to_string(self, block):
        truncate_len = 12
        return self.format_hash_to_str(block.hashPrevBlock)[:truncate_len] + " " + str(self.block_heights[block.hashPrevBlock])

    def create_blockchain_graph(self):
        # pygraph can be obtained from https://github.com/Shoobx/python-graph
        import pygraph
        import pygraph.classes.graph
        gr = pygraph.classes.graph.graph()
        genesis_label = "genesis"
        gr.add_node(genesis_label)
        for b in self.blocks:
            # we fill becausse integer to hex omits leading zeros
            curr_hash = self.format_block_to_string(self.blocks[b], "b"+str(b))
            gr.add_node(curr_hash)
        for b in self.blocks:
            prev_hash = self.format_prevblock_to_string(self.blocks[b])
            curr_hash = self.format_block_to_string(self.blocks[b], "b"+str(b))
            prev_hash = genesis_label if self.blocks[b].hashPrevBlock == self.genesis_hash else prev_hash
            # print("Block:", curr_hash, prev_hash, b)
            # find the prev node that contains the prev_hash
            prev_node = [label for label in gr.nodes() if prev_hash in label]
            assert len(prev_node) == 1  # only one node with that hash should be found
            gr.add_edge((curr_hash, prev_node[0]))
        return gr

    @staticmethod
    def write_graph_to_file(gr, filename):
        from pygraph.readwrite import dot, markup
        dotstr = dot.write(gr)
        with open(filename, "w") as f:
            f.write(dotstr)

    def run_genesis_block_hash_test(self):
        """
        Test that hashing of a block works fine
        Returns: nothing
        """
        genesis_block_hex = self.nodes[0].getblock(self.nodes[0].getbestblockhash(), False)
        genesis_block = CBlock()
        genesis_block_raw_io = BytesIO(bytes.fromhex(genesis_block_hex))
        genesis_block.deserialize(genesis_block_raw_io)
        genesis_block.rehash()
        assert genesis_block.hash is not None
        assert_equal(genesis_block.hash, self.nodes[0].getblockhash(0))
        assert_equal(genesis_block.hash, self.nodes[0].calculateblockhash(genesis_block.serialize().hex()))
        assert_equal(genesis_block_hex, genesis_block.serialize().hex())

    def get_tests(self):
        self.genesis_hash = int(self.nodes[0].getbestblockhash(), 16)
        self.run_genesis_block_hash_test()
        self.block_heights[self.genesis_hash] = 0
        spendable_outputs = []

        # save the current tip so it can be spent by a later block
        def save_spendable_output():
            spendable_outputs.append(self.tip)

        # get an output that we previously marked as spendable
        def get_spendable_output():
            return PreviousSpendableOutput(spendable_outputs.pop(0).vtx[0], 0)

        # returns a test case that asserts that the current tip was accepted
        def accepted():
            return TestInstance([[self.tip, True]])

        # returns a test case that asserts that the current tip was rejected
        # it DOESN'T mean the block is invalid. It means it's (still) not the new tip
        def rejected(reject = None):
            if reject is None:
                return TestInstance([[self.tip, False]])
            else:
                return TestInstance([[self.tip, reject]])

        # move the tip back to a previous block
        def tip(number):
            self.tip = self.blocks[number]

        # adds transactions to the block and updates state
        def update_block(block_number, new_transactions, update_time=True):
            logger.info("Updating block: {}".format(block_number))  # useful marker for debugging, marks the last block that was created
            block = self.blocks[block_number]
            self.add_transactions_to_block(block, new_transactions)
            old_sha256 = block.sha256
            block.hashMerkleRoot = block.calc_merkle_root()
            if update_time:
                block.fix_time_then_resolve(False)
            block.solve()
            block.vchBlockSig = self.coinbase_key.sign(bytes.fromhex(block.hash)[::-1])
            # Update the internal state just like in next_block
            self.tip = block
            if block.sha256 != old_sha256:
                self.block_heights[block.sha256] = self.block_heights[old_sha256]
                del self.block_heights[old_sha256]
            self.blocks[block_number] = block
            logger.info("Updated block {} with hash {}".format(block_number, block.hash))  # useful marker for debugging, marks the last block that was created
            return block

        # shorthand for functions
        block = self.next_block
        create_tx = self.create_tx
        create_and_sign_tx = self.create_and_sign_transaction

        # these must be updated if consensus changes
        MAX_BLOCK_SIGOPS = 20000

        # Create a new block
        block(0)
        save_spendable_output()
        yield accepted()


        # Now we need that block to mature so we can spend the coinbase.
        test = TestInstance(sync_every_block=False)
        for i in range(99):
            block(5000 + i)
            test.blocks_and_transactions.append([self.tip, True])
            save_spendable_output()
        yield test

        # collect spendable outputs now to avoid cluttering the code later on
        out = []
        for i in range(33):
            out.append(get_spendable_output())

        # Start by building a couple of blocks on top (which output is spent is
        # in parentheses):
        #     genesis -> b1 (0) -> b2 (1)
        block(1, spend=out[0])
        save_spendable_output()
        yield accepted()

        block(2, spend=out[1])
        yield accepted()
        save_spendable_output()

        # so fork like this:
        #
        #     genesis -> b1 (0) -> b2 (1)
        #                      \-> b3 (1)
        #
        # Nothing should happen at this point. We saw b2 first so it takes priority.
        tip(1)
        b3 = block(3, spend=out[1])
        txout_b3 = PreviousSpendableOutput(b3.vtx[1], 0)
        yield rejected()


        # Now we add another block to make the alternative chain longer.
        #
        #     genesis -> b1 (0) -> b2 (1)
        #                      \-> b3 (1) -> b4 (2)
        block(4, spend=out[2])
        yield accepted()


        # ... and back to the first chain.
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6 (3)
        #                      \-> b3 (1) -> b4 (2)
        tip(2)
        block(5, spend=out[2])
        save_spendable_output()
        yield rejected()

        block(6, spend=out[3])
        yield accepted()

        # let's issue one token on ensure it's mainchain
        issue_raw_tx = self.nodes[0].issuenewntp1token([{"txid": out[5].tx.hash, "vout": out[5].n}], "XyZ", "10000", self.nodes[0].getnewaddress(), "MyMetadata".encode("ascii").hex())
        signed_issue_raw_tx = self.nodes[0].signrawtransaction(issue_raw_tx)
        issue_tx_signed = CTransaction()
        issue_tx_signed.deserialize(BytesIO(bytes.fromhex(signed_issue_raw_tx['hex'])))
        self.sign_tx(issue_tx_signed, out[5].tx, out[5].n)
        issue_tx_signed.rehash()

        block(7, spend=out[4])
        update_block(7, [issue_tx_signed])
        yield accepted()

        # if we attempt to issue a token with the same symbol, it should fail
        issue_raw_tx = self.nodes[0].issuenewntp1token([{"txid": out[7].tx.hash, "vout": out[7].n}], "xyz", "100000", self.nodes[0].getnewaddress(), "OtheryMetadata".encode("ascii").hex())
        signed_issue_raw_tx = self.nodes[0].signrawtransaction(issue_raw_tx)
        issue_tx_signed = CTransaction()
        issue_tx_signed.deserialize(BytesIO(bytes.fromhex(signed_issue_raw_tx['hex'])))
        self.sign_tx(issue_tx_signed, out[7].tx, out[7].n)
        issue_tx_signed.rehash()

        block(8, spend=out[6])
        update_block(8, [issue_tx_signed])
        yield rejected(RejectResult(16, b'ntp1-error-issuance-symbol-duplicate'))


        # now we try to issue a token in another branch in the blockchain, and this should succeed
        tip(6)
        issue_raw_tx = self.nodes[0].issuenewntp1token([{"txid": out[8].tx.hash, "vout": out[8].n}], "XyZ", "10000", self.nodes[0].getnewaddress(), "MyMetadata".encode("ascii").hex())
        signed_issue_raw_tx = self.nodes[0].signrawtransaction(issue_raw_tx)
        issue_tx_signed = CTransaction()
        issue_tx_signed.deserialize(BytesIO(bytes.fromhex(signed_issue_raw_tx['hex'])))
        self.sign_tx(issue_tx_signed, out[8].tx, out[8].n)
        issue_tx_signed.rehash()

        # this won't be main chain yet, by next block it will be
        block(9, spend=out[9])
        update_block(9, [issue_tx_signed])
        yield rejected()

        # now it should be accepted
        block(10, spend=out[10])
        yield accepted()


        # add the same token twice in one block; should be rejected
        issue_raw_tx1 = self.nodes[0].issuenewntp1token([{"txid": out[11].tx.hash, "vout": out[11].n}], "XyZ1", "20000", self.nodes[0].getnewaddress(), "MyMetadata".encode("ascii").hex())
        signed_issue_raw_tx1 = self.nodes[0].signrawtransaction(issue_raw_tx1)
        issue_tx_signed1 = CTransaction()
        issue_tx_signed1.deserialize(BytesIO(bytes.fromhex(signed_issue_raw_tx1['hex'])))
        self.sign_tx(issue_tx_signed1, out[11].tx, out[11].n)
        issue_tx_signed1.rehash()

        issue_raw_tx2 = self.nodes[0].issuenewntp1token([{"txid": out[12].tx.hash, "vout": out[12].n}], "XyZ1", "30000", self.nodes[0].getnewaddress(), "MyMetadata".encode("ascii").hex())
        signed_issue_raw_tx2 = self.nodes[0].signrawtransaction(issue_raw_tx2)
        issue_tx_signed2 = CTransaction()
        issue_tx_signed2.deserialize(BytesIO(bytes.fromhex(signed_issue_raw_tx2['hex'])))
        self.sign_tx(issue_tx_signed2, out[11].tx, out[11].n)
        issue_tx_signed2.rehash()

        block(11, spend=out[13])
        update_block(11, [issue_tx_signed1, issue_tx_signed2])
        yield rejected(RejectResult(16, b'ntp1-error-issuance-symbol-duplicate'))


        # now we use the same issuance transaction on two forks, and it should work fine
        tip(10)
        block(12, spend=out[15])
        update_block(12, [issue_tx_signed1])
        yield accepted()

        # now we fork and add a block, it won't become mainchain
        tip(10)
        block(13, spend=out[16])
        update_block(13, [issue_tx_signed1])
        yield rejected()

        # now we add an arbitrary block with no special txs, and it should be accepted without issues
        block(14, spend=out[17])
        update_block(14, [])
        yield accepted()


if __name__ == '__main__':
    FullBlockTest().main()
