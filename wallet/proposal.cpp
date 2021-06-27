#include "proposal.h"

#include "logging/logger.h"

Result<ProposalVote, ProposalVoteCreationError>
ProposalVote::CreateVote(int FromBlock, int ToBlock, uint32_t ProposalID, uint32_t VoteValue)
{
    if (FromBlock < 0) {
        return Err(ProposalVoteCreationError::InvalidStartBlockHeight);
    }
    if (ToBlock < 0) {
        return Err(ProposalVoteCreationError::InvalidEndBlockHeight);
    }
    if (FromBlock > ToBlock) {
        return Err(ProposalVoteCreationError::InvalidBlockHeightRange);
    }

    ProposalVote result;
    result.firstBlockHeight = FromBlock;
    result.lastBlockHeight  = ToBlock;
    result.valueAndID       = TRY(VoteValueAndID::CreateVote(ProposalID, VoteValue));

    return Ok(std::move(result));
}

uint32_t ProposalVote::getProposalID() const { return valueAndID.getProposalID(); }

int ProposalVote::getFirstBlockHeight() const { return firstBlockHeight; }

int ProposalVote::getLastBlockHeight() const { return lastBlockHeight; }

uint32_t ProposalVote::getVoteValue() const { return valueAndID.getVoteValue(); }

VoteValueAndID ProposalVote::getVoteValueAndProposalID() const { return valueAndID; }

json_spirit::Value ProposalVote::asJson() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("FirstVoteBlock", getFirstBlockHeight()));
    root.push_back(json_spirit::Pair("LastVoteBlock", getLastBlockHeight()));
    root.push_back(json_spirit::Pair("VoteValue", std::to_string(getVoteValue())));
    root.push_back(json_spirit::Pair("ProposalID", std::to_string(getProposalID())));

    return json_spirit::Value(std::move(root));
}

Result<void, AddVoteError> AllStoredVotes::addVote(const ProposalVote& vote)
{
    std::lock_guard<std::mutex> lg(mtx);
    {
        auto it = votes.find(vote.getFirstBlockHeight());
        if (it != votes.end()) {
            return Err(AddVoteError::FirstBlockAlreadyInAnotherVote);
        }
    }
    {
        auto it = votes.find(vote.getLastBlockHeight());
        if (it != votes.end()) {
            return Err(AddVoteError::LastBlockAlreadyInAnotherVote);
        }
    }

    const auto interval = boost::icl::discrete_interval<int>::closed(vote.getFirstBlockHeight(),
                                                                     vote.getLastBlockHeight());
    votes.insert(std::make_pair(interval, vote.getVoteValueAndProposalID()));
    return Ok();
}

void AllStoredVotes::removeAllVotesOfHeightRangeOfHeight(int someHeightInIt)
{
    std::lock_guard<std::mutex> lg(mtx);

    auto it = votes.find(someHeightInIt);
    if (it != votes.end()) {
        votes.erase(it->first);
    }
}

void AllStoredVotes::removeVotesAtHeightRange(int startHeight, int lastHeight)
{
    std::lock_guard<std::mutex> lg(mtx);
    votes.erase(boost::icl::discrete_interval<int>::open(startHeight - 1, lastHeight + 1));
}

boost::optional<ProposalVote> AllStoredVotes::voteFromIterator(decltype(votes)::const_iterator it) const
{
    const int                                             left  = it->first.lower();
    const int                                             right = it->first.upper();
    const VoteValueAndID&                                 vote  = it->second;
    const Result<ProposalVote, ProposalVoteCreationError> result =
        ProposalVote::CreateVote(left, right, vote.getProposalID(), vote.getVoteValue());
    if (result.isOk()) {
        return boost::make_optional(result.UNWRAP());
    } else {
        NLog.critical("Failed to recreate vote from interval [{},{}]; it seems that insertion input "
                      "validation failed and allowed an invalid vote in",
                      left, right);
        return boost::none;
    }
}

boost::optional<ProposalVote> AllStoredVotes::getProposalAtBlockHeight(int height) const
{
    std::lock_guard<std::mutex> lg(mtx);

    auto it = votes.find(height);
    if (it != votes.end()) {
        return voteFromIterator(it);
    }
    return boost::none;
}

