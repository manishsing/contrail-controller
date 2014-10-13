/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <boost/bind.hpp>

#include <ovsdb_client_tcp.h>

OvsdbClientTcp::OvsdbClientTcp(Agent *agent,
        boost::asio::ip::address ip_addr, int port) :
        TcpServer(agent->event_manager()), agent_(agent),
        server_ep_(ip_addr, port) {
    session_ = CreateSession();
    Connect(session_, server_ep_);
}

TcpSession *OvsdbClientTcp::AllocSession(Socket *socket) {
    TcpSession *session = new OvsdbClientTcpSession(agent_, this, socket);
    session->set_observer(boost::bind(&OvsdbClientTcp::OnSessionEvent,
                                      this, _1, _2));
    return session;
}

void OvsdbClientTcp::OnSessionEvent(TcpSession *session,
        TcpSession::Event event) {
    OvsdbClientTcpSession *tcp = static_cast<OvsdbClientTcpSession *>(session);
    switch (event) {
    case TcpSession::CONNECT_FAILED:
        /* Failed to Connect, Try Again! */
        Connect(session_, server_ep_);
        break;
    case TcpSession::CLOSE:
        /* TODO need to handle reconnects */
        tcp->OnClose();
        break;
    case TcpSession::CONNECT_COMPLETE:
        tcp->OnEstablish();
        break;
    default:
        break;
    }
}

OvsdbClientTcpSession::OvsdbClientTcpSession(Agent *agent, TcpServer *server,
        Socket *sock, bool async_ready) : OvsdbClientSession(agent),
    TcpSession(server, sock, async_ready) {
    reader_ = new OvsdbClientTcpSessionReader(this, 
            boost::bind(&OvsdbClientTcpSession::RecvMsg, this, _1, _2));
    receive_queue_ = new WorkQueue<queue_msg>(
            TaskScheduler::GetInstance()->GetTaskId("Agent::KSync"), -1,
            boost::bind(&OvsdbClientTcpSession::ReceiveDequeue, this, _1));
}

void OvsdbClientTcpSession::OnRead(Buffer buffer) {
    reader_->OnRead(buffer);
}

OvsdbClientTcpSessionReader::OvsdbClientTcpSessionReader(
    TcpSession *session, ReceiveCallback callback) : 
    TcpMessageReader(session, callback) {
}

int OvsdbClientTcpSessionReader::MsgLength(Buffer buffer, int offset) {
    size_t size = TcpSession::BufferSize(buffer);
    int remain = size - offset;
    if (remain < GetHeaderLenSize()) {
        return -1;
    }

    return remain;
}

