/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <boost/uuid/uuid_io.hpp>
#include <vnc_cfg_types.h>
#include <cmn/agent_cmn.h>

#include <ifmap/ifmap_node.h>
#include <cfg/cfg_init.h>
#include <cfg/cfg_listener.h>
#include <oper/agent_sandesh.h>
#include <oper/ifmap_dependency_manager.h>
#include <oper/interface_common.h>
#include <oper/physical_device.h>
#include <oper/nexthop.h>

#include <vector>
#include <string>

using std::string;


/////////////////////////////////////////////////////////////////////////////
// PhysicalInterface routines
/////////////////////////////////////////////////////////////////////////////
PhysicalInterface::PhysicalInterface(const boost::uuids::uuid &uuid,
                                     const std::string &name, VrfEntry *vrf) :
    Interface(Interface::PHYSICAL, uuid, name, vrf),
    persistent_(false), subtype_(INVALID), encap_type_(ETHERNET),
    no_arp_(false) {
}

PhysicalInterface::~PhysicalInterface() {
}

string PhysicalInterface::ToString() const {
    if (uuid_ != nil_uuid()) {
        return name_ + ":" + UuidToString(uuid_);
    } else {
        return name_;
    }
}

bool PhysicalInterface::CmpInterface(const DBEntry &rhs) const {
    const PhysicalInterface &a = static_cast<const PhysicalInterface &>(rhs);

    if (uuid_ != a.uuid_) {
        return (uuid_ < a.uuid_);
    }

    if (uuid_ != nil_uuid()) {
        return false;
    }

    return name_ < a.name_;
}

DBEntryBase::KeyPtr PhysicalInterface::GetDBRequestKey() const {
    InterfaceKey *key = new PhysicalInterfaceKey(uuid_, name_);
    return DBEntryBase::KeyPtr(key);
}

void PhysicalInterface::Add() {
}

bool PhysicalInterface::OnChange(const InterfaceTable *table,
                                 const PhysicalInterfaceBaseData *data) {
    bool ret = false;

    if (subtype_ == FABRIC || subtype_ == VMWARE) {
        return ret;
    }

    const ConfigPhysicalInterfaceData *cfg_data =
        static_cast<const ConfigPhysicalInterfaceData *>(data);
    if (fq_name_ != cfg_data->fq_name_) {
        fq_name_ = cfg_data->fq_name_;
        ret = true;
    }

    PhysicalDeviceEntry *dev =
        table->agent()->physical_device_table()->Find(cfg_data->device_uuid_);
    if (dev != physical_device_.get()) {
        physical_device_.reset(dev);
        ret = true;
    }

    return ret;
}

bool PhysicalInterface::Delete(const DBRequest *req) {
    InterfaceNH::DeletePhysicalInterfaceNh(uuid_, name_);
    return true;
}

void PhysicalInterface::PostAdd() {
    InterfaceNH::CreatePhysicalInterfaceNh(uuid_, name_, mac_);

    InterfaceTable *table = static_cast<InterfaceTable *>(get_table());
    if (table->agent()->test_mode()) {
        return;
    }

    // Interfaces in VMWARE must be put into promiscous mode
    if (subtype_ != VMWARE) {
        return;
    }

    int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    assert(fd >= 0);

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, name_.c_str(), IF_NAMESIZE);
    if (ioctl(fd, SIOCGIFFLAGS, (void *)&ifr) < 0) {
        LOG(ERROR, "Error <" << errno << ": " << strerror(errno) <<
            "> setting promiscuous flag for interface <" << name_ << ">");
        close(fd);
        return;
    }

    ifr.ifr_flags |= IFF_PROMISC;
    if (ioctl(fd, SIOCSIFFLAGS, (void *)&ifr) < 0) {
        LOG(ERROR, "Error <" << errno << ": " << strerror(errno) <<
            "> setting promiscuous flag for interface <" << name_ << ">");
        close(fd);
        return;
    }

    close(fd);
}

/////////////////////////////////////////////////////////////////////////////
// PhysicalInterfaceKey routines
/////////////////////////////////////////////////////////////////////////////

PhysicalInterfaceKey::PhysicalInterfaceKey(const boost::uuids::uuid &uuid,
                                           const std::string &name) :
    InterfaceKey(AgentKey::ADD_DEL_CHANGE, Interface::PHYSICAL, uuid,
                 name, false) {
}

PhysicalInterfaceKey::~PhysicalInterfaceKey() {
}

Interface *PhysicalInterfaceKey::AllocEntry(const InterfaceTable *table) const {
    return new PhysicalInterface(uuid_, name_, NULL);
}

Interface *PhysicalInterfaceKey::AllocEntry(const InterfaceTable *table,
                                            const InterfaceData *data) const {
    VrfKey key(data->vrf_name_);
    VrfEntry *vrf = static_cast<VrfEntry *>
        (table->agent()->vrf_table()->FindActiveEntry(&key));
    assert(vrf);

    PhysicalInterface *intf = new PhysicalInterface(uuid_, name_, vrf);

    const PhysicalInterfaceBaseData *phy_data =
        static_cast<const PhysicalInterfaceBaseData *>(data);
    intf->subtype_ = phy_data->subtype_;
    if (intf->subtype_ == PhysicalInterface::VMWARE) {
        intf->persistent_ = true;
    }

    intf->encap_type_ = phy_data->encap_;
    intf->no_arp_ = phy_data->no_arp_;

    intf->OnChange(table, phy_data);
    return intf;
}

