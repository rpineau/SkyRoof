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

    int        Connect(const char *szPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return bIsConnected; }

    void        SetSerxPointer(SerXInterface *p) { pSerx = p; }
    void        setLogger(LoggerInterface *pLogger) { mLogger = pLogger; };
    void        setSleeper(SleeperInterface *pSleeper) { mSleeper = pSleeper; };

    // Dome commands
    int syncDome(double dAz, double dEl);
    int parkDome(void);
    int unparkDome(void);
    int gotoAzimuth(double newAz);
    int openShutter();
    int closeShutter();

    // command complete functions
    int isGoToComplete(bool &complete);
    int isOpenComplete(bool &complete);
    int isCloseComplete(bool &complete);
    int isParkComplete(bool &complete);
    int isUnparkComplete(bool &complete);
    int isFindHomeComplete(bool &complete);
    int isCalibratingComplete(bool &complete);

    int abortCurrentCommand();

    double getCurrentAz();
    double getCurrentEl();

    int getCurrentShutterState();
    int getCurrentParkStatus();
    void setDebugLog(bool enable);

    int enableDewHeater(bool enable);
    bool getDewHeaterStatus();
    
protected:

    int             readResponse(char *respBuffer, unsigned int bufferLen);
    int             getDomeAz(double &domeAz);
    int             getDomeEl(double &domeEl);
    int             getShutterState(int &state);
    int             getAtParkStatus(int &status);

    int             domeCommand(const char *cmd, char *result, int resultMaxLen);

    LoggerInterface *mLogger;
    SleeperInterface    *mSleeper;

    bool            bDebugLog;

    bool            bIsConnected;
    bool            mShutterOpened;

    double          mCurrentAzPosition;
    double          mCurrentElPosition;

    SerXInterface   *pSerx;

    int             mShutterState;
    int             mAtParkStatus;
    bool            mDewHeaterOn;
    
    char            mLogBuffer[ND_LOG_BUFFER_SIZE];
};

#endif
