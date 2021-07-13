#include "RMGManager.hh"

#include <iostream>
#include <getopt.h>
#include <string>
#include <vector>
#include <random>

#include "G4Threading.hh"
#ifdef G4MULTITHREADED
#include "G4MTRunManager.hh"
#endif
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4VisManager.hh"
#include "G4VUserPhysicsList.hh"
#include "G4UImanager.hh"
#include "G4UIsession.hh"
#include "G4UIterminal.hh"
#include "G4VisManager.hh"
#include "G4UIExecutive.hh"
#include "G4UItcsh.hh"
#include "G4VisExecutive.hh"
#include "Randomize.hh"

#include "RMGProcessesList.hh"
#include "RMGManagementDetectorConstruction.hh"
#include "RMGManagementUserAction.hh"
#include "RMGLog.hh"
#include "RMGManagerMessenger.hh"

RMGManager* RMGManager::fRMGManager = nullptr;

RMGManager::RMGManager(G4String app_name) :
  fApplicationName(app_name),
  fMacroFileName(""),
  fControlledRandomization(false) {

  if (fRMGManager) RMGLog::Out(RMGLog::fatal, "RMGManager must be singleton!");
  fRMGManager = this;
  fG4Messenger = std::make_unique<RMGManagerMessenger>(this);
}

RMGManager::~RMGManager() {
  if (RMGLog::IsOpen()) RMGLog::CloseLog();
}

void RMGManager::Initialize() {

  RMGLog::Out(RMGLog::detail, "Initializing application");

  // Suppress the Geant4 header:
  // save underlying buffer and set null (only standard output)
  std::streambuf* orig_buf = std::cout.rdbuf();
  std::cout.rdbuf(nullptr);

  if (!fG4RunManager) {
    RMGLog::Out(RMGLog::debug, "Initializing default run manager");
    fG4RunManager = std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager());
  }

  // restore buffer
  std::cout.rdbuf(orig_buf);

  if (!fG4VisManager) fG4VisManager = std::unique_ptr<G4VisManager>(new G4VisExecutive());
  if (!fProcessesList) fProcessesList = new RMGProcessesList();
  if (!fManagementUserAction) fManagementUserAction = new RMGManagementUserAction();

  fG4RunManager->SetVerboseLevel(0);
  fG4VisManager->SetVerboseLevel(0);

  if (!fManagerDetectorConstruction) fG4RunManager->SetUserInitialization(fManagerDetectorConstruction);
  fG4RunManager->SetUserInitialization(fProcessesList);
  fG4RunManager->SetUserInitialization(fManagementUserAction);

  if (!fControlledRandomization) {
    std::uniform_int_distribution<int> dist(0, std::numeric_limits<int>::max());
    std::random_device rd; // uses RDRND or /dev/urandom
    auto rand_seed = dist(rd);
    G4Random::setTheSeed(rand_seed);
    RMGLog::Out(RMGLog::summary, "CLHEP::HepRandom seed set to: ", rand_seed);
  }

  fG4RunManager->Initialize();
}

void RMGManager::Run() {
    if (fMacroFileName.empty()) {
    // MGLog(routine) << "Entering interactive mode." << endlog;
    // G4UIExecutive *session = new G4UIExecutive(argc,argv);
    // session->SessionStart();
    // delete session;
  }
  else {
    // MGLog(routine) << "Entering batch mode..." << endlog;
    // MGLog(routine) << "Executing script file from command line: " << *(args.end()-1) << endlog;
    auto UI = G4UImanager::GetUIpointer();
    UI->ApplyCommand("/control/execute " + fMacroFileName);
  }
}

G4bool RMGManager::ParseCommandLineArgs(int argc, char** argv) {

    const char* const short_opts = ":h";
    const option long_opts[] = {
        { "help",  no_argument, nullptr, 'h' },
        { nullptr, no_argument, nullptr, 0   }
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'h': // -h or --help
            case '?': // Unrecognized option
            default:
                this->PrintUsage();
                return false;
        }
    }

    // extra arguments
    std::vector<std::string> args;
    for(; optind < argc; optind++) {
        args.emplace_back(argv[optind]);
    }

    if (!args.empty()) fMacroFileName = args[0];

    return true;
}

void RMGManager::PrintUsage() {
  std::cout << fApplicationName << ": USAGE" << std::endl;
}

// vim: tabstop=2 shiftwidth=2 expandtab
