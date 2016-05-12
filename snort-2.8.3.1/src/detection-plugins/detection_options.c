/* $Id$ */
/*
** Copyright (C) 2007-2008 Sourcefire, Inc.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
**/

/**
**  @file        detection_options.c
**
**  @author      Steven Sturges
** 
**  @brief       Support functions for rule option tree
**
**  This implements tree processing for rule options, evaluating common
**  detection options only once per pattern match.
**
*/

#ifdef DETECTION_OPTION_TREE

#include "sfutil/sfxhash.h"
#include "sfutil/sfhashfcn.h"
#include "detection_options.h"
#include "rules.h"
#include "util.h"
#include "fpcreate.h"

#include "sp_asn1.h"
#include "sp_byte_check.h"
#include "sp_byte_jump.h"
#include "sp_clientserver.h"
#include "sp_cvs.h"
#include "sp_dsize_check.h"
#include "sp_flowbits.h"
#include "sp_ftpbounce.h"
#include "sp_icmp_code_check.h"
#include "sp_icmp_id_check.h"
#include "sp_icmp_seq_check.h"
#include "sp_icmp_type_check.h"
#include "sp_ip_fragbits.h"
#include "sp_ip_id_check.h"
#include "sp_ipoption_check.h"
#include "sp_ip_proto.h"
#include "sp_ip_same_check.h"
#include "sp_ip_tos_check.h"
#include "sp_isdataat.h"
#include "sp_pattern_match.h"
#include "sp_pcre.h"
#if defined(ENABLE_RESPONSE) || defined(ENABLE_REACT)
#include "sp_react.h"
#endif
#if defined(ENABLE_RESPONSE) && !defined(ENABLE_RESPONSE2)
#include "sp_respond.h"
#endif
#if defined(ENABLE_RESPONSE2) && !defined(ENABLE_RESPONSE)
#include "sp_respond2.h"
#endif
#include "sp_rpc_check.h"
#include "sp_session.h"
#include "sp_tcp_ack_check.h"
#include "sp_tcp_flag_check.h"
#include "sp_tcp_seq_check.h"
#include "sp_tcp_win_check.h"
#include "sp_ttl_check.h"
#include "sp_urilen_check.h"

#include "sp_preprocopt.h"
#include "sp_dynamic.h"

#include "fpdetect.h"
#include "ppm.h"
#include "profiler.h"

extern const u_int8_t *doe_ptr;

SFXHASH *detection_option_hash_table = NULL;
SFXHASH *detection_option_tree_hash_table = NULL;

typedef struct _detection_option_key
{
    option_type_t option_type;
    void *option_data;
} detection_option_key_t;

#define HASH_RULE_OPTIONS 16384
#define HASH_RULE_TREE 8192

u_int32_t detection_option_hash_func(SFHASHFCN *p, unsigned char *k, int n)
{
    u_int32_t hash = 0;
    detection_option_key_t *key = (detection_option_key_t*)k;

    switch (key->option_type)
    {
        /* Call hash function specific to the key type */
        case RULE_OPTION_TYPE_ASN1:
            hash = Asn1Hash(key->option_data);
            break;
        case RULE_OPTION_TYPE_BYTE_TEST:
            hash = ByteTestHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_BYTE_JUMP:
            hash = ByteJumpHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_FLOW:
            hash = FlowHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_CVS:
            hash = CvsHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_DSIZE:
            hash = DSizeCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_FLOWBIT:
            hash = FlowBitsHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_FTPBOUNCE:
            break;
        case RULE_OPTION_TYPE_ICMP_CODE:
            hash = IcmpCodeCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_ID:
            hash = IcmpIdCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_SEQ:
            hash = IcmpSeqCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_TYPE:
            hash = IcmpTypeCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_FRAGBITS:
            hash = IpFragBitsCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_FRAG_OFFSET:
            hash = IpFragOffsetCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_ID:
            hash = IpIdCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_OPTION:
            hash = IpOptionCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_PROTO:
            hash = IpProtoCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_SAME:
            hash = IpSameCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_TOS:
            hash = IpTosCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_IS_DATA_AT:
            hash = IsDataAtHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_CONTENT:
        case RULE_OPTION_TYPE_CONTENT_URI:
            hash = PatternMatchHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_PCRE:
            hash = PcreHash(key->option_data);
            break;
#if defined(ENABLE_RESPONSE) || defined(ENABLE_REACT)
        case RULE_OPTION_TYPE_REACT:
            hash = ReactHash(key->option_data);
#endif
            break;
#if defined(ENABLE_RESPONSE) && !defined(ENABLE_RESPONSE2)
        case RULE_OPTION_TYPE_RESPOND:
            hash = RespondHash(key->option_data);
            break;
#endif
#if defined(ENABLE_RESPONSE2) && !defined(ENABLE_RESPONSE)
        case RULE_OPTION_TYPE_RESPOND2:
            hash = Respond2Hash(key->option_data);
            break;
#endif
        case RULE_OPTION_TYPE_RPC_CHECK:
            hash = RpcCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_SESSION:
            hash = SessionHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_ACK:
            hash = TcpAckCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_FLAG:
            hash = TcpFlagCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_SEQ:
            hash = TcpSeqCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_WIN:
            hash = TcpWinCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_TTL:
            hash = TtlCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_URILEN:
            hash = UriLenCheckHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_PREPROCESSOR:
            hash = PreprocessorRuleOptionHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_DYNAMIC:
            hash = DynamicRuleHash(key->option_data);
            break;
        case RULE_OPTION_TYPE_LEAF_NODE:
            hash = 0;
            break;
    }

    return hash;
}

