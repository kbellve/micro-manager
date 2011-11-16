///////////////////////////////////////////////////////////////////////////////
// FILE:          ASIStage.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   ASIStage adapter
// COPYRIGHT:     University of California, San Francisco, 2007
//                All rights reserved
//
// LICENSE:       This library is free software; you can redistribute it and/or
//                modify it under the terms of the GNU Lesser General Public
//                License as published by the Free Software Foundation.
//                
//                You should have received a copy of the GNU Lesser General Public
//                License along with the source distribution; if not, write to
//                the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
//                Boston, MA  02111-1307  USA
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
//
// AUTHOR:        Jizhen Zhao (j.zhao@andor.com) based on code by Nenad Amodaj, April 2007, modified by Nico Stuurman, 12/2007
// MAINTAINER     Maintained by Nico Stuurman (nico@cmp.ucsf.edu)
//

#ifdef WIN32
#define snprintf _snprintf 
#pragma warning(disable: 4355)
#endif

#include "ASIStage.h"
#include <cstdio>
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include "../../MMDevice/DeviceUtils.h"
#include <sstream>


#include <iostream>
using namespace std;

const char* g_XYStageDeviceName = "XYStage";
const char* g_ZStageDeviceName = "ZStage";
const char* g_CRIFDeviceName = "CRIF";
const char* g_CRISPDeviceName = "CRISP";
const char* g_AZ100TurretName = "AZ100 Turret";
const char* g_LEDName = "LED";
const char* g_Open = "Open";
const char* g_Closed = "Closed";

// CRIF states
const char* g_CRIFState = "CRIF State";
const char* g_CRIF_I = "Unlock (Laser Off)";
const char* g_CRIF_L = "Laser On";
const char* g_CRIF_Cal = "Calibrate";
const char* g_CRIF_G = "Calibration Succeeded";
const char* g_CRIF_B = "Calibration Failed";
const char* g_CRIF_k = "Locking";
const char* g_CRIF_K = "Lock";
const char* g_CRIF_E = "Error";
const char* g_CRIF_O = "Laser Off";

// CRISP states
const char* g_CRISPState = "CRISP State";
const char* g_CRISP_I = "Idle";
const char* g_CRISP_R = "Ready";
const char* g_CRISP_D = "Dim";
const char* g_CRISP_K = "Lock";
const char* g_CRISP_F = "In Focus";
const char* g_CRISP_N = "Inhibit";
const char* g_CRISP_E = "Error";
const char* g_CRISP_G = "loG_cal";
const char* g_CRISP_SG = "gain_Cal";
const char* g_CRISP_Cal = "Calibrating";
const char* g_CRISP_f = "Dither";
const char* g_CRISP_C = "Curve";
const char* g_CRISP_B = "Balance";
const char* g_CRISP_RFO = "Reset Focus Offset";

using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
   AddAvailableDeviceName(g_ZStageDeviceName, "Add-on Z-stage");
   AddAvailableDeviceName(g_XYStageDeviceName, "XY Stage");
   AddAvailableDeviceName(g_CRIFDeviceName, "CRIF");
   AddAvailableDeviceName(g_CRISPDeviceName, "CRISP");
   AddAvailableDeviceName(g_AZ100TurretName, "AZ100 Turret");
   AddAvailableDeviceName(g_LEDName, "LED");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   if (strcmp(deviceName, g_ZStageDeviceName) == 0)
   {
      ZStage* s = new ZStage();
      return s;
   }
   else if (strcmp(deviceName, g_XYStageDeviceName) == 0)
   {
      XYStage* s = new XYStage();
      return s;
   }
   else if (strcmp(deviceName, g_CRIFDeviceName) == 0)
   {
      return  new CRIF();
   }
   else if (strcmp(deviceName, g_CRISPDeviceName) == 0)
   {
      return  new CRISP();
   }
   else if (strcmp(deviceName, g_AZ100TurretName) == 0)
   {
      return  new AZ100Turret();
   }
   else if (strcmp(deviceName, g_LEDName) == 0)
   {
      return  new LED();
   }

   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}


MM::DeviceDetectionStatus ASICheckSerialPort(MM::Device& device, MM::Core& core, std::string portToCheck, double answerTimeoutMs)
{
   // all conditions must be satisfied...
   MM::DeviceDetectionStatus result = MM::Misconfigured;
   char answerTO[MM::MaxStrLength];

   try
   {
      std::string portLowerCase = portToCheck;
      for( std::string::iterator its = portLowerCase.begin(); its != portLowerCase.end(); ++its)
      {
         *its = (char)tolower(*its);
      }
      if( 0< portLowerCase.length() &&  0 != portLowerCase.compare("undefined")  && 0 != portLowerCase.compare("unknown") )
      {
         result = MM::CanNotCommunicate;
         core.GetDeviceProperty(portToCheck.c_str(), "AnswerTimeout", answerTO);
         // device specific default communication parameters
         // for ASI Stage
         core.SetDeviceProperty(portToCheck.c_str(), MM::g_Keyword_Handshaking, "Off");
         core.SetDeviceProperty(portToCheck.c_str(), MM::g_Keyword_StopBits, "1");
         std::ostringstream too;
         too << answerTimeoutMs;
         core.SetDeviceProperty(portToCheck.c_str(), "AnswerTimeout", too.str().c_str());
         core.SetDeviceProperty(portToCheck.c_str(), "DelayBetweenCharsMs", "0");
         MM::Device* pS = core.GetDevice(&device, portToCheck.c_str());
         std::vector< std::string> possibleBauds;
         possibleBauds.push_back("9600");
         for( std::vector< std::string>::iterator bit = possibleBauds.begin(); bit!= possibleBauds.end(); ++bit )
         {
            core.SetDeviceProperty(portToCheck.c_str(), MM::g_Keyword_BaudRate, (*bit).c_str() );
            pS->Initialize();
            core.PurgeSerial(&device, portToCheck.c_str());
            // check status
            const char* command = "/";
            int ret = core.SetSerialCommand( &device, portToCheck.c_str(), command, "\r");
            if( DEVICE_OK == ret)
            {
               char answer[MM::MaxStrLength];

               ret = core.GetSerialAnswer(&device, portToCheck.c_str(), MM::MaxStrLength, answer, "\r\n");
               if( DEVICE_OK != ret )
               {
                  char text[MM::MaxStrLength];
                  device.GetErrorText(ret, text);
                  core.LogMessage(&device, text, true);
               }
               else
               {
                  // to succeed must reach here....
                  result = MM::CanCommunicate;
               }
            }
            else
            {
               char text[MM::MaxStrLength];
               device.GetErrorText(ret, text);
               core.LogMessage(&device, text, true);
            }
            pS->Shutdown();
            if( MM::CanCommunicate == result)
               break;
            else
               // try to yield to GUI
               CDeviceUtils::SleepMs(10);
         }
         // always restore the AnswerTimeout to the default
         core.SetDeviceProperty(portToCheck.c_str(), "AnswerTimeout", answerTO);
      }
   }
   catch(...)
   {
      core.LogMessage(&device, "Exception in DetectDevice!",false);
   }
   return result;
}


///////////////////////////////////////////////////////////////////////////////
// ASIBase (convenience parent class)
//
ASIBase::ASIBase(MM::Device *device, const char *prefix) :
   initialized_(false),
   oldstagePrefix_(prefix),
   port_("Undefined")
{
   device_ = static_cast<ASIDeviceBase *>(device);
}

ASIBase::~ASIBase()
{
}

// Communication "clear buffer" utility function:
int ASIBase::ClearPort(void)
{
   // Clear contents of serial port
   const int bufSize = 255;
   unsigned char clear[bufSize];
   unsigned long read = bufSize;
   int ret;
   while ((int) read == bufSize)
   {
      ret = device_->ReadFromComPort(port_.c_str(), clear, bufSize, read);
      if (ret != DEVICE_OK)
         return ret;
   }
   return DEVICE_OK;                                                           
} 

// Communication "send" utility function:
int ASIBase::SendCommand(const char *command) const
{
   std::string base_command = "";
   int ret;

   if (oldstage_)
      base_command += oldstagePrefix_;
   base_command += command;
   // send command
   ret = device_->SendSerialCommand(port_.c_str(), base_command.c_str(), "\r");
   return ret;
}

// Communication "send & receive" utility function:
int ASIBase::QueryCommand(const char *command, std::string &answer) const
{
   const char *terminator;
   int ret;

   // send command
   ret = SendCommand(command);
   if (ret != DEVICE_OK)
      return ret;
   // block/wait for acknowledge (or until we time out)
   if (oldstage_)
      terminator = "\r\n\3";
   else
      terminator = "\r\n";
   ret = device_->GetSerialAnswer(port_.c_str(), terminator, answer);
   return ret;
}

// Communication "send, receive, and look for acknowledgement" utility function:
int ASIBase::QueryCommandACK(const char *command)
{
   std::string answer;
   int ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK)
      return ret;

   // The controller only acknowledges receipt of the command
   if (answer.substr(0,2) != ":A")
      return ERR_UNRECOGNIZED_ANSWER;

   return DEVICE_OK;
}

