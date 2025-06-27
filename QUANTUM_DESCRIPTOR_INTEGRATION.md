# Quantum Descriptor Integration Plan

## Current State
- Quantum keys are stored in a global temporary keystore (`g_quantum_keystore`)
- Keys are lost when bitcoind restarts
- Basic functionality works but lacks persistence

## Implementation Steps for Full Integration

### 1. Update DescriptorScriptPubKeyMan
- Add quantum key storage methods
- Integrate with existing descriptor framework
- Handle quantum key loading from database

### 2. Modify Quantum Descriptor Implementation
- Update `QuantumPubkeyProvider` to work with wallet database
- Ensure proper serialization/deserialization
- Add support for encrypted quantum keys

### 3. Update RPC Commands
- Modify `getnewquantumaddress` to:
  - Create proper quantum descriptors
  - Import them into the wallet
  - Store keys in wallet database
- Update `importdescriptors` to handle quantum descriptors

### 4. Database Integration Complete
- `WriteQuantumDescriptorKey` - ✅ Implemented
- `WriteCryptedQuantumDescriptorKey` - ✅ Implemented  
- `WriteQuantumKeyMetadata` - ✅ Implemented
- Need to add corresponding Load methods

### 5. Migration Path
- Create migration tool for existing quantum keys
- Update wallet loading to handle quantum descriptors
- Remove global keystore after migration

## Technical Challenges

### Non-Copyable Keys
- Quantum keys are non-copyable (unique_ptr)
- Need careful handling during serialization
- Use `GetPrivKeyData()` and `Load()` methods

### Descriptor Format
- Current: `qpkh(<pubkey_hex>)`
- Need to ensure proper parsing and storage
- Integration with existing descriptor system

### Backward Compatibility
- Maintain support for existing quantum addresses
- Ensure smooth migration from temporary keystore
- Preserve existing transaction compatibility

## Next Implementation Tasks

1. **Add Load Methods** (Priority: High)
   - `LoadQuantumDescriptorKey`
   - `LoadCryptedQuantumDescriptorKey`
   - `LoadQuantumKeyMetadata`

2. **Update DescriptorScriptPubKeyMan** (Priority: High)
   - Add quantum key management methods
   - Integrate with signing provider
   - Handle key generation and storage

3. **Update Import/Export** (Priority: Medium)
   - Modify `importdescriptors` RPC
   - Add quantum descriptor export support
   - Ensure proper backup/restore

4. **Remove Temporary Keystore** (Priority: Low)
   - After full integration complete
   - Provide migration tool
   - Update all references

## Testing Requirements

1. **Persistence Tests**
   - Generate quantum keys
   - Restart bitcoind
   - Verify keys are still available

2. **Encryption Tests**
   - Test with encrypted wallets
   - Verify quantum keys are properly encrypted
   - Test decryption and signing

3. **Migration Tests**
   - Test migration from temporary keystore
   - Verify no data loss
   - Test backward compatibility

## Estimated Completion
- Database methods: ✅ 50% Complete
- Descriptor integration: 🔄 In Progress
- RPC updates: ⏳ Pending
- Testing: ⏳ Pending
- Documentation: ⏳ Pending