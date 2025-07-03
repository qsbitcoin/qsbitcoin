#!/usr/bin/env python3
# Copyright (c) 2025 The QSBitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Simple test for quantum wallet functionality - FIXED VERSION."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

class SimpleQuantumFixedTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # Enable descriptor wallets which are required for quantum
        self.extra_args = [["-addresstype=bech32", "-changetype=bech32"]]
        
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        
    def run_test(self):
        node = self.nodes[0]
        
        # Create quantum-enabled wallet 
        self.log.info("Creating quantum-enabled wallet...")
        node.createwallet("quantum_test", False, False, "", False, True, None, False, True)
        
        self.log.info("Testing basic quantum info...")
        info = node.getquantuminfo()
        assert_equal(info['enabled'], True)
        assert_equal(info['activated'], True)
        
        # Generate some blocks
        self.log.info("Generating blocks...")
        self.generatetoaddress(node, 101, node.getnewaddress())
        
        self.log.info("Testing quantum address generation...")
        
        # Test ML-DSA address generation - CORRECT API
        ml_addr = node.getnewaddress("test", "bech32", "ml-dsa")
        self.log.info(f"Generated ML-DSA address: {ml_addr}")
        
        # Check it's a bech32 address
        assert ml_addr.startswith("bcrt1"), f"Expected bech32 address, got {ml_addr}"
        
        # Validate the address - CORRECT API  
        val = node.validateaddress(ml_addr)
        assert_equal(val['isvalid'], True)
        assert_equal(val['iswitness'], True)
        
        # Test SLH-DSA address generation
        slh_addr = node.getnewaddress("test2", "bech32", "slh-dsa")
        self.log.info(f"Generated SLH-DSA address: {slh_addr}")
        assert slh_addr.startswith("bcrt1"), f"Expected bech32 address, got {slh_addr}"
        
        self.log.info("Basic quantum test passed!")

if __name__ == '__main__':
    SimpleQuantumFixedTest(__file__).main()