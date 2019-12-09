#ifndef TYPES_H
#define TYPES_H

#include <boost/version.hpp>

#if BOOST_VERSION >= 106100
#include <boost/utility/string_view.hpp>
using StringViewT = boost::string_view;
#else
#include <boost/utility/string_ref.hpp>
using StringViewT = boost::string_ref;
#endif

#endif // TYPES_H
