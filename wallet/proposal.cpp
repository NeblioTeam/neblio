#include "proposal.h"

#include "logging/logger.h"
#include "ntp1/ntp1tools.h"
#include "stringmanip.h"
#include "util.h"

static constexpr const char* FIRST_BLOCK_JSON_KEY = "FirstVoteBlock";
static constexpr const char* LAST_BLOCK_JSON_KEY  = "LastVoteBlock";
static constexpr const char* VOTE_VALUE_JSON_KEY  = "VoteValue";
static constexpr const char* PROP_ID_JSON_KEY     = "ProposalID";

static constexpr const char* VOTES_DB_FILENAME = "votes.json";

Result<ProposalVote, ProposalVoteCreationError>
ProposalVote::CreateVote(int FromBlock, int ToBlock, uint32_t ProposalID, uint32_t VoteValue)
{
    TRYV(ValidateStartBlock(FromBlock));
    TRYV(ValidateLastBlock(ToBlock));

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

    root.push_back(json_spirit::Pair(FIRST_BLOCK_JSON_KEY, getFirstBlockHeight()));
    root.push_back(json_spirit::Pair(LAST_BLOCK_JSON_KEY, getLastBlockHeight()));
    root.push_back(json_spirit::Pair(VOTE_VALUE_JSON_KEY, static_cast<int>(getVoteValue())));
    root.push_back(json_spirit::Pair(PROP_ID_JSON_KEY, static_cast<int>(getProposalID())));

    return json_spirit::Value(std::move(root));
}

Result<void, std::string> JsonTypeOrError(const json_spirit::Value&     val,
                                          const json_spirit::Value_type expected_type,
                                          const StringViewT             objName)
{
    if (val.type() != expected_type) {
        return Err(std::string(fmt::format("{} expected json type {}, found {}", objName.to_string(),
                                           json_spirit::Value_type_name[expected_type],
                                           json_spirit::Value_type_name[val.type()])));
    }
    return Ok();
}

Result<ProposalVote, std::string> ProposalVote::FromJson(const json_spirit::Value& value)
{
    if (value.type() != json_spirit::obj_type) {
        return Err(std::string("Vote type is not a json object"));
    }
    const json_spirit::Object voteObj = value.get_obj();
    json_spirit::Value        val;
    val = json_spirit::find_value(voteObj, FIRST_BLOCK_JSON_KEY);
    TRYV(JsonTypeOrError(val, json_spirit::int_type, "First block"));
    const int firstBlock = val.get_int();
    //////////
    val = json_spirit::find_value(voteObj, LAST_BLOCK_JSON_KEY);
    TRYV(JsonTypeOrError(val, json_spirit::int_type, "Last block"));
    const int lastBlock = val.get_int();
    //////////
    val = json_spirit::find_value(voteObj, PROP_ID_JSON_KEY);
    TRYV(JsonTypeOrError(val, json_spirit::int_type, "Proposal ID"));
    const uint32_t proposalID = static_cast<uint32_t>(val.get_int());
    //////////
    val = json_spirit::find_value(voteObj, VOTE_VALUE_JSON_KEY);
    TRYV(JsonTypeOrError(val, json_spirit::int_type, "Vote value"));
    const uint32_t voteValue = static_cast<uint32_t>(val.get_int());
    //////////
    const Result<ProposalVote, ProposalVoteCreationError> voteCreateResult =
        ProposalVote::CreateVote(firstBlock, lastBlock, proposalID, voteValue);
    if (voteCreateResult.isErr()) {
        return Err(ProposalVoteCreationErrorAsString(voteCreateResult.UNWRAP_ERR()));
    }
    return Ok(voteCreateResult.UNWRAP());
}

AllStoredVotes::AllStoredVotes(const AllStoredVotes& other)
{
    std::unique_lock<std::mutex> lock1(mtx, std::defer_lock);
    std::unique_lock<std::mutex> lock2(other.mtx, std::defer_lock);
    std::lock(lock1, lock2);
    votes = other.votes;
}

AllStoredVotes::AllStoredVotes(AllStoredVotes&& other)
{
    std::unique_lock<std::mutex> lock1(mtx, std::defer_lock);
    std::unique_lock<std::mutex> lock2(other.mtx, std::defer_lock);
    std::lock(lock1, lock2);
    votes = std::move(other.votes);
}

