/*
 * libtins is a net packet wrapper library for crafting and
 * interpreting sniffed packets.
 *
 * Copyright (C) 2011 Nasel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdexcept>
#include <cstring>
#include <cassert>
#include <algorithm>
#ifndef WIN32
    #include <netdb.h>
    #include <sys/socket.h>
#endif
#include "ip.h"
#include "tcp.h"
#include "udp.h"
#include "icmp.h"
#include "rawpdu.h"
#include "utils.h"
#include "constants.h"


using namespace std;

namespace Tins {

const uint8_t IP::DEFAULT_TTL = 128;

IP::IP(address_type ip_dst, address_type ip_src, PDU *child) 
: PDU(Constants::IP::PROTO_IP, child) 
{
    init_ip_fields();
    this->dst_addr(ip_dst);
    this->src_addr(ip_src); 
}

IP::IP(const uint8_t *buffer, uint32_t total_sz) 
: PDU(Constants::IP::PROTO_IP)
{
    static const char *msg("Not enough size for an IP header in the buffer.");
    if(total_sz < sizeof(iphdr))
        throw std::runtime_error(msg);
    std::memcpy(&_ip, buffer, sizeof(iphdr));

    /* Options... */
    /* Establish beginning and ending of the options */
    const uint8_t* ptr_buffer = buffer + sizeof(iphdr);
    if(total_sz < head_len() * sizeof(uint32_t))
        throw std::runtime_error(msg);
    if(head_len() * sizeof(uint32_t) < sizeof(iphdr))
        throw std::runtime_error("Malformed head len field");
    buffer += head_len() * sizeof(uint32_t);
    
    _options_size = 0;
    _padded_options_size = head_len() * sizeof(uint32_t) - sizeof(iphdr);
    /* While the end of the options is not reached read an option */
    while (ptr_buffer < buffer && (*ptr_buffer != 0)) {
        IPOption opt_to_add;
        memcpy(&opt_to_add.type, ptr_buffer, sizeof(uint8_t));
        ptr_buffer++;
        switch (opt_to_add.type.number) {
            /* Multibyte options with length as second byte */
            case SEC:
            case LSSR:
            case TIMESTAMP:
            case EXTSEC:
            case RR:
            case SID:
            case SSRR:
            case MTUPROBE:
            case MTUREPLY:
            case EIP:
            case TR:
            case ADDEXT:
            case RTRALT:
            case SDB:
            case DPS:
            case UMP:
            case QS:
                if(ptr_buffer == buffer || *ptr_buffer == 0)
                    throw std::runtime_error(msg);
                    
                {
                    const uint8_t data_size = *ptr_buffer - 1;
                    if(data_size > 0) {
                        if(buffer - ptr_buffer < data_size)
                            throw std::runtime_error(msg);
                        opt_to_add.optional_data.assign(ptr_buffer, ptr_buffer + data_size);
                    }
                }
                
                ptr_buffer += opt_to_add.optional_data.size();
        }
        this->_ip_options.push_back(opt_to_add);
        this->_options_size += opt_to_add.optional_data.size() + 1;
    }
    // check this line PLX
    total_sz -= head_len() * sizeof(uint32_t);
    if (total_sz) {
        switch(_ip.protocol) {
            case IPPROTO_TCP:
                inner_pdu(new Tins::TCP(buffer, total_sz));
                break;
            case IPPROTO_UDP:
                inner_pdu(new Tins::UDP(buffer, total_sz));
                break;
            case IPPROTO_ICMP:
                inner_pdu(new Tins::ICMP(buffer, total_sz));
                break;
            default:
                inner_pdu(new Tins::RawPDU(buffer, total_sz));
                break;
        }
    }
}

void IP::init_ip_fields() {
    memset(&_ip, 0, sizeof(iphdr));
    this->_ip.version = 4;
    this->ttl(DEFAULT_TTL);
    this->id(1);
    this->_options_size = 0;
    this->_padded_options_size = 0;
}

/* Setters */

void IP::tos(uint8_t new_tos) {
    _ip.tos = new_tos;
}

void IP::tot_len(uint16_t new_tot_len) {
    _ip.tot_len = Endian::host_to_be(new_tot_len);
}

void IP::id(uint16_t new_id) {
    _ip.id = Endian::host_to_be(new_id);
}

void IP::frag_off(uint16_t new_frag_off) {
    _ip.frag_off = Endian::host_to_be(new_frag_off);
}

void IP::ttl(uint8_t new_ttl) {
    _ip.ttl = new_ttl;
}

void IP::protocol(uint8_t new_protocol) {
    _ip.protocol = new_protocol;
}

void IP::check(uint16_t new_check) {
    _ip.check = Endian::host_to_be(new_check);
}


void IP::src_addr(address_type ip) {
    _ip.saddr = ip;
}


void IP::dst_addr(address_type ip) {
    _ip.daddr = ip;
}

void IP::head_len(small_uint<4> new_head_len) {
    _ip.ihl = new_head_len;
}

void IP::version(small_uint<4> ver) {
    _ip.version = ver;
}

void IP::set_eol_option() {
    this->set_option(0, IP::CONTROL, IP::END);
}

void IP::set_noop_option() {
    this->set_option(0, IP::CONTROL, IP::NOOP);
}

void IP::set_sec_option(const uint8_t* data, uint32_t data_len) {
    this->set_option(1, IP::CONTROL, IP::SEC, data, data_len);
}

