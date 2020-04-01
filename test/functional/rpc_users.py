#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test multiple RPC users."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import str_to_b64str, assert_equal

import os
import http.client
import urllib.parse

class HTTPBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def setup_chain(self):
        super().setup_chain()
        #Append rpcauth to neblio.conf before initialization
        rpcuser1 = "rpcuser=abcccc"
        rpcpassword1 = "rpcpassword=xyzzzz"
        rpcuser2 = "rpcuser=rpcuser"
        rpcpassword2 = "rpcpassword=rpcpassword"
        with open(os.path.join(self.options.tmpdir+"/node0", "neblio.conf"), 'a', encoding='utf8') as f:
            f.write(rpcuser1+"\n")
            f.write(rpcpassword1+"\n")
        with open(os.path.join(self.options.tmpdir+"/node1", "neblio.conf"), 'a', encoding='utf8') as f:
            f.write(rpcuser2+"\n")
            f.write(rpcpassword2+"\n")

    def run_test(self):

        ##################################################
        # Check correctness of the rpcauth config option #
        ##################################################
        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password
        print(authpair)

        #New authpair generated via share/rpcuser tool
        password = "cA773lm788buwYe4g4WT+05pKyNruVKjQ25x3n0DQcM="

        #Second authpair with different username
        password2 = "8/F3uMDw4KSEbw96U3CA1C4X05dkHDN2BPFjTgZW4KI="
        authpairnew = "rt:"+password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        conn.close()

        #Wrong login name with rt's password
        authpairnew = "rtwrong:"+password
        headers = {"Authorization": "Basic " + str_to_b64str(authpairnew)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 401)
        conn.close()

        #Wrong password for rt
        authpairnew = "rt:"+password+"wrong"
        headers = {"Authorization": "Basic " + str_to_b64str(authpairnew)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 401)
        conn.close()

        ###############################################################
        # Check correctness of the rpcuser/rpcpassword config options #
        ###############################################################
        url = urllib.parse.urlparse(self.nodes[1].url)

        # rpcuser and rpcpassword authpair
        rpcuserauthpair = "rpcuser:rpcpassword"

        headers = {"Authorization": "Basic " + str_to_b64str(rpcuserauthpair)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        conn.close()

        #Wrong login name with rpcuser's password
        rpcuserauthpair = "rpcuserwrong:rpcpassword"
        headers = {"Authorization": "Basic " + str_to_b64str(rpcuserauthpair)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 401)
        conn.close()

        #Wrong password for rpcuser
        rpcuserauthpair = "rpcuser:rpcpasswordwrong"
        headers = {"Authorization": "Basic " + str_to_b64str(rpcuserauthpair)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 401)
        conn.close()


if __name__ == '__main__':
    HTTPBasicsTest ().main ()
