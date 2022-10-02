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

    m_pSerx = NULL;
    m_bIsConnected = false;


    m_dCurrentAzPosition = 0.0;
    m_dCurrentElPosition = 0.0;


    m_bShutterOpened = false;
    m_nShutterState = UNKNOWN;

    m_bDewHeaterOn = false;

    m_CmdTimer.Reset();

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\X2_SkyRoof_Log.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_SkyRoof_Log";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_SkyRoof_Log";
#endif
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CSkyRoof] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CSkyRoof] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif

}

CSkyRoof::~CSkyRoof()
{

}

int CSkyRoof::Connect(const char *pszPort)
{
    int nErr;


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Called." << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Connecting to " << pszPort << std::endl;
    m_sLogFile.flush();
#endif

    // 9600 8N1
    if(m_pSerx->open(pszPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::this_thread::yield();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Connected to " << pszPort << std::endl;
    m_sLogFile.flush();
#endif

    // get the current shutter state just to check the connection, we don't care about the state for now.
    nErr = getShutterState(m_nShutterState);
    if(nErr) {
        nErr = false;
        return ERR_COMMNOLINK;
    }


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect]  SkyRoof init done." << std::endl;
    m_sLogFile.flush();
#endif

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


int CSkyRoof::domeCommand(const std::string sCmd, std::string &sResp, int nTimeout)
{
    int nErr = PLUGIN_OK;
    unsigned long  ulBytesWrite;
    std::string localResp;
    int dDelayMs;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [domeCommand] m_CmdTimer.GetElapsedSeconds() : " << m_CmdTimer.GetElapsedSeconds() << " s" << std::endl;
    m_sLogFile.flush();
#endif

    if(m_CmdTimer.GetElapsedSeconds()<(CMD_DELAY/1000.0)) {
        dDelayMs = int(CMD_DELAY - int(m_CmdTimer.GetElapsedSeconds() *1000));
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [domeCommand] sleeping for : " << std::dec << dDelayMs << "ms" << std::endl;
        m_sLogFile.flush();
#endif
        if(dDelayMs>0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(dDelayMs));
            std::this_thread::yield();
        }
    }

    m_pSerx->purgeTxRx();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [domeCommand] Sending : " << sCmd << std::endl;
    m_sLogFile.flush();
#endif
    nErr = m_pSerx->writeFile((void *)(sCmd.c_str()), sCmd.size(), ulBytesWrite);
    m_pSerx->flushTx();

    m_CmdTimer.Reset();

    if(nErr){
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [domeCommand] writeFile error : " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

    if(nTimeout == 0)
        return nErr;

    // read response
    nErr = readResponse(localResp, nTimeout);
    if(nErr)
        return nErr;

    if(!localResp.size())
        return BAD_CMD_RESPONSE;

    sResp.assign(trim(localResp,"\n\r "));

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [domeCommand] response : " << sResp << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CSkyRoof::readResponse(std::string &sResp, int nTimeout)
{
    int nErr = PLUGIN_OK;
    char pszBuf[SERIAL_BUFFER_SIZE];
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
    int nBytesWaiting = 0 ;
    int nbTimeouts = 0;

    sResp.clear();
    memset(pszBuf, 0, SERIAL_BUFFER_SIZE);
    pszBufPtr = pszBuf;

    do {
        nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] nBytesWaiting = " << nBytesWaiting << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] nBytesWaiting nErr = " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        if(!nBytesWaiting) {
            nbTimeouts += MAX_READ_WAIT_TIMEOUT;
            if(nbTimeouts >= nTimeout) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
                m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] bytesWaitingRx timeout, no data for " << nbTimeouts <<" ms" << std::endl;
                m_sLogFile.flush();
#endif
                nErr = COMMAND_TIMEOUT;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(MAX_READ_WAIT_TIMEOUT));
            continue;
        }
        nbTimeouts = 0;
        if(ulTotalBytesRead + nBytesWaiting <= SERIAL_BUFFER_SIZE)
            nErr = m_pSerx->readFile(pszBufPtr, nBytesWaiting, ulBytesRead, nTimeout);
        else {
            nErr = ERR_RXTIMEOUT;
            break; // buffer is full.. there is a problem !!
        }
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile error." << std::endl;
            m_sLogFile.flush();
#endif
            return nErr;
        }

        if (ulBytesRead != nBytesWaiting) { // timeout
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile Timeout Error." << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile nBytesWaiting = " << nBytesWaiting << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile ulBytesRead =" << ulBytesRead << std::endl;
            m_sLogFile.flush();
#endif
        }

        ulTotalBytesRead += ulBytesRead;
        pszBufPtr+=ulBytesRead;
    } while (ulTotalBytesRead < SERIAL_BUFFER_SIZE  && *(pszBufPtr-1) != 0x0a);

    if(!ulTotalBytesRead)
        nErr = COMMAND_TIMEOUT; // we didn't get an answer.. so timeout
    else
        *(pszBufPtr-1) = 0; //remove the 0x0d

    sResp.assign(pszBuf);
    return nErr;
}


int CSkyRoof::getDomeAz(double &dDomeAz)
{
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;


    // convert Az string to double
    dDomeAz = m_dCurrentAzPosition;
    return nErr;
}

int CSkyRoof::getDomeEl(double &dDomeEl)
{
    int nErr = PLUGIN_OK;

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
    int nErr = PLUGIN_OK;
    std::string sResp;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getShutterState] called." << std::endl;
    m_sLogFile.flush();
#endif
    nErr = domeCommand("Status#\r", sResp);
    if(nErr)
        return nErr;


    if(sResp.find("Open")!= -1)  {
        nState = OPEN;
        m_bShutterOpened = true;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getShutterState] Shutter is open." << std::endl;
        m_sLogFile.flush();
