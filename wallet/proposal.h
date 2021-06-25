#ifndef PROPOSAL_H
#define PROPOSAL_H

#include "result.h"
#include <boost/icl/interval_map.hpp>
#include <boost/optional/optional.hpp>
#include <cinttypes>
#include <set>

enum class AddVoteError
{
    FirstBlockAlreadyInAnotherVote,
    LastBlockAlreadyInAnotherVote,
};

enum class ProposalVoteCreationError
{
    InvalidStartBlockHeight,
    InvalidEndBlockHeight,
    InvalidBlockHeightRange,
    ProposalIDOutOfRange,
    VoteValueOutOfRange,
};

class ProposalVote
{
    int      firstBlockHeight;
    int      lastBlockHeight;
    uint32_t proposalID;
    uint32_t voteValue;

public:
    static Result<ProposalVote, ProposalVoteCreationError>
    CreateVote(int FromBlock, int ToBlock, uint32_t ProposalID, uint32_t VoteValue);

    uint32_t getProposalID() const;
    int      getFirstBlockHeight() const;
    int      getLastBlockHeight() const;
    uint32_t getVoteValue() const;

    bool operator==(const ProposalVote& other) const;
};

class AllStoredVotes
{
    boost::icl::interval_map<int, ProposalVote> votes;

public:
    [[nodiscard]] Result<void, AddVoteError>    addVote(const ProposalVote& vote);
    void                                        removeProposalAtHeight(int someHeightInIt);
    [[nodiscard]] boost::optional<ProposalVote> getProposalAtBlockHeight(int height) const;
    [[nodiscard]] bool                          proposalExists(uint32_t proposalID) const;
    [[nodiscard]] bool                          empty() const;
    [[nodiscard]] std::size_t                   voteCount() const;
};

#endif // PROPOSAL_H