// Communication "test device type" utility function:
int ASIBase::CheckDeviceStatus(void)
{
   const char* command = "/"; // check STATUS
   std::string answer;
   int ret;

   // send status command (test for new protocol)
   oldstage_ = false;
   ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK && !oldstagePrefix_.empty())
   {
      // send status command (test for older LX-4000 protocol)
      oldstage_ = true;
      ret = QueryCommand(command, answer);
   }
   return ret;
}


///////////////////////////////////////////////////////////////////////////////
// XYStage
//
XYStage::XYStage() :
   CXYStageBase<XYStage>(),
   ASIBase(this, "2H"),
   stepSizeXUm_(0.0), 
   stepSizeYUm_(0.0), 
   ASISerialUnit_(10.0),
   motorOn_(true),
   joyStickSpeedFast_(60),
   joyStickSpeedSlow_(5),
   joyStickMirror_(false),
   joyStickSwapXY_(false),
   nrMoveRepetitions_(0),
   answerTimeoutMs_(1000)
{
   InitializeDefaultErrorMessages();

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_XYStageDeviceName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "ASI XY stage driver adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &XYStage::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
   stopSignal_ = false;
}

XYStage::~XYStage()
{
   Shutdown();
}

void XYStage::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_XYStageDeviceName);
}


MM::DeviceDetectionStatus XYStage::DetectDevice(void)
{

   return ASICheckSerialPort(*this,*GetCoreCallback(), port_, answerTimeoutMs_);
}

int XYStage::Initialize()
{
   // empty the Rx serial buffer before sending command
   ClearPort(); 

   CPropertyAction* pAct = new CPropertyAction (this, &XYStage::OnVersion);
   CreateProperty("Version", "", MM::String, true, pAct);

   // check status first (test for communication protocol)
   int ret = CheckDeviceStatus();
   if (ret != DEVICE_OK)
      return ret;

   // Most ASIStages have the origin in the top right corner, the following reverses direction of the X-axis:
   ret = SetAxisDirection();
   if (ret != DEVICE_OK)
      return ret;

   // set stage step size and resolution
   /**
    * NOTE:  ASI return numbers in 10th of microns with an extra decimal place
	* To convert into steps, we multiply by 10 (variable ASISerialUnit_) making the step size 0.01 microns
	*/
   stepSizeXUm_ = 0.01;
   stepSizeYUm_ = 0.01;

   // Step size
   pAct = new CPropertyAction (this, &XYStage::OnStepSizeX);
   CreateProperty("StepSizeX_um", "0.0", MM::Float, true, pAct);
   pAct = new CPropertyAction (this, &XYStage::OnStepSizeY);
   CreateProperty("StepSizeY_um", "0.0", MM::Float, true, pAct);

   // Wait cycles
   if (hasCommand("WT X?")) {
      pAct = new CPropertyAction (this, &XYStage::OnWait);
      CreateProperty("Wait_Cycles", "5", MM::Integer, false, pAct);
      SetPropertyLimits("Wait_Cycles", 0, 255);
   }

   // Speed (sets both x and y)
   if (hasCommand("S X?")) {
      pAct = new CPropertyAction (this, &XYStage::OnSpeed);
      CreateProperty("Speed-S", "1", MM::Float, false, pAct);
   }

   // Backlash (sets both x and y)
   if (hasCommand("B X?")) {
      pAct = new CPropertyAction (this, &XYStage::OnBacklash);
      CreateProperty("Backlash-B", "0", MM::Float, false, pAct);
   }

   // Error (sets both x and y)
   if (hasCommand("E X?")) {
      pAct = new CPropertyAction (this, &XYStage::OnError);
      CreateProperty("Error-E(nm)", "0", MM::Float, false, pAct);
   }

   // Finish Error (sets both x and y)
   if (hasCommand("PC X?")) {
      pAct = new CPropertyAction (this, &XYStage::OnFinishError);
      CreateProperty("FinishError-PCROS(nm)", "0", MM::Float, false, pAct);
   }

   // OverShoot (sets both x and y)
   if (hasCommand("OS X?")) {
      pAct = new CPropertyAction (this, &XYStage::OnOverShoot);
      CreateProperty("OverShoot(um)", "0", MM::Float, false, pAct);
   }

   // MotorCtrl, (works on both x and y)
   pAct = new CPropertyAction (this, &XYStage::OnMotorCtrl);
   CreateProperty("MotorOnOff", "On", MM::String, false, pAct);
   AddAllowedValue("MotorOnOff", "On");
   AddAllowedValue("MotorOnOff", "Off");

   // JoyStick MirrorsX 
   // TODO: the following properties should only appear in controllers version 8 and higher
   pAct = new CPropertyAction (this, &XYStage::OnJSMirror);
   CreateProperty("JoyStick Reverse", "Off", MM::String, false, pAct);
   AddAllowedValue("JoyStick Reverse", "On");
   AddAllowedValue("JoyStick Reverse", "Off");

   pAct = new CPropertyAction (this, &XYStage::OnJSFastSpeed);
   CreateProperty("JoyStick Fast Speed", "100", MM::Integer, false, pAct);
   SetPropertyLimits("JoyStick Fast Speed", 1, 100);

   pAct = new CPropertyAction (this, &XYStage::OnJSSlowSpeed);
   CreateProperty("JoyStick Slow Speed", "100", MM::Integer, false, pAct);
   SetPropertyLimits("JoyStick Slow Speed", 1, 100);


   /* Disabled.  Use the MA command instead
   // Number of times stage approaches a new position (+1)
   if (hasCommand("CCA Y?")) {
      pAct = new CPropertyAction(this, &XYStage::OnNrMoveRepetitions);
      CreateProperty("NrMoveRepetitions", "0", MM::Integer, false, pAct);
      SetPropertyLimits("NrMoveRepetitions", 0, 10);
   }
   */

   /*
   ret = UpdateStatus();  
   if (ret != DEVICE_OK)
      return ret;
   */

   initialized_ = true;
   return DEVICE_OK;
}

int XYStage::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool XYStage::Busy()
{
   // empty the Rx serial buffer before sending command
   ClearPort(); 

   const char* command = "/";
   string answer;
   // query the device
   int ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK)
      return false;

   if (answer.length() >= 1)
   {
	  if (answer.substr(0,1) == "B") return true;
	  else if (answer.substr(0,1) == "N") return false;
	  else return false;
   }

   return false;
}


int XYStage::SetPositionSteps(long x, long y)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << fixed << "M X=" << x/ASISerialUnit_ << " Y=" << y/ASISerialUnit_; // steps are 10th of micros

   string answer;
   // query the device
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if ( (answer.substr(0,2).compare(":A") == 0) || (answer.substr(1,2).compare(":A") == 0) )
   {
      return DEVICE_OK;
   }
   // deal with error later
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(4).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;  
}

int XYStage::SetRelativePositionSteps(long x, long y)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << fixed << "R X=" << x/ASISerialUnit_  << " Y=" << y/ASISerialUnit_; // in 10th of micros

   string answer;
   // query the device
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if ( (answer.substr(0,2).compare(":A") == 0) || (answer.substr(1,2).compare(":A") == 0) )
   {
      return DEVICE_OK;
   }
   // deal with error later
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(4).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;  
}

int XYStage::GetPositionSteps(long& x, long& y)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << "W X Y";

   string answer;
   // query the device
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.length() > 2 && answer.substr(0, 2).compare(":N") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
	  float xx, yy;
      char head[64];
	  char iBuf[256];
	  strcpy(iBuf,answer.c_str());
	  sscanf(iBuf, "%s %f %f\r\n", head, &xx, &yy);
	  x = (long) (xx * ASISerialUnit_);
	  y = (long) (yy * ASISerialUnit_); 

      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}
  
int XYStage::SetOrigin()
{
   string answer;
   // query the device
   int ret = QueryCommand("H X=0 Y=0", answer); // use command HERE, zero (z) zero all x,y,z
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2,4).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;   
};


void XYStage::Wait()
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   //if (stopSignal_) return DEVICE_OK;
   bool busy=true;
   const char* command = "/";
   string answer="";
   // query the device
   QueryCommand(command, answer);
   //if (ret != DEVICE_OK)
   //   return ret;

   // block/wait for acknowledge, or until we time out;
   
   if (answer.substr(0,1) == "B") busy = true;
   else if (answer.substr(0,1) == "N") busy = false;
   else busy = true;

   //if (stopSignal_) return DEVICE_OK;

   int intervalMs = 100;
   int totaltime=0;
   while ( busy ) {
		//if (stopSignal_) return DEVICE_OK;
		//Sleep(intervalMs);
		totaltime += intervalMs;

		// query the device
		QueryCommand(command, answer);
		//if (ret != DEVICE_OK)
		//  return ret;

 	    if (answer.substr(0,1) == "B") busy = true;
		else if (answer.substr(0,1) == "N") busy = false;
		else busy = true;

		if (!busy) break;
		//if (totaltime > timeout ) break;

   }

   //return DEVICE_OK;
}

