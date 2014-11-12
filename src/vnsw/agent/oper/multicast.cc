/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include <boost/uuid/uuid_io.hpp>
#include <cmn/agent_cmn.h>

#include <base/logging.h>
#include <oper/route_common.h>
#include <oper/interface_common.h>
#include <oper/nexthop.h>
#include <oper/tunnel_nh.h>
#include <oper/vrf.h>
#include <oper/agent_sandesh.h>
#include <oper/multicast.h>
#include <oper/mirror_table.h>
#include <oper/mpls.h>
#include <controller/controller_init.h>
#include <controller/controller_route_path.h>
#include <physical_devices/tables/physical_device.h>
#include <physical_devices/tables/physical_device_vn.h>

#include <sandesh/sandesh_types.h>
#include <sandesh/sandesh_constants.h>
#include <sandesh/sandesh.h>
#include <sandesh/sandesh_trace.h>

using namespace std;
using AGENT::PhysicalDeviceVnTable;
using AGENT::PhysicalDeviceTable;
using AGENT::PhysicalDeviceVnEntry;
using AGENT::PhysicalDeviceEntry;

#define INVALID_PEER_IDENTIFIER ControllerPeerPath::kInvalidPeerIdentifier

MulticastHandler *MulticastHandler::obj_;
SandeshTraceBufferPtr MulticastTraceBuf(SandeshTraceBufferCreate("Multicast",
                                                                     1000));
/*
 * Registeration for notification
 * VM - Looking for local VM added 
 * VN - Looking for subnet information from VN 
 * Enable trace print messages
 */
void MulticastHandler::Register() {
    vn_listener_id_ = agent_->vn_table()->Register(
        boost::bind(&MulticastHandler::ModifyVN, _1, _2));
    interface_listener_id_ = agent_->interface_table()->Register(
        boost::bind(&MulticastHandler::ModifyVmInterface, _1, _2));
    if (Agent::GetInstance()->tsn_enabled()) {
        physical_device_vn_listener_id_ =
            agent_->device_manager()->physical_device_vn_table()->
            Register(boost::bind(&MulticastHandler::ModifyTor,
                                 _1, _2));
    }

    MulticastHandler::GetInstance()->GetMulticastObjList().clear();
}

void MulticastHandler::Terminate() {
    agent_->vn_table()->Unregister(vn_listener_id_);
    agent_->interface_table()->Unregister(interface_listener_id_);
}

void MulticastHandler::AddL2BroadcastRoute(MulticastGroupObject *obj,
                                           const string &vrf_name,
                                           const string &vn_name,
                                           const Ip4Address &addr,
                                           uint32_t label,
                                           int vxlan_id,
                                           uint32_t ethernet_tag)
{
    boost::system::error_code ec;
    MCTRACE(Log, "add L2 bcast route ", vrf_name, addr.to_string(), 0);
    //Add Layer2 FF:FF:FF:FF:FF:FF
    ComponentNHKeyList component_nh_key_list =
        GetInterfaceComponentNHKeyList(obj, InterfaceNHFlags::LAYER2);
    if (component_nh_key_list.size() == 0)
        return;
    uint32_t route_tunnel_bmap = TunnelType::AllType();
    Layer2AgentRouteTable::AddLayer2BroadcastRoute(agent_->local_vm_peer(),
                                                   vrf_name, vn_name,
                                                   label, vxlan_id,
                                                   ethernet_tag,
                                                   route_tunnel_bmap,
                                                   Composite::L2INTERFACE,
                                                   component_nh_key_list);
    /*
    RebakeSubnetRoute(agent_->local_vm_peer(), vrf_name, label,
                      vxlan_id, vn_name, false,
                      component_nh_key_list, Composite::L2INTERFACE);
                      */
}

/*
 * Route address 255.255.255.255 deletion from last VM in VN del
 */
void MulticastHandler::DeleteBroadcast(const Peer *peer,
                                       const std::string &vrf_name,
                                       uint32_t ethernet_tag)
{
    boost::system::error_code ec;
    MCTRACE(Log, "delete bcast route ", vrf_name, "255.255.255.255", 0);
    Layer2AgentRouteTable::DeleteBroadcastReq(peer, vrf_name, ethernet_tag);
}

