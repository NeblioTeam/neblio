#ifndef UDADDRESS_H
#define UDADDRESS_H

#include "CustomTypes.h"
#include "curltools.h"
#include <boost/optional.hpp>
#include <boost/regex.h>
#include <string>

bool IsUDAddressSyntaxValid(const StringViewT UDDomain);

boost::optional<std::string> GetUDAddressAPICall(const StringViewT UDDomain);

boost::optional<std::string> GetNeblioAddressFromUDAddress(const StringViewT UDDomain);

#endif // UDADDRESS_H
