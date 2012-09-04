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

#include <cstring>
#include <cassert>
#include <stdexcept>
#include "eapol.h"
#include "dot11.h"
#include "rsn_information.h"


namespace Tins {
EAPOL::EAPOL(uint8_t packet_type, EAPOLTYPE type) : PDU(0xff) {
    std::memset(&_header, 0, sizeof(_header));
    _header.version = 1;
    _header.packet_type = packet_type;
    _header.type = (uint8_t)type;
}

EAPOL::EAPOL(const uint8_t *buffer, uint32_t total_sz) : PDU(0xff) {
    if(total_sz < sizeof(_header))
        throw std::runtime_error("Not enough size for an EAPOL header in the buffer.");
    std::memcpy(&_header, buffer, sizeof(_header));
}

EAPOL::EAPOL(const EAPOL &other) : PDU(other) {
    copy_eapol_fields(&other);
}

EAPOL *EAPOL::from_bytes(const uint8_t *buffer, uint32_t total_sz) {
    if(total_sz < sizeof(eapolhdr))
        throw std::runtime_error("Not enough size for an EAPOL header in the buffer.");
    const eapolhdr *ptr = (const eapolhdr*)buffer;
    switch(ptr->type) {
        case RC4:
            return new Tins::RC4EAPOL(buffer, total_sz);
            break;
        case RSN:
        case EAPOL_WPA:
            return new Tins::RSNEAPOL(buffer, total_sz);
            break;
    }
    return 0;
}

void EAPOL::version(uint8_t new_version) {
    _header.version = new_version;
}
        
void EAPOL::packet_type(uint8_t new_ptype) {
    _header.packet_type = new_ptype;
}

void EAPOL::length(uint16_t new_length) {
    _header.length = new_length;
}

void EAPOL::type(uint8_t new_type) {
    _header.type = new_type;
}

void EAPOL::write_serialization(uint8_t *buffer, uint32_t total_sz, const PDU *) {
    uint32_t sz = header_size();
    assert(total_sz >= sz);
    if(!_header.length)
        length(sz - sizeof(_header.version) - sizeof(_header.length) - sizeof(_header.type));
    std::memcpy(buffer, &_header, sizeof(_header));
    write_body(buffer + sizeof(_header), total_sz - sizeof(_header));
}

void EAPOL::copy_eapol_fields(const EAPOL *other) {
    std::memcpy(&_header, &other->_header, sizeof(_header));
    
}

/* RC4EAPOL */

RC4EAPOL::RC4EAPOL() 
: EAPOL(0x03, RC4) 
{
    std::memset(&_header, 0, sizeof(_header));
}

RC4EAPOL::RC4EAPOL(const uint8_t *buffer, uint32_t total_sz) 
: EAPOL(buffer, total_sz)
{
    buffer += sizeof(eapolhdr);
    total_sz -= sizeof(eapolhdr);
    if(total_sz < sizeof(_header))
        throw std::runtime_error("Not enough size for an EAPOL header in the buffer.");
    std::memcpy(&_header, buffer, sizeof(_header));
    buffer += sizeof(_header);
    total_sz -= sizeof(_header);
    if(total_sz == key_length())
        _key.assign(buffer, buffer + total_sz);
}

void RC4EAPOL::key_length(uint16_t new_key_length) {
    _header.key_length = Endian::host_to_be(new_key_length);
}
        
void RC4EAPOL::replay_counter(uint16_t new_replay_counter) {
    _header.replay_counter = Endian::host_to_be(new_replay_counter);
}

void RC4EAPOL::key_iv(const uint8_t *new_key_iv) {
    std::memcpy(_header.key_iv, new_key_iv, sizeof(_header.key_iv));
}

void RC4EAPOL::key_flag(small_uint<1> new_key_flag) {
    _header.key_flag = new_key_flag;
}

void RC4EAPOL::key_index(small_uint<7> new_key_index) {
    _header.key_index = new_key_index;
}

void RC4EAPOL::key_sign(const uint8_t *new_key_sign) {
    std::memcpy(_header.key_sign, new_key_sign, sizeof(_header.key_sign));
}

void RC4EAPOL::key(const key_type &new_key) {
    _key = new_key;
}

uint32_t RC4EAPOL::header_size() const {
    return sizeof(eapolhdr) + sizeof(_header) + _key.size();
}

void RC4EAPOL::write_body(uint8_t *buffer, uint32_t total_sz) {
    uint32_t sz = sizeof(_header) + _key.size();
    assert(total_sz >= sz);
    if(_key.size())
        _header.key_length = Endian::host_to_be(_key.size());
    std::memcpy(buffer, &_header, sizeof(_header));
    buffer += sizeof(_header);
    std::copy(_key.begin(), _key.end(), buffer);
}

/* RSNEAPOL */


RSNEAPOL::RSNEAPOL() 
: EAPOL(0x03, RSN) 
{
    std::memset(&_header, 0, sizeof(_header));
}

RSNEAPOL::RSNEAPOL(const uint8_t *buffer, uint32_t total_sz) 
: EAPOL(0x03, RSN)
{
    buffer += sizeof(eapolhdr);
    total_sz -= sizeof(eapolhdr);
    if(total_sz < sizeof(_header))
        throw std::runtime_error("Not enough size for an EAPOL header in the buffer.");
    std::memcpy(&_header, buffer, sizeof(_header));
    buffer += sizeof(_header);
    total_sz -= sizeof(_header);
    if(total_sz == wpa_length())
        _key.assign(buffer, buffer + total_sz);
}

void RSNEAPOL::RSNEAPOL::nonce(const uint8_t *new_nonce) {
    std::memcpy(_header.nonce, new_nonce, sizeof(_header.nonce));
}

void RSNEAPOL::rsc(uint64_t new_rsc) {
    _header.rsc = Endian::host_to_be(new_rsc);
}

void RSNEAPOL::id(uint64_t new_id) {
    _header.id = Endian::host_to_be(new_id);
}

void RSNEAPOL::mic(const uint8_t *new_mic) {
    std::memcpy(_header.mic, new_mic, sizeof(_header.mic));
}

void RSNEAPOL::wpa_length(uint16_t new_wpa_length) {
    _header.wpa_length = Endian::host_to_be(new_wpa_length);
}

void RSNEAPOL::key(const key_type &new_key) {
    _key = new_key;
    _header.key_t = 0;
}

void RSNEAPOL::rsn_information(const RSNInformation &rsn) {
    _key = rsn.serialize();
    _header.key_t = 1;
}

uint32_t RSNEAPOL::header_size() const {
    uint32_t padding(0);
    if(_header.key_t && _key.size())
        padding = 2;
    return sizeof(eapolhdr) + sizeof(_header) + _key.size() + padding;
}

void RSNEAPOL::write_body(uint8_t *buffer, uint32_t total_sz) {
    uint32_t sz = header_size() - sizeof(eapolhdr);
    assert(total_sz >= sz);
    if(_key.size()) {
        if(!_header.key_t) {
            _header.key_length = Endian::host_to_be<uint16_t>(32);
            wpa_length(_key.size());
        }
        else if(_key.size()) {
            _header.key_length = 0;
            wpa_length(_key.size() + 2);
        }
        else
            wpa_length(0);
    }
    std::memcpy(buffer, &_header, sizeof(_header));
    buffer += sizeof(_header);
    if(_header.key_t && _key.size()) {
        *(buffer++) = Dot11::RSN;
        *(buffer++) = _key.size();
    }
    std::copy(_key.begin(), _key.end(), buffer);
}
}