void MulticastHandler::HandleVxLanChange(const VnEntry *vn) {
    if (vn->IsDeleted() || !vn->GetVrf()) 
        return;

    MulticastGroupObject *obj =
        FindFloodGroupObject(vn->GetVrf()->GetName());
    if (!obj || obj->IsDeleted())
        return;

    int new_vxlan_id = vn->GetVxLanId();

    if (new_vxlan_id != obj->vxlan_id()) {
        boost::system::error_code ec;
        Ip4Address broadcast =  IpAddress::from_string("255.255.255.255",
                                                       ec).to_v4();
        obj->set_vxlan_id(new_vxlan_id);
        //Rebake new vxlan id in mcast route
        AddL2BroadcastRoute(obj, vn->GetVrf()->GetName(), vn->GetName(),
                            broadcast, obj->evpn_mpls_label(), new_vxlan_id, 0);
    }
}

void MulticastHandler::HandleFamilyConfig(const VnEntry *vn) 
{
    bool new_layer2_forwarding = vn->layer2_forwarding();

    if (!vn->GetVrf())
        return;

    std::string vrf_name = vn->GetVrf()->GetName();
    for (std::set<MulticastGroupObject *>::iterator it =
         MulticastHandler::GetInstance()->GetMulticastObjList().begin(); 
         it != MulticastHandler::GetInstance()->GetMulticastObjList().end(); it++) {
        if (vrf_name != (*it)->vrf_name())
            continue;

        //L2 family disabled
        if (!(new_layer2_forwarding) && (*it)->layer2_forwarding()) {
            (*it)->SetLayer2Forwarding(new_layer2_forwarding);
            if (IS_BCAST_MCAST((*it)->GetGroupAddress())) { 
                Layer2AgentRouteTable::DeleteBroadcastReq(agent_->
                                                          local_vm_peer(),
                                                          (*it)->vrf_name(), 0);
            }
        }
    }
}

/* Regsitered call for VN */
void MulticastHandler::ModifyVN(DBTablePartBase *partition, DBEntryBase *e)
{
    const VnEntry *vn = static_cast<const VnEntry *>(e);

    MulticastHandler::GetInstance()->HandleIpam(vn);
    MulticastHandler::GetInstance()->HandleFamilyConfig(vn);
    MulticastHandler::GetInstance()->HandleVxLanChange(vn);
    MulticastHandler::GetInstance()->HandleTor(vn);
}

void HandleTorRoute(const PhysicalDeviceVnEntry *device_vn,
                    const VnEntry *vn)
{
    if (device_vn->vn() != vn)
        return;

    PhysicalDeviceEntry *physical_device = device_vn->device();

    uint32_t vxlan_id = vn->GetVxLanId();
    boost::system::error_code ec;
    Ip4Address addr = physical_device->ip().to_v4();
    MulticastGroupObject *obj = MulticastHandler::GetInstance()->
        FindFloodGroupObject(vn->GetVrf()->GetName());
    bool add_request = true;

    if (device_vn->IsDeleted()) {
        if (obj == NULL)
            return;
        obj->DeleteFromTorList(addr, vxlan_id, TunnelType::VxlanType());
        if (obj->tor_olist().empty())
            add_request = false;
    }

    if (!add_request) {
        Layer2AgentRouteTable::DeleteBroadcastReq(Agent::GetInstance()->
                                                  multicast_tor_peer(),
                                                  vn->GetVrf()->GetName(),
                                                  vxlan_id);
        MulticastHandler::GetInstance()->
            DeleteMulticastObject(vn->GetVrf()->GetName(), addr);
        return;
    }

    if (add_request) {
        //TBD Make a common func to create object
        if (obj == NULL) {
            boost::system::error_code ec;
            Ip4Address broadcast =  IpAddress::from_string("255.255.255.255",
                                                           ec).to_v4();
            obj = MulticastHandler::GetInstance()->
                CreateMulticastGroupObject(vn->GetVrf()->GetName(),
                                           broadcast,
                                           vn->GetName());
        }

        obj->AddInTorList(addr, vxlan_id, TunnelType::VxlanType());
    }

    MulticastHandler::ModifyTorMembers(Agent::GetInstance()->
                                       multicast_tor_peer(),
                                       vn->GetVrf()->GetName(),
                                       obj->tor_olist(),
                                       vn->GetVxLanId(),
                                       1);
}

void MulticastHandler::ModifyTor(DBTablePartBase *partition, DBEntryBase *e)
{
    const PhysicalDeviceVnEntry *device_vn =
        static_cast<const PhysicalDeviceVnEntry *>(e);

    //Take IP out of it
    //PhysicalDeviceEntry *device = device_vn->device();
    VnEntry *vn = device_vn->vn();
    PhysicalDeviceEntry *physical_device = device_vn->device();

    if (!physical_device || !vn || !(vn->GetVrf()))
        return;

    if (vn->GetVrf() == NULL) {
        return;
    }
    HandleTorRoute(device_vn, vn);
}

