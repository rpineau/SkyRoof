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
    bDebugLog = false;

    pSerx = NULL;
    bIsConnected = false;


    mCurrentAzPosition = 0.0;
    mCurrentElPosition = 0.0;


    mShutterOpened = false;
    mShutterState = UNKNOWN;

    mDewHeaterOn = false;
    
    memset(mLogBuffer,0,ND_LOG_BUFFER_SIZE);
}

CSkyRoof::~CSkyRoof()
{

}

int CSkyRoof::Connect(const char *szPort)
{
    int err;

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::CSkyRoof] Starting log for version 1.0");
        mLogger->out(mLogBuffer);
    }

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] Trying to connect to %s.", szPort);
        mLogger->out(mLogBuffer);
    }

    // 9600 8N1
    if(pSerx->open(szPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        bIsConnected = true;
    else
        bIsConnected = false;

    if(!bIsConnected)
        return ERR_COMMNOLINK;

    mSleeper->sleep(2000);
    
    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] Connected.");
        mLogger->out(mLogBuffer);
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] Getting shutter state.");
        mLogger->out(mLogBuffer);
    }

    // get the current shutter state just to check the connection, we don't care about the state for now.
    err = getShutterState(mShutterState);
    if(err) {
        bIsConnected = false;
        return ERR_COMMNOLINK;
    }

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] SkyRoof init done.");
        mLogger->out(mLogBuffer);
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::Connect] bIsConnected = %u.", bIsConnected);
        mLogger->out(mLogBuffer);
    }

    syncDome(mCurrentAzPosition,mCurrentElPosition);

    return SB_OK;
}


void CSkyRoof::Disconnect()
{
    if(bIsConnected) {
        pSerx->purgeTxRx();
        pSerx->close();
    }
    bIsConnected = false;
}


int CSkyRoof::readResponse(char *respBuffer, unsigned int bufferLen)
{
    int err = RoR_OK;
    unsigned long nBytesRead = 0;
    unsigned int totalBytesRead = 0;
    char *bufPtr;

    memset(respBuffer, 0, (size_t) bufferLen);
    bufPtr = respBuffer;

    do {
        err = pSerx->readFile(bufPtr, 1, nBytesRead, MAX_TIMEOUT);
        if(err) {
            if (bDebugLog) {
                snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::readResponse] readFile error.");
                mLogger->out(mLogBuffer);
            }
            return err;
        }
        if (nBytesRead !=1) {// timeout
            err = RoR_BAD_CMD_RESPONSE;
            if (bDebugLog) {
                snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::readResponse] readFile Timeout while getting response.");
                mLogger->out(mLogBuffer);
                snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::readResponse] nBytesRead = %lu", nBytesRead);
                mLogger->out(mLogBuffer);
            }
            break;
        }
        totalBytesRead += nBytesRead;
    } while (*bufPtr++ != 0x0d && totalBytesRead < bufferLen ); // \r

    *bufPtr = 0; //remove the \r
    return err;
}


int CSkyRoof::domeCommand(const char *cmd, char *result, int resultMaxLen)
{
    int err = RoR_OK;
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    pSerx->purgeTxRx();
    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::domeCommand] Sending %s",cmd);
        mLogger->out(mLogBuffer);
    }
    err = pSerx->writeFile((void *)cmd, strlen(cmd), nBytesWrite);
    pSerx->flushTx();
    if(err)
        return err;

    // only read the response if we expect a response.
    if(result) {
        err = readResponse(resp, SERIAL_BUFFER_SIZE);
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::domeCommand] response = %s", resp);
            mLogger->out(mLogBuffer);
        }
        
        if(err)
            return err;
        strncpy(result, resp, resultMaxLen);
    }

    return err;

}


int CSkyRoof::getDomeAz(double &domeAz)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;


    // convert Az string to double
    domeAz = mCurrentAzPosition;
    return err;
}

int CSkyRoof::getDomeEl(double &domeEl)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(!mShutterOpened)
    {
        domeEl = 0.0;
        return err;
    }

    domeEl = mCurrentElPosition;

    return err;
}


