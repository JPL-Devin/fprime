/*
 * ComPacket.hpp
 *
 *  Created on: May 24, 2014
 *      Author: Timothy Canham
 */

#ifndef COMPACKET_HPP_
#define COMPACKET_HPP_

#include <Fw/Types/Serializable.hpp>
#include "config/ApidEnumAc.hpp"

// Packet format:
// | packet type-specific data |
// The packet type (APID) is not serialized into the packet data; it is carried
// alongside the packet through port interfaces and protocol headers

namespace Fw {

// This type is defined in config/ComCfg.fpp
using ComPacketType = ComCfg::Apid::T;

class ComPacket : public Serializable {
  public:
    ComPacket();
    virtual ~ComPacket();

    //! Get the packet type (APID)
    ComPacketType getPacketType() const;

  protected:
    ComPacketType m_type;
};

} /* namespace Fw */

#endif /* COMPACKET_HPP_ */