void MulticastHandler::WalkDone(const uuid &vn_uuid) {
    std::map<uuid, DBTableWalker::WalkId>::iterator it =
        physical_device_vn_walker_id_.find(vn_uuid);
    if (it->second != DBTableWalker::kInvalidWalkerId)
        physical_device_vn_walker_id_.erase(it);
}

bool MulticastHandler::TorWalker(DBTablePartBase *partition,
                                 DBEntryBase *entry,
                                 const VnEntry *vn) {
    PhysicalDeviceVnEntry *physical_vn_device =
        static_cast<PhysicalDeviceVnEntry *>(entry);
    HandleTorRoute(physical_vn_device, vn);
    return true;
}

void MulticastHandler::HandleTor(const VnEntry *vn) 
{
    if (!vn->GetVrf())
        return;
    if ((Agent::GetInstance()->device_manager() == NULL) ||
        (Agent::GetInstance()->device_manager()->physical_device_vn_table()
         == NULL)) {
        return;
    }
    //std::map<uuid, DBTableWalker::WalkId>::iterator it =
    //    physical_device_vn_walker_id_.find(vn->GetUuid());

    DBTableWalker *walker = Agent::GetInstance()->db()->GetWalker();
    /// TODO cancel walk
    //if (it->second != DBTableWalker::kInvalidWalkerId)
    //    walker->WalkCancel(it->second);
    DBTableWalker::WalkId walk_id = DBTableWalker::kInvalidWalkerId;
    //Start a walk on VN table
    walk_id = walker->WalkTable(Agent::GetInstance()->device_manager()->
                      physical_device_vn_table(), NULL,
                      boost::bind(&MulticastHandler::TorWalker, this, _1,
                                  _2, vn),
                      boost::bind(&MulticastHandler::WalkDone, this,
                                  vn->GetUuid()));
    physical_device_vn_walker_id_[vn->GetUuid()] = walk_id;
}

MulticastGroupObject *MulticastHandler::CreateMulticastGroupObject
(const string &vrf_name, const Ip4Address &ip_addr, const string &vn_name) {
    MulticastGroupObject *obj =
        new MulticastGroupObject(vrf_name, ip_addr, vn_name);
    AddToMulticastObjList(obj);
    return obj;
}

void MulticastHandler::HandleIpam(const VnEntry *vn) {
    const uuid &vn_uuid = vn->GetUuid();
    const std::vector<VnIpam> &ipam = vn->GetVnIpam();
    bool delete_ipam = false;
    std::map<uuid, string>::iterator it;
    string vrf_name;

    if (!(vn->layer3_forwarding()) || vn->IsDeleted() || (ipam.size() == 0) ||
        (vn->GetVrf() == NULL)) {
        delete_ipam = true;
    }

    it = vn_vrf_mapping_.find(vn_uuid);
    if (it != vn_vrf_mapping_.end()) {
        vrf_name = it->second;
        vrf_ipam_mapping_.erase(vrf_name);
        if (delete_ipam) {
            vn_vrf_mapping_.erase(vn_uuid);
            return;
        }
    } else {
        if (delete_ipam == false) {
            vrf_name = vn->GetVrf()->GetName();
            vn_vrf_mapping_.insert(std::pair<uuid, string>(vn_uuid, vrf_name));
        }
    }

    if (delete_ipam)
        return;

    vrf_ipam_mapping_.insert(std::pair<string, std::vector<VnIpam> >(vrf_name,
                                                                     ipam));
}

/* Registered call for VM */
void MulticastHandler::ModifyVmInterface(DBTablePartBase *partition, 
                                         DBEntryBase *e)
{
    const Interface *intf = static_cast<const Interface *>(e);
    const VmInterface *vm_itf;

    if (intf->type() != Interface::VM_INTERFACE) {
        return;
    }

    vm_itf = static_cast<const VmInterface *>(intf);
    if (vm_itf->sub_type() == VmInterface::TOR) {
        //Ignore TOR VMI, they are not active VMI.
        return;
    }

    if (intf->IsDeleted() || (intf->l2_active() == false)) {
        MulticastHandler::GetInstance()->DeleteVmInterface(intf);
        return;
    }
    assert(vm_itf->vn() != NULL);

    MulticastHandler::GetInstance()->AddVmInterfaceInFloodGroup(vm_itf);
    return;
}

/*
 * Delete VM interface
 * Traverse the multicast obj list of which this VM is a member.
 * Delete the VM from them and check if local VM list size is zero.
 * If it is zero then delete the route and mpls.
 */
