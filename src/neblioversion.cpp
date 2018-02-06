#include "neblioversion.h"

#include <stdexcept>
#include <util.h>

void NeblioVersion::checkInitialization()
{
    if(major < 0 || minor < 0 || revision < 0 || build < 0)
        throw std::runtime_error("NeblioVersion object is not initialized.");
}

NeblioVersion::NeblioVersion(int Major, int Minor, int Revision, int Build)
{
    major = Major;
    minor = Minor;
    revision = Revision;
    build = Build;
}

bool NeblioVersion::operator>(const NeblioVersion &rhs)
{
    checkInitialization();
    if(this->major > rhs.major)
        return true;
    else if(this->major < rhs.major)
        return false;

    if(this->minor > rhs.minor)
        return true;
    else if(this->minor < rhs.minor)
        return false;

    if(this->revision > rhs.revision)
        return true;
    else if(this->revision < rhs.revision)
        return false;

    if(this->build > rhs.build)
        return true;
    else if(this->build < rhs.build)
        return false;

    return false;
}

bool NeblioVersion::operator<(const NeblioVersion &rhs)
{
    return (!(*this > rhs) && !(*this == rhs));
}

bool NeblioVersion::operator>=(const NeblioVersion &rhs)
{
    return !(*this < rhs);
}

bool NeblioVersion::operator<=(const NeblioVersion &rhs)
{
    return !(*this > rhs);
}

bool NeblioVersion::operator==(const NeblioVersion &rhs)
{
    return (major    == rhs.major &&
            minor    == rhs.minor &&
            revision == rhs.revision &&
            build    == rhs.build);
}

bool NeblioVersion::operator!=(const NeblioVersion &rhs)
{
    return !(*this == rhs);
}

std::string NeblioVersion::toString()
{
    return ToString(major)    + "." +
           ToString(minor)    + "." +
           ToString(revision) + "." +
           ToString(build);

}

void NeblioVersion::clear()
{
    *this = NeblioVersion();
}