void IP::set_option(uint8_t copied,
                OptionClass op_class,
                Option number,
                const uint8_t* data,
                uint32_t data_size) 
{
    IPOption option;
    option.type.copied = copied;
    option.type.op_class = op_class;
    option.type.number = number;
    if (data_size) {
        option.optional_data.push_back(data_size);
        std::copy(data, data + data_size, 
                  std::back_inserter(option.optional_data)
        );
        data_size++;
    }
    _ip_options.push_back(option);
    _options_size += 1 + (!option.optional_data.empty() ? (data_size) : 0);
    uint8_t padding = _options_size & 3;
    _padded_options_size = padding ? (_options_size - padding + 4) : _options_size;
}

const IP::IPOption *IP::search_option(OptionClass opt_class, Option opt_number) const {
    for(std::list<IPOption>::const_iterator it = _ip_options.begin(); it != _ip_options.end(); ++it) {
        if(it->type.op_class == (uint8_t)opt_class && it->type.number == (uint8_t)opt_number)
            return &(*it);
    }
    return 0;
}

uint8_t* IP::IPOption::write(uint8_t* buffer) {
    memcpy(buffer, &type, 1);
    buffer += 1;
    if (!optional_data.empty()) {
        std::copy(optional_data.begin(), optional_data.end(), buffer);
        buffer += optional_data.size();
    }
    return buffer;
}

const uint8_t* IP::IPOption::data_ptr() const {
    return !optional_data.empty() ? (&optional_data[1]) : 0;
}

uint8_t IP::IPOption::data_size() const {
    return !optional_data.empty() ? (optional_data.size() - 1) : 0;
}

/* Virtual method overriding. */

uint32_t IP::header_size() const {
    return sizeof(iphdr) + _padded_options_size;
}

bool IP::send(PacketSender* sender) {
    struct sockaddr_in link_addr;
    PacketSender::SocketType type = PacketSender::IP_SOCKET;
    link_addr.sin_family = AF_INET;
    link_addr.sin_port = 0;
    link_addr.sin_addr.s_addr = _ip.daddr;
    if(inner_pdu() && inner_pdu()->flag() == IPPROTO_ICMP)
        type = PacketSender::ICMP_SOCKET;

    return sender->send_l3(this, (struct sockaddr*)&link_addr, sizeof(link_addr), type);
}

PDU *IP::recv_response(PacketSender *sender) {
    struct sockaddr_in link_addr;
    PacketSender::SocketType type = PacketSender::IP_SOCKET;
    link_addr.sin_family = AF_INET;
    link_addr.sin_port = 0;
    link_addr.sin_addr.s_addr = _ip.daddr;
    if(inner_pdu() && inner_pdu()->flag() == IPPROTO_ICMP)
        type = PacketSender::ICMP_SOCKET;

    return sender->recv_l3(this, (struct sockaddr*)&link_addr, sizeof(link_addr), type);
}

void IP::write_serialization(uint8_t *buffer, uint32_t total_sz, const PDU* parent) {
    uint32_t my_sz = header_size();
    assert(total_sz >= my_sz);
    if(inner_pdu()) {
        uint32_t new_flag;
        new_flag = inner_pdu()->flag();
        if(new_flag == IPPROTO_IP)
            new_flag = IPPROTO_IPIP;

        this->protocol(new_flag);
        this->flag(new_flag);
    }
    this->tot_len(total_sz);
    this->head_len(my_sz / sizeof(uint32_t));

    memcpy(buffer, &_ip, sizeof(_ip));

    uint8_t* ptr_buffer = buffer + sizeof(_ip);
    for(list<IPOption>::iterator it = _ip_options.begin(); it != _ip_options.end(); ++it)
        ptr_buffer = it->write(ptr_buffer);

    memset(buffer + sizeof(_ip) + _options_size, 0, _padded_options_size - _options_size);

    if(parent && !_ip.check) {
        uint32_t checksum = Utils::do_checksum(buffer, buffer + sizeof(_ip) + _padded_options_size);
        while (checksum >> 16)
            checksum = (checksum & 0xffff) + (checksum >> 16);
        ((iphdr*)buffer)->check = Endian::host_to_be<uint16_t>(~checksum);
        this->check(0);
    }
}

bool IP::matches_response(uint8_t *ptr, uint32_t total_sz) {
    if(total_sz < sizeof(iphdr))
        return false;
    iphdr *ip_ptr = (iphdr*)ptr;
    if(_ip.daddr == ip_ptr->saddr && _ip.saddr == ip_ptr->daddr) {
        uint32_t sz = _ip.ihl * sizeof(uint32_t);
        return inner_pdu() ? inner_pdu()->matches_response(ptr + sz, total_sz - sz) : true;
    }
    return false;
}

PDU *IP::clone_packet(const uint8_t *ptr, uint32_t total_sz) {
    if(total_sz < sizeof(iphdr))
        return 0;
    const iphdr *ip_ptr = (iphdr*)ptr;
    uint32_t sz = ip_ptr->ihl * sizeof(uint32_t);
    if(total_sz < sz)
        return 0;
    PDU *child = 0, *cloned;
    if(total_sz > sz) {
        if((child = PDU::clone_inner_pdu(ptr + sizeof(_ip), total_sz - sizeof(_ip))) == 0)
            return 0;
    }
    cloned = new IP(ptr, std::min(total_sz, (uint32_t)(Endian::be_to_host(ip_ptr->tot_len) * sizeof(uint32_t))));
    cloned->inner_pdu(child);
    return cloned;
}
}