void MulticastHandler::DeleteVmInterface(const Interface *intf)
{
    const VmInterface *vm_itf = static_cast<const VmInterface *>(intf);
    std::list<MulticastGroupObject *> &obj_list = this->GetVmToMulticastObjMap(
                                                          vm_itf->GetUuid());
    for (std::list<MulticastGroupObject *>::iterator it = obj_list.begin(); 
         it != obj_list.end(); it++) {
        // When IPAM/VN goes off first than VM then it marks mc obj 
        // for deletion. Cleanup of related data structures like vm-mcobj
        // happens when VM goes off. So dont trigger any notify at same time.
        // However if all local VMs are gone then route will be deleted only
        // when VN/IPAM goes off. At that time notify xmpp to unsubscribe 
        //Deletelocalmember removes vm from mc obj 
        if (((*it)->DeleteLocalMember(vm_itf->GetUuid()) == true) && 
            ((*it)->IsDeleted() == false) &&
            ((*it)->GetLocalListSize() != 0)) {
            TriggerLocalRouteChange(*it, agent_->local_vm_peer());
            MCTRACE(Log, "modify route, vm is deleted ", (*it)->vrf_name(),
                    (*it)->GetGroupAddress().to_string(), 0);
        }

        if((*it)->GetLocalListSize() == 0) {
            MCTRACE(Info, "Del route for multicast address",
                    vm_itf->ip_addr().to_string());
            //Time to delete route(for mcast address) and mpls
            MulticastHandler::GetInstance()->
                DeleteBroadcast(agent_->local_vm_peer(),
                                (*it)->vrf_name_, 0);
            /* delete mcast object */
            DeleteMulticastObject((*it)->vrf_name_, (*it)->grp_address_);
        }
    }
    vm_to_mcobj_list_[vm_itf->GetUuid()].clear();
    DeleteVmToMulticastObjMap(vm_itf->GetUuid());
    MCTRACE(Info, "Del vm notify done ", vm_itf->ip_addr().to_string());
}

//Delete multicast object for vrf/G
void MulticastHandler::DeleteMulticastObject(const std::string &vrf_name,
                                             const Ip4Address &grp_addr) {
    for(std::set<MulticastGroupObject *>::iterator it =
        this->GetMulticastObjList().begin(); 
        it != this->GetMulticastObjList().end(); it++) {
        if (((*it)->vrf_name() == vrf_name) &&
            ((*it)->GetGroupAddress() == grp_addr)) {
            if (((*it)->GetLocalListSize() != 0) ||
                !(((*it)->tor_olist()).empty()))
                return;
            MCTRACE(Log, "delete obj  vrf/grp/size ", vrf_name,
                    grp_addr.to_string(),
                    this->GetMulticastObjList().size());
            delete (*it);
            this->GetMulticastObjList().erase(it++);
            break;
        }
    }
}

MulticastGroupObject *MulticastHandler::FindFloodGroupObject(const std::string &vrf_name) {
    boost::system::error_code ec;
    Ip4Address broadcast =  IpAddress::from_string("255.255.255.255",
                                                   ec).to_v4();
    return (FindGroupObject(vrf_name, broadcast));
}

//Helper to find object for VRF/G
MulticastGroupObject *MulticastHandler::FindGroupObject(const std::string &vrf_name, 
                                                        const Ip4Address &dip) {
    for(std::set<MulticastGroupObject *>::iterator it =
        this->GetMulticastObjList().begin(); 
        it != this->GetMulticastObjList().end(); it++) {
        if (((*it)->vrf_name() == vrf_name) &&
            ((*it)->GetGroupAddress() == dip)) {
            return (*it);
        }
    }
    MCTRACE(Log, "mcast obj size ", vrf_name, dip.to_string(),
            this->GetMulticastObjList().size());
    return NULL;
}

MulticastGroupObject *MulticastHandler::FindActiveGroupObject(
                                                    const std::string &vrf_name,
                                                    const Ip4Address &dip) {
    MulticastGroupObject *obj = FindGroupObject(vrf_name, dip);
    if ((obj == NULL) || obj->IsDeleted()) {
        MCTRACE(Log, "Multicast object deleted ", vrf_name, 
                dip.to_string(), 0);
        return NULL;
    }

    return obj;
}

