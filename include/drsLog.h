#pragma once

typedef struct {
   unsigned short Year;
   unsigned short Month;
   unsigned short Day;
   unsigned short Hour;
   unsigned short Minute;
   unsigned short Second;
   unsigned short Milliseconds;
} TIMESTAMP;

typedef struct trigger_t {
  bool triggerPolarity;      // fasle = Rising, true = falling
  bool triggerLogic;         // false = OR, true = AND
  bool triggerSource[5];     // CH0, CH1, CH2, CH3, EXT
  double triggerLevel[4];    // Trigger threshold in Volts
  double triggerDelay;       // Trigger delay from start of sample window
} trigger_t;

DRS      *m_drs;
int m_evSerial = 1;
int m_nBoards = 4;
int m_waveDepth; //1024 hopefully
TIMESTAMP m_evTimestamp;
int m_inputRange = 0;
int chip = 0;
int m_board;
unsigned char m_wavebuffer[MAX_N_BOARDS][9*1024*2];
int m_writeSR[MAX_N_BOARDS];
bool m_calibrated = true;
bool m_calibrated2 = true;
bool m_tcalon = true;
bool m_rotated = true;
bool m_spikeRemoval = false;
float m_time[MAX_N_BOARDS][4][2048];
float m_timeClk[MAX_N_BOARDS][1024];
int m_triggerCell[MAX_N_BOARDS];
int m_fd = 0;
char filename[1024];
float m_waveform[MAX_N_BOARDS][4][2048];
bool m_clkOn = false;
double m_samplingSpeed = 1;
int  m_chnOffset = 0;

int SaveWaveforms(int fd);
void GetTimeStamp(TIMESTAMP &ts);
void ReadWaveforms();
int GetWaveformDepth(int channel);
double GetSamplingSpeed();
DRSBoard *GetBoard(int i){ return m_drs->GetBoard(i); }
double GetWaveformLength()    { return m_waveDepth / GetSamplingSpeed(); }
int setTrigger(DRSBoard* board, trigger_t trigger);
void exitGracefully(int sig);