int XYStage::Home()
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   string answer;
   // query the device
   int ret = QueryCommand("! X Y", answer); // use command HOME
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {
      //do nothing;
   }
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2,4).c_str());
      return ERR_OFFSET + errNo;
   }

   return DEVICE_OK;

};

int XYStage::Calibrate(){
	
	if (stopSignal_) return DEVICE_OK;

	double x1, y1;
	int ret = GetPositionUm(x1, y1);
    if (ret != DEVICE_OK)
      return ret;

	Wait();
	//ret = Wait();
 //   if (ret != DEVICE_OK)
 //     return ret;
	if (stopSignal_) return DEVICE_OK;

	//

   // do home command
   string answer;
   // query the device
   ret = QueryCommand("! X Y", answer); // use command HOME
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0  || answer.substr(1,2).compare(":A") == 0)
   {
      //do nothing;
   }
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2,4).c_str());
      return ERR_OFFSET + errNo;
   }

	//Wait();
	//if (stopSignal_) return DEVICE_OK;
	////

	//double x2, y2;
	//ret = GetPositionUm(x2, y2);
 //   if (ret != DEVICE_OK)
 //     return ret;

	//Wait();
	////ret = Wait();
 ////   if (ret != DEVICE_OK)
 ////     return ret;
	//if (stopSignal_) return DEVICE_OK;

	//ret = SetOrigin();
	//if (ret != DEVICE_OK)
 //     return ret;

	//Wait();
	////ret = Wait();
 ////   if (ret != DEVICE_OK)
 ////     return ret;
	//if (stopSignal_) return DEVICE_OK;

	////
	//double x = x1-x2;
	//double y = y1-y2;
	//ret = SetPositionUm(x, y);
	//if (ret != DEVICE_OK)
 //     return ret;
	//
	//Wait();
	////ret = Wait();
 ////   if (ret != DEVICE_OK)
 ////     return ret;
	//if (stopSignal_) return DEVICE_OK;

	return DEVICE_OK;

}

int XYStage::Calibrate1() {
	int ret = Calibrate();
	stopSignal_ = false;
	return ret;
}

int XYStage::Stop() 
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   stopSignal_ = true;
   string answer;
   // query the device
   int ret = QueryCommand("HALT", answer);  // use command HALT "\"
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2,4).c_str());
	  if (errNo == -21) return DEVICE_OK;
      else return errNo; //ERR_OFFSET + errNo;
   }

   return DEVICE_OK;
}
 
int XYStage::GetLimitsUm(double& /*xMin*/, double& /*xMax*/, double& /*yMin*/, double& /*yMax*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

int XYStage::GetStepLimits(long& /*xMin*/, long& /*xMax*/, long& /*yMin*/, long& /*yMax*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

bool XYStage::hasCommand(std::string command) {
   string answer;
   // query the device
   int ret = QueryCommand(command.c_str(), answer);
   if (ret != DEVICE_OK)
      return false;

   if (answer.substr(0,2).compare(":A") == 0)
      return true;
   if (answer.substr(0,4).compare(":N-1") == 0)
      return false;

   // if we do not get an answer, or any other answer, this is probably OK
   return true;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int XYStage::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int XYStage::OnStepSizeX(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(stepSizeXUm_);
   }

   return DEVICE_OK;
}
int XYStage::OnStepSizeY(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(stepSizeYUm_);
   }

   return DEVICE_OK;
}

// Get the version of this controller
int XYStage::OnVersion(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      ostringstream command;
      command << "V";
      string answer;
      // query the device
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
      {
         pProp->Set(answer.substr(3).c_str());
         return DEVICE_OK;
      }
      // deal with error later
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;  
   }

   return DEVICE_OK;
}


// This sets how often the stage will approach the same position (0 = 1!!)
int XYStage::OnNrMoveRepetitions(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // some controllers will return this, the current ones do not, so cache
      pProp->Set(nrMoveRepetitions_);
   }
   else if (eAct == MM::AfterSet) {
      pProp->Get(nrMoveRepetitions_);
      if (nrMoveRepetitions_ < 0)
         nrMoveRepetitions_ = 0;
      ostringstream command;
      command << "CCA Y=" << nrMoveRepetitions_;
      string answer;
      // some controller do not answer, so do not check answer
      int ret = SendCommand(command.str().c_str());
      if (ret != DEVICE_OK)
         return ret;

      /*
      // block/wait for acknowledge, or until we time out;
      string answer;
      ret = GetSerialAnswer(port_.c_str(), "\r\n", answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;
      */
   }
   return DEVICE_OK;
}

// This sets the number of waitcycles
int XYStage::OnWait(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // To simplify our life we only read out waitcycles for the X axis, but set for both
      ostringstream command;
      command << "WT X?";
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":X") == 0)
      {
         long waitCycles = atol(answer.substr(3).c_str());
         pProp->Set(waitCycles);
         return DEVICE_OK;
      }
      // deal with error later
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;  
   }
   else if (eAct == MM::AfterSet) {
      long waitCycles;
      pProp->Get(waitCycles);
      if (waitCycles < 0)
         waitCycles = 0;
      else if (waitCycles > 255)
         waitCycles = 255;
      ostringstream command;
      command << "WT X=" << waitCycles << " Y=" << waitCycles;
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;
   }
   return DEVICE_OK;
}


int XYStage::OnBacklash(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // To simplify our life we only read out waitcycles for the X axis, but set for both
      ostringstream command;
      command << "B X?";
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":X") == 0)
      {
         double speed = atof(answer.substr(3, 8).c_str());
         pProp->Set(speed);
         return DEVICE_OK;
      }
      // deal with error later
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;  
   }
   else if (eAct == MM::AfterSet) {
      double backlash;
      pProp->Get(backlash);
      if (backlash < 0.0)
         backlash = 0.0;
      ostringstream command;
      command << "B X=" << backlash << " Y=" << backlash;
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;
   }
   return DEVICE_OK;
}

int XYStage::OnFinishError(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // To simplify our life we only read out waitcycles for the X axis, but set for both
      ostringstream command;
      command << "PC X?";
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":X") == 0)
      {
         double fError = atof(answer.substr(3, 8).c_str());
         pProp->Set(1000000* fError);
         return DEVICE_OK;
      }
      if (answer.substr(0,2).compare(":A") == 0)
      {
         // Answer is of the form :A X=0.00003
         double fError = atof(answer.substr(5, 8).c_str());
         pProp->Set(1000000* fError);
         return DEVICE_OK;
      }
      // deal with error later
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;  
   }
   else if (eAct == MM::AfterSet) {
      double error;
      pProp->Get(error);
      if (error < 0.0)
         error = 0.0;
      error = error/1000000;
      ostringstream command;
      command << "PC X=" << error << " Y=" << error;
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;
   }
   return DEVICE_OK;
}

int XYStage::OnOverShoot(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // To simplify our life we only read out waitcycles for the X axis, but set for both
      ostringstream command;
      command << "OS X?";
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
      {
         double overShoot = atof(answer.substr(5, 8).c_str());
         pProp->Set(overShoot * 1000.0);
         return DEVICE_OK;
      }
      // deal with error later
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;  
   }
   else if (eAct == MM::AfterSet) {
      double overShoot;
      pProp->Get(overShoot);
      if (overShoot < 0.0)
         overShoot = 0.0;
      overShoot = overShoot / 1000.0;
      ostringstream command;
      command << fixed << "OS X=" << overShoot << " Y=" << overShoot;
      string answer;
      // query the device
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;
   }
   return DEVICE_OK;
}

int XYStage::OnError(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // To simplify our life we only read out waitcycles for the X axis, but set for both
      ostringstream command;
      command << "E X?";
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":X") == 0)
      {
         double fError = atof(answer.substr(3, 8).c_str());
         pProp->Set(fError * 1000000.0);
         return DEVICE_OK;
      }
      // deal with error later
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;  
   }
   else if (eAct == MM::AfterSet) {
      double error;
      pProp->Get(error);
      if (error < 0.0)
         error = 0.0;
      error = error / 1000000.0;
      ostringstream command;
      command << fixed << "E X=" << error << " Y=" << error;
      string answer;
      // query the device
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;
   }
   return DEVICE_OK;
}

int XYStage::OnSpeed(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // To simplify our life we only read out waitcycles for the X axis, but set for both
      ostringstream command;
      command << "S X?";
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
      {
         double speed = atof(answer.substr(5).c_str());
         pProp->Set(speed);
         return DEVICE_OK;
      }
      // deal with error later
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;  
   }
   else if (eAct == MM::AfterSet) {
      double speed;
      pProp->Get(speed);
      if (speed < 0.0)
         speed = 0.0;
      // Note, max speed may differ depending on pitch screw
      else if (speed > 7.5)
         speed = 7.5;
      ostringstream command;
      command << fixed << "S X=" << speed << " Y=" << speed;
      string answer;
      // query the device
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;
   }
   return DEVICE_OK;
}