ComponentNHKeyList
MulticastHandler::GetInterfaceComponentNHKeyList(MulticastGroupObject *obj,
                                                 uint8_t interface_flags) {
    ComponentNHKeyList component_nh_key_list;
    for (std::list<uuid>::const_iterator it = obj->GetLocalOlist().begin();
            it != obj->GetLocalOlist().end(); it++) {
        ComponentNHKeyPtr component_nh_key(new ComponentNHKey(0, (*it),
                                                              interface_flags));
        component_nh_key_list.push_back(component_nh_key);
    }
    return component_nh_key_list;
}

void MulticastHandler::TriggerLocalRouteChange(MulticastGroupObject *obj,
                                          const Peer *peer) {
    if (obj->layer2_forwarding() == false) {
        return;
    }

    DBRequest req;
    ComponentNHKeyList component_nh_key_list;

    component_nh_key_list =
        GetInterfaceComponentNHKeyList(obj, InterfaceNHFlags::LAYER2);
    MCTRACE(Log, "enqueue route change with local peer",
            obj->vrf_name(),
            obj->GetGroupAddress().to_string(),
            component_nh_key_list.size());
    //Add Layer2 FF:FF:FF:FF:FF:FF, local_vm_peer
    uint32_t route_tunnel_bmap = TunnelType::AllType();
    Layer2AgentRouteTable::AddLayer2BroadcastRoute(peer,
                                                   obj->vrf_name(),
                                                   obj->GetVnName(),
                                                   obj->evpn_mpls_label(),
                                                   obj->vxlan_id(),
                                                   0,
                                                   route_tunnel_bmap,
                                                   Composite::L2INTERFACE,
                                                   component_nh_key_list);
}

void MulticastHandler::TriggerRemoteRouteChange(MulticastGroupObject *obj,
                                                const Peer *peer,
                                                const string &vrf_name,
                                                const TunnelOlist &olist,
                                                uint64_t peer_identifier,
                                                bool delete_op,
                                                COMPOSITETYPE comp_type,
                                                uint32_t label,
                                                bool fabric,
                                                uint32_t ethernet_tag) {
    uint64_t obj_peer_identifier = obj ? obj->peer_identifier()
        : ControllerPeerPath::kInvalidPeerIdentifier;

    // Peer identifier cases
    // if its a delete operation -
    // 1) Internal delete (invalid peer identifier), dont update peer identifier
    // as it is a forced removal.
    // 2) Control node removing stales i.e. delete if local peer identifier is
    // less than global peer identifier.
    //
    // if its not a delete operation -
    // 1) Update only if local peer identifier is less than or equal to sent
    // global peer identifier.

    // if its internal delete then peer_identifier will be 0xFFFFFFFF;
    // if external delete(via control node) then its stale cleanup so delete
    // only when local peer identifier is less than global multicast sequence.
    if (delete_op) {
        if ((peer_identifier != ControllerPeerPath::kInvalidPeerIdentifier) &&
            (peer_identifier <= obj_peer_identifier))
            return;

        // After resetting tunnel and mpls label return if it was a delete call,
        // dont update peer_identifier. Let it get updated via update operation only
        MCTRACE(Log, "delete bcast path from remote peer", vrf_name,
                "255.255.255.255", 0);
        Layer2AgentRouteTable::DeleteBroadcastReq(peer, vrf_name,
                                                  ethernet_tag);
        ComponentNHKeyList component_nh_key_list; //dummy list
        RebakeSubnetRoute(peer, vrf_name, 0, ethernet_tag,
                          obj ? obj->GetVnName() : "",
                          true, component_nh_key_list,
                          comp_type);
        return;
    }

    // - Update operation with lower sequence number sent compared to
    // local identifier, ignore
    if (peer_identifier < obj_peer_identifier) {
        return;
    }

    // Ideally wrong update call
    if (peer_identifier == INVALID_PEER_IDENTIFIER) {
        MCTRACE(Log, "Invalid peer identifier sent for modification",
                obj->vrf_name(), "255.255.255.255", 0);
        return;
    }

    obj->set_peer_identifier(peer_identifier);
    ComponentNHKeyList component_nh_key_list;

    for (TunnelOlist::const_iterator it = olist.begin();
         it != olist.end(); it++) {
        TunnelNHKey *key =
            new TunnelNHKey(Agent::GetInstance()->fabric_vrf_name(),
                            Agent::GetInstance()->router_id(),
                            it->daddr_, false,
                            TunnelType::ComputeType(it->tunnel_bmap_));
        TunnelNHData *tnh_data = new TunnelNHData();
        DBRequest req;
        req.oper = DBRequest::DB_ENTRY_ADD_CHANGE;
        req.key.reset(key);
        req.data.reset(tnh_data);
        Agent::GetInstance()->nexthop_table()->Enqueue(&req);
        MCTRACE(Log, "Enqueue add TOR TUNNEL ",
                Agent::GetInstance()->fabric_vrf_name(),
                it->daddr_.to_string(), it->label_);

        ComponentNHKeyPtr component_key_ptr(new ComponentNHKey(it->label_,
                    agent_->fabric_vrf_name(),
                    agent_->router_id(), it->daddr_,
                    false, it->tunnel_bmap_));
        component_nh_key_list.push_back(component_key_ptr);
    }

    MCTRACE(Log, "enqueue route change with remote peer",
            obj->vrf_name(),
            obj->GetGroupAddress().to_string(),
            component_nh_key_list.size());

    //Add Layer2 FF:FF:FF:FF:FF:FF$
    uint32_t route_tunnel_bmap = TunnelType::AllType();
    if (comp_type == Composite::TOR)
        route_tunnel_bmap = TunnelType::VxlanType();
    Layer2AgentRouteTable::AddLayer2BroadcastRoute(peer,
                                                   obj->vrf_name(),
                                                   obj->GetVnName(),
                                                   label,
                                                   obj->vxlan_id(),
                                                   ethernet_tag,
                                                   route_tunnel_bmap,
                                                   comp_type,
                                                   component_nh_key_list);
    //if ((comp_type == Composite::EVPN) || (comp_type == Composite::TOR)) {
        MCTRACE(Log, "rebake subnet peer for subnet", vrf_name,
                "255.255.255.255", comp_type);
        RebakeSubnetRoute(peer, obj->vrf_name(), label, obj->vxlan_id(),
                          obj->GetVnName(), false, component_nh_key_list,
                          comp_type);
    //}
}

