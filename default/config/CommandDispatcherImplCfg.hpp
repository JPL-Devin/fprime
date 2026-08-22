/*
 * CmdDispatcherImplCfg.hpp
 *
 *  Created on: May 6, 2015
 *      Author: tcanham
 */

#ifndef CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_
#define CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

// Define configuration values for dispatcher

enum {
    CMD_DISPATCHER_DISPATCH_TABLE_SIZE = 150,  // !< The size of the table holding opcodes to dispatch
    CMD_DISPATCHER_SEQUENCER_TABLE_SIZE = 25,  // !< The size of the table holding commands in progress
};

namespace Svc {
namespace CmdDispatcherCfg {

//! Include command opcodes in events when true.
//! When false, opcode fields are set to the maximum FwOpcodeType value.
constexpr bool IncludeCommandOpcodesInEvents = true;

//! Mask (obfuscate) command opcodes in events when true.
//! Only applies when IncludeCommandOpcodesInEvents is true. Opcodes are passed
//! through a keyed small-block (Feistel) permutation before being placed in
//! event arguments, hiding the raw opcode values on the downlink. The ground
//! system recovers the original opcode by applying the inverse permutation
//! (see unmaskOpcode) with the same keys.
constexpr bool MaskCommandOpcodesInEvents = false;

//! Number of Feistel rounds used by the opcode masking permutation
constexpr U32 OpcodeMaskRounds = 4;

//! Round keys for the opcode masking permutation.
//! Projects enabling MaskCommandOpcodesInEvents must replace these with
//! mission-specific random constants and configure the same keys in the
//! ground system for unmasking. The keys are not cryptographically protected
//! in the binary; this feature is obfuscation, not encryption.
constexpr U64 OpcodeMaskKeys[OpcodeMaskRounds] = {
    0x243F6A8885A308D3,
    0x13198A2E03707344,
    0xA4093822299F31D0,
    0x082EFA98EC4E6C89,
};

//! Number of bits in one Feistel half of an opcode
constexpr U32 OpcodeMaskHalfBits = sizeof(FwOpcodeType) * 4;

//! Bit mask selecting one Feistel half of an opcode
constexpr FwOpcodeType OpcodeMaskHalfMask = static_cast<FwOpcodeType>((static_cast<U64>(1) << OpcodeMaskHalfBits) - 1);

//! Feistel round function: mixes one opcode half with a round key.
//! The multiplier is the 64-bit golden-ratio constant (2^64/phi), a standard
//! integer-hash diffusion multiplier.
constexpr FwOpcodeType opcodeMaskRoundFunction(const FwOpcodeType half, const U64 key) {
    const U64 mixed = (static_cast<U64>(half) * 0x9E3779B97F4A7C15) ^ key;
    return static_cast<FwOpcodeType>(mixed ^ (mixed >> OpcodeMaskHalfBits)) & OpcodeMaskHalfMask;
}

//! Keyed Feistel permutation over the full FwOpcodeType width.
//! A Feistel network is a bijection regardless of the round function, so
//! distinct opcodes always map to distinct masked values (no collisions).
constexpr FwOpcodeType maskOpcode(const FwOpcodeType opcode) {
    FwOpcodeType left = static_cast<FwOpcodeType>(opcode >> OpcodeMaskHalfBits) & OpcodeMaskHalfMask;
    FwOpcodeType right = opcode & OpcodeMaskHalfMask;
    for (U32 round = 0; round < OpcodeMaskRounds; round++) {
        const FwOpcodeType newRight = left ^ opcodeMaskRoundFunction(right, OpcodeMaskKeys[round]);
        left = right;
        right = newRight;
    }
    return static_cast<FwOpcodeType>(static_cast<FwOpcodeType>(left << OpcodeMaskHalfBits) | right);
}

//! Inverse of maskOpcode: recovers the original opcode from a masked value.
//! Provided for ground-tool reference and unit testing; flight code does not
//! call it.
constexpr FwOpcodeType unmaskOpcode(const FwOpcodeType masked) {
    FwOpcodeType left = static_cast<FwOpcodeType>(masked >> OpcodeMaskHalfBits) & OpcodeMaskHalfMask;
    FwOpcodeType right = masked & OpcodeMaskHalfMask;
    for (U32 round = OpcodeMaskRounds; round > 0; round--) {
        const FwOpcodeType prevRight = left;
        const FwOpcodeType prevLeft = right ^ opcodeMaskRoundFunction(prevRight, OpcodeMaskKeys[round - 1]);
        left = prevLeft;
        right = prevRight;
    }
    return static_cast<FwOpcodeType>(static_cast<FwOpcodeType>(left << OpcodeMaskHalfBits) | right);
}

static_assert(unmaskOpcode(maskOpcode(0x5A)) == 0x5A, "Opcode mask permutation must round-trip");

constexpr FwOpcodeType getEventOpcode(const FwOpcodeType opcode) {
    return IncludeCommandOpcodesInEvents ? (MaskCommandOpcodesInEvents ? maskOpcode(opcode) : opcode)
                                         : std::numeric_limits<FwOpcodeType>::max();
}

}  // namespace CmdDispatcherCfg
}  // namespace Svc

#endif /* CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_ */