int XYStage::OnMotorCtrl(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // The controller can not report whether or not the motors are on.  Cache the value
      if (motorOn_)
         pProp->Set("On");
      else
         pProp->Set("Off");

      return DEVICE_OK;
   }
   else if (eAct == MM::AfterSet) {
      string motorOn;
      string value;
      pProp->Get(motorOn);
      if (motorOn == "On") {
         motorOn_ = true;
         value = "+";
      } else {
         motorOn_ = false;
         value = "-";
      }
      ostringstream command;
      command << "MC X" << value << " Y" << value;
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      return ERR_UNRECOGNIZED_ANSWER;
   }
   return DEVICE_OK;
}

int XYStage::OnJSMirror(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // TODO: read from device, at least on initialization
      if (joyStickMirror_)
         pProp->Set("On");
      else
         pProp->Set("Off");

      return DEVICE_OK;
   }
   else if (eAct == MM::AfterSet) {
      string mirror;
      string value;
      pProp->Get(mirror);
      if (mirror == "On") {
         if (joyStickMirror_)
            return DEVICE_OK;
         joyStickMirror_ = true;
         value = "-";
      } else {
         if (!joyStickMirror_)
            return DEVICE_OK;
         joyStickMirror_ = false;
         value = "";
      }
      ostringstream command;
      command << "JS X=" << value << joyStickSpeedFast_ << " Y=" << value << joyStickSpeedSlow_;
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(answer.length(), 1) == "A")
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      else
         return ERR_UNRECOGNIZED_ANSWER;
   }
   return DEVICE_OK;
}


int XYStage::OnJSSwapXY(MM::PropertyBase* /* pProp*/ , MM::ActionType /* eAct*/)
{
   return DEVICE_NOT_SUPPORTED;
}

int XYStage::OnJSFastSpeed(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // TODO: read from device, at least on initialization
      pProp->Set((long) joyStickSpeedFast_);
      return DEVICE_OK;
   }
   else if (eAct == MM::AfterSet) {
      long speed;
      pProp->Get(speed);
      joyStickSpeedFast_ = (int) speed;

      string value = "";
      if (joyStickMirror_)
         value = "-";

      ostringstream command;
      command << "JS X=" << value << joyStickSpeedFast_ << " Y=" << value << joyStickSpeedSlow_;
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(answer.length(), 1) == "A")
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;
      }
      else
         return ERR_UNRECOGNIZED_ANSWER;
   }
   
   return DEVICE_OK;
}

int XYStage::OnJSSlowSpeed(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // TODO: read from device, at least on initialization
      pProp->Set((long) joyStickSpeedSlow_);
      return DEVICE_OK;
   }
   else if (eAct == MM::AfterSet) {
      long speed;
      pProp->Get(speed);
      joyStickSpeedSlow_ = (int) speed;

      string value = "";
      if (joyStickMirror_)
         value = "-";

      ostringstream command;
      command << "JS X=" << value << joyStickSpeedFast_ << " Y=" << value << joyStickSpeedSlow_;
      string answer;
      // query command
      int ret = QueryCommand(command.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0)
         return DEVICE_OK;
      else if (answer.substr(answer.length(), 1) == "A")
         return DEVICE_OK;
      else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2) {
         int errNo = atoi(answer.substr(3).c_str());
         return ERR_OFFSET + errNo;

      }
      else
         return ERR_UNRECOGNIZED_ANSWER;
   }

   return DEVICE_OK;
}


int XYStage::GetPositionStepsSingle(char /*axis*/, long& /*steps*/)
{
   return ERR_UNRECOGNIZED_ANSWER;
}

int XYStage::SetAxisDirection()
{
   ostringstream command;
   command << "UM X=-10000 Y=10000";
   string answer = "";
   // query command
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0)
      return DEVICE_OK;
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(3).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}



///////////////////////////////////////////////////////////////////////////////
// ZStage

ZStage::ZStage() :
   ASIBase(this, "1H"),
   axis_("Z"),
   stepSizeUm_(0.1),
   answerTimeoutMs_(1000),
   sequenceable_(false),
   hasRingBuffer_(false),
   nrEvents_(50)
{
   InitializeDefaultErrorMessages();

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_ZStageDeviceName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "ASI Z-stage driver adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &ZStage::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
 
   // Axis
   pAct = new CPropertyAction (this, &ZStage::OnAxis);
   CreateProperty("Axis", "Z", MM::String, false, pAct, true);
   AddAllowedValue("Axis", "Z");
   AddAllowedValue("Axis", "F");
}

ZStage::~ZStage()
{
   Shutdown();
}

void ZStage::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_ZStageDeviceName);
}

MM::DeviceDetectionStatus ZStage::DetectDevice(void)
{

   return ASICheckSerialPort(*this,*GetCoreCallback(), port_, answerTimeoutMs_);
}

int ZStage::Initialize()
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   // check status first (test for communication protocol)
   int ret = CheckDeviceStatus();
   if (ret != DEVICE_OK)
       return ret;

   // needs to be called first since it sets hasRingBuffer_
   GetControllerInfo();

   stepSizeUm_ = 0.1; //res;

   ret = GetPositionSteps(curSteps_);
   // if command fails, try one more time, 
   // other devices may have send crud to this serial port during device detection
   if (ret != DEVICE_OK) 
      ret = GetPositionSteps(curSteps_);

   if (HasRingBuffer())
   {
      CPropertyAction* pAct = new CPropertyAction (this, &ZStage::OnSequence);
      const char* spn = "Use Sequence";
      CreateProperty(spn, "No", MM::String, false, pAct);
      AddAllowedValue(spn, "No");
      AddAllowedValue(spn, "Yes");
   }
      
   initialized_ = true;
   return DEVICE_OK;
}

int ZStage::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool ZStage::Busy()
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   const char* command = "/";
   string answer;
   // query command
   int ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK)
      return false;

   if (answer.length() >= 1)
   {
	  if (answer.substr(0,1) == "B") return true;
	  else if (answer.substr(0,1) == "N") return false;
	  else return false;
   }

   return false;
}

int ZStage::SetPositionUm(double pos)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << fixed << "M " << axis_ << "=" << pos / stepSizeUm_; // in 10th of micros

   string answer;
   // query the device
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {
      this->OnStagePositionChanged(pos);
      return DEVICE_OK;
   }
   // deal with error later
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(4).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER; 
}

int ZStage::GetPositionUm(double& pos)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << "W " << axis_;

   string answer;
   // query command
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.length() > 2 && answer.substr(0, 2).compare(":N") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      char head[64];
	  float zz;
	  char iBuf[256];
	  strcpy(iBuf,answer.c_str());
	  sscanf(iBuf, "%s %f\r\n", head, &zz);
	  
	  pos = zz * stepSizeUm_;
	  curSteps_ = (long)zz;

      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}
  
int ZStage::SetPositionSteps(long pos)
{
   ostringstream command;
   command << "M " << axis_ << "=" << pos; // in 10th of micros

   string answer;
   // query command
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {
      return DEVICE_OK;
   }
   // deal with error later
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(4).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER; 
 
}
  
int ZStage::GetPositionSteps(long& steps)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << "W " << axis_;

   string answer;
   // query command
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.length() > 2 && answer.substr(0, 2).compare(":N") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      char head[64];
	  float zz;
	  char iBuf[256];
	  strcpy(iBuf,answer.c_str());
	  sscanf(iBuf, "%s %f\r\n", head, &zz);
	  
	  steps = (long) zz;
	  curSteps_ = (long)steps;

      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

//int ZStage::GetResolution(double& res)
//{
//   const char* command="RES,Z";
//
//   string answer;
//   // query command
//   int ret = QueryCommand(command, answer);
//   if (ret != DEVICE_OK)
//      return ret;
//
//   if (answer.length() > 2 && answer.substr(0, 1).compare("E") == 0)
//   {
//      int errNo = atoi(answer.substr(2).c_str());
//      return ERR_OFFSET + errNo;
//   }
//   else if (answer.length() > 0)
//   {
//      res = atof(answer.c_str());
//      return DEVICE_OK;
//   }
//
//   return ERR_UNRECOGNIZED_ANSWER;
//}

int ZStage::SetOrigin()
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream os;
   os << "H " << axis_;
   string answer;
   // query command
   int ret = QueryCommand(os.str().c_str(), answer); // use command HERE, zero (z) zero all x,y,z
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2,4).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;  
}

int ZStage::Calibrate(){

	return DEVICE_OK;;
}

