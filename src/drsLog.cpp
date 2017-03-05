/********************************************************************\

Name:         drs4.cpp
Created by:   Sawaiz Syed

Contents:     Simple example application to read out a several
DRS4 evaluation board in daisy-chain mode

Outputs a file with a filename
2017-02-15_16h43m45s345_5000MSPS_-0050mV-0950mV_060000psDelay_Rising_AND_CH1-XXXXXX_CH2-50mV_CH3-XXXXXX_Ch4-50mV_EXT-X_50events.dat

\********************************************************************/

#include <math.h>

#define O_BINARY 0
#define MAX_N_BOARDS 4

#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DIR_SEPARATOR '/'

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>

#include "strlcpy.h"
#include "DRS.h"
#include <drsLog.h>

// Configuration
#define RANGECENTER 0.45       // -0.05V to 0.95V
#define SAMPLESPEED 5.0        // 5 GS/S
#define MAXEVENTS 50
#define MAXTIME   5

volatile int killSignalFlag = 0;

const double timeResolution = 1.0 / SAMPLESPEED;    // Time resolution in ns
const double sampleWindow = timeResolution * 1024;  // 1024 bins * resloution

// Trigger config
trigger_t trigger = {false,                     // fasle = Rising edge, true = falling edge
                     true,                      // false = OR         , true = AND
                     {0, 1, 1, 0, 0},           // CH1, CH2, CH3, CH4, EXT
                     {0.05, 0.05, 0.05, 0.05},  // Trigger threshold in Volts  
                     60.0};                     // Trigger delay from start of sample window

/*------------------------------------------------------------------*/

// Global
DRSBoard* b;

int main() {
  m_drs = NULL;
  m_board = 0;
  m_writeSR[0] = 0;
  m_triggerCell[0] = 0;

  int i, j;
  DRS* drs;

  // Exit gracefully if user terminates application
  signal(SIGINT, exitGracefully);

  /* do initial scan, sort boards accordning to their serial numbers */
  drs = new DRS();
  m_drs = drs;
  drs->SortBoards();

  /* show any found board(s) */
  for (i = 0; i < drs->GetNumberOfBoards(); i++) {
    b = drs->GetBoard(i);
    printf("Found DRS4 evaluation board, serial #%d, firmware revision %d\n",
           b->GetBoardSerialNumber(), b->GetFirmwareVersion());
    if (b->GetBoardType() < 8) {
      printf("Found pre-V4 board, aborting\n");
      return 0;
    }
  }

  /* exit if no board found */
  if (drs->GetNumberOfBoards() == 0) {
    printf("No DRS4 evaluation board found\n");
    return 0;
  }

  /* common configuration for all boards */
  for (i = 0; i < drs->GetNumberOfBoards(); i++) {
    b = drs->GetBoard(i);
    m_board = i;
    m_waveDepth = b->GetChannelDepth();  // 1024 hopefully
    /* initialize board */
    b->Init();

    /* select external reference clock for slave modules */
    /* NOTE: this only works if the clock chain is connected */
    if (i > 0) {
      if (b->GetFirmwareVersion() >=
          21260) {  // this only works with recent firmware versions
        if (b->GetScaler(5) > 300000)  // check if external clock is connected
          b->SetRefclk(true);          // switch to external reference clock
      }
    }

    // set sampling frequency
    b->SetFrequency(SAMPLESPEED, true);

    // set input range
    b->SetInputRange(RANGECENTER);

    // Set the triggers based on configuration
    setTrigger(b, trigger);

  }

  // Time
  struct timeval startTime;
  gettimeofday(&startTime, NULL);
  printf("Starting time: %s", ctime(&startTime.tv_sec));

  // Create the filename
  char filename[256];
  char concatBuffer[32];
  time_t printTime = startTime.tv_sec;
  struct tm *printfTime = localtime(&printTime);
  strftime(concatBuffer, sizeof concatBuffer, "%Y-%m-%d_%Hh%Mm%Ss", printfTime);
  snprintf(filename, sizeof filename, "%s%06ld", concatBuffer, startTime.tv_usec);
  snprintf(concatBuffer, sizeof concatBuffer, "_%dMSPS", (int) SAMPLESPEED * 1000);
  strcat(filename, concatBuffer);
  snprintf(concatBuffer, sizeof concatBuffer, "_%04dmV-%04dmV",(int) ((RANGECENTER-0.5)*1000), (int) ((RANGECENTER+0.5)*1000));
  strcat(filename, concatBuffer);
  snprintf(concatBuffer, sizeof concatBuffer, "_%06dpsDelay",  (int) trigger.triggerDelay*1000);
  strcat(filename, concatBuffer);
  if(trigger.triggerPolarity){
    strcat(filename, "_Fall");
  } else {
    strcat(filename, "_Rise");
  }
  if(trigger.triggerLogic){
    strcat(filename, "_AND");
  } else {   
    strcat(filename, "__OR");
  }
  for (unsigned int i = 0; i < sizeof(trigger.triggerSource) / sizeof(trigger.triggerSource[0]) ; i++) {
    if(i == sizeof(trigger.triggerSource) / sizeof(trigger.triggerSource[0])-1){
      strcat(filename, "_EXT-");
      if(trigger.triggerSource[i]){
        strcat(filename, "T");
      } else {
        strcat(filename, "F");
      }
    } else {
    strcat(filename, "_CH");
    if(trigger.triggerSource[i]){
      snprintf(concatBuffer, sizeof concatBuffer, "%d-BYPASS", i+1);      
    } else {
      snprintf(concatBuffer, sizeof concatBuffer, "%d-%04dmV", i+1, (int) (trigger.triggerLevel[i]*1000));
    }
    strcat(filename, concatBuffer);
    }
  }

  snprintf(concatBuffer, sizeof concatBuffer, "_%08d-Events", MAXEVENTS);
  strcat(filename, concatBuffer);
  snprintf(concatBuffer, sizeof concatBuffer, "_%08d-Seconds", MAXTIME);
  strcat(filename, concatBuffer);
  strcat(filename, ".dat");

  printf("%s\n", filename);
  
  m_fd = open("data.dat", O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644);

  /* repeat ten times */
  for (i = 0; i < MAXEVENTS; i++) {
    m_drs->GetBoard(m_board);
    m_drs->GetBoard(m_board)->GetBoardType();

    /* start boards (activate domino wave), master is last */
    for (j = drs->GetNumberOfBoards() - 1; j >= 0; j--) {
      drs->GetBoard(j)->StartDomino();
    }

    /* wait for trigger on master board */
    printf("Waiting for trigger...");
    while (drs->GetBoard(0)->IsBusy()) {
      struct timeval currentTime;
      gettimeofday(&currentTime, NULL);
      if (killSignalFlag | (currentTime.tv_sec - startTime.tv_sec >= MAXTIME)) {
        if (m_fd){
          close(m_fd);
        }
        delete drs;
        printf("Program finished after %d events and %ld seconds. \n", i , currentTime.tv_sec-startTime.tv_sec);
        return 0;
      }
    }

    for (j = 0; j < drs->GetNumberOfBoards(); j++) {
      m_board = j;
      drs->GetBoard(j);
      if (drs->GetBoard(0)->IsBusy()) {
        i--; /* skip that event, must be some fake trigger */
        break;
      }
      m_board = j;
      ReadWaveforms();
      SaveWaveforms(m_fd);
    }

    /* print some progress indication */
    printf("\rEvent #%d read successfully\n", i);
  }

  if (m_fd){
    close(m_fd);
  }
  m_fd = 0;
  printf("Program finished.\n");

  /* delete DRS object -> close USB connection */
  delete drs;
}

