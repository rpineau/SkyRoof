//
//  SkyRoof.cpp
//  CSkyRoof
//
//  Created by Rodolphe Pineau on 2017-4-6
//  SkyRoof X2 plugin

#include "SkyRoof.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

CSkyRoof::CSkyRoof()
{
    // set some sane values
    m_bDebugLog = false;

    m_pSerx = NULL;
    m_bIsConnected = false;


    m_dCurrentAzPosition = 0.0;
    m_dCurrentElPosition = 0.0;


    m_bShutterOpened = false;
    m_nShutterState = UNKNOWN;

    m_bDewHeaterOn = false;
    
    memset(m_szLogBuffer,0,ND_LOG_BUFFER_SIZE);
}

CSkyRoof::~CSkyRoof()
{

}

int CSkyRoof::Connect(const char *pszPort)
{
    int nErr;

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::CSkyRoof] Starting log for version 1.0");
        m_pLogger->out(m_szLogBuffer);
    }

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] Trying to connect to %s.", pszPort);
        m_pLogger->out(m_szLogBuffer);
    }

    // 9600 8N1
    if(m_pSerx->open(pszPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    m_pSleeper->sleep(2000);
    
    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] Connected.");
        m_pLogger->out(m_szLogBuffer);
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] Getting shutter state.");
        m_pLogger->out(m_szLogBuffer);
    }

    // get the current shutter state just to check the connection, we don't care about the state for now.
    nErr = getShutterState(m_nShutterState);
    if(nErr) {
        nErr = false;
        return ERR_COMMNOLINK;
    }

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] SkyRoof init done.");
        m_pLogger->out(m_szLogBuffer);
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] m_bIsConnected = %u.", m_bIsConnected);
        m_pLogger->out(m_szLogBuffer);
    }

    syncDome(m_dCurrentAzPosition,m_dCurrentElPosition);

    return SB_OK;
}


void CSkyRoof::Disconnect()
{
    if(m_bIsConnected) {
        m_pSerx->purgeTxRx();
        m_pSerx->close();
    }
    m_bIsConnected = false;
}


int CSkyRoof::readResponse(char *pszRespBuffer, unsigned int nBufferLen)
{
    int nErr = RoR_OK;
    unsigned long ulBytesRead = 0;
    unsigned int ulTotalBytesRead = 0;
    char *pszBufPtr;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
            if (m_bDebugLog) {
                snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::readResponse] readFile error.");
                m_pLogger->out(m_szLogBuffer);
            }
            return nErr;
        }
        if (ulBytesRead !=1) {// timeout
            nErr = RoR_BAD_CMD_RESPONSE;
            if (m_bDebugLog) {
                snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::readResponse] readFile Timeout while getting response.");
                m_pLogger->out(m_szLogBuffer);
                snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::readResponse] nBytesRead = %lu", ulBytesRead);
                m_pLogger->out(m_szLogBuffer);
            }
            break;
        }
        ulTotalBytesRead += ulBytesRead;
    } while (*pszBufPtr++ != 0x0d && ulTotalBytesRead < nBufferLen ); // \r

    *pszBufPtr = 0; //remove the \r
    return nErr;
}


int CSkyRoof::domeCommand(const char *pszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = RoR_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    m_pSerx->purgeTxRx();
    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::domeCommand] Sending %s",pszCmd);
        m_pLogger->out(m_szLogBuffer);
    }
    nErr = m_pSerx->writeFile((void *)pszCmd, strlen(pszCmd), nBytesWrite);
    m_pSerx->flushTx();
    if(nErr)
        return nErr;

    // only read the response if we expect a response.
    if(pszResult) {
        nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::domeCommand] response = %s", szResp);
            m_pLogger->out(m_szLogBuffer);
        }
        
        if(nErr)
            return nErr;
        strncpy(pszResult, szResp, nResultMaxLen);
    }

    return nErr;
}


int CSkyRoof::getDomeAz(double &dDomeAz)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;


    // convert Az string to double
    dDomeAz = m_dCurrentAzPosition;
    return nErr;
}

int CSkyRoof::getDomeEl(double &dDomeEl)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(!m_bShutterOpened)
    {
        dDomeEl = 0.0;
        return nErr;
    }

    dDomeEl = m_dCurrentElPosition;

    return nErr;
}


