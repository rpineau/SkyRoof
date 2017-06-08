//
//  SkyRoof.h
//  CSkyRoof
//
//  Created by Rodolphe Pineau on 2017-4-6
//  SkyRoof X2 plugin

#ifndef __m1_OASYS__
#define __m1_OASYS__
#include <math.h>
#include <string.h>
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 500
#define ND_LOG_BUFFER_SIZE 256
#define CMD_DELAY   750
// error codes
enum SkyRoofErrors {RoR_OK=0, NOT_CONNECTED, RoR_CANT_CONNECT, RoR_BAD_CMD_RESPONSE, COMMAND_FAILED};

// Error code
enum SkyRoofShutterState {OPEN=1, OPENING, CLOSED, CLOSING, SAFETY, SHUTTER_ERROR, UNKNOWN};
enum AtParkStatus {PARKED = 0, UNPARKED};

class CSkyRoof
{
public:
    CSkyRoof();
    ~CSkyRoof();

    int         Connect(const char *szPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; }

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; }
    void        setLogger(LoggerInterface *pLogger) { m_pLogger = pLogger; };
    void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };

    // Dome commands
    int syncDome(double dAz, double dEl);
    int parkDome(void);
    int unparkDome(void);
    int gotoAzimuth(double dNewAz);
    int openShutter();
    int closeShutter();

    // command complete functions
    int isGoToComplete(bool &bComplete);
    int isOpenComplete(bool &bComplete);
    int isCloseComplete(bool &bComplete);
    int isParkComplete(bool &bComplete);
    int isUnparkComplete(bool &bComplete);
    int isFindHomeComplete(bool &bComplete);
    int isCalibratingComplete(bool &bComplete);

    int abortCurrentCommand();

    double getCurrentAz();
    double getCurrentEl();

    int getCurrentShutterState();
    int getCurrentParkStatus();
    void setDebugLog(bool enable);

    int enableDewHeater(bool enable);
    bool getDewHeaterStatus();
    
protected:

    int             readResponse(char *pszRespBuffer, unsigned int nBufferLen);
    int             getDomeAz(double &dDomeAz);
    int             getDomeEl(double &dDomeEl);
    int             getShutterState(int &nState);
    int             getAtParkStatus(int &nStatus);

    int             domeCommand(const char *pszCmd, char *pszResult, int nResultMaxLen);

    LoggerInterface *m_pLogger;
    SleeperInterface    *m_pSleeper;

    bool            m_bDebugLog;

    bool            m_bIsConnected;
    bool            m_bShutterOpened;

    double          m_dCurrentAzPosition;
    double          m_dCurrentElPosition;

    SerXInterface   *m_pSerx;

    int             m_nShutterState;
    int             m_nAtParkStatus;
    bool            m_bDewHeaterOn;
    
    char            m_szLogBuffer[ND_LOG_BUFFER_SIZE];
};

#endif