int setTrigger(DRSBoard* board, trigger_t trigger) {

  // Evaluation Board earlier than V4&5
  if (board->GetBoardType() < 8) {
    printf("Board too old");
    return -1;
  }

  int triggerSource = 0;
  board->EnableTrigger(1, 0);                     // Enable hardware trigger
  board->SetTranspMode(1);                        // Set transparent mode for OR logic
  
  // Create trigger sources bytes based on triggers logic type.
  for (unsigned int i = 0; i < sizeof(trigger.triggerSource) / sizeof(trigger.triggerSource[0]) ; i++) {
    if(!trigger.triggerLogic){
      triggerSource |= (trigger.triggerSource[i] << i);
    } else {
      triggerSource |= (trigger.triggerSource[i] << (i + 8));
    }
  }

  // Set Trigger Voltages
  for (unsigned int i = 0; i < sizeof(trigger.triggerLevel) / sizeof(trigger.triggerLevel[0]) ; i++) {
    board->SetIndividualTriggerLevel(i + 1, trigger.triggerLevel[i]);    
  }  

  board->SetTriggerSource(triggerSource);                             // Trigger Sources
  board->SetTriggerPolarity(trigger.triggerPolarity);                 // Set trigger edge style
  board->SetTriggerDelayNs(sampleWindow - trigger.triggerDelay);      // Set trigger delay

  return 0;
}

