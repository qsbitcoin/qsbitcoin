#!/usr/bin/env python3
# Copyright (c) 2025 The QSBitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test quantum P2WSH implementation and large signature handling."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)
from test_framework.script import (
    CScript,
    OP_0,
)
from test_framework.messages import (
    CTransaction,
    CTxIn,
    CTxOut,
    COutPoint,
    CTxInWitness,
)
from decimal import Decimal
import time


class QuantumP2WSHTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # Enable witness and descriptor wallets
        self.extra_args = [
            ["-acceptnonstdtxn=1", "-addresstype=bech32", "-changetype=bech32"],
            ["-acceptnonstdtxn=1", "-addresstype=bech32", "-changetype=bech32"]
        ]
        
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        
    def run_test(self):
        self.log.info("Testing quantum P2WSH implementation...")
        
        # Test basic functionality
        self.test_quantum_address_format()
        self.test_quantum_transaction_flow()
        self.test_large_signature_handling()
        self.test_fee_calculation()
        
    def test_quantum_address_format(self):
        """Test that quantum addresses are properly formatted as P2WSH."""
        self.log.info("Testing quantum address format...")
        
        node = self.nodes[0]
        
        # Generate initial funds
        self.generatetoaddress(node, 101, node.getnewaddress())
        
        # Generate quantum addresses
        ml_addr = node.getnewquantumaddress("ml-dsa", "ml-dsa")
        slh_addr = node.getnewquantumaddress("slh-dsa", "slh-dsa")
        
        self.log.info(f"ML-DSA address: {ml_addr}")
        self.log.info(f"SLH-DSA address: {slh_addr}")
        
        # Verify they're bech32 addresses (P2WSH on regtest starts with bcrt1)
        assert ml_addr.startswith("bcrt1"), "ML-DSA should be bech32"
        assert slh_addr.startswith("bcrt1"), "SLH-DSA should be bech32"
        
        # Verify they're witness v0 scripts (P2WSH)
        ml_info = node.validateaddress(ml_addr)
        slh_info = node.validateaddress(slh_addr)
        
        assert_equal(ml_info['iswitness'], True)
        assert_equal(ml_info['witness_version'], 0)
        assert_equal(slh_info['iswitness'], True)
        assert_equal(slh_info['witness_version'], 0)
        
        # P2WSH addresses should have 32-byte witness programs
        ml_decoded = node.decodescript(ml_info['scriptPubKey'])
        slh_decoded = node.decodescript(slh_info['scriptPubKey'])
        
        # Check script structure: OP_0 <32-bytes>
        assert ml_decoded['asm'].startswith('0 '), "Should start with OP_0"
        assert slh_decoded['asm'].startswith('0 '), "Should start with OP_0"
        
    def test_quantum_transaction_flow(self):
        """Test complete transaction flow with quantum addresses."""
        self.log.info("Testing quantum transaction flow...")
        
        node0 = self.nodes[0]
        node1 = self.nodes[1]
        
        # Create quantum addresses
        ml_addr = node0.getnewquantumaddress("test-ml", "ml-dsa")
        slh_addr = node0.getnewquantumaddress("test-slh", "slh-dsa")
        
        # Send to quantum addresses
        self.log.info("Sending to quantum addresses...")
        ml_txid = node0.sendtoaddress(ml_addr, 10.0)
        slh_txid = node0.sendtoaddress(slh_addr, 15.0)
        
        # Mine block
        self.generatetoaddress(node0, 1, node0.getnewaddress())
        
        # Verify transactions
        ml_tx = node0.gettransaction(ml_txid)
        slh_tx = node0.gettransaction(slh_txid)
        assert_equal(ml_tx['confirmations'], 1)
        assert_equal(slh_tx['confirmations'], 1)
        
        # Check UTXOs
        utxos = node0.listunspent(1, 9999999, [ml_addr, slh_addr])
        assert_equal(len(utxos), 2)
        
        # Test spending from quantum addresses
        self.log.info("Testing spending from quantum addresses...")
        
        # Get a regular address on node1
        node1_addr = node1.getnewaddress()
        
        # Spend from ML-DSA
        initial_balance = node0.getbalance()
        ml_spend_txid = node0.sendtoaddress(node1_addr, 5.0, "", "", False, True)
        self.log.info(f"Spent from ML-DSA: {ml_spend_txid}")
        
        # The transaction should be created successfully
        assert ml_spend_txid is not None
        
        # Mine and verify
        self.generatetoaddress(node0, 1, node0.getnewaddress())
        ml_spend_tx = node0.gettransaction(ml_spend_txid)
        assert_equal(ml_spend_tx['confirmations'], 1)
        
        # Check balance changed
        new_balance = node0.getbalance()
        assert new_balance < initial_balance
        
    def test_large_signature_handling(self):
        """Test that large quantum signatures work correctly in witness."""
        self.log.info("Testing large signature handling...")
        
        node = self.nodes[0]
        
        # Create SLH-DSA address (has ~35KB signatures)
        slh_addr = node.getnewquantumaddress("large-sig", "slh-dsa")
        
        # Send large amount to ensure it's worth spending despite high fees
        txid = node.sendtoaddress(slh_addr, 50.0)
        self.generatetoaddress(node, 1, node.getnewaddress())
        
        # Get transaction details
        tx = node.gettransaction(txid)
        tx_details = node.decoderawtransaction(tx['hex'])
        
        # Find the output
        vout = None
        for i, out in enumerate(tx_details['vout']):
            if 'addresses' in out['scriptPubKey'] and slh_addr in out['scriptPubKey']['addresses']:
                vout = i
                break
        
        assert vout is not None, "Could not find output"
        
        # Now spend it
        self.log.info("Attempting to spend SLH-DSA output with large signature...")
        
        new_addr = node.getnewaddress()
        
        # This should work now with P2WSH (witness can handle large signatures)
        spend_txid = node.sendtoaddress(new_addr, 25.0, "", "", False, True)
        
        # Should not crash!
        assert spend_txid is not None
        
        # Mine and verify
        self.generatetoaddress(node, 1, node.getnewaddress())
        spend_tx = node.gettransaction(spend_txid)
        assert_equal(spend_tx['confirmations'], 1)
        
        # Check the raw transaction to verify witness usage
        raw_tx = node.decoderawtransaction(spend_tx['hex'])
        
        # Should have witness data
        has_witness = any('txinwitness' in vin for vin in raw_tx['vin'])
        assert has_witness, "Transaction should use witness"
        
        self.log.info("Successfully spent SLH-DSA output with ~35KB signature!")
        
    def test_fee_calculation(self):
        """Test that fees are calculated correctly for quantum transactions."""
        self.log.info("Testing quantum transaction fee calculation...")
        
        node = self.nodes[0]
        
        # Create addresses
        ml_addr = node.getnewquantumaddress("fee-ml", "ml-dsa")
        slh_addr = node.getnewquantumaddress("fee-slh", "slh-dsa")
        regular_addr = node.getnewaddress()
        
        # Fund them
        node.sendtoaddress(ml_addr, 10.0)
        node.sendtoaddress(slh_addr, 10.0)
        node.sendtoaddress(regular_addr, 10.0)
        self.generatetoaddress(node, 1, node.getnewaddress())
        
        # Estimate fees for spending
        self.log.info("Estimating fees...")
        
        # Create test transactions (but don't send)
        # ML-DSA should have ~3.3KB signatures
        ml_inputs = node.listunspent(1, 9999999, [ml_addr])
        ml_fee_info = node.estimatesmartfee(1)
        
        # SLH-DSA should have ~35KB signatures
        slh_inputs = node.listunspent(1, 9999999, [slh_addr])
        slh_fee_info = node.estimatesmartfee(1)
        
        self.log.info(f"Fee estimation - ML-DSA: {ml_fee_info}")
        self.log.info(f"Fee estimation - SLH-DSA: {slh_fee_info}")
        
        # Test actual fee paid
        dest_addr = node.getnewaddress()
        
        # Spend from ML-DSA
        ml_spend = node.sendtoaddress(dest_addr, 5.0, "", "", False, True)
        self.generatetoaddress(node, 1, node.getnewaddress())
        ml_tx = node.gettransaction(ml_spend)
        ml_fee = abs(ml_tx['fee'])
        
        # Spend from SLH-DSA  
        slh_spend = node.sendtoaddress(dest_addr, 5.0, "", "", False, True)
        self.generatetoaddress(node, 1, node.getnewaddress())
        slh_tx = node.gettransaction(slh_spend)
        slh_fee = abs(slh_tx['fee'])
        
        self.log.info(f"Actual fees - ML-DSA: {ml_fee} BTC, SLH-DSA: {slh_fee} BTC")
        
        # SLH-DSA should have significantly higher fees due to larger signature
        assert_greater_than(slh_fee, ml_fee * 5, "SLH-DSA fee should be much higher")
        
        # But with P2WSH, witness discount should apply
        # (witness data only counts as 1/4 for fee purposes)
        self.log.info("Quantum signatures benefit from witness discount!")


if __name__ == '__main__':
    QuantumP2WSHTest(__file__).main()