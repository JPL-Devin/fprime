// ======================================================================
// \title  DirectiveFile.cpp
// \author Canham
// \brief  DirectiveFile implementation
//
// \copyright
// Copyright (C) 2009-2018 California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
// ======================================================================

#include "Svc/CmdSequencer/test/ut/SequenceFiles/DirectiveFile.hpp"
#include <string.h>
#include "Svc/CmdSequencer/test/ut/SequenceFiles/FPrime/FPrime.hpp"
#include "gtest/gtest.h"

namespace Svc {

namespace SequenceFiles {

namespace {

//! The record descriptor and time base used for command records in these files
typedef CmdSequencerComponentImpl::Sequence::Record Record;

}  // namespace

// ----------------------------------------------------------------------
// Constructors
// ----------------------------------------------------------------------

DirectiveFile ::DirectiveFile(const char* const baseName) : File(Format::F_PRIME), m_numRecords(0) {
    this->setName(baseName);
    // A test runs its file by passing the name through CS_RUN, whose argument is a
    // Fw::CmdStringArg. A name that does not fit is truncated, and the component then reports
    // a file that does not exist, which is a confusing way to learn the name is too long.
    EXPECT_LT(this->getName().length(), Fw::CmdStringArg::STRING_SIZE)
        << "Sequence file name " << this->getName() << " does not fit in a command argument";
}

// ----------------------------------------------------------------------
// Builder methods for well-formed records
// ----------------------------------------------------------------------

DirectiveFile& DirectiveFile ::command(const FwOpcodeType opcode, const U32 argument) {
    // Zero relative time makes the command immediate, so a test does not have to advance
    // the clock to get it dispatched. This pairs with the zero test time set by
    // CmdSequencerTester::prepare in Directives.cpp; changing either one alone leaves
    // every command sitting in the timer.
    const Fw::Time t(TimeBase::TB_WORKSTATION_TIME, 0, 0);
    FPrime::Records::serialize(Record::RELATIVE, t, opcode, argument, this->m_records);
    ++this->m_numRecords;
    return *this;
}

DirectiveFile& DirectiveFile ::label(const char* const labelName) {
    return this->labelDirective(DirectiveId::LABEL, labelName);
}

DirectiveFile& DirectiveFile ::jcf(const char* const labelName) {
    return this->labelDirective(DirectiveId::JCF, labelName);
}

DirectiveFile& DirectiveFile ::jcs(const char* const labelName) {
    return this->labelDirective(DirectiveId::JCS, labelName);
}

DirectiveFile& DirectiveFile ::exitSeq(const U8 status) {
    return this->u8Directive(DirectiveId::EXIT, status);
}

DirectiveFile& DirectiveFile ::errorMode(const U8 mode) {
    return this->u8Directive(DirectiveId::ERROR_MODE, mode);
}

DirectiveFile& DirectiveFile ::endOfSequence() {
    const Fw::Time t(TimeBase::TB_WORKSTATION_TIME, 0, 0);
    FPrime::Records::serialize(Record::END_OF_SEQUENCE, t, this->m_records);
    ++this->m_numRecords;
    return *this;
}

// ----------------------------------------------------------------------
// Builder methods for deliberately malformed records
// ----------------------------------------------------------------------

DirectiveFile& DirectiveFile ::rawDirective(const U8* const payload, const U32 payloadSize) {
    FPrime::Records::serializeDirective(payload, payloadSize, this->m_records);
    ++this->m_numRecords;
    return *this;
}

DirectiveFile& DirectiveFile ::directiveIdOnly(const U8 directiveId) {
    return this->rawDirective(&directiveId, sizeof(directiveId));
}

DirectiveFile& DirectiveFile ::emptyDirective() {
    // The pointer is unused at size zero, but pass a valid one rather than null
    const U8 unused = 0;
    return this->rawDirective(&unused, 0);
}

DirectiveFile& DirectiveFile ::jumpWithLabelLength(const DirectiveId::T directive,
                                                   const U8 declaredLength,
                                                   const char* const chars) {
    const size_t charCount = strlen(chars);
    U8 payload[MAX_PAYLOAD_SIZE] = {};
    // The payload is the directive ID, the declared length, and the characters. The count
    // written is the real length of chars, which the caller chooses independently of
    // declaredLength; that mismatch is the point of this builder.
    if (charCount + JUMP_PREFIX_SIZE > sizeof(payload)) {
        ADD_FAILURE() << "label of " << charCount << " characters does not fit in a directive payload";
        return *this;
    }
    payload[0] = static_cast<U8>(directive);
    payload[1] = declaredLength;
    (void)memcpy(&payload[JUMP_PREFIX_SIZE], chars, charCount);
    return this->rawDirective(payload, static_cast<U32>(charCount + JUMP_PREFIX_SIZE));
}

// ----------------------------------------------------------------------
// Accessors
// ----------------------------------------------------------------------

U32 DirectiveFile ::getNumRecords() const {
    return this->m_numRecords;
}

// ----------------------------------------------------------------------
// File interface
// ----------------------------------------------------------------------

void DirectiveFile ::serializeFPrime(Fw::LinearBufferBase& buffer) {
    // The header declares the size of everything that follows it, including the CRC
    const U32 recordDataSize = static_cast<U32>(this->m_records.getSize());
    const U32 dataSize = recordDataSize + FPrime::CRCs::SIZE;
    const TimeBase timeBase = TimeBase::TB_WORKSTATION_TIME;
    const U32 timeContext = 0;
    FPrime::Headers::serialize(dataSize, this->m_numRecords, timeBase, timeContext, buffer);
    // Records, already serialized by the builder methods
    ASSERT_EQ(Fw::FW_SERIALIZE_OK,
              buffer.serializeFrom(this->m_records.getBuffAddr(), recordDataSize, Fw::Serialization::OMIT_LENGTH));
    // CRC over the header and records
    FPrime::CRCs::serialize(buffer);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

DirectiveFile& DirectiveFile ::labelDirective(const DirectiveId::T directive, const char* const labelName) {
    const size_t nameLength = strlen(labelName);
    // A label longer than MAX_LABEL_SIZE cannot be expressed by a well-formed record. Use
    // jumpWithLabelLength to build one deliberately.
    if (nameLength > static_cast<size_t>(Record::MAX_LABEL_SIZE)) {
        ADD_FAILURE() << "label \"" << labelName << "\" exceeds MAX_LABEL_SIZE";
        return *this;
    }
    U8 payload[MAX_PAYLOAD_SIZE] = {};
    payload[0] = static_cast<U8>(directive);
    payload[1] = static_cast<U8>(nameLength);
    (void)memcpy(&payload[JUMP_PREFIX_SIZE], labelName, nameLength);
    return this->rawDirective(payload, static_cast<U32>(nameLength + JUMP_PREFIX_SIZE));
}

DirectiveFile& DirectiveFile ::u8Directive(const DirectiveId::T directive, const U8 argument) {
    const U8 payload[] = {static_cast<U8>(directive), argument};
    return this->rawDirective(payload, sizeof(payload));
}

}  // namespace SequenceFiles

}  // namespace Svc
