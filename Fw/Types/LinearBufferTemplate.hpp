// ======================================================================
// @file   LinearBufferTemplate.hpp
// @author Devin (from nasa/fprime#5018)
// @brief  A linear buffer template parameterized by type ID and size
// ======================================================================

#ifndef FW_LINEAR_BUFFER_TEMPLATE_HPP
#define FW_LINEAR_BUFFER_TEMPLATE_HPP

#include <Fw/FPrimeBasicTypes.hpp>
#include <Fw/Types/Assert.hpp>
#include <Fw/Types/Serializable.hpp>

namespace Fw {

//! \brief A linear buffer template parameterized by type ID and buffer size
//!
//! This class template provides a fixed-size linear serialization buffer
//! derived from LinearBufferBase. It replaces the many hand-coded concrete
//! buffer classes (CmdArgBuffer, LogBuffer, TlmBuffer, etc.) that share
//! identical structure, eliminating boilerplate and reducing error potential.
//!
//! Usage:
//! \code
//!   using CmdArgBuffer = LinearBufferTemplate<FW_TYPEID_CMD_BUFF, FW_CMD_ARG_BUFFER_MAX_SIZE>;
//! \endcode
//!
//! \tparam TypeId   Serialized type identifier (from SerIds.hpp or project-specific)
//! \tparam MaxSize  Maximum buffer capacity in bytes
template <int TypeId, FwSizeType MaxSize>
class LinearBufferTemplate final : public LinearBufferBase {
  public:
    enum {
        SERIALIZED_TYPE_ID = TypeId,                        //!< type id for serialization
        SERIALIZED_SIZE = STATIC_SERIALIZED_SIZE(MaxSize),  //!< size when serialized: buffer + stored size
    };

    LinearBufferTemplate() = default;

    LinearBufferTemplate(const U8* args, FwSizeType size) {
        const SerializeStatus stat = this->setBuff(args, size);
        FW_ASSERT(FW_SERIALIZE_OK == stat, static_cast<FwAssertArgType>(stat));
    }

    LinearBufferTemplate(const LinearBufferTemplate& other) : LinearBufferBase() {
        const SerializeStatus stat = this->setBuff(other.getBuffAddr(), other.getSize());
        FW_ASSERT(FW_SERIALIZE_OK == stat, static_cast<FwAssertArgType>(stat));
    }

    ~LinearBufferTemplate() override = default;

    LinearBufferTemplate& operator=(const LinearBufferTemplate& other) {
        if (this == &other) {
            return *this;
        }
        const SerializeStatus stat = this->setBuff(other.getBuffAddr(), other.getSize());
        FW_ASSERT(FW_SERIALIZE_OK == stat, static_cast<FwAssertArgType>(stat));
        return *this;
    }

    DEPRECATED(FwSizeType getBuffCapacity() const, "Use getCapacity() instead") { return this->getCapacity(); }

    FwSizeType getCapacity() const override { return sizeof this->m_bufferData; }

    U8* getBuffAddr() override { return this->m_bufferData; }

    const U8* getBuffAddr() const override { return this->m_bufferData; }

  private:
    U8 m_bufferData[MaxSize];
};

}  // namespace Fw

#endif
