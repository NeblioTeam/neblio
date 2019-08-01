#ifndef JSON_SPIRIT_ERROR_POSITION
#define JSON_SPIRIT_ERROR_POSITION

//          Copyright John W. Wilkinson 2007 - 2009.
// Distributed under the MIT License, see accompanying file LICENSE.txt

// json spirit version 4.03

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <string>
#include <stdexcept>

namespace json_spirit
{
    // An Error_position exception is thrown by the "read_or_throw" functions below on finding an error.
    // Note the "read_or_throw" functions are around 3 times slower than the standard functions "read" 
    // functions that return a bool.
    //
    // std::invalid_argument added by Sam to solve the issue of having to catch by Error_position type
    struct Error_position : public std::invalid_argument
    {
        Error_position();
        Error_position( unsigned int line, unsigned int column, const std::string& reason );
        bool operator==( const Error_position& lhs ) const;
        unsigned int line_;
        unsigned int column_;
        std::string reason_;
        mutable std::string stdExceptMessage;

    public:
        inline const char *what() const noexcept override;
    };

    inline Error_position::Error_position()
    :   std::invalid_argument ("Unspedified json Error_position")
    ,   line_( 0 )
    ,   column_( 0 )
    {
    }

    inline Error_position::Error_position( unsigned int line, unsigned int column, const std::string& reason )
    :   std::invalid_argument(
              std::string("Json parsing failed at line: " + std::to_string(line) +
              std::string(", and columnd ") + std::to_string(column)) +
              std::string(", and reason: ") + reason)
    ,   line_( line )
    ,   column_( column )
    ,   reason_( reason )
    {
    }

    inline bool Error_position::operator==( const Error_position& lhs ) const
    {
        if( this == &lhs ) return true;

        return ( reason_ == lhs.reason_ ) &&
               ( line_   == lhs.line_ ) &&
                ( column_ == lhs.column_ );
    }

    const char *Error_position::what() const noexcept
    {
        stdExceptMessage = std::string("Json parsing failed at line: " + std::to_string(line_) +
                           std::string(", and columnd ") + std::to_string(column_)) +
                           std::string(", and reason: ") + reason_;
        return stdExceptMessage.c_str();
    }
}

#endif