int ZStage::GetLimits(double& /*min*/, double& /*max*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

bool ZStage::HasRingBuffer()
{
   return hasRingBuffer_;
}


int ZStage::StartStageSequence() const
{
   string answer;
   // Note, what to do with F axis????
   int ret = QueryCommand("RM Y=4 Z=0", answer); // ensure that ringbuffer pointer points to first entry and that we only trigger the Z axis
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {  
      ret = QueryCommand("TTL X=1", answer); // switches on TTL triggering
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
         return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

int ZStage::StopStageSequence() const 
{
   std::string answer;
   int ret = QueryCommand("TTL X=0", answer); // switches off TTL triggering
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
         return DEVICE_OK;

   return DEVICE_OK;
}

int ZStage::SendStageSequence() const
{
   // first clear the buffer in the device
   std::string answer;
   int ret = QueryCommand("RM X=0", answer); // clears the ringbuffer
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {
      for (unsigned i=0; i< sequence_.size(); i++)
      {
         // Note, what to do with F axis????
         ostringstream os;
         os.precision(0);
         // Strange, but this command needs <CR><LF>.  
         os << fixed << "LD Z=" << sequence_[i] * 10 << "\r\n"; 
         ret = QueryCommand(os.str().c_str(), answer);
         if (ret != DEVICE_OK)
            return ret;

         // the asnwer will also have a :N-1 in it, ignore.
         if (! (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0) )
            return ERR_UNRECOGNIZED_ANSWER;
      }
   }

   return DEVICE_OK;
}

int ZStage::ClearStageSequence()
{
   sequence_.clear();

   // clear the buffer in the device
   std::string answer;
   int ret = QueryCommand("RM X=0", answer); // clears the ringbuffer
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
      return DEVICE_OK;
   
   return ERR_UNRECOGNIZED_ANSWER;
}

int ZStage::AddToStageSequence(double position)
{
   sequence_.push_back(position);

   return DEVICE_OK;
}

/*
 * This function checks what is available in this controller
 * It should really be part of a Hub Device
 */
int ZStage::GetControllerInfo()
{
   std::string answer;
   int ret = QueryCommand("BU X", answer);
   if (ret != DEVICE_OK)
      return ret;

   std::istringstream iss(answer);
   std::string token;
   while (getline(iss, token, '\r'))
   {
      if (token == "RING BUFFER")
         hasRingBuffer_ = true;
      // TODO: add in tests for other capabilities/devices
   }

   LogMessage(answer.c_str(), false);

   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int ZStage::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int ZStage::OnAxis(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(axis_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(axis_);
   }

   return DEVICE_OK;
}

int ZStage::OnSequence(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      if (sequenceable_)
         pProp->Set("Yes");
      else
         pProp->Set("No");
   }
   else if (eAct == MM::AfterSet)
   {
      std::string prop;
      pProp->Get(prop);
      sequenceable_ = false;
      if (prop == "Yes")
         sequenceable_ = true;
   }

   return DEVICE_OK;
}

//////////////////////////////////////////////////////////////////////
// CRIF reflection-based autofocussing unit (Nico, May 2007)
////
CRIF::CRIF() :
   ASIBase(this, "" /* LX-4000 Prefix Unknown */),
   justCalibrated_(false),
   axis_("Z"),
   stepSizeUm_(0.1),
   waitAfterLock_(3000)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_NOT_CALIBRATED, "CRIF is not calibrated.  Try focusing close to a coverslip and selecting 'Calibrate'");
   SetErrorText(ERR_UNRECOGNIZED_ANSWER, "The ASI controller said something incomprehensible");
   SetErrorText(ERR_NOT_LOCKED, "The CRIF failed to lock");

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_CRIFDeviceName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "ASI CRIF Autofocus adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &CRIF::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

}

CRIF::~CRIF()
{
   initialized_ = false;
}


int CRIF::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   // check status first (test for communication protocol)
   int ret = CheckDeviceStatus();
   if (ret != DEVICE_OK)
      return ret;

   CPropertyAction* pAct = new CPropertyAction(this, &CRIF::OnFocus);
   CreateProperty (g_CRIFState, "Undefined", MM::String, false, pAct);

   // Add values (TODO: check manual)
   AddAllowedValue(g_CRIFState, g_CRIF_I);
   AddAllowedValue(g_CRIFState, g_CRIF_L);
   AddAllowedValue(g_CRIFState, g_CRIF_Cal);
   AddAllowedValue(g_CRIFState, g_CRIF_G);
   AddAllowedValue(g_CRIFState, g_CRIF_B);
   AddAllowedValue(g_CRIFState, g_CRIF_k);
   AddAllowedValue(g_CRIFState, g_CRIF_K);
   AddAllowedValue(g_CRIFState, g_CRIF_O);

   pAct = new CPropertyAction(this, &CRIF::OnWaitAfterLock);
   CreateProperty("Wait ms after Lock", "3000", MM::Integer, false, pAct);

   initialized_ = true;
   return DEVICE_OK;
}

int CRIF::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

bool CRIF::Busy()
{
   //TODO implement
   return false;
}

// TODO: See if this can be implemented for the CRIF
int CRIF::GetOffset(double& offset)
{
   offset = 0;
   return DEVICE_OK;
}

// TODO: See if this can be implemented for the CRIF
int CRIF::SetOffset(double /* offset */)
{
   return DEVICE_OK;
}

void CRIF::GetName(char* pszName) const
{
   CDeviceUtils::CopyLimitedString(pszName, g_CRIFDeviceName);
}

int CRIF::GetFocusState(std::string& focusState)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   const char* command = "LOCK X?"; // Requests single char lock state description
   string answer;
   // query command
   int ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK)
      return ERR_UNRECOGNIZED_ANSWER;

   // translate response to one of our globals (see page 6 of CRIF manual)
   char test = answer.c_str()[3];
   switch (test) {
      case 'I': 
         focusState = g_CRIF_I;
         break;
      case 'L': 
         focusState = g_CRIF_L;
         break;
      case '1': 
      case '2': 
      case '3': 
         focusState = g_CRIF_Cal;
         break;
      case 'G': 
         focusState = g_CRIF_G;
         break;
      case 'B': 
         focusState = g_CRIF_B;
         break;
      case 'k': 
         focusState = g_CRIF_k;
         break;
      case 'K': 
         focusState = g_CRIF_K;
         break;
      case 'E': 
         focusState = g_CRIF_E;
         break;
      case 'O': 
         focusState = g_CRIF_O;
         break;
      default:
         return ERR_UNRECOGNIZED_ANSWER;
   }

   return DEVICE_OK;
}

int CRIF::SetFocusState(std::string focusState)
{
   std::string currentState;
   int ret = GetFocusState(currentState);
   if (ret != DEVICE_OK)
      return ret;

   if (focusState == g_CRIF_I || focusState == g_CRIF_O)
   {
      // Unlock and switch off laser:
      ret = SetContinuousFocusing(false);
      if (ret != DEVICE_OK)
         return ret;
   }

   /*
   else if (focusState == g_CRIF_O) // we want the laser off and discard calibration (start anew)
   {
      if (currentState == g_CRIF_K)
      { // unlock first
         ret = SetContinuousFocusing(false);
         if (ret != DEVICE_OK)
            return ret;
      }
      if (currentState == g_CRIF_G || currentState == g_CRIF_B || currentState == g_CRIF_L)
      {
         const char* command = "LK Z";
         // query command and wait for acknowledgement
         int ret = QueryCommandACK(command);
         if (ret != DEVICE_OK)
            return ret;
      }
      if (currentState == g_CRIF_L) // we need to advance the state once more.  Wait a bit for calibration to finish)
      {
         CDeviceUtils::SleepMs(1000); // ms
         const char* command = "LK Z";
         // query command and wait for acknowledgement
         int ret = QueryCommandACK(command);
         if (ret != DEVICE_OK)
            return ret;
      }
   }
   */

   else if (focusState == g_CRIF_L)
   {
      if ( (currentState == g_CRIF_I) || currentState == g_CRIF_O)
      {
         const char* command = "LK Z";
         // query command and wait for acknowledgement
         int ret = QueryCommandACK(command);
         if (ret != DEVICE_OK)
            return ret;
      }
   }

   else if (focusState == g_CRIF_Cal) 
   {
      const char* command = "LK Z";
      if (currentState == g_CRIF_B || currentState == g_CRIF_O)
      {
         // query command and wait for acknowledgement
         int ret = QueryCommandACK(command);
         if (ret != DEVICE_OK)
            return ret;
         ret = GetFocusState(currentState);
         if (ret != DEVICE_OK)
            return ret;
      }  
      if (currentState == g_CRIF_I) // Idle, first switch on laser
      {
         // query command and wait for acknowledgement
         int ret = QueryCommandACK(command);
         if (ret != DEVICE_OK)
            return ret;
         ret = GetFocusState(currentState);
         if (ret != DEVICE_OK)
            return ret;
      }
      if (currentState == g_CRIF_L)
      {
         // query command and wait for acknowledgement
         int ret = QueryCommandACK(command);
         if (ret != DEVICE_OK)
            return ret;
      }

      // now wait for the lock to occur
      MM::MMTime startTime = GetCurrentMMTime();
      MM::MMTime wait(3,0);
      bool cont = false;
      std::string finalState;
      do {
         CDeviceUtils::SleepMs(250);
         GetFocusState(finalState);
         cont = (startTime - GetCurrentMMTime()) < wait;
      } while ( finalState != g_CRIF_G && finalState != g_CRIF_B && cont);

      justCalibrated_ = true; // we need this to know whether this is the first time we lock
   }

   else if ( (focusState == g_CRIF_K) || (focusState == g_CRIF_k) )
   {
      // only try a lock when we are good
      if ( (currentState == g_CRIF_G) || (currentState == g_CRIF_O) ) 
      {
         int ret = SetContinuousFocusing(true);
         if (ret != DEVICE_OK)
            return ret;
      }
      else if (! ( (currentState == g_CRIF_k) || currentState == g_CRIF_K) ) 
      {
         // tell the user that we first need to calibrate before starting a lock
         return ERR_NOT_CALIBRATED;
      }
   }

   return DEVICE_OK;
}

