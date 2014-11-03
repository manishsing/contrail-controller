/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_VLAN_PORT_BINDING_OVSDB_H_
#define SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_VLAN_PORT_BINDING_OVSDB_H_

#include <ovsdb_entry.h>
#include <ovsdb_object.h>

namespace AGENT {
class PhysicalDeviceVnEntry;
class VlanLogicalPortEntry;
};

namespace OVSDB {
class VlanPortBindingTable : public OvsdbDBObject {
public:
    VlanPortBindingTable(OvsdbClientIdl *idl, DBTable *table);
    virtual ~VlanPortBindingTable();

    void OvsdbNotify(OvsdbClientIdl::Op, struct ovsdb_idl_row*);
    KSyncEntry *Alloc(const KSyncEntry *key, uint32_t index);
    KSyncEntry *DBToKSyncEntry(const DBEntry*);
    OvsdbDBEntry* AllocOvsEntry(struct ovsdb_idl_row*);
    DBFilterResp DBEntryFilter(const DBEntry *entry);

private:
    DISALLOW_COPY_AND_ASSIGN(VlanPortBindingTable);
};

class VlanPortBindingEntry : public OvsdbDBEntry {
public:
    VlanPortBindingEntry(VlanPortBindingTable *table,
            const VlanPortBindingEntry *key);
    VlanPortBindingEntry(VlanPortBindingTable *table,
            const AGENT::VlanLogicalPortEntry *entry);

    void PreAddChange();
    void PostDelete();
    void AddMsg(struct ovsdb_idl_txn *);
    void ChangeMsg(struct ovsdb_idl_txn *);
    void DeleteMsg(struct ovsdb_idl_txn *);
    bool Sync(DBEntry*);
    bool IsLess(const KSyncEntry&) const;
    std::string ToString() const {return "Vlan Port Binding";}
    KSyncEntry* UnresolvedReference();

private:
    friend class VlanPortBindingTable;
    KSyncEntryPtr logical_switch_;
    KSyncEntryPtr physical_port_;
    std::string logical_switch_name_;
    std::string physical_port_name_;
    std::string physical_device_name_;
    uint16_t vlan_;
    DISALLOW_COPY_AND_ASSIGN(VlanPortBindingEntry);
};
};

#endif //SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_VLAN_PORT_BINDING_OVSDB_H_
