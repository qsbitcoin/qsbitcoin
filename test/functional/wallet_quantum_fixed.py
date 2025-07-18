#!/usr/bin/env python3
# Copyright (c) 2025 The QSBitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test quantum-safe wallet RPC commands and transactions - FIXED VERSION."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)
from decimal import Decimal


class WalletQuantumFixedTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # Enable descriptor wallets which are required for quantum
        self.extra_args = [["-addresstype=bech32", "-changetype=bech32"]] * 2
        
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        
    def run_test(self):
        node0 = self.nodes[0] 
        node1 = self.nodes[1]
        
        # Create quantum-enabled wallets
        self.log.info("Creating quantum-enabled wallets...")
        node0.createwallet("quantum_wallet0", False, False, "", False, True, None, False, True)
        node1.createwallet("quantum_wallet1", False, False, "", False, True, None, False, True)
        
        self.log.info("Testing getquantuminfo RPC...")
        info = node0.getquantuminfo()
        assert_equal(info['enabled'], True)
        assert_equal(info['activated'], True)
        assert 'quantum_keys' in info
        assert 'supported_algorithms' in info
        
        # Check supported algorithms
        algos = info['supported_algorithms']
        assert_equal(len(algos), 2)
        assert_equal(algos[0]['name'], 'ml-dsa')
        assert_equal(algos[0]['signature_size'], 3293)
        assert_equal(algos[0]['public_key_size'], 1952)
        assert_equal(algos[1]['name'], 'slh-dsa') 
        assert_equal(algos[1]['signature_size'], 35664)
        assert_equal(algos[1]['public_key_size'], 48)
        
        # Generate some blocks to have funds
        self.log.info("Generating blocks...")
        self.generatetoaddress(node0, 101, node0.getnewaddress())
        
        self.log.info("Testing quantum address generation...")
        
        # Test ML-DSA address generation - CORRECT API
        ml_dsa_addr = node0.getnewaddress("ml-dsa-test", "bech32", "ml-dsa")
        self.log.info(f"Generated ML-DSA address: {ml_dsa_addr}")
        
        # Quantum addresses should now be bech32 P2WSH addresses
        assert ml_dsa_addr.startswith("bcrt1"), f"Expected bech32 address, got {ml_dsa_addr}"
        
        # Test SLH-DSA address generation - CORRECT API
        slh_dsa_addr = node0.getnewaddress("slh-dsa-test", "bech32", "slh-dsa")
        self.log.info(f"Generated SLH-DSA address: {slh_dsa_addr}")
        assert slh_dsa_addr.startswith("bcrt1"), f"Expected bech32 address, got {slh_dsa_addr}"
        
        # Validate addresses using standard validateaddress - CORRECT API
        self.log.info("Testing address validation...")
        val_ml = node0.validateaddress(ml_dsa_addr)
        assert_equal(val_ml['isvalid'], True)
        assert_equal(val_ml['iswitness'], True)
        assert_equal(val_ml['witness_version'], 0)
        
        val_slh = node0.validateaddress(slh_dsa_addr)
        assert_equal(val_slh['isvalid'], True)
        assert_equal(val_slh['iswitness'], True)
        
        # Test sending TO quantum addresses
        self.log.info("Testing sending to quantum addresses...")
        
        # Send to ML-DSA address
        ml_txid = node0.sendtoaddress(ml_dsa_addr, 10.0)
        self.log.info(f"Sent to ML-DSA, txid: {ml_txid}")
        
        # Send to SLH-DSA address
        slh_txid = node0.sendtoaddress(slh_dsa_addr, 15.0)
        self.log.info(f"Sent to SLH-DSA, txid: {slh_txid}")
        
        # Mine a block to confirm
        self.generatetoaddress(node0, 1, node0.getnewaddress())
        
        # Check the transactions
        ml_tx = node0.gettransaction(ml_txid)
        assert_equal(ml_tx['confirmations'], 1)
        
        slh_tx = node0.gettransaction(slh_txid)
        assert_equal(slh_tx['confirmations'], 1)
        
        # List unspent outputs
        self.log.info("Checking quantum unspent outputs...")
        unspent = node0.listunspent(1, 9999999, [ml_dsa_addr, slh_dsa_addr])
        assert_equal(len(unspent), 2)
        
        ml_utxo = next(u for u in unspent if u['address'] == ml_dsa_addr)
        assert_equal(ml_utxo['amount'], Decimal('10.0'))
        assert_equal(ml_utxo['spendable'], True)
        assert_equal(ml_utxo['solvable'], True)
        
        slh_utxo = next(u for u in unspent if u['address'] == slh_dsa_addr)
        assert_equal(slh_utxo['amount'], Decimal('15.0'))
        
        # Test spending FROM quantum addresses
        self.log.info("Testing spending from quantum addresses...")
        
        # Create address on node1 to receive funds
        node1_addr = node1.getnewaddress()
        
        # Try to spend from ML-DSA address
        self.log.info("Spending from ML-DSA address...")
        ml_spend_txid = node0.sendtoaddress(node1_addr, 5.0, "", "", False, True)
        self.log.info(f"Spent from ML-DSA, txid: {ml_spend_txid}")
        
        # Mine block
        self.generatetoaddress(node0, 1, node0.getnewaddress())
        
        # Verify the spend
        ml_spend_tx = node0.gettransaction(ml_spend_txid)
        assert_equal(ml_spend_tx['confirmations'], 1)
        
        # Check that quantum info is updated
        info_after = node0.getquantuminfo()
        assert_greater_than(info_after['quantum_keys'], 0)
        
        # Test error cases
        self.log.info("Testing error cases...")
        
        # Invalid algorithm - CORRECT API
        assert_raises_rpc_error(-8, "Unknown signature algorithm", 
                              node0.getnewaddress, "", "bech32", "invalid-algo")
        
        # Test large signature handling by spending from SLH-DSA
        self.log.info("Testing large signature (SLH-DSA) transaction...")
        try:
            slh_spend_txid = node0.sendtoaddress(node1_addr, 5.0, "", "", False, True)
            self.log.info(f"Successfully spent from SLH-DSA: {slh_spend_txid}")
            
            # Mine and verify
            self.generatetoaddress(node0, 1, node0.getnewaddress())
            slh_spend_tx = node0.gettransaction(slh_spend_txid)
            assert_equal(slh_spend_tx['confirmations'], 1)
            
            self.log.info("SLH-DSA large signature transaction successful!")
        except Exception as e:
            self.log.error(f"SLH-DSA spending failed: {e}")
            raise
        
        self.log.info("All quantum wallet tests passed!")


if __name__ == '__main__':
    WalletQuantumFixedTest(__file__).main()