std::vector<ProposalVote> AllStoredVotes::getAllVotes() const
{
    std::lock_guard<std::mutex> lg(mtx);
    std::vector<ProposalVote>   result;
    for (auto it = votes.begin(); it != votes.end(); ++it) {
        const auto& vote = voteFromIterator(it);
        if (vote) {
            result.push_back(*vote);
        }
    }
    return result;
}

json_spirit::Array AllStoredVotes::getAllVotesAsJson() const
{
    const std::vector<ProposalVote> votes = getAllVotes();
    json_spirit::Array              result;
    for (auto&& vote : votes) {
        result.push_back(vote.asJson());
    }
    return result;
}

bool AllStoredVotes::proposalExists(uint32_t proposalID) const
{
    std::lock_guard<std::mutex> lg(mtx);
    for (const auto& el : votes) {
        if (el.second.getProposalID() == proposalID) {
            return true;
        }
    }
    return false;
}

bool AllStoredVotes::empty() const
{
    std::lock_guard<std::mutex> lg(mtx);
    return votes.empty();
}

std::size_t AllStoredVotes::voteCount() const
{
    std::lock_guard<std::mutex> lg(mtx);
    return std::distance(votes.begin(), votes.end());
}

void AllStoredVotes::clear() { votes.clear(); }

std::string AllStoredVotes::AddVoteErrorAsString(AddVoteError                error,
                                                 const boost::optional<int>& startHeight,
                                                 const boost::optional<int>& lastHeight)
{
    switch (error) {
    case AddVoteError::FirstBlockAlreadyInAnotherVote:
        return fmt::format("The start block height{} is already in another vote. Remove that vote first",
                           startHeight ? " " + std::to_string(*startHeight) : "");
    case AddVoteError::LastBlockAlreadyInAnotherVote:
        return fmt::format("The last block height {} is already in another vote. Remove that vote first",
                           lastHeight ? " " + std::to_string(*lastHeight) : " ");
    }
    return "Unknown error";
}

uint32_t VoteValueAndID::getProposalID() const { return proposalID; }

uint32_t VoteValueAndID::getVoteValue() const { return voteValue; }

uint32_t VoteValueAndID::serializeToUint32() const
{
    uint32_t result = 0;
    result |= voteValue & 0x000000FF;
    result |= proposalID << 8;
    return result;
}

Result<VoteValueAndID, ProposalVoteCreationError> VoteValueAndID::CreateVote(uint32_t ProposalID,
                                                                             uint32_t VoteValue)
{
    if (ProposalID >= UINT32_C(1) << 24) {
        return Err(ProposalVoteCreationError::ProposalIDOutOfRange);
    }
    if (VoteValue >= UINT32_C(1) << 8) {
        return Err(ProposalVoteCreationError::VoteValueOutOfRange);
    }

    VoteValueAndID result;
    result.proposalID = ProposalID;
    result.voteValue  = VoteValue;
    return Ok(std::move(result));
}

VoteValueAndID VoteValueAndID::CreateVoteFromUint32(uint32_t serialized)
{
    uint32_t voteValue  = serialized & 0x000000FF;
    uint32_t proposalID = serialized >> 8;
    // this can never fail because the numbers are always smaller than the limits
    return CreateVote(proposalID, voteValue).UNWRAP();
}

std::string ProposalVote::ProposalVoteCreationErrorAsString(ProposalVoteCreationError error)
{
    switch (error) {
    case ProposalVoteCreationError::InvalidStartBlockHeight:
        return "The start block height has invalid value";
    case ProposalVoteCreationError::InvalidEndBlockHeight:
        return "The last block height has invalid value";
    case ProposalVoteCreationError::InvalidBlockHeightRange:
        return "The range provided is invalid; make sure the last block is larger or equal to the start "
               "height";
    case ProposalVoteCreationError::ProposalIDOutOfRange:
        return "The proposal ID value is invalid (out of allowed range)";
    case ProposalVoteCreationError::VoteValueOutOfRange:
        return "The vote value is invalid (out of allowed range)";
    }
    return "Unknown error";
}

bool VoteValueAndID::operator==(const VoteValueAndID& other) const
{
    return proposalID == other.proposalID && voteValue == other.voteValue;
}
