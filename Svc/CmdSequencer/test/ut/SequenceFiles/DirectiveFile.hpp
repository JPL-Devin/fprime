// ======================================================================
// \title  DirectiveFile.hpp
// \author Canham
// \brief  A sequence file built record by record, for directive tests
//
// \copyright
// Copyright (C) 2009-2018 California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
// ======================================================================

#ifndef Svc_SequenceFiles_DirectiveFile_HPP
#define Svc_SequenceFiles_DirectiveFile_HPP

#include "Svc/CmdSequencer/CmdSequencerImpl.hpp"
#include "Svc/CmdSequencer/test/ut/SequenceFiles/Buffers.hpp"
#include "Svc/CmdSequencer/test/ut/SequenceFiles/File.hpp"

namespace Svc {

namespace SequenceFiles {

//! A sequence file assembled from an explicit list of records
//!
//! The other file classes in this directory each encode one fixed shape. Directive tests
//! need many one-off shapes — a label here, a jump there, a deliberately malformed payload —
//! so this class exposes the record list directly. Each builder method appends one record
//! and returns *this, so a test reads like the sequence it describes:
//!
//!     DirectiveFile file("jcf_jumps");
//!     file.command(0, 1).jcf("HANDLER").exitSeq(0).label("HANDLER").command(1, 2).endOfSequence();
//!
//! Only the F Prime format is supported: the AMPCS format has no directive records, and
//! serializeAMPCS is left to the base class, which fails the test if it is reached.
class DirectiveFile : public File {
  public:
    // ----------------------------------------------------------------------
    // Types and constants
    // ----------------------------------------------------------------------

    typedef CmdSequencerComponentImpl::Sequence::Record::DirectiveId DirectiveId;

    enum Constants {
        //! The size of a jump directive's fixed fields: the directive ID and the label length
        JUMP_PREFIX_SIZE = 2,
        //! The largest directive payload a builder method will assemble. A well-formed jump
        //! directive is JUMP_PREFIX_SIZE + MAX_LABEL_SIZE; the slack above that is for the
        //! malformed builders, which deliberately write a longer label than the format allows.
        MAX_PAYLOAD_SIZE = JUMP_PREFIX_SIZE + CmdSequencerComponentImpl::Sequence::Record::MAX_LABEL_SIZE + 16
    };

  public:
    // ----------------------------------------------------------------------
    // Constructors
    // ----------------------------------------------------------------------

    //! Construct an empty DirectiveFile. Records are added by the builder methods.
    explicit DirectiveFile(const char* const baseName  //!< The base name, without path or extension
    );

  public:
    // ----------------------------------------------------------------------
    // Builder methods for well-formed records
    // ----------------------------------------------------------------------

    //! Append an immediate command record, as CommandBuffers::create encodes it
    DirectiveFile& command(const FwOpcodeType opcode,  //!< The opcode
                           const U32 argument          //!< The single U32 argument
    );

    //! Append a LABEL directive
    DirectiveFile& label(const char* const labelName  //!< The label name
    );

    //! Append a JCF (jump on command failure) directive
    DirectiveFile& jcf(const char* const labelName  //!< The target label name
    );

    //! Append a JCS (jump on command success) directive
    DirectiveFile& jcs(const char* const labelName  //!< The target label name
    );

    //! Append an EXIT directive
    DirectiveFile& exitSeq(const U8 status  //!< 0 = OK, 1 = EXECUTION_ERROR
    );

    //! Append an ERROR_MODE directive
    DirectiveFile& errorMode(const U8 mode  //!< 0 = OFF, 1 = ON
    );

    //! Append an end-of-sequence record
    DirectiveFile& endOfSequence();

  public:
    // ----------------------------------------------------------------------
    // Builder methods for deliberately malformed records
    // ----------------------------------------------------------------------

    //! Append a directive record whose payload is written verbatim.
    //! Use this to build a payload no well-formed builder would produce.
    DirectiveFile& rawDirective(const U8* const payload,  //!< The payload bytes
                                const U32 payloadSize     //!< The payload size
    );

    //! Append a directive record carrying only a directive ID, with no arguments
    DirectiveFile& directiveIdOnly(const U8 directiveId  //!< The directive ID byte, valid or not
    );

    //! Append a directive record with an empty payload, so that not even the directive ID
    //! can be read
    DirectiveFile& emptyDirective();

    //! Append a jump directive whose declared label length does not match the characters
    //! that follow it
    DirectiveFile& jumpWithLabelLength(const DirectiveId::T directive,  //!< JCF or JCS
                                       const U8 declaredLength,         //!< The length byte to write
                                       const char* const chars          //!< The characters to write after it
    );

  public:
    // ----------------------------------------------------------------------
    // Accessors
    // ----------------------------------------------------------------------

    //! Get the number of records appended so far
    U32 getNumRecords() const;

  public:
    // ----------------------------------------------------------------------
    // File interface
    // ----------------------------------------------------------------------

    //! Serialize the file in F Prime format
    void serializeFPrime(Fw::LinearBufferBase& buffer  //!< The buffer
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Append a directive carrying a directive ID and a label
    DirectiveFile& labelDirective(const DirectiveId::T directive,  //!< The directive
                                  const char* const labelName  //!< The label name
    );

    //! Append a directive carrying a directive ID and one U8 argument
    DirectiveFile& u8Directive(const DirectiveId::T directive,  //!< The directive
                               const U8 argument                //!< The argument
    );

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! The serialized records, assembled as the builder methods are called. Holding the
    //! bytes rather than a description of them keeps serializeFPrime idempotent, so a
    //! test may write the same file more than once.
    Buffers::FileBuffer m_records;

    //! The number of records appended
    U32 m_numRecords;
};

}  // namespace SequenceFiles

}  // namespace Svc

#endif
