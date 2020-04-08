#include "bitcoinaddressvalidator.h"

/* Base58 characters are:
     "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

  This is:
  - All numbers except for '0'
  - All upper-case letters except for 'I' and 'O'
  - All lower-case letters except for 'l'

  User friendly Base58 input can map
  - 'l' and 'I' to '1'
  - '0' and 'O' to 'o'
*/

BitcoinAddressValidator::BitcoinAddressValidator(QObject* parent) : QValidator(parent) {}

QValidator::State BitcoinAddressValidator::validate(QString& input, int& /*pos*/) const
{
    // Correction
    for (int idx = 0; idx < input.size();) {
        bool  removeChar = false;
        QChar ch         = input.at(idx);
        // Corrections made are very conservative on purpose, to avoid
        // users unexpectedly getting away with typos that would normally
        // be detected, and thus sending to the wrong address.
        switch (ch.unicode()) {
        // Qt categorizes these as "Other_Format" not "Separator_Space"
        case 0x200B: // ZERO WIDTH SPACE
        case 0xFEFF: // ZERO WIDTH NO-BREAK SPACE
            removeChar = true;
            break;
        default:
            break;
        }
        // Remove whitespace
        if (ch.isSpace())
            removeChar = true;
        // To next character
        if (removeChar)
            input.remove(idx, 1);
        else
            ++idx;
    }

    // Validation
    QValidator::State state      = QValidator::Acceptable;
    uint_fast16_t     dot_count  = 0;
    uint_fast16_t     dash_count = 0;
    for (int idx = 0; idx < input.size(); ++idx) {
        int ch = input.at(idx).unicode();

        if (ch == '.') {
            dot_count++;
        }
        if (dot_count > 1) {
            return QValidator::Invalid;
        }

        if (ch == '-') {
            dash_count++;
        }
        if (dash_count > 1) {
            return QValidator::Invalid;
        }

        if ((std::isalnum(ch) || ch == '.' || ch == '-')) {
            // Alphanumeric and not a 'forbidden' character
        } else {
            state = QValidator::Invalid;
        }
    }

    // Empty address is "intermediate" input
    if (input.isEmpty()) {
        state = QValidator::Intermediate;
    }

    return state;
}