int CSkyRoof::getShutterState(int &nState)
{
    int nErr = RoR_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState]");
        m_pLogger->out(m_szLogBuffer);
    }
    m_pSleeper->sleep(CMD_DELAY);
    nErr = domeCommand("Status#\r", szResp,  SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp, "Open")) {
        nState = OPEN;
        m_bShutterOpened = true;
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState] Shutter is opened");
            m_pLogger->out(m_szLogBuffer);
        }
    } else if (strstr(szResp, "Close")) {
        nState = CLOSED;
        m_bShutterOpened = false;
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState] Shutter is closed");
            m_pLogger->out(m_szLogBuffer);
        }
    } else if (strstr(szResp, "Safety")) {
        nState = SAFETY;
        m_bShutterOpened = false;
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState] Shutter is moving or stopped in the middle");
            m_pLogger->out(m_szLogBuffer);
        }
    } else {
        nState = UNKNOWN;
        m_bShutterOpened = false;
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState] Shutter state is unknown");
            m_pLogger->out(m_szLogBuffer);
        }
    }

    return nErr;
}


void CSkyRoof::setDebugLog(bool bEnable)
{
    m_bDebugLog = bEnable;
}


int CSkyRoof::syncDome(double dAz, double dEl)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_dCurrentAzPosition = dAz;
    return nErr;
}

int CSkyRoof::parkDome()
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    return nErr;

}

int CSkyRoof::unparkDome()
{
    syncDome(m_dCurrentAzPosition,m_dCurrentElPosition);
    return 0;
}

int CSkyRoof::gotoAzimuth(double dNewAz)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_dCurrentAzPosition = dNewAz;

    return nErr;
}

int CSkyRoof::openShutter()
{
    int nErr = RoR_OK;
    int nStatus;
    char szResp[SERIAL_BUFFER_SIZE];

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::openShutter]");
        m_pLogger->out(m_szLogBuffer);
    }

    if(!m_bIsConnected) {
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::openShutter] NOT CONNECTED !!!!");
            m_pLogger->out(m_szLogBuffer);
        }
        return NOT_CONNECTED;
    }

    // get the AtPark status
    nErr = getAtParkStatus(nStatus);
    if(nErr)
        return nErr;

    // we can't move the roof if we're not parked
    if (nStatus != PARKED) {
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::openShutter] Not parked, not moving the roof");
            m_pLogger->out(m_szLogBuffer);
        }
        return ERR_CMDFAILED;
    }

    m_pSleeper->sleep(CMD_DELAY);
    nErr = domeCommand("Open#\r", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp, "0#")) {
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}

int CSkyRoof::closeShutter()
{
    int nErr = RoR_OK;
    int nStatus;
    char szResp[SERIAL_BUFFER_SIZE];

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::closeShutter]");
        m_pLogger->out(m_szLogBuffer);
    }

    if(!m_bIsConnected) {
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::closeShutter] NOT CONNECTED !!!!");
            m_pLogger->out(m_szLogBuffer);
        }
        return NOT_CONNECTED;
    }

    // get the AtPark status
    nErr = getAtParkStatus(nStatus);
    if(nErr)
        return nErr;


    // we can't move the roof if we're not parked
    if (nStatus != PARKED) {
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::closeShutter] Not parked, not moving the roof");
            m_pLogger->out(m_szLogBuffer);
        }
        return ERR_CMDFAILED;
    }

    m_pSleeper->sleep(CMD_DELAY);
    nErr = domeCommand("Close#\r", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp, "0#")) {
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}


int CSkyRoof::isGoToComplete(bool &bComplete)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;
    bComplete = true;

    return nErr;
}

int CSkyRoof::isOpenComplete(bool &bComplete)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::isOpenComplete] Checking roof state");
        m_pLogger->out(m_szLogBuffer);
    }

    nErr = getShutterState(m_nShutterState);
    if(nErr)
        return ERR_CMDFAILED;

    if(m_nShutterState == OPEN){
        m_bShutterOpened = true;
        bComplete = true;
        m_dCurrentElPosition = 90.0;
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::isOpenComplete] Roof is opened");
            m_pLogger->out(m_szLogBuffer);
        }
    }
    else {
        m_bShutterOpened = false;
        bComplete = false;
        m_dCurrentElPosition = 0.0;
    }

    return nErr;
}

