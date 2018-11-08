// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#if defined(USE_LMDB)

#include "txdb-lmdb.h"

#elif defined(USE_LEVELDB)

#include "txdb-leveldb.h"

#endif

#endif // BITCOIN_TXDB_H
