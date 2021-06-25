#include "proposal.h"

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
    if (ProposalID >= UINT32_C(1) << 24) {
        return Err(ProposalVoteCreationError::ProposalIDOutOfRange);
    }
    if (VoteValue >= UINT32_C(1) << 8) {
        return Err(ProposalVoteCreationError::VoteValueOutOfRange);
    }

    ProposalVote result;
    result.firstBlockHeight = FromBlock;
    result.lastBlockHeight  = ToBlock;
    result.proposalID       = ProposalID;
    result.voteValue        = VoteValue;

    return Ok(std::move(result));
}

uint32_t ProposalVote::getProposalID() const { return proposalID; }

int ProposalVote::getFirstBlockHeight() const { return firstBlockHeight; }

int ProposalVote::getLastBlockHeight() const { return lastBlockHeight; }

uint32_t ProposalVote::getVoteValue() const { return voteValue; }

bool ProposalVote::operator==(const ProposalVote& other) const
{
    return firstBlockHeight == other.firstBlockHeight && lastBlockHeight == other.lastBlockHeight &&
           proposalID == other.proposalID && voteValue == other.voteValue;
}

Result<void, AddVoteError> AllStoredVotes::addVote(const ProposalVote& vote)
{
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
    votes.insert(std::make_pair(interval, vote));
    return Ok();
}

void AllStoredVotes::removeProposalAtHeight(int someHeightInIt)
{
    auto it = votes.find(someHeightInIt);
    if (it != votes.end()) {
        votes.erase(it->first.closed(it->second.getFirstBlockHeight(), it->second.getLastBlockHeight()));
    }
}

boost::optional<ProposalVote> AllStoredVotes::getProposalAtBlockHeight(int height) const
{
    auto it = votes.find(height);
    if (it != votes.end()) {
        return boost::make_optional(it->second);
    }
    return boost::none;
}

bool AllStoredVotes::proposalExists(uint32_t proposalID) const
{
    for (const auto& el : votes) {
        if (el.second.getProposalID() == proposalID) {
            return true;
        }
    }
    return false;
}

bool AllStoredVotes::empty() const { return votes.empty(); }

std::size_t AllStoredVotes::voteCount() const { return std::distance(votes.begin(), votes.end()); }
