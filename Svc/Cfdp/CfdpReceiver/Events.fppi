@ Received a CFDP Metadata PDU indicating a new incoming file transaction
event MetadataReceived(
                        sourceFileName: string size 100 @< The source file name
                        destFileName: string size 100 @< The destination file name
                        fileSize: U32 @< Expected file size in bytes
                        transactionId: U32 @< The CFDP transaction sequence number
                      ) \
  severity activity high \
  id 0x00 \
  format "CFDP: Metadata received for {} -> {} ({} bytes, txn {})"

@ An error occurred opening a file for writing during CFDP reception
event FileOpenError(
                     fileName: string size 100 @< The name of the file
                   ) \
  severity warning high \
  id 0x01 \
  format "CFDP: Could not open file {}" \
  throttle 5

@ An error occurred writing to a file during CFDP reception
event FileWriteError(
                      fileName: string size 100 @< The name of the file
                    ) \
  severity warning high \
  id 0x02 \
  format "CFDP: Could not write to file {}" \
  throttle 5

@ A CFDP file transfer was completed successfully
event FileReceived(
                    fileName: string size 100 @< The name of the file
                    fileSize: U32 @< The file size in bytes
                    transactionId: U32 @< The CFDP transaction sequence number
                  ) \
  severity activity high \
  id 0x03 \
  format "CFDP: Received file {} ({} bytes, txn {})"

@ EOF PDU received for a transaction
event EofReceived(
                   transactionId: U32 @< The CFDP transaction sequence number
                   conditionCode: U8 @< The condition code from the EOF PDU
                 ) \
  severity activity low \
  id 0x04 \
  format "CFDP: EOF received for txn {} (condition code {})"

@ Checksum verification failed on received file
event ChecksumFailure(
                       fileName: string size 100 @< The name of the file
                       computed: U32 @< The computed checksum
                       received: U32 @< The checksum from the EOF PDU
                     ) \
  severity warning high \
  id 0x05 \
  format "CFDP: Checksum failure on {}: computed 0x{x}, received 0x{x}"

@ Received a PDU with an invalid or unrecognized directive code
event InvalidDirective(
                        directiveCode: U8 @< The directive code
                      ) \
  severity warning high \
  id 0x06 \
  format "CFDP: Invalid directive code 0x{x}" \
  throttle 10

@ Received a PDU in an unexpected state (e.g., data PDU without metadata)
event InvalidReceiveMode(
                          pduType: U8 @< The type of the PDU received
                          mode: U8 @< The current receive mode
                        ) \
  severity warning high \
  id 0x07 \
  format "CFDP: PDU type {} received in mode {}" \
  throttle 5

@ A file data PDU was received with an offset/size out of bounds for the current file
event DataOutOfBounds(
                       dataOffset: U32 @< The offset in the file data PDU
                       dataSize: U32 @< The size of the file data
                       fileSize: U32 @< The expected file size
                     ) \
  severity warning high \
  id 0x08 \
  format "CFDP: Data out of bounds: offset {} + size {} > file size {}" \
  throttle 5

@ File reception was canceled
event ReceiveCanceled(
                       transactionId: U32 @< The CFDP transaction sequence number
                     ) \
  severity activity high \
  id 0x09 \
  format "CFDP: Reception canceled for txn {}"

@ Received a PDU with an invalid header
event InvalidPduHeader() \
  severity warning high \
  id 0x0A \
  format "CFDP: Received PDU with invalid header" \
  throttle 10

@ Received a PDU that was too small to contain a valid CFDP header
event PduTooSmall(
                   pduSize: U32 @< The size of the received buffer
                 ) \
  severity warning high \
  id 0x0B \
  format "CFDP: Received PDU too small ({} bytes)" \
  throttle 10
