#include "googletest/googletest/include/gtest/gtest.h"

#include "environment.h"

#include "proposal.h"

TEST(proposal_tests, vote_creation)
{
    {
        const ProposalVote vote = ProposalVote::CreateVote(1, 10, 123, 1).UNWRAP();
        EXPECT_EQ(vote.getProposalID(), 123u);
        EXPECT_EQ(vote.getVoteValue(), 1u);
        EXPECT_EQ(vote.getFirstBlockHeight(), 1);
        EXPECT_EQ(vote.getLastBlockHeight(), 10);
    }
    {
        const ProposalVoteCreationError err = ProposalVote::CreateVote(-5, 10, 123, 1).UNWRAP_ERR();
        EXPECT_EQ(err, ProposalVoteCreationError::InvalidStartBlockHeight);
    }
    {
        const ProposalVoteCreationError err = ProposalVote::CreateVote(11, -5, 123, 1).UNWRAP_ERR();
        EXPECT_EQ(err, ProposalVoteCreationError::InvalidEndBlockHeight);
    }
    {
        const ProposalVoteCreationError err = ProposalVote::CreateVote(11, 10, 123, 1).UNWRAP_ERR();
        EXPECT_EQ(err, ProposalVoteCreationError::InvalidBlockHeightRange);
    }
    {
        const ProposalVoteCreationError err = ProposalVote::CreateVote(1, 10, 1 << 25, 1).UNWRAP_ERR();
        EXPECT_EQ(err, ProposalVoteCreationError::ProposalIDOutOfRange);
    }
    {
        const ProposalVoteCreationError err = ProposalVote::CreateVote(1, 10, 123, 260).UNWRAP_ERR();
        EXPECT_EQ(err, ProposalVoteCreationError::VoteValueOutOfRange);
    }
}

