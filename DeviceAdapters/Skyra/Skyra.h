///////////////////////////////////////////////////////////////////////////////
// FILE:          Skyra.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controls Cobolt and Skyra lasers through a serial port
// COPYRIGHT:     University of Massachusetts Medical School, 2019
// LICENSE:       LGPL
// AUTHORS:       Karl Bellve, Karl.Bellve@umassmed.edu, Karl.Bellve@gmail.com
//

#ifndef _SKYRA_H_
#define _SKYRA_H_
#endif

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ModuleInterface.h"


#include <iostream>
#include <string>
#include <map>

#define MODULATION_STATUS 1
#define MODULATION_ANALOG 2
#define MODULATION_DIGITAL 3
#define MODULATION_INTERNAL 4

//////////////////////////////////////////////////////////////////////////////
// Struct
//

struct Lasers {
	int laserNumber; // int version of LaserID
	long power;
	long powerSetting;
	std::string current;
	std::string currentSetting;
	std::string controlMode;
	std::string active;
	std::string laserType;
	std::string laserID;  
	std::string waveLength;
};

//////////////////////////////////////////////////////////////////////////////
// Strings
//

const char * g_DeviceVendorName = "Cobolt: a HÜBNER Group Company";
const char * g_DeviceSkyraName = "Skyra";
const char * g_DeviceSkyraDescription = "Skyra/Cobolt Controller by Karl Bellvé";


const char * g_SendTerm = "\r";
const char * g_RecvTerm = "\r\n";


const char * g_PropertySkyraHelp1 = "Off->On: If Autostart is enabled, the start-up sequence will Restart";
const char * g_PropertySkyraHelp2 = "Off->On: If Autostart is disabled, laser(s) will go directly to On";
const char * g_PropertySkyraHelp3 = "On->Off: If Autostart is enabled, the start-up sequence will Abort";
const char * g_PropertySkyraHelp4 = "On->Off: If Autostart is disabled, laser(s) will go directly to Off state";

const char * const gPropertySkyraControlMode = "Control Mode";

const char * const g_PropertySkyraAnalogImpedance = "Analog Impedance";
const char * const g_PropertySkyraAnalogImpedanceStatus = "Analog Impedance Status";

const char * const g_PropertySkyraCurrent = "Current";
const char * const g_PropertySkyraCurrentOutput = "Current Output";
const char * const g_PropertySkyraCurrentSetting = "Current Setting";
const char * const g_PropertySkyraCurrentHelp =  "Current Units are in mA";

const char * const g_PropertySkyraPower =  "Power";
const char * const g_PropertySkyraPowerOutput =  "Power Output";
const char * const g_PropertySkyraPowerSetting =  "Power Setting";
const char * const g_PropertySkyraPowerHelp =  "Power Units are in mW";

const char * const g_PropertySkyraAutostart = "Autostart";
const char * const g_PropertySkyraAutostartStatus = "Autostart Status";

const char * const g_PropertySkyraActive = "Active";
const char * const g_PropertySkyraActiveStatus = "Active Status";

const char * const g_PropertySkyraModulationStatus = "Modulation: Status";
const char * const g_PropertySkyraAnalogModulation = "Modulation: Analog";
const char * const g_PropertySkyraDigitalModulation = "Modulation: Digital ";
const char * const g_PropertySkyraInternalModulation = "Modulation: Internal";

const char * const g_PropertySkyraWavelength = "Wavelength";
const char * const g_PropertySkyraLaserType = "Laser Type";

const char * const g_PropertySkyraAllLaser = "All Lasers";
const char * const g_PropertySkyraLaser = "Laser";
const char * const g_PropertySkyraLaserStatus = "Laser Status";

const char * const g_PropertyActive = "Active";
const char * const g_PropertyInactive = "Inactive";

const char * const g_PropertyOn = "On";
const char * const g_PropertyOff = "Off";

const char * const g_PropertyEnabled = "Enabled";
const char * const g_PropertyDisabled = "Disabled";

