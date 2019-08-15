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

const char * const g_PropertySkyraCurrent = "Current (mA)";
const char * const g_PropertySkyraCurrentOutput = "Output Current (mA)";

const char * const g_PropertySkyraPower =  "Power (mW)";
const char * const g_PropertySkyraPowerOutput =  "Output Power (mW)";

const char * const g_PropertySkyraAutostart = "Autostart";
const char * const g_PropertySkyraAutostartStatus = "Autostart Status";

const char * const g_PropertySkyraActive = "Active";
const char * const g_PropertySkyraActiveStatus = "Active Status";

const char * const g_PropertySkyraModulation = "Modulation";
const char * const g_PropertySkyraModulationStatus = "Modulation Status";

const char * const g_PropertySkyraWavelength = "Wavelength";
const char * const g_PropertySkyraLaserType = "Laser Type";

const char * const g_PropertySkyraAllLaser = "All Lasers";
const char * const g_PropertySkyraLaser = "Laser";
const char * const g_PropertySkyraLaserStatus = "Laser Status";

const char * const g_PropertyActive = "Active";
const char * const g_PropertyInactive = "Inactive";

const char * const g_On = "On";
const char * const g_Off = "Off";
const char * const g_Default_String = "Unknown";
const char * const g_Default_Integer = "0";
const char * const g_Default_Float = "0.00";

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
    int AllLasersOnOff(int);

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
	int OnPowerStatus(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnAutoStart(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnAutoStartStatus(MM::PropertyBase* pProp, MM::ActionType eAct);
	
	int OnActive(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnActiveStatus(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnModulation(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnModulationStatus(MM::PropertyBase* pProp, MM::ActionType eAct);

	int OnCurrent(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCurrentStatus(MM::PropertyBase* pProp, MM::ActionType eAct);

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
	std::string ModulationStatus();
	std::string AnalogImpedanceStatus();
	std::string GetPower (long &value );

	// Shutter API
    // ----------------
    int SetOpen(bool open = true);
    int GetOpen(bool& open);
    int Fire(double deltaT);

private:
    bool bInitialized_;
    bool bBusy_;
	bool bModulation_;
	bool bImpedance_;
	long nPower_;
    long nMaxPower_;
	long nSkyra_;
	std::string name_;
    std::string hours_;
	std::string keyStatus_;
    std::string laserStatus_;
    std::string interlock_;  
    std::string current_;
    std::string fault_;
	std::string operatingStatus_;
	std::string identity_;
    std::string serialNumber_;
    std::string version_;
	std::string controlMode_;
	std::string model_;
	std::string autostartStatus_;
	std::string modulationStatus_;
	std::string impedanceStatus_;
	std::string currentLaserType_;
	std::string currentLaser_;
	std::string currentLaserID_;

	std::vector<std::string> waveLengths_;
	std::vector<std::string> laserTypes_;
	std::vector<std::string> IDs_;
    
	int ConfirmIdentity();
    int GetState(int &value);
    int SetPowerSetpoint(long requestedPowerSetpoint);
    int GetPowerSetpoint(long& value);

	// Serial Port
	std::string port_;
	int ClearPort(void);
	std::string GetPort();
	std::string SerialCommand (std::string serialCommand);

};