void MulticastHandler::RebakeSubnetRoute(const Peer *peer,
                                         const std::string &vrf_name,
                                         uint32_t label,
                                         uint32_t vxlan_id,
                                         const std::string &vn_name,
                                         bool del_op,
                                         const ComponentNHKeyList &comp_nh_list,
                                         COMPOSITETYPE comp_type)
{
    std::vector<VnIpam> &vrf_ipam =
        (vrf_ipam_mapping_.find(vrf_name))->second;
    for (std::vector<VnIpam>::iterator it = vrf_ipam.begin();
         it != vrf_ipam.end(); it++) {
        const Ip4Address &subnet_addr = (*it).GetSubnetAddress();
        DBRequest req;

        req.key.reset(new InetUnicastRouteKey(peer, vrf_name, subnet_addr,
                                              (*it).plen));
        if (del_op == false) {
            DBRequest nh_req;
            nh_req.oper = DBRequest::DB_ENTRY_ADD_CHANGE;
            nh_req.key.reset(new CompositeNHKey(comp_type, false,
                                                comp_nh_list, vrf_name));
            nh_req.data.reset(new CompositeNHData());
            //add route
            req.oper = DBRequest::DB_ENTRY_ADD_CHANGE;
            req.data.reset(new SubnetRoute(vn_name,
                                           vxlan_id,
                                           nh_req));
        } else {
            //del route
            req.oper = DBRequest::DB_ENTRY_DELETE;
            req.data.reset(NULL);
        }

        agent_->fabric_inet4_unicast_table()->Enqueue(&req);
    }
}

void MulticastHandler::AddVmInterfaceInFloodGroup(const VmInterface *vm_itf) {
    const string vrf_name = vm_itf->vrf()->GetName();
    const uuid intf_uuid = vm_itf->GetUuid();
    const VnEntry *vn = vm_itf->vn();
    MulticastGroupObject *all_broadcast = NULL;
    boost::system::error_code ec;
    Ip4Address broadcast =  IpAddress::from_string("255.255.255.255",
                                                   ec).to_v4();
    bool add_route = false;
    std::string vn_name = vn->GetName();

    //TODO Push every thing via multi proto and remove multi proto check 
    //All broadcast addition 255.255.255.255
    all_broadcast = this->FindGroupObject(vrf_name, broadcast);
    if (all_broadcast == NULL) {
        all_broadcast = CreateMulticastGroupObject(vrf_name, broadcast, 
                                                   vn_name);
        add_route = true;
    }

    all_broadcast->set_vxlan_id(vn->GetVxLanId());
    //Modify Nexthops
    if (all_broadcast->AddLocalMember(intf_uuid) == true) {
        if (vn->layer2_forwarding()) {
            TriggerLocalRouteChange(all_broadcast, agent_->local_vm_peer());
        }
        AddVmToMulticastObjMap(intf_uuid, all_broadcast);
    }
    //Modify routes
    if ((add_route || (all_broadcast->layer2_forwarding() != 
                       vn->layer2_forwarding())) && vn->layer2_forwarding()) {
        if (TunnelType::ComputeType(TunnelType::AllType()) ==
            TunnelType::VXLAN) {
            all_broadcast->set_vxlan_id(vn->GetVxLanId());
        } 
        all_broadcast->SetLayer2Forwarding(vn->layer2_forwarding());
        //TODO OVS no need to alloc mpls label for OVS VMI.
        uint32_t evpn_label = Agent::GetInstance()->mpls_table()->
            AllocLabel();
        if (evpn_label != MplsLabel::INVALID) { 
            all_broadcast->set_evpn_mpls_label(evpn_label);
        } else {
            MCTRACE(Log, "allocation of  evpn mpls label failed",
                    vrf_name, broadcast.to_string(), 0);
        }
        TriggerLocalRouteChange(all_broadcast, agent_->local_vm_peer());
    }
}

