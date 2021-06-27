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

    const auto interval =
        boost::icl::interval<int>::closed(vote.getFirstBlockHeight(), vote.getLastBlockHeight());
    votes.insert(std::make_pair(interval, vote.getVoteValueAndProposalID()));
    return Ok();
}

void AllStoredVotes::removeProposalAtHeight(int someHeightInIt)
{
    std::lock_guard<std::mutex> lg(mtx);

    auto it = votes.find(someHeightInIt);
    if (it != votes.end()) {
        votes.erase(it->first);
    }
}

boost::optional<ProposalVote> AllStoredVotes::getProposalAtBlockHeight(int height) const
{
    std::lock_guard<std::mutex> lg(mtx);

    auto it = votes.find(height);
    if (it != votes.end()) {
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
    return boost::none;
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

bool VoteValueAndID::operator==(const VoteValueAndID& other) const
{
    return proposalID == other.proposalID && voteValue == other.voteValue;
}
