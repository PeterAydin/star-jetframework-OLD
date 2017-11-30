// $Id$
// Calculation of rho from a collection of jets.
// If scale function is given the scaled rho will be exported
// with the name as "fOutRhoName".Apppend("_Scaled").
//

#include "StRho.h"

// ROOT includes
#include <TClonesArray.h>
#include <TMath.h>
#include "TH2.h"
#include "TH2F.h"

class TH2;
class TH2F;

// JetFramework includes
#include "StJet.h"
#include "StRhoParameter.h"
#include "StJetMakerTask.h"

// STAR includes
#include "StRoot/StPicoDstMaker/StPicoDst.h"
#include "StRoot/StPicoDstMaker/StPicoDstMaker.h"
#include "StRoot/StPicoEvent/StPicoTrack.h"

// STAR centrality includes
#include "StRoot/StRefMultCorr/StRefMultCorr.h"
#include "StRoot/StRefMultCorr/CentralityMaker.h"

class StJetMakerTask;

ClassImp(StRho)

//________________________________________________________________________
StRho::StRho() : 
  StRhoBase(""),
  fNExclLeadJets(0),
  fJets(0),
  fHistMultvsRho(0),
  mOutName(""), 
  fJetMakerName(""),
  fRhoMakerName("")
{
  // Standard constructor.
}

//________________________________________________________________________
StRho::StRho(const char *name, Bool_t histo, const char *outName, const char *jetMakerName) :
  StRhoBase(name, histo, jetMakerName),
  fNExclLeadJets(0),
  fJets(0),
  fHistMultvsRho(0),
  mOutName(outName),
  fJetMakerName(jetMakerName),
  fRhoMakerName(name)
{
  // Constructor.
  if (!name) return;
  SetName(name);
}

//________________________________________________________________________
StRho::~StRho()
{ /*  */
  // destructor
  delete fHistMultvsRho;

}

//________________________________________________________________________
Int_t StRho::Init()
{
  // nothing done - base class should take care of that
  // this in effect inherits from StJetFrameworkPicoBase - check it out!
  StRhoBase::Init();

  DeclareHistograms();

  // Create user objects.
  fJets = new TClonesArray("StJet");
  //fJets->SetName(fJetsName);      

  return kStOk;
}

//________________________________________________________________________
Int_t StRho::Finish() {
  //  Summarize the run.
  //cout << "StRho::Finish()\n";

  //  Write histos to file and close it.
  if(mOutName!="") {
    TFile *fout = new TFile(mOutName.Data(), "UPDATE");
    fout->cd();
    fout->mkdir(fRhoMakerName);
    fout->cd(fRhoMakerName);
    ///StRhoBase::WriteHistograms();
    WriteHistograms();
    fout->cd();
    fout->Write();
    fout->Close();
  }

/*   ===== test ======
  if(mOutName!="") {
    cout<<"checking output file in StRhoMaker::Finish().."<<endl;
    TFile *fout = new TFile(mOutName.Data(),"RECREATE");
    fout->cd();
    fout->mkdir(GetName());
    fout->cd(GetName());
    WriteHistograms();
    fout->cd();
    fout->Close();
  }
*/

  //cout<<"End of StRho::Finish"<<endl;
  return kStOK;
}

//________________________________________________________________________
void StRho::DeclareHistograms() {
    // declare histograms
    // mult was 150, 0, 1500
    fHistMultvsRho = new TH2F("fHistMultvsRho", "fHistMultvsRho", 160, 0., 800., 100, 0., 100.);
    fHistMultvsRho->GetXaxis()->SetTitle("Charged track multiplicity");
    fHistMultvsRho->GetYaxis()->SetTitle("#rho (GeV/c)/A");
}

//________________________________________________________________________
void StRho::WriteHistograms() {
  // write histograms
  fHistMultvsRho->Write();
}

//________________________________________________________________________
void StRho::Clear(Option_t *opt) {
//  StRhoBase::Clear();

//  fJets->Clear();
}

