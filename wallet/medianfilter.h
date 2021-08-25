#ifndef CMEDIANFILTER_H
#define CMEDIANFILTER_H

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <vector>

/** Median filter over a stream of values.
 * Returns the median of the last N numbers
 */
template <typename T>
class CMedianFilter
{
private:
    std::vector<T>       vValues;
    std::vector<T>       vSorted;
    unsigned int         nSize;
    mutable boost::mutex mtx;

public:
    CMedianFilter(unsigned int size, T initial_value)
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        nSize = size;
        vValues.reserve(size);
        vValues.push_back(initial_value);
        vSorted = vValues;
    }

    void input(T value)
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        if (vValues.size() == nSize) {
            vValues.erase(vValues.begin());
        }
        vValues.push_back(value);

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    T median() const
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        int                             size = vSorted.size();
        assert(size > 0);
        if (size & 1) // Odd number of elements
        {
            return vSorted[size / 2];
        } else // Even number of elements
        {
            return (vSorted[size / 2 - 1] + vSorted[size / 2]) / 2;
        }
    }

    int size() const
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        return vValues.size();
    }

    std::vector<T> sorted() const
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        return vSorted;
    }
};

#endif // CMEDIANFILTER_H
