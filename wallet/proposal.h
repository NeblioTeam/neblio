﻿#ifndef PROPOSAL_H
#define PROPOSAL_H

#include "json_spirit.h"
#include "result.h"
#include <boost/icl/interval_map.hpp>
#include <boost/optional/optional.hpp>
#include <cinttypes>
#include <mutex>
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

class VoteValueAndID
{
    uint32_t proposalID;
    uint32_t voteValue;

public:
    [[nodiscard]] uint32_t getProposalID() const;
    [[nodiscard]] uint32_t getVoteValue() const;
    [[nodiscard]] uint32_t serializeToUint32() const;
    [[nodiscard]] static Result<VoteValueAndID, ProposalVoteCreationError>
                                        CreateVote(uint32_t ProposalID, uint32_t VoteValue);
    [[nodiscard]] static VoteValueAndID CreateVoteFromUint32(uint32_t serialized);

    [[nodiscard]] bool operator==(const VoteValueAndID& other) const;
};

class ProposalVote
{
    int            firstBlockHeight;
    int            lastBlockHeight;
    VoteValueAndID valueAndID;

public:
    [[nodiscard]] static Result<ProposalVote, ProposalVoteCreationError>
    CreateVote(int FromBlock, int ToBlock, uint32_t ProposalID, uint32_t VoteValue);

    [[nodiscard]] uint32_t           getProposalID() const;
    [[nodiscard]] int                getFirstBlockHeight() const;
    [[nodiscard]] int                getLastBlockHeight() const;
    [[nodiscard]] uint32_t           getVoteValue() const;
    [[nodiscard]] VoteValueAndID     getVoteValueAndProposalID() const;
    [[nodiscard]] json_spirit::Value asJson() const;
    [[nodiscard]] static std::string ProposalVoteCreationErrorAsString(ProposalVoteCreationError error);
};

class AllStoredVotes
{
    boost::icl::interval_map<int, VoteValueAndID> votes;
    mutable std::mutex                            mtx;

    boost::optional<ProposalVote> voteFromIterator(decltype(votes)::const_iterator it) const;

public:
    [[nodiscard]] Result<void, AddVoteError>    addVote(const ProposalVote& vote);
    void                                        removeVoteRangeAtHeight(int someHeightInIt);
    [[nodiscard]] boost::optional<ProposalVote> getProposalAtBlockHeight(int height) const;
    [[nodiscard]] std::vector<ProposalVote>     getAllVotes() const;
    [[nodiscard]] json_spirit::Array            getAllVotesAsJson() const;
    [[nodiscard]] bool                          proposalExists(uint32_t proposalID) const;
    [[nodiscard]] bool                          empty() const;
    [[nodiscard]] std::size_t                   voteCount() const;
    void                                        clear();
    [[nodiscard]] static std::string
    AddVoteErrorAsString(AddVoteError error, const boost::optional<int>& startHeight = boost::none,
                         const boost::optional<int>& lastHeight = boost::none);
};

#endif // PROPOSAL_H
