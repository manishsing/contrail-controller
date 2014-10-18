/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __UNICAST_MAC_REMOTE_OVSDB_H__
#define __UNICAST_MAC_REMOTE_OVSDB_H__

#include <ovsdb_client_idl.h>

class KSyncEntry;
class OvsdbDBEntry;
class UnicastMacRemoteEntry;

class UnicastMacRemoteTable : public OvsdbDBObject {
public:
    UnicastMacRemoteTable(OvsdbClientIdl *idl, DBTable *table);
    virtual ~UnicastMacRemoteTable();

    void OvsdbNotify(OvsdbClientIdl::Op, struct ovsdb_idl_row *);
    KSyncEntry *Alloc(const KSyncEntry *key, uint32_t index);
    KSyncEntry *DBToKSyncEntry(const DBEntry*);
    OvsdbDBEntry *AllocOvsEntry(struct ovsdb_idl_row *row);
private:
    friend class UnicastMacRemoteEntry;
    DISALLOW_COPY_AND_ASSIGN(UnicastMacRemoteTable);
};

class UnicastMacRemoteEntry : public OvsdbDBEntry {
public:
    UnicastMacRemoteEntry(UnicastMacRemoteTable *table,
            std::string l_switch_name, MacAddress mac_, std::string dest_ip);
    UnicastMacRemoteEntry(UnicastMacRemoteTable *table,
            const UnicastMacRemoteEntry *key);
    UnicastMacRemoteEntry(UnicastMacRemoteTable *table, const NextHop *entry);
    UnicastMacRemoteEntry(UnicastMacRemoteTable *table,
            struct ovsdb_idl_row *entry);
    ~UnicastMacRemoteEntry();

    void AddMsg(struct ovsdb_idl_txn *);
    void ChangeMsg(struct ovsdb_idl_txn *);
    void DeleteMsg(struct ovsdb_idl_txn *);
    bool Sync(DBEntry*);
    bool IsLess(const KSyncEntry&) const;
    std::string ToString() const {return "Unicast Remote Entry";}
    KSyncEntry* UnresolvedReference();
private:
    friend class UnicastMacRemoteTable;
    std::string l_switch_name_;
    MacAddress mac_;
    std::string dest_ip_;

    DISALLOW_COPY_AND_ASSIGN(UnicastMacRemoteEntry);
};

#endif //__UNICAST_MAC_REMOTE_OVSDB_H__

