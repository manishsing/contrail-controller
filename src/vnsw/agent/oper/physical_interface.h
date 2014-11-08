/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */
#ifndef vnsw_agent_physical_interface_hpp
#define vnsw_agent_physical_interface_hpp

/////////////////////////////////////////////////////////////////////////////
// Implementation of Physical Ports
// Can be Ethernet Ports or LAG Ports
// Name of port is used as key
/////////////////////////////////////////////////////////////////////////////
class PhysicalInterface : public Interface {
public:
    enum SubType {
        FABRIC,     // Physical port connecting to fabric network
        VMWARE,     // For vmware, port connecting to contrail-vm-portgroup
        INVALID
    };

    enum EncapType {
        ETHERNET,       // Ethernet with ARP
        RAW_IP          // No L2 header. Packets sent as raw-ip
    };

    PhysicalInterface(const std::string &name, VrfEntry *vrf, SubType subtype,
                      EncapType encap_type, bool no_arp);
    virtual ~PhysicalInterface();

    bool CmpInterface(const DBEntry &rhs) const;

    std::string ToString() const { return "ETH <" + name() + ">"; }
    KeyPtr GetDBRequestKey() const;
    virtual void Add();
    virtual bool Delete(const DBRequest *req);
    void PostAdd();

    SubType subtype() const { return subtype_; }
    // Lets kernel know if physical interface is to be kept after agent exits or
    // dies. If its true keep the interface, else remove it.
    // Currently only vnware physical interface is persistent.
    // By default every physical interface is non-persistent.
    bool persistent() const {return persistent_;}
    EncapType encap_type() const { return encap_type_; }
    bool no_arp() const { return no_arp_; }

    // Helper functions
    static void CreateReq(InterfaceTable *table, const std::string &ifname,
                          const std::string &vrf_name, SubType subtype,
                          EncapType encap, bool no_arp);
    static void Create(InterfaceTable *table, const std::string &ifname,
                       const std::string &vrf_name, SubType sub_type,
                       EncapType encap, bool no_arp);
    static void DeleteReq(InterfaceTable *table, const std::string &ifname);
    static void Delete(InterfaceTable *table, const std::string &ifname);
private:
    bool persistent_;
    SubType subtype_;
    EncapType encap_type_;
    bool no_arp_;
    DISALLOW_COPY_AND_ASSIGN(PhysicalInterface);
};

struct PhysicalInterfaceData : public InterfaceData {
    PhysicalInterfaceData(const std::string &vrf_name,
                          PhysicalInterface::SubType subtype,
                          PhysicalInterface::EncapType encap,
                          bool no_arp);
    PhysicalInterface::SubType subtype_;
    PhysicalInterface::EncapType encap_type_;
    bool no_arp_;
};

struct PhysicalInterfaceKey : public InterfaceKey {
    PhysicalInterfaceKey(const std::string &name);
    virtual ~PhysicalInterfaceKey();

    Interface *AllocEntry(const InterfaceTable *table) const;
    Interface *AllocEntry(const InterfaceTable *table,
                          const InterfaceData *data) const;
    InterfaceKey *Clone() const;
};

#endif // vnsw_agent_physical_interface_hpp