int SaveWaveforms(int fd) {
  // char str[80];
  unsigned char* p;
  unsigned short d;
  float t;
  int size;
  static unsigned char* buffer;
  static int buffer_size = 0;

  if (fd) {
    if (buffer_size == 0) {
      buffer_size = 4 + m_nBoards * (4 + 4 * (4 + m_waveDepth * 4));
      buffer_size += 24 + m_nBoards * (8 + 4 * (4 + m_waveDepth * 2));
      buffer = (unsigned char*)malloc(buffer_size);
    }

    p = buffer;

    if (m_evSerial == 1) {
      printf("Time Cal header\n");
      // time calibration header
      memcpy(p, "TIME", 4);
      p += 4;

      for (int b = 0; b < m_nBoards; b++) {
        // store board serial number
        sprintf((char*)p, "B#");
        p += 2;
        *(unsigned short*)p = m_drs->GetBoard(b)->GetBoardSerialNumber();
        p += sizeof(unsigned short);

        for (int i = 0; i < 4; i++) {
          // if (m_chnOn[b][i]) {
          sprintf((char*)p, "C%03d", i + 1);
          p += 4;
          float tcal[2048];
          m_drs->GetBoard(b)->GetTimeCalibration(0, i * 2, 0, tcal, 0);
          for (int j = 0; j < m_waveDepth; j++) {
            // save binary time as 32-bit float value
            if (m_waveDepth == 2048) {
              t = (tcal[j % 1024] + tcal[(j + 1) % 1024]) / 2;
              j++;
            } else
              t = tcal[j];
            *(float*)p = t;
            p += sizeof(float);
          }
          //   }
        }
      }
    }

    memcpy(p, "EHDR", 4);
    p += 4;
    *(int*)p = m_evSerial;
    p += sizeof(int);
    *(unsigned short*)p = m_evTimestamp.Year;
    p += sizeof(unsigned short);
    *(unsigned short*)p = m_evTimestamp.Month;
    p += sizeof(unsigned short);
    *(unsigned short*)p = m_evTimestamp.Day;
    p += sizeof(unsigned short);
    *(unsigned short*)p = m_evTimestamp.Hour;
    p += sizeof(unsigned short);
    *(unsigned short*)p = m_evTimestamp.Minute;
    p += sizeof(unsigned short);
    *(unsigned short*)p = m_evTimestamp.Second;
    p += sizeof(unsigned short);
    *(unsigned short*)p = m_evTimestamp.Milliseconds;
    p += sizeof(unsigned short);
    *(unsigned short*)p = (unsigned short)(m_inputRange * 1000);  // range
    p += sizeof(unsigned short);

    for (int b = 0; b < m_nBoards; b++) {
      // store board serial number
      sprintf((char*)p, "B#");
      p += 2;
      *(unsigned short*)p = m_drs->GetBoard(b)->GetBoardSerialNumber();
      p += sizeof(unsigned short);

      // store trigger cell
      sprintf((char*)p, "T#");
      p += 2;
      *(unsigned short*)p = m_triggerCell[b];
      p += sizeof(unsigned short);

      for (int i = 0; i < 4; i++) {
        // if (m_chnOn[b][i]) {
        sprintf((char*)p, "C%03d", i + 1);
        p += 4;
        for (int j = 0; j < m_waveDepth; j++) {
          // save binary date as 16-bit value:
          // 0 = -0.5V,  65535 = +0.5V    for range 0
          // 0 = -0.05V, 65535 = +0.95V   for range 0.45
          if (m_waveDepth == 2048) {
            // in cascaded mode, save 1024 values as averages of the 2048 values
            d = (unsigned short)(((m_waveform[b][i][j] +
                                   m_waveform[b][i][j + 1]) /
                                      2000.0 -
                                  m_inputRange + 0.5) *
                                 65535);
            *(unsigned short*)p = d;
            p += sizeof(unsigned short);
            j++;
          } else {
            d = (unsigned short)((m_waveform[b][i][j] / 1000.0 - m_inputRange +
                                  0.5) *
                                 65535);
            *(unsigned short*)p = d;
            p += sizeof(unsigned short);
          }
        }
        //}
      }
    }

    size = p - buffer;
    assert(size <= buffer_size);
    int n = write(fd, buffer, size);
    if (n != size)
      return -1;
  }

  m_evSerial++;

  return 1;
}

