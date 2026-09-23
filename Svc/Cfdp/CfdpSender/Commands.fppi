@ Send a file using CFDP Class 1 (Unacknowledged mode)
async command SendFile(
                        sourceFileName: string size 100 @< The name of the on-board file to send
                        destFileName: string size 100 @< The name of the destination file on the remote entity
                      ) \
  opcode 0x00

@ Cancel the current CFDP file transfer in progress
async command Cancel \
  opcode 0x01

@ Send a partial file using CFDP Class 1
async command SendPartial(
                           sourceFileName: string size 100 @< The name of the on-board file to send
                           destFileName: string size 100 @< The name of the destination file on the remote entity
                           startOffset: U32 @< Starting offset of the source file
                           length: U32 @< Number of bytes to send from starting offset. 0 means until end of file
                         ) \
  opcode 0x02
