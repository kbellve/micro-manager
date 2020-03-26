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
///////////////////////////////////////////////////////////////////////////////
Skyra::Skyra() :
bInitialized_(false),
	bBusy_(false),
	bModulation_(true),
	bModulationStatus_(false),
	bAnalogModulation_(false),
	bDigitalModulation_(false),
	bInternalModulation_(false),
	nSkyra_(0),
	serialNumber_(g_Default_Integer),
	version_(g_Default_Integer),
	hours_(g_Default_Integer),
	keyStatus_("Off"),
	laserCurrent_(g_Default_Integer),
	laserCurrentMinimum_(g_Default_Integer),
	laserPower_(0),
	laserControlMode_(g_Default_ControlMode),
	laserID_(g_Default_Empty),
	laserStatus_(g_Default_String),
	laserType_(g_Default_String),
	autostartStatus_(g_Default_String),
	interlock_ (g_Default_String),
	fault_(g_Default_String),
	identity_(g_Default_String),
	port_(g_Default_String),
	Laser_(NULL)
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
			GetCoreCallback()->SetDeviceProperty(GetPort().c_str(), "DelayBetweenCharsMs", g_Default_Integer);
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

	AllLasersOn(true);
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
	nRet = CreateProperty("All Lasers Help #1", g_PropertySkyraAutostartHelp1, MM::String, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnLaserHelp2);
	nRet = CreateProperty("All Lasers Help #2", g_PropertySkyraAutostartHelp2, MM::String, true, pAct);
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

	// This is where check if this is an actual Skyra, with multipe lasers.
	std::string answer;

	std::stringstream command;

	struct Lasers laser;	

	// get default type if not Skyra
	laserType_ = SerialCommand("glm?");

	for (int x = 1; x < 5; x++) 
	{

		command.str("");
		command << x;
		laser.laserID = command.str();
		command << "glm?";
		answer = SerialCommand(command.str());

		// We do this because there might be IDs available, but without an actual laser installed. 
		// The only way to tell if it is installed is to look at the model number.
		if (answer.compare(g_Default_Integer) != 0){
			command.str("");
			command << x << "glw?";
			answer = SerialCommand(command.str());
			if ((answer).compare(g_Msg_UNSUPPORTED_COMMAND) != 0) {
				nSkyra_++;

				waveLengths_.push_back(answer);
				laser.waveLength = answer;

				laser.laserNumber = nSkyra_; // int version of LaserID

				laser.laserType = SerialCommand(laser.laserID + "glm?");

				laser.currentOn = SerialCommand(laser.laserID + "glc?");

				laser.currentMaximum = SerialCommand(laser.laserID + "gmc?");
				
				laser.currentMinimum = SerialCommand(laser.laserID + "glth?");

				laser.controlMode = g_Default_ControlMode;

				laser.powerOn = 0;
				// get last used power setting and save
				GetPowerOn(laser.powerOn, laser.laserID); 

				LogMessage("Skyra::Initialize: 1b " + std::to_string((_Longlong) laserPower_));

				Skyra_.push_back(laser); 
			} 
		}
	}


	if (nSkyra_ > 0) 
	{
		Laser_ = &Skyra_[0];
		if (Laser_) {
			laserID_				= Laser_->laserID;
			laserType_				= Laser_->laserType;
			laserWavelength_		= Laser_->waveLength;
			laserCurrentOn_			= Laser_->currentOn;
			laserCurrentMaximum_	= Laser_->currentMaximum;
			laserCurrentMinimum_	= Laser_->currentMinimum;
			laserPowerOn_			= Laser_->powerOn;
			laserControlMode_		= Laser_->controlMode;

			pAct = new CPropertyAction (this, &Skyra::OnLaserStatus);
			nRet = CreateProperty(g_PropertySkyraLaserStatus, g_Default_String, MM::String, true, pAct);

			pAct = new CPropertyAction (this, &Skyra::OnLaser);
			nRet = CreateProperty(g_PropertySkyraLaser, g_Default_String, MM::String, false, pAct);

			pAct = new CPropertyAction (this, &Skyra::OnWaveLength);
			nRet = CreateProperty(g_PropertySkyraWavelength, Laser_->waveLength.c_str(), MM::String, false, pAct);
			SetAllowedValues(g_PropertySkyraWavelength, waveLengths_);

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
	} 
	else
	{
		//check if Single Laser supports supports 'em' command, modulation mode
		if (SerialCommand ("em").compare(g_Msg_UNSUPPORTED_COMMAND) == 0) bModulation_ = false;
		else {
			bModulation_ = true;
			//
			// SerialCommand ("cp").compare(g_Msg_UNSUPPORTED_COMMAND); // return to constant power mode, if 'em' worked.
			// 
		}
	}

	pAct = new CPropertyAction (this, &Skyra::OnLaserType);
	nRet = CreateProperty(g_PropertySkyraLaserType,  laserType_.c_str(), MM::String, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	// POWER
	// current setpoint power, not current output power
	pAct = new CPropertyAction (this, &Skyra::OnPower);
	nRet = CreateProperty(g_PropertySkyraPower, g_Default_Integer, MM::Integer, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	// output, not setpoint power
	GetPowerStatus(laserPower_, laserID_);
	pAct = new CPropertyAction (this, &Skyra::OnPowerStatus);
	nRet = CreateProperty(g_PropertySkyraPowerStatus, std::to_string((_Longlong) laserPower_).c_str(), MM::Integer, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	// setpoint power, not current output
	GetPowerOn(laserPower_, laserID_);
	pAct = new CPropertyAction (this, &Skyra::OnPowerOn);
	nRet = CreateProperty(g_PropertySkyraPowerOn, std::to_string((_Longlong) laserPower_).c_str(), MM::Integer, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	CreateProperty("Power: Units", g_PropertySkyraPowerHelp, MM::String, true);
	CreateProperty("Power: On Help", g_PropertySkyraPowerHelpOn, MM::String, true);
		

	/// CURRENT
	pAct = new CPropertyAction (this, &Skyra::OnCurrent);
	nRet = CreateProperty(g_PropertySkyraCurrent, g_Default_Integer, MM::Integer, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnCurrentStatus);
	nRet = CreateProperty(g_PropertySkyraCurrentStatus, g_Default_Integer, MM::Integer, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnCurrentMinimum);
	nRet = CreateProperty(g_PropertySkyraCurrentMinimum, g_Default_Integer, MM::Integer, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &Skyra::OnCurrentOn);
	nRet = CreateProperty(g_PropertySkyraCurrentOn, g_Default_Integer, MM::Integer, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;
 
	pAct = new CPropertyAction (this, &Skyra::OnCurrentMaximum);
	nRet = CreateProperty(g_PropertySkyraCurrentMaximum, g_Default_Integer, MM::Integer, false, pAct);
	if (DEVICE_OK != nRet)
		return nRet;

	CreateProperty("Current: Units", g_PropertySkyraCurrentHelp, MM::String, true);
	CreateProperty("Current: Minimum Help", g_PropertySkyraCurrentHelpMinimum, MM::String, true);
	CreateProperty("Current: Maximum Help", g_PropertySkyraCurrentHelpMaximum, MM::String, true);
	CreateProperty("Current: On Help", g_PropertySkyraCurrentHelpOn, MM::String, true);
		

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
	nRet = CreateProperty(gPropertySkyraControlMode, laserControlMode_.c_str(), MM::String, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	commands.clear();
	commands.push_back("Constant Power");
	commands.push_back("Constant Current");
	if (bModulation_ == true)  commands.push_back("Modulation");
	SetAllowedValues(gPropertySkyraControlMode, commands);  

	//default to Constant Power Mode
	//if (laserControlMode_.compare("Contant Power") == 0) SerialCommand(laserID_ + "cp");
	//else if (laserControlMode_.compare("Contant Current") == 0) SerialCommand(laserID_ + "ci"); 
	//else if (laserControlMode_.compare("Modulation") == 0) SerialCommand(laserID_ + "em"); 

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

	} else {
		// even though this should be only set by OEM, users requested it. - kdb
		// Also, individual lasers in a Skyra doesn't appear to support Autostart, despite documentation saying otherwise :(
		// But make autostart available for those who can't modulate their lasers.
		commands.clear();
		commands.push_back(g_PropertyEnabled);
		commands.push_back(g_PropertyDisabled);
		SetAllowedValues(g_PropertySkyraAutostart, commands);

		AutostartStatus();
		pAct = new CPropertyAction (this, &Skyra::OnAutoStart);
		nRet = CreateProperty(g_PropertySkyraAutostart, autostartStatus_.c_str(), MM::String, false, pAct);
		if (nRet != DEVICE_OK)
			return nRet;

		SetAllowedValues(g_PropertySkyraAutostart, commands);
		pAct = new CPropertyAction (this, &Skyra::OnAutoStartStatus);
		nRet = CreateProperty(g_PropertySkyraAutostartStatus, autostartStatus_.c_str(), MM::String, true, pAct);
		if (DEVICE_OK != nRet)
			return nRet;

		CreateProperty("Autostart: Help", g_PropertySkyraAutostartHelp, MM::String, true);
	}

	nRet = UpdateStatus();
	if (nRet != DEVICE_OK)
		return nRet;

	bInitialized_ = true;

	LogMessage("Skyra::Initialize: Success", true);

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
int Skyra::AllLasersOn(int onoff)
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
std::string Skyra::GetPowerStatus (long &value, std::string laserid = "") 
{
	std::string answer;

	answer = SerialCommand (laserid + "pa?");

	float power = (float)atof(answer.c_str()) * 1000;

	value = (long) power;

	LogMessage ("Skyra::GetPowerStatus 1: " + laserid + " " + answer);

	LogMessage ("Skyra::GetPowerStatus 2: " + laserid + " " + std::to_string((long double)power));

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
int Skyra::OnPowerMax(MM::PropertyBase* /*pProp */, MM::ActionType /*eAct*/)
{

	/*if (eAct == MM::BeforeGet)
	{
	pProp->Set(nMaxPower_);
	}
	else if (eAct == MM::AfterSet)
	{
	pProp->Get(nMaxPower_);
	}
	*/
	return DEVICE_OK;
}
int Skyra::OnControlMode(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	std::string answer;

	if (eAct == MM::BeforeGet)
	{
		pProp->Set(laserControlMode_.c_str());  
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(laserControlMode_);
		if (Laser_) Laser_->controlMode = laserControlMode_;

		if (laserControlMode_.compare("Constant Power") == 0) {
			SerialCommand(laserID_ + "cp");
			Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled);
		}
		else if (laserControlMode_.compare("Constant Current") == 0) {
			SerialCommand(laserID_ + "ci");
			Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled);
		}
		if (laserControlMode_.compare("Modulation") == 0) {
			SerialCommand(laserID_ + "em");
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

	// This property is only shown if an actual Skyra

	std::string answer;

	if (eAct == MM::BeforeGet)
	{	
		answer = SerialCommand(laserID_ + "gla? ");
		if (answer.compare("0") == 0) pProp->Set(g_PropertyInactive);
		if (answer.compare("1") == 0) pProp->Set(g_PropertyActive);
		LogMessage("Skyra::OnActive 1 " + answer);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(answer);
		if (answer.compare(g_PropertyActive) == 0) SerialCommand(laserID_ + "sla 1");
		if (answer.compare(g_PropertyInactive) == 0) SerialCommand(laserID_ + "sla 0");
		LogMessage("Skyra::OnActive 2" + answer);

	}

	return DEVICE_OK;
}
int Skyra::OnPower(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(laserPower_);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(laserPower_);

		if (laserPower_ > 0) laserPowerOn_ = laserPower_; // remember high power value
		if (nSkyra_) 
		{
			if (!Laser_) Laser_ = &Skyra_[0]; 
			if (Laser_) {
				// only set laserPowerHigh_ to anything over zero, because we must have something level to go back to for shutter control
				if (laserPower_ > 0) Laser_->powerOn = laserPower_;
			}
		}
		// Switch to constant power mode if not already set that way
		if (laserControlMode_.compare("Constant Power")  != 0) 
			Skyra::SetProperty(gPropertySkyraControlMode,"Constant Power");

		SetPower(laserPower_,laserID_);
	}
	return DEVICE_OK;
}
int Skyra::OnPowerOn(MM::PropertyBase* pProp, MM::ActionType eAct )
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(laserPowerOn_);
		LogMessage("Skyra::OnPowerOn " + std::to_string((_Longlong)laserPowerOn_));
	} 

	return DEVICE_OK;
}
int Skyra::OnPowerStatus(MM::PropertyBase* pProp, MM::ActionType  eAct)
{
	long answer;
	if (eAct == MM::BeforeGet)
	{
		GetPowerStatus(answer,laserID_);
		pProp->Set(answer);
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
	if (Laser_) pProp->Set(Laser_->laserType.c_str());
	else  pProp->Set(laserType_.c_str());

	return DEVICE_OK;
}
int Skyra::OnVersion(MM::PropertyBase* pProp, MM::ActionType /* eAct */)
{

	pProp->Set(version_.c_str());

	return DEVICE_OK;
}
int Skyra::OnCurrentStatus(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	std::string answer;

	if (eAct == MM::BeforeGet)
	{
		answer = SerialCommand(laserID_ + "i? ");
		pProp->Set(answer.c_str());
	}
	return DEVICE_OK;
}
int Skyra::OnCurrent(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	LogMessage("Skyra::OnCurrent dddd ", false);
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(laserCurrent_.c_str());

	} else 
	{
		if (eAct == MM::AfterSet)
		{
			// Switch to Current regulated mode and then set Current Level
			Skyra::SetProperty(gPropertySkyraControlMode,"Constant Current");
			Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled);
			bModulationStatus_ = false;

			pProp->Get(laserCurrent_);
			LogMessage("Skyra::OnCurrent " + laserCurrent_,false);
			//reset laserCurrentOn_ to new value, assuming not 0.0. Need to do this for Shuttering.
			if (laserCurrent_.compare(g_Default_Integer) != 0) {
				laserCurrentOn_ = laserCurrent_;
				// Save  new current setting in vector, important if using as a shutter
				if (nSkyra_) {
					if (!Laser_) Laser_ = &Skyra_[0]; 
					if (Laser_) {
						Laser_->currentOn = laserCurrentOn_;
					}
				}
			}
			SetCurrent(atoi(laserCurrent_.c_str()));
		}
	}
	return DEVICE_OK;
}
int Skyra::OnCurrentOn(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		LogMessage("OnCurrentOn: " + laserCurrentOn_);
		pProp->Set(laserCurrentOn_.c_str());
	} 
	return DEVICE_OK;
}
int Skyra::OnCurrentMaximum(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		LogMessage("OnCurrentMaximum: " + laserCurrentMaximum_);
		pProp->Set(laserCurrentMaximum_.c_str());
	} else 
	{
		if (eAct == MM::AfterSet)
		{
			pProp->Get(laserCurrentMaximum_);
			LogMessage("Skyra::OnCurrentMaximum " + laserCurrentMaximum_,false);
			if (laserCurrentMaximum_.compare(g_Default_Integer) != 0) {
			// Save  new current setting in vector, important if using as a shutter
				if (nSkyra_) {
					if (!Laser_) Laser_ = &Skyra_[0]; 
					if (Laser_) {
						Laser_->currentMaximum = laserCurrentMaximum_;
					}
				}
			}
			SerialCommand (laserID_ + "smc " + laserCurrentMaximum_);
		}
	}
	return DEVICE_OK;
}
int Skyra::OnCurrentMinimum(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		MM::Property* pChildProperty = (MM::Property*)pProp;

		if (laserType_.compare("DPL1") == 0) 
			pChildProperty->SetReadOnly(false);
			
		else {
			pChildProperty->SetReadOnly(true);
			laserCurrentMinimum_ = "0";
		}

		pProp->Set(laserCurrentMinimum_.c_str());
	} else 
		if (eAct == MM::AfterSet)
		{
			pProp->Get(laserCurrentMinimum_);
			// Save  new current setting in vector, important if using as a shutter
			// Will use this value to set the laser to this power when closed shutter
			if (nSkyra_) {
				if (!Laser_) Laser_ = &Skyra_[0]; 
				if (Laser_) {
					Laser_->currentMinimum = laserCurrentMinimum_;
				}
			}
			SerialCommand (laserID_ + "slth " + laserCurrentMinimum_);
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

	// Only shown if Skyra, not a normal Cobolt laser.

	//if (!Laser_) Laser_ = &Skyra_.front();
	//if (!Laser_) return ERR_DEVICE_NOT_FOUND; 

	if (eAct == MM::BeforeGet)
	{
		if (Laser_) {
			laserWavelength_ = Laser_->waveLength; 
		}
		pProp->Set(laserWavelength_.c_str()); 
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(laserWavelength_);

		std::string answer,value;

		for (unsigned i=0; i<Skyra_.size(); ++i) {
			Laser_ = &Skyra_[i];
			if (Laser_->waveLength.compare(laserWavelength_) == 0) 
			{

				laserID_ = Laser_->laserID;;
				laserType_ = Laser_->laserType; 
				laserWavelength_ = Laser_->waveLength;

				laserCurrentOn_ = Laser_->currentOn;
				laserCurrentMinimum_ = Laser_->currentMinimum;
				laserCurrentMaximum_ = Laser_->currentMaximum;

				laserPower_ = Laser_->powerOn;

				LogMessage("Laser ID: " + laserID_);
				LogMessage("Laser Type: " + laserType_);
				LogMessage("Laser Current: " + laserCurrent_);
				LogMessage("Laser Current Off: " + laserCurrentMinimum_);
				LogMessage("Laser Current Maximum: " + laserCurrentMaximum_);
				LogMessage("Laser Power: " + std::to_string((_Longlong)laserPower_));

				// update Current Output from laser
				Skyra::SetProperty(g_PropertySkyraCurrentStatus,SerialCommand(laserID_ + "i? ").c_str());

				// update Current Setting from saved Lasers Struct, not laser itself.
				Skyra::SetProperty(g_PropertySkyraCurrentOn,laserCurrentOn_.c_str());

				// update Power Output from laser
				Skyra::SetProperty(g_PropertySkyraPowerStatus,SerialCommand(laserID_ + "pa? ").c_str());

				// update Power Setting from saved Lasers Struct, not laser itself.
				Skyra::SetProperty(g_PropertySkyraPowerOn,std::to_string((_Longlong)laserPowerOn_).c_str());

				// update active status
				value = SerialCommand(laserID_ + "gla? ");
				if (value.compare("0") == 0) Skyra::SetProperty(g_PropertySkyraActiveStatus, g_PropertyInactive);
				else if (value.compare("1") == 0) Skyra::SetProperty(g_PropertySkyraActiveStatus, g_PropertyActive);
				// update modulation status

				bModulationStatus_ = GetModulation(MODULATION_STATUS);
				if (bModulationStatus_) Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyEnabled);
				else Skyra::SetProperty(g_PropertySkyraModulationStatus, g_PropertyDisabled);

				bAnalogModulation_ = GetModulation(MODULATION_ANALOG);
				if (bAnalogModulation_) Skyra::SetProperty(g_PropertySkyraAnalogModulation, g_PropertyEnabled);
				else Skyra::SetProperty(g_PropertySkyraAnalogModulation, g_PropertyDisabled);

				bDigitalModulation_ = GetModulation(MODULATION_DIGITAL);
				if (bDigitalModulation_) Skyra::SetProperty(g_PropertySkyraDigitalModulation, g_PropertyEnabled);
				else Skyra::SetProperty(g_PropertySkyraDigitalModulation, g_PropertyDisabled);

				bInternalModulation_ = GetModulation(MODULATION_INTERNAL);
				if (bInternalModulation_) Skyra::SetProperty(g_PropertySkyraInternalModulation, g_PropertyEnabled);
				else Skyra::SetProperty(g_PropertySkyraInternalModulation, g_PropertyDisabled);

				break;
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
		bModulationStatus_ = GetModulation(MODULATION_STATUS);

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
		bAnalogModulation_ = GetModulation(MODULATION_ANALOG);
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
		bDigitalModulation_ = GetModulation(MODULATION_DIGITAL);
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
		bInternalModulation_ = GetModulation(MODULATION_INTERNAL);
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
		if (laserStatus_.compare("On") == 0) pProp->Set(g_PropertySkyraAutostartHelp3); 
		else if (laserStatus_.compare("Off") == 0) pProp->Set(g_PropertySkyraAutostartHelp1); 
	}

	return DEVICE_OK;
}
int Skyra::OnLaserHelp2(MM::PropertyBase* pProp , MM::ActionType eAct)
{	
	if (eAct == MM::BeforeGet)
	{
		if (laserStatus_.compare("On") == 0) pProp->Set(g_PropertySkyraAutostartHelp4); 
		else if (laserStatus_.compare("Off") == 0) pProp->Set(g_PropertySkyraAutostartHelp2); 
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
			AllLasersOn(true);
		}
		else
		{
			AllLasersOn(false);
		}

	}
	return DEVICE_OK;
} 
int Skyra::OnLaser(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	// This property is only shown if an actual Skyra

	std::string answer;

	if (eAct == MM::BeforeGet)
	{
		answer = SerialCommand(laserID_ + "l?"); 

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
			SerialCommand(laserID_ + "l1");  
		}
		else
		{
			SerialCommand(laserID_ + "l0");  
		}

	}
	return DEVICE_OK;
} 
int Skyra::OnLaserStatus(MM::PropertyBase* pProp, MM::ActionType eAct)
{

	std::string answer;

	if (eAct == MM::BeforeGet)
	{
		answer = SerialCommand(laserID_ + "l?"); 

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
///////////////////////////////////////////////////////////////////////////////
// Base Calls  
///////////////////////////////////////////////////////////////////////////////
/*
std::string Skyra::GetCurrentSetting() 
{
// returns in mAmps.
std::string answer;

answer = SerialCommand (laserID_ +"glc?");
if (answer.compare(g_Default_Float) != 0) 
{
laserCurrentOn_ = answer;
if (Laser_) Laser_->currentOn = answer;
}

LogMessage("Skyra::GetCurrentSetting() "  + answer);
return answer;

} */
/*
std::string Skyra::GetCurrent() 
{

std::string answer;

answer = SerialCommand(laserID_ + "i? ");

return answer;

}
*/
std::string Skyra::SetCurrent(long Current)
{
	std::string answer;
	std::ostringstream command;
	std::ostringstream setpointString;

	if (nSkyra_) command << laserID_;
	command << "slc " << Current;
	answer = SerialCommand (command.str().c_str());
	
	LogMessage("Skyra::SetCurrent: " + command.str(), true);

	return answer;
}
std::string Skyra::GetPowerOn(long& value, std::string laserid = "")
{
	// returns in Watts
	double result = 0.0;
	std::string answer;

	answer = SerialCommand (laserid +"p?");

	LogMessage("Skyra::GetPowerOn: " + laserid + " " + answer, true);

	result = atof(answer.c_str()) * 1000;
	value  = (long)result;

	return answer;
}
std::string Skyra::SetPower(long requestedPowerSetpoint, std::string laserid = "")
{
	std::string answer;
	std::ostringstream command;
	std::ostringstream setpointString;

	float fPower = (float)requestedPowerSetpoint/1000;
	setpointString << fPower;
	if (nSkyra_) command << laserID_;

	command << "p " <<setpointString.str();

	answer = SerialCommand (command.str().c_str());

	LogMessage("SetPower: " + command.str());

	return answer;
}
std::string Skyra::AutostartStatus() {

	std::string answer;

	answer = SerialCommand("@cobas?");

	if (answer.at(0) == '0')
		autostartStatus_ = g_PropertyDisabled;
	else if (answer.at(0) == '1')
		autostartStatus_ = g_PropertyEnabled;

	return answer;
}
std::string Skyra::SetModulation(int modulation, bool value) 
{

	std::string answer;

	if (modulation == MODULATION_STATUS) { 
		bModulationStatus_ = value;
		// can only turn on, but if turned off, turn on constant power as default
		if (bModulationStatus_ ) answer = SerialCommand (laserID_ + "em");
		else {
			answer = "ERROR";
			LogMessage("Modulation can't be switched off, please turn on Constant Power or Constant Current instead", true);
		}
	}

	// Analog and Digital can be active at the same time, but Internal must only be activated by itself
	if (modulation == MODULATION_ANALOG) {
		bAnalogModulation_ = value;
		if (bAnalogModulation_) {
			SerialCommand (laserID_ + "eswm 0");
			answer = SerialCommand (laserID_ + "sames 1");
		}
		else answer = SerialCommand (laserID_ + "sames 0");
	}

	if (modulation == MODULATION_DIGITAL) {
		bDigitalModulation_ = value;
		if (bDigitalModulation_) {
			SerialCommand (laserID_ + "eswm 0");
			answer = SerialCommand (laserID_ + "sdmes 1");
		}
		else  answer = SerialCommand (laserID_ + "sdmes 0");
	}

	if (modulation == MODULATION_INTERNAL) {
		bInternalModulation_ = value;
		if (bInternalModulation_) {
			SerialCommand (laserID_ + "sames 0");
			SerialCommand (laserID_ + "sdmes 0");
			answer = SerialCommand (laserID_ + "eswm 1");
		}
		else answer = SerialCommand (laserID_ + "eswm 0");

	}

	return answer;
}
bool Skyra::GetModulation(int modulation) {

	std::string answer;

	if (modulation == MODULATION_ANALOG || modulation == MODULATION_STATUS) {
		answer = SerialCommand (laserID_ + "games?");  

		if (answer.at(0) == '1') return true;
	}

	if (modulation == MODULATION_DIGITAL || modulation == MODULATION_STATUS) {
		answer = SerialCommand (laserID_ + "gdmes?");

		if (answer.at(0) == '1') return true;
	}

	if (modulation == MODULATION_INTERNAL || modulation == MODULATION_STATUS) {
		answer = SerialCommand (laserID_ + "gswm?");

		if (answer.at(0) == '1') return true;
	}

	return false;
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

	if (bModulation_) {
		// if we are using a shutter and we have a laser that can be modulated, use Constant Current
		if (laserControlMode_.compare("Constant Current") !=0) { 
			SerialCommand(laserID_ + "ci");
			laserControlMode_ = "Constant Current";
		}

		if (open) {
			// return to last Current Setting
			//answer = SerialCommand(laserID_ + "slc " + laserCurrentOn_);
			answer = SerialCommand(laserID_ + "slc " + laserCurrentMaximum_);
		}
		else answer = SerialCommand(laserID_ + "slc " + laserCurrentMinimum_);

		return DEVICE_OK;
	}
	else return AllLasersOn((int) open);
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