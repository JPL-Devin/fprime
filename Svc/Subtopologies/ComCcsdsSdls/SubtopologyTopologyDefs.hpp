#ifndef COMCCSDSSDLSSUBTOPOLOGY_DEFS_HPP
#define COMCCSDSSDLSSUBTOPOLOGY_DEFS_HPP

#include <Fw/Types/MallocAllocator.hpp>
#include <Svc/BufferManager/BufferManager.hpp>
#include <Svc/FrameAccumulator/FrameDetector/CcsdsTcFrameDetector.hpp>
#include "ComCcsdsSdlsConfig/ComCcsdsSdlsSubtopologyConfig.hpp"
#include "Svc/Subtopologies/ComCcsdsSdls/ComCcsdsSdlsConfig/FppConstantsAc.hpp"
#include "Svc/Subtopologies/ComCcsdsSdls/Ports_ComBufferQueueEnumAc.hpp"
#include "Svc/Subtopologies/ComCcsdsSdls/Ports_ComPacketQueueEnumAc.hpp"

namespace ComCcsdsSdls {
struct SubtopologyState {
    // Empty - no external state needed for ComCcsdsSdls subtopology
};

struct TopologyState {
    SubtopologyState comCcsdsSdls;
};
}  // namespace ComCcsdsSdls

#endif
