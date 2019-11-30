#ifndef JSONSTRINGQUEUE_H
#define JSONSTRINGQUEUE_H

#include <cassert>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

class JsonStringQueue
{
    static const char     _openChar             = '{';
    static const char     _closeChar            = '}';
    static const uint32_t LARGEST_EXPECTED_JSON = (1 << 10);

    std::deque<char>         dataQueue;
    std::vector<std::string> parsedValuesQueue;
    uint32_t                 charCount    = 0;
    int32_t                  bracketLevel = 0;
    inline void              parseAndAppendJson(const std::string& jsonStr);
    inline void              parseAndAppendJson(std::string&& jsonStr);

public:
    template <typename Iterator>
    inline void                     pushData(Iterator dataBegin, Iterator dataEnd);
    inline std::vector<std::string> pullDataAndClear();
    inline void                     clear();
};

void JsonStringQueue::parseAndAppendJson(const std::string& jsonStr)
{
    parsedValuesQueue.push_back(jsonStr);
}

void JsonStringQueue::parseAndAppendJson(std::string&& jsonStr)
{
    parsedValuesQueue.push_back(std::move(jsonStr));
}

template <typename Iterator>
void JsonStringQueue::pushData(Iterator dataBegin, Iterator dataEnd)
{
    dataQueue.insert(dataQueue.end(), dataBegin, dataEnd);
    while (charCount < dataQueue.size()) {
        if (charCount > LARGEST_EXPECTED_JSON) {
            throw std::runtime_error("While attempting to detect json length, huge unparsed json data "
                                     "was found. Max allowed size is: " +
                                     std::to_string(LARGEST_EXPECTED_JSON));
        }
        if (dataQueue[charCount] == _openChar) {
            bracketLevel++;
            charCount++;
            continue;
        } else if (dataQueue[charCount] == _closeChar) {
            bracketLevel--;
            if (bracketLevel < 0) {
                throw std::runtime_error(
                    "While attempting to detect json length, an invalid closure "
                    "was found. Max allowed size is: " +
                    std::string(dataQueue.cbegin(), dataQueue.cbegin() + charCount));
            }
            charCount++;
            if (bracketLevel == 0) {
                // after having found one json object, exit
                parseAndAppendJson(std::string(dataQueue.begin(), dataQueue.begin() + charCount));
                break;
                //                dataQueue.erase(dataQueue.begin(), dataQueue.begin() + charCount);
                //                charCount -= std::distance(dataQueue.begin(), dataQueue.begin() +
                //                charCount);
                assert(charCount >= 0);
            }
            continue;
        } else {
            charCount++;
        }
    }
}

std::vector<std::string> JsonStringQueue::pullDataAndClear()
{
    std::vector<std::string> res = std::move(parsedValuesQueue);
    return res;
}

void JsonStringQueue::clear()
{
    dataQueue.clear();
    parsedValuesQueue.clear();
    charCount    = 0;
    bracketLevel = 0;
}

#endif // JSONSTRINGQUEUE_H