/*
 * Static funtion to be called to handle XMPP info from ctrl node
 * Key is VRF/G/S
 * Info has label (for source to vrouter) and
 * OLIST of NH (server IP + label for that server)
 */
void MulticastHandler::ModifyFabricMembers(const Peer *peer,
                                           const std::string &vrf_name,
                                           const Ip4Address &grp,
                                           const Ip4Address &src,
                                           uint32_t label,
                                           const TunnelOlist &olist,
                                           uint64_t peer_identifier)
{
    MulticastGroupObject *obj = 
        MulticastHandler::GetInstance()->FindActiveGroupObject(vrf_name, grp);
    MCTRACE(Log, "XMPP call(edge replicate) multicast handler ", vrf_name,
            grp.to_string(), label);

    bool delete_op = false;

    //Invalid peer identifier signifies delete.
    //If add operation, obj should exist, else return.
    if (peer_identifier == ControllerPeerPath::kInvalidPeerIdentifier) {
        delete_op = true;
    } else if (obj == NULL) {
        return;
    }

    MulticastHandler::GetInstance()->TriggerRemoteRouteChange(obj, peer,
                                     vrf_name, olist, peer_identifier,
                                     delete_op, Composite::FABRIC,
                                     label, true, 0);
    MCTRACE(Log, "Add fabric grp label ", vrf_name, grp.to_string(), label);
}

/*
 * Request to populate evpn olist by list of TOR NH seen by control node
 * Currently this is done only for TOR/Gateway(outside contrail vrouter network)
 * Source label is ignored as it is used by non-vrouters.
 * Olist consists of TOR/Gateway endpoints with label advertised or use VXLAN.
 * Note: Non Vrouter can talk in VXLAN/MPLS. Encap received in XMPP will
 * convey the same.
 */
void MulticastHandler::ModifyEvpnMembers(const Peer *peer,
                                         const std::string &vrf_name,
                                         const TunnelOlist &olist,
                                         uint32_t ethernet_tag,
                                         uint64_t peer_identifier)
{
    boost::system::error_code ec;
    Ip4Address grp = Ip4Address::from_string("255.255.255.255", ec);
    MulticastGroupObject *obj =
        MulticastHandler::GetInstance()->FindActiveGroupObject(vrf_name, grp);
    MCTRACE(Log, "XMPP call(EVPN) multicast handler ", vrf_name,
            grp.to_string(), 0);

    if (obj == NULL) {
        return;
    }

    bool delete_op = false;
    if (peer_identifier == ControllerPeerPath::kInvalidPeerIdentifier) {
        delete_op = true;
    }

    MulticastHandler::GetInstance()->TriggerRemoteRouteChange(obj, peer, vrf_name, olist,
                                     peer_identifier, delete_op, Composite::EVPN,
                                     obj->evpn_mpls_label(), false, ethernet_tag);
    MCTRACE(Log, "Add EVPN TOR Olist ", vrf_name, grp.to_string(), 0);
}

void MulticastHandler::ModifyTorMembers(const Peer *peer,
                                        const std::string &vrf_name,
                                        const TunnelOlist &olist,
                                        uint32_t ethernet_tag,
                                        uint64_t peer_identifier)
{
    boost::system::error_code ec;
    Ip4Address grp = Ip4Address::from_string("255.255.255.255", ec);
    MulticastGroupObject *obj =
        MulticastHandler::GetInstance()->FindActiveGroupObject(vrf_name, grp);
    MCTRACE(Log, "TOR multicast handler ", vrf_name, grp.to_string(), 0);

    if (obj == NULL) {
        return;
    }

    bool delete_op = false;
    if (peer_identifier == ControllerPeerPath::kInvalidPeerIdentifier) {
        delete_op = true;
    }

    MulticastHandler::GetInstance()->TriggerRemoteRouteChange(obj, peer, vrf_name, olist,
                                     peer_identifier, delete_op, Composite::TOR,
                                     MplsTable::kInvalidLabel, false, ethernet_tag);
    MCTRACE(Log, "Add external TOR Olist ", vrf_name, grp.to_string(), 0);
}

const OlistTunnelEntry *MulticastGroupObject::FindInTorList
(const Ip4Address &ip_addr, uint32_t vxlan_id, uint32_t tunnel_bmap) {
    for (std::vector<OlistTunnelEntry>::iterator it = tor_olist_.begin();
         it != tor_olist_.end(); it++) {
        if (((*it).daddr_ == ip_addr) && ((*it).label_ == vxlan_id) &&
            ((*it).tunnel_bmap_ == tunnel_bmap)) {
            return &(*it);
        }
    }
    return NULL;
}

void MulticastGroupObject::AddInTorList(const Ip4Address &ip_addr,
                                        uint32_t vxlan_id,
                                        uint32_t tunnel_bmap) {
    if (FindInTorList(ip_addr, vxlan_id, tunnel_bmap))
        return;

    tor_olist_.push_back(OlistTunnelEntry(vxlan_id, ip_addr,
                                          tunnel_bmap));
}

void MulticastGroupObject::DeleteFromTorList(const Ip4Address &ip_addr,
                                             uint32_t vxlan_id,
                                             uint32_t tunnel_bmap) {
    for (std::vector<OlistTunnelEntry>::iterator it = tor_olist_.begin();
         it != tor_olist_.end(); it++) {
        if (((*it).daddr_ == ip_addr) && ((*it).label_ == vxlan_id) &&
            ((*it).tunnel_bmap_ == tunnel_bmap)) {
            tor_olist_.erase(it);
            return;
        }
    }
}

// Helper to delete fabric nh
// For internal delete it uses invalid identifier. 
// For delete via control node it uses the sequence sent.
void MulticastGroupObject::FlushAllPeerInfo(const Agent *agent,
                                            const Peer *peer,
                                            uint64_t peer_identifier) {
    if ((peer_identifier != peer_identifier_) ||
        (peer_identifier == INVALID_PEER_IDENTIFIER)) {
        MulticastHandler::GetInstance()->
            DeleteBroadcast(peer, vrf_name_, 0);
    }
    MCTRACE(Log, "Delete broadcast route", vrf_name_,
            grp_address_.to_string(), 0);
}

MulticastHandler::MulticastHandler(Agent *agent)
        : agent_(agent),
          vn_listener_id_(DBTable::kInvalidId),
          interface_listener_id_(DBTable::kInvalidId) { 
    obj_ = this; 
}

bool MulticastHandler::FlushPeerInfo(uint64_t peer_sequence) {
    for (std::set<MulticastGroupObject *>::iterator it = 
         GetMulticastObjList().begin(); it != GetMulticastObjList().end(); 
         it++) {
        //Delete all control node given paths
        (*it)->FlushAllPeerInfo(agent_, agent_->multicast_tree_builder_peer(),
                                peer_sequence);
    }
    return false;
}

/*
 * Shutdown for clearing all stuff related to multicast
 */
void MulticastHandler::Shutdown() {
    const Agent *agent = MulticastHandler::GetInstance()->agent();
    //Delete all route mpls and trigger cnh change

    for (std::set<MulticastGroupObject *>::iterator it =
         MulticastHandler::GetInstance()->GetMulticastObjList().begin();
         it != MulticastHandler::GetInstance()->GetMulticastObjList().end();
         it++) {
        MulticastGroupObject *obj = (*it);
        AgentRoute *route = Layer2AgentRouteTable::FindRoute(agent,
                                                             obj->vrf_name(),
                                                             MacAddress::BroadcastMac());
        if (route == NULL)
            return;

        for(Route::PathList::iterator it = route->GetPathList().begin();
            it != route->GetPathList().end(); it++) {
            const AgentPath *path =
                static_cast<const AgentPath *>(it.operator->());
            //Delete the tunnel OLIST
            (obj)->FlushAllPeerInfo(agent,
                                    path->peer(),
                                    INVALID_PEER_IDENTIFIER);
        }
        //Delete the multicast object
        delete (obj);
    }
}