AllStoredVotes& AllStoredVotes::operator=(const AllStoredVotes& other)
{
    if (&other == this) {
        return *this;
    }
    std::unique_lock<std::mutex> lock1(mtx, std::defer_lock);
    std::unique_lock<std::mutex> lock2(other.mtx, std::defer_lock);
    std::lock(lock1, lock2);
    votes = other.votes;
    return *this;
}

AllStoredVotes& AllStoredVotes::operator=(AllStoredVotes&& other)
{
    if (&other == this) {
        return *this;
    }
    std::unique_lock<std::mutex> lock1(mtx, std::defer_lock);
    std::unique_lock<std::mutex> lock2(other.mtx, std::defer_lock);
    std::lock(lock1, lock2);
    votes = std::move(other.votes);
    return *this;
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

void AllStoredVotes::removeAllVotesAdjacentToHeight(int someHeightInIt)
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

    const std::size_t initialCount = voteCount_unsafe();

    votes.erase(boost::icl::discrete_interval<int>::open(startHeight - 1, lastHeight + 1));
}

void AllStoredVotes::removeAllVotesOfProposal(uint32_t proposalID)
{
    std::lock_guard<std::mutex> lg(mtx);

    auto it = votes.begin();
    while (it != votes.end()) {
        if (it->second.getProposalID() == proposalID) {
            votes.erase(it);
            // the iterator is invalidated after erasure, so we start from the beginning again
            it = votes.begin();
        } else {
            ++it;
        }
    }
}

boost::optional<ProposalVote>
AllStoredVotes::voteFromIterator_unsafe(decltype(votes)::const_iterator it) const
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
        return voteFromIterator_unsafe(it);
    }
    return boost::none;
}

std::vector<ProposalVote> AllStoredVotes::getAllVotes_unsafe() const
{
    std::vector<ProposalVote> result;
    for (auto it = votes.begin(); it != votes.end(); ++it) {
        const auto& vote = voteFromIterator_unsafe(it);
        if (vote) {
            result.push_back(*vote);
        }
    }
    return result;
}

std::vector<ProposalVote> AllStoredVotes::getAllVotes() const
{
    std::lock_guard<std::mutex> lg(mtx);
    return getAllVotes_unsafe();
}

json_spirit::Array AllStoredVotes::getAllVotesAsJson_unsafe() const
{
    const std::vector<ProposalVote> votesVec = getAllVotes_unsafe();
    json_spirit::Array              result;
    for (auto&& vote : votesVec) {
        result.push_back(vote.asJson());
    }
    return result;
}

json_spirit::Array AllStoredVotes::getAllVotesAsJson() const
{
    std::lock_guard<std::mutex> lg(mtx);
    return getAllVotesAsJson_unsafe();
}

boost::optional<ProposalVote> AllStoredVotes::getProposalAtIndex(std::size_t index) const
{
    std::lock_guard<std::mutex> lg(mtx);
    auto                        it = votes.begin();
    if (index >= voteCount_unsafe()) {
        return boost::none;
    }
    std::advance(it, index);
    return voteFromIterator_unsafe(it);
}

void AllStoredVotes::writeAllVotesAsJsonToDataDir() const
{
    std::lock_guard<std::mutex> lg(mtx);
    const std::string           votesFilename = GetStorageVotesFileName();
    const json_spirit::Value    votesValues         = getAllVotesAsJson_unsafe();
    const std::string           jsonData      = json_spirit::write_formatted(votesValues);
    boost::filesystem::save_string_file(votesFilename, jsonData);
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
    return voteCount_unsafe();
}

std::size_t AllStoredVotes::voteCount_unsafe() const
{
    return std::distance(votes.begin(), votes.end());
}

void AllStoredVotes::clear() { votes.clear(); }

