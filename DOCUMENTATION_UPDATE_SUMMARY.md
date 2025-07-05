# Documentation Update Summary - Fee Removal

## Overview
Updated all project documentation to reflect the removal of custom quantum fee logic. Quantum transactions now pay standard fees based purely on transaction size.

## Files Updated

### 1. CLAUDE.md
- Removed: "Fee discounts: ML-DSA 10%, SLH-DSA 5%" from consensus changes
- Updated: RPC command descriptions to remove quantum discount parameters
  - `estimatesmartfee` - Now described as "Standard fee estimation"
  - `estimatetxfee` - Now described as "Transaction fee estimation (fees based on transaction size only)"

### 2. QSBITCOIN_PLAN.md
- Changed: "Fee incentives only - 10% discount for quantum signatures (not yet implemented)"
- To: "No fee incentives - Fees based purely on transaction size"

### 3. QSBITCOIN_TASKS.md
- Updated fee structure documentation:
  - Removed: "Base fee × 1.5 × discount factor"
  - Removed: "ML-DSA: 10% discount (0.9 factor)"
  - Removed: "SLH-DSA: 5% discount (0.95 factor)"
  - Added: "Standard fees based on transaction size only"
  - Added: "No special discounts or multipliers"
  - Added: "Larger quantum signatures result in higher fees due to size"

### 4. README.md
- Changed: "Fee Optimization - Smart fee structure with quantum signature discounts"
- To: "Standard Fee Structure - Fees based on transaction size with no special discounts"

### 5. Spec.md
- Completely rewrote Section 5 (Fee Policy):
  - Removed all references to fee discounts and multipliers
  - Updated fee calculation to show standard Bitcoin Core logic
  - Emphasized that fees are based purely on transaction weight/size

### 6. QSBITCOIN_IMPLEMENTATION_STATUS.md
- Changed: "Transaction fees automatically adjusted based on signature type"
- To: "Transaction fees based on transaction size only (no special discounts)"

### 7. quantum_regtest_results.md
- Updated fee estimation section to note that `estimatequantumfee` RPC has been removed
- Noted that standard fee estimation commands should be used instead

## Key Messages Across Documentation

1. **No Special Treatment**: Quantum signatures receive no special fee treatment
2. **Size-Based Fees**: Fees are determined purely by transaction size/weight
3. **Proportional Costs**: Larger quantum signatures naturally cost more due to their size
4. **Simplified Implementation**: Removal of custom fee logic makes QSBitcoin more aligned with Bitcoin Core
5. **Standard Commands**: Use standard Bitcoin fee estimation commands, not quantum-specific ones

## Documentation Consistency

All documentation now consistently reflects that:
- ML-DSA transactions (~3.3KB signatures) pay more than ECDSA due to size
- SLH-DSA transactions (~35KB signatures) pay significantly more due to size
- No discounts, multipliers, or special fee calculations exist
- The implementation follows standard Bitcoin fee principles