TEST(proposal_tests, votes_store)
{
    AllStoredVotes storedVotes;

    EXPECT_TRUE(storedVotes.empty());
    EXPECT_EQ(storedVotes.voteCount(), 0u);
    EXPECT_EQ(storedVotes.getAllVotes().size(), 0u);

    {
        {
            const uint32_t proposalID = 123u;
            const int      firstBlock = 5;
            const int      lastBlock  = 10;
            const uint32_t voteValue  = 1;

            {
                const ProposalVote vote =
                    ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
                EXPECT_EQ(vote.getProposalID(), proposalID);
                EXPECT_EQ(vote.getVoteValue(), voteValue);
                EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

                const auto storeResult = storedVotes.addVote(vote);
                EXPECT_FALSE(storeResult.isErr());
            }

            EXPECT_TRUE(storedVotes.proposalExists(proposalID));
            for (int i = firstBlock; i <= lastBlock; i++) {
                const auto vote = storedVotes.getProposalAtBlockHeight(i);

                ASSERT_TRUE(!!vote);

                EXPECT_EQ(vote->getProposalID(), proposalID);
                EXPECT_EQ(vote->getVoteValue(), voteValue);
                EXPECT_EQ(vote->getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote->getLastBlockHeight(), lastBlock);
            }

            EXPECT_FALSE(storedVotes.empty());
            EXPECT_EQ(storedVotes.voteCount(), 1u);
            EXPECT_EQ(storedVotes.getAllVotes().size(), 1u);
        }

        {
            const uint32_t proposalID = 555u;
            const int      firstBlock = 10;
            const int      lastBlock  = 20;
            const uint32_t voteValue  = 1;

            const ProposalVote vote =
                ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
            EXPECT_EQ(vote.getProposalID(), proposalID);
            EXPECT_EQ(vote.getVoteValue(), voteValue);
            EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
            EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

            const auto storeResult = storedVotes.addVote(vote);
            EXPECT_TRUE(storeResult.isErr());
            EXPECT_EQ(storeResult.UNWRAP_ERR(), AddVoteError::FirstBlockAlreadyInAnotherVote);

            EXPECT_FALSE(storedVotes.empty());
            EXPECT_EQ(storedVotes.voteCount(), 1u);
            EXPECT_EQ(storedVotes.getAllVotes().size(), 1u);
        }

        {
            const uint32_t proposalID = 555u;
            const int      firstBlock = 0;
            const int      lastBlock  = 5;
            const uint32_t voteValue  = 1;

            const ProposalVote vote =
                ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
            EXPECT_EQ(vote.getProposalID(), proposalID);
            EXPECT_EQ(vote.getVoteValue(), voteValue);
            EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
            EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

            const auto storeResult = storedVotes.addVote(vote);
            EXPECT_TRUE(storeResult.isErr());
            EXPECT_EQ(storeResult.UNWRAP_ERR(), AddVoteError::LastBlockAlreadyInAnotherVote);

            EXPECT_FALSE(storedVotes.empty());
            EXPECT_EQ(storedVotes.voteCount(), 1u);
            EXPECT_EQ(storedVotes.getAllVotes().size(), 1u);
        }

        {
            int      firstBlock = 20;
            int      lastBlock  = 1000;
            uint32_t proposalID = 555u;
            uint32_t voteValue  = 1;

            {

                {
                    const ProposalVote vote =
                        ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
                    EXPECT_EQ(vote.getProposalID(), proposalID);
                    EXPECT_EQ(vote.getVoteValue(), voteValue);
                    EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
                    EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

                    const auto storeResult = storedVotes.addVote(vote);
                    EXPECT_FALSE(storeResult.isErr());
                }

                EXPECT_TRUE(storedVotes.proposalExists(proposalID));
                for (int i = firstBlock; i <= lastBlock; i++) {
                    const auto vote = storedVotes.getProposalAtBlockHeight(i);

                    ASSERT_TRUE(!!vote);

                    EXPECT_EQ(vote->getProposalID(), proposalID);
                    EXPECT_EQ(vote->getVoteValue(), voteValue);
                    EXPECT_EQ(vote->getFirstBlockHeight(), firstBlock);
                    EXPECT_EQ(vote->getLastBlockHeight(), lastBlock);
                }

                EXPECT_FALSE(storedVotes.empty());
                EXPECT_EQ(storedVotes.voteCount(), 2u);
                EXPECT_EQ(storedVotes.getAllVotes().size(), 2u);

                // test consecutive assignment
                firstBlock               = lastBlock + 1;
                lastBlock                = firstBlock + 100;
                int someBlockInTheMiddle = firstBlock + (lastBlock - firstBlock) / 2;
                proposalID               = 666u;
                voteValue                = 7;
                {
                    const ProposalVote vote =
                        ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
                    EXPECT_EQ(vote.getProposalID(), proposalID);
                    EXPECT_EQ(vote.getVoteValue(), voteValue);
                    EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
                    EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

                    const auto storeResult = storedVotes.addVote(vote);
                    EXPECT_FALSE(storeResult.isErr());
                }

                EXPECT_TRUE(storedVotes.proposalExists(proposalID));
                for (int i = firstBlock; i <= lastBlock; i++) {
                    const auto vote = storedVotes.getProposalAtBlockHeight(i);

                    ASSERT_TRUE(!!vote);

                    EXPECT_EQ(vote->getProposalID(), proposalID);
                    EXPECT_EQ(vote->getVoteValue(), voteValue);
                    EXPECT_EQ(vote->getFirstBlockHeight(), firstBlock);
                    EXPECT_EQ(vote->getLastBlockHeight(), lastBlock);
                }

                EXPECT_FALSE(storedVotes.empty());
                EXPECT_EQ(storedVotes.voteCount(), 3u);
                EXPECT_EQ(storedVotes.getAllVotes().size(), 3u);

                // test consecutive assignment, again
                firstBlock = lastBlock + 1;
                lastBlock  = firstBlock + 200;
                proposalID = 555u;
                voteValue  = 7;
                {
                    const ProposalVote vote =
                        ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
                    EXPECT_EQ(vote.getProposalID(), proposalID);
                    EXPECT_EQ(vote.getVoteValue(), voteValue);
                    EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
                    EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

                    const auto storeResult = storedVotes.addVote(vote);
                    EXPECT_FALSE(storeResult.isErr());
                }

                EXPECT_TRUE(storedVotes.proposalExists(proposalID));
                for (int i = firstBlock; i <= lastBlock; i++) {
                    const auto vote = storedVotes.getProposalAtBlockHeight(i);

                    ASSERT_TRUE(!!vote);

                    EXPECT_EQ(vote->getProposalID(), proposalID);
                    EXPECT_EQ(vote->getVoteValue(), voteValue);
                    EXPECT_EQ(vote->getFirstBlockHeight(), firstBlock);
                    EXPECT_EQ(vote->getLastBlockHeight(), lastBlock);
                }

                EXPECT_FALSE(storedVotes.empty());
                EXPECT_EQ(storedVotes.voteCount(), 4u);
                EXPECT_EQ(storedVotes.getAllVotes().size(), 4u);

                // erase the middle range
                storedVotes.removeAllVotesAdjacentToHeight(someBlockInTheMiddle);
                EXPECT_FALSE(storedVotes.empty());
                EXPECT_EQ(storedVotes.getAllVotes().size(), 3u);

                // retrieve those votes and test they have the expected values
                const auto vote1 = storedVotes.getProposalAtBlockHeight(8);
                ASSERT_TRUE(vote1);
                EXPECT_EQ(vote1->getFirstBlockHeight(), 5);
                EXPECT_EQ(vote1->getLastBlockHeight(), 10);
                EXPECT_EQ(vote1->getProposalID(), 123u);
                EXPECT_EQ(vote1->getVoteValue(), 1u);
                const auto vote2 = storedVotes.getProposalAtBlockHeight(500);
                ASSERT_TRUE(vote2);
                EXPECT_EQ(vote2->getFirstBlockHeight(), 20);
                EXPECT_EQ(vote2->getLastBlockHeight(), 1000);
                EXPECT_EQ(vote2->getProposalID(), 555u);
                EXPECT_EQ(vote2->getVoteValue(), 1u);
                const auto vote3 = storedVotes.getProposalAtBlockHeight(1200);
                ASSERT_TRUE(vote3);
                EXPECT_EQ(vote3->getFirstBlockHeight(), 1102);
                EXPECT_EQ(vote3->getLastBlockHeight(), 1302);
                EXPECT_EQ(vote3->getProposalID(), 555u);
                EXPECT_EQ(vote3->getVoteValue(), 7u);
            }
        }
    }

    storedVotes.clear();

    EXPECT_TRUE(storedVotes.empty());
    EXPECT_EQ(storedVotes.voteCount(), 0u);
    EXPECT_EQ(storedVotes.getAllVotes().size(), 0u);
}

