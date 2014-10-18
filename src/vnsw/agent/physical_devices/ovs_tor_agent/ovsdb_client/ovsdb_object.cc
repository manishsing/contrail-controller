/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_wrapper.h>
};

#include <ovsdb_entry.h>
#include <ovsdb_object.h>

using OVSDB::OvsdbObject;
using OVSDB::OvsdbDBObject;
using OVSDB::OvsdbDBEntry;

OvsdbObject::OvsdbObject(OvsdbClientIdl *idl) : KSyncObject(),
    client_idl_(idl) {
}

OvsdbObject::~OvsdbObject() {
}

OvsdbDBObject::OvsdbDBObject(OvsdbClientIdl *idl) : KSyncDBObject(),
    client_idl_(idl) {
}

OvsdbDBObject::OvsdbDBObject(OvsdbClientIdl *idl, DBTableBase *tbl) :
    KSyncDBObject(tbl), client_idl_(idl) {
}

OvsdbDBObject::~OvsdbDBObject() {
}

void OvsdbDBObject::NotifyAddOvsdb(OvsdbDBEntry *key, struct ovsdb_idl_row *row) {
    OvsdbDBEntry *entry = static_cast<OvsdbDBEntry *>(Find(key));
    if (entry) {
        if (entry->IsAddChangeAckWaiting()) {
            entry->NotifyAdd(row);
        }
    } else {
        //TODO trigger delete of this entry
        OvsdbDBEntry *del_entry = AllocOvsEntry(row);
        Delete(del_entry);
        //del_entry->Delete();
        //delete del_entry;
    }
}

void OvsdbDBObject::NotifyDeleteOvsdb(OvsdbDBEntry *key) {
    OvsdbDBEntry *entry = static_cast<OvsdbDBEntry *>(Find(key));
    if (entry) {
        if (entry->IsDelAckWaiting()) {
            entry->NotifyDelete();
        } else {
            // Clear OVS State and trigger Add/Change Req on entry
            entry->clear_ovs_entry();
            NotifyEvent(entry, KSyncEntry::ADD_CHANGE_REQ);
        }
    }
}

