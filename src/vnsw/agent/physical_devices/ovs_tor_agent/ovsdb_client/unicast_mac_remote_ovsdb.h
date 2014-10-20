/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_UNICAST_MAC_REMOTE_OVSDB_H_
#define SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_UNICAST_MAC_REMOTE_OVSDB_H_

#include <ovsdb_entry.h>
#include <ovsdb_object.h>
#include <ovsdb_client_idl.h>

class Layer2RouteEntry;

namespace OVSDB {
class UnicastMacRemoteTable : public OvsdbDBObject {
public:
    UnicastMacRemoteTable(OvsdbClientIdl *idl, DBTable *table);
    virtual ~UnicastMacRemoteTable();

    void OvsdbNotify(OvsdbClientIdl::Op, struct ovsdb_idl_row *);

    KSyncEntry *Alloc(const KSyncEntry *key, uint32_t index);
    KSyncEntry *DBToKSyncEntry(const DBEntry*);
    OvsdbDBEntry *AllocOvsEntry(struct ovsdb_idl_row *row);
private:
    DISALLOW_COPY_AND_ASSIGN(UnicastMacRemoteTable);
};

class UnicastMacRemoteEntry : public OvsdbDBEntry {
public:
    enum Trace {
        ADD_REQ,
        DEL_REQ,
        ADD_ACK,
        DEL_ACK,
    };
    UnicastMacRemoteEntry(OvsdbDBObject *table, const std::string mac,
            const std::string logical_switch);
    UnicastMacRemoteEntry(OvsdbDBObject *table, const Layer2RouteEntry *entry);
    UnicastMacRemoteEntry(OvsdbDBObject *table, const UnicastMacRemoteEntry *key);
    UnicastMacRemoteEntry(OvsdbDBObject *table,
            struct ovsdb_idl_row *entry);

    void AddMsg(struct ovsdb_idl_txn *);
    void ChangeMsg(struct ovsdb_idl_txn *);
    void DeleteMsg(struct ovsdb_idl_txn *);

    void OvsdbChange();

    bool Sync(DBEntry*);
    bool IsLess(const KSyncEntry&) const;
    std::string ToString() const {return "Unicast Mac Remote";}
    KSyncEntry* UnresolvedReference();
private:
    void SendTrace(Trace event) const;

    friend class UnicastMacRemoteTable;
    std::string mac_;
    std::string logical_switch_name_;
    std::string dest_ip_;
    KSyncEntryPtr logical_switch_;
    DISALLOW_COPY_AND_ASSIGN(UnicastMacRemoteEntry);
};
};

#endif //SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_UNICAST_MAC_REMOTE_OVSDB_H_

