In this doc you will find the most important information regarding [Ledger HW wallet](https://www.ledger.com/) support in the Neblio wallet. Ledger support was added to the Neblio wallet in April 2023. Integration was provided by [Vacuumlabs](https://vacuumlabs.com/).

Integrated features:
- Add Ledger address
- Verify Ledger address
- Sign Ledger transactions

Unsupported features:
- message signing
- NTP1 tokens (NTP1 tokens should not be sent to a Ledger address because they will become **unspendable**)
- cold-staking

## Add Ledger address
Available via the "New address" button in the "Receive" tab in Qt Wallet by checking the "Ledger address" checkbox in the "New Address" dialog. The user chooses the account index and address index which are used to build the BIP-32 address derivation path (e.g. "m/44'/146'/0'/0/0"). Along with the payment address (m/44'/146'/0'/**0**/0) a change address (m/44'/146'/0'/**1**/0) is imported as well. Ledger address labels **must** be unique i.e. a Ledger account label cannot be used by **any** other wallet account and they also **must** be non-empty.

Also available via RPC - `addledgeraddress <accountindex> <addressindex> <label>`.

*Note: when adding a Ledger address which might have already been used a blockchain rescan is required to discover the address's funds.*

## Verify Ledger address
Available via the "Verify Address" button in the "Receive" tab in Qt Wallet. Displays the Ledger address on the screen and then also on Ledger.

Also available via RPC - `verifyledgeraddress <accountindex> <ischange> <addressindex> <expectedaddress>`.

## Sign Ledger transactions
Available by checking the "Pay from a Ledger address" in the "Send" tab in Qt Wallet. A Ledger transaction can only contain inputs from a single **Ledger** payment address and its corresponding change address. This is why a "Pay From" Ledger account has to be selected for Ledger transactions. This also means that funds from the "internal" wallet accounts cannot be sent along with Ledger funds in the same transactions. 

Ledger accounts don't follow BIP-44 i.e. change addresses are reused. However the user can manually follow BIP-44 by always importing an address with the next unused address index and using its change address for a transaction. The coin control feature can be leveraged to manipulate inputs and change address.

Also available via RPC - `sendfrom <fromaccount> <toneblioaddress> <amount>`.
