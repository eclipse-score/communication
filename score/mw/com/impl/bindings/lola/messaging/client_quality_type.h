#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_CLIENT_QUALITY_TYPE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_CLIENT_QUALITY_TYPE_H

#include <cstdint>

namespace score::mw::com::impl::lola
{

/// \brief Quality type of the client from a server's perspective.
///
/// A QM process will contain a QM MessagePassingServiceInstance while an ASIL-B process will contain both a QM and an
/// ASIL-B MessagePassingServiceInstance. A QM process always communicates with another QM process using the QM
/// MessagePassingServiceInstance and an ASIL-B process always communicates with another ASIL-B process using the ASIL-B
/// MessagePassingServiceInstance. When an ASIL-B process communicates with a QM process, it uses the QM
/// MessagePassingServiceInstance but also needs to be aware that it is an ASIL-B process communicating with a QM
/// process. This is because the ASIL-B process needs to introduce additional safeguards to prevent the QM communication
/// partner from blocking it. kASIL_QMfromB is used to identify this special case.
enum class ClientQualityType : std::uint8_t
{
    kASIL_QM,
    kASIL_QMfromB,
    kASIL_B,
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_CLIENT_QUALITY_TYPE_H
