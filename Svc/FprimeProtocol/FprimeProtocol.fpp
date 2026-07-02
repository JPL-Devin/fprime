module Svc {
module FprimeProtocol {

    type TokenType = U32

    @ Describes the frame header format for the F Prime communications protocol
    @ The lengthField accounts for the apid field and the payload that follows the
    @ header: lengthField = sizeof(apid) + payload size
    struct FrameHeader {
        startWord: TokenType,
        lengthField: TokenType,
        apid: ComCfg.Apid,
    } default {
        startWord = 0xdeadbeef
    }

    @ Describes the frame trailer format for the F Prime communications protocol
    struct FrameTrailer {
        crcField: U32
    }

}
}
