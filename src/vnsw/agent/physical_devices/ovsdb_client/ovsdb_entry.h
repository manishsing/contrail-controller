/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __OVSDB_ENTRY__
#define __OVSDB_ENTRY__

#include <boost/intrusive_ptr.hpp>

#include <db/db_entry.h>
#include <ksync/ksync_entry.h>

class OvsdbEntry : public KSyncEntry {
public:
    OvsdbEntry() : KSyncEntry() {}
    OvsdbEntry(uint32_t index) : KSyncEntry(index) {}
    virtual ~OvsdbEntry() {}

    bool Add() {return true;}
    bool Change() {return true;}
    bool Delete() {return true;}

private:
    DISALLOW_COPY_AND_ASSIGN(OvsdbEntry);
};

class OvsdbDBEntry : public KSyncDBEntry {
public:
    OvsdbDBEntry() : KSyncDBEntry() {}
    OvsdbDBEntry(uint32_t index) : KSyncDBEntry(index) {}
    virtual ~OvsdbDBEntry() {}

    // Encode add message for entry
    virtual void AddMsg(struct ovsdb_idl_txn *) = 0;
    // Encode change message for entry
    virtual void ChangeMsg(struct ovsdb_idl_txn *) = 0;
    // Encode delete message for entry
    virtual void DeleteMsg(struct ovsdb_idl_txn *) = 0;

    bool Add();
    bool Change();
    bool Delete();

private:
    DISALLOW_COPY_AND_ASSIGN(OvsdbDBEntry);
};

#endif //__OVSDB_ENTRY__