void ReadWaveforms() {
  // unsigned char *pdata;
  // unsigned short *p;
  // int size = 0;
  // m_armed = false;
  m_nBoards = 1;

  int ofs = m_chnOffset;
  // int chip = m_chip;

  if (m_drs->GetBoard(m_board)->GetBoardType() == 9) {
    // DRS4 Evaluation Boards 1.1 + 3.0 + 4.0
    // get waveforms directly from device
    m_drs->GetBoard(m_board)->TransferWaves(m_wavebuffer[0], 0, 8);
    m_triggerCell[0] = m_drs->GetBoard(m_board)->GetStopCell(chip);
    m_writeSR[0] = m_drs->GetBoard(m_board)->GetStopWSR(chip);
    GetTimeStamp(m_evTimestamp);

    for (int i = 0; i < m_nBoards; i++) {
      if (m_nBoards > 1)
        b = m_drs->GetBoard(i);
      else
        b = m_drs->GetBoard(m_board);

      // obtain time arrays
      m_waveDepth = b->GetChannelDepth();

      for (int w = 0; w < 4; w++)
        b->GetTime(0, w * 2, m_triggerCell[i], m_time[i][w], m_tcalon,
                   m_rotated);

      if (m_clkOn && GetWaveformDepth(0) > kNumberOfBins) {
        for (int j = 0; j < kNumberOfBins; j++)
          m_timeClk[i][j] = m_time[i][0][j] + GetWaveformLength() / 2;
      } else {
        for (int j = 0; j < kNumberOfBins; j++)
          m_timeClk[i][j] = m_time[i][0][j];
      }

      // decode and calibrate waveforms from buffer
      if (b->GetChannelCascading() == 2) {
        b->GetWave(m_wavebuffer[i], 0, 0, m_waveform[i][0], m_calibrated,
                   m_triggerCell[i], m_writeSR[i], !m_rotated, 0,
                   m_calibrated2);
        b->GetWave(m_wavebuffer[i], 0, 1, m_waveform[i][1], m_calibrated,
                   m_triggerCell[i], m_writeSR[i], !m_rotated, 0,
                   m_calibrated2);
        b->GetWave(m_wavebuffer[i], 0, 2, m_waveform[i][2], m_calibrated,
                   m_triggerCell[i], m_writeSR[i], !m_rotated, 0,
                   m_calibrated2);
        if (m_clkOn && b->GetBoardType() < 9)
          b->GetWave(m_wavebuffer[i], 0, 8, m_waveform[i][3], m_calibrated,
                     m_triggerCell[i], 0, !m_rotated);
        else
          b->GetWave(m_wavebuffer[i], 0, 3, m_waveform[i][3], m_calibrated,
                     m_triggerCell[i], m_writeSR[i], !m_rotated, 0,
                     m_calibrated2);
        // if (m_spikeRemoval)
        //  RemoveSpikes(i, true);
      } else {
        b->GetWave(m_wavebuffer[i], 0, 0 + ofs, m_waveform[i][0], m_calibrated,
                   m_triggerCell[i], 0, !m_rotated, 0, m_calibrated2);
        b->GetWave(m_wavebuffer[i], 0, 2 + ofs, m_waveform[i][1], m_calibrated,
                   m_triggerCell[i], 0, !m_rotated, 0, m_calibrated2);
        b->GetWave(m_wavebuffer[i], 0, 4 + ofs, m_waveform[i][2], m_calibrated,
                   m_triggerCell[i], 0, !m_rotated, 0, m_calibrated2);
        b->GetWave(m_wavebuffer[i], 0, 6 + ofs, m_waveform[i][3], m_calibrated,
                   m_triggerCell[i], 0, !m_rotated, 0, m_calibrated2);

        // if (m_spikeRemoval)
        //   RemoveSpikes(i, false);
      }

      // extrapolate the first two samples (are noisy)
      for (int j = 0; j < 4; j++) {
        m_waveform[i][j][1] = 2 * m_waveform[i][j][2] - m_waveform[i][j][3];
        m_waveform[i][j][0] = 2 * m_waveform[i][j][1] - m_waveform[i][j][2];
      }
    }
  }
}

void GetTimeStamp(TIMESTAMP& ts) {
  struct timeval t;
  struct tm* lt;
  time_t now;
  // static unsigned int ofs = 0;

  gettimeofday(&t, NULL);
  time(&now);
  lt = localtime(&now);

  ts.Year = lt->tm_year + 1900;
  ts.Month = lt->tm_mon + 1;
  ts.Day = lt->tm_mday;
  ts.Hour = lt->tm_hour;
  ts.Minute = lt->tm_min;
  ts.Second = lt->tm_sec;
  ts.Milliseconds = t.tv_usec / 1000;
}

int GetWaveformDepth(int channel) {
  if (channel == 3 && m_clkOn && m_waveDepth > kNumberOfBins)
    return m_waveDepth - kNumberOfBins;  // clock chnnael has only 1024 bins

  return m_waveDepth;
}

double GetSamplingSpeed() {
  //  if (m_drs->GetNumberOfBoards() > 0) {
  //     DRSBoard *b = m_drs->GetBoard(m_board);
  //     return b->GetNominalFrequency();
  //  }
  return m_samplingSpeed;
}

void exitGracefully(int sig) {
  killSignalFlag = 1;
}