//________________________________________________________________________
Int_t StRho::Make() 
{
  // Run the analysis - ran for each event

  // get PicoDstMaker
  mPicoDstMaker = (StPicoDstMaker*)GetMaker("picoDst");
  if(!mPicoDstMaker) {
    LOG_WARN << " No PicoDstMaker! Skip! " << endm;
    return kStWarn;
  }

  // construct PicoDst object from maker
  mPicoDst = mPicoDstMaker->picoDst();
  if(!mPicoDst) {
    LOG_WARN << " No PicoDst! Skip! " << endm;
    return kStWarn;
  }

  // create pointer to PicoEvent
  mPicoEvent = mPicoDst->event();
  if(!mPicoEvent) {
    LOG_WARN << " No PicoEvent! Skip! " << endm;
    return kStWarn;
  }

  // get vertex 3 vector and declare variables
  StThreeVectorF mVertex = mPicoEvent->primaryVertex();
  double zVtx = mVertex.z();

  // z-vertex cut
  // per the Aj analysis (-40, 40) for reference
  if((zVtx < fEventZVtxMinCut) || (zVtx > fEventZVtxMaxCut)) return kStOk;  // kStWarn;

  // get JetMaker
  JetMaker = (StJetMakerTask*)GetMaker(fJetMakerName);
  const char *fJetMakerNameCh = fJetMakerName;
  if(!JetMaker) {
    LOG_WARN << Form(" No %s! Skip! ", fJetMakerNameCh) << endm;
    return kStWarn;
  }

  // if we have JetMaker, get jet collection associated with it
  if(JetMaker) {
    fJets =  JetMaker->GetJets();
    //fJets->SetName("BGJetsRho");  // name is set by Maker who created it
  }
  if(!fJets) return kStWarn;

  // get run # for centrality correction
  Int_t RunId = mPicoEvent->runId();
  Float_t fBBCCoincidenceRate = mPicoEvent->BBCx();
  Float_t fZDCCoincidenceRate = mPicoEvent->ZDCx();

  // Centrality correction calculation
  // 10 14 21 29 40 54 71 92 116 145 179 218 263 315 373 441  // RUN 14 AuAu binning
  int grefMult = mPicoEvent->grefMult();
  grefmultCorr->init(RunId);
  grefmultCorr->initEvent(grefMult, zVtx, fBBCCoincidenceRate);
  Double_t refCorr2 = grefmultCorr->getRefMultCorr(grefMult, zVtx, fBBCCoincidenceRate, 2); 
  Int_t cent16 = grefmultCorr->getCentralityBin16();
  Int_t centbin = GetCentBin(cent16, 16);
  if(cent16 == -1) return kStOk; // maybe kStOk; - this is for lowest multiplicity events 80%+ centrality, cut on them

  // to limit filling unused entries in sparse, only fill for certain centrality ranges
  // ranges can be different than functional cent bin setter
  Int_t cbin = -1;
  // need to figure out centrality first in STAR: TODO
  if (centbin>-1 && centbin < 2)    cbin = 1; // 0-10%
  else if (centbin>1 && centbin<4)  cbin = 2; // 10-20%
  else if (centbin>3 && centbin<6)  cbin = 3; // 20-30%
  else if (centbin>5 && centbin<10) cbin = 4; // 30-50%
  else if (centbin>9 && centbin<16) cbin = 5; // 50-80%
  else cbin = -99;

  // cut on centrality for analysis before doing anything
  if(fRequireCentSelection) { if(!SelectAnalysisCentralityBin(centbin, fCentralitySelectionCut)) return kStOk; }

  // get event multiplicity - TODO is this correct? same as that used for centrality
  //const int multiplicity = mPicoDst->numberOfTracks(); // this is total tracks not multiplicity
  const int multiplicity = refCorr2;

  // ============================ end of CENTRALITY ============================== //

  // initialize Rho and scaled Rho
  fOutRho->SetVal(0);
  if(fOutRhoScaled) fOutRhoScaled->SetVal(0);

  // get number of jets, initialize arrays
  const Int_t Njets   = fJets->GetEntries();
  Int_t maxJetIds[]   = {-1, -1};
  Float_t maxJetPts[] = { 0,  0};

  // exclude leading jets
  if(fNExclLeadJets > 0) {
    // loop over jets
    for (Int_t ij = 0; ij < Njets; ++ij) {
      StJet *jet = static_cast<StJet*>(fJets->At(ij));
      if(!jet) { continue; } 

      // NEED TO CHECK DEFAULTS // FIXME
      //if (!AcceptJet(jet)) continue;
      // get some jet parameters
      double jetPt = jet->Pt();
      double jetEta = jet->Eta();
      double jetPhi = jet->Phi();
      double jetArea = jet->Area();
      // some threshold cuts for tests
      if(jetPt < 0) continue;

      // get ID and pt of the leading and sub-leading jet   
      if(jetPt > maxJetPts[0]) {
	maxJetPts[1] = maxJetPts[0];
	maxJetIds[1] = maxJetIds[0];
	maxJetPts[0] = jetPt;
	maxJetIds[0] = ij;
      } else if (jetPt > maxJetPts[1]) {
	maxJetPts[1] = jetPt;
	maxJetIds[1] = ij;
      }
    }
    // only set to remove leading jet 
    if(fNExclLeadJets < 2) {
      maxJetIds[1] = -1;
      maxJetPts[1] = 0;
    }
  }

  static Double_t rhovec[999];
  Int_t NjetAcc = 0;

  // push all jets within selected acceptance into stack
  for(Int_t iJets = 0; iJets < Njets; ++iJets) {

    // excluding lead jets
    if(iJets == maxJetIds[0] || iJets == maxJetIds[1])
      continue;

    // pointer to jet
    StJet *jet = static_cast<StJet*>(fJets->At(iJets));
    if(!jet) { continue; } 

    // NEED TO CHECK FOR DEFAULTS - cuts are done at the jet finder level
    //if(!AcceptJet(jet)) continue; //FIXME
    // get some get parameters
    double jetPt = jet->Pt();
    double jetEta = jet->Eta();
    double jetPhi = jet->Phi();
    double jetArea = jet->Area();
    // some threshold cuts for tests
    if(jetPt < 0) continue;
 
    rhovec[NjetAcc] = jetPt / jetArea;
    ++NjetAcc;
  }

  // when we have accepted Jets - calculate and set rho
  if(NjetAcc > 0) {
    //find median value
    Double_t rho = TMath::Median(NjetAcc, rhovec);
    fOutRho->SetVal(rho);

    // fill histo
    fHistMultvsRho->Fill(multiplicity, rho);

    // if we want scaled Rho from charged -> ch+ne
    if(fOutRhoScaled) {
      //Double_t rhoScaled = rho * GetScaleFactor(fCent); //Don't need yet, fCent is in 5% bins
      Double_t rhoScaled = rho * 1.0;
      fOutRhoScaled->SetVal(rhoScaled);
    }
  }

  StRhoBase::FillHistograms();

  return kStOk;
} 