bool CRIF::IsContinuousFocusLocked()
{
   std::string focusState;
   int ret = GetFocusState(focusState);
   if (ret != DEVICE_OK)
      return false;

   if (focusState == g_CRIF_K)
      return true;

   return false;
}


int CRIF::SetContinuousFocusing(bool state)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   string command;
   if (state)
   {
      // TODO: check that the system has been calibrated and can be locked!
      if (justCalibrated_)
         command = "LK";
      else
         command = "RL"; // Turns on laser and initiated lock state using previously saved reference
   }
   else
   {
      command = "UL X"; // Turns off laser and unlocks
   }
   string answer;
   // query command
   int ret = QueryCommand(command.c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   // The controller only acknowledges receipt of the command
   if (answer.substr(0,2) != ":A")
      return ERR_UNRECOGNIZED_ANSWER;

   justCalibrated_ = false;

   return DEVICE_OK;
}


int CRIF::GetContinuousFocusing(bool& state)
{
   std::string focusState;
   int ret = GetFocusState(focusState);
   if (ret != DEVICE_OK)
      return ret;

   if (focusState == g_CRIF_K)
      state = true;
   else
      state =false;
   
   return DEVICE_OK;
}

int CRIF::FullFocus()
{
   double pos;
   int ret = GetPositionUm(pos);
   if (ret != DEVICE_OK)
      return ret;
   ret = SetContinuousFocusing(true);
   if (ret != DEVICE_OK)
      return ret;

   MM::MMTime startTime = GetCurrentMMTime();
   MM::MMTime wait(3, 0);
   while (!IsContinuousFocusLocked() && ( (GetCurrentMMTime() - startTime) < wait) ) {
      CDeviceUtils::SleepMs(25);
   }

   CDeviceUtils::SleepMs(waitAfterLock_);

   if (!IsContinuousFocusLocked()) {
      SetContinuousFocusing(false);
      SetPositionUm(pos);
      return ERR_NOT_LOCKED;
   }

   return SetContinuousFocusing(false);
}

int CRIF::IncrementalFocus()
{
   return FullFocus();
}

int CRIF::GetLastFocusScore(double& score)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   score = 0;
   const char* command = "LOCK Y?"; // Requests present value of the PSD signal as shown on LCD panel
   string answer;
   // query command
   int ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK)
      return ret;

   score = atof (answer.substr(2).c_str());
   if (score == 0)
      return ERR_UNRECOGNIZED_ANSWER;

   return DEVICE_OK;
}

int CRIF::SetPositionUm(double pos)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << fixed << "M " << axis_ << "=" << pos / stepSizeUm_; // in 10th of micros

   string answer;
   // query the device
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,2).compare(":A") == 0 || answer.substr(1,2).compare(":A") == 0)
   {
      return DEVICE_OK;
   }
   // deal with error later
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(4).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER; 
}

int CRIF::GetPositionUm(double& pos)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << "W " << axis_;

   string answer;
   // query command
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.length() > 2 && answer.substr(0, 2).compare(":N") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      char head[64];
      float zz;
      char iBuf[256];
      strcpy(iBuf,answer.c_str());
      sscanf(iBuf, "%s %f\r\n", head, &zz);
	  
	   pos = zz * stepSizeUm_;

      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int CRIF::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}


int CRIF::OnFocus(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      int ret = GetFocusState(focusState_);
      if (ret != DEVICE_OK)
         return ret;
      pProp->Set(focusState_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(focusState_);
      int ret = SetFocusState(focusState_);
      if (ret != DEVICE_OK)
         return ret;
   }

   return DEVICE_OK;
}

int CRIF::OnWaitAfterLock(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(waitAfterLock_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(waitAfterLock_);
   }

   return DEVICE_OK;
}


//////////////////////////////////////////////////////////////////////
// CRISP reflection-based autofocussing unit (Nico, Nov 2011)
////
CRISP::CRISP() :
   ASIBase(this, "" /* LX-4000 Prefix Unknown */),
   justCalibrated_(false),
   axis_("Z"),
   stepSizeUm_(0.1),
   ledIntensity_(50),
   na_(0.65),
   waitAfterLock_(1000)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_NOT_CALIBRATED, "CRISP is not calibrated.  Try focusing close to a coverslip and selecting 'Calibrate'");
   SetErrorText(ERR_UNRECOGNIZED_ANSWER, "The ASI controller said something incomprehensible");
   SetErrorText(ERR_NOT_LOCKED, "The CRISP failed to lock");

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_CRISPDeviceName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "ASI CRISP Autofocus adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &CRISP::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

   // Axis
   pAct = new CPropertyAction (this, &CRISP::OnAxis);
   CreateProperty("Axis", "Z", MM::String, false, pAct, true);
   AddAllowedValue("Axis", "Z");
   AddAllowedValue("Axis", "F");
}

CRISP::~CRISP()
{
   initialized_ = false;
}


int CRISP::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   // check status first (test for communication protocol)
   int ret = CheckDeviceStatus();
   if (ret != DEVICE_OK)
      return ret;

   CPropertyAction* pAct = new CPropertyAction(this, &CRISP::OnFocus);
   CreateProperty (g_CRISPState, "Undefined", MM::String, false, pAct);

   // Add values (TODO: check manual)
   AddAllowedValue(g_CRISPState, g_CRISP_I);
   AddAllowedValue(g_CRISPState, g_CRISP_R);
   AddAllowedValue(g_CRISPState, g_CRISP_D);
   AddAllowedValue(g_CRISPState, g_CRISP_K);
   AddAllowedValue(g_CRISPState, g_CRISP_F);
   AddAllowedValue(g_CRISPState, g_CRISP_N);
   AddAllowedValue(g_CRISPState, g_CRISP_E);
   AddAllowedValue(g_CRISPState, g_CRISP_G);
   AddAllowedValue(g_CRISPState, g_CRISP_f);
   AddAllowedValue(g_CRISPState, g_CRISP_C);
   AddAllowedValue(g_CRISPState, g_CRISP_B);
   AddAllowedValue(g_CRISPState, g_CRISP_SG);
   AddAllowedValue(g_CRISPState, g_CRISP_RFO);

   pAct = new CPropertyAction(this, &CRISP::OnWaitAfterLock);
   CreateProperty("Wait ms after Lock", "3000", MM::Integer, false, pAct);

   pAct = new CPropertyAction(this, &CRISP::OnNA);
   CreateProperty("Objective NA", "0.8", MM::Float, false, pAct);
   SetPropertyLimits("Objective NA", 0, 1.65);

   pAct = new CPropertyAction(this, &CRISP::OnLockRange);
   CreateProperty("Max Lock Range(mm)", "0.05", MM::Float, false, pAct);

   pAct = new CPropertyAction(this, &CRISP::OnCalGain);
   CreateProperty("Calibration Gain", "0.05", MM::Integer, false, pAct);

   pAct = new CPropertyAction(this, &CRISP::OnLEDIntensity);
   CreateProperty("LED Intensity", "50", MM::Integer, false, pAct);
   SetPropertyLimits("LED Intensity", 0, 100);

   pAct = new CPropertyAction(this, &CRISP::OnGainMultiplier);
   CreateProperty("GainMultiplier", "10", MM::Integer, false, pAct);
   SetPropertyLimits("GainMultiplier", 1, 100);

   pAct = new CPropertyAction(this, &CRISP::OnNumAvg);
   CreateProperty("Number of Averages", "1", MM::Integer, false, pAct);
   SetPropertyLimits("Number of Averages", 0, 10);

   const char* fc = "Obtain Focus Curve";
   pAct = new CPropertyAction(this, &CRISP::OnFocusCurve);
   CreateProperty(fc, " ", MM::String, false, pAct);
   AddAllowedValue(fc, " ");
   AddAllowedValue(fc, "Do it");

   for (long i = 0; i < 4; i++)
   {
      std::ostringstream os("");
      os << "Focus Curve Data" << i;
      CPropertyActionEx* pActEx = new CPropertyActionEx(this, &CRISP::OnFocusCurveData, i);
      CreateProperty(os.str().c_str(), "", MM::String, true, pActEx);
   }

   pAct = new CPropertyAction(this, &CRISP::OnSNR);
   CreateProperty("Signal Noise Ratio", "", MM::Float, true, pAct);

   return DEVICE_OK;
}

int CRISP::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

bool CRISP::Busy()
{
   //TODO implement
   return false;
}

/**
 * Note that offset is not in um but arbitrary (integer) numbers
 */
int CRISP::GetOffset(double& offset)
{
   float val;
   int ret = GetValue("LK Z?", val);
   if (ret != DEVICE_OK)
      return ret;

   int v = (int) val;

   offset = (double) v;

   return DEVICE_OK;
}

/**
 * Note that offset is not in um but arbitrary (integer) numbers
 */