const char * const g_Default_Empty = "";
const char * const g_Default_String = "Unknown";
const char * const g_Default_Integer = "0";
const char * const g_Default_Float = "0.0";

const char * const g_Default_ControlMode = "Constant Power";

//////////////////////////////////////////////////////////////////////////////
// Error codes
//

#define ERR_PORT_CHANGE_FORBIDDEN	101 

#define ERR_DEVICE_NOT_FOUND		10000


//class Skyra: public CGenericBase<Skyra>
class Skyra: public CShutterBase<Skyra>
{
public:
    Skyra();
    ~Skyra();

    // MMDevice API
    // ------------
    int Initialize();
    int Shutdown();

    void GetName(char* pszName) const;
    bool Busy();
    int AllLasersOn(int);

	// Automatic Serial Port Detection
	bool SupportsDeviceDetection(void);
	MM::DeviceDetectionStatus DetectDevice(void);
	int DetectInstalledDevices();
 
    // action interface
    // ----------------
    int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnPowerMax(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnPowerSetMax(MM::PropertyBase* pProp, MM::ActionType  eAct);
   
	int OnControlMode(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnWaveLength(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnPower(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPowerOutput(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPowerSetting(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnCurrent(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCurrentOutput(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCurrentSetting(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnAutoStart(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnAutoStartStatus(MM::PropertyBase* pProp, MM::ActionType eAct);
	
	int OnActive(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnModulationStatus(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDigitalModulation(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnAnalogModulation(MM::PropertyBase* pProp, MM::ActionType eAct);	
	int OnInternalModulation(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnAllLasers(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnLaserHelp1(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnLaserHelp2(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnLaser(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnLaserStatus(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnAnalogImpedance(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnAnalogImpedanceStatus(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnHours(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnKeyStatus(MM::PropertyBase* pProp, MM::ActionType eAct);
    
    int OnInterlock(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnFault(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnOperatingStatus(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnSerialNumber(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnVersion(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnModel(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnLaserType(MM::PropertyBase* pProp, MM::ActionType eAct);
    
    // base functions
	std::string AutostartStatus();
	std::string GetModulation(int modulation=0);
	std::string SetModulation(int modulation=0, bool value = false);
	std::string AnalogImpedanceStatus();
	
	std::string SetPower(long value, std::string laserid);
	std::string GetPowerOutput(long &value, std::string laserid);
	std::string GetPowerSetting(long& value, std::string laserid);
   
	std::string SetCurrent(long value);
	//std::string GetCurrent();
	//std::string GetCurrentSetting ();

	// Shutter API
    // ----------------
    int SetOpen(bool open = true);
    int GetOpen(bool& open);
    int Fire(double deltaT);

private:
    bool bInitialized_;
    bool bBusy_;
	bool bImpedance_;
	bool bModulation_;
	bool bModulationStatus_;
	bool bAnalogModulation_;
	bool bDigitalModulation_;
	bool bInternalModulation_;
	long nSkyra_;
	std::string name_;
    std::string hours_;
	std::string keyStatus_;
    std::string laserStatus_;
    std::string interlock_;  
    std::string fault_;
	std::string operatingStatus_;
	std::string identity_;
    std::string serialNumber_;
    std::string version_;
	
	std::string model_;
	std::string autostartStatus_;
	std::string impedanceStatus_;
	
	//global, but should be set to current laser if a Skyra
	long laserPower_;
	long laserPowerSetting_;
	std::string laserCurrent_;
	std::string laserCurrentSetting_;
	std::string laserID_;
	std::string laserType_;
	std::string laserWavelength_;
	std::string laserControlMode_;

	// need this vector fro micromanager property drowndown
	std::vector<std::string> waveLengths_;

	std::vector<struct Lasers> Skyra_;
	
	struct Lasers *Laser_;
    
	int ConfirmIdentity();
    int GetState(int &value);

	// Serial Port
	std::string port_;
	int ClearPort(void);
	std::string GetPort();
	std::string SerialCommand (std::string serialCommand);

};
