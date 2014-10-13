/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <ovsdb_client_tcp.h>
#include <physical_devices/ovs_tor_agent/tor_agent_param.h>

void OvsdbClient::Init() {
}

OvsdbClient *OvsdbClient::Allocate(Agent *agent, TorAgentParam *params) {
    return (new OvsdbClientTcp(agent, IpAddress(params->tor_ip()),
                               params->tor_port()));
}

