/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_ROUTE_H_
#define SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_ROUTE_H_

#include <ovsdb_client_idl.h>
class OvsPeer;

namespace OVSDB {
class UnicastMacLocalOvsdb {
public:
    UnicastMacLocalOvsdb(OvsdbClientIdl *idl, OvsPeer *peer);
    ~UnicastMacLocalOvsdb();

    void Notify(OvsdbClientIdl::Op op, struct ovsdb_idl_row *row);

private:
    OvsdbClientIdl *client_idl_;
    OvsPeer *peer_;
    DISALLOW_COPY_AND_ASSIGN(UnicastMacLocalOvsdb);
};
};

#endif //SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_ROUTE_H_

