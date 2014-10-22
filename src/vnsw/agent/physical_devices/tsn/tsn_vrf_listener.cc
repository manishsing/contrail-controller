/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include <cmn/agent_cmn.h>
#include <oper/vrf.h>
#include <oper/interface.h>
#include <oper/route_common.h>
#include <physical_devices/tsn/tsn_vrf_listener.h>
#include <controller/controller_route_path.h>

TsnVrfListener::TsnVrfListener(Agent *agent) : agent_(agent) {
    vrf_table_listener_id_ = agent->vrf_table()->Register(
                             boost::bind(&TsnVrfListener::VrfNotify, this, _2));
}

TsnVrfListener::~TsnVrfListener() {
    agent_->vrf_table()->Unregister(vrf_table_listener_id_);
}

void TsnVrfListener::VrfNotify(DBEntryBase *entry) {
    VrfEntry *vrf = static_cast<VrfEntry *>(entry);
    if (vrf->GetName() == agent_->fabric_vrf_name())
        return;
    MacAddress address(agent_->vhost_interface()->mac());

    if (entry->IsDeleted()) {
        Layer2AgentRouteTable::Delete(agent_->local_peer(), vrf->GetName(), -1,
                                      address);
        return;
    }

    std::string vn_name;
    GetVnName(vrf, &vn_name);
    Layer2AgentRouteTable::AddLayer2ReceiveRoute(agent_->local_peer(),
                                                 vrf->GetName(), vn_name,
                                                 address,
                                                 "pkt0", true);
    //Add TSN route
    boost::system::error_code ec;
    IpAddress tsn_addr =  IpAddress::from_string(agent_->tsn_ip_1(), ec).to_v4();
    IpAddress tsn_gw_addr =  IpAddress::from_string(agent_->tsn_ip_2(), ec).to_v4();
    SecurityGroupList sg;
    PathPreference pref;
    ControllerVmRoute *data =
        ControllerVmRoute::MakeControllerVmRoute(agent_->local_peer(),
                              Agent::GetInstance()->fabric_vrf_name(),
                              Agent::GetInstance()->router_id(),
                              Agent::GetInstance()->fabric_vrf_name(),
                              tsn_gw_addr.to_v4(), TunnelType::VxlanType(),
                              4, "", sg, pref);

    InetUnicastAgentRouteTable::AddRemoteVmRouteReq(agent_->local_peer(),
                                                    Agent::GetInstance()->fabric_vrf_name(),
                                                    tsn_addr, 32,
                                                    data);
}

// TODO: change this
void TsnVrfListener::GetVnName(VrfEntry *vrf, std::string *vn_name) {
    std::size_t pos = vrf->GetName().find_last_of(":");
    if (pos == std::string::npos)
        return;
    *vn_name = vrf->GetName().substr(0, pos - 1);
}
