/********************************************************************\

Name:         drs4.cpp
Created by:   Sawaiz Syed

Contents:     Simple example application to read out a several
DRS4 evaluation board in daisy-chain mode

Outputs a file with a filename
2017-02-15_16h43m45s345_5000MSPS_-0050mV-0950mV_060000psDelay_Rising_AND_CH1-XXXXXX_CH2-0050mV_CH3-XXXXXX_Ch4-0050mV_EXT-X_00000050-Events_00003600-Seconds.dat

\********************************************************************/

#include <math.h>

#define O_BINARY 0
#define MAX_N_BOARDS 4

#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>

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

/*------------------------------------------------------------------*/

// Global
DRSBoard* b;
volatile int killSignalFlag = 0;

int main(int argc, char** argv) {

  m_drs = NULL;
  m_board = 0;
  m_writeSR[0] = 0;
  m_triggerCell[0] = 0;

  int i, j;
  DRS* drs;

  // Should consider changing to opt
   if (argc != 14){
      printf("Usage: %s" , argv[0]);
      printf("\n      <sample speed (0.1-6)            (5.0)GSPS>");
      printf("\n      <range center                    (0.450)V>");
      printf("\n      <trigger delay                   (60.0)ns>");      
      printf("\n      <trigger type [R|F]              (R)ising edge>");
      printf("\n      <trigger logic [AND|OR]          (AND) logic>");
      printf("\n      <trigger [CH1,CH2,CH3,CH4,EXT]   (01010) CH2,CH4>");
      printf("\n      <trigger voltage CH1             (0.050)V>");
      printf("\n      <trigger voltage CH2             (0.060)V>");
      printf("\n      <trigger voltage CH3             (0.800)V>");
      printf("\n      <trigger voltage CH4             (0.020)V>");
      printf("\n      <max events                      (10000) events>");
      printf("\n      <max time                        (3600) seconds>");
      printf("\n      <path                            ../data>");
      printf("\n");
      printf("\n      %s 5.0 0.45 60.0 R AND 01010 0.1 0.1 0.1 100 50 5 .",argv[0]);
      printf("\n");
      return 1;
    }

    trigger_t trigger;

    // Sample Speed
    double sampleSpeed; // 0.1 to 6 GS/s
    if((strtof(argv[1],NULL) < 0.1) | (strtof(argv[1],NULL) > 6)){
      printf("Sample Speed, %f out of range (0.1 GSPS -6 GSPS).\n", strtof(argv[1],NULL));
      return 1;      
    } else {
      sampleSpeed = strtof(argv[01],NULL);
    }

    // Range Center
    double rangeCenter; // 0 to 0.5 V
    if((strtof(argv[2],NULL) < 0) | (strtof(argv[2],NULL) > 0.5)){
      printf("Range Center, %f out of range (0 V - 0.5 V).\n", strtof(argv[2],NULL));
      return 1;      
    } else {
      rangeCenter = strtof(argv[2],NULL);
    }

    // Trigger Delay
    if((strtof(argv[3],NULL) < 0) | ((strtof(argv[3],NULL)) > ((1/sampleSpeed)*1024))){
      printf("Trigger delay, %f, out of range (0 ns - %f ns).\n", strtof(argv[3],NULL), ((1/sampleSpeed)*1024));
      return 1;
    } else {
      trigger.triggerDelay = strtof(argv[3],NULL);
    }

    // Trigger Edge polarity: fasle = Rising edge, true = falling edge
    if(argv[4][0] == 'R'){
      trigger.triggerPolarity = false;
    } else if (argv[4][0] == 'F') {
      trigger.triggerPolarity = true;
    } else {
      printf("Trigger edge type, %s not vaild, enter 'R' for rising or 'F' for falling.\n", argv[4]);
      return 1;
    }

    // Trigger Logic: false = OR, true = AND
    if(!strcmp(argv[5],"AND")){
      trigger.triggerLogic = true;
    } else if (!strcmp(argv[5],"OR")){
      trigger.triggerLogic = false;
    } else {
      printf("Logic type, %s is not valid. Enter either 'AND' or 'OR'.\n", argv[5]);
    }

    // Trigger
    if(strlen(argv[6]) == 5){
      for(unsigned int i = 0 ; i < sizeof(argv[6])/sizeof(argv[6][0]) ; i ++){
        if(argv[6][i] == '0'){
          trigger.triggerSource[i] = 0;
        } else if (argv[6][i] == '1'){
          trigger.triggerSource[i] = 1;
        } else {
          printf("Trigger argument for CH%d, '%c' is not valid it must be either '0' (diabled) or '1'(enabled).\n", i+1, argv[6][i]);
          return 1;
        }
      }
    } else {
      printf("Trigger number of arguments, %d do not match, you must have 5 (CH1,CH2,CH3,Ch4,EXT).\n", strlen(argv[6]));
      return 1;
    }

    // Trigger Levels
    for(int i = 0 ; i < 4 ; i ++){
      double level = strtod(argv[7+i],NULL);
      double minRange = rangeCenter - 0.5;
      double maxRange = rangeCenter + 0.5;
      if((level < minRange) | (level > maxRange)){
        printf("Trigger level for CH%d, %f out of range %f V - %f V.\n", i+1, level, minRange, maxRange);
        return 1;
      } else {
        trigger.triggerLevel[i] = level;
      }
    }
    
    // Max Events
    long maxEvents; // 1-max_long Events 
    if(strtol(argv[11],NULL,10) < 1){
      printf("Events, %ld out of range (1 Event - 2^64 Events).\n", strtol(argv[11],NULL,10));
      return 1;
    } else {
      maxEvents = strtol(argv[11],NULL,10);
    }

    long maxTime; // 1-max_long Seconds 
    if(strtol(argv[12],NULL,10) < 1){
      printf("Time, %ld out of range (1s-2^64s).\n", strtol(argv[12],NULL,10));
      return 1;
    } else {
      maxTime = strtol(argv[12],NULL,10);
    }

    // filepath
    char filepath[64];
    if(access(argv[13], W_OK) == 0){
      strcpy(filepath, argv[13]);
      strcat(filepath, "/");
    } else {
      printf("Do not have write premissions for path '%s'.\n", argv[13]);
      return 1;
    }

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
    b->SetFrequency(sampleSpeed, true);

    // set input range
    b->SetInputRange(rangeCenter);

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
  strcpy(filename, filepath);
  strftime(concatBuffer, sizeof concatBuffer, "%Y-%m-%d_%Hh%Mm%Ss", printfTime);
  strcat(filename, concatBuffer);  
  snprintf(concatBuffer, sizeof concatBuffer, "%06ld", startTime.tv_usec);
  strcat(filename, concatBuffer);  
  snprintf(concatBuffer, sizeof concatBuffer, "_%dMSPS", (int) sampleSpeed * 1000);
  strcat(filename, concatBuffer);
  snprintf(concatBuffer, sizeof concatBuffer, "_%04dmV-%04dmV",(int) ((rangeCenter-0.5)*1000), (int) ((rangeCenter+0.5)*1000));
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
      snprintf(concatBuffer, sizeof concatBuffer, "%d-%04dmV", i+1, (int) (trigger.triggerLevel[i]*1000));
    } else {
      snprintf(concatBuffer, sizeof concatBuffer, "%d-BYPASS", i+1);      
    }
    strcat(filename, concatBuffer);
    }
  }

  snprintf(concatBuffer, sizeof concatBuffer, "_%08ld-Events", maxEvents);
  strcat(filename, concatBuffer);
  snprintf(concatBuffer, sizeof concatBuffer, "_%08ld-Seconds", maxTime);
  strcat(filename, concatBuffer);
  strcat(filename, ".dat");

  printf("Logging data in: %s\n", filename);
  fflush(stdout);
  
  m_fd = open(filename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644);

  // Repeat untiul maxEvents or maxTime
  for (i = 0; i < maxEvents; i++) {
    m_drs->GetBoard(m_board);
    m_drs->GetBoard(m_board)->GetBoardType();

    /* start boards (activate domino wave), master is last */
    for (j = drs->GetNumberOfBoards() - 1; j >= 0; j--) {
      drs->GetBoard(j)->StartDomino();
    }

    /* wait for trigger on master board */
    while (drs->GetBoard(0)->IsBusy()) {
      struct timeval currentTime;
      gettimeofday(&currentTime, NULL);
      if (killSignalFlag | (currentTime.tv_sec - startTime.tv_sec >= maxTime)) {
        if (m_fd){
          close(m_fd);
        }
        delete drs;
        printf("Program finished after %d events and %ld seconds. \n", i , currentTime.tv_sec-startTime.tv_sec);
        fflush(stdout);
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
    fflush(stdout);

  }

  if (m_fd){
    close(m_fd);
  }
  m_fd = 0;
  struct timeval currentTime;
  gettimeofday(&currentTime, NULL);
  printf("Program finished after %d events and %ld seconds. \n", i , currentTime.tv_sec-startTime.tv_sec);
  fflush(stdout);


  /* delete DRS object -> close USB connection */
  delete drs;
  return 0;
  
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

  const double timeResolution = 1.0 / board->GetNominalFrequency();   // Time resolution in ns
  const double sampleWindow = timeResolution * 1024;                  // 1024 bins * resloution

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
