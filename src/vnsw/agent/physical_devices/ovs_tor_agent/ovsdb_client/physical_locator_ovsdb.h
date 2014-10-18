/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __PHYSICAL_LOCATOR_OVSDB_H__
#define __PHYSICAL_LOCATOR_OVSDB_H__

#include <ovsdb_client_idl.h>

class KSyncEntry;
class OvsdbDBEntry;
class PhysicalLocatorEntry;

class PhysicalLocatorTable : public OvsdbDBObject {
public:
    PhysicalLocatorTable(OvsdbClientIdl *idl, DBTable *table);
    virtual ~PhysicalLocatorTable();

    void OvsdbNotify(OvsdbClientIdl::Op, struct ovsdb_idl_row *);
    KSyncEntry *Alloc(const KSyncEntry *key, uint32_t index);
    KSyncEntry *DBToKSyncEntry(const DBEntry*);
    OvsdbDBEntry *AllocOvsEntry(struct ovsdb_idl_row *row);
private:
    friend class PhysicalLocatorEntry;
    DISALLOW_COPY_AND_ASSIGN(PhysicalLocatorTable);
};

class PhysicalLocatorEntry : public OvsdbDBEntry {
public:
    PhysicalLocatorEntry(PhysicalLocatorTable *table, const char *dip_str);
    PhysicalLocatorEntry(PhysicalLocatorTable *table,
            const PhysicalLocatorEntry *key);
    PhysicalLocatorEntry(PhysicalLocatorTable *table, const NextHop *entry);
    PhysicalLocatorEntry(PhysicalLocatorTable *table, struct ovsdb_idl_row *entry);
    ~PhysicalLocatorEntry();

    void AddMsg(struct ovsdb_idl_txn *);
    void ChangeMsg(struct ovsdb_idl_txn *);
    void DeleteMsg(struct ovsdb_idl_txn *);
    bool Sync(DBEntry*);
    bool IsLess(const KSyncEntry&) const;
    std::string ToString() const {return "Physical Locator";}
    KSyncEntry* UnresolvedReference();
private:
    friend class PhysicalLocatorTable;
    std::string dip_;
    bool is_vxlan_nh_;
    const NextHop *nh_;
    DISALLOW_COPY_AND_ASSIGN(PhysicalLocatorEntry);
};

#endif //__PHYSICAL_LOCATOR_OVSDB_H__