int detection_option_key_compare_func(const void *k1, const void *k2, size_t n)
{
    int ret = DETECTION_OPTION_NOT_EQUAL;
    const detection_option_key_t *key1 = (detection_option_key_t*)k1;
    const detection_option_key_t *key2 = (detection_option_key_t*)k2;

#ifdef KEEP_THEM_ALLOCATED
    return DETECTION_OPTION_NOT_EQUAL;
#endif

    if (!key1 || !key2)
        return DETECTION_OPTION_NOT_EQUAL;

    if (key1->option_type != key2->option_type)
        return DETECTION_OPTION_NOT_EQUAL;

    switch (key1->option_type)
    {
        /* Call compare function specific to the key type */
        case RULE_OPTION_TYPE_LEAF_NODE:
            /* Leaf node always not equal. */
            break;
        case RULE_OPTION_TYPE_ASN1:
            ret = Asn1Compare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_BYTE_TEST:
            ret = ByteTestCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_BYTE_JUMP:
            ret = ByteJumpCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_FLOW:
            ret = FlowCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_CVS:
            ret = CvsCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_DSIZE:
            ret = DSizeCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_FLOWBIT:
            ret = FlowBitsCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_FTPBOUNCE:
            break;
        case RULE_OPTION_TYPE_ICMP_CODE:
            ret = IcmpCodeCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_ID:
            ret = IcmpIdCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_SEQ:
            ret = IcmpSeqCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_TYPE:
            ret = IcmpTypeCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_IP_FRAGBITS:
            ret = IpFragBitsCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_IP_FRAG_OFFSET:
            ret = IpFragOffsetCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_IP_ID:
            ret = IpIdCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_IP_OPTION:
            ret = IpOptionCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_IP_PROTO:
            ret = IpProtoCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_IP_SAME:
            ret = IpSameCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_IP_TOS:
            ret = IpTosCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_IS_DATA_AT:
            ret = IsDataAtCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_CONTENT:
        case RULE_OPTION_TYPE_CONTENT_URI:
            ret = PatternMatchCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_PCRE:
            ret = PcreCompare(key1->option_data, key2->option_data);
            break;
#if defined(ENABLE_RESPONSE) || defined(ENABLE_REACT)
        case RULE_OPTION_TYPE_REACT:
            ret = ReactCompare(key1->option_data, key2->option_data);
            break;
#endif
#if defined(ENABLE_RESPONSE) && !defined(ENABLE_RESPONSE2)
        case RULE_OPTION_TYPE_RESPOND:
            ret = RespondCompare(key1->option_data, key2->option_data);
            break;
#endif
#if defined(ENABLE_RESPONSE2) && !defined(ENABLE_RESPONSE)
        case RULE_OPTION_TYPE_RESPOND2:
            ret = Respond2Compare(key1->option_data, key2->option_data);
            break;
#endif
        case RULE_OPTION_TYPE_RPC_CHECK:
            ret = RpcCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_SESSION:
            ret = SessionCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_ACK:
            ret = TcpAckCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_FLAG:
            ret = TcpFlagCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_SEQ:
            ret = TcpSeqCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_WIN:
            ret = TcpWinCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_TTL:
            ret = TtlCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_URILEN:
            ret = UriLenCheckCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_PREPROCESSOR:
            ret = PreprocessorRuleOptionCompare(key1->option_data, key2->option_data);
            break;
        case RULE_OPTION_TYPE_DYNAMIC:
            ret = DynamicRuleCompare(key1->option_data, key2->option_data);
            break;
    }

    return ret;
}

