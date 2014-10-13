/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __OVSDB_CLIENT_TCP_H__
#define __OVSDB_CLIENT_TCP_H__
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>

#include <io/tcp_session.h>
#include <base/queue_task.h>

#include <cmn/agent_cmn.h>
#include <cmn/agent.h>
#include <agent_types.h>
#include <ovsdb_client.h>
#include <ovsdb_client_idl.h>

class OvsdbClientTcpSessionReader : public TcpMessageReader {
public:
     OvsdbClientTcpSessionReader(TcpSession *session, ReceiveCallback callback);
     virtual ~OvsdbClientTcpSessionReader() {}

protected:
    virtual int MsgLength(Buffer buffer, int offset);

    virtual const int GetHeaderLenSize() {
        // We don't have any header
        return 0;
    }

    virtual const int GetMaxMessageSize() {
        return kMaxMessageSize;
    }

private:
    static const int kMaxMessageSize = 4096;
};

class OvsdbClientTcpSession : public OvsdbClientSession, public TcpSession {
public:
    struct queue_msg {
        u_int8_t *buf;
        std::size_t len;
    };
    OvsdbClientTcpSession(Agent *agent, TcpServer *server, Socket *sock,
                        bool async_ready = true);
    ~OvsdbClientTcpSession() {
        delete reader_;
        receive_queue_->Shutdown();
        delete receive_queue_;
    }

    void SendMsg(u_int8_t *buf, std::size_t len) {
        Send(buf, len, NULL);
    }

    void RecvMsg(const u_int8_t *buf, std::size_t len) {
        queue_msg msg;
        msg.buf = (u_int8_t *)malloc(len);
        memcpy(msg.buf, buf, len);
        msg.len = len;
        receive_queue_->Enqueue(msg);
    }

    bool ReceiveDequeue(queue_msg msg) {
        MessageProcess(msg.buf, msg.len);
        free(msg.buf);
        return true;
    }

protected:
    virtual void OnRead(Buffer buffer);
private:
    OvsdbClientTcpSessionReader *reader_;
    WorkQueue<queue_msg> *receive_queue_;
};

class OvsdbClientTcp : public TcpServer, public OvsdbClient {
public:
    OvsdbClientTcp(Agent *agent, boost::asio::ip::address ip_addr,
                   int port);
    virtual ~OvsdbClientTcp() { };

    virtual TcpSession *AllocSession(Socket *socket);
    void OnSessionEvent(TcpSession *session, TcpSession::Event event);
private:
    friend class OvsdbClientTcpSession;
    Agent *agent_;
    TcpSession *session_;
    boost::asio::ip::tcp::endpoint server_ep_;
};

#endif //__OVSDB_CLIENT_TCP_H__

