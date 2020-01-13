#include <stdio.h>
#include <string.h>
#include "x2dome.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/serialportparams2interface.h"


X2Dome::X2Dome(const char* pszSelection,
							 const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface*	pTheSkyXForMounts,
					SleeperInterface*					pSleeper,
					BasicIniUtilInterface*			pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*						pIOMutex,
					TickCountInterface*					pTickCount)
{

    m_nPrivateISIndex				= nISIndex;
	m_pSerX							= pSerX;
	m_pTheSkyXForMounts				= pTheSkyXForMounts;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;

	m_bLinked = false;
    m_SkyRoof.SetSerxPointer(pSerX);
    m_SkyRoof.setLogger(pLogger);
    m_SkyRoof.setSleeper(pSleeper);
}


X2Dome::~X2Dome()
{
	if (m_pSerX)
		delete m_pSerX;
	if (m_pTheSkyXForMounts)
		delete m_pTheSkyXForMounts;
	if (m_pSleeper)
		delete m_pSleeper;
	if (m_pIniUtil)
		delete m_pIniUtil;
	if (m_pLogger)
		delete m_pLogger;
	if (m_pIOMutex)
		delete m_pIOMutex;
	if (m_pTickCount)
		delete m_pTickCount;

}


int X2Dome::establishLink(void)
{
    int nErr;
    char szPort[DRIVER_MAX_STRING];

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr =m_SkyRoof.Connect(szPort);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

	return nErr;
}

int X2Dome::terminateLink(void)
{
    X2MutexLocker ml(GetMutex());
    m_SkyRoof.Disconnect();
	m_bLinked = false;
	return SB_OK;
}

 bool X2Dome::isLinked(void) const
{
	return m_bLinked;
}


int X2Dome::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LoggerInterface_Name))
        *ppVal = GetLogger();
    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);
    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

    return SB_OK;
}

#pragma mark - UI binding

int X2Dome::execModalSettingsDialog()
{
    int nErr =SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;
    char szTmpBuf[SERIAL_BUFFER_SIZE];
    int	 nAtParkStatus;
    bool bDewHeaterOn;
    
    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr =ui->loadUserInterface("SkyRoof.ui", deviceType(), m_nPrivateISIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    memset(szTmpBuf,0,SERIAL_BUFFER_SIZE);

    if(m_bLinked) {
        dx->setEnabled("dewHeaterOnOff",true);
        dx->setEnabled("pushButton",true);
        // get AtPark Status
        nAtParkStatus = m_SkyRoof.getCurrentParkStatus();
        // set the field
        if(nAtParkStatus == PARKED){
            snprintf(szTmpBuf,16,"Parked");
            dx->setPropertyString("AtParkStatus","text", szTmpBuf);
        }
        else {
            snprintf(szTmpBuf,16,"Unparked");
            dx->setPropertyString("AtParkStatus","text", szTmpBuf);
        }
        bDewHeaterOn = m_SkyRoof.getDewHeaterStatus();
        if (bDewHeaterOn)
            dx->setChecked("dewHeaterOnOff", true);
        else
            dx->setChecked("dewHeaterOnOff", false);
    }
    else {
        dx->setEnabled("dewHeaterOnOff",false);
        snprintf(szTmpBuf,16,"NA");
        dx->setPropertyString("AtParkStatus","text", szTmpBuf);
        dx->setEnabled("pushButton",false);
    }
    X2MutexLocker ml(GetMutex());

    //Display the user interface
    if ((nErr =ui->exec(bPressedOK)))
        return nErr;

    return nErr;

}

void X2Dome::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    bool bDewHeaterOn;
    char szTmpBuf[SERIAL_BUFFER_SIZE];
    int     nAtParkStatus;

    if (!strcmp(pszEvent, "on_timer"))
    {
        m_nDewHeaterState = uiex->isChecked("dewHeaterOnOff");
        if(m_bLinked) {
            bDewHeaterOn = m_SkyRoof.getDewHeaterStatus();
            if(m_nDewHeaterState) {
                if(!bDewHeaterOn)
                    m_SkyRoof.enableDewHeater(true);
            }
            else {
                if(bDewHeaterOn)
                    m_SkyRoof.enableDewHeater(false);
            }
        }
        // get AtPark Status
        nAtParkStatus = m_SkyRoof.getCurrentParkStatus();
        // set the field
        if(nAtParkStatus == PARKED){
            snprintf(szTmpBuf,16,"Parked");
            uiex->setPropertyString("AtParkStatus","text", szTmpBuf);
        }
        else {
            snprintf(szTmpBuf,16,"Unparked");
            uiex->setPropertyString("AtParkStatus","text", szTmpBuf);
        }

    }
}