InterfaceKey *PhysicalInterfaceKey::Clone() const {
    return new PhysicalInterfaceKey(uuid_, name_);
}

/////////////////////////////////////////////////////////////////////////////
// PhysicalInterfaceData routines
/////////////////////////////////////////////////////////////////////////////
PhysicalInterfaceBaseData::PhysicalInterfaceBaseData
    (const std::string &vrf_name, PhysicalInterface::SubType subtype,
     PhysicalInterface::EncapType encap, bool no_arp) :
    InterfaceData(), subtype_(subtype), encap_(encap), no_arp_(no_arp) {
    EthInit(vrf_name);
}


PhysicalInterfaceData::PhysicalInterfaceData(const string &vrf_name,
                                             PhysicalInterface::SubType subtype,
                                             PhysicalInterface::EncapType encap,
                                             bool no_arp) :
    PhysicalInterfaceBaseData(vrf_name, subtype, encap, no_arp) {
}
    
ConfigPhysicalInterfaceData::ConfigPhysicalInterfaceData
    (const std::string &vrf_name, const std::string &fq_name,
     const boost::uuids::uuid &device_uuid, PhysicalInterface::SubType subtype,
     PhysicalInterface::EncapType encap, bool no_arp) :
    PhysicalInterfaceBaseData(vrf_name, subtype, encap, no_arp),
    fq_name_(fq_name), device_uuid_(device_uuid) {
}

/////////////////////////////////////////////////////////////////////////////
// Config handling routines
/////////////////////////////////////////////////////////////////////////////
static PhysicalInterfaceKey *BuildKey(const autogen::PhysicalInterface *port) {
    autogen::IdPermsType id_perms = port->id_perms();
    boost::uuids::uuid u;
    CfgUuidSet(id_perms.uuid.uuid_mslong, id_perms.uuid.uuid_lslong, u);
    return new PhysicalInterfaceKey(u, port->display_name());
}

static ConfigPhysicalInterfaceData *BuildData(const Agent *agent,
    IFMapNode *node, const autogen::PhysicalInterface *port) {

    boost::uuids::uuid dev_uuid = nil_uuid();
    // Find link with physical-router adjacency
    IFMapNode *adj_node = NULL;
    adj_node = agent->cfg_listener()->FindAdjacentIFMapNode(agent, node,
                                                            "physical-router");
    if (adj_node) {
        autogen::PhysicalRouter *router =
            static_cast<autogen::PhysicalRouter *>(adj_node->GetObject());
        autogen::IdPermsType id_perms = router->id_perms();
        CfgUuidSet(id_perms.uuid.uuid_mslong, id_perms.uuid.uuid_lslong,
                   dev_uuid);
    }

    PhysicalInterface::SubType subtype = PhysicalInterface::CONFIG;
    // TODO : Override subtype if attached vrouter is same as agent

    return new ConfigPhysicalInterfaceData(agent->fabric_vrf_name(),
                                           node->name(), dev_uuid, subtype,
                                           PhysicalInterface::ETHERNET,
                                           false);
}

bool InterfaceTable::PhysicalInterfaceIFNodeToReq(IFMapNode *node,
                                                  DBRequest &req) {
    autogen::PhysicalInterface *port =
        static_cast <autogen::PhysicalInterface *>(node->GetObject());
    assert(port);

    req.key.reset(BuildKey(port));
    if (node->IsDeleted()) {
        req.oper = DBRequest::DB_ENTRY_DELETE;
        return true;
    }

    req.oper = DBRequest::DB_ENTRY_ADD_CHANGE;
    req.data.reset(BuildData(agent(), node, port));
    return true;
}

void PhysicalInterface::ConfigEventHandler(IFMapNode *node) {
}

/////////////////////////////////////////////////////////////////////////////
// Utility methods
/////////////////////////////////////////////////////////////////////////////
// Enqueue DBRequest to create a Host Interface
void PhysicalInterface::CreateReq(InterfaceTable *table, const string &ifname,
                                  const string &vrf_name, SubType subtype,
                                  EncapType encap, bool no_arp) {
    DBRequest req(DBRequest::DB_ENTRY_ADD_CHANGE);
    req.key.reset(new PhysicalInterfaceKey(nil_uuid(), ifname));
    req.data.reset(new PhysicalInterfaceData(vrf_name, subtype, encap, no_arp));
    table->Enqueue(&req);
}

void PhysicalInterface::Create(InterfaceTable *table, const string &ifname,
                               const string &vrf_name, SubType subtype,
                               EncapType encap, bool no_arp) {
    DBRequest req(DBRequest::DB_ENTRY_ADD_CHANGE);
    req.key.reset(new PhysicalInterfaceKey(nil_uuid(), ifname));
    req.data.reset(new PhysicalInterfaceData(vrf_name, subtype, encap, no_arp));
    table->Process(req);
}

// Enqueue DBRequest to delete a Host Interface
void PhysicalInterface::DeleteReq(InterfaceTable *table, const string &ifname) {
    DBRequest req(DBRequest::DB_ENTRY_DELETE);
    req.key.reset(new PhysicalInterfaceKey(nil_uuid(), ifname));
    req.data.reset(NULL);
    table->Enqueue(&req);
}

void PhysicalInterface::Delete(InterfaceTable *table, const string &ifname) {
    DBRequest req(DBRequest::DB_ENTRY_DELETE);
    req.key.reset(new PhysicalInterfaceKey(nil_uuid(), ifname));
    req.data.reset(NULL);
    table->Process(req);
}
