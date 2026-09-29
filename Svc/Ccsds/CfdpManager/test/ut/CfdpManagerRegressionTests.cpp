// ======================================================================
// \title  CfdpManagerRegressionTests.cpp
// \brief  Regression tests for RX file retention, archive paths, timer
//         minimums, and playback path bounds
// ======================================================================

#include <cstring>

#include <Fw/Prm/ParamValid.hpp>
#include <Os/FileSystem.hpp>
#include <Svc/Ccsds/CfdpManager/Timer.hpp>
#include "CfdpManagerTester.hpp"

namespace Svc {
namespace Ccsds {
namespace Cfdp {

namespace {

void createFileWithData(const char* path, const U8* data, FwSizeType size) {
    Os::File file;
    ASSERT_EQ(Os::File::OP_OK, file.open(path, Os::File::OPEN_CREATE, Os::File::OVERWRITE)) << path;
    FwSizeType written = size;
    ASSERT_EQ(Os::File::OP_OK, file.write(data, written, Os::File::WAIT));
    file.close();
}

}  // namespace

// ----------------------------------------------------------------------
// RX file retention
// ----------------------------------------------------------------------

void CfdpManagerTester::testRxClass1CrcMismatchDeletesFile() {
    const U8 channelId = 0;
    const EntityId sourceEid = TEST_GROUND_EID;
    const EntityId destEid = this->component.getLocalEidParam();
    const TransactionSeq transactionSeq = 8100;
    const char* srcFile = "/ground/crc_delete_c1.bin";
    const char* dstFile = "test/ut/output/crc_delete_c1.bin";
    U8 testData[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const FileSize fileSize = sizeof(testData);

    Os::FileSystem::removeFile(dstFile);
    this->clearHistory();

    this->sendMetadataPdu(channelId, sourceEid, destEid, transactionSeq, fileSize, srcFile, dstFile,
                          Cfdp::Class::CLASS_1, 0);
    this->component.doDispatch();
    this->sendFileDataPdu(channelId, sourceEid, destEid, transactionSeq, 0, static_cast<U16>(fileSize), testData,
                          Cfdp::Class::CLASS_1);
    this->component.doDispatch();
    ASSERT_TRUE(Os::FileSystem::exists(dstFile)) << "Partial file should exist while receiving";

    this->sendEofPdu(channelId, sourceEid, destEid, transactionSeq, Cfdp::ConditionCode::CONDITION_CODE_NO_ERROR,
                     0xDEADBEEF, fileSize, Cfdp::Class::CLASS_1);
    this->component.doDispatch();
    for (U32 i = 0; i < 20; i++) {
        this->invoke_to_run1Hz(0, 0);
        this->component.doDispatch();
    }

    ASSERT_EVENTS_RxCrcMismatch_SIZE(1);
    ASSERT_EVENTS_RxFileTransferFailed_SIZE(1);
    ASSERT_EVENTS_RxFileTransferCompleted_SIZE(0);
    ASSERT_EVENTS_FileRemoveFailed_SIZE(0);
    EXPECT_FALSE(Os::FileSystem::exists(dstFile)) << "CRC-mismatched Class 1 file must be removed";

    Os::FileSystem::removeFile(dstFile);
}

void CfdpManagerTester::testRxClass2CrcMismatchDeletesFile() {
    const U8 channelId = 0;
    const EntityId sourceEid = TEST_GROUND_EID;
    const EntityId destEid = this->component.getLocalEidParam();
    const TransactionSeq transactionSeq = 8200;
    const char* srcFile = "/ground/crc_delete_c2.bin";
    const char* dstFile = "test/ut/output/crc_delete_c2.bin";
    U8 testData[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const FileSize fileSize = sizeof(testData);

    Os::FileSystem::removeFile(dstFile);
    this->clearHistory();

    this->sendMetadataPdu(channelId, sourceEid, destEid, transactionSeq, fileSize, srcFile, dstFile,
                          Cfdp::Class::CLASS_2, 1);
    this->component.doDispatch();
    this->sendFileDataPdu(channelId, sourceEid, destEid, transactionSeq, 0, static_cast<U16>(fileSize), testData,
                          Cfdp::Class::CLASS_2);
    this->component.doDispatch();
    this->sendEofPdu(channelId, sourceEid, destEid, transactionSeq, Cfdp::ConditionCode::CONDITION_CODE_NO_ERROR,
                     0xDEADBEEF, fileSize, Cfdp::Class::CLASS_2);
    this->component.doDispatch();

    Transaction* txn = this->findTransaction(channelId, transactionSeq);
    ASSERT_TRUE(txn != nullptr);
    for (U32 i = 0; (i < 20) && (txn->m_state_data.receive.sub_state != RxSubState::RX_SUB_STATE_CLOSEOUT_SYNC); i++) {
        this->invoke_to_run1Hz(0, 0);
        this->component.doDispatch();
    }
    ASSERT_EVENTS_RxCrcMismatch_SIZE(1);
    ASSERT_EQ(RxSubState::RX_SUB_STATE_CLOSEOUT_SYNC, txn->m_state_data.receive.sub_state)
        << "FIN should have been sent after the CRC failure";
    EXPECT_TRUE(Os::FileSystem::exists(dstFile)) << "File is held until the FIN is acknowledged";

    // FIN-ACK from the sender closes out the transaction
    this->sendAckPdu(channelId, sourceEid, destEid, transactionSeq, Cfdp::FileDirective::FILE_DIRECTIVE_FIN, 1,
                     Cfdp::ConditionCode::CONDITION_CODE_FILE_CHECKSUM_FAILURE,
                     Cfdp::AckTxnStatus::ACK_TXN_STATUS_TERMINATED);
    this->component.doDispatch();

    EXPECT_EQ(TxnState::TXN_STATE_HOLD, txn->m_state);
    ASSERT_EVENTS_RxFileTransferFailed_SIZE(1);
    ASSERT_EVENTS_RxFileTransferCompleted_SIZE(0);
    ASSERT_EVENTS_FileRemoveFailed_SIZE(0);
    EXPECT_FALSE(Os::FileSystem::exists(dstFile)) << "CRC-mismatched Class 2 file must be removed";

    Os::FileSystem::removeFile(dstFile);
}

void CfdpManagerTester::testRxLateMetadataSizeMismatchDeletesTempFile() {
    const U8 channelId = 0;
    const EntityId sourceEid = TEST_GROUND_EID;
    const EntityId destEid = this->component.getLocalEidParam();
    const TransactionSeq transactionSeq = 8300;
    const char* srcFile = "/ground/late_md.bin";
    const char* dstFile = "test/ut/output/late_md.bin";
    U8 testData[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const U8 existingData[4] = {0xCA, 0xFE, 0xBA, 0xBE};
    const FileSize eofSize = sizeof(testData);
    const FileSize mdSize = eofSize + 16;

    // A pre-existing file at the destination must survive a transaction that never renamed onto it
    createFileWithData(dstFile, existingData, sizeof(existingData));
    this->clearHistory();

    // FileData before Metadata: the receiver opens a temp file and NAKs for the metadata
    this->sendFileDataPdu(channelId, sourceEid, destEid, transactionSeq, 0, static_cast<U16>(eofSize), testData,
                          Cfdp::Class::CLASS_2);
    this->component.doDispatch();
    ASSERT_EVENTS_RxTempFileCreated_SIZE(1);
    Fw::String tmpFile(this->eventHistory_RxTempFileCreated->at(0).filename);
    EXPECT_TRUE(Os::FileSystem::exists(tmpFile.toChar()));

    this->sendEofPdu(channelId, sourceEid, destEid, transactionSeq, Cfdp::ConditionCode::CONDITION_CODE_NO_ERROR,
                     0xDEADBEEF, eofSize, Cfdp::Class::CLASS_2);
    this->component.doDispatch();

    // Late metadata disagrees with the EOF size, so it is rejected and the temp file is never renamed
    this->sendMetadataPdu(channelId, sourceEid, destEid, transactionSeq, mdSize, srcFile, dstFile, Cfdp::Class::CLASS_2,
                          1);
    this->component.doDispatch();
    ASSERT_EVENTS_RxEofMdSizeMismatch_SIZE(1);
    EXPECT_FALSE(Os::FileSystem::exists(tmpFile.toChar())) << "Temp file must be removed on metadata mismatch";
    EXPECT_TRUE(Os::FileSystem::exists(dstFile));

    Transaction* txn = this->findTransaction(channelId, transactionSeq);
    ASSERT_TRUE(txn != nullptr);
    for (U32 i = 0; (i < 20) && (txn->m_state_data.receive.sub_state != RxSubState::RX_SUB_STATE_CLOSEOUT_SYNC); i++) {
        this->invoke_to_run1Hz(0, 0);
        this->component.doDispatch();
    }
    ASSERT_EQ(RxSubState::RX_SUB_STATE_CLOSEOUT_SYNC, txn->m_state_data.receive.sub_state);

    this->sendAckPdu(channelId, sourceEid, destEid, transactionSeq, Cfdp::FileDirective::FILE_DIRECTIVE_FIN, 1,
                     Cfdp::ConditionCode::CONDITION_CODE_FILE_SIZE_ERROR,
                     Cfdp::AckTxnStatus::ACK_TXN_STATUS_TERMINATED);
    this->component.doDispatch();

    EXPECT_EQ(TxnState::TXN_STATE_HOLD, txn->m_state);
    ASSERT_EVENTS_RxFileTransferFailed_SIZE(1);
    ASSERT_EVENTS_FileRemoveFailed_SIZE(0);
    EXPECT_FALSE(Os::FileSystem::exists(tmpFile.toChar()));
    ASSERT_TRUE(Os::FileSystem::exists(dstFile)) << "Destination named by rejected metadata must not be deleted";
    U8 readBack[sizeof(existingData)] = {0};
    Os::File check;
    ASSERT_EQ(Os::File::OP_OK, check.open(dstFile, Os::File::OPEN_READ));
    FwSizeType readSize = sizeof(readBack);
    ASSERT_EQ(Os::File::OP_OK, check.read(readBack, readSize));
    check.close();
    EXPECT_EQ(sizeof(existingData), readSize);
    EXPECT_EQ(0, memcmp(existingData, readBack, sizeof(existingData)));

    Os::FileSystem::removeFile(dstFile);
}

void CfdpManagerTester::testRxCancelDeletesFile() {
    const U8 channelId = 0;
    const EntityId sourceEid = TEST_GROUND_EID;
    const EntityId destEid = this->component.getLocalEidParam();
    const TransactionSeq transactionSeq = 8300;
    const char* srcFile = "/ground/cancel_delete.bin";
    const char* dstFile = "test/ut/output/cancel_delete.bin";
    U8 testData[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const FileSize fileSize = 64;  // more data is still outstanding when the cancel arrives

    Os::FileSystem::removeFile(dstFile);
    this->clearHistory();

    this->sendMetadataPdu(channelId, sourceEid, destEid, transactionSeq, fileSize, srcFile, dstFile,
                          Cfdp::Class::CLASS_1, 0);
    this->component.doDispatch();
    this->sendFileDataPdu(channelId, sourceEid, destEid, transactionSeq, 0, static_cast<U16>(sizeof(testData)),
                          testData, Cfdp::Class::CLASS_1);
    this->component.doDispatch();
    ASSERT_TRUE(Os::FileSystem::exists(dstFile)) << "Partial file should exist while receiving";

    this->sendCmd_CancelTransaction(0, 0, channelId, transactionSeq, sourceEid);
    this->component.doDispatch();

    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CfdpManagerComponentBase::OPCODE_CANCELTRANSACTION, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_TransactionCanceled_SIZE(1);
    ASSERT_EVENTS_RxFileTransferCompleted_SIZE(0);
    ASSERT_EVENTS_FileRemoveFailed_SIZE(0);
    EXPECT_FALSE(Os::FileSystem::exists(dstFile)) << "Canceled RX file must be removed";

    Os::FileSystem::removeFile(dstFile);
}

void CfdpManagerTester::testRxInactivityDeletesFile() {
    const U8 channelId = 0;
    const EntityId sourceEid = TEST_GROUND_EID;
    const EntityId destEid = this->component.getLocalEidParam();
    const TransactionSeq transactionSeq = 8400;
    const char* srcFile = "/ground/inactivity_delete.bin";
    const char* dstFile = "test/ut/output/inactivity_delete.bin";
    U8 testData[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const FileSize fileSize = 64;

    Os::FileSystem::removeFile(dstFile);
    this->clearHistory();

    // Class 1: no ACK/NAK timers, so silence from the sender can only end via the inactivity timer
    this->sendMetadataPdu(channelId, sourceEid, destEid, transactionSeq, fileSize, srcFile, dstFile,
                          Cfdp::Class::CLASS_1, 0);
    this->component.doDispatch();
    this->sendFileDataPdu(channelId, sourceEid, destEid, transactionSeq, 0, static_cast<U16>(sizeof(testData)),
                          testData, Cfdp::Class::CLASS_1);
    this->component.doDispatch();
    ASSERT_TRUE(Os::FileSystem::exists(dstFile)) << "Partial file should exist while receiving";

    // Sender goes silent: inactivity fires and the transaction is recycled with its file open
    const U32 cyclesToRun = this->component.getInactivityTimerParam(channelId) + 3;
    for (U32 i = 0; i < cyclesToRun; i++) {
        this->invoke_to_run1Hz(0, 0);
        this->component.doDispatch();
    }

    ASSERT_EVENTS_RxInactivityTimeout_SIZE(1);
    ASSERT_EVENTS_DanglingFileHandleClosed_SIZE(0);
    ASSERT_EVENTS_FileRemoveFailed_SIZE(0);
    EXPECT_EQ(nullptr, this->findTransaction(channelId, transactionSeq)) << "Transaction should be recycled";
    EXPECT_FALSE(Os::FileSystem::exists(dstFile)) << "Timed-out RX file must be removed";

    Os::FileSystem::removeFile(dstFile);
}

void CfdpManagerTester::testRxClass1SuccessKeepsFile() {
    const U8 channelId = 0;
    const EntityId sourceEid = TEST_GROUND_EID;
    const EntityId destEid = this->component.getLocalEidParam();
    const TransactionSeq transactionSeq = 8150;
    const char* srcFile = "/ground/keep_c1.bin";
    const char* dstFile = "test/ut/output/keep_c1.bin";
    U8 testData[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const FileSize fileSize = sizeof(testData);
    CFDP::Checksum crc;
    crc.update(testData, 0, static_cast<U32>(fileSize));

    Os::FileSystem::removeFile(dstFile);
    this->clearHistory();

    this->sendMetadataPdu(channelId, sourceEid, destEid, transactionSeq, fileSize, srcFile, dstFile,
                          Cfdp::Class::CLASS_1, 0);
    this->component.doDispatch();
    this->sendFileDataPdu(channelId, sourceEid, destEid, transactionSeq, 0, static_cast<U16>(fileSize), testData,
                          Cfdp::Class::CLASS_1);
    this->component.doDispatch();
    this->sendEofPdu(channelId, sourceEid, destEid, transactionSeq, Cfdp::ConditionCode::CONDITION_CODE_NO_ERROR,
                     crc.getValue(), fileSize, Cfdp::Class::CLASS_1);
    this->component.doDispatch();

    ASSERT_EVENTS_RxCrcMismatch_SIZE(0);
    ASSERT_EVENTS_RxFileTransferFailed_SIZE(0);
    ASSERT_EVENTS_RxFileTransferCompleted_SIZE(1);
    ASSERT_TRUE(Os::FileSystem::exists(dstFile)) << "Successfully received Class 1 file must be retained";
    this->verifyReceivedFile(dstFile, testData, fileSize);

    Os::FileSystem::removeFile(dstFile);
}

void CfdpManagerTester::testRxClass2SuccessKeepsFile() {
    const U8 channelId = 0;
    const EntityId sourceEid = TEST_GROUND_EID;
    const EntityId destEid = this->component.getLocalEidParam();
    const TransactionSeq transactionSeq = 8250;
    const char* srcFile = "/ground/keep_c2.bin";
    const char* dstFile = "test/ut/output/keep_c2.bin";
    U8 testData[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const FileSize fileSize = sizeof(testData);
    CFDP::Checksum crc;
    crc.update(testData, 0, static_cast<U32>(fileSize));

    Os::FileSystem::removeFile(dstFile);
    this->clearHistory();

    this->sendMetadataPdu(channelId, sourceEid, destEid, transactionSeq, fileSize, srcFile, dstFile,
                          Cfdp::Class::CLASS_2, 1);
    this->component.doDispatch();
    this->sendFileDataPdu(channelId, sourceEid, destEid, transactionSeq, 0, static_cast<U16>(fileSize), testData,
                          Cfdp::Class::CLASS_2);
    this->component.doDispatch();
    this->sendEofPdu(channelId, sourceEid, destEid, transactionSeq, Cfdp::ConditionCode::CONDITION_CODE_NO_ERROR,
                     crc.getValue(), fileSize, Cfdp::Class::CLASS_2);
    this->component.doDispatch();

    Transaction* txn = this->findTransaction(channelId, transactionSeq);
    ASSERT_TRUE(txn != nullptr);
    for (U32 i = 0; (i < 20) && (txn->m_state_data.receive.sub_state != RxSubState::RX_SUB_STATE_CLOSEOUT_SYNC); i++) {
        this->invoke_to_run1Hz(0, 0);
        this->component.doDispatch();
    }
    ASSERT_EQ(RxSubState::RX_SUB_STATE_CLOSEOUT_SYNC, txn->m_state_data.receive.sub_state)
        << "FIN should have been sent after the CRC check";

    this->sendAckPdu(channelId, sourceEid, destEid, transactionSeq, Cfdp::FileDirective::FILE_DIRECTIVE_FIN, 1,
                     Cfdp::ConditionCode::CONDITION_CODE_NO_ERROR, Cfdp::AckTxnStatus::ACK_TXN_STATUS_TERMINATED);
    this->component.doDispatch();

    EXPECT_EQ(TxnState::TXN_STATE_HOLD, txn->m_state);
    ASSERT_EVENTS_RxCrcMismatch_SIZE(0);
    ASSERT_EVENTS_RxFileTransferFailed_SIZE(0);
    ASSERT_EVENTS_RxFileTransferCompleted_SIZE(1);
    ASSERT_TRUE(Os::FileSystem::exists(dstFile)) << "Successfully received Class 2 file must be retained";
    this->verifyReceivedFile(dstFile, testData, fileSize);

    Os::FileSystem::removeFile(dstFile);
}

// ----------------------------------------------------------------------
// move_dir / fail_dir archive paths
// ----------------------------------------------------------------------

void CfdpManagerTester::testMoveDirArchivesBasename() {
    const U8 channelId = 0;
    const char* moveDir = "test/ut/output/archive";
    const char* srcFiles[2] = {"test/ut/output/archive_src_a.bin", "test/ut/output/archive_src_b.bin"};
    const char* archived[2] = {"test/ut/output/archive/archive_src_a.bin", "test/ut/output/archive/archive_src_b.bin"};
    const U8 testData[4] = {1, 2, 3, 4};

    Os::FileSystem::removeFile(archived[0]);
    Os::FileSystem::removeFile(archived[1]);
    Os::FileSystem::removeDirectory(moveDir);
    ASSERT_EQ(Os::FileSystem::OP_OK, Os::FileSystem::createDirectory(moveDir));

    Fw::ParamValid valid;
    ChannelArrayParams channelConfig = this->component.paramGet_ChannelConfig(valid);
    ASSERT_TRUE(FW_PARAM_OK(valid));
    channelConfig[channelId].set_move_dir(Fw::String(moveDir));
    this->paramSet_ChannelConfig(channelConfig, Fw::ParamValid::VALID);
    this->paramSend_ChannelConfig(0, 0);

    for (U32 i = 0; i < 2; i++) {
        createFileWithData(srcFiles[i], testData, sizeof(testData));
        Transaction* txn =
            this->setupTestTransaction(TxnState::TXN_STATE_S1, channelId, srcFiles[i], "/ground/archived.bin",
                                       sizeof(testData), 4100 + i, TEST_GROUND_EID);
        ASSERT_TRUE(txn != nullptr);
        txn->m_engine = this->component.m_engine;
        txn->m_chan = this->component.m_engine->m_channels[channelId];
        txn->m_keep = Cfdp::Keep::DELETE;

        this->clearEvents();
        this->component.m_engine->handleNotKeepFile(txn);
        ASSERT_EVENTS_SIZE(0);
    }

    EXPECT_TRUE(Os::FileSystem::exists(archived[0])) << "First file should be archived under its basename";
    EXPECT_TRUE(Os::FileSystem::exists(archived[1])) << "Second file should be archived under its basename";
    EXPECT_FALSE(Os::FileSystem::exists(srcFiles[0]));
    EXPECT_FALSE(Os::FileSystem::exists(srcFiles[1]));

    Os::FileSystem::removeFile(archived[0]);
    Os::FileSystem::removeFile(archived[1]);
    Os::FileSystem::removeDirectory(moveDir);
}

void CfdpManagerTester::testFailDirArchivesBasename() {
    const U8 channelId = 0;
    const char* pollDir = "test/ut/output/poll_src";
    const char* failDir = "test/ut/output/failed";
    const char* srcFiles[2] = {"test/ut/output/poll_src/poll_a.bin", "test/ut/output/poll_src/poll_b.bin"};
    const char* archived[2] = {"test/ut/output/failed/poll_a.bin", "test/ut/output/failed/poll_b.bin"};
    const U8 testData[4] = {1, 2, 3, 4};

    Os::FileSystem::removeFile(archived[0]);
    Os::FileSystem::removeFile(archived[1]);
    Os::FileSystem::removeDirectory(failDir);
    ASSERT_EQ(Os::FileSystem::OP_OK, Os::FileSystem::createDirectory(failDir));
    Os::FileSystem::Status dirStatus = Os::FileSystem::createDirectory(pollDir);
    ASSERT_TRUE(dirStatus == Os::FileSystem::OP_OK || dirStatus == Os::FileSystem::ALREADY_EXISTS);

    Fw::ParamValid valid;
    ChannelArrayParams channelConfig = this->component.paramGet_ChannelConfig(valid);
    ASSERT_TRUE(FW_PARAM_OK(valid));
    channelConfig[channelId].set_fail_dir(Fw::String(failDir));
    this->paramSet_ChannelConfig(channelConfig, Fw::ParamValid::VALID);
    this->paramSend_ChannelConfig(0, 0);

    // Files are recognized as poll files by their parent directory matching an active poll slot
    this->component.m_engine->m_channels[channelId]->getPollDir(0)->srcDir = pollDir;

    for (U32 i = 0; i < 2; i++) {
        createFileWithData(srcFiles[i], testData, sizeof(testData));
        Transaction* txn =
            this->setupTestTransaction(TxnState::TXN_STATE_S1, channelId, srcFiles[i], "/ground/failed.bin",
                                       sizeof(testData), 4300 + i, TEST_GROUND_EID);
        ASSERT_TRUE(txn != nullptr);
        txn->m_engine = this->component.m_engine;
        txn->m_chan = this->component.m_engine->m_channels[channelId];
        txn->m_keep = Cfdp::Keep::DELETE;
        txn->m_history->txn_stat = TxnStatus::TXN_STATUS_ACK_LIMIT_NO_EOF;

        this->clearEvents();
        this->component.m_engine->handleNotKeepFile(txn);
        ASSERT_EVENTS_SIZE(0);
    }

    EXPECT_TRUE(Os::FileSystem::exists(archived[0])) << "First failed poll file should be archived under its basename";
    EXPECT_TRUE(Os::FileSystem::exists(archived[1])) << "Second failed poll file should be archived under its basename";
    EXPECT_FALSE(Os::FileSystem::exists(srcFiles[0]));
    EXPECT_FALSE(Os::FileSystem::exists(srcFiles[1]));

    this->component.m_engine->m_channels[channelId]->getPollDir(0)->srcDir = "";
    Os::FileSystem::removeFile(archived[0]);
    Os::FileSystem::removeFile(archived[1]);
    Os::FileSystem::removeDirectory(failDir);
    Os::FileSystem::removeDirectory(pollDir);
}

void CfdpManagerTester::testMoveDirPathTooLong() {
    const U8 channelId = 0;
    const char* srcFile = "test/ut/output/archive_long_src.bin";
    const U8 testData[4] = {1, 2, 3, 4};

    // <moveDir>/<basename> lands just past MaxFilePathSize
    char longDir[MaxFilePathSize + 1];
    const FwSizeType dirLen = MaxFilePathSize - 20;
    memset(longDir, 'd', dirLen);
    longDir[dirLen] = '\0';
    Fw::String moveDir(longDir);

    Fw::ParamValid valid;
    ChannelArrayParams channelConfig = this->component.paramGet_ChannelConfig(valid);
    ASSERT_TRUE(FW_PARAM_OK(valid));
    channelConfig[channelId].set_move_dir(moveDir);
    this->paramSet_ChannelConfig(channelConfig, Fw::ParamValid::VALID);
    this->paramSend_ChannelConfig(0, 0);

    createFileWithData(srcFile, testData, sizeof(testData));
    Transaction* txn = this->setupTestTransaction(TxnState::TXN_STATE_S1, channelId, srcFile, "/ground/archived.bin",
                                                  sizeof(testData), 4200, TEST_GROUND_EID);
    ASSERT_TRUE(txn != nullptr);
    txn->m_engine = this->component.m_engine;
    txn->m_chan = this->component.m_engine->m_channels[channelId];
    txn->m_keep = Cfdp::Keep::DELETE;

    this->clearEvents();
    this->component.m_engine->handleNotKeepFile(txn);

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_FilePathTooLong_SIZE(1);
    ASSERT_EVENTS_FilePathTooLong(0, moveDir.toChar(), "archive_long_src.bin", MaxFilePathSize);
    EXPECT_FALSE(Os::FileSystem::exists(srcFile)) << "Source should fall back to deletion when the move is skipped";

    Os::FileSystem::removeFile(srcFile);
}

// ----------------------------------------------------------------------
// Timer minimums
// ----------------------------------------------------------------------

void CfdpManagerTester::testZeroTimerParamClamped() {
    const U8 channelId = 0;
    const EntityId sourceEid = TEST_GROUND_EID;
    const EntityId destEid = this->component.getLocalEidParam();
    const TransactionSeq transactionSeq = 8500;
    const char* dstFile = "test/ut/output/zero_timer_rx.bin";

    Fw::ParamValid valid;
    ChannelArrayParams channelConfig = this->component.paramGet_ChannelConfig(valid);
    ASSERT_TRUE(FW_PARAM_OK(valid));
    channelConfig[channelId].set_ack_timer(0);
    channelConfig[channelId].set_inactivity_timer(0);

    this->clearHistory();
    this->paramSet_ChannelConfig(channelConfig, Fw::ParamValid::VALID);
    this->paramSend_ChannelConfig(0, 0);

    const U32 minimum = CfdpManager::MinTimerSeconds;
    ASSERT_EVENTS_InvalidTimerParameter_SIZE(2);
    ASSERT_EVENTS_InvalidTimerParameter(0, channelId, "ack_timer", 0, minimum);
    ASSERT_EVENTS_InvalidTimerParameter(1, channelId, "inactivity_timer", 0, minimum);
    EXPECT_EQ(minimum, this->component.getAckTimerParam(channelId));
    EXPECT_EQ(minimum, this->component.getInactivityTimerParam(channelId));

    // An active transaction under the zero configuration must tick without asserting and still time out
    Os::FileSystem::removeFile(dstFile);
    this->sendMetadataPdu(channelId, sourceEid, destEid, transactionSeq, 100, "/ground/zero_timer.bin", dstFile,
                          Cfdp::Class::CLASS_2, 1);
    this->component.doDispatch();
    ASSERT_TRUE(this->findTransaction(channelId, transactionSeq) != nullptr);

    this->clearEvents();
    for (U32 i = 0; i < 5; i++) {
        this->invoke_to_run1Hz(0, 0);
        this->component.doDispatch();
    }

    ASSERT_EVENTS_RxInactivityTimeout_SIZE(1);
    EXPECT_EQ(nullptr, this->findTransaction(channelId, transactionSeq)) << "Transaction should time out and recycle";
    EXPECT_FALSE(Os::FileSystem::exists(dstFile));

    Os::FileSystem::removeFile(dstFile);
}

// ----------------------------------------------------------------------
// Playback path bounds
// ----------------------------------------------------------------------

void CfdpManagerTester::testPlaybackDirectoryPathTooLong() {
    const U8 channelId = 0;
    const char* srcDir = "test/ut/output/playback_long_src";
    const char* dstDir = "/ground/playback";
    const U8 testData[4] = {1, 2, 3, 4};

    // <srcDir>/<longName> exceeds MaxFilePathSize while the file name itself is a legal directory entry
    char longName[MaxFilePathSize];
    const FwSizeType nameLen = MaxFilePathSize - 10;
    memset(longName, 'n', nameLen);
    longName[nameLen] = '\0';
    Fw::String longPath(srcDir);
    longPath += "/";
    longPath += longName;
    Fw::String okPath(srcDir);
    okPath += "/ok.bin";

    Os::FileSystem::Status dirStatus = Os::FileSystem::createDirectory(srcDir);
    ASSERT_TRUE(dirStatus == Os::FileSystem::OP_OK || dirStatus == Os::FileSystem::ALREADY_EXISTS);
    createFileWithData(longPath.toChar(), testData, sizeof(testData));
    createFileWithData(okPath.toChar(), testData, sizeof(testData));

    this->clearHistory();
    this->sendCmd_PlaybackDirectory(0, 0, channelId, TEST_GROUND_EID, Cfdp::Class::CLASS_1, Cfdp::Keep::KEEP, 0,
                                    Fw::String(srcDir), Fw::String(dstDir));
    this->component.doDispatch();
    ASSERT_CMD_RESPONSE(0, CfdpManagerComponentBase::OPCODE_PLAYBACKDIRECTORY, 0, Fw::CmdResponse::OK);

    // Directory entries are consumed one per cycle
    for (U32 i = 0; i < 5; i++) {
        this->invoke_to_run1Hz(0, 0);
        this->component.doDispatch();
    }

    ASSERT_EVENTS_FilePathTooLong_SIZE(1);
    ASSERT_EVENTS_FilePathTooLong(0, srcDir, longName, MaxFilePathSize);
    ASSERT_EVENTS_TxFileTransferStarted_SIZE(1);
    EXPECT_STREQ(okPath.toChar(), this->eventHistory_TxFileTransferStarted->at(0).srcFile.toChar())
        << "The valid entry must still be transmitted";

    Os::FileSystem::removeFile(longPath.toChar());
    Os::FileSystem::removeFile(okPath.toChar());
    Os::FileSystem::removeDirectory(srcDir);
}

// ----------------------------------------------------------------------
// Timer helper
// ----------------------------------------------------------------------

TEST(TimerHelper, ZeroDurationExpiresWithoutAssert) {
    Timer timer;
    timer.setTimer(0);
    EXPECT_EQ(Timer::RUNNING, timer.getStatus());
    timer.run();
    EXPECT_EQ(Timer::EXPIRED, timer.getStatus());
    timer.run();
    EXPECT_EQ(Timer::EXPIRED, timer.getStatus());
}

TEST(TimerHelper, OneSecondExpiresAfterOneRun) {
    Timer timer;
    timer.setTimer(1);
    EXPECT_EQ(Timer::RUNNING, timer.getStatus());
    timer.run();
    EXPECT_EQ(Timer::EXPIRED, timer.getStatus());
}

}  // namespace Cfdp
}  // namespace Ccsds
}  // namespace Svc