TEST(proposal_tests, remove_votes_of_proposal_id)
{
    AllStoredVotes storedVotes;

    EXPECT_TRUE(storedVotes.empty());
    EXPECT_EQ(storedVotes.voteCount(), 0u);
    EXPECT_EQ(storedVotes.getAllVotes().size(), 0u);

    {
        {
            const uint32_t proposalID = 123u;
            const int      firstBlock = 5;
            const int      lastBlock  = 10;
            const uint32_t voteValue  = 1;

            {
                const ProposalVote vote =
                    ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
                EXPECT_EQ(vote.getProposalID(), proposalID);
                EXPECT_EQ(vote.getVoteValue(), voteValue);
                EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

                const auto storeResult = storedVotes.addVote(vote);
                EXPECT_FALSE(storeResult.isErr());
            }

            EXPECT_TRUE(storedVotes.proposalExists(proposalID));
            for (int i = firstBlock; i <= lastBlock; i++) {
                const auto vote = storedVotes.getProposalAtBlockHeight(i);

                ASSERT_TRUE(!!vote);

                EXPECT_EQ(vote->getProposalID(), proposalID);
                EXPECT_EQ(vote->getVoteValue(), voteValue);
                EXPECT_EQ(vote->getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote->getLastBlockHeight(), lastBlock);
            }

            EXPECT_FALSE(storedVotes.empty());
            EXPECT_EQ(storedVotes.voteCount(), 1u);
            EXPECT_EQ(storedVotes.getAllVotes().size(), 1u);
        }

        {
            const uint32_t proposalID = 123u;
            const int      firstBlock = 12;
            const int      lastBlock  = 50;
            const uint32_t voteValue  = 1;

            {
                const ProposalVote vote =
                    ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
                EXPECT_EQ(vote.getProposalID(), proposalID);
                EXPECT_EQ(vote.getVoteValue(), voteValue);
                EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

                const auto storeResult = storedVotes.addVote(vote);
                EXPECT_FALSE(storeResult.isErr());
            }

            EXPECT_TRUE(storedVotes.proposalExists(proposalID));
            for (int i = firstBlock; i <= lastBlock; i++) {
                const auto vote = storedVotes.getProposalAtBlockHeight(i);

                ASSERT_TRUE(!!vote);

                EXPECT_EQ(vote->getProposalID(), proposalID);
                EXPECT_EQ(vote->getVoteValue(), voteValue);
                EXPECT_EQ(vote->getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote->getLastBlockHeight(), lastBlock);
            }

            EXPECT_FALSE(storedVotes.empty());
            EXPECT_EQ(storedVotes.voteCount(), 2u);
            EXPECT_EQ(storedVotes.getAllVotes().size(), 2u);
        }

        {
            const uint32_t proposalID = 2222u;
            const int      firstBlock = 51;
            const int      lastBlock  = 100;
            const uint32_t voteValue  = 1;

            {
                const ProposalVote vote =
                    ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
                EXPECT_EQ(vote.getProposalID(), proposalID);
                EXPECT_EQ(vote.getVoteValue(), voteValue);
                EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

                const auto storeResult = storedVotes.addVote(vote);
                EXPECT_FALSE(storeResult.isErr());
            }

            EXPECT_TRUE(storedVotes.proposalExists(proposalID));
            for (int i = firstBlock; i <= lastBlock; i++) {
                const auto vote = storedVotes.getProposalAtBlockHeight(i);

                ASSERT_TRUE(!!vote);

                EXPECT_EQ(vote->getProposalID(), proposalID);
                EXPECT_EQ(vote->getVoteValue(), voteValue);
                EXPECT_EQ(vote->getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote->getLastBlockHeight(), lastBlock);
            }

            EXPECT_FALSE(storedVotes.empty());
            EXPECT_EQ(storedVotes.voteCount(), 3u);
            EXPECT_EQ(storedVotes.getAllVotes().size(), 3u);
        }

        {
            const uint32_t proposalID = 123u;
            const int      firstBlock = 101;
            const int      lastBlock  = 200;
            const uint32_t voteValue  = 1;

            {
                const ProposalVote vote =
                    ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
                EXPECT_EQ(vote.getProposalID(), proposalID);
                EXPECT_EQ(vote.getVoteValue(), voteValue);
                EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

                const auto storeResult = storedVotes.addVote(vote);
                EXPECT_FALSE(storeResult.isErr());
            }

            EXPECT_TRUE(storedVotes.proposalExists(proposalID));
            for (int i = firstBlock; i <= lastBlock; i++) {
                const auto vote = storedVotes.getProposalAtBlockHeight(i);

                ASSERT_TRUE(!!vote);

                EXPECT_EQ(vote->getProposalID(), proposalID);
                EXPECT_EQ(vote->getVoteValue(), voteValue);
                EXPECT_EQ(vote->getFirstBlockHeight(), firstBlock);
                EXPECT_EQ(vote->getLastBlockHeight(), lastBlock);
            }

            EXPECT_FALSE(storedVotes.empty());
            EXPECT_EQ(storedVotes.voteCount(), 4u);
            EXPECT_EQ(storedVotes.getAllVotes().size(), 4u);
        }
    }

    storedVotes.removeAllVotesOfProposal(123);
    EXPECT_FALSE(storedVotes.empty());
    EXPECT_EQ(storedVotes.voteCount(), 1u);
    EXPECT_EQ(storedVotes.getAllVotes().size(), 1u);

    storedVotes.clear();
    EXPECT_TRUE(storedVotes.empty());
    EXPECT_EQ(storedVotes.voteCount(), 0u);
    EXPECT_EQ(storedVotes.getAllVotes().size(), 0u);
}

