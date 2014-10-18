/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __UNICAST_MAC_LOCAL_OVSDB_H__
#define __UNICAST_MAC_LOCAL_OVSDB_H__

#include <ovsdb_object.h>

class UnicastMacLocalTable : public OvsdbObject {
public:
    UnicastMacLocalTable(OvsdbClientIdl *idl);
    virtual ~UnicastMacLocalTable();

    void Notify(OvsdbClientIdl::Op, struct ovsdb_idl_row *);
    KSyncEntry *Alloc(const KSyncEntry *key, uint32_t index);
private:
    DISALLOW_COPY_AND_ASSIGN(UnicastMacLocalTable);
};

class UnicastMacLocalEntry : public OvsdbEntry {
public:
    UnicastMacLocalEntry(UnicastMacLocalTable *table,
            struct ovsdb_idl_row *ovsdb_row, std::string mac,
            std::string ip_addr, std::string ls_name);

    bool IsLess(const KSyncEntry&) const;
    std::string ToString() const {return "Unicast Mac Local";}
    KSyncEntry* UnresolvedReference();
private:
    friend class UnicastMacLocalTable;
    struct ovsdb_idl_row *ovsdb_row_;
    std::string mac_;
    std::string ip_addr_;
    std::string ls_name_;
    UnicastMacLocalTable *table_;
};

#endif //__UNICAST_MAC_LOCAL_OVSDB_H__

