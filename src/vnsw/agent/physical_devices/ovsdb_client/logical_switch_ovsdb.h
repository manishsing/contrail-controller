/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __LOGICAL_SWITCH_OVSDB_H_
#define __LOGICAL_SWITCH_OVSDB_H_

#include <ovsdb_object.h>

namespace AGENT {
class PhysicalDeviceVnEntry;
};

class LogicalSwitchTable : public OvsdbDBObject {
public:
    LogicalSwitchTable(OvsdbClientIdl *idl, DBTable *table);
    virtual ~LogicalSwitchTable();

    void Notify(OvsdbClientIdl::Op, struct ovsdb_idl_row *);
    KSyncEntry *Alloc(const KSyncEntry *key, uint32_t index);
    KSyncEntry *DBToKSyncEntry(const DBEntry*);
private:
    DISALLOW_COPY_AND_ASSIGN(LogicalSwitchTable);
};

class LogicalSwitchEntry : public OvsdbDBEntry {
public:
    LogicalSwitchEntry(LogicalSwitchTable *table, const char *name) :
        OvsdbDBEntry(), name_(name), table_(table) {}
    LogicalSwitchEntry(LogicalSwitchTable *table, const LogicalSwitchEntry *key);
    LogicalSwitchEntry(LogicalSwitchTable *table,
            const AGENT::PhysicalDeviceVnEntry *entry);

    void AddMsg(struct ovsdb_idl_txn *);
    void ChangeMsg(struct ovsdb_idl_txn *);
    void DeleteMsg(struct ovsdb_idl_txn *);
    bool Sync(DBEntry*);
    bool IsLess(const KSyncEntry&) const;
    std::string ToString() const {return "Logical Switch";}
    KSyncObject* GetObject();
    KSyncEntry* UnresolvedReference();
private:
    friend class LogicalSwitchTable;
    std::string name_;
    std::string device_name_;
    int64_t vxlan_id_;
    LogicalSwitchTable *table_;
};

#endif //__LOGICAL_SWITCH_OVSDB_H_