//
//HardwareInfoInterface
//
#pragma mark - HardwareInfoInterface

void X2Dome::deviceInfoNameShort(BasicStringInterface& str) const
{
	str = "SkyRoog system";
}

void X2Dome::deviceInfoNameLong(BasicStringInterface& str) const
{
    str = "SkyRoof system Roof Control";
}

void X2Dome::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
    str = "SkyRoof system roll-off roof Control";
}

 void X2Dome::deviceInfoFirmwareVersion(BasicStringInterface& str)
{
    str = "N/A";
}

void X2Dome::deviceInfoModel(BasicStringInterface& str)
{
    str = "RoR";
}

//
//DriverInfoInterface
//
#pragma mark - DriverInfoInterface

 void	X2Dome::driverInfoDetailedInfo(BasicStringInterface& str) const
{
    str = "SkyRoof system Roof Control X2 plugin by Rodolphe Pineau";
}

double	X2Dome::driverInfoVersion(void) const
{
	return DRIVER_VERSION;
}

//
//DomeDriverInterface
//
#pragma mark - DomeDriverInterface

int X2Dome::dapiGetAzEl(double* pdAz, double* pdEl)
{
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    *pdAz = m_SkyRoof.getCurrentAz();
    *pdEl = m_SkyRoof.getCurrentEl();
    return SB_OK;
}

int X2Dome::dapiGotoAzEl(double dAz, double dEl)
{
    int nErr;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr =m_SkyRoof.gotoAzimuth(dAz);
    if(nErr)
        return ERR_CMDFAILED;

    else
        return SB_OK;
}

int X2Dome::dapiAbort(void)
{

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    m_SkyRoof.abortCurrentCommand();
	return SB_OK;
}

int X2Dome::dapiOpen(void)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[X2Dome::dapiOpen] NOT CONNECTED");
        m_pLogger->out(mLogBuffer);
        return ERR_NOLINK;
    }

    nErr =m_SkyRoof.openShutter();
    if(nErr)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiClose(void)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[X2Dome::dapiClose] NOT CONNECTED");
        m_pLogger->out(mLogBuffer);
        return ERR_NOLINK;
    }

    nErr =m_SkyRoof.closeShutter();
    if(nErr)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiPark(void)
{
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;


	return SB_OK;
}

int X2Dome::dapiUnpark(void)
{
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

	return SB_OK;
}

int X2Dome::dapiFindHome(void)
{
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    return SB_OK;
}

int X2Dome::dapiIsGotoComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr =m_SkyRoof.isGoToComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;
    return SB_OK;
}

int X2Dome::dapiIsOpenComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr =m_SkyRoof.isOpenComplete(*pbComplete);
    if(nErr) {
        return ERR_CMDFAILED;
    }
    return SB_OK;
}

int	X2Dome::dapiIsCloseComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr =m_SkyRoof.isCloseComplete(*pbComplete);
    if(nErr) {
        return ERR_CMDFAILED;
    }
    return SB_OK;
}

int X2Dome::dapiIsParkComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr =m_SkyRoof.isParkComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsUnparkComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr =m_SkyRoof.isUnparkComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsFindHomeComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr =m_SkyRoof.isFindHomeComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiSync(double dAz, double dEl)
{
    int nErr;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr =m_SkyRoof.syncDome(dAz, dEl);
    if(nErr)
        return ERR_CMDFAILED;
	return SB_OK;
}

//
// SerialPortParams2Interface
//
#pragma mark - SerialPortParams2Interface

void X2Dome::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Dome::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort);

}


void X2Dome::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize,DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);

}



