#include "RMGRunAction.hh"

#include <cstdlib>
#include <ctime>
#include <limits>

#include "G4AnalysisManager.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"

#include "RMGEventAction.hh"
#include "RMGGermaniumOutputScheme.hh"
#include "RMGLog.hh"
#include "RMGManager.hh"
#include "RMGMasterGenerator.hh"
#include "RMGOpticalOutputScheme.hh"
#include "RMGRun.hh"
#include "RMGVGenerator.hh"

#include "fmt/chrono.h"

G4Run* RMGRunAction::GenerateRun() {
  fRMGRun = new RMGRun();
  return fRMGRun;
}

RMGRunAction::RMGRunAction(bool persistency) : fIsPersistencyEnabled(persistency) {

  if (fIsPersistencyEnabled) { this->SetupAnalysisManager(); }
}

RMGRunAction::RMGRunAction(RMGMasterGenerator* gene, bool persistency)
    : fIsPersistencyEnabled(persistency), fRMGMasterGenerator(gene) {

  if (fIsPersistencyEnabled) { this->SetupAnalysisManager(); }
}

// called in the run action constructor
void RMGRunAction::SetupAnalysisManager() {

  auto rmg_man = RMGManager::Instance();
  if (rmg_man->GetDetectorConstruction()->GetActiveDetectorList().empty()) {
    rmg_man->EnablePersistency(false);
    fIsPersistencyEnabled = false;
  }

  RMGLog::Out(RMGLog::debug, "Setting up analysis manager");

  auto ana_man = G4AnalysisManager::Instance();

  // HACK: https://geant4-forum.web.cern.ch/t/output-file-is-empty-if-hdf5-type/9252/2
  // TODO: wait for v11.0.4 to be released and set lower bound
  ana_man->CreateH1("dummy", "dummy", 10, 0, 10);
  ana_man->SetHistoDirectoryName("dummy");

  // otherwise the ntuples get placed in /default_ntuples (at least with HDF5 output)
  ana_man->SetNtupleDirectoryName("hit");

  if (RMGLog::GetLogLevelScreen() <= RMGLog::debug) ana_man->SetVerboseLevel(10);
  else ana_man->SetVerboseLevel(0);

  if (!RMGManager::Instance()->IsExecSequential()) ana_man->SetNtupleMerging(true);
  else ana_man->SetNtupleMerging(false);

  // do it only for activated detectors (have to ask to the manager)
  auto det_cons = RMGManager::Instance()->GetDetectorConstruction();
  for (const auto& d_type : det_cons->GetActiveDetectorList()) {

    RMGLog::OutFormatDev(RMGLog::debug,
        "Initializing output scheme for sensitive detector type '{}'", magic_enum::enum_name(d_type));

    // instantiate concrete output scheme class
    // TODO: allow for library user to register own output scheme
    switch (d_type) {
      case RMGHardware::kOptical:
        fOutputDataFields.emplace(d_type, std::make_unique<RMGOpticalOutputScheme>(ana_man));
        fOutputDataFields[d_type]->AssignOutputNames(ana_man);
        break;
      case RMGHardware::kGermanium:
        fOutputDataFields.emplace(d_type, std::make_unique<RMGGermaniumOutputScheme>(ana_man));
        fOutputDataFields[d_type]->AssignOutputNames(ana_man);
        break;
      default:
        RMGLog::OutDev(RMGLog::fatal, "No output scheme sensitive detector type '",
            magic_enum::enum_name(d_type), "' implemented (implement me)");
    }
  }
}

