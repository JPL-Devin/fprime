# Svc/FilePorts SDD

## 1 Introduction

The `Svc/Ports/FilePorts` directory has ports related to file handling between components.

## 2 Port Descriptions

Port Type | Purpose | Arguments 
---- | ---- | ----
[FileAnnounce](../FileAnnounce.fpp) | This port is used to announce the production of a new file to interested components | `file_name` - name of uplinked file
[FileDispatch](../FileDispatch.fpp) | This port is used for dispatching files to various services | `file_name` - the file to dispatch
[FileRead](../FileWorkerPorts.fpp) | This port is used to request a file read operation | `path` - file path, `buffer` - buffer for read data
[FileWrite](../FileWorkerPorts.fpp) | This port is used to request a file write operation | `path` - file path, `buffer` - data buffer, `offsetBytes` - write offset, `append` - append flag
[SignalDone](../FileWorkerPorts.fpp) | This port signals completion of a file operation | `status` - operation status, `sizeBytes` - bytes processed
[CancelStatus](../FileWorkerPorts.fpp) | This port is used to cancel a current file operation | (none)
[VerifyStatus](../FileWorkerPorts.fpp) | This port is used to verify a file against an expected CRC checksum | `path` - file path, `crc` - expected CRC value

## 3 Example Usage

[Svc/FileUplink](../../../FileUplink/docs/sdd.md) uses the `FileAnnounce` port to announce the completion of a file uplink. [Svc/FileDispatcher](../../../FileDispatcher/docs/sdd.md) receives file announcements and dispatches them using the `FileDispatch` port. [Svc/FileWorker](../../../FileWorker/docs/sdd.md) uses the `FileRead`, `FileWrite`, `SignalDone`, `CancelStatus`, and `VerifyStatus` ports for file operations.  