int CSkyRoof::getShutterState(int &state)
{
    int err = RoR_OK;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState]");
        mLogger->out(mLogBuffer);
    }
    mSleeper->sleep(CMD_DELAY);
    err = domeCommand("Status#\r", resp,  SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    if(strstr(resp,"Open")) {
        state = OPEN;
        mShutterOpened = true;
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState] Shutter is opened");
            mLogger->out(mLogBuffer);
        }
    } else if (strstr(resp,"Close")) {
        state = CLOSED;
        mShutterOpened = false;
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState] Shutter is closed");
            mLogger->out(mLogBuffer);
        }
    } else if (strstr(resp,"Safety")) {
        state = SAFETY;
        mShutterOpened = false;
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState] Shutter is moving or stopped in the middle");
            mLogger->out(mLogBuffer);
        }
    } else {
        state = UNKNOWN;
        mShutterOpened = false;
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getShutterState] Shutter state is unknown");
            mLogger->out(mLogBuffer);
        }
    }

    return err;
}


void CSkyRoof::setDebugLog(bool enable)
{
    bDebugLog = enable;
}


int CSkyRoof::syncDome(double dAz, double dEl)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;

    mCurrentAzPosition = dAz;
    return err;
}

int CSkyRoof::parkDome()
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;

    return err;

}

int CSkyRoof::unparkDome()
{
    syncDome(mCurrentAzPosition,mCurrentElPosition);
    return 0;
}

int CSkyRoof::gotoAzimuth(double newAz)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;

    mCurrentAzPosition = newAz;

    return err;
}

int CSkyRoof::openShutter()
{
    int err = RoR_OK;
    int status;
    char resp[SERIAL_BUFFER_SIZE];

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::openShutter]");
        mLogger->out(mLogBuffer);
    }

    if(!bIsConnected) {
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::openShutter] NOT CONNECTED !!!!");
            mLogger->out(mLogBuffer);
        }
        return NOT_CONNECTED;
    }

    // get the AtPark status
    err = getAtParkStatus(status);
    if(err)
        return err;

    // we can't move the roof if we're not parked
    if (status != PARKED) {
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::openShutter] Not parked, not moving the roof");
            mLogger->out(mLogBuffer);
        }
        return ERR_CMDFAILED;
    }

    mSleeper->sleep(CMD_DELAY);
    err = domeCommand("Open#\r", resp, SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    if(!strstr(resp,"0#")) {
        err = ERR_CMDFAILED;
    }

    return err;
}

int CSkyRoof::closeShutter()
{
    int err = RoR_OK;
    int status;
    char resp[SERIAL_BUFFER_SIZE];

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::closeShutter]");
        mLogger->out(mLogBuffer);
    }

    if(!bIsConnected) {
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::closeShutter] NOT CONNECTED !!!!");
            mLogger->out(mLogBuffer);
        }
        return NOT_CONNECTED;
    }

    // get the AtPark status
    err = getAtParkStatus(status);
    if(err)
        return err;


    // we can't move the roof if we're not parked
    if (status != PARKED) {
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::closeShutter] Not parked, not moving the roof");
            mLogger->out(mLogBuffer);
        }
        return ERR_CMDFAILED;
    }

    mSleeper->sleep(CMD_DELAY);
    err = domeCommand("Close#\r", resp, SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    if(!strstr(resp,"0#")) {
        err = ERR_CMDFAILED;
    }

    return err;
}


int CSkyRoof::isGoToComplete(bool &complete)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;
    complete = true;

    return err;
}

int CSkyRoof::isOpenComplete(bool &complete)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::isOpenComplete] Checking roof state");
        mLogger->out(mLogBuffer);
    }

    err = getShutterState(mShutterState);
    if(err)
        return ERR_CMDFAILED;

    if(mShutterState == OPEN){
        mShutterOpened = true;
        complete = true;
        mCurrentElPosition = 90.0;
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::isOpenComplete] Roof is opened");
            mLogger->out(mLogBuffer);
        }
    }
    else {
        mShutterOpened = false;
        complete = false;
        mCurrentElPosition = 0.0;
    }

    return err;
}