void RMGRunAction::BeginOfRunAction(const G4Run*) {

  RMGLog::OutDev(RMGLog::debug, "Start of run action");

  auto manager = RMGManager::Instance();

  if (fIsPersistencyEnabled) {
    auto ana_man = G4AnalysisManager::Instance();
    // TODO: realpath
    if (this->IsMaster())
      RMGLog::Out(RMGLog::summary, "Opening output file: ", manager->GetOutputFileName());
    ana_man->OpenFile(manager->GetOutputFileName());
  } else {
    if (this->IsMaster()) RMGLog::Out(RMGLog::warning, "Object persistency disabled");
  }

  if (fRMGMasterGenerator) {
    if (fRMGMasterGenerator->GetVertexGenerator()) {
      fRMGMasterGenerator->GetVertexGenerator()->BeginOfRunAction(fRMGRun);
    }
    if (fRMGMasterGenerator->GetGenerator()) {
      fRMGMasterGenerator->GetGenerator()->BeginOfRunAction(fRMGRun);
    }
  }

  // save start time for future
  fRMGRun->SetStartTime(std::chrono::system_clock::now());

  if (this->IsMaster()) {
    auto tt = fmt::localtime(fRMGRun->GetStartTime());

    RMGLog::OutFormat(RMGLog::summary,
        "Starting run nr. {:d}. Current local time is {:%d-%m-%Y %H:%M:%S}", fRMGRun->GetRunID(), tt);
    RMGLog::OutFormat(RMGLog::summary, "Number of events to be processed: {:d}",
        fRMGRun->GetNumberOfEventToBeProcessed());
  }

  auto g4manager = G4RunManager::GetRunManager();
  auto tot_events = g4manager->GetNumberOfEventsToBeProcessed();
  if (manager->GetPrintModulo() <= 0 and tot_events >= 100)
    manager->SetPrintModulo(tot_events / 10);
  else if (tot_events < 100) manager->SetPrintModulo(100);
}

void RMGRunAction::EndOfRunAction(const G4Run*) {

  RMGLog::OutDev(RMGLog::debug, "End of run action");

  // report some stats
  if (this->IsMaster()) {
    auto time_now = std::chrono::system_clock::now();

    RMGLog::OutFormat(RMGLog::summary,
        "Run nr. {:d} completed. {:d} events simulated. Current local time is {:%d-%m-%Y %H:%M:%S}",
        fRMGRun->GetRunID(), fRMGRun->GetNumberOfEventToBeProcessed(), fmt::localtime(time_now));

    auto start_time = fRMGRun->GetStartTime();
    auto tot_elapsed_s =
        std::chrono::duration_cast<std::chrono::seconds>(time_now - start_time).count();
    long partial = 0;
    long elapsed_d = (tot_elapsed_s - partial) / 86400;
    partial += elapsed_d * 86400;
    long elapsed_h = (tot_elapsed_s - partial) / 3600;
    partial += elapsed_h * 3600;
    long elapsed_m = (tot_elapsed_s - partial) / 60;
    partial += elapsed_m * 60;
    long elapsed_s = tot_elapsed_s - partial;

    RMGLog::OutFormat(RMGLog::summary,
        "Stats: run time was {:d} days, {:d} hours, {:d} minutes and {:d} seconds", elapsed_d,
        elapsed_h, elapsed_m, elapsed_s);

    auto total_sec_hres = std::chrono::duration<double>(time_now - fRMGRun->GetStartTime()).count();

    double n_ev = fRMGRun->GetNumberOfEvent();
    RMGLog::OutFormat(RMGLog::summary,
        "Stats: average event processing time was {:.5g} seconds/event = {:.5g} events/second",
        total_sec_hres / n_ev, n_ev / total_sec_hres);

    if (n_ev < 100) RMGLog::Out(RMGLog::warning, "Event processing time might be inaccurate");
  }

  if (fRMGMasterGenerator) {
    if (fRMGMasterGenerator->GetVertexGenerator()) {
      fRMGMasterGenerator->GetVertexGenerator()->EndOfRunAction(fRMGRun);
    }
    if (fRMGMasterGenerator->GetGenerator()) {
      fRMGMasterGenerator->GetGenerator()->EndOfRunAction(fRMGRun);
    }
  }

  if (fIsPersistencyEnabled) {
    G4AnalysisManager::Instance()->Write();
    G4AnalysisManager::Instance()->CloseFile();
  }

  // reset print modulo
  // TODO: if it's user specified, it shouldn't be reset
  RMGManager::Instance()->SetPrintModulo(-1);
}

// vim: tabstop=2 shiftwidth=2 expandtab
