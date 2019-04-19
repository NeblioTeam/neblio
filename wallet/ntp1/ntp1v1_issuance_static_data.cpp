#include "ntp1v1_issuance_static_data.h"

std::unordered_map<std::string, json_spirit::Value> ParseNTP1v1Metadata()
{
    try {
        json_spirit::Value val;
        json_spirit::read_or_throw(NTP1v1IssuanceMetadataRaw, val);
        std::unordered_map<std::string, json_spirit::Value> res;
        json_spirit::Array                                  array = val.get_array();
        for (auto e : array) {
            auto               obj = e.get_obj();
            std::string        k   = obj.at(0).name_;
            json_spirit::Value v   = obj.at(0).value_;
            res[k]                 = v;
        }
        return res;
    } catch (std::exception& ex) {
        throw std::runtime_error("Failed to parse NTP1v1 raw data to json. Error: " +
                                 std::string(ex.what()));
    }
}

json_spirit::Value GetNTP1v1IssuanceMetadataNode(const std::string& tokenId)
{
    static std::unordered_map<std::string, json_spirit::Value> nodes = ParseNTP1v1Metadata();
    auto                                                       it    = nodes.find(tokenId);
    if (it != nodes.cend()) {
        return it->second;
    } else {
        throw std::runtime_error("TokenID " + tokenId +
                                 " does not exist within NTP1 issuance transactions");
    }
}
