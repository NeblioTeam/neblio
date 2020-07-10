#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Copyright (c) 2020 The Neblio developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
# -*- coding: utf-8 -*-

from io import BytesIO
from time import sleep

from test_framework.authproxy import JSONRPCException
from test_framework.messages import CTransaction, CTxIn, CTxOut, COIN, COutPoint, CBlock
from test_framework.mininode import network_thread_start
from test_framework.script import CScript, OP_CHECKSIG
from test_framework.address import *
from test_framework.key import *
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import connect_nodes_bi, p2p_port, bytes_to_hex_str, \
    assert_equal, assert_greater_than, sync_blocks, assert_raises_rpc_error, hex_str_to_bytes

import random
import time

# filter utxos based on first 5 bytes of scriptPubKey
def getDelegatedUtxos(utxos):
    return [x for x in utxos if x["scriptPubKey"][:10] == '76a97b63d1']


STAKE_TARGET_SPACING = 30  # target seconds between blocks

class ColdStakingTest(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 4
        self.extra_args = [[], [], [], []]

    def setup_network(self):
        ''' Can't rely on syncing all the nodes when staking=1
        '''
        # self.setup_nodes()
        # for i in range(self.num_nodes - 1):
        #     for j in range(i+1, self.num_nodes):
        #         connect_nodes_bi(self.nodes, i, j)
        super().setup_network()

    def gen_pos_block(self, node_number, submit_block=True, inputs_to_use=None, extra_payout=0, max_retries=10, average_block_time=STAKE_TARGET_SPACING, block_time_spread=10):
        r = random.randrange(-block_time_spread, block_time_spread + 1)
        self.progress_mock_time(average_block_time - self.last_random_time_offset + r)
        self.last_random_time_offset = r
        staking_outputs = self.nodes[node_number].getstakinginfo()["stakableoutputs"]
        if staking_outputs == 0:
            raise ValueError("Node has no outputs to stake")
        for i in range(max_retries):
            hashes = self.nodes[node_number].generatepos(1, submit_block, inputs_to_use, extra_payout)
            if len(hashes) > 0:
                return hashes[0]
            else:
                # we progress in time to provide some window for nSearchTime vs nLastCoinStakeSearchTime
                self.progress_mock_time(1)
        raise ValueError("Failed to stake. Max tries limit reached.")

    def gen_pow_block(self, node_number, average_block_time, block_time_spread):
        hashes = self.nodes[node_number].generate(1)
        assert_equal(len(hashes), 1)
        r = random.randrange(-block_time_spread, block_time_spread + 1)
        self.progress_mock_time(average_block_time - self.last_random_time_offset + r)
        self.last_random_time_offset = r
        return hashes[0]

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

    @staticmethod
    def wif_privkey_to_eckey(wif_key):
        privKey, compressed = wif_to_privkey(wif_key)
        eckey = CECKey()
        eckey.set_secretbytes(bytes.fromhex(privKey))
        eckey.set_compressed(compressed)
        return eckey

    @staticmethod
    def get_pubkey_from_p2cs_scriptSig(scriptSig):
        s = CScript(scriptSig)
        it = s.__iter__()
        next(it)  # skip sig
        next(it)  # skip OP_TRUE/OP_FALSE
        pub_key = next(it)
        return pub_key[:]

    def run_test(self):
        self.sync_all()
        self.reset_mock_time()
        self.last_random_time_offset = 0
        block_time_spread = 10
        average_block_time = 30
        self.DEFAULT_FEE = 3*0.0001

        self.description = "Performs tests on the Cold Staking P2CS implementation"
        LAST_POW_BLOCK = 30
        NUM_OF_INPUTS = 20
        INPUT_VALUE = 50
        INITIAL_MINED_BLOCKS = LAST_POW_BLOCK + 1

        # nodes[0] - coin-owner
        # nodes[1] - cold-staker

        # 1) nodes[0] mines 20 blocks. nodes[2] mines 980 blocks.
        # -----------------------------------------------------------
        print("*** 1 ***")
        self.log.info("Mining %d blocks..." % INITIAL_MINED_BLOCKS)
        for i in range(20):
            hash = self.gen_pow_block(3, average_block_time, block_time_spread)

        # find the output that has the genesis block reward
        self.nodes[3].sendtoaddress(self.nodes[0].getnewaddress(), 10000)
        sync_blocks(self.nodes)
        self.log.info("20 Blocks mined.")
        for i in range(INITIAL_MINED_BLOCKS - 20):
            hash = self.gen_pow_block(2, average_block_time, block_time_spread)
            if i % 200 == 0:
                sync_blocks(self.nodes)
        sync_blocks(self.nodes)
        listunspent = self.nodes[0].listunspent()
        self.log.info(str(INITIAL_MINED_BLOCKS) + " Blocks mined.")

        # 2) nodes[0] generates a owner address
        #    nodes[1] generates a cold-staking address.
        # ---------------------------------------------
        print("*** 2 ***")
        owner_address = self.nodes[0].getnewaddress()
        self.log.info("Owner Address: %s" % owner_address)
        staker_address = self.nodes[1].getnewaddress()
        staker_privkey = self.nodes[1].dumpprivkey(staker_address)
        self.log.info("Staking Address: %s" % staker_address)

        # # 3) Check enforcement.
        # # ---------------------
        # print("*** 3 ***")
        # self.log.info("Creating a stake-delegation tx before cold staking enforcement...")
        # assert_raises_rpc_error(-4, "The transaction was rejected!",
        #                         self.nodes[0].delegatestake, staker_address, INPUT_VALUE, owner_address, False, False, True)
        # self.log.info("Good. Cold Staking NOT ACTIVE yet.")
        #
        # # Enable SPORK
        # self.setColdStakingEnforcement()
        # # double check
        # assert (self.isColdStakingEnforced())

        # 4) nodes[0] delegates a number of inputs for nodes[1] to stake em.
        # ------------------------------------------------------------------
        print("*** 4 ***")
        self.log.info("First check warning when using external addresses...")
        assert_raises_rpc_error(-5, "Only the owner of the key to owneraddress will be allowed to spend these coins",
                                self.nodes[0].delegatestake, staker_address, INPUT_VALUE, "TNuyq5dXf4dapmXg8XUSgF1zCdVWNY5mk3")
        self.log.info("Good. Warning triggered.")

        self.log.info("Now force the use of external address creating (but not sending) the delegation...")
        res = self.nodes[0].rawdelegatestake(staker_address, INPUT_VALUE, "TNuyq5dXf4dapmXg8XUSgF1zCdVWNY5mk3", True)
        assert(res is not None and res != "")
        self.log.info("Good. Warning NOT triggered.")

        self.log.info("Now delegate with internal owner address..")
        self.log.info("Try first with a value (0.99) below the threshold")
        assert_raises_rpc_error(-8, "Invalid amount",
                                self.nodes[0].delegatestake, staker_address, 9.99, owner_address)
        self.log.info("Nice. it was not possible.")
        self.log.info("Then try (creating but not sending) with the threshold value (1.00)")
        res = self.nodes[0].rawdelegatestake(staker_address, 10.00, owner_address)
        assert(res is not None and res != "")
        self.log.info("Good. Warning NOT triggered.")

        self.log.info("Now creating %d real stake-delegation txes..." % NUM_OF_INPUTS)
        for i in range(NUM_OF_INPUTS):
            res = self.nodes[0].delegatestake(staker_address, INPUT_VALUE, owner_address)
            assert(res != None and res["txid"] != None and res["txid"] != "")
            assert_equal(res["owner_address"], owner_address)
            assert_equal(res["staker_address"], staker_address)
        hash = self.gen_pow_block(0, average_block_time, block_time_spread)
        sync_blocks(self.nodes)
        self.log.info("%d Txes created." % NUM_OF_INPUTS)
        # check balances:
        self.expected_balance = NUM_OF_INPUTS * INPUT_VALUE
        self.expected_immature_balance = 0
        self.checkBalances()

        # 5) check that the owner (nodes[0]) can spend the coins.
        # -------------------------------------------------------
        print("*** 5 ***")
        self.log.info("Spending back one of the delegated UTXOs...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_equal(20, len(delegated_utxos))
        assert_equal(len(delegated_utxos), len(self.nodes[0].listcoldutxos()))
        u = delegated_utxos[0]
        txhash = self.spendUTXOwithNode(u, 0)
        assert(txhash != None)
        self.log.info("Good. Owner was able to spend - tx: %s" % str(txhash))

        hash = self.gen_pow_block(0, average_block_time, block_time_spread)
        sync_blocks(self.nodes)
        # check balances after spend.
        self.expected_balance -= float(u["amount"])
        self.checkBalances()
        self.log.info("Balances check out after spend")
        assert_equal(19, len(self.nodes[0].listcoldutxos()))

        # 6) check that the staker CANNOT use the coins to stake yet.
        # He needs to whitelist the owner first.
        # -----------------------------------------------------------
        print("*** 6 ***")
        self.log.info("Trying to generate a cold-stake block before whitelisting the owner...")
        assert_equal(self.nodes[1].getstakinginfo()["stakableoutputs"], 0)
        self.log.info("Nice. Cold staker was NOT able to create the block yet.")

        self.log.info("Whitelisting the owner...")
        ret = self.nodes[1].delegatoradd(owner_address)
        assert(ret)
        self.log.info("Delegator address %s whitelisted" % owner_address)

        # 7) check that the staker CANNOT spend the coins.
        # ------------------------------------------------
        print("*** 7 ***")
        self.log.info("Trying to spend one of the delegated UTXOs with the cold-staking key...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_greater_than(len(delegated_utxos), 0)
        u = delegated_utxos[0]
        # this is not usable due to the unavailability of validation state
        # assert_raises_rpc_error(-26, "mandatory-script-verify-flag-failed (Script failed an OP_CHECKCOLDSTAKEVERIFY operation",
        #                         self.spendUTXOwithNode, u, 1)
        assert_raises_rpc_error(-26, "TX rejected",
                                self.spendUTXOwithNode, u, 1)
        self.log.info("Good. Cold staker was NOT able to spend (failed OP_CHECKCOLDSTAKEVERIFY)")
        for i in range(30):
            hash = self.gen_pow_block(3, average_block_time, block_time_spread)
        sync_blocks(self.nodes)

        ###########
        # TODO: put this in a funtion
        # generate blocks up to 1000 to be able to start with PoS
        current_block_count = self.nodes[1].getblockcount()
        for i in range(1000 - current_block_count):
            hash = self.gen_pow_block(3, average_block_time, block_time_spread)
            if i % 100 == 0:
                sync_blocks(self.nodes)
        sync_blocks(self.nodes)
        ###########

        # 8) check that the staker can use the coins to stake a block with internal miner.
        # --------------------------------------------------------------------------------
        print("*** 8 ***")
        # since we spent one input already, we have NUM_OF_INPUTS - 1 left
        assert_equal(self.nodes[1].getstakinginfo()["stakableoutputs"], NUM_OF_INPUTS - 1)
        self.log.info("Generating one valid cold-stake block...")
        # hash = self.gen_pow_block(1, average_block_time, block_time_spread)
        self.gen_pos_block(1)
        self.log.info("New block created by cold-staking. Trying to submit...")
        newblockhash = self.nodes[1].getbestblockhash()
        self.log.info("Block %s submitted" % newblockhash)

        # Verify that nodes[0] accepts it
        sync_blocks(self.nodes)
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        assert_equal(newblockhash, self.nodes[0].getbestblockhash())
        self.log.info("Great. Cold-staked block was accepted!")

        # # check balances after staked block.
        # self.expected_balance -= 50
        # self.expected_immature_balance += 300
        # self.checkBalances()
        # self.log.info("Balances check out after staked block")

        # 9) check that the staker can use the coins to stake a block with a rawtransaction.
        # ----------------------------------------------------------------------------------
        print("*** 9 ***")
        self.log.info("Generating another valid cold-stake block...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInput = stakeable_coins[0]
        assert_equal(len(stakeable_coins), NUM_OF_INPUTS - 2)
        assert_greater_than(len(stakeInput), 0)
        self.gen_pos_block(1, True, [[stakeInput["txid"],stakeInput["vout"]]])
        staked_block_mint1 = self.nodes[1].getblockbynumber(self.nodes[1].getblockcount(), True)["mint"]
        self.log.info("New block created (rawtx) by cold-staking.")

        # Verify that nodes[0] accepts it
        sync_blocks(self.nodes)
        remaining_stakable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_equal(len(remaining_stakable_coins), NUM_OF_INPUTS - 3)
        for s in remaining_stakable_coins:
            print(s["txid"])
            print(stakeInput["txid"])
            assert s["txid"] != stakeInput["txid"]
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        self.log.info("Great. Cold-staked block was accepted!")
        staked_block_mint2 = self.nodes[1].getblockbynumber(self.nodes[1].getblockcount(), True)["mint"]

        # check balances after staked block.
        self.expected_balance -= 100  # 2 x 50 spent
        self.expected_immature_balance += 100 + float(staked_block_mint1) + float(staked_block_mint2)
        self.checkBalances()
        self.log.info("Balances check out after staked block")

        # 10) check that the staker cannot stake a block changing the coinstake scriptPubkey.
        # ----------------------------------------------------------------------------------
        print("*** 10 ***")
        self.log.info("Generating one invalid cold-stake block (changing first coinstake output)...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInput = stakeable_coins[0]
        assert_equal(len(stakeable_coins), NUM_OF_INPUTS - 3)
        assert_greater_than(len(stakeInput), 0)
        # Create the block
        block_serialized = self.gen_pos_block(1, False, [[stakeInput["txid"], stakeInput["vout"]]])
        new_block = CBlock()
        new_block.deserialize(BytesIO(bytes.fromhex(block_serialized)))

        # extract sign key from scriptSig
        s = CScript(new_block.vtx[1].vin[0].scriptSig)
        pub_key = self.get_pubkey_from_p2cs_scriptSig(s)
        # get the address associated with that pub key
        sign_address = key_to_p2pkh(pub_key.hex())
        # get the private key of that address
        sign_address_priv_key_wif = self.nodes[1].dumpprivkey(sign_address)
        # get arbitrary key from someone else to replace the scriptPubKey of P2CS
        rogue_address = self.nodes[1].getnewaddress()
        block_sig_key = self.wif_privkey_to_eckey(sign_address_priv_key_wif)

        # we ensure that we arrived at the same public key
        # attempt to change the destinations to custom ones
        script = self.nodes[1].getscriptpubkeyforp2cs(rogue_address, rogue_address)
        # modify the coinstake with the destination we created
        # inject the modified script
        new_block.vtx[1].vout[1].scriptPubKey = hex_str_to_bytes(script)
        serialized_coinstake = new_block.vtx[1].serialize()
        # self.nodes[1].signrawtransaction(bytes_to_hex_str(serialized_coinstake))
        new_block.hashMerkleRoot = new_block.calc_merkle_root()
        new_block.rehash()
        new_block.vchBlockSig = block_sig_key.sign(bytes.fromhex(new_block.hash)[::-1])
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block | this should fail at CheckProofOfStake(), where script evaluation will fail
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert("rejected" in ret)

        # Verify that nodes[0] rejects it
        sync_blocks(self.nodes)
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getblock, new_block.hash)
        self.log.info("Great. Malicious cold-staked block was NOT accepted!")
        self.checkBalances()
        self.log.info("Balances check out after (non) staked block")

        # 11) neither adding different outputs to the coinstake.
        # ------------------------------------------------------
        print("*** 11 ***")
        self.log.info("Generating another invalid cold-stake block (adding coinstake value)...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInput = stakeable_coins[0]
        assert_equal(len(stakeable_coins), NUM_OF_INPUTS - 3)
        assert_greater_than(len(stakeInput), 0)
        # Create the block + add 1 satoshi, illegitimately, and then watch it fail
        block_serialized = self.gen_pos_block(1, False, [[stakeInput["txid"], stakeInput["vout"]]], 1)
        new_block = CBlock()
        new_block.deserialize(BytesIO(bytes.fromhex(block_serialized)))
        new_block.rehash()

        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert("rejected" in ret)

        # Verify that nodes[0] rejects it
        sync_blocks(self.nodes)
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getblock, new_block.hash)
        self.log.info("Great. Malicious cold-staked block was NOT accepted!")
        self.checkBalances()
        self.log.info("Balances check out after (non) staked block")

        # 12) Now node[0] gets mad and spends all the delegated coins, voiding the P2CS contracts.
        # ----------------------------------------------------------------------------------------
        self.log.info("Let's void the contracts.")
        for i in range(20):
            block_hash = self.gen_pos_block(3)
        sync_blocks(self.nodes)
        print("*** 12 ***")
        self.log.info("Cancel the stake delegation spending the delegated utxos...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_equal(len(delegated_utxos), NUM_OF_INPUTS - 1)  # we spent one at the very beginning
        # remove one utxo to spend later
        txhash = self.spendUTXOsWithNode(delegated_utxos, 0)
        assert(txhash != None)
        self.log.info("Good. Owner was able to void the stake delegations - tx: %s" % str(txhash))
        block_hash = self.gen_pos_block(0)
        sync_blocks(self.nodes)
        print(self.nodes[0].listunspent())
        print(getDelegatedUtxos(self.nodes[0].listunspent()))

        # check balances after big spend.
        # no more delegated or cold balance
        self.expected_balance = 0
        self.expected_immature_balance = 0
        self.checkBalances()
        self.log.info("Balances check out after the delegations have been voided.")

        # 13) check that coinstaker is empty and can no longer stake.
        # -----------------------------------------------------------
        print("*** 13 ***")
        self.log.info("Trying to generate one cold-stake block again...")
        assert_equal(self.nodes[1].getstakinginfo()["stakableoutputs"], 0)
        self.log.info("Cigar. Cold staker was NOT able to create any more blocks.")


    def checkBalances(self):
        w_info = self.nodes[0].getwalletinfo()
        self.log.info("OWNER - Delegated %f / Cold %f   [%f / %f]" % (
            float(w_info["delegated_balance"]), w_info["cold_staking_balance"],
            float(w_info["immature_delegated_balance"]), w_info["immature_cold_staking_balance"]))
        assert_equal(float(w_info["delegated_balance"]), self.expected_balance)
        assert_equal(float(w_info["immature_delegated_balance"]), self.expected_immature_balance)
        assert_equal(float(w_info["cold_staking_balance"]), 0)
        w_info = self.nodes[1].getwalletinfo()
        self.log.info("STAKER - Delegated %f / Cold %f   [%f / %f]" % (
            float(w_info["delegated_balance"]), w_info["cold_staking_balance"],
            float(w_info["immature_delegated_balance"]), w_info["immature_cold_staking_balance"]))
        assert_equal(float(w_info["delegated_balance"]), 0)
        assert_equal(float(w_info["cold_staking_balance"]), self.expected_balance)
        assert_equal(float(w_info["immature_cold_staking_balance"]), self.expected_immature_balance)

    def spendUTXOwithNode(self, utxo, node_n):
        new_addy = self.nodes[node_n].getnewaddress()
        inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
        out_amount = (float(utxo["amount"]) - self.DEFAULT_FEE)
        outputs = {}
        outputs[new_addy] = out_amount
        spendingTx = self.nodes[node_n].createrawtransaction(inputs, outputs)
        spendingTx_signed = self.nodes[node_n].signrawtransaction(spendingTx)
        return self.nodes[node_n].sendrawtransaction(spendingTx_signed["hex"])

    def spendUTXOsWithNode(self, utxos, node_n):
        new_addy = self.nodes[node_n].getnewaddress()
        inputs = []
        outputs = {}
        outputs[new_addy] = 0
        for utxo in utxos:
            inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
            outputs[new_addy] += float(utxo["amount"])
        outputs[new_addy] -= self.DEFAULT_FEE
        spendingTx = self.nodes[node_n].createrawtransaction(inputs, outputs)
        spendingTx_signed = self.nodes[node_n].signrawtransaction(spendingTx)
        return self.nodes[node_n].sendrawtransaction(spendingTx_signed["hex"])


if __name__ == '__main__':
    ColdStakingTest().main()
