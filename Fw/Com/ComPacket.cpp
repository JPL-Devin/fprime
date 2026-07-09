/*
 * ComPacket.cpp
 *
 *  Created on: May 24, 2014
 *      Author: Timothy Canham
 */

#include <Fw/Com/ComPacket.hpp>

namespace Fw {

ComPacket::ComPacket() : m_type(ComPacketType::FW_PACKET_UNKNOWN) {}

ComPacket::~ComPacket() {}

ComPacketType ComPacket::getPacketType() const {
    return this->m_type;
}

} /* namespace Fw */
