/**
 * \class HLTBeamModeFilter
 *
 *
 * Description: see class header.
 *
 *
 * \author: Vasile Mihai Ghete - HEPHY Vienna
 *
 * $Date$
 * $Revision$
 *
 */

// this class header
#include "HLTrigger/HLTfilters/interface/HLTBeamModeFilter.h"

// system include files
#include <vector>
#include <iostream>

#include <boost/cstdint.hpp>

// user include files
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"

#include "DataFormats/Common/interface/Handle.h"

#include "DataFormats/L1GlobalTrigger/interface/L1GlobalTriggerEvmReadoutRecord.h"

#include "FWCore/Utilities/interface/InputTag.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/MessageLogger/interface/MessageDrop.h"


// constructor(s)
HLTBeamModeFilter::HLTBeamModeFilter(const edm::ParameterSet& parSet) : HLTFilter(parSet),

    m_l1GtEvmReadoutRecordTag(parSet.getParameter<edm::InputTag>(
            "L1GtEvmReadoutRecordTag")),
    m_allowedBeamMode(parSet.getParameter<std::vector<unsigned int> >(
            "AllowedBeamMode")),
    m_isDebugEnabled(edm::isDebugEnabled()) {

    if (m_isDebugEnabled) {

        LogDebug("HLTBeamModeFilter") << std::endl;

        LogTrace("HLTBeamModeFilter") << "Input tag for L1 GT EVM record: "
                << m_l1GtEvmReadoutRecordTag << "\nAllowed beam modes:"
                << std::endl;

        for (std::vector<unsigned int>::const_iterator itMode =
                m_allowedBeamMode.begin(); itMode != m_allowedBeamMode.end(); ++itMode) {

            LogTrace("HLTBeamModeFilter") << "  " << (*itMode) << std::endl;

        }

        LogTrace("HLTBeamModeFilter") << std::endl;

    }
}


// destructor
HLTBeamModeFilter::~HLTBeamModeFilter() {
    // empty now
}

// member functions

bool HLTBeamModeFilter::hltFilter(edm::Event& iEvent, const edm::EventSetup& evSetup, trigger::TriggerFilterObjectWithRefs & filterproduct) {

    // for MC samples, return always true (not even checking validity of L1GlobalTriggerEvmReadoutRecord)
    // eventually, the BST information will be filled also in MC simulation to spare this check

    if (!(iEvent.isRealData())) {
        //LogDebug("HLTBeamModeFilter") << "\nRunning on MC sample."
        //        << "\nAll events are automatically selected, without testing the beam mode"
        //        << "\n" << std::endl;
        return true;
    }

    // data

    // initialize filter result
    bool filterResult = false;

    // get L1GlobalTriggerEvmReadoutRecord and beam mode
    edm::Handle<L1GlobalTriggerEvmReadoutRecord> gtEvmReadoutRecord;
    iEvent.getByLabel(m_l1GtEvmReadoutRecordTag, gtEvmReadoutRecord);

    if (!gtEvmReadoutRecord.isValid()) {
        edm::LogWarning("HLTBeamModeFilter")
                << "\nWarning: L1GlobalTriggerEvmReadoutRecord with input tag "
                << m_l1GtEvmReadoutRecordTag
                << "\nrequested in configuration, but not found in the event."
                << std::endl;

        return false;
    }

    const boost::uint16_t beamModeValue =
            (gtEvmReadoutRecord->gtfeWord()).beamMode();

    edm::LogInfo("HLTBeamModeFilter") << "Beam mode: " << beamModeValue;

    for (std::vector<unsigned int>::const_iterator itMode =
            m_allowedBeamMode.begin(); itMode != m_allowedBeamMode.end(); ++itMode) {

        if (beamModeValue == (*itMode)) {
            filterResult = true;

            //LogTrace("HLTBeamModeFilter") << "Event selected - beam mode: "
            //        << beamModeValue << "\n" << std::endl;

            break;

        }
    }

    //
    return filterResult;

}

// register as framework plugin
#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(HLTBeamModeFilter);

