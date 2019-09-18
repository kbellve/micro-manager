///////////////////////////////////////////////////////////////////////////////
// FILE:          Skyra.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controls Cobolt and Skyra lasers through a serial port
// COPYRIGHT:     University of Massachusetts Medical School, 2019
// LICENSE:       LGPL
// AUTHORS:       Karl Bellve, Karl.Bellve@umassmed.edu, Karl.Bellve@gmail.com
//               
//

#include "Skyra.h"


///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
    RegisterDevice(g_DeviceSkyraName, MM::ShutterDevice, "Skyra Laser Controller");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
    if (deviceName == 0)
        return 0;
    
    if (strcmp(deviceName, g_DeviceSkyraName) == 0)
    {
        return new Skyra;
    }
    
    return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
    delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// Skyra

Skyra::Skyra() :
bInitialized_(false),
bBusy_(false),
bModulation_(true),
bModulationStatus_(false),
bAnalogModulation_(false),
bDigitalModulation_(false),
bInternalModulation_(false),
nPower_(0),
nSkyra_(0),
nMaxPower_(0),
serialNumber_("0"),
version_("0"),
hours_("0"),
keyStatus_("Off"),
currentLaser_("N/A"),
controlMode_("Constant Power"),
laserStatus_(g_Default_String),
autostartStatus_(g_Default_String),
interlock_ (g_Default_String),
fault_(g_Default_String),
identity_(g_Default_String),
port_(g_Default_String)
{
    InitializeDefaultErrorMessages();
    SetErrorText(ERR_PORT_CHANGE_FORBIDDEN, "You can't change the port after device has been initialized.");
    
    // Name
    CreateProperty(MM::g_Keyword_Name, g_DeviceSkyraName, MM::String, true); 
    
    // Description
    CreateProperty(MM::g_Keyword_Description, g_DeviceSkyraDescription, MM::String, true);

    // Port
    CPropertyAction* pAct = new CPropertyAction (this, &Skyra::OnPort);
    CreateProperty(MM::g_Keyword_Port, g_Default_String, MM::String, false, pAct, true);
	
	// Company Name
    CreateProperty("Vendor", g_DeviceVendorName, MM::String, true);

}

Skyra::~Skyra()
{
    Shutdown();
}

void Skyra::GetName(char* Name) const
{
    CDeviceUtils::CopyLimitedString(Name, g_DeviceSkyraName);
}

bool Skyra::SupportsDeviceDetection(void)
{
   return true;
}

MM::DeviceDetectionStatus Skyra::DetectDevice(void)
{
	// Code modified from Nico's Arduino Device Adapter

	if (bInitialized_)
      return MM::CanCommunicate;

   // all conditions must be satisfied...
   MM::DeviceDetectionStatus result = MM::Misconfigured;
   char answerTO[MM::MaxStrLength];

   try
   {
      std::string portLowerCase = GetPort();
	  for( std::string::iterator its = portLowerCase.begin(); its != portLowerCase.end(); ++its)
      {
         *its = (char)tolower(*its);
      }
      if( 0< portLowerCase.length() &&  0 != portLowerCase.compare("undefined")  && 0 != portLowerCase.compare("unknown") )
      {
         result = MM::CanNotCommunicate;
         // record the default answer time out
         GetCoreCallback()->GetDeviceProperty(GetPort().c_str(), "AnswerTimeout", answerTO);
         CDeviceUtils::SleepMs(2000);
         GetCoreCallback()->SetDeviceProperty(GetPort().c_str(), MM::g_Keyword_Handshaking, g_PropertyOff);
         GetCoreCallback()->SetDeviceProperty(GetPort().c_str(), MM::g_Keyword_StopBits, "1");
         GetCoreCallback()->SetDeviceProperty(GetPort().c_str(), "AnswerTimeout", "500.0");
         GetCoreCallback()->SetDeviceProperty(GetPort().c_str(), "DelayBetweenCharsMs", "0");
         MM::Device* pS = GetCoreCallback()->GetDevice(this, GetPort().c_str());

		 // This next Function Block is adapted from Jon Daniels ASISStage Device Adapter
		 std::vector<std::string> possibleBauds;
		 possibleBauds.push_back("115200");
		 possibleBauds.push_back("19200");
         for( std::vector< std::string>::iterator bit = possibleBauds.begin(); bit!= possibleBauds.end(); ++bit )
		 {
			GetCoreCallback()->SetDeviceProperty(GetPort().c_str(), MM::g_Keyword_BaudRate, (*bit).c_str());
			pS->Initialize();
			PurgeComPort(GetPort().c_str());
			// First check if the Cobolt/Skyra can communicate at 115,200 baud.
			if (ConfirmIdentity() == DEVICE_OK) {
				result = MM::CanCommunicate;
			}
			pS->Shutdown();
			if (MM::CanCommunicate == result) break;
			else CDeviceUtils::SleepMs(10);
		}
		
		// always restore the AnswerTimeout to the default
		GetCoreCallback()->SetDeviceProperty(GetPort().c_str(), "AnswerTimeout", answerTO);
	  }
   }
   catch(...)
   {
      LogMessage("Exception in DetectDevice!",false);
   }

   return result;
}

int Skyra::ConfirmIdentity()
{

	std::string answer;

	answer = SerialCommand("@cob0");
	if (answer == "OK") {
		answer = SerialCommand("l0");
		if (answer == "OK") {
			return DEVICE_OK;
		}
		else DEVICE_SERIAL_INVALID_RESPONSE;
	}
	else return DEVICE_SERIAL_INVALID_RESPONSE;

	return DEVICE_ERR;
}

int Skyra::DetectInstalledDevices()
{
	// Code modified from Nico's Arduino Device Adapter
	if (MM::CanCommunicate == DetectDevice())
	{
		std::vector<std::string> peripherals;
		peripherals.clear();
		peripherals.push_back(g_DeviceSkyraName);
		for (size_t i=0; i < peripherals.size(); i++)
		{
			MM::Device* pDev = ::CreateDevice(peripherals[i].c_str());
			if (pDev)
			{
				//AddInstalledDevice(pDev);
			}
		}
	}

	return DEVICE_OK;
}


int Skyra::Initialize()
{   
	CPropertyAction* pAct;

	AllLasersOnOff(true);
    pAct = new CPropertyAction (this, &Skyra::OnAllLasers);
    int nRet = CreateProperty(g_PropertySkyraAllLaser, laserStatus_.c_str(), MM::String, false, pAct);
    if (DEVICE_OK != nRet)
        return nRet;
    
    std::vector<std::string> commands;
	commands.clear();
    commands.push_back(g_PropertyOff);
    commands.push_back(g_PropertyOn);
    SetAllowedValues(g_PropertySkyraAllLaser, commands);

	pAct = new CPropertyAction (this, &Skyra::OnLaserHelp1);
	nRet = CreateProperty("All Lasers Help #1", g_PropertySkyraHelp1, MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnLaserHelp2);
	nRet = CreateProperty("All Lasers Help #2", g_PropertySkyraHelp2, MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;

    pAct = new CPropertyAction (this, &Skyra::OnHours);
    nRet = CreateProperty("Hours", "0.00", MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnKeyStatus);
    nRet = CreateProperty("Key On/Off", "Off", MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;
    
    pAct = new CPropertyAction (this, &Skyra::OnInterlock);
    nRet = CreateProperty("Interlock", "Interlock Open", MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;
    
    pAct = new CPropertyAction (this, &Skyra::OnFault);
    nRet = CreateProperty("Fault", "No Fault", MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnOperatingStatus);
    nRet = CreateProperty("Operating Status", g_Default_String, MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;
    

	// The following should never change, so read once and remember
	serialNumber_ = SerialCommand("sn?");
    pAct = new CPropertyAction (this, &Skyra::OnSerialNumber);
	nRet = CreateProperty("Serial Number", serialNumber_.c_str(), MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;

	model_ = SerialCommand("glm?");
	pAct = new CPropertyAction (this, &Skyra::OnModel);
    nRet = CreateProperty("Model", model_.c_str(), MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;
    
	version_ = SerialCommand("ver?");
    pAct = new CPropertyAction (this, &Skyra::OnVersion);
    nRet = CreateProperty("Firmware Version", version_.c_str(), MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;
    
	
	// Removed since this should be set by OEM, and not USER - kdb
	//AutostartStatus();
	//pAct = new CPropertyAction (this, &Skyra::OnAutoStart);
	//nRet = CreateProperty(g_PropertySkyraAutostart, autostartStatus_.c_str(), MM::String, false, pAct);
	//if (nRet != DEVICE_OK)
    //    return nRet;

	//commands.clear();
	//commands.push_back("Enabled");
    //commands.push_back("Disabled");
    //SetAllowedValues(g_PropertySkyraAutostart, commands);

	pAct = new CPropertyAction (this, &Skyra::OnAutoStartStatus);
    nRet = CreateProperty(g_PropertySkyraAutostartStatus, autostartStatus_.c_str(), MM::String, true, pAct);
    if (DEVICE_OK != nRet)
        return nRet;

	// This is where check if this is an actual Skyra, with multipe lasers.
	std::string answer;

	std::stringstream command;

	for (int x = 1; x < 5; x++) 
	{
		command.str("");
		command << x;
		currentLaserID_ = command.str();
		command << "glm?";
		answer = SerialCommand(command.str());
		
		// check if laser has a high current set, if not, skip it.
		if (answer.compare("0") != 0){
			command.str("");
			command << x << "glw?";
			answer = SerialCommand(command.str());
			if ((answer).compare(g_Msg_UNSUPPORTED_COMMAND) != 0) {
				nSkyra_++;
				currentLaser_ = answer;
				waveLengths_.push_back(currentLaser_);
				currentLaserType_ = SerialCommand(currentLaserID_+"glm?");
				laserTypes_.push_back(currentLaserType_);
				IDs_.push_back(currentLaserID_);
			} 
		}
	}

	if (nSkyra_ > 0) 
	{
		currentLaser_ = waveLengths_.front();
		currentLaserID_ = IDs_.front();
		currentLaserType_ = laserTypes_.front(); 

		pAct = new CPropertyAction (this, &Skyra::OnLaserStatus);
		nRet = CreateProperty(g_PropertySkyraLaserStatus, g_Default_String, MM::String, true, pAct);

		pAct = new CPropertyAction (this, &Skyra::OnLaser);
		nRet = CreateProperty(g_PropertySkyraLaser, g_Default_String, MM::String, false, pAct);

		pAct = new CPropertyAction (this, &Skyra::OnWaveLength);
		nRet = CreateProperty(g_PropertySkyraWavelength, currentLaser_.c_str(), MM::String, false, pAct);
		SetAllowedValues(g_PropertySkyraWavelength, waveLengths_);

		pAct = new CPropertyAction (this, &Skyra::OnLaserType);
		nRet = CreateProperty(g_PropertySkyraLaserType, currentLaserType_.c_str(), MM::String, true, pAct);

		pAct = new CPropertyAction (this, &Skyra::OnActive);
		nRet = CreateProperty(g_PropertySkyraActive, g_Default_String, MM::String, false, pAct);
		
		commands.clear();
		commands.push_back(g_PropertyActive);
		commands.push_back(g_PropertyInactive);
		SetAllowedValues(g_PropertySkyraActive, commands);
		SetAllowedValues(g_PropertySkyraActiveStatus, commands);

		commands.clear();
		commands.push_back(g_PropertyOn);
		commands.push_back(g_PropertyOff);
		SetAllowedValues(g_PropertySkyraLaser, commands);
	} 
	else
	{
		//check if Single Laser supports supports 'em' command, modulation mode
		if (SerialCommand ("em").compare(g_Msg_UNSUPPORTED_COMMAND) == 0) bModulation_ = false;
		else {
			bModulation_ = true;
			SerialCommand ("cp").compare(g_Msg_UNSUPPORTED_COMMAND); // return to constant power mode, if 'em' worked.
		}
	}

	//int nRet =  CreateProperty("Minimum Laser Power (mW)", "0", MM::Integer, true);
	//if (DEVICE_OK != nRet)
    //return nRet;
		
	//pAct = new CPropertyAction (this, &Skyra::OnPowerSetMax);
	//nRet = CreateProperty("Maximum Laser Power (mW)", "100", MM::Integer, true, pAct);
	//if (DEVICE_OK != nRet)
    //return nRet;

	// current setpoint power, not current output power
	pAct = new CPropertyAction (this, &Skyra::OnPowerSet);
	nRet = CreateProperty(g_PropertySkyraPower, "0", MM::Integer, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	// current output power, not current setpoint power
	GetPowerOutput(nPower_);
	pAct = new CPropertyAction (this, &Skyra::OnPowerOutput);
	nRet = CreateProperty(g_PropertySkyraPowerOutput, std::to_string((_Longlong) nPower_).c_str(), MM::Integer, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
	
	pAct = new CPropertyAction (this, &Skyra::OnCurrent);
	nRet = CreateProperty(g_PropertySkyraCurrent, "0", MM::String, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnCurrentStatus);
	nRet = CreateProperty(g_PropertySkyraCurrentOutput, "0", MM::Integer, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	// check if Laser supports supports analog impedance
	AnalogImpedanceStatus();	
	if (bImpedance_ == true) {
		pAct = new CPropertyAction (this, &Skyra::OnAnalogImpedance);
		nRet = CreateProperty(g_PropertySkyraAnalogImpedance, impedanceStatus_.c_str(), MM::String, false, pAct);
		if (DEVICE_OK != nRet)
			return nRet;

		commands.clear();
		commands.push_back(g_PropertyEnabled);
		commands.push_back(g_PropertyDisabled);
		SetAllowedValues(g_PropertySkyraAnalogImpedance, commands);

		pAct = new CPropertyAction (this, &Skyra::OnAnalogImpedanceStatus);
		nRet = CreateProperty(g_PropertySkyraAnalogImpedanceStatus, impedanceStatus_.c_str(), MM::String, true, pAct);
		if (DEVICE_OK != nRet)
			return nRet;
	}


	// Constant Power (Default) or Constant Current or Modulation Mode
	pAct = new CPropertyAction (this, &Skyra::OnControlMode);
	nRet = CreateProperty(gPropertySkyraControlMode, controlMode_.c_str(), MM::String, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	commands.clear();
	commands.push_back("Constant Power");
	commands.push_back("Constant Current");
	if (bModulation_ == true)  commands.push_back("Modulation");
	SetAllowedValues(gPropertySkyraControlMode, commands);  
	
	//default to Constant Power Mode
	if (controlMode_.compare("Contant Power") == 0) SerialCommand("cp");
	else if (controlMode_.compare("Contant Current") == 0) SerialCommand("ci"); 

	// Modulation
	if (bModulation_ == true) {

		pAct = new CPropertyAction (this, &Skyra::OnModulationStatus);
		CreateProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled, MM::String, true, pAct);

		pAct = new CPropertyAction (this, &Skyra::OnAnalogModulation);
		CreateProperty(g_PropertySkyraAnalogModulation, g_PropertyDisabled, MM::String, false, pAct);

		pAct = new CPropertyAction (this, &Skyra::OnDigitalModulation);
		CreateProperty(g_PropertySkyraDigitalModulation, g_PropertyDisabled, MM::String, false, pAct);
		
		pAct = new CPropertyAction (this, &Skyra::OnInternalModulation);
		CreateProperty(g_PropertySkyraInternalModulation, g_PropertyDisabled, MM::String, false, pAct);
	
		commands.clear();
		commands.push_back(g_PropertyEnabled);
		commands.push_back(g_PropertyDisabled);

		SetAllowedValues(g_PropertySkyraDigitalModulation, commands);
		SetAllowedValues(g_PropertySkyraAnalogModulation, commands);
		SetAllowedValues(g_PropertySkyraInternalModulation, commands);

	}

    nRet = UpdateStatus();
    if (nRet != DEVICE_OK)
        return nRet;
    
    bInitialized_ = true;
    return DEVICE_OK;
}


int Skyra::Shutdown()
{
    if (bInitialized_)
    {
        //AllLasersOnOff(false);
        bInitialized_ = false;
        
    }
    return DEVICE_OK;
}

bool Skyra::Busy()
{
    return bBusy_;
}

int Skyra::AllLasersOnOff(int onoff)
{
     
    if (onoff == 0)
    {
		SerialCommand("l0");
		laserStatus_ = "Off";
    }
    else if (onoff == 1)
    {
        SerialCommand("l1");
		laserStatus_ = "On";
    }
    
    return DEVICE_OK;
}

int Skyra::GetState(int &value )
{
   
	value = 0;
    return DEVICE_OK;
    
}

std::string Skyra::GetPowerOutput (long &value ) 
{
	std::string answer;
    
	answer = SerialCommand (currentLaserID_ + "pa?");
   	
	float power = (float)atof(answer.c_str()) * 1000;

	value = (long) power;

	LogMessage ("Skyra::GetPowerOutput 1: " + answer,true);

	LogMessage ("Skyra::GetPowerOutput 2: " + std::to_string((long double)power),true);

	return answer;
}


///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int Skyra::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(port_.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        if (bInitialized_)
        {
            // revert
            pProp->Set(port_.c_str());
            return ERR_PORT_CHANGE_FORBIDDEN;
        }
        
        pProp->Get(port_);
    }
    
    return DEVICE_OK;
}

int Skyra::OnPowerMax(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    
	if (eAct == MM::BeforeGet)
    {
        pProp->Set(nMaxPower_);
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(nMaxPower_);
    }
    
    return DEVICE_OK;
}


int Skyra::OnControlMode(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	std::string answer;

	if (eAct == MM::BeforeGet)
    {
       
    }
    else if (eAct == MM::AfterSet)
    {
       pProp->Get(controlMode_);
	   if (controlMode_.compare("Constant Power") == 0) {
		   SerialCommand(currentLaserID_ + "cp");
		   Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled);
	   }
	   else if (controlMode_.compare("Constant Current") == 0) {
		   SerialCommand(currentLaserID_ + "ci");
		   Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled);
	   }
	   if (controlMode_.compare("Modulation") == 0) {
		   SerialCommand(currentLaserID_ + "em");
		   Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyEnabled);
	   }
    }
    
    return DEVICE_OK;
}

int Skyra::OnAutoStart(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	if (eAct == MM::BeforeGet)
    {
       
    }
    else if (eAct == MM::AfterSet)
    {
       pProp->Get(autostartStatus_);
	   
	   if (autostartStatus_.compare("Enabled") == 0) {
		   SerialCommand("@cobas 1");
	   }
	   else if (autostartStatus_.compare("Disabled") == 0) {
		   SerialCommand("@cobas 0");
	   }
    }
    
    return DEVICE_OK;
}

int Skyra::OnAutoStartStatus(MM::PropertyBase* pProp, MM::ActionType  /* eAct */)
{
	AutostartStatus();
   
    pProp->Set(autostartStatus_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnActive(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	std::string answer;

	if (eAct == MM::BeforeGet)
    {	
		answer = SerialCommand(currentLaserID_ + "gla? ");
		if (answer.compare("0") == 0) pProp->Set(g_PropertyInactive);
		if (answer.compare("1") == 0) pProp->Set(g_PropertyActive);
    }
	else if (eAct == MM::AfterSet)
    {
		pProp->Get(answer);
		if (answer.compare(g_PropertyActive) == 0) SerialCommand(currentLaserID_ + "sla 1");
		if (answer.compare(g_PropertyInactive) == 0) SerialCommand(currentLaserID_ + "sla 0");
    }
    
    return DEVICE_OK;
}

int Skyra::OnPowerSet(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
    
    if (eAct == MM::BeforeGet)
    {
       GetPowerSetpoint(nPower_);
       pProp->Set(nPower_);
	   LogMessage("Skyra::OnPowerSet " + std::to_string((_Longlong)nPower_),true);
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(nPower_);
		
		//if(nPower_ > nMaxPower_) nPower_ = nMaxPower_;
		
		// Switch to constant power mode
		Skyra::SetProperty(gPropertySkyraControlMode,"Constant Power");
        
		SetPowerSetpoint(nPower_);
    }
    
    return DEVICE_OK;
}


int Skyra::OnPowerSetMax(MM::PropertyBase* pProp, MM::ActionType eAct )
{
	if (eAct == MM::BeforeGet)
    {
		pProp->Set(nMaxPower_);
    } 
    return DEVICE_OK;
}

int Skyra::OnPowerOutput(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	if (eAct == MM::BeforeGet)
    {
		GetPowerOutput(nPower_);
		pProp->Set(nPower_);
	}

    return DEVICE_OK;
}

int Skyra::OnHours(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    
	hours_= SerialCommand ("hrs?");
    
    pProp->Set(hours_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnKeyStatus(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    std::ostringstream command;
	std::string answer;

    answer = SerialCommand("@cobasks?");

	if (answer.at(0) == '0')
        keyStatus_ = "Off";
    else if (answer.at(0) == '1')
        keyStatus_ = "On";
   
    pProp->Set(keyStatus_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnSerialNumber(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    
    pProp->Set(serialNumber_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnModel(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{  
    
	pProp->Set(model_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnLaserType(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    
    pProp->Set(currentLaserType_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnVersion(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    
    pProp->Set(version_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnCurrentStatus(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    current_ = SerialCommand(currentLaserID_ + "i? ");

    pProp->Set(current_.c_str());
    
    return DEVICE_OK;
}


int Skyra::OnCurrent(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    
	std::ostringstream command;
   
	if (eAct == MM::BeforeGet)
    {
	}   
    else if (eAct == MM::AfterSet)
    {
		// Switch to constant current mode
		Skyra::SetProperty(gPropertySkyraControlMode,"Constant Current");
		Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled);
		bModulationStatus_ = false;

        pProp->Get(current_);

		float fCurrent = (float)atof(current_.c_str());
		if (nSkyra_) command << currentLaserID_;
		command << "slc " << fCurrent;
		SerialCommand (command.str().c_str());

    }
    
    return DEVICE_OK;
}



int Skyra::OnAnalogImpedance(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    
	std::string answer;
   
	if (eAct == MM::BeforeGet)
    {
	}   
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(answer);

		if (answer.compare("Enabled") == 0)
			SerialCommand("salis 1");
		if (answer.compare("Disabled") == 0)
			SerialCommand("salis 0");
    }
    
    return DEVICE_OK;
}

int Skyra::OnAnalogImpedanceStatus(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    
	AnalogImpedanceStatus();
    pProp->Set(impedanceStatus_.c_str());
    
	return DEVICE_OK;
}

int Skyra::OnInterlock(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    
    std::string answer;
    
    answer = SerialCommand ("ilk?");
    
    if (answer.at(0) == '0')
        interlock_ = "Closed";
    else if (answer.at(0) == '1')
        interlock_ = "Open";
    
    pProp->Set(interlock_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnOperatingStatus(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    
    std::string answer;
     
	answer = SerialCommand ("gom?");

	
    if (answer.at(0) == '0')
        operatingStatus_ = "Off";
    else if (answer.at(0) == '1')
        operatingStatus_ = "Waiting for temperature";
    else if (answer.at(0) == '5')
        operatingStatus_ = "Fault";
	else if (answer.at(0) == '6')
        operatingStatus_ = "Aborted";
	else { 
		if (nSkyra_ > 0 ) {
			if (answer.at(0) == '2')
				operatingStatus_ = "Waiting for key";
			if (answer.at(0) == '3')
				operatingStatus_ = "Warm-up";
			else if (answer.at(0) == '4')
				operatingStatus_ = "Completed";
		} else {
			if (answer.at(0) == '2')
				operatingStatus_ = "Continuous";
			if (answer.at(0) == '3')
				operatingStatus_ = "On/Off Modulation";
			else if (answer.at(0) == '4') {
				operatingStatus_ = "Modulation";
			}
		}
	}
	
	
    
    pProp->Set(operatingStatus_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnFault(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    
    std::string answer;
     
	answer = SerialCommand ("f?");

    if (answer.at(0) == '0')
        fault_ = "No Fault";
    else if (answer.at(0) == '1')
        fault_ = "Temperature Fault";
    else if (answer.at(0) == '3')
        fault_ = "Open Interlock";
    else if (answer.at(0) == '4')
        fault_ = "Constant Power Fault";
    
    pProp->Set(fault_.c_str());
    
    return DEVICE_OK;
}

int Skyra::OnWaveLength(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
    
    if (eAct == MM::BeforeGet)
    {
		pProp->Set(currentLaser_.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(currentLaser_);
		LogMessage("Current Wavelength: " + currentLaser_);
		std::vector<std::string>::iterator it;
		// Iterate and print values of vector 
		__int64 index = 0;
		std::string answer,value;
		for (it = waveLengths_.begin(); it != waveLengths_.end(); it++) 
		{
			answer = *it;
			if (answer.compare(currentLaser_) == 0) 
			{
				index = std::distance(waveLengths_.begin(), it); 
				currentLaserID_ = IDs_.at(index);
				LogMessage("Current ID: " + currentLaserID_,true);
				currentLaserType_ = laserTypes_.at(index);
				LogMessage("Current Type: " + currentLaserType_,true);

				// update current
				current_ = SerialCommand(currentLaserID_ + "i? ");
				Skyra::SetProperty(g_PropertySkyraCurrentOutput,current_.c_str());

				LogMessage("Current Power: " + GetPowerOutput(nPower_), true);
				GetPowerOutput(nPower_);
				Skyra::SetProperty(g_PropertySkyraPowerOutput,std::to_string((_Longlong)nPower_).c_str());

				// update active status
				value = SerialCommand(currentLaserID_ + "gla? ");
				if (value.compare("0") == 0) Skyra::SetProperty(g_PropertySkyraActive, g_PropertyInactive);
				else if (value.compare("1") == 0) Skyra::SetProperty(g_PropertySkyraActive, g_PropertyActive);
				// update modulation status

				GetModulation(MODULATION_STATUS);
				if (bModulationStatus_) Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyEnabled);
				else Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled);

				GetModulation(MODULATION_ANALOG);
				if (bAnalogModulation_) Skyra::SetProperty(g_PropertySkyraAnalogModulation, g_PropertyEnabled);
				else Skyra::SetProperty(g_PropertySkyraAnalogModulation, g_PropertyDisabled);

				GetModulation(MODULATION_DIGITAL);
				if (bDigitalModulation_) Skyra::SetProperty(g_PropertySkyraDigitalModulation, g_PropertyEnabled);
				else Skyra::SetProperty(g_PropertySkyraDigitalModulation, g_PropertyDisabled);
				
				GetModulation(MODULATION_INTERNAL);
				if (bInternalModulation_) Skyra::SetProperty(g_PropertySkyraInternalModulation, g_PropertyEnabled);
				else Skyra::SetProperty(g_PropertySkyraInternalModulation, g_PropertyDisabled);


			}
		}
	}
    
    return DEVICE_OK;
}

int Skyra::OnModulationStatus(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	std::string answer;
	if (eAct == MM::BeforeGet)
    {
		GetModulation(MODULATION_STATUS);

		if (bModulationStatus_) pProp->Set(g_PropertyEnabled);
		else pProp->Set(g_PropertyDisabled);
	}

    return DEVICE_OK;
}

int Skyra::OnAnalogModulation(MM::PropertyBase* pProp, MM::ActionType  eAct)
{ 
	std::string answer;

    if (eAct == MM::BeforeGet)
    {
		GetModulation(MODULATION_ANALOG);

		if (bAnalogModulation_) pProp->Set(g_PropertyEnabled);
		else pProp->Set(g_PropertyDisabled);
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(answer);
		
		if (answer.compare(g_PropertyEnabled) == 0) SetModulation(MODULATION_ANALOG, true);
		else SetModulation(MODULATION_ANALOG, false);
    }
    
    return DEVICE_OK;
}

int Skyra::OnDigitalModulation(MM::PropertyBase* pProp, MM::ActionType  eAct)
{ 
	std::string answer;

    if (eAct == MM::BeforeGet)
    {
		GetModulation(MODULATION_DIGITAL);
		if (bDigitalModulation_) pProp->Set(g_PropertyEnabled);
		else pProp->Set(g_PropertyDisabled);
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(answer);
		if (answer.compare(g_PropertyEnabled) == 0) SetModulation(MODULATION_DIGITAL, true);
		else SetModulation(MODULATION_DIGITAL, false);
    }
    
    return DEVICE_OK;
}

int Skyra::OnInternalModulation(MM::PropertyBase* pProp, MM::ActionType  eAct)
{ 
	std::string answer;

	if (eAct == MM::BeforeGet)
    {
		GetModulation(MODULATION_INTERNAL);
		if (bInternalModulation_) pProp->Set(g_PropertyEnabled);
		else pProp->Set(g_PropertyDisabled);
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(answer);
		if (answer.compare(g_PropertyEnabled) == 0) SetModulation(MODULATION_INTERNAL, true);
		else SetModulation(MODULATION_INTERNAL, false);
    }
    
    return DEVICE_OK;
}
int Skyra::OnLaserHelp1(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
    {
		if (laserStatus_.compare("On") == 0) pProp->Set(g_PropertySkyraHelp3); 
		else if (laserStatus_.compare("Off") == 0) pProp->Set(g_PropertySkyraHelp1); 
    }
	
	return DEVICE_OK;
}

int Skyra::OnLaserHelp2(MM::PropertyBase* pProp , MM::ActionType eAct)
{
	
	if (eAct == MM::BeforeGet)
    {
		if (laserStatus_.compare("On") == 0) pProp->Set(g_PropertySkyraHelp4); 
		else if (laserStatus_.compare("Off") == 0) pProp->Set(g_PropertySkyraHelp2); 
    }

	return DEVICE_OK;
}

int Skyra::OnAllLasers(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    
    std::string answer;
    
	if (eAct == MM::BeforeGet)
    {
       
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(answer);
		if (answer.compare("On") == 0)
        {
            AllLasersOnOff(true);
        }
        else
        {
            AllLasersOnOff(false);
        }
        
    }
    return DEVICE_OK;
} 

int Skyra::OnLaser(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    
    std::string answer;
    
	if (eAct == MM::BeforeGet)
    {

		answer = SerialCommand(currentLaserID_ + "l?"); 
		
		if (answer.compare("1") == 0)
		{
			pProp->Set(g_PropertyOn);
		}
		else
		{
			if (answer.compare("0") == 0) pProp->Set(g_PropertyOff);
		}
	
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(answer);
		if (answer.compare("On") == 0)
        {
            SerialCommand(currentLaserID_ + "l1");  
        }
        else
        {
            SerialCommand(currentLaserID_ + "l0");  
        }
        
    }
    return DEVICE_OK;
} 

int Skyra::OnLaserStatus(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    
    std::string answer;
    
	if (eAct == MM::BeforeGet)
    {
		answer = SerialCommand(currentLaserID_ + "l?"); 
		
		if (answer.compare("1") == 0)
		{
			pProp->Set(g_PropertyOn);
		}
		else
		{
			if (answer.compare("0") == 0) pProp->Set(g_PropertyOff);
		}
    }
    
    return DEVICE_OK;
} 

std::string Skyra::GetPowerSetpoint(long& value)
{
    double result = 0.0;
	std::string answer;
    
	answer = SerialCommand (currentLaserID_ +"p?");
   
	result = atof(answer.c_str()) * 1000;
	value  = (long)result;
    
    return answer;
}

std::string Skyra::SetPowerSetpoint(long requestedPowerSetpoint)
{
    std::string answer;
    std::ostringstream command;
    std::ostringstream setpointString;
	
	float fPower = (float)requestedPowerSetpoint/1000;
	setpointString << fPower;
	if (nSkyra_) command << currentLaserID_;
	command << "p " <<setpointString.str();

	answer = SerialCommand (command.str().c_str());
    
    return answer;
}


std::string Skyra::AutostartStatus() {

	std::string answer;

	answer = SerialCommand("@cobas?");

	if (answer.at(0) == '0')
        autostartStatus_ = "Disabled";
    else if (answer.at(0) == '1')
        autostartStatus_ = "Enabled";

	return answer;
}


std::string Skyra::SetModulation(int modulation, bool value) {

	std::string answer;
    
	if (modulation == MODULATION_STATUS) { 
		bModulationStatus_ = value;
		// can only turn on, but if turned off, turn on constant power as default
		if (bModulationStatus_ ) answer = SerialCommand (currentLaserID_ + "em");
		else {
			answer = "ERROR";
			LogMessage("Modulation can't be switched off, please turn on Constant Power or Constant Current instead", true);
		}
	}

	// Analog and Digital can be active at the same time, but Internal must only be activated by itself
	if (modulation == MODULATION_ANALOG) {
   		bAnalogModulation_ = value;
		if (bAnalogModulation_) {
			SerialCommand (currentLaserID_ + "eswm 0");
			answer = SerialCommand (currentLaserID_ + "sames 1");
		}
		else answer = SerialCommand (currentLaserID_ + "sames 0");
	}
    
	if (modulation == MODULATION_DIGITAL) {
		bDigitalModulation_ = value;
		if (bDigitalModulation_) {
			SerialCommand (currentLaserID_ + "eswm 0");
			answer = SerialCommand (currentLaserID_ + "sdmes 1");
		}
		else  answer = SerialCommand (currentLaserID_ + "sdmes 0");
	}

	if (modulation == MODULATION_INTERNAL) {
		bInternalModulation_ = value;
		if (bInternalModulation_) {
			SerialCommand (currentLaserID_ + "sames 0");
			SerialCommand (currentLaserID_ + "sdmes 0");
			answer = SerialCommand (currentLaserID_ + "eswm 1");
		}
		else answer = SerialCommand (currentLaserID_ + "eswm 0");
		
	}

	return answer;
}
std::string Skyra::GetModulation(int modulation) {

	std::string answer;
    
	if (modulation == MODULATION_STATUS) {
		answer = SerialCommand (currentLaserID_ + "gmes?");
		
		if (answer.at(0) == '1') bModulationStatus_ = true;
		else bModulationStatus_ = false;
	}

	if (modulation == MODULATION_ANALOG) {
   		answer = SerialCommand (currentLaserID_ + "games?");  
		
		if (answer.at(0) == '1') bAnalogModulation_ = true;
		else bAnalogModulation_ = false;
	}
    
	if (modulation == MODULATION_DIGITAL) {
		answer = SerialCommand (currentLaserID_ + "gdmes?");
		
		if (answer.at(0) == '1') bDigitalModulation_ = true;
		else bDigitalModulation_ = false;
	}

	if (modulation == MODULATION_INTERNAL) {
		LogMessage ("CRASH 17");
		answer = SerialCommand (currentLaserID_ + "gswm?");
		
		if (answer.at(0) == '1') bInternalModulation_ = true;
		else bInternalModulation_ = false;
	}

	return answer;
}


std::string Skyra::AnalogImpedanceStatus() {

	std::string answer;
    
    answer = SerialCommand ("galis?");   
    if (answer.at(0) == '0') impedanceStatus_ = g_PropertyDisabled;
    else if (answer.at(0) == '1') impedanceStatus_ = g_PropertyEnabled;

	if (answer.compare(g_Msg_UNSUPPORTED_COMMAND) == 0 ) bImpedance_ = false;
	else bImpedance_ = true;

	return answer;
}
//********************
// Shutter API
//********************

int Skyra::SetOpen(bool open)
{
	//We should have different modes that turn on/off lasers
	std::string answer;

	if (nSkyra_) {
		if (open) answer = SerialCommand(currentLaserID_ + "l1");
		else answer = SerialCommand(currentLaserID_ + "l0");

		return DEVICE_OK;
	}
	else return AllLasersOnOff((int) open);
}

int Skyra::GetOpen(bool& open)
{
    int state;
    int ret = GetState(state);
    if (ret != DEVICE_OK) return ret;
    
    if (state==1)
        open = true;
    else if (state==0)
        open = false;
    else
        return  DEVICE_UNKNOWN_POSITION;
    
    return DEVICE_OK;
}


// ON for deltaT milliseconds
// other implementations of Shutter don't implement this
// is this perhaps because this blocking call is not appropriate

int Skyra::Fire(double deltaT)
{
	SetOpen(true);
	CDeviceUtils::SleepMs((long)(deltaT+.5));
	SetOpen(false);
    return DEVICE_OK;
}

std::string Skyra::SerialCommand (std::string serialCommand) {

	std::ostringstream command;
	std::string answer;

	command << serialCommand;

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), g_SendTerm);
    if (ret != DEVICE_OK) return "Sending Serial Command Failed";
    ret = GetSerialAnswer(port_.c_str(), g_RecvTerm, answer);
    if (ret != DEVICE_OK) return  "Receiving Serial Command Failed";

	if (answer.compare ("Syntax error: illegal command") == 0) answer = g_Msg_UNSUPPORTED_COMMAND;

	return answer;
}

std::string Skyra::GetPort()
{
	std::string port;
	//MMThreadGuard guard(mutex_);
	port = port_;

	return port;
}