/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __OVSDB_OBJECT__
#define __OVSDB_OBJECT__

#include <ovsdb_entry.h>
#include <ovsdb_client_idl.h>
#include <ksync/ksync_index.h>
#include <ksync/ksync_object.h>

class OvsdbObject : public KSyncObject {
public:
    OvsdbObject(OvsdbClientIdl *idl) : KSyncObject(), client_idl_(idl) {}

    virtual ~OvsdbObject() {}
    OvsdbClientIdl *client_idl() { return client_idl_;}
protected:
    OvsdbClientIdl *client_idl_;
private:
    DISALLOW_COPY_AND_ASSIGN(OvsdbObject);
};

class OvsdbDBObject : public KSyncDBObject {
public:
    OvsdbDBObject(OvsdbClientIdl *idl) : KSyncDBObject(), client_idl_(idl) {}
    OvsdbDBObject(OvsdbClientIdl *idl, DBTableBase *tbl) : KSyncDBObject(tbl),
                                                           client_idl_(idl) {}

    virtual ~OvsdbDBObject() {}
    OvsdbClientIdl *client_idl() { return client_idl_;}
protected:
    OvsdbClientIdl *client_idl_;
private:
    friend class OvsdbDBEntry;
    DISALLOW_COPY_AND_ASSIGN(OvsdbDBObject);
};

#endif //__OVSDB_ENTRY__