int detection_hash_free_func(void *option_key, void *data)
{
    detection_option_key_t *key = (detection_option_key_t*)option_key;

    switch (key->option_type)
    {
        /* Call free function specific to the key type */
        case RULE_OPTION_TYPE_ASN1:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_BYTE_TEST:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_BYTE_JUMP:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_FLOW:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_CVS:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_DSIZE:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_FLOWBIT:
            FlowBitsFree(key->option_data);
            break;
        case RULE_OPTION_TYPE_FTPBOUNCE:
            /* Data is NULL, nothing to free */
            break;
        case RULE_OPTION_TYPE_ICMP_CODE:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_ID:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_SEQ:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_ICMP_TYPE:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_FRAGBITS:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_FRAG_OFFSET:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_ID:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_OPTION:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_PROTO:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_IP_SAME:
            /* Data is NULL, nothing to free */
            break;
        case RULE_OPTION_TYPE_IP_TOS:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_IS_DATA_AT:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_CONTENT:
        case RULE_OPTION_TYPE_CONTENT_URI:
            PatternMatchFree(key->option_data);
            break;
        case RULE_OPTION_TYPE_PCRE:
            PcreFree(key->option_data);
            break;
#if defined(ENABLE_RESPONSE) || defined(ENABLE_REACT)
        case RULE_OPTION_TYPE_REACT:
            ReactFree(key->option_data);
#endif
            break;
#if defined(ENABLE_RESPONSE) && !defined(ENABLE_RESPONSE2)
        case RULE_OPTION_TYPE_RESPOND:
            free(key->option_data);
            break;
#endif
#if defined(ENABLE_RESPONSE2) && !defined(ENABLE_RESPONSE)
        case RULE_OPTION_TYPE_RESPOND2:
            free(key->option_data);
            break;
#endif
        case RULE_OPTION_TYPE_RPC_CHECK:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_SESSION:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_ACK:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_FLAG:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_SEQ:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_TCP_WIN:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_TTL:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_URILEN:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_PREPROCESSOR:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_DYNAMIC:
            free(key->option_data);
            break;
        case RULE_OPTION_TYPE_LEAF_NODE:
            break;
    }
    return 0;
}

void init_detection_hash_table()
{
    detection_option_hash_table = sfxhash_new(HASH_RULE_OPTIONS,
                                sizeof(detection_option_key_t),
                                0,      /* Data size == 0, just store the ptr */
                                0,      /* Memcap */
                                0,      /* Auto node recovery */
                                NULL,   /* Auto free function */
                                detection_hash_free_func,   /* User free function */
                                1);     /* Recycle nodes */


    if (!detection_option_hash_table)
    {
        FatalError("Failed to create rule detection option hash table");
    }

    sfxhash_set_keyops(detection_option_hash_table,
                       detection_option_hash_func,
                       detection_option_key_compare_func);
}

void delete_detection_hash_table()
{
    if (detection_option_hash_table)
    {
        sfxhash_delete(detection_option_hash_table);
        detection_option_hash_table = NULL;
    }
}

int add_detection_option(option_type_t type, void *option_data, void **existing_data)
{
    detection_option_key_t key;

    if (!detection_option_hash_table)
    {
        init_detection_hash_table();
    }

    if (!option_data)
    {
        /* No option data, no conflict to resolve. */
        return DETECTION_OPTION_EQUAL;
    }

    key.option_type = type;
    key.option_data = option_data;

    *existing_data = sfxhash_find(detection_option_hash_table, &key);
    if (*existing_data)
    {
        return DETECTION_OPTION_EQUAL;
    }

    sfxhash_add(detection_option_hash_table, &key, option_data);
    return DETECTION_OPTION_NOT_EQUAL;
}

u_int32_t detection_option_tree_hash(detection_option_tree_node_t *node)
{
    u_int32_t a,b,c;
    int i;

    if (!node)
        return 0;

    a = b = c = 0;

    for (i=0;i<node->num_children;i++)
    {
#if (defined(__ia64) || defined(__amd64) || defined(_LP64))
        {
            /* Cleanup warning because of cast from 64bit ptr to 32bit int
             * warning on 64bit OSs */
            UINT64 ptr; /* Addresses are 64bits */
            ptr = (UINT64)node->children[i]->option_data;
            a += (ptr << 32) & 0XFFFFFFFF;
            b += (ptr & 0xFFFFFFFF);
        }
#else
        a += (u_int32_t)node->children[i]->option_data;
        b += 0;
#endif
        c += detection_option_tree_hash(node->children[i]);
        mix(a,b,c);
        a += node->children[i]->num_children;
        mix(a,b,c);
#if 0
        a += (u_int32_t)node->children[i]->option_data;
        /* Recurse & hash up this guy's children */
        b += detection_option_tree_hash(node->children[i]);
        c += node->children[i]->num_children;
        mix(a,b,c);
#endif
    }

    final(a,b,c);

    return c;
}