int CSkyRoof::isCloseComplete(bool &complete)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::isCloseComplete] Checking roof state");
        mLogger->out(mLogBuffer);
    }

    err = getShutterState(mShutterState);
    if(err)
        return ERR_CMDFAILED;

    if(mShutterState == CLOSED){
        mShutterOpened = false;
        complete = true;
        mCurrentElPosition = 0.0;
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::isOpenComplete] Roof is closed");
            mLogger->out(mLogBuffer);
        }
    }
    else {
        mShutterOpened = true;
        complete = false;
        mCurrentElPosition = 90.0;
    }

    return err;
}


int CSkyRoof::isParkComplete(bool &complete)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;

    complete = true;
    return err;
}

int CSkyRoof::isUnparkComplete(bool &complete)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;

    complete = true;

    return err;
}

int CSkyRoof::isFindHomeComplete(bool &complete)
{
    int err = RoR_OK;

    if(!bIsConnected)
        return NOT_CONNECTED;
    complete = true;

    return err;

}

int CSkyRoof::abortCurrentCommand()
{

    int err = RoR_OK;
    char resp[SERIAL_BUFFER_SIZE];

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::abortCurrentCommand]");
        mLogger->out(mLogBuffer);
    }

    if(!bIsConnected) {
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::abortCurrentCommand] NOT CONNECTED !!!!");
            mLogger->out(mLogBuffer);
        }
        return NOT_CONNECTED;
    }

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::abortCurrentCommand] Sending abort command.");
        mLogger->out(mLogBuffer);
    }

    mSleeper->sleep(CMD_DELAY);
    err = domeCommand("Stop#\r", resp, SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    if(!strstr(resp,"0#")) {
        err = ERR_CMDFAILED;
    }

    return err;
}


int CSkyRoof::enableDewHeater(bool enable)
{
    int err = RoR_OK;
    char resp[SERIAL_BUFFER_SIZE];
    
    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::enableDewHeater]");
        mLogger->out(mLogBuffer);
    }
    
    if(!bIsConnected) {
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::enableDewHeater] NOT CONNECTED !!!!");
            mLogger->out(mLogBuffer);
        }
        return NOT_CONNECTED;
    }
    
    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::enableDewHeater] Sending dew heater command.");
        mLogger->out(mLogBuffer);
    }
    
    mSleeper->sleep(CMD_DELAY);
    if(enable) {
        err = domeCommand("HeaterOn#\r", NULL, SERIAL_BUFFER_SIZE);
    }
    else {
        err = domeCommand("HeaterOff#\r", NULL, SERIAL_BUFFER_SIZE);
    }
    if(err)
        return err;

    mDewHeaterOn = enable;

    return err;
  
}

#pragma mark - Getter / Setter


double CSkyRoof::getCurrentAz()
{
    if(bIsConnected)
        getDomeAz(mCurrentAzPosition);

    return mCurrentAzPosition;
}

double CSkyRoof::getCurrentEl()
{
    if(bIsConnected)
        getDomeEl(mCurrentElPosition);

    return mCurrentElPosition;
}

int CSkyRoof::getCurrentShutterState()
{
    if(bIsConnected)
        getShutterState(mShutterState);

    return mShutterState;
}

int CSkyRoof::getCurrentParkStatus()
{
    if(bIsConnected)
        getAtParkStatus(mAtParkStatus);

    return mAtParkStatus;

}

int CSkyRoof::getAtParkStatus(int &status)
{
    int err = RoR_OK;
    char resp[SERIAL_BUFFER_SIZE];

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus]");
        mLogger->out(mLogBuffer);
    }

    if(!bIsConnected) {
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus] NOT CONNECTED !!!!");
            mLogger->out(mLogBuffer);
        }
        return NOT_CONNECTED;
    }

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus] Sending Parkstatus command.");
        mLogger->out(mLogBuffer);
    }

    mSleeper->sleep(CMD_DELAY);
    err = domeCommand("Parkstatus#\r", resp, SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    if(strstr(resp,"0#")) {
        status = PARKED; //Parked
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus] PARKED.");
            mLogger->out(mLogBuffer);
        }
    }
    else {
        status = UNPARKED; //Parked
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CSkyRoof::getAtParkStatus] UNPARKED.");
            mLogger->out(mLogBuffer);
        }
    }
    return err;
}

bool CSkyRoof::getDewHeaterStatus()
{
    return mDewHeaterOn;
}
