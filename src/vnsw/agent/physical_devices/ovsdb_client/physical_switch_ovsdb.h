/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __PHYSICAL_SWITCH_OVSDB_H_
#define __PHYSICAL_SWITCH_OVSDB_H_

#include <ovsdb_object.h>

class PhysicalSwitchTable : public OvsdbObject {
public:
    PhysicalSwitchTable(OvsdbClientIdl *idl);
    virtual ~PhysicalSwitchTable();

    void Notify(OvsdbClientIdl::Op, struct ovsdb_idl_row *);
    KSyncEntry *Alloc(const KSyncEntry *key, uint32_t index);
private:
    DISALLOW_COPY_AND_ASSIGN(PhysicalSwitchTable);
};

class PhysicalSwitchEntry : public OvsdbEntry {
public:
    PhysicalSwitchEntry(PhysicalSwitchTable *table, const char *name) : OvsdbEntry(),
    name_(name), table_(table) {}
    PhysicalSwitchEntry(PhysicalSwitchTable *table, const std::string &name) : OvsdbEntry(),
    name_(name), table_(table) {}

    bool IsLess(const KSyncEntry&) const;
    std::string ToString() const {return "Physical Switch";}
    KSyncObject* GetObject();
    KSyncEntry* UnresolvedReference();
private:
    friend class PhysicalSwitchTable;
    std::string name_;
    PhysicalSwitchTable *table_;
};

#endif //__PHYSICAL_SWITCH_OVSDB_H_

