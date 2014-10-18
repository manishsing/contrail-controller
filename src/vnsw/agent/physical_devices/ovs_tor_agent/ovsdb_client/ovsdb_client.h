/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_CLIENT_H_
#define SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_CLIENT_H_

class TorAgentParam;
class SandeshOvsdbClient;

namespace OVSDB {
class OvsdbClient {
public:
    OvsdbClient() {}
    virtual ~OvsdbClient() {}
    virtual void RegisterClients() = 0;
    virtual const std::string protocol() = 0;
    virtual const std::string server() = 0;
    virtual uint16_t port() = 0;
    virtual Ip4Address tsn_ip() = 0;
    virtual void AddSessionInfo(SandeshOvsdbClient &client) = 0;
    void Init();
    static OvsdbClient* Allocate(Agent *agent, TorAgentParam *params);
private:
    DISALLOW_COPY_AND_ASSIGN(OvsdbClient);
};
};

#endif //SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_CLIENT_H_
