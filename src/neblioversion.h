#ifndef NEBLIOVERSION_H
#define NEBLIOVERSION_H

#include <string>

//TODO: Sam: Write unit tests for this class's comparators
class NeblioVersion
{
    int major;
    int minor;
    int revision;
    int build;
    void checkInitialization();

public:
    NeblioVersion(int Major = -1, int Minor = -1, int Revision = -1, int Build = -1);
    bool operator>(const NeblioVersion& rhs);
    bool operator<(const NeblioVersion& rhs);
    bool operator>=(const NeblioVersion& rhs);
    bool operator<=(const NeblioVersion& rhs);
    bool operator==(const NeblioVersion& rhs);
    bool operator!=(const NeblioVersion& rhs);
    std::string toString();
    void clear();
};

#endif // NEBLIOVERSION_H
