/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __OVSDB_API_H__
#define __OVSDB_API_H__

extern "C" {
#include <stdlib.h>
/* OVSDB APIs which can be directly used */
extern void ovsdb_idl_destroy(struct ovsdb_idl *);
extern void ovsdb_idl_set_callback(struct ovsdb_idl *idl, void *idl_base,
        void (*cb)(void*, int, struct ovsdb_idl_row *));
extern struct ovsdb_idl_txn *ovsdb_idl_txn_create(struct ovsdb_idl *idl);
extern struct jsonrpc_msg *ovsdb_idl_txn_encode(struct ovsdb_idl_txn *txn);
extern struct jsonrpc_msg *ovsdb_idl_encode_monitor_request(struct ovsdb_idl *);
extern struct json *jsonrpc_msg_to_json(struct jsonrpc_msg *);
extern void ovsdb_idl_msg_process(struct ovsdb_idl *, struct jsonrpc_msg *msg);
extern char *json_to_string(const struct json *, int);
extern void json_destroy(struct json *);
extern struct json_parser *json_parser_create(int);
extern size_t json_parser_feed(json_parser *, const char *, size_t);
extern bool json_parser_is_done(const struct json_parser *);
extern struct json *json_parser_finish(struct json_parser *);
extern char *jsonrpc_msg_from_json(struct json *, struct jsonrpc_msg **);
extern void jsonrpc_msg_destroy(struct jsonrpc_msg *msg);

/* Wrapper for C APIs */
int ovsdb_api_row_type(struct ovsdb_idl_row *row);
struct ovsdb_idl * ovsdb_api_idl_create();
const struct vteprec_global *ovsdb_api_vteprec_global_first(struct ovsdb_idl *);
bool ovsdb_api_msg_echo_req(struct jsonrpc_msg *msg);
bool ovsdb_api_msg_echo_reply(struct jsonrpc_msg *msg);
struct jsonrpc_msg* ovsdb_api_jsonrpc_create_reply(struct jsonrpc_msg *msg);

/* Physical Switch */
char *ovsdb_api_physical_switch_name(struct ovsdb_idl_row *row);

/* Logical Switch */
char *ovsdb_api_logical_switch_name(struct ovsdb_idl_row *row);
int64_t ovsdb_api_logical_switch_tunnel_key(struct ovsdb_idl_row *row);
void ovsdb_api_add_logical_switch(struct ovsdb_idl_txn *, const char *, int64_t);
};
#endif //__OVSDB_API_H__