#endif
    } else if(sResp.find("Close")!= -1)  {
        nState = CLOSED;
        m_bShutterOpened = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getShutterState] Shutter is closed." << std::endl;
        m_sLogFile.flush();
#endif
    } else if(sResp.find("Safety")!= -1)  {
        nState = SAFETY;
        m_bShutterOpened = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getShutterState] Shutter is moving or stopped in the middle." << std::endl;
        m_sLogFile.flush();
#endif
    } else {
        nState = UNKNOWN;
        m_bShutterOpened = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getShutterState] Shutter state is unknown." << std::endl;
        m_sLogFile.flush();
#endif
    }

    return nErr;
}


int CSkyRoof::syncDome(double dAz, double dEl)
{
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_dCurrentAzPosition = dAz;
    return nErr;
}

int CSkyRoof::parkDome()
{
    int nErr = PLUGIN_OK;

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
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_dCurrentAzPosition = dNewAz;

    return nErr;
}

int CSkyRoof::openShutter()
{
    int nErr = PLUGIN_OK;
    int nStatus;
    std::string sResp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [openShutter] called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected) {
        return NOT_CONNECTED;
    }

    // get the AtPark status
    nErr = getAtParkStatus(nStatus);
    if(nErr)
        return nErr;

    // we can't move the roof if we're not parked
    if (nStatus != PARKED) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [openShutter] Not parked, not moving the roof." << std::endl;
            m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    nErr = domeCommand("Open#\r", sResp);
    if(nErr)
        return nErr;

    if(sResp.find("0#") == -1)  {
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}

int CSkyRoof::closeShutter()
{
    int nErr = PLUGIN_OK;
    int nStatus;
    std::string sResp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [closeShutter] called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected) {
        return NOT_CONNECTED;
    }

    // get the AtPark status
    nErr = getAtParkStatus(nStatus);
    if(nErr)
        return nErr;


    // we can't move the roof if we're not parked
    if (nStatus != PARKED) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [closeShutter] Not parked, not moving the roof." << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    nErr = domeCommand("Close#\r", sResp);
    if(nErr)
        return nErr;

    if(sResp.find("0#") == -1)  {
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}


int CSkyRoof::isGoToComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;
    bComplete = true;

    return nErr;
}

int CSkyRoof::isOpenComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isOpenComplete] called." << std::endl;
    m_sLogFile.flush();
#endif

    nErr = getShutterState(m_nShutterState);
    if(nErr)
        return ERR_CMDFAILED;

    if(m_nShutterState == OPEN) {
        m_bShutterOpened = true;
        bComplete = true;
        m_dCurrentElPosition = 90.0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isOpenComplete] Roof is open." << std::endl;
        m_sLogFile.flush();
#endif
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
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isCloseComplete] called." << std::endl;
    m_sLogFile.flush();
#endif

    nErr = getShutterState(m_nShutterState);
    if(nErr)
        return ERR_CMDFAILED;

    if(m_nShutterState == CLOSED){
        m_bShutterOpened = false;
        bComplete = true;
        m_dCurrentElPosition = 0.0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isCloseComplete] Roof is closed." << std::endl;
        m_sLogFile.flush();
#endif
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
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    bComplete = true;
    return nErr;
}

int CSkyRoof::isUnparkComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    bComplete = true;

    return nErr;
}

int CSkyRoof::isFindHomeComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;
    bComplete = true;

    return nErr;

}

int CSkyRoof::abortCurrentCommand()
{

    int nErr = PLUGIN_OK;
    std::string sResp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [abortCurrentCommand] called." << std::endl;
    m_sLogFile.flush();
#endif


    if(!m_bIsConnected) {
        return NOT_CONNECTED;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [abortCurrentCommand] Sending abort command." << std::endl;
    m_sLogFile.flush();
#endif

    nErr = domeCommand("Stop#\r", sResp);
    if(nErr)
        return nErr;

    if(sResp.find("0#") == -1)  {
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}


int CSkyRoof::enableDewHeater(bool bEnable)
{
    int nErr = PLUGIN_OK;
    std::string sResp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [enableDewHeater] called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected) {
        return NOT_CONNECTED;
    }
    
    if(bEnable) {
        nErr = domeCommand("HeaterOn#\r", sResp, 0);
    }
    else {
        nErr = domeCommand("HeaterOff#\r", sResp, 0);
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
    int nErr = PLUGIN_OK;
    std::string sResp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getAtParkStatus] called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected) {
        return NOT_CONNECTED;
    }

    nErr = domeCommand("Parkstatus#\r", sResp);
    if(nErr)
        return nErr;

    if(sResp.find("0#") != -1)  {
        nStatus = PARKED; //Parked
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getAtParkStatus] PARKED." << std::endl;
        m_sLogFile.flush();
#endif
    }
    else {
        nStatus = UNPARKED; //Parked
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getAtParkStatus] UNPARKED." << std::endl;
        m_sLogFile.flush();
#endif
    }
    return nErr;
}

bool CSkyRoof::getDewHeaterStatus()
{
    return m_bDewHeaterOn;
}


std::string& CSkyRoof::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CSkyRoof::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CSkyRoof::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}

std::string CSkyRoof::findField(std::vector<std::string> &svFields, const std::string& token)
{
    for(int i=0; i<svFields.size(); i++){
        if(svFields[i].find(token)!= -1) {
            return svFields[i];
        }
    }
    return std::string();
}


#ifdef PLUGIN_DEBUG
const std::string CSkyRoof::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif
