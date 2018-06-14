#include "ntp1sendtokensdata.h"

#include "init.h"
#include "util.h"
#include "wallet.h"
#include "json/json_spirit.h"

NTP1SendTokensData::NTP1SendTokensData() {}

void NTP1SendTokensData::addRecipient(const NTP1SendTokensOneRecipientData& data)
{
    recipients.push_back(data);
}

void NTP1SendTokensData::addTokenSourceAddress(const std::string& tokenSourceAddress)
{
    tokenSourceAddresses.push_back(tokenSourceAddress);
}

void NTP1SendTokensData::calculateSources(boost::shared_ptr<NTP1Wallet> wallet, bool recalculateFee)
{
    // collect all required amounts in one map, with tokenId vs amount
    std::map<std::string, int64_t> required_amounts;
    for (const auto& r : recipients) {
        if (required_amounts.find(r.tokenId) == required_amounts.end()) {
            required_amounts[r.tokenId] = 0;
        }
        required_amounts[r.tokenId] += r.amount;
    }

    // get token balances from the wallet
    const auto& balancesMap = wallet->getBalancesMap();

    for (const auto& required_amount : required_amounts) {
        auto available_balance = balancesMap.find(required_amount.first);
        if (available_balance != balancesMap.end()) {
            if (required_amount.second > available_balance->second) {
                throw std::runtime_error("Your balance is not sufficient to cover for " +
                                         wallet->getTokenName(required_amount.first));
            }
        } else {
            throw std::runtime_error("You're trying to spend tokens that you don't own; namely: " +
                                     wallet->getTokenName(required_amount.first));
        }
    }

    // calculate reserved balances to be used in this transaction
    const auto&               availableOutputsMap = wallet->getWalletOutputsWithTokens();
    std::vector<NTP1OutPoint> availableOutputs;
    availableOutputs.reserve(availableOutputsMap.size());
    for (const auto& el : availableOutputsMap) {
        availableOutputs.push_back(el.first);
    }

    // to improve privacy, shuffle inputs; pseudo-random is good enough here
    std::random_shuffle(availableOutputs.begin(), availableOutputs.end());

    // this container will be filled and must have tokens that are higher than the required amounts
    std::map<std::string, int64_t> fulfilledTokenAmounts;
    // reset fulfilled amounts to zero
    for (const auto& el : required_amounts) {
        fulfilledTokenAmounts[el.first] = 0;
    }
    tokenSourceAddresses.clear();
    for (const auto& required_amount : required_amounts) {
        for (auto it = availableOutputs.begin(); it != availableOutputs.end(); ++it) {
            const auto&            output    = *it;
            const NTP1Transaction& txData    = availableOutputsMap.find(output)->second;
            const NTP1TxOut&       ntp1txOut = txData.getTxOut(output.getIndex());

            auto numOfTokensInOutput = ntp1txOut.getNumOfTokens();
            bool takeThisTransaction = false;
            for (auto i = 0u; i < numOfTokensInOutput; i++) {
                std::string outputTokenId = ntp1txOut.getToken(i).getTokenIdBase58();
                // if token id matches in the transaction with the required one, take it into account
                int64_t required_amount_still =
                    required_amount.second - fulfilledTokenAmounts[outputTokenId];
                if (required_amount.first == outputTokenId && required_amount_still > 0) {
                    takeThisTransaction = true;
                    break;
                }
            }

            // take this transaction by
            // 1. remove it from the vector of available outputs
            // 2. add its values to fulfilledTokenAmounts
            // 3. add the address to the list of addresses to use
            if (takeThisTransaction) {
                for (auto i = 0u; i < numOfTokensInOutput; i++) {
                    std::string outputTokenId = ntp1txOut.getToken(i).getTokenIdBase58();
                    fulfilledTokenAmounts[outputTokenId] += ntp1txOut.getToken(i).getAmount();
                }
                tokenSourceAddresses.push_back(ntp1txOut.getAddress());
                it = availableOutputs.erase(it);
            }
        }
    }

    // finally, verify that the balances caught cover the amount to be sent
    for (const auto& required_amount : required_amounts) {
        if (fulfilledTokenAmounts[required_amount.first] < required_amount.second) {
            throw std::runtime_error("Failed to cover required balance of " +
                                     wallet->getTokenName(required_amount.first));
        }
    }

    // remove duplicate addresses
    std::sort(tokenSourceAddresses.begin(), tokenSourceAddresses.end());
    tokenSourceAddresses.erase(unique(tokenSourceAddresses.begin(), tokenSourceAddresses.end()),
                               tokenSourceAddresses.end());

    int64_t currentTotalNeblsInSelectedAddresses = 0;
    while (true) {
        unsigned long numbeOfAddressesAtBeginning = tokenSourceAddresses.size();

        if (recalculateFee) {
            fee = EstimateTxFee(tokenSourceAddresses.size(), recipients.size());
        }

        currentTotalNeblsInSelectedAddresses = __addAddressesThatCoverFees();

        if (tokenSourceAddresses.size() == numbeOfAddressesAtBeginning) {
            break;
        }
    }

    if (fee > currentTotalNeblsInSelectedAddresses) {
        throw std::runtime_error("Insufficient nebls to pay for fees (" + ToString(fee / 1.e8) + ")");
    }
}

void NTP1SendTokensData::setFee(uint64_t Fee) { fee = Fee; }

json_spirit::Value NTP1SendTokensData::exportJsonData() const
{
    json_spirit::Object root;

    // fee field
    root.push_back(json_spirit::Pair("fee", fee));

    // from array
    json_spirit::Array fromArray;

    for (long i = 0; i < static_cast<long>(tokenSourceAddresses.size()); i++) {
        fromArray.push_back(json_spirit::Value(tokenSourceAddresses[i]));
    }
    root.push_back(json_spirit::Pair("from", json_spirit::Value(fromArray)));

    // to array
    json_spirit::Array toArray;

    for (long i = 0; i < static_cast<long>(recipients.size()); i++) {
        toArray.push_back(recipients[i].exportJsonData());
    }
    root.push_back(json_spirit::Pair("to", json_spirit::Value(toArray)));

    // flags
    json_spirit::Object flags;
    flags.push_back(json_spirit::Pair("splitChange", json_spirit::Value(true)));

    root.push_back(json_spirit::Pair("flags", flags));

    return root;
}

std::string NTP1SendTokensData::exportToAPIFormat() const
{
    std::stringstream s;
    json_spirit::write(exportJsonData(), s);
    return s.str();
}

int64_t NTP1SendTokensData::__addAddressesThatCoverFees()
{

    // get nebls that fulfill the fee (if required)
    // get the address vs balance map
    std::map<CTxDestination, int64_t> neblAddressesBalancesMap = pwalletMain->GetAddressBalances();

    // create a map og address vs balance, to check the addresses that are already used easily
    std::map<std::string, int64_t> neblBalancesMap;
    std::transform(neblAddressesBalancesMap.begin(), neblAddressesBalancesMap.end(),
                   std::inserter(neblBalancesMap, neblBalancesMap.end()),
                   [](const std::pair<CTxDestination, int64_t>& el) {
                       return std::make_pair(CBitcoinAddress(boost::get<CKeyID>(el.first)).ToString(),
                                             el.second);
                   });

    // TODO: exclude NTP1 UTXOs from fee calculation
    int64_t currentTotalNeblsInSelectedAddresses = 0;
    for (const auto& address : tokenSourceAddresses) {
        currentTotalNeblsInSelectedAddresses += neblBalancesMap[address];
    }

    // check if the total amount in selected addresses is sufficient for fees
    if (fee > currentTotalNeblsInSelectedAddresses) {
        // convert map of addresses to vector of pairs
        // (to have freedom of shuffling order in the interest of privacy)
        std::deque<std::pair<std::string, int64_t>> neblBalancesVector;
        std::transform(neblAddressesBalancesMap.begin(), neblAddressesBalancesMap.end(),
                       std::back_inserter(neblBalancesVector),
                       [](const std::pair<CTxDestination, int64_t>& el) {
                           return std::make_pair(
                               CBitcoinAddress(boost::get<CKeyID>(el.first)).ToString(), el.second);
                       });

        // remove addresses that are already taken
        neblBalancesVector.erase(std::remove_if(neblBalancesVector.begin(), neblBalancesVector.end(),
                                                [this](const std::pair<std::string, int64_t>& el) {
                                                    return (std::find(tokenSourceAddresses.begin(),
                                                                      tokenSourceAddresses.end(),
                                                                      el.first) !=
                                                            tokenSourceAddresses.end());
                                                }),
                                 neblBalancesVector.end());

        // shuffle addresses before picking from them
        std::random_shuffle(neblBalancesVector.begin(), neblBalancesVector.end());

        // add more addresses to satisfy the balance
        for (const auto& el : neblBalancesVector) {
            tokenSourceAddresses.push_back(el.first);
            currentTotalNeblsInSelectedAddresses += el.second;
            if (fee <= currentTotalNeblsInSelectedAddresses) {
                break;
            }
        }
    }

    return currentTotalNeblsInSelectedAddresses;
}

int64_t NTP1SendTokensData::EstimateTxSizeInBytes(int64_t num_of_inputs, int64_t num_of_outputs)
{
    return num_of_inputs * 181 + num_of_outputs * 34 + 10;
}

int64_t NTP1SendTokensData::EstimateTxFee(int64_t num_of_inputs, int64_t num_of_outputs)
{
    double Fee = static_cast<double>(MIN_TX_FEE) *
                 (static_cast<double>(EstimateTxSizeInBytes(num_of_inputs, num_of_outputs)) / 1000.);
    // nearest 10000
    return static_cast<int64_t>(std::ceil(Fee / 10000) * 10000);
}