int CRISP::SetOffset(double  offset)
{
   std::ostringstream os;
   os << "LK Z=" << fixed << (int) offset;
   return SetCommand(os.str().c_str());
}

void CRISP::GetName(char* pszName) const
{
   CDeviceUtils::CopyLimitedString(pszName, g_CRISPDeviceName);
}

int CRISP::GetFocusState(std::string& focusState)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   const char* command = "LK X?"; // Requests single char lock state description
   string answer;
   // query command
   int ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK)
      return ERR_UNRECOGNIZED_ANSWER;

   // translate response to one of our globals (see page 6 of CRIF manual)
   char test = answer.c_str()[3];
   switch (test) {
      case 'I': 
         focusState = g_CRISP_I;
         break;
      case 'R': 
         focusState = g_CRISP_R;
         break;
      case '1': 
      case '2': 
      case '3': 
      case '4': 
      case '5': 
      case 'g':
      case 'h' :
      case 'i' :
      case 'j' :
         focusState = g_CRISP_Cal;
         break;
      case 'D': 
         focusState = g_CRISP_D;
         break;
      case 'F': 
         focusState = g_CRISP_F;
         break;
      case 'N': 
         focusState = g_CRISP_N;
         break;
      case 'E': 
         focusState = g_CRISP_E;
         break;
      case 'G': 
         focusState = g_CRISP_G;
         break;
      case 'f': 
         focusState = g_CRISP_f;
         break;
      case 'C': 
         focusState = g_CRISP_C;
         break;
      case 'B': 
         focusState = g_CRISP_B;
         break;
      case 'l':
         focusState = g_CRISP_RFO;
         break;
      default:
         return ERR_UNRECOGNIZED_ANSWER;
   }

   return DEVICE_OK;
}

int CRISP::SetFocusState(std::string focusState)
{
   std::string currentState;
   int ret = GetFocusState(currentState);
   if (ret != DEVICE_OK)
      return ret;

   if (focusState == currentState)
      return DEVICE_OK;

   if (focusState == g_CRISP_I)
   {
      // Idle (switch off LED)
      const char* command = "LK F=79";
      return SetCommand(command);
   }

   else if (focusState == g_CRISP_R )
   {
      // Unlock 
      ret = SetContinuousFocusing(false);
      if (ret != DEVICE_OK)
         return ret;
   }

   else if (focusState == g_CRIF_K)
   {
      // Lock
      ret = SetContinuousFocusing(true);
      if (ret != DEVICE_OK)
         return ret;
   }

   else if (focusState == g_CRISP_G) 
   {
      // Log-Amp Calibration
      const char* command = "LK F=72"; 
      return SetCommand(command);
   }

   else if (focusState == g_CRISP_SG) 
   {
      // gain_Cal Calibration
      const char* command = "LK F=67"; 
      return SetCommand(command);
   }

   else if (focusState == g_CRISP_f)
   {
      // Dither
      const char* command = "LK F=102";
      return SetCommand(command);
   }

   else if (focusState == g_CRISP_RFO)
   {
      // Reset focus offset
      const char* command = "LK F=108";
      return SetCommand(command);
   }
   
   return DEVICE_OK;
}

bool CRISP::IsContinuousFocusLocked()
{
   std::string focusState;
   int ret = GetFocusState(focusState);
   if (ret != DEVICE_OK)
      return false;

   if (focusState == g_CRISP_F)
      return true;

   return false;
}


int CRISP::SetContinuousFocusing(bool state)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   string command;
   if (state)
   {
      command = "LK F=83";
   }
   else
   {
      command = "LK F=85"; // Turns off laser and unlocks
   }

   return SetCommand(command);
}


int CRISP::GetContinuousFocusing(bool& state)
{
   std::string focusState;
   int ret = GetFocusState(focusState);
   if (ret != DEVICE_OK)
      return ret;

   if (focusState == g_CRIF_K)
      state = true;
   else
      state =false;
   
   return DEVICE_OK;
}

/**
 * Does a "one-shot" autofocus: locks and then unlocks again
 */
int CRISP::FullFocus()
{
   int ret = SetContinuousFocusing(true);
   if (ret != DEVICE_OK)
      return ret;

   MM::MMTime startTime = GetCurrentMMTime();
   MM::MMTime wait(0, waitAfterLock_ * 1000);
   while (!IsContinuousFocusLocked() && ( (GetCurrentMMTime() - startTime) < wait) ) {
      CDeviceUtils::SleepMs(25);
   }

   CDeviceUtils::SleepMs(waitAfterLock_);

   if (!IsContinuousFocusLocked()) {
      SetContinuousFocusing(false);
      return ERR_NOT_LOCKED;
   }

   return SetContinuousFocusing(false);
}

int CRISP::IncrementalFocus()
{
   return FullFocus();
}

int CRISP::GetLastFocusScore(double& score)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   score = 0;
   const char* command = "LK Y?"; // Requests present value of the focus error as shown on LCD panel
   string answer;
   // query command
   int ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK)
      return ret;

   score = atof (answer.substr(2).c_str());
   if (score == 0)
      return ERR_UNRECOGNIZED_ANSWER;

   return DEVICE_OK;
}

int CRISP::GetCurrentFocusScore(double& score)
{
   return GetLastFocusScore(score);
}

int CRISP::GetValue(string cmd, float& val)
{
   string answer;
   // query command
   int ret = QueryCommand(cmd.c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.length() > 2 && answer.substr(0, 2).compare(":N") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      size_t index = 0;
      while (!isdigit(answer[index]) && (index < answer.length()))
         index++;
      if (index >= answer.length())
         return ERR_UNRECOGNIZED_ANSWER;
      val = (float) atof( (answer.substr(index)).c_str());

      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}


int CRISP::SetCommand(std::string cmd)
{
   string answer;
   // query command
   int ret = QueryCommand(cmd.c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.length() > 2 && answer.substr(0, 2).compare(":N") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   if (answer.substr(0,2) == ":A") {
      return DEVICE_OK;
   }
 
   return ERR_UNRECOGNIZED_ANSWER;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int CRISP::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}


int CRISP::OnFocus(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      int ret = GetFocusState(focusState_);
      if (ret != DEVICE_OK)
         return ret;
      pProp->Set(focusState_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(focusState_);
      int ret = SetFocusState(focusState_);
      if (ret != DEVICE_OK)
         return ret;
   }

   return DEVICE_OK;
}

int CRISP::OnWaitAfterLock(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(waitAfterLock_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(waitAfterLock_);
   }

   return DEVICE_OK;
}

int CRISP::OnNA(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(na_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(na_);
   }

   return DEVICE_OK;
}

int CRISP::OnCalGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      float calGain;
      int ret = GetValue("LR X?", calGain);
      if (ret != DEVICE_OK)
         return ret;

      pProp->Set(calGain);
   }
   else if (eAct == MM::AfterSet)
   {
      double lr;
      pProp->Get(lr);
      ostringstream command;
      command << fixed << "LR X=" << (int) lr;

      return SetCommand(command.str());
   }

   return DEVICE_OK;
}
int CRISP::OnLockRange(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      float lockRange;
      int ret = GetValue("LR Z?", lockRange);
      if (ret != DEVICE_OK)
         return ret;

      pProp->Set(lockRange);
   }
   else if (eAct == MM::AfterSet)
   {
      double lr;
      pProp->Get(lr);
      ostringstream command;
      command << fixed << "LR Z=" << lr;

      return SetCommand(command.str());
   }

   return DEVICE_OK;
}

int CRISP::OnNumAvg(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      float numAvg;
      int ret = GetValue("RT F?", numAvg);
      if (ret != DEVICE_OK)
         return ret;

      pProp->Set(numAvg);
   }
   else if (eAct == MM::AfterSet)
   {
      long nr;
      pProp->Get(nr);
      ostringstream command;
      command << fixed << "RT F=" << nr;

      return SetCommand(command.str());
   }

   return DEVICE_OK;
}

int CRISP::OnGainMultiplier(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      float gainMultiplier;
      std::string command = "KA " + axis_ + "?";
      int ret = GetValue(command.c_str(), gainMultiplier);
      if (ret != DEVICE_OK)
         return ret;

      pProp->Set(gainMultiplier);
   }
   else if (eAct == MM::AfterSet)
   {
      long nr;
      pProp->Get(nr);
      ostringstream command;
      command << fixed << "KA Z=" << nr;

      return SetCommand(command.str());
   }

   return DEVICE_OK;
}

int CRISP::OnLEDIntensity(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(ledIntensity_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(ledIntensity_);
   }

   return DEVICE_OK;
}

