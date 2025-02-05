#include "RMGMasterGenerator.hh"

#include "ProjectInfo.hh"
#include "RMGGeneratorG4Gun.hh"
#include "RMGGeneratorGPS.hh"
#include "RMGManager.hh"
#include "RMGVGenerator.hh"
#include "RMGVVertexGenerator.hh"
#include "RMGVertexConfinement.hh"
#if RMG_HAS_BXDECAY0
#include "RMGGeneratorDecay0.hh"
#endif
#include "RMGGeneratorCosmicMuons.hh"
#include "RMGLog.hh"
#include "RMGVertexFromFile.hh"

#include "G4GenericMessenger.hh"
#include "G4ThreeVector.hh"

#include "RMGTools.hh"

RMGMasterGenerator::RMGMasterGenerator()
    : fConfinement(RMGMasterGenerator::Confinement::kUnConfined), fVertexGenerator(nullptr),
      fGenerator(RMGMasterGenerator::Generator::kUndefined), fGeneratorObj(nullptr) {

  this->DefineCommands();
}

void RMGMasterGenerator::GeneratePrimaries(G4Event* event) {

  if (!fGeneratorObj) RMGLog::Out(RMGLog::fatal, "No primary generator specified!");

  // invoke vertex position generator, if specified
  if (fGenerator != Generator::kBxDecay0 and fConfinement != Confinement::kUnConfined) {
    // HACK: The BxDecay0 generator takes the responsibility for shooting the primary vertex
    // position, and this conflicts with the design I had in mind here (i.e. that a RMGVGenerator is
    // instructed about the vertex position from outside, in particular in this function here).

    if (!fVertexGenerator)
      RMGLog::Out(RMGLog::fatal, "No primary position generator (confinement) specified!");

    auto vertex = G4ThreeVector();
    auto done = fVertexGenerator->GeneratePrimariesVertex(vertex);
    if (!done) { // try aborting gracefully
      RMGLog::Out(RMGLog::error,
          "Primary vertex generation did not succeed, trying to abort the run gracefully");
      RMGManager::Instance()->GetG4RunManager()->AbortRun();
    }
    RMGLog::OutDev(RMGLog::debug, "Primary vertex position: ", vertex / CLHEP::cm, " cm");

    fGeneratorObj->SetParticlePosition(vertex);
  }

  // invoke generator (might also provide the vertex position)
  fGeneratorObj->GeneratePrimariesKinematics(event);
}

void RMGMasterGenerator::SetConfinement(RMGMasterGenerator::Confinement code) {

  fConfinement = code;

  switch (fConfinement) {
    case Confinement::kUnConfined:
      fVertexGenerator = std::make_unique<RMGVVertexGenerator>("DummyGenerator");
      break;
    case Confinement::kVolume: fVertexGenerator = std::make_unique<RMGVertexConfinement>(); break;
    case Confinement::kFromFile: fVertexGenerator = std::make_unique<RMGVertexFromFile>(); break;
    default:
      RMGLog::Out(RMGLog::fatal, "No sampling strategy for confinement '", fConfinement,
          "' specified (implement me)");
  }
  RMGLog::OutFormat(RMGLog::debug, "Primary vertex confinement strategy set to {}",
      magic_enum::enum_name<RMGMasterGenerator::Confinement>(code));
}

void RMGMasterGenerator::SetGenerator(RMGMasterGenerator::Generator gen) {

  fGenerator = gen;

  switch (fGenerator) {
    case Generator::kG4gun: fGeneratorObj = std::make_unique<RMGGeneratorG4Gun>(); break;
    case Generator::kGPS: fGeneratorObj = std::make_unique<RMGGeneratorGPS>(); break;
    case Generator::kBxDecay0:
#if RMG_HAS_BXDECAY0
      // NOTE: release ownership here, BxDecay0 will own the pointer (sigh...)
      // fVertexGenerator will hold nullptr after a call to release()
      fGeneratorObj = std::make_unique<RMGGeneratorDecay0>(fVertexGenerator.release());
#else
      RMGLog::OutFormat(RMGLog::fatal,
          "BxDecay0 not available, please build remage with -DRMG_USE_BXDECAY0=ON");
#endif
      break;
    case Generator::kCosmicMuons:
      fGeneratorObj = std::make_unique<RMGGeneratorCosmicMuons>();
      break;
    case Generator::kUndefined:
    case Generator::kUserDefined: break;
    default:
      RMGLog::Out(RMGLog::fatal, "No known implementation for generator '", fGenerator,
          "' (implement me)");
  }
  RMGLog::OutFormat(RMGLog::debug, "Primary generator set to {}",
      magic_enum::enum_name<RMGMasterGenerator::Generator>(gen));
}

void RMGMasterGenerator::SetConfinementString(std::string code) {
  try {
    this->SetConfinement(
        RMGTools::ToEnum<RMGMasterGenerator::Confinement>(code, "confinement code"));
  } catch (const std::bad_cast&) { return; }
}

void RMGMasterGenerator::SetGeneratorString(std::string gen) {
  try {
    this->SetGenerator(RMGTools::ToEnum<RMGMasterGenerator::Generator>(gen, "generator name"));
  } catch (const std::bad_cast&) { return; }
}

void RMGMasterGenerator::SetUserGenerator(RMGVGenerator* gen) {

  fGenerator = RMGMasterGenerator::Generator::kUserDefined;
  fGeneratorObj = std::unique_ptr<RMGVGenerator>(gen);
}

void RMGMasterGenerator::DefineCommands() {

  fMessenger = std::make_unique<G4GenericMessenger>(this, "/RMG/Generator/",
      "Commands for controlling generators");

  fMessenger->DeclareMethod("Confine", &RMGMasterGenerator::SetConfinementString)
      .SetGuidance("Select primary confinement strategy")
      .SetParameterName("strategy", false)
      .SetCandidates(RMGTools::GetCandidates<RMGMasterGenerator::Confinement>())
      .SetStates(G4State_Idle)
      .SetToBeBroadcasted(true);

  fMessenger->DeclareMethod("Select", &RMGMasterGenerator::SetGeneratorString)
      .SetGuidance("Select event generator")
      .SetParameterName("generator", false)
      .SetCandidates(RMGTools::GetCandidates<RMGMasterGenerator::Generator>())
      .SetStates(G4State_Idle)
      .SetToBeBroadcasted(true);
}

// vim: tabstop=2 shiftwidth=2 expandtab
