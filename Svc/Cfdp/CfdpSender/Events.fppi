@ An error occurred opening a file for CFDP transfer
event FileOpenError(
                     fileName: string size 100 @< The name of the file
                   ) \
  severity warning high \
  id 0x00 \
  format "CFDP: Could not open file {}"

@ An error occurred reading a file during CFDP transfer
event FileReadError(
                     fileName: string size 100 @< The name of the file
                     status: I32 @< The file status of read
                   ) \
  severity warning high \
  id 0x01 \
  format "CFDP: Could not read file {} with status {}"

@ The CFDP sender component successfully sent a file
event FileSent(
                sourceFileName: string size 100 @< The source file name
                destFileName: string size 100 @< The destination file name
                fileSize: U32 @< The file size in bytes
                transactionId: U32 @< The CFDP transaction sequence number
              ) \
  severity activity high \
  id 0x02 \
  format "CFDP: Sent file {} to {} ({} bytes, txn {})"

@ The CFDP sender canceled a file transfer
event SendCanceled(
                    sourceFileName: string size 100 @< The source file name
                    destFileName: string size 100 @< The destination file name
                  ) \
  severity activity high \
  id 0x03 \
  format "CFDP: Canceled send of file {} to {}"

@ A CFDP file send has started (Metadata PDU transmitted)
event SendStarted(
                   fileSize: U32 @< The source file size
                   sourceFileName: string size 100 @< The source filename
                   destFileName: string size 100 @< The destination filename
                   transactionId: U32 @< The CFDP transaction sequence number
                 ) \
  severity activity high \
  id 0x04 \
  format "CFDP: Downlink of {} bytes started from {} to {} (txn {})"

@ Supplied source filename has overflowed
event SourceFilenameOverflow() \
  severity warning high \
  id 0x05 \
  format "CFDP: Commanded source filename too long"

@ Supplied destination filename has overflowed
event DestFilenameOverflow() \
  severity warning high \
  id 0x06 \
  format "CFDP: Commanded destination filename too long"

@ File had zero size
event ZeroSizeFile(
                    sourceFileName: string size 100 @< The source file name
                  ) \
  severity warning high \
  id 0x07 \
  format "CFDP: Downlink of file {} stopped due to zero-size."

@ Partial send had invalid offset
event PartialSendError(
                        sourceFileName: string size 100 @< The source filename
                        startOffset: U32 @< Starting file offset in bytes
                        fileSize: U32 @< Size of source file
                      ) \
  severity warning high \
  id 0x08 \
  format "CFDP: Partial send of {} failed: offset {} >= file size {}"

@ EOF PDU was transmitted
event EofSent(
               transactionId: U32 @< The CFDP transaction sequence number
               fileChecksum: U32 @< The file checksum
             ) \
  severity activity low \
  id 0x09 \
  format "CFDP: EOF PDU sent for txn {} (checksum 0x{x})"