TEST(proposal_tests, interval_joining_and_cutting)
{
    AllStoredVotes storedVotes;

    EXPECT_TRUE(storedVotes.empty());
    EXPECT_EQ(storedVotes.voteCount(), 0u);
    EXPECT_EQ(storedVotes.getAllVotes().size(), 0u);

    {
        const uint32_t proposalID = 555u;
        const int      firstBlock = 10;
        const int      lastBlock  = 20;
        const uint32_t voteValue  = 99;

        const ProposalVote vote =
            ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
        EXPECT_EQ(vote.getProposalID(), proposalID);
        EXPECT_EQ(vote.getVoteValue(), voteValue);
        EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
        EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

        const auto storeResult = storedVotes.addVote(vote);
        EXPECT_TRUE(storeResult.isOk());

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.voteCount(), 1u);
        EXPECT_EQ(storedVotes.getAllVotes().size(), 1u);
    }

    {
        const uint32_t proposalID = 555u;
        const int      firstBlock = 30;
        const int      lastBlock  = 40;
        const uint32_t voteValue  = 99;

        const ProposalVote vote =
            ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
        EXPECT_EQ(vote.getProposalID(), proposalID);
        EXPECT_EQ(vote.getVoteValue(), voteValue);
        EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
        EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

        const auto storeResult = storedVotes.addVote(vote);
        EXPECT_TRUE(storeResult.isOk());

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.getAllVotes().size(), 2u);
    }

    // now we fill the middle with a different vote, nothing special happens
    {
        const uint32_t proposalID = 555u;
        const int      firstBlock = 21;
        const int      lastBlock  = 29;
        const uint32_t voteValue  = 1;

        const ProposalVote vote =
            ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
        EXPECT_EQ(vote.getProposalID(), proposalID);
        EXPECT_EQ(vote.getVoteValue(), voteValue);
        EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
        EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

        const auto storeResult = storedVotes.addVote(vote);
        EXPECT_TRUE(storeResult.isOk());

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.getAllVotes().size(), 3u);
    }

    {
        storedVotes.removeAllVotesAdjacentToHeight(25);

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.voteCount(), 2u);
        EXPECT_EQ(storedVotes.getAllVotes().size(), 2u);
    }

    // now we add an interval that joins the one with the intervals on its sides,
    // because all values are the same
    {
        const uint32_t proposalID = 555u;
        const int      firstBlock = 21;
        const int      lastBlock  = 29;
        const uint32_t voteValue  = 99;

        const ProposalVote vote =
            ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
        EXPECT_EQ(vote.getProposalID(), proposalID);
        EXPECT_EQ(vote.getVoteValue(), voteValue);
        EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
        EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

        const auto storeResult = storedVotes.addVote(vote);
        EXPECT_TRUE(storeResult.isOk());

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.voteCount(), 1u);
        EXPECT_EQ(storedVotes.getAllVotes().size(), 1u);
    }

    // remove a range and expect it to split the current range
    {
        const uint32_t proposalID = 555u;
        const uint32_t voteValue  = 99;

        storedVotes.removeVotesAtHeightRange(23, 32);

        const std::vector<ProposalVote> votes = storedVotes.getAllVotes();

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.voteCount(), 2u);
        ASSERT_EQ(votes.size(), 2u);

        EXPECT_EQ(votes[0].getFirstBlockHeight(), 10);
        EXPECT_EQ(votes[0].getLastBlockHeight(), 22);
        EXPECT_EQ(votes[0].getProposalID(), proposalID);
        EXPECT_EQ(votes[0].getVoteValue(), voteValue);

        EXPECT_EQ(votes[1].getFirstBlockHeight(), 33);
        EXPECT_EQ(votes[1].getLastBlockHeight(), 40);
        EXPECT_EQ(votes[1].getProposalID(), proposalID);
        EXPECT_EQ(votes[1].getVoteValue(), voteValue);
    }

    // remove a range, part of which doesn't exist
    {
        const uint32_t proposalID = 555u;
        const uint32_t voteValue  = 99;

        storedVotes.removeVotesAtHeightRange(27, 37);

        const std::vector<ProposalVote> votes = storedVotes.getAllVotes();

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.voteCount(), 2u);
        ASSERT_EQ(votes.size(), 2u);

        EXPECT_EQ(votes[0].getFirstBlockHeight(), 10);
        EXPECT_EQ(votes[0].getLastBlockHeight(), 22);
        EXPECT_EQ(votes[0].getProposalID(), proposalID);
        EXPECT_EQ(votes[0].getVoteValue(), voteValue);

        EXPECT_EQ(votes[1].getFirstBlockHeight(), 38);
        EXPECT_EQ(votes[1].getLastBlockHeight(), 40);
        EXPECT_EQ(votes[1].getProposalID(), proposalID);
        EXPECT_EQ(votes[1].getVoteValue(), voteValue);
    }

    {
        storedVotes.clear();

        EXPECT_TRUE(storedVotes.empty());
        EXPECT_EQ(storedVotes.voteCount(), 0u);
        EXPECT_EQ(storedVotes.getAllVotes().size(), 0u);
    }
}

