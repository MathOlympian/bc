/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

// main.cpp
// Include the Irrlicht header
#include "irrlicht.h"

#include "DefaultEventReceiver.hpp"
#include "GUIMain.hpp"
#include "ScenarioDataStructure.hpp"
#include "SimulationModel.hpp"
#include "ScenarioChoice.hpp"
#include "MyEventReceiver.hpp"
#include "Network.hpp"
#include "IniFile.hpp"
#include "Constants.hpp"
#include "Lang.hpp"
#include "NMEA.hpp"
#include "Sound.hpp"
#include "Utilities.hpp"
#include "OperatingModeEnum.hpp"

#include <cstdlib> //For rand(), srand()
#include <vector>
#include <sstream>
#include <fstream> //To save to log

#ifdef _WIN32
#include <direct.h> //for windows _mkdir
#else
#include <sys/stat.h>
#endif // _WIN32

#include "profile.hpp"

//Mac OS:
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif


//Global definition for ini logger
namespace IniFile {
    irr::ILogger* irrlichtLogger = 0;
}

// Irrlicht Namespaces
using namespace irr;

int main()
{

    //Mac OS:
	#ifdef __APPLE__
    //Find starting folder
    char exePath[1024];
    uint32_t pathSize = sizeof(exePath);
    std::string exeFolderPath = "";
    if (_NSGetExecutablePath(exePath, &pathSize) == 0) {
        std::string exePathString(exePath);
        size_t pos = exePathString.find_last_of("\\/");
        if (std::string::npos != pos) {
            exeFolderPath = exePathString.substr(0, pos);
        }
    }
    //change up from BridgeCommand.app/Contents/MacOS/bc.app/Contents/MacOS to BridgeCommand.app/Contents/Resources
    exeFolderPath.append("/../../../../Resources");
    //change to this path now, so ini file is read
    chdir(exeFolderPath.c_str());
    //Note, we use this again after the createDevice call
	#endif

    //User read/write location - look in here first and the exe folder second for files
    std::string userFolder = Utilities::getUserDir();

    //Read basic ini settings
    std::string iniFilename = "bc5.ini";
    //Use local ini file if it exists
    if (Utilities::pathExists(userFolder + iniFilename)) {
        iniFilename = userFolder + iniFilename;
    }

    u32 graphicsWidth = IniFile::iniFileTou32(iniFilename, "graphics_width");
    u32 graphicsHeight = IniFile::iniFileTou32(iniFilename, "graphics_height");
    u32 graphicsDepth = IniFile::iniFileTou32(iniFilename, "graphics_depth");
    bool fullScreen = (IniFile::iniFileTou32(iniFilename, "graphics_mode")==1); //1 for full screen
    u32 antiAlias = IniFile::iniFileTou32(iniFilename, "anti_alias"); // 0 or 1 for disabled, 2,4,6,8 etc for FSAA
    u32 directX = IniFile::iniFileTou32(iniFilename, "use_directX"); // 0 for openGl, 1 for directX (if available)
	u32 disableShaders = IniFile::iniFileTou32(iniFilename, "disable_shaders"); // 0 for normal, 1 for no shaders
	if (directX == 1) {
		disableShaders = 1; //FIXME: Hardcoded for no directX shaders
	}
    //Initial view configuration
    f32 viewAngle = IniFile::iniFileTof32(iniFilename, "view_angle"); //Horizontal field of view
    f32 lookAngle = IniFile::iniFileTof32(iniFilename, "look_angle"); //Initial look angle
    if (viewAngle <= 0) {
        viewAngle = 90;
    }

    f32 cameraMinDistance = IniFile::iniFileTof32(iniFilename, "minimum_distance");
    f32 cameraMaxDistance = IniFile::iniFileTof32(iniFilename, "maximum_distance");
    if (cameraMinDistance<=0) {
        cameraMinDistance = 1;
    }
    if (cameraMaxDistance<=0) {
        cameraMaxDistance = 6*M_IN_NM;
    }

    //Load joystick settings, subtract 1 as first axis is 0 internally (not 1)
    JoystickSetup joystickSetup;
    joystickSetup.portJoystickAxis = IniFile::iniFileTou32(iniFilename, "port_throttle_channel")-1;
    joystickSetup.stbdJoystickAxis = IniFile::iniFileTou32(iniFilename, "stbd_throttle_channel")-1;
    joystickSetup.rudderJoystickAxis = IniFile::iniFileTou32(iniFilename, "rudder_channel")-1;
    joystickSetup.bowThrusterJoystickAxis = IniFile::iniFileTou32(iniFilename, "bow_thruster_channel")-1;
    joystickSetup.sternThrusterJoystickAxis = IniFile::iniFileTou32(iniFilename, "stern_thruster_channel")-1;
    //Which joystick number
    joystickSetup.portJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_port"); //TODO: Note that these have changed after 5.0b4 to be consistent with BC4.7
    joystickSetup.stbdJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_stbd");
    joystickSetup.rudderJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_rudder");
    joystickSetup.bowThrusterJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_bow_thruster");
    joystickSetup.sternThrusterJoystickNo = IniFile::iniFileTou32(iniFilename, "joystick_no_stern_thruster");

    //Joystick mapping
    u32 numberOfJoystickPoints = IniFile::iniFileTou32(iniFilename, "joystick_map_points");
    if (numberOfJoystickPoints > 0) {
        for (u32 i = 1; i < numberOfJoystickPoints+1; i++) {
            joystickSetup.inputPoints.push_back(IniFile::iniFileTof32(iniFilename, IniFile::enumerate2("joystick_map",i,1)));
            joystickSetup.outputPoints.push_back(IniFile::iniFileTof32(iniFilename, IniFile::enumerate2("joystick_map",i,2)));
        }
    }
    //Default linear mapping if not set
    if (joystickSetup.inputPoints.size()<2) {
        joystickSetup.inputPoints.clear();
        joystickSetup.outputPoints.clear();
        joystickSetup.inputPoints.push_back(-1.0);
        joystickSetup.inputPoints.push_back(1.0);
        joystickSetup.outputPoints.push_back(-1.0);
        joystickSetup.outputPoints.push_back(1.0);
    }

    //Load NMEA settings
    std::string nmeaSerialPortName = IniFile::iniFileToString(iniFilename, "NMEA_ComPort");
    std::string nmeaUDPAddressName = IniFile::iniFileToString(iniFilename, "NMEA_UDPAddress");
    std::string nmeaUDPPortName = IniFile::iniFileToString(iniFilename, "NMEA_UDPPort");

    //Load UDP network settings
    u32 udpPort = IniFile::iniFileTou32(iniFilename, "udp_send_port");
    if (udpPort == 0) {
        udpPort = 18304;
    }

    //Sensible defaults if not set
    if (graphicsWidth==0) {graphicsWidth=800;}
    if (graphicsHeight==0) {graphicsHeight=600;}
    if (graphicsDepth==0) {graphicsDepth=32;}

    //set size of camera window
    u32 graphicsWidth3d = graphicsWidth;
    u32 graphicsHeight3d = graphicsHeight*0.6;
    f32 aspect = (f32)graphicsWidth/(f32)graphicsHeight;
    f32 aspect3d = (f32)graphicsWidth3d/(f32)graphicsHeight3d;

    //create device
    SIrrlichtCreationParameters deviceParameters;
    deviceParameters.DriverType = video::EDT_OPENGL;
	//Allow optional directX if available
	if (directX==1) {
        if (IrrlichtDevice::isDriverSupported(video::EDT_DIRECT3D9)) {
            deviceParameters.DriverType = video::EDT_DIRECT3D9;
        } else {
            std::cout << "DirectX 9 requested but not available.\nThis may be because Bridge Command has been compiled without DirectX support,\nor your system does not support DirectX.\nTrying OpenGL" << std::endl << std::endl;
        }
	}

    deviceParameters.WindowSize = core::dimension2d<u32>(graphicsWidth,graphicsHeight);
    deviceParameters.Bits = graphicsDepth;
    deviceParameters.Fullscreen = fullScreen;
    deviceParameters.AntiAlias = antiAlias;
    IrrlichtDevice* device = createDeviceEx(deviceParameters);

	if (device == 0) {
		std::cerr << "Could not start - please check your graphics options." << std::endl;
		exit(EXIT_FAILURE); //Could not get file system
	}

    device->setWindowCaption(core::stringw(LONGNAME.c_str()).c_str()); //Note: Odd conversion from char* to wchar*!

    video::IVideoDriver* driver = device->getVideoDriver();
    scene::ISceneManager* smgr = device->getSceneManager();

    std::vector<std::string> logMessages;
    DefaultEventReceiver defReceiver(&logMessages, device);
    device->setEventReceiver(&defReceiver);

    //Tell the Ini routine the logger address
    IniFile::irrlichtLogger = device->getLogger();

    device->getLogger()->log("User folder is:");
    device->getLogger()->log(userFolder.c_str());

    smgr->getParameters()->setAttribute(scene::ALLOW_ZWRITE_ON_TRANSPARENT, true);

    #ifdef __APPLE__
    //Bring window to front
    //NSWindow* window = reinterpret_cast<NSWindow>(device->getVideoDriver()->getExposedVideoData().HWnd);
    //Mac OS - cd back to original dir - seems to be changed during createDevice
    io::IFileSystem* fileSystem = device->getFileSystem();
    if (fileSystem==0) {
        std::cerr << "Could not get filesystem:" << std::endl;
        exit(EXIT_FAILURE); //Could not get file system

    }
    fileSystem->changeWorkingDirectoryTo(exeFolderPath.c_str());
    #endif

    //load language
    std::string languageFile = "language.txt";
    if (Utilities::pathExists(userFolder + languageFile)) {
        languageFile = userFolder + languageFile;
    }
    Lang language(languageFile);

    //set gui skin and 'flatten' this
    gui::IGUISkin* newskin = device->getGUIEnvironment()->createSkin(gui::EGST_WINDOWS_METALLIC   );

    device->getGUIEnvironment()->setSkin(newskin);
    newskin->drop();

    //Set font : Todo - make this configurable
    gui::IGUIFont *font = device->getGUIEnvironment()->getFont("media/lucida.xml");
    if (font == 0) {
        device->getLogger()->log("Could not load font, using default");
    } else {
        //set skin default font
        device->getGUIEnvironment()->getSkin()->setFont(font);
    }

    //Choose scenario
    std::string scenarioName = "";
    std::string hostname = "";
    //Scenario path - default to user dir if it exists
    std::string scenarioPath = "Scenarios/";
    if (Utilities::pathExists(userFolder + scenarioPath)) {
        scenarioPath = userFolder + scenarioPath;
    }

    //Find default hostname if set in user directory (hostname.txt)
    if (Utilities::pathExists(userFolder + "/hostname.txt")) {
        hostname=IniFile::iniFileToString(userFolder + "/hostname.txt","hostname");
    }

	//Start sound
	Sound sound;

    OperatingMode::Mode mode = OperatingMode::Normal;
    ScenarioChoice scenarioChoice(device,&language);
    scenarioChoice.chooseScenario(scenarioName, hostname, mode, scenarioPath);

    //Save hostname in user directory (hostname.txt). Check first that the location exists
    if (!Utilities::pathExists(Utilities::getUserDirBase())) {
        std::string pathToMake = Utilities::getUserDirBase();
        if (pathToMake.size() > 1) {pathToMake.erase(pathToMake.size()-1);} //Remove trailing slash
        #ifdef _WIN32
        _mkdir(pathToMake.c_str());
        #else
        mkdir(pathToMake.c_str(),0755);
        #endif // _WIN32
    }
    if (!Utilities::pathExists(Utilities::getUserDir())) {
        std::string pathToMake = Utilities::getUserDir();
        if (pathToMake.size() > 1) {pathToMake.erase(pathToMake.size()-1);} //Remove trailing slash
        #ifdef _WIN32
        _mkdir(pathToMake.c_str());
        #else
        mkdir(pathToMake.c_str(),0755);
        #endif // _WIN32
    }

    if (Utilities::pathExists(userFolder)) { //TODO: Should we make this if it doesn't exist?
        std::string hostnameFile = userFolder + "/hostname.txt";
        std::ofstream file (hostnameFile.c_str());
        if (file.is_open()) {
            file << "hostname=" << hostname << std::endl;
            file.close();
        }
    }


    u32 creditsStartTime = device->getTimer()->getRealTime();

    //seed random number generator
    std::srand(device->getTimer()->getTime());

    //create GUI
    GUIMain guiMain;

    //Set up networking (this will get a pointer to the model later)
    //Create networking, linked to model, choosing whether to use main or secondary network mode
    Network* network = Network::createNetwork(mode, udpPort, device);
    //Network network(&model);
    network->connectToServer(hostname);

    //Read in scenario data (work in progress)
    ScenarioData scenarioData;
    if (mode == OperatingMode::Normal) {
        scenarioData = Utilities::getScenarioDataFromFile(scenarioPath + scenarioName, scenarioName);
    } else {
        //If in secondary mode, get scenario information from the server
        std::string receivedSerialisedScenarioData;
        while (device->run() && receivedSerialisedScenarioData.empty()) {
            network->getScenarioFromNetwork(receivedSerialisedScenarioData);
        }
        scenarioData.deserialise(receivedSerialisedScenarioData);
    }
    std::string serialisedScenarioData = scenarioData.serialise();

    //Note: We could use this serialised format as a scenario import/export format or for online distribution


    //Create simulation model
    SimulationModel model(device, smgr, &guiMain, &sound, scenarioData, mode, viewAngle, lookAngle, cameraMinDistance, cameraMaxDistance, disableShaders);

    //Load the gui
    bool hideEngineAndRudder=false;
    if (mode==OperatingMode::Secondary) {
        hideEngineAndRudder=true;
    }
    guiMain.load(device, &language, &logMessages, model.isSingleEngine(),hideEngineAndRudder,model.hasDepthSounder(),model.getMaxSounderDepth(),model.hasGPS(), model.hasBowThruster(), model.hasSternThruster());

    //Give the network class a pointer to the model
    network->setModel(&model);

    //load realistic water
    //RealisticWaterSceneNode* realisticWater = new RealisticWaterSceneNode(smgr, 4000, 4000, "./",irr::core::dimension2du(512, 512),smgr->getRootSceneNode());

    //create event receiver, linked to model
    MyEventReceiver receiver(device, &model, &guiMain, joystickSetup, &logMessages);
    device->setEventReceiver(&receiver);

    //create NMEA serial port and UDP, linked to model
    NMEA nmea(&model, nmeaSerialPortName, nmeaUDPAddressName, nmeaUDPPortName, device);

	//Load sound files
	sound.load(model.getOwnShipEngineSound(), model.getOwnShipWaveSound(), model.getOwnShipHornSound());

    //check enough time has elapsed to show the credits screen (5s)
    while(device->getTimer()->getRealTime() - creditsStartTime < 5000) {
        device->run();
    }
    //remove credits here
    //loadingMessage->remove(); loadingMessage = 0;

    //set up timing for NMEA
    const u32 NMEA_UPDATE_MS = 250;
    u32 nextNMEATime = device->getTimer()->getTime()+NMEA_UPDATE_MS;

//    Profiling
//    Profiler networkProfile("Network");
//    Profiler nmeaProfile("NMEA");
//    Profiler modelProfile("Model");
//    Profiler renderSetupProfile("Render setup");
//    Profiler renderRadarProfile("Render radar");
//    Profiler renderProfile("3d render");
//    Profiler guiProfile("GUI render");
//    Profiler renderFinishProfile("Render finish");

	sound.StartSound();

    //main loop
    while(device->run())
    {

//        networkProfile.tic();
        network->update();
//        networkProfile.toc();

        //Check if time has elapsed, so we send data once per NMEA_UPDATE_MS.
//        nmeaProfile.tic();
        if (device->getTimer()->getTime() >= nextNMEATime) {

            if (!nmeaSerialPortName.empty() || (!nmeaUDPAddressName.empty() && !nmeaUDPPortName.empty())) {
                nmea.updateNMEA();

                if (!nmeaSerialPortName.empty()) {
                    nmea.sendNMEASerial();
                }

                if (!nmeaUDPAddressName.empty() && !nmeaUDPPortName.empty()) {
                    nmea.sendNMEAUDP();
                }

            nextNMEATime = device->getTimer()->getTime()+NMEA_UPDATE_MS;
            }
        }
//        nmeaProfile.toc();

//        modelProfile.tic();
        model.update();
//        modelProfile.toc();


        //Set up

//        renderSetupProfile.tic();
        driver->setViewPort(core::rect<s32>(0,0,graphicsWidth,graphicsHeight)); //Full screen before beginScene
        driver->beginScene(irr::video::ECBF_COLOR|irr::video::ECBF_DEPTH, irr::video::SColor(0,128,128,128));
//        renderSetupProfile.toc();

//        renderRadarProfile.tic();

        bool fullScreenRadar = guiMain.getLargeRadar();

        //radar view portion
        if (graphicsHeight>graphicsHeight3d && (guiMain.getShowInterface() || fullScreenRadar)) {
            model.setWaterVisible(false); //Hide the reflecting water, as this updates itself on drawAll()
            if (fullScreenRadar) {
                driver->setViewPort(guiMain.getLargeRadarRect());
            } else {
                driver->setViewPort(core::rect<s32>(graphicsWidth-(graphicsHeight-graphicsHeight3d),graphicsHeight3d,graphicsWidth,graphicsHeight));
            }
            model.setRadarCameraActive();
            smgr->drawAll();
            model.setWaterVisible(true); //Re-show the water
        }

 //       renderRadarProfile.toc();

 //       renderProfile.tic();

        //3d view portion
        model.setMainCameraActive(); //Note that the NavLights expect the main camera to be active, so they know where they're being viewed from
        if (!fullScreenRadar) {
            if (guiMain.getShowInterface()) {
                driver->setViewPort(core::rect<s32>(0,0,graphicsWidth3d,graphicsHeight3d));
                model.updateViewport(aspect3d);
            } else {
                driver->setViewPort(core::rect<s32>(0,0,graphicsWidth,graphicsHeight));
                model.updateViewport(aspect);
            }
            //drawAll3dProfile.tic();
            smgr->drawAll();
            //drawAll3dProfile.toc();
        }

 //       renderProfile.toc();

 //       guiProfile.tic();
        //gui
        driver->setViewPort(core::rect<s32>(0,0,graphicsWidth,graphicsHeight)); //Full screen for gui
        guiMain.drawGUI();
 //       guiProfile.toc();

 //       renderFinishProfile.tic();
        driver->endScene();
 //       renderFinishProfile.toc();

    }

    //networking should be stopped (presumably with destructor when it goes out of scope?)
    device->getLogger()->log("About to stop network");
    delete network;

    device->drop();

    //Save log messages to user directory, into log.txt, overwrite old file with that name
    std::ofstream logFile;
    logFile.open(userFolder + "log.txt");
    for (unsigned int i=0;i<logMessages.size();i++) {
        if (logFile.good()) {
            //Check we're not creating an excessively long file
            if (i<=1000 && logMessages.at(i).length() <=1000) {
                logFile << logMessages.at(i) << std::endl;
            }
        }
    }

    //Debug - dump log
    //for (unsigned int i=0;i<logMessages.size();i++) {
    //    std::cout << logMessages.at(i) << std::endl;
    //}

    //End
    return(0);
}