int CSkyRoof::isCloseComplete(bool &bComplete)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::isCloseComplete] Checking roof state");
        m_pLogger->out(m_szLogBuffer);
    }

    nErr = getShutterState(m_nShutterState);
    if(nErr)
        return ERR_CMDFAILED;

    if(m_nShutterState == CLOSED){
        m_bShutterOpened = false;
        bComplete = true;
        m_dCurrentElPosition = 0.0;
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::isOpenComplete] Roof is closed");
            m_pLogger->out(m_szLogBuffer);
        }
    }
    else {
        m_bShutterOpened = true;
        bComplete = false;
        m_dCurrentElPosition = 90.0;
    }

    return nErr;
}


int CSkyRoof::isParkComplete(bool &bComplete)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    bComplete = true;
    return nErr;
}

int CSkyRoof::isUnparkComplete(bool &bComplete)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    bComplete = true;

    return nErr;
}

int CSkyRoof::isFindHomeComplete(bool &bComplete)
{
    int nErr = RoR_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;
    bComplete = true;

    return nErr;

}

int CSkyRoof::abortCurrentCommand()
{

    int nErr = RoR_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::abortCurrentCommand]");
        m_pLogger->out(m_szLogBuffer);
    }

    if(!m_bIsConnected) {
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::abortCurrentCommand] NOT CONNECTED !!!!");
            m_pLogger->out(m_szLogBuffer);
        }
        return NOT_CONNECTED;
    }

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::abortCurrentCommand] Sending abort command.");
        m_pLogger->out(m_szLogBuffer);
    }

    m_pSleeper->sleep(CMD_DELAY);
    nErr = domeCommand("Stop#\r", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp, "0#")) {
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}


int CSkyRoof::enableDewHeater(bool bEnable)
{
    int nErr = RoR_OK;
	
    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::enableDewHeater]");
        m_pLogger->out(m_szLogBuffer);
    }
    
    if(!m_bIsConnected) {
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::enableDewHeater] NOT CONNECTED !!!!");
            m_pLogger->out(m_szLogBuffer);
        }
        return NOT_CONNECTED;
    }
    
    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::enableDewHeater] Sending dew heater command.");
        m_pLogger->out(m_szLogBuffer);
    }
    
    m_pSleeper->sleep(CMD_DELAY);
    if(bEnable) {
        nErr = domeCommand("HeaterOn#\r", NULL, SERIAL_BUFFER_SIZE);
    }
    else {
        nErr = domeCommand("HeaterOff#\r", NULL, SERIAL_BUFFER_SIZE);
    }
    if(nErr)
        return nErr;

    m_bDewHeaterOn = bEnable;

    return nErr;
  
}

#pragma mark - Getter / Setter


double CSkyRoof::getCurrentAz()
{
    if(m_bIsConnected)
        getDomeAz(m_dCurrentAzPosition);

    return m_dCurrentAzPosition;
}

double CSkyRoof::getCurrentEl()
{
    if(m_bIsConnected)
        getDomeEl(m_dCurrentElPosition);

    return m_dCurrentElPosition;
}

int CSkyRoof::getCurrentShutterState()
{
    if(m_bIsConnected)
        getShutterState(m_nShutterState);

    return m_nShutterState;
}

int CSkyRoof::getCurrentParkStatus()
{
    if(m_bIsConnected)
        getAtParkStatus(m_nAtParkStatus);

    return m_nAtParkStatus;

}

int CSkyRoof::getAtParkStatus(int &nStatus)
{
    int nErr = RoR_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus]");
        m_pLogger->out(m_szLogBuffer);
    }

    if(!m_bIsConnected) {
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus] NOT CONNECTED !!!!");
            m_pLogger->out(m_szLogBuffer);
        }
        return NOT_CONNECTED;
    }

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus] Sending Parkstatus command.");
        m_pLogger->out(m_szLogBuffer);
    }

    m_pSleeper->sleep(CMD_DELAY);
    nErr = domeCommand("Parkstatus#\r", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp, "0#")) {
        nStatus = PARKED; //Parked
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus] PARKED.");
            m_pLogger->out(m_szLogBuffer);
        }
    }
    else {
        nStatus = UNPARKED; //Parked
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus] UNPARKED.");
            m_pLogger->out(m_szLogBuffer);
        }
    }
    return nErr;
}

bool CSkyRoof::getDewHeaterStatus()
{
    return m_bDewHeaterOn;
}
