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
         GetCoreCallback()->SetDeviceProperty(GetPort().c_str(), MM::g_Keyword_Handshaking, g_Off);
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
    commands.push_back(g_Off);
    commands.push_back(g_On);
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

		pAct = new CPropertyAction (this, &Skyra::OnActiveStatus);
		nRet = CreateProperty(g_PropertySkyraActiveStatus, g_Default_String, MM::String, true, pAct);
		
		commands.clear();
		commands.push_back(g_PropertyActive);
		commands.push_back(g_PropertyInactive);
		SetAllowedValues(g_PropertySkyraActive, commands);
		SetAllowedValues(g_PropertySkyraActiveStatus, commands);

		commands.clear();
		commands.push_back(g_On);
		commands.push_back(g_Off);
		SetAllowedValues(g_PropertySkyraLaser, commands);
	}

	//int nRet =  CreateProperty("Minimum Laser Power (mW)", "0", MM::Integer, true);
	//if (DEVICE_OK != nRet)
    //return nRet;
		
	//pAct = new CPropertyAction (this, &Skyra::OnPowerSetMax);
	//nRet = CreateProperty("Maximum Laser Power (mW)", "100", MM::Integer, true, pAct);
	//if (DEVICE_OK != nRet)
    //return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnPower);
	nRet = CreateProperty(g_PropertySkyraPower, "0", MM::Integer, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	GetPower(nPower_);
	pAct = new CPropertyAction (this, &Skyra::OnPowerStatus);
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
		commands.push_back("Enabled");
		commands.push_back("Disabled");
		SetAllowedValues(g_PropertySkyraAnalogImpedance, commands);

		pAct = new CPropertyAction (this, &Skyra::OnAnalogImpedanceStatus);
		nRet = CreateProperty(g_PropertySkyraAnalogImpedanceStatus, impedanceStatus_.c_str(), MM::String, true, pAct);
		if (DEVICE_OK != nRet)
			return nRet;
	}

	// check if Laser supports supports modulation
	if (SerialCommand ("em").compare(g_Msg_UNSUPPORTED_COMMAND) == 0) bModulation_ = false;
	else bModulation_ = true;

	// Constant Power (Default) or Constant Current
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

	if (bModulation_ == true) {
	// Modulation
		ModulationStatus();
		pAct = new CPropertyAction (this, &Skyra::OnModulation);
		CreateProperty(g_PropertySkyraModulation, modulationStatus_.c_str(), MM::String, false, pAct);
	
		commands.clear();
		commands.push_back("Analog");
		commands.push_back("Digital");
		SetAllowedValues(g_PropertySkyraModulation, commands);

		pAct = new CPropertyAction (this, &Skyra::OnModulationStatus);
		nRet = CreateProperty(g_PropertySkyraModulationStatus, modulationStatus_.c_str(), MM::String, true, pAct);
		if (DEVICE_OK != nRet)
			return nRet;
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

std::string Skyra::GetPower (long &value ) 
{
	std::string answer;
    
	if (nSkyra_) answer = SerialCommand (currentLaserID_ + "pa?");
	else answer = SerialCommand (currentLaserID_ + "pa?");
   	
	float power = (float)atof(answer.c_str()) * 1000;

	value = (long) power;

	//LogMessage ("POWER: " + answer);

	//LogMessage ("POWER: " + std::to_string(_Longlong)power));

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
		   SerialCommand("cp");
	   }
	   else if (controlMode_.compare("Constant Current") == 0) {
		   SerialCommand("ci");
	   }
	   if (controlMode_.compare("Modulation") == 0) {
		   SerialCommand("em");
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
	
	if (eAct == MM::BeforeGet)
    {
       if (nSkyra_) {
			std::string answer;
			answer = SerialCommand(currentLaserID_ + "gla? ");
			if (answer.compare("0") == 0) pProp->Set(g_PropertyInactive);
			if (answer.compare("1") == 0) pProp->Set(g_PropertyActive);
	   }
    }
	else if (eAct == MM::AfterSet)
    {
       if (nSkyra_) {
			std::string answer;
			pProp->Get(answer);
			if (answer.compare(g_PropertyActive) == 0) SerialCommand(currentLaserID_ + "sla 1");
			if (answer.compare(g_PropertyInactive) == 0) SerialCommand(currentLaserID_ + "sla 0");
	   }
    }
    
    return DEVICE_OK;
}

int Skyra::OnActiveStatus(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	
    if (eAct == MM::BeforeGet)
    {
       if (nSkyra_) {
			std::string answer;
			answer = SerialCommand(currentLaserID_ + "gla? ");
			if (answer.compare("0") == 0) pProp->Set(g_PropertyInactive);
			if (answer.compare("1") == 0) pProp->Set(g_PropertyActive);
	   }
    }
    
    return DEVICE_OK;
}

int Skyra::OnPower(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
    
    if (eAct == MM::BeforeGet)
    {
        //GetPowerSetpoint(Power);
       // pProp->Set(Power);
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


int Skyra::OnPowerSetMax(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    pProp->Set(nMaxPower_);
    return DEVICE_OK;
}

int Skyra::OnPowerStatus(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	if (eAct == MM::BeforeGet)
    {
		GetPower(nPower_);
		pProp->Set(nPower_);
	} else if (eAct == MM::AfterSet)
    {

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
    
	if (nSkyra_) current_ = SerialCommand(currentLaserID_ + "i? ");
	else current_ =  SerialCommand("i? ");

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
		for (it = waveLengths_.begin(); it != waveLengths_.end(); it++) {
			answer = *it;
			if (answer.compare(currentLaser_) == 0) {
				index = std::distance(waveLengths_.begin(), it); 
				currentLaserID_ = IDs_.at(index);
				LogMessage("Current ID: " + currentLaserID_);
				currentLaserType_ = laserTypes_.at(index);
				LogMessage("Current Type: " + currentLaserType_);

				// update current
				current_ = SerialCommand(currentLaserID_ + "i? ");
				Skyra::SetProperty(g_PropertySkyraCurrentOutput,current_.c_str());

				LogMessage("Current Type: " + GetPower(nPower_));
				GetPower(nPower_);
				Skyra::SetProperty(g_PropertySkyraPowerOutput,std::to_string((_Longlong)nPower_).c_str());

				// update active status
				value = SerialCommand(currentLaserID_ + "gla? ");
				if (value.compare("0") == 0) Skyra::SetProperty(g_PropertySkyraActive, g_PropertyInactive);
				else if (value.compare("1") == 0) Skyra::SetProperty(g_PropertySkyraActive, g_PropertyActive);
			}
		}
	}
    
    return DEVICE_OK;
}

int Skyra::OnModulation(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
    
    if (eAct == MM::BeforeGet)
    {
      
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(modulationStatus_);
		
		if (modulationStatus_.compare("Analog") == 0) {
			// From Manual:
			//		Analog and digital modulation states are independent. 
			//		To switch from analog to digital modulation it is necessary to disable analog and enable digital.
			
			SerialCommand("sdmes 0");
			SerialCommand("sames 1");
	   }
	   else if (modulationStatus_.compare("Digital") == 0) {
			SerialCommand("sames 0");
			SerialCommand("sdmes 1");
	   }
    }
    
    return DEVICE_OK;
}

int Skyra::OnModulationStatus(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{
    ModulationStatus();
    
    pProp->Set(modulationStatus_.c_str());
    
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
		if (nSkyra_) {
			answer = SerialCommand(currentLaserID_ + "l?"); 
		
			if (answer.compare("1") == 0)
			{
				pProp->Set(g_On);
			}
			else
			{
				if (answer.compare("0") == 0) pProp->Set(g_Off);
			}
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
		if (nSkyra_) {
			answer = SerialCommand(currentLaserID_ + "l?"); 
		
			if (answer.compare("1") == 0)
			{
				pProp->Set(g_On);
			}
			else
			{
				if (answer.compare("0") == 0) pProp->Set(g_Off);
			}
		}
    }
    
    return DEVICE_OK;
} 

int Skyra::SetPowerSetpoint(long requestedPowerSetpoint)
{
    std::string answer;
    std::ostringstream command;
    std::ostringstream setpointString;
	
	float fPower = (float)requestedPowerSetpoint/1000;
	setpointString << fPower;
	if (nSkyra_) command << currentLaserID_;
	command << "p " <<setpointString.str();

	answer = SerialCommand (command.str().c_str());
    
    return DEVICE_OK;
}


int Skyra::GetPowerSetpoint(long& value)
{
    std::string answer;
    
    if (nSkyra_) answer = currentLaserID_;
	answer += SerialCommand ("p?");
   
	value  = (long)atof(answer.c_str()) * 1000;
    
    return DEVICE_OK;
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

std::string Skyra::ModulationStatus() {

	std::string answer;
    
	modulationStatus_ = g_Msg_UNSUPPORTED_COMMAND;

    answer = SerialCommand ("games?");   
    if (answer.at(0) == '1') modulationStatus_ = "Analog";
    
	answer = SerialCommand ("gdmes?");
	if (answer.at(0) == '1') modulationStatus_ = "Digital";

	return answer;
}


std::string Skyra::AnalogImpedanceStatus() {

	std::string answer;
    
    answer = SerialCommand ("galis?");   
    if (answer.at(0) == '0') impedanceStatus_ = "Disabled";
    else if (answer.at(0) == '1') impedanceStatus_ = "Enabled";

	if (answer.compare(g_Msg_UNSUPPORTED_COMMAND) == 0 ) bImpedance_ = false;
	else bImpedance_ = true;

	return answer;
}
//********************
// Shutter API
//********************

int Skyra::SetOpen(bool open)
{
    return AllLasersOnOff((int) open);
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