u_int32_t detection_option_tree_hash_func(SFHASHFCN *p, unsigned char *k, int n)
{
    detection_option_key_t *key = (detection_option_key_t *)k;
    detection_option_tree_node_t *node;

    if (!key || !key->option_data)
        return 0;
    
    node = (detection_option_tree_node_t*)key->option_data;

    return detection_option_tree_hash(node);
}

int detection_option_tree_compare(detection_option_tree_node_t *r, detection_option_tree_node_t *l)
{
    int ret = DETECTION_OPTION_NOT_EQUAL;
    int i;

    if ((!r && l) || (r && !l))
        return DETECTION_OPTION_NOT_EQUAL;

    if (r->option_data != l->option_data)
        return DETECTION_OPTION_NOT_EQUAL;

    if (r->num_children != l->num_children)
        return DETECTION_OPTION_NOT_EQUAL;

    for (i=0;i<r->num_children;i++)
    {
        /* Recurse & check the children for equality */
        ret = detection_option_tree_compare(r->children[i], l->children[i]);
        if (ret != DETECTION_OPTION_EQUAL)
            return ret;
    }

    return DETECTION_OPTION_EQUAL;
}

int detection_option_tree_compare_func(const void *k1, const void *k2, size_t n)
{
    detection_option_key_t *key_r = (detection_option_key_t *)k1;
    detection_option_key_t *key_l = (detection_option_key_t *)k2;
    detection_option_tree_node_t *r;
    detection_option_tree_node_t *l;

    if (!key_r || !key_l)
        return DETECTION_OPTION_NOT_EQUAL;

    r = (detection_option_tree_node_t *)key_r->option_data;
    l = (detection_option_tree_node_t *)key_l->option_data;

    return detection_option_tree_compare(r, l);
}

int detection_option_tree_free_func(void *option_key, void *data)
{
    detection_option_tree_node_t *node = (detection_option_tree_node_t *)data;
    /* In fpcreate.c */
    free_detection_option_tree(node);
    return 0;
}

void delete_detection_tree_hash_table()
{
    if (detection_option_tree_hash_table)
    {
        sfxhash_delete(detection_option_tree_hash_table);
        detection_option_tree_hash_table = NULL;
    }
}

void init_detection_tree_hash_table()
{
    detection_option_tree_hash_table = sfxhash_new(HASH_RULE_TREE,
                                sizeof(detection_option_key_t),
                                0,      /* Data size == 0, just store the ptr */
                                0,      /* Memcap */
                                0,      /* Auto node recovery */
                                NULL,   /* Auto free function */
                                detection_option_tree_free_func,   /* User free function */
                                1);     /* Recycle nodes */


    if (!detection_option_tree_hash_table)
    {
        FatalError("Failed to create rule detection option hash table");
    }

    sfxhash_set_keyops(detection_option_tree_hash_table,
                       detection_option_tree_hash_func,
                       detection_option_tree_compare_func);
}

