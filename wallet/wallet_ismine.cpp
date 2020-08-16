// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2020-2020 The Neblio developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet_ismine.h"

#include "key.h"
#include "keystore.h"
#include "script.h"
#include "util.h"

typedef std::vector<unsigned char> valtype;

bool IsMineCheck(isminetype var, isminetype toCheckFor)
{
    using T = std::underlying_type<isminetype>::type;
    return (static_cast<T>(var) & static_cast<T>(toCheckFor)) != 0;
}