TEST(proposal_tests, serializationToUin32)
{
    const VoteValueAndID vote = VoteValueAndID::CreateVote(123, 1).UNWRAP();
    EXPECT_EQ(vote.getProposalID(), 123u);
    EXPECT_EQ(vote.getVoteValue(), 1u);

    const uint32_t serialized = vote.serializeToUint32();

    const VoteValueAndID deserializedVote = VoteValueAndID::CreateVoteFromUint32(serialized);

    EXPECT_EQ(deserializedVote, vote) << " ERROR: failed for vote value " << vote.getVoteValue()
                                      << " and proposal ID " << vote.getProposalID();
}

TEST(proposal_tests, votesToAndFromJson)
{
    AllStoredVotes storedVotes;

    EXPECT_TRUE(storedVotes.empty());
    EXPECT_EQ(storedVotes.voteCount(), 0u);
    EXPECT_EQ(storedVotes.getAllVotes().size(), 0u);

    {
        const uint32_t proposalID = 555u;
        const int      firstBlock = 10;
        const int      lastBlock  = 20;
        const uint32_t voteValue  = 99;

        const ProposalVote vote =
            ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
        EXPECT_EQ(vote.getProposalID(), proposalID);
        EXPECT_EQ(vote.getVoteValue(), voteValue);
        EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
        EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

        const auto storeResult = storedVotes.addVote(vote);
        EXPECT_TRUE(storeResult.isOk());

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.voteCount(), 1u);
        EXPECT_EQ(storedVotes.getAllVotes().size(), 1u);
    }

    {
        const uint32_t proposalID = 555u;
        const int      firstBlock = 30;
        const int      lastBlock  = 40;
        const uint32_t voteValue  = 99;

        const ProposalVote vote =
            ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
        EXPECT_EQ(vote.getProposalID(), proposalID);
        EXPECT_EQ(vote.getVoteValue(), voteValue);
        EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
        EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

        const auto storeResult = storedVotes.addVote(vote);
        EXPECT_TRUE(storeResult.isOk());

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.getAllVotes().size(), 2u);
        EXPECT_EQ(storedVotes.getAllVotes().size(), 2u);
    }

    {
        const uint32_t proposalID = 222u;
        const int      firstBlock = 21;
        const int      lastBlock  = 29;
        const uint32_t voteValue  = 99;

        const ProposalVote vote =
            ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue).UNWRAP();
        EXPECT_EQ(vote.getProposalID(), proposalID);
        EXPECT_EQ(vote.getVoteValue(), voteValue);
        EXPECT_EQ(vote.getFirstBlockHeight(), firstBlock);
        EXPECT_EQ(vote.getLastBlockHeight(), lastBlock);

        const auto storeResult = storedVotes.addVote(vote);
        EXPECT_TRUE(storeResult.isOk());

        EXPECT_FALSE(storedVotes.empty());
        EXPECT_EQ(storedVotes.getAllVotes().size(), 3u);
        EXPECT_EQ(storedVotes.getAllVotes().size(), 3u);
    }

    const json_spirit::Array        jsonVotes           = storedVotes.getAllVotesAsJson();
    const std::string               serializedJsonVotes = json_spirit::write_formatted(jsonVotes);
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_FALSE(importResult.isErr()) << " got error: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(storedVotes.getAllVotes(), importedVotes.getAllVotes());
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_NoErrors)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : 10,
        "LastVoteBlock" : 20,
        "ProposalID" : 555,
        "VoteValue" : 99
    },
    {
        "FirstVoteBlock" : 21,
        "LastVoteBlock" : 29,
        "ProposalID" : 102,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 2
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_FALSE(importResult.isErr()) << " got error: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 3u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 10);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 20);
    EXPECT_EQ(votesVec[0].getProposalID(), 555u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 99u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 21);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 29);
    EXPECT_EQ(votesVec[1].getProposalID(), 102u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 222u);
    EXPECT_EQ(votesVec[2].getFirstBlockHeight(), 30);
    EXPECT_EQ(votesVec[2].getLastBlockHeight(), 40);
    EXPECT_EQ(votesVec[2].getProposalID(), 99u);
    EXPECT_EQ(votesVec[2].getVoteValue(), 2u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_NegativeFirstHeight)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : 10,
        "LastVoteBlock" : -10,
        "ProposalID" : 555,
        "VoteValue" : 99
    },
    {
        "FirstVoteBlock" : 21,
        "LastVoteBlock" : 29,
        "ProposalID" : 102,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 2
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_TRUE(boost::algorithm::icontains(importResult.UNWRAP_ERR(), "Last block"))
        << "; instead error found: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 2u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 21);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 29);
    EXPECT_EQ(votesVec[0].getProposalID(), 102u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 222u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 30);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 40);
    EXPECT_EQ(votesVec[1].getProposalID(), 99u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 2u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_NegativeLastHeight)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : -10,
        "LastVoteBlock" : 10,
        "ProposalID" : 555,
        "VoteValue" : 99
    },
    {
        "FirstVoteBlock" : 21,
        "LastVoteBlock" : 29,
        "ProposalID" : 102,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 2
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_TRUE(boost::algorithm::icontains(importResult.UNWRAP_ERR(), "First block"))
        << "; instead error found: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 2u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 21);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 29);
    EXPECT_EQ(votesVec[0].getProposalID(), 102u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 222u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 30);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 40);
    EXPECT_EQ(votesVec[1].getProposalID(), 99u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 2u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_TooBigProposalID)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : 10,
        "LastVoteBlock" : 20,
        "ProposalID" : 555,
        "VoteValue" : 99
    },
    {
        "FirstVoteBlock" : 21,
        "LastVoteBlock" : 29,
        "ProposalID" : 33554432,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 2
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_TRUE(boost::algorithm::icontains(importResult.UNWRAP_ERR(), "Proposal ID"))
        << "; instead error found: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 2u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 10);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 20);
    EXPECT_EQ(votesVec[0].getProposalID(), 555u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 99u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 30);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 40);
    EXPECT_EQ(votesVec[1].getProposalID(), 99u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 2u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_LargeVoteValue)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : 10,
        "LastVoteBlock" : 20,
        "ProposalID" : 555,
        "VoteValue" : 99
    },
    {
        "FirstVoteBlock" : 21,
        "LastVoteBlock" : 29,
        "ProposalID" : 102,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 666
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_TRUE(boost::algorithm::icontains(importResult.UNWRAP_ERR(), "Vote value"))
        << "; instead error found: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 2u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 10);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 20);
    EXPECT_EQ(votesVec[0].getProposalID(), 555u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 99u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 21);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 29);
    EXPECT_EQ(votesVec[1].getProposalID(), 102u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 222u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_VoteValueMissing)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : 10,
        "LastVoteBlock" : 20,
        "ProposalID" : 555
    },
    {
        "FirstVoteBlock" : 21,
        "LastVoteBlock" : 29,
        "ProposalID" : 102,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 2
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_TRUE(boost::algorithm::icontains(importResult.UNWRAP_ERR(), "Vote value"))
        << "; instead error found: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 2u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 21);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 29);
    EXPECT_EQ(votesVec[0].getProposalID(), 102u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 222u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 30);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 40);
    EXPECT_EQ(votesVec[1].getProposalID(), 99u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 2u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_ProposalIDMissing)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : 10,
        "LastVoteBlock" : 20,
        "ProposalID" : 555,
        "VoteValue" : 99
    },
    {
        "FirstVoteBlock" : 21,
        "LastVoteBlock" : 29,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 2
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_TRUE(boost::algorithm::icontains(importResult.UNWRAP_ERR(), "Proposal ID"))
        << "; instead error found: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 2u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 10);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 20);
    EXPECT_EQ(votesVec[0].getProposalID(), 555u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 99u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 30);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 40);
    EXPECT_EQ(votesVec[1].getProposalID(), 99u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 2u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_FirstBlockMissing)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : 10,
        "LastVoteBlock" : 20,
        "ProposalID" : 555,
        "VoteValue" : 99
    },
    {
        "LastVoteBlock" : 29,
        "ProposalID" : 102,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 2
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_TRUE(boost::algorithm::icontains(importResult.UNWRAP_ERR(), "First block"))
        << "; instead error found: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 2u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 10);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 20);
    EXPECT_EQ(votesVec[0].getProposalID(), 555u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 99u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 30);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 40);
    EXPECT_EQ(votesVec[1].getProposalID(), 99u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 2u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_LastBlockMissing)
{
    const std::string               serializedJsonVotes = R"(
[
    {
        "FirstVoteBlock" : 10,
        "LastVoteBlock" : 20,
        "ProposalID" : 555,
        "VoteValue" : 99
    },
    {
        "FirstVoteBlock" : 21,
        "ProposalID" : 102,
        "VoteValue" : 222
    },
    {
        "FirstVoteBlock" : 30,
        "LastVoteBlock" : 40,
        "ProposalID" : 99,
        "VoteValue" : 2
    }
]
)";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_TRUE(boost::algorithm::icontains(importResult.UNWRAP_ERR(), "Last block"))
        << "; instead error found: " << importResult.UNWRAP_ERR();
    EXPECT_EQ(importedVotes.voteCount(), 2u);
    const auto votesVec = importedVotes.getAllVotes();
    EXPECT_EQ(votesVec[0].getFirstBlockHeight(), 10);
    EXPECT_EQ(votesVec[0].getLastBlockHeight(), 20);
    EXPECT_EQ(votesVec[0].getProposalID(), 555u);
    EXPECT_EQ(votesVec[0].getVoteValue(), 99u);
    EXPECT_EQ(votesVec[1].getFirstBlockHeight(), 30);
    EXPECT_EQ(votesVec[1].getLastBlockHeight(), 40);
    EXPECT_EQ(votesVec[1].getProposalID(), 99u);
    EXPECT_EQ(votesVec[1].getVoteValue(), 2u);
}

TEST(proposal_tests, votesToAndFromJsonWithErrors_InvalidJson)
{
    const std::string               serializedJsonVotes = R"([sdsd])";
    AllStoredVotes                  importedVotes;
    const Result<void, std::string> importResult =
        importedVotes.importVotesFromJson(serializedJsonVotes);
    ASSERT_TRUE(importResult.isErr());
    EXPECT_EQ(importedVotes.voteCount(), 0u);
}
