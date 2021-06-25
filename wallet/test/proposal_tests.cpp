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

                // erase the middle range
                storedVotes.removeProposalAtHeight(someBlockInTheMiddle);
                EXPECT_FALSE(storedVotes.empty());
                EXPECT_EQ(storedVotes.voteCount(), 3u);

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
}
