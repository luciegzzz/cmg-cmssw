#ifndef TrackAssociatorByChi2_h
#define TrackAssociatorByChi2_h

/** \class TrackAssociatorByChi2
 *  Class that performs the association of reco::Tracks and TrackingParticles evaluating the chi2 of reco tracks parameters and sim tracks parameters. The cut can be tuned from the config file: see data/TrackAssociatorByChi2.cfi. Note that the Association Map is filled with -ch2 and not chi2 because it is ordered using std::greater: the track with the lowest association chi2 will be the first in the output map.It is possible to use only diagonal terms (associator by pulls) seeting onlyDiagonal = true in the PSet 
 *
 *  $Date: 2010/05/18 15:40:39 $
 *  $Revision: 1.27 $
 *  \author cerati, magni
 */

#include "SimTracker/TrackAssociation/interface/TrackAssociatorBase.h"
#include "SimDataFormats/Track/interface/SimTrackContainer.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "MagneticField/Engine/interface/MagneticField.h" 
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h" 
#include "FWCore/Framework/interface/EventSetup.h"
#include "SimDataFormats/Vertex/interface/SimVertexContainer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "DataFormats/Math/interface/LorentzVector.h"
#include "DataFormats/BeamSpot/interface/BeamSpot.h"
#include "FWCore/Utilities/interface/InputTag.h"

#include<map>

//Note that the Association Map is filled with -ch2 and not chi2 because it is ordered using std::greater:
//the track with the lowest association chi2 will be the first in the output map.

class TrackAssociatorByChi2 : public TrackAssociatorBase {

 public:
  typedef std::map<double,  SimTrack> Chi2SimMap;
  typedef std::pair< reco::Track, Chi2SimMap> RecoToSimPair;
  typedef std::vector< RecoToSimPair > RecoToSimPairAssociation;

  /// Constructor with PSet
  TrackAssociatorByChi2(const edm::ESHandle<MagneticField> mF, edm::ParameterSet conf):
    chi2cut(conf.getParameter<double>("chi2cut")),
    onlyDiagonal(conf.getParameter<bool>("onlyDiagonal")),
    bsSrc(conf.getParameter<edm::InputTag>("beamSpot")) {
    theMF=mF;  
    if (onlyDiagonal)
      edm::LogInfo("TrackAssociator") << " ---- Using Off Diagonal Covariance Terms = 0 ---- " <<  "\n";
    else 
      edm::LogInfo("TrackAssociator") << " ---- Using Off Diagonal Covariance Terms != 0 ---- " <<  "\n";
  }

  /// Constructor with magnetic field, double, bool and InputTag
  TrackAssociatorByChi2(const edm::ESHandle<MagneticField> mF, double chi2Cut, bool onlyDiag, edm::InputTag beamspotSrc){
    chi2cut=chi2Cut;
    onlyDiagonal=onlyDiag;
    theMF=mF;  
    bsSrc = beamspotSrc;
  }

  /// Destructor
  ~TrackAssociatorByChi2(){}

  /// compare reco::TrackCollection and edm::SimTrackContainer iterators: returns the chi2
  double compareTracksParam(reco::TrackCollection::const_iterator, 
			    edm::SimTrackContainer::const_iterator, 
			    const math::XYZTLorentzVectorD, 
			    GlobalVector,
			    reco::TrackBase::CovarianceMatrix,
			    const reco::BeamSpot&) const;

  /// compare collections reco to sim
  RecoToSimPairAssociation compareTracksParam(const reco::TrackCollection&, 
					      const edm::SimTrackContainer&, 
					      const edm::SimVertexContainer&,
					      const reco::BeamSpot&) const;

  /// compare reco::TrackCollection and TrackingParticleCollection iterators: returns the chi2
  double associateRecoToSim(reco::TrackCollection::const_iterator,
			    TrackingParticleCollection::const_iterator,
			    const reco::BeamSpot&) const;

  /// propagate the track parameters of TrackinParticle from production vertex to the point of closest approach to the beam line. 
  std::pair<bool,reco::TrackBase::ParameterVector> parametersAtClosestApproach(Basic3DVector<double>,// vertex
									       Basic3DVector<double>,// momAtVtx
									       float,// charge
									       const reco::BeamSpot&) const;//beam spot
  /// Association Reco To Sim with Collections
  reco::RecoToSimCollection associateRecoToSim(const edm::RefToBaseVector<reco::Track>&,
					       const edm::RefVector<TrackingParticleCollection>&,
					       const edm::Event * event = 0,
                                               const edm::EventSetup * setup = 0 ) const ;
  /// Association Sim To Reco with Collections
  reco::SimToRecoCollection associateSimToReco(const edm::RefToBaseVector<reco::Track>&,
					       const edm::RefVector<TrackingParticleCollection>&,
					       const edm::Event * event = 0,
                                               const edm::EventSetup * setup = 0 ) const ;
  
  /// compare reco to sim the handle of reco::Track and TrackingParticle collections
  reco::RecoToSimCollection associateRecoToSim(edm::Handle<edm::View<reco::Track> >& tCH, 
					       edm::Handle<TrackingParticleCollection>& tPCH, 
					       const edm::Event * event = 0,
                                               const edm::EventSetup * setup = 0) const {
    return TrackAssociatorBase::associateRecoToSim(tCH,tPCH,event,setup);
  }
  
  /// compare reco to sim the handle of reco::Track and TrackingParticle collections
  reco::SimToRecoCollection associateSimToReco(edm::Handle<edm::View<reco::Track> >& tCH, 
					       edm::Handle<TrackingParticleCollection>& tPCH,
					       const edm::Event * event = 0,
                                               const edm::EventSetup * setup = 0) const {
    return TrackAssociatorBase::associateSimToReco(tCH,tPCH,event,setup);
  }  
 private:
  edm::ESHandle<MagneticField> theMF;
  double chi2cut;
  bool onlyDiagonal;
  edm::InputTag bsSrc;
};

#endif
