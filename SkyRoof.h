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

// C++ includes
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>

#include "StopWatch.h"

// #define PLUGIN_DEBUG 3
#define PLUGIN_VERSION  1.2

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 500
#define MAX_READ_WAIT_TIMEOUT 25
#define NB_RX_WAIT 10

#define CMD_DELAY   1000
// error codes
enum SkyRoofErrors {PLUGIN_OK=0, NOT_CONNECTED, CANT_CONNECT, BAD_CMD_RESPONSE, COMMAND_FAILED, COMMAND_TIMEOUT, ERR_RAINING, ERR_BATTERY_LOW};

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

    int enableDewHeater(bool enable);
    bool getDewHeaterStatus();
    
protected:

    int             domeCommand(const std::string sCmd, std::string &sResp, int nTimeout = MAX_TIMEOUT);
    int             readResponse(std::string &sResp, int nTimeout = MAX_TIMEOUT);

    int             getDomeAz(double &dDomeAz);
    int             getDomeEl(double &dDomeEl);
    int             getShutterState(int &nState);
    int             getAtParkStatus(int &nStatus);

    std::string&    trim(std::string &str, const std::string &filter );
    std::string&    ltrim(std::string &str, const std::string &filter);
    std::string&    rtrim(std::string &str, const std::string &filter);
    std::string     findField(std::vector<std::string> &svFields, const std::string& token);

    
    bool            m_bIsConnected;
    bool            m_bShutterOpened;

    double          m_dCurrentAzPosition;
    double          m_dCurrentElPosition;

    SerXInterface   *m_pSerx;

    int             m_nShutterState;
    int             m_nAtParkStatus;
    bool            m_bDewHeaterOn;
    
    CStopWatch      m_CmdTimer;
#ifdef PLUGIN_DEBUG
    // timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sLogfilePath;
#endif

};

#endif
