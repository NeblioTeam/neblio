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
        for i in range(30):
            block(i+1, spend=out[i])
            save_spendable_output()
            yield accepted()

        tip(15)
        block("f15", spend=out[15])
        save_spendable_output()
        yield rejected()

        # here we attempt to spend the same output that was spent in "f15"
        tip("f15")
        block("f16a", spend=out[15])
        save_spendable_output()
        # TODO: get an error specific message from VIU() (still not implemented in neblio-Qt)
        yield rejected(RejectResult(16, b'bad-txns-inputs-missingorspent'))

        # create one valid transaction above the last tip as a template
        tip("f15")
        height = self.block_heights[self.tip.sha256] + 1
        coinbase = create_coinbase(height, self.coinbase_pubkey)
        blk16 = CBlock()
        blk16.nTime = self.tip.nTime + 1
        blk16.hashPrevBlock = self.tip.sha256
        blk16.nBits = 0x207fffff
        blk16.vtx.append(coinbase)
        blk16.hashMerkleRoot = blk16.calc_merkle_root()
        blk16.fix_time_then_resolve()
        self.tip = blk16
        self.block_heights[blk16.sha256] = height
        self.blocks["f16b"] = blk16
        yield rejected()

        def create_tx_manual(prevout_hash, n, value):
            tx = CTransaction()
            tx.vin.append(CTxIn(COutPoint(prevout_hash, n), b'', 0xffffffff))
            tx.vout.append(CTxOut(value, CScript([OP_TRUE])))
            tx.calc_sha256()
            return tx

        tip("f16b")
        height = self.block_heights[self.tip.sha256] + 1
        coinbase = create_coinbase(height, self.coinbase_pubkey)
        blk16 = CBlock()
        blk16.nTime = self.tip.nTime + 1
        blk16.hashPrevBlock = self.tip.sha256
        blk16.nBits = 0x207fffff
        blk16.vtx.append(coinbase)
        tx1 = create_tx_manual(out[16].tx.sha256, out[16].n+10, 10000)
        blk16.vtx.append(tx1)
        blk16.hashMerkleRoot = blk16.calc_merkle_root()
        blk16.fix_time_then_resolve()
        self.tip = blk16
        self.block_heights[blk16.sha256] = height
        self.blocks["f16b"] = blk16
        yield rejected(RejectResult(16, b'bad-txns-inputs-missingorspent'))


if __name__ == '__main__':
    FullBlockTest().main()