char *option_type_str[] =
{
    "RULE_OPTION_TYPE_LEAF_NODE",
    "RULE_OPTION_TYPE_ASN1",
    "RULE_OPTION_TYPE_BYTE_TEST",
    "RULE_OPTION_TYPE_BYTE_JUMP",
    "RULE_OPTION_TYPE_FLOW",
    "RULE_OPTION_TYPE_CVS",
    "RULE_OPTION_TYPE_DSIZE",
    "RULE_OPTION_TYPE_FLOWBIT",
    "RULE_OPTION_TYPE_FTPBOUNCE",
    "RULE_OPTION_TYPE_ICMP_CODE",
    "RULE_OPTION_TYPE_ICMP_ID",
    "RULE_OPTION_TYPE_ICMP_SEQ",
    "RULE_OPTION_TYPE_ICMP_TYPE",
    "RULE_OPTION_TYPE_IP_FRAGBITS",
    "RULE_OPTION_TYPE_IP_FRAG_OFFSET",
    "RULE_OPTION_TYPE_IP_ID",
    "RULE_OPTION_TYPE_IP_OPTION",
    "RULE_OPTION_TYPE_IP_PROTO",
    "RULE_OPTION_TYPE_IP_SAME",
    "RULE_OPTION_TYPE_IP_TOS",
    "RULE_OPTION_TYPE_IS_DATA_AT",
    "RULE_OPTION_TYPE_CONTENT",
    "RULE_OPTION_TYPE_CONTENT_URI",
    "RULE_OPTION_TYPE_PCRE",
#if defined(ENABLE_RESPONSE) || defined(ENABLE_REACT)
    "RULE_OPTION_TYPE_REACT",
#endif
#if defined(ENABLE_RESPONSE) && !defined(ENABLE_RESPONSE2)
    "RULE_OPTION_TYPE_RESPOND",
#endif
#if defined(ENABLE_RESPONSE2) && !defined(ENABLE_RESPONSE)
    "RULE_OPTION_TYPE_RESPOND2",
#endif
    "RULE_OPTION_TYPE_RPC_CHECK",
    "RULE_OPTION_TYPE_SESSION",
    "RULE_OPTION_TYPE_TCP_ACK",
    "RULE_OPTION_TYPE_TCP_FLAG",
    "RULE_OPTION_TYPE_TCP_SEQ",
    "RULE_OPTION_TYPE_TCP_WIN",
    "RULE_OPTION_TYPE_TTL",
    "RULE_OPTION_TYPE_URILEN",
    "RULE_OPTION_TYPE_PREPROCESSOR",
    "RULE_OPTION_TYPE_DYNAMIC"
};

#ifdef DEBUG_OPTION_TREE
void print_option_tree(detection_option_tree_node_t *node, int level)
{
    int i;
    unsigned int indent = 12 - (11 - level) + strlen(option_type_str[node->option_type]);
    unsigned int offset = 0;
    if (level >= 10)
        offset++;

    DEBUG_WRAP(
        DebugMessage(DEBUG_DETECT, "%d%*s%*d 0x%x\n",
           level, indent - offset, option_type_str[node->option_type],
           54 - indent, node->num_children,
           node->option_data);
        for (i=0;i<node->num_children;i++)
            print_option_tree(node->children[i], level+1);
    );
}
#endif

int add_detection_option_tree(detection_option_tree_node_t *option_tree, void **existing_data)
{
    detection_option_key_t key;

    if (!detection_option_tree_hash_table)
    {
        init_detection_tree_hash_table();
    }

    if (!option_tree)
    {
        /* No option data, no conflict to resolve. */
        return DETECTION_OPTION_EQUAL;
    }

    key.option_data = (void *)option_tree;
    key.option_type = RULE_OPTION_TYPE_LEAF_NODE;

    *existing_data = sfxhash_find(detection_option_tree_hash_table, &key);
    if (*existing_data)
    {
        return DETECTION_OPTION_EQUAL;
    }

    sfxhash_add(detection_option_tree_hash_table, &key, option_tree);
    return DETECTION_OPTION_NOT_EQUAL;
}

extern u_int8_t DecodeBuffer[DECODE_BLEN];  /* decode.c */
#define REBUILD_FLAGS \
    (PKT_REBUILT_FRAG | PKT_REBUILT_STREAM | PKT_DCE_RPKT | PKT_DCE_RPKT)

