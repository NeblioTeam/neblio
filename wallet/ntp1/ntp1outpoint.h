#ifndef NTP1OUTPOINT_H
#define NTP1OUTPOINT_H

#include <string>

#include "uint256.h"
#include "util.h"

#include <boost/functional/hash.hpp>

#include "json/json_spirit.h"

class NTP1OutPoint
{
    uint256 hash;
    unsigned int index;
public:
    NTP1OutPoint();
    NTP1OutPoint(uint256 hashIn, unsigned int indexIn);
    void setNull();
    bool isNull() const;
    uint256 getHash() const;
    unsigned int getIndex() const;
    friend inline bool operator==(const NTP1OutPoint& lhs, const NTP1OutPoint& rhs);
    json_spirit::Value exportDatabaseJsonData() const;
    void importDatabaseJsonData(const json_spirit::Value& data);
};


namespace boost {

template <>
struct hash<NTP1OutPoint>
{
    std::size_t operator()(const NTP1OutPoint& k) const
    {
        // Compute individual hash values for first,
        // second and third and combine them using XOR
        // and bit shifting:
        std::string toHash = k.getHash().ToString() + ":" + ToString(k.getIndex());
        return boost::hash<std::string>()(toHash);
    }
};

}

bool operator==(const NTP1OutPoint &lhs, const NTP1OutPoint &rhs)
{
    return (lhs.hash == rhs.hash && lhs.index == rhs.index);
}

#endif // NTP1OUTPOINT_H