Result<void, std::string> AllStoredVotes::importVotesFromJson(const std::string& voteJsonData)
{
    json_spirit::Value parsed;
    try {
        json_spirit::read_or_throw(voteJsonData, parsed);
    } catch (const std::exception& ex) {
        return Err(std::string(ex.what()));
    }

    if (parsed.type() != json_spirit::array_type) {
        return Err(std::string("Outer type should be an array of objects"));
    }

    std::string errors;

    const json_spirit::Array array = parsed.get_array();
    for (const auto& el : array) {
        const Result<ProposalVote, std::string> res = ProposalVote::FromJson(el);
        if (res.isErr()) {
            errors += res.UNWRAP_ERR();
            continue;
        }
        const Result<void, AddVoteError> addRes = addVote(res.UNWRAP());
        if (addRes.isErr()) {
            errors += AddVoteErrorAsString(addRes.UNWRAP_ERR());
        }
    }

    if (!errors.empty()) {
        return Err(errors);
    }
    return Ok();
}

Result<AllStoredVotes, std::string>
AllStoredVotes::CreateFromJsonFileData(const std::string& voteJsonData)
{
    AllStoredVotes result;
    TRYV(result.importVotesFromJson(voteJsonData));
    return Ok(std::move(result));
}

Result<AllStoredVotes, std::string> AllStoredVotes::CreateFromJsonFileFromWalletDir()
{
    const std::string filename = GetStorageVotesFileName();
    std::string       filedata;
    boost::filesystem::load_string_file(filename, filedata);
    return CreateFromJsonFileData(filedata);
}

std::string AllStoredVotes::GetStorageVotesFileName()
{
    const boost::filesystem::path datadir = GetDataDir();
    return PossiblyWideStringToString((datadir / VOTES_DB_FILENAME).native());
}

std::string AllStoredVotes::AddVoteErrorAsString(AddVoteError                error,
                                                 const boost::optional<int>& startHeight,
                                                 const boost::optional<int>& lastHeight)
{
    switch (error) {
    case AddVoteError::FirstBlockAlreadyInAnotherVote:
        return fmt::format("The start block height{} is already in another vote. Remove that vote first",
                           startHeight ? " " + std::to_string(*startHeight) : "");
    case AddVoteError::LastBlockAlreadyInAnotherVote:
        return fmt::format("The last block height{} is already in another vote. Remove that vote first",
                           lastHeight ? " " + std::to_string(*lastHeight) : "");
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

json_spirit::Value VoteValueAndID::toJson() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair(VOTE_VALUE_JSON_KEY, static_cast<int>(getVoteValue())));
    root.push_back(json_spirit::Pair(PROP_ID_JSON_KEY, static_cast<int>(getProposalID())));

    return json_spirit::Value(std::move(root));
}

Result<VoteValueAndID, ProposalVoteCreationError> VoteValueAndID::CreateVote(uint32_t ProposalID,
                                                                             uint32_t VoteValue)
{
    TRYV(ProposalVote::ValidateProposalID(ProposalID));
    TRYV(ProposalVote::ValidateVoteValue(VoteValue));

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
        return "The first block height has invalid value";
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

bool ProposalVote::operator==(const ProposalVote& other) const
{
    return getFirstBlockHeight() == other.getFirstBlockHeight() &&
           getLastBlockHeight() == other.getLastBlockHeight() &&
           getVoteValueAndProposalID() == other.getVoteValueAndProposalID();
}

Result<void, ProposalVoteCreationError> ProposalVote::ValidateProposalID(uint32_t value)
{
    if (value > MAX_PROPOSAL_ID) {
        return Err(ProposalVoteCreationError::ProposalIDOutOfRange);
    }
    return Ok();
}

Result<void, ProposalVoteCreationError> ProposalVote::ValidateVoteValue(uint32_t value)
{
    if (value > MAX_VOTE_VALUE) {
        return Err(ProposalVoteCreationError::VoteValueOutOfRange);
    }
    return Ok();
}

Result<void, ProposalVoteCreationError> ProposalVote::ValidateStartBlock(int value)
{
    if (value < 0) {
        return Err(ProposalVoteCreationError::InvalidStartBlockHeight);
    }
    return Ok();
}

Result<void, ProposalVoteCreationError> ProposalVote::ValidateLastBlock(int value)
{
    if (value < 0) {
        return Err(ProposalVoteCreationError::InvalidEndBlockHeight);
    }
    return Ok();
}

bool VoteValueAndID::operator==(const VoteValueAndID& other) const
{
    return proposalID == other.proposalID && voteValue == other.voteValue;
}