int detection_option_node_evaluate(detection_option_tree_node_t *node, detection_option_eval_data_t *eval_data)
{
    int i, result = 0;
    int rval = DETECTION_OPTION_NO_MATCH;
    const u_int8_t *start_doe_ptr = NULL, *tmp_doe_ptr, *orig_doe_ptr;
    char tmp_noalert_flag = 0;
    PatternMatchData dup_content_option_data;
    PcreData dup_pcre_option_data;
    const u_int8_t *dp = NULL;
    int dsize;
    char continue_loop = 1;
    PROFILE_VARS;

    if (!node || !eval_data || !eval_data->p || !eval_data->pomd || !eval_data->otnx)
        return 0;

    /* see if evaluated it before ... */
    if (node->last_check.is_relative == 0)
    {
        /* Only matters if not relative... */
        if ( (node->last_check.ts.tv_usec == eval_data->p->pkth->ts.tv_usec) &&
            (node->last_check.ts.tv_sec == eval_data->p->pkth->ts.tv_sec) &&
            (node->last_check.packet_number == pc.total_from_pcap) &&
            (node->last_check.pipeline_number == eval_data->p->http_pipeline_count) &&
            (node->last_check.rebuild_flag == (eval_data->p->packet_flags &
                REBUILD_FLAGS)))
        {
            /* eval'd this rule option before on this packet,
             * use the cached result. */
            if ((node->last_check.flowbit_failed == 0) && !(eval_data->p->packet_flags & PKT_IP_RULE_2ND))
            {
                return node->last_check.result;
            }
        }
    }

    NODE_PROFILE_START(node);

    node->last_check.ts.tv_sec = eval_data->p->pkth->ts.tv_sec;
    node->last_check.ts.tv_usec = eval_data->p->pkth->ts.tv_usec;
    node->last_check.packet_number = pc.total_from_pcap;
    node->last_check.pipeline_number = eval_data->p->http_pipeline_count;
    node->last_check.rebuild_flag = (eval_data->p->packet_flags & REBUILD_FLAGS);
    node->last_check.flowbit_failed = 0;

    /* Save some stuff off for repeated pattern tests */
    orig_doe_ptr = doe_ptr;

    if (node->option_type == RULE_OPTION_TYPE_CONTENT)
    {
        PatternMatchDuplicatePmd(node->option_data, &dup_content_option_data);
        if ((eval_data->p->packet_flags & PKT_ALT_DECODE) && (dup_content_option_data.rawbytes == 0))
        {
            dp = (u_int8_t *)DecodeBuffer;
            dsize = eval_data->p->alt_dsize;
        }
        else
        {
            dp = eval_data->p->data;
            dsize = eval_data->p->dsize;
        }
    }
    else if (node->option_type == RULE_OPTION_TYPE_PCRE)
    {
        PcreDuplicatePcreData(node->option_data, &dup_pcre_option_data);
    }

    /* No, haven't evaluated this one before... Check it. */
    do
    {
        switch (node->option_type)
        {
            case RULE_OPTION_TYPE_LEAF_NODE:
                /* Add the match for this otn to the queue. */
                {
                    OptTreeNode *otn = (OptTreeNode *)node->option_data;
                    PatternMatchData *pmd = (PatternMatchData *)eval_data->pmd;
                    int pattern_size = 0;

                    if (pmd)
                        pattern_size = pmd->pattern_size;
#ifdef TARGET_BASED
#ifdef PORTLISTS
                    if ((otn->sigInfo.service_ordinal != 0) &&
                        (eval_data->p->application_protocol_ordinal != 0))
                    {
                        if (eval_data->p->application_protocol_ordinal != otn->sigInfo.service_ordinal)
                        {
                            DEBUG_WRAP(DebugMessage(DEBUG_DETECT,
                                "[**] SID %d not matched because of service mismatch (%d!=%d [**]\n",
                                otn->sigInfo.id,
                                eval_data->p->application_protocol_ordinal,
                                otn->sigInfo.service_ordinal););
                            break;
                        }
                    }
#endif
#endif
                    if (fpEvalRTN(otn->proto_node, eval_data->p, 1))
                    {
#ifdef PERF_PROFILING
                        otn->matches++;
#endif
                        if (!eval_data->flowbit_noalert)
                        {   
                            fpAddMatch(eval_data->pomd, eval_data->otnx, pattern_size, otn);
                        }
                        result++;
                        rval = DETECTION_OPTION_MATCH;
                    }
                }
                break;
            case RULE_OPTION_TYPE_CONTENT:
                if (node->evaluate)
                {
                    rval = node->evaluate(&dup_content_option_data, eval_data->p);
                    if (rval == DETECTION_OPTION_MATCH)
                    {
                        if (!dup_content_option_data.exception_flag)
                        {
                            if (doe_ptr == start_doe_ptr)
                            {
                                /* result doe_ptr == starting doe_ptr, meaning
                                 * this is the same search result we just had.
                                 * and already evaluated.  We're done.
                                 */
                                rval = DETECTION_OPTION_NO_MATCH;
                            }
                            else
                            {
                                start_doe_ptr = doe_ptr - dup_content_option_data.pattern_size + dup_content_option_data.pattern_max_jump_size;
                            }
                        }
                        else
                        {
                            start_doe_ptr = NULL;
                        }
                    }
                }
                break;
            case RULE_OPTION_TYPE_PCRE:
                if (node->evaluate)
                {
                    rval = node->evaluate(&dup_pcre_option_data, eval_data->p);
                    if (rval == DETECTION_OPTION_MATCH)
                    {
                        /* Start at end of current pattern */
                        start_doe_ptr = doe_ptr;
                    }
                }
                break;
            case RULE_OPTION_TYPE_ASN1:
            case RULE_OPTION_TYPE_BYTE_TEST:
            case RULE_OPTION_TYPE_BYTE_JUMP:
            case RULE_OPTION_TYPE_FLOW:
            case RULE_OPTION_TYPE_CVS:
            case RULE_OPTION_TYPE_CONTENT_URI:
            case RULE_OPTION_TYPE_DSIZE:
            case RULE_OPTION_TYPE_FLOWBIT:
            case RULE_OPTION_TYPE_FTPBOUNCE:
            case RULE_OPTION_TYPE_ICMP_CODE:
            case RULE_OPTION_TYPE_ICMP_ID:
            case RULE_OPTION_TYPE_ICMP_SEQ:
            case RULE_OPTION_TYPE_ICMP_TYPE:
            case RULE_OPTION_TYPE_IP_FRAGBITS:
            case RULE_OPTION_TYPE_IP_FRAG_OFFSET:
            case RULE_OPTION_TYPE_IP_ID:
            case RULE_OPTION_TYPE_IP_OPTION:
            case RULE_OPTION_TYPE_IP_PROTO:
            case RULE_OPTION_TYPE_IP_SAME:
            case RULE_OPTION_TYPE_IP_TOS:
            case RULE_OPTION_TYPE_IS_DATA_AT:
#if defined(ENABLE_RESPONSE) || defined(ENABLE_REACT)
            case RULE_OPTION_TYPE_REACT:
#endif
#if defined(ENABLE_RESPONSE) && !defined(ENABLE_RESPONSE2)
            case RULE_OPTION_TYPE_RESPOND:
#endif
#if defined(ENABLE_RESPONSE2) && !defined(ENABLE_RESPONSE)
            case RULE_OPTION_TYPE_RESPOND2:
#endif
            case RULE_OPTION_TYPE_RPC_CHECK:
            case RULE_OPTION_TYPE_SESSION:
            case RULE_OPTION_TYPE_TCP_ACK:
            case RULE_OPTION_TYPE_TCP_FLAG:
            case RULE_OPTION_TYPE_TCP_SEQ:
            case RULE_OPTION_TYPE_TCP_WIN:
            case RULE_OPTION_TYPE_TTL:
            case RULE_OPTION_TYPE_URILEN:
            case RULE_OPTION_TYPE_PREPROCESSOR:
            case RULE_OPTION_TYPE_DYNAMIC:
                if (node->evaluate)
                    rval = node->evaluate(node->option_data, eval_data->p);
                break;
        }

        if (rval == DETECTION_OPTION_NO_MATCH)
        {
            node->last_check.result = result;
            NODE_PROFILE_END_NOMATCH(node);
            return result;
        }
        else if (rval == DETECTION_OPTION_FAILED_BIT)
        {
            eval_data->flowbit_failed = 1;
            /* clear the timestamp so failed flowbit gets eval'd again */
            node->last_check.flowbit_failed = 1;
            node->last_check.result = result;
            NODE_PROFILE_END_MATCH(node);
            return 0;
        }
        else if (rval == DETECTION_OPTION_NO_ALERT)
        {
            /* Cache the current flowbit_noalert flag, and set it
             * so nodes below this don't alert. */
            tmp_noalert_flag = eval_data->flowbit_noalert;
            eval_data->flowbit_noalert = 1;
        }

        tmp_doe_ptr = doe_ptr;

#ifdef PPM_MGR
        if( PPM_ENABLED() )
        {
            PPM_GET_TIME();
            /* Packet test */
            if( PPM_PKTS_ENABLED() )
            {
                PPM_PACKET_TEST();
                if( PPM_PACKET_ABORT_FLAG() )
                {
                    /* bail if we exceeded time */
                    if (result == DETECTION_OPTION_NO_MATCH)
                    {
                        NODE_PROFILE_END_NOMATCH(node);
                    }
                    else
                    {
                        NODE_PROFILE_END_MATCH(node);
                    }
                    node->last_check.result = result;
                    return result;
                }
            }
        }
#endif
        /* Don't include children's time in this node */
        NODE_PROFILE_TMPEND(node);

        /* Passed, check the children. */
        if (node->num_children)
        {
            for (i=0;i<node->num_children; i++)
            {
                doe_ptr = tmp_doe_ptr; /* reset the DOE ptr for each child from here */
                result += detection_option_node_evaluate(node->children[i], eval_data);
#ifdef PPM_MGR
                if( PPM_ENABLED() )
                {
                    PPM_GET_TIME();

                    /* Packet test */
                    if( PPM_PKTS_ENABLED() )
                    {
                        PPM_PACKET_TEST();
                        if( PPM_PACKET_ABORT_FLAG() )
                        {
                            /* bail if we exceeded time */
                            node->last_check.result = result;
                            return result;
                        }
                    }
                }
#endif
            }
        }
        NODE_PROFILE_TMPSTART(node);

        if (rval == DETECTION_OPTION_NO_ALERT)
        {
            /* Reset the flowbit_noalert flag in eval data */
            eval_data->flowbit_noalert = tmp_noalert_flag;
        }

        if ((rval == DETECTION_OPTION_MATCH) && (node->relative_children))
        {
            if (node->option_type == RULE_OPTION_TYPE_CONTENT)
            {
                if (dup_content_option_data.exception_flag)
                {
                    continue_loop = 0;
                }
                else
                {
                    continue_loop = PatternMatchAdjustRelativeOffsets(&dup_content_option_data, orig_doe_ptr, start_doe_ptr, dp);
                
                    doe_ptr = start_doe_ptr;
                    dup_content_option_data.use_doe = 1;
                }
            }
            else if (node->option_type == RULE_OPTION_TYPE_PCRE)
            {
                continue_loop = PcreAdjustRelativeOffsets(&dup_pcre_option_data, doe_ptr - orig_doe_ptr);
                doe_ptr = start_doe_ptr;
            }
            else
            {
                continue_loop = 0;
            }
        }
        else
        {
            continue_loop = 0;
        }
    } while (continue_loop);

    if (eval_data->flowbit_failed)
    {
        /* something deeper in the tree failed a flowbit test, we may need to
         * reeval this node. */
        node->last_check.flowbit_failed = 1;
    }
    node->last_check.result = result;
    if (result == DETECTION_OPTION_NO_MATCH)
    {
        NODE_PROFILE_END_NOMATCH(node);
    }
    else
    {
        NODE_PROFILE_END_MATCH(node);
    }
    return result;
}