int CRISP::OnFocusCurve(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(" ");
   } 
   else if (eAct == MM::AfterSet)
   {
      std::string val;
      pProp->Get(val);
      if (val == "Do it")
      {
         // TODO: may need to extend serial port timeout temporarily
         std::string answer;
         int ret = QueryCommand("LK F=97", answer);
         if (ret != DEVICE_OK)
            return ret;


         // we will time out while getting these data, so do not throw an error
         // Also, the total length will be about 3500 chars, since MM::MaxStrlength is 1024, we
         // need at least 4 strings.
         int index = 0;
         focusCurveData_[index] = "";
         while (ret == DEVICE_OK && index < 5)
         {
            ret = GetSerialAnswer(port_.c_str(), "\r", answer);
            focusCurveData_[index] += answer + "\r\n";
            if (focusCurveData_[index].length() > 975)
            {
               index++;
               focusCurveData_[index] = "";
            }
         }
      }
      for (int i=0; i < 5; i++)
      {
         int l = focusCurveData_[i].length();
         printf("Length of String: %d\n", l);
      }
   }
   return DEVICE_OK;
}

int CRISP::OnFocusCurveData(MM::PropertyBase* pProp, MM::ActionType eAct, long index)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(focusCurveData_[index].c_str());
   }
   return DEVICE_OK;
}

int CRISP::OnAxis(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(axis_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(axis_);
   }

   return DEVICE_OK;
}

int CRISP::OnSNR(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      std::string command = "EXTRA Y?";
      std::string answer;
      int ret = QueryCommand(command.c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      std::stringstream ss(answer);
      double snr;
      ss >> snr;

      pProp->Set(snr);
   }

   return DEVICE_OK;
}

/***************************************************************************
 *  AZ100 adapter 
 */
AZ100Turret::AZ100Turret() :
   ASIBase(this, "" /* LX-4000 Prefix Unknown */),
   numPos_(4),
   position_ (0)
{
   InitializeDefaultErrorMessages();

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_AZ100TurretName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "ASI AZ100 Turret Controller", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &AZ100Turret::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
 
}

AZ100Turret::~AZ100Turret()
{
   Shutdown();
}

void AZ100Turret::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_AZ100TurretName);
}

int AZ100Turret::Initialize()
{
   // state
   CPropertyAction* pAct = new CPropertyAction (this, &AZ100Turret::OnState);
   int ret = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   AddAllowedValue(MM::g_Keyword_State, "0");
   AddAllowedValue(MM::g_Keyword_State, "1");
   AddAllowedValue(MM::g_Keyword_State, "2");
   AddAllowedValue(MM::g_Keyword_State, "3");

   // Label
   // -----
   pAct = new CPropertyAction (this, &CStateBase::OnLabel);
   ret = CreateProperty(MM::g_Keyword_Label, "", MM::String, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   SetPositionLabel(0, "Position-1");
   SetPositionLabel(1, "Position-2");
   SetPositionLabel(2, "Position-3");
   SetPositionLabel(3, "Position-4");

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;
   return DEVICE_OK;
}

int AZ100Turret::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool AZ100Turret::Busy()
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   const char* command = "RS F";
   string answer;
   // query command
   int ret = QueryCommand(command, answer);
   if (ret != DEVICE_OK)
      return false;

   if (answer.length() >= 1)
   {
      int status = atoi(answer.substr(2).c_str());
      if (status & 1)
         return true;
	   return false;
   }
   return false;
}

int AZ100Turret::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int AZ100Turret::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(position_);
   }
   else if (eAct == MM::AfterSet)
   {
      long position;
      pProp->Get(position);

      ostringstream os;
      os << "MTUR X=" << position + 1;

      string answer;
      // query command
      int ret = QueryCommand(os.str().c_str(), answer);
      if (ret != DEVICE_OK)
         return ret;

      if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
      {
         int errNo = atoi(answer.substr(2,4).c_str());
         return ERR_OFFSET + errNo;
      }

      if (answer.substr(0,2) == ":A") {
         position_ = position;
      }
      else
         return ERR_UNRECOGNIZED_ANSWER;
   }

   return DEVICE_OK;
}


LED::LED() :
   CShutterBase<LED>(),
   ASIBase(this, "" /* LX-4000 Prefix Unknown */),
   open_(false),
   intensity_(1),
   name_("LED"),
   answerTimeoutMs_(1000)
{
   InitializeDefaultErrorMessages();

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_LEDName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "ASI LED controller", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &LED::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

void LED::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_LEDName);
}


MM::DeviceDetectionStatus LED::DetectDevice(void)
{
   return ASICheckSerialPort(*this,*GetCoreCallback(), port_, answerTimeoutMs_);
}

LED::~LED()
{
   Shutdown();
}

int LED::Initialize()
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   // check status first (test for communication protocol)
   int ret = CheckDeviceStatus();
   if (ret != DEVICE_OK)
       return ret;

   CPropertyAction* pAct = new CPropertyAction (this, &LED::OnState);
   CreateProperty(MM::g_Keyword_State, g_Closed, MM::String, false, pAct);
   AddAllowedValue(MM::g_Keyword_State, g_Closed);
   AddAllowedValue(MM::g_Keyword_State, g_Open);

   pAct = new CPropertyAction(this, &LED::OnIntensity);
   CreateProperty("Intensity", "1", MM::Integer, false, pAct);
   SetPropertyLimits("Intensity", 1, 100);

   ret = IsOpen(&open_);
   if (ret != DEVICE_OK)
      return ret;

   ret = CurrentIntensity(&intensity_);
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;
   return DEVICE_OK;
}

int LED::Shutdown() 
{
   initialized_ = false;
   return DEVICE_OK;
}

bool LED::Busy()
{
   // The LED should be a whole lot faster than our serial communication
   // so always respond false
   return false;
} 

   // Shutter API                                                                     
// All communication with the LED takes place in this function
int LED::SetOpen (bool open)
{
   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   if (open)
   {
      if (intensity_ == 100)
         command << "TTL Y=1";
      else
         command << fixed << "TTL Y=9 " << intensity_;
   } else
   {
      command << "TTL Y=0";
   }

   string answer;
   // query the device
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   if ( (answer.substr(0,2).compare(":A") == 0) || (answer.substr(1,2).compare(":A") == 0) )
   {
      open_ = open;
      return DEVICE_OK;
   }
   // deal with error later
   else if (answer.substr(0, 2).compare(":N") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(4).c_str());
      return ERR_OFFSET + errNo;
   }

   open_ = open;
   return DEVICE_OK;
}

/**
 * GetOpen returns a cached value.  If ASI ever gives another control to the TTL out
 * other than the serial interface, this will need to be changed to a call to IsOpen
 */
int LED::GetOpen(bool& open)
{
   open = open_;

   return DEVICE_OK;
}

/**
 * IsOpen queries the microscope for the state of the TTL
 */
int LED::IsOpen(bool *open)
{
   *open = true;

   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << "TTL Y?";

   string answer;
   // query the device
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   std::istringstream is(answer);
   std::string tok;
   is >> tok;
   if ( (tok.substr(0,2).compare(":A") == 0) || (tok.substr(1,2).compare(":A") == 0) ) {
      is >> tok;
      if (tok.substr(2,1) == "0")
         *open = false;
   }
   else if (tok.substr(0, 2).compare(":N") == 0 && tok.length() > 2)
   {
      int errNo = atoi(tok.substr(4).c_str());
      return ERR_OFFSET + errNo;
   }
   return DEVICE_OK;
}

/**
 * IsOpen queries the microscope for the state of the TTL
 */
int LED::CurrentIntensity(long* intensity)
{
   *intensity = 1;

   // empty the Rx serial buffer before sending command
   ClearPort();

   ostringstream command;
   command << "LED X?";

   string answer;
   // query the device
   int ret = QueryCommand(command.str().c_str(), answer);
   if (ret != DEVICE_OK)
      return ret;

   std::istringstream is(answer);
   std::string tok;
   std::string tok2;
   is >> tok;
   is >> tok2;
   if ( (tok2.substr(0,2).compare(":A") == 0) || (tok2.substr(1,2).compare(":A") == 0) ) {
      *intensity = atoi(tok.substr(2).c_str());
   }
   else if (tok.substr(0, 2).compare(":N") == 0 && tok.length() > 2)
   {
      int errNo = atoi(tok.substr(4).c_str());
      return ERR_OFFSET + errNo;
   }
   return DEVICE_OK;
}

int LED::Fire(double )
{
   return DEVICE_OK;
}

// action interface

int LED::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      std::string state = g_Open;
      if (!open_)
         state = g_Closed;
      pProp->Set(state.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      bool open = true;
      std::string state;
      pProp->Get(state);
      if (state == g_Closed)
         open = false;
      return SetOpen(open);
   }

   return DEVICE_OK;
}

int LED::OnIntensity(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(intensity_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(intensity_);
      // We could check that 0 < intensity_ < 101, but the system shoudl guarantee that
      if (intensity_ < 100)
      {
         ClearPort();

         ostringstream command;
         command << "LED X=";
         command << intensity_;

         string answer;
         // query the device
         int ret = QueryCommand(command.str().c_str(), answer);
         if (ret != DEVICE_OK)
            return ret;

         std::istringstream is(answer);
         std::string tok;
         is >> tok;
         if (tok.substr(0, 2).compare(":N") == 0 && tok.length() > 2)
         {
            int errNo = atoi(tok.substr(4).c_str());
            return ERR_OFFSET + errNo;
         }

      }
      if (open_)
         return SetOpen(open_);
   }

   return DEVICE_OK;
}

int LED::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}
