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

    if (vrf->vn()) {
        Layer2AgentRouteTable::AddLayer2ReceiveRoute(agent_->local_peer(),
                                                     vrf->GetName(),
                                                     vrf->vn()->GetName(),
                                                     address,
                                                     "pkt0", true);
    }
}