#ifdef PERF_PROFILING
typedef struct node_profile_stats
{
    UINT64 ticks;
    UINT64 ticks_match;
    UINT64 ticks_no_match;
    UINT64 checks;
    UINT64 disables;
} node_profile_stats_t;

static void detection_option_node_update_otn_stats(detection_option_tree_node_t *node, node_profile_stats_t *stats, UINT64 disables)
{
    int i;
    node_profile_stats_t local_stats; /* cumulative stats for this node */

    if (stats)
    {
        local_stats.ticks = stats->ticks + node->ticks; 
        local_stats.ticks_match = stats->ticks_match + node->ticks_match; 
        local_stats.ticks_no_match = stats->ticks_match + node->ticks_no_match; 
        local_stats.checks = stats->checks;
        local_stats.disables = disables;
    }
    else
    {
        local_stats.ticks = node->ticks; 
        local_stats.ticks_match = node->ticks_match; 
        local_stats.ticks_no_match = node->ticks_no_match; 
        local_stats.checks = node->checks;
        local_stats.disables = disables;
    }

    if (node->option_type == RULE_OPTION_TYPE_LEAF_NODE)
    {
        OptTreeNode *otn = (OptTreeNode *)node->option_data;
        /* Update stats for this otn */
        otn->ticks += local_stats.ticks;
        otn->ticks_match += local_stats.ticks_match;
        otn->ticks_no_match += local_stats.ticks_no_match;
        otn->checks += local_stats.checks;
        otn->ppm_disable_cnt += local_stats.disables;
    }

    if (node->num_children)
    {
        for (i=0;i<node->num_children; i++)
        {
            detection_option_node_update_otn_stats(node->children[i], &local_stats, disables);
        }
    }
}

void detection_option_tree_update_otn_stats()
{
    SFXHASH_NODE *hashnode;

    /* Find the first tree root in the table */
    hashnode = sfxhash_findfirst(detection_option_tree_hash_table);
    while (hashnode)
    {
        detection_option_tree_node_t *node = hashnode->data;
        if (node->checks)
        {
            detection_option_node_update_otn_stats(node, NULL,
#ifdef PPM_MGR
                                                   node->ppm_disable_cnt
#else
                                                   0
#endif
                                                   );
        }
        hashnode = sfxhash_findnext(detection_option_tree_hash_table);
    }
}
#endif

#endif /* DETECTION_OPTION_TREE */
