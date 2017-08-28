/** -*-mode:c; c-basic-offset: 2-*-
 *
 */

#ifndef _PIPOPROC_H_
#define _PIPOPROC_H_

#include "MaxPiPoHost.h"
#include "mimo.h"

class MaxPiPoProc;

/** interface defining access to a max external running the pipo/mimo process
 */
class MaxPiPoProcControlInterface
{
public:
  virtual t_object *getExt () = 0;
  virtual MaxPiPoHost *getMaxPiPoHost () = 0;
  
  virtual int getContinuousProcessing() = 0; // max attr
  virtual int getPriority() = 0; // max attr
  virtual int getMaxFrames() = 0; // max attr
  virtual int getPrePad() = 0; // max attr
  
  virtual void incCallContainerListeners() = 0;	// todo: move counter here
  virtual int getUpdateOutputTrackAttributes() = 0;
  virtual MuBuTrackDescriptionT *getUserTrackDescr() = 0;
  virtual bool getOwnsOutputTrack() = 0;
  virtual void setOwnsOutputTrack(bool flag) = 0;
  virtual void setOutputClock (bool unset = false) = 0;
  virtual t_clock *getOutputRescheduledInputClock() = 0;
  virtual void *getInfoOutlet() = 0;
  virtual bool isAllDone() = 0;

public:
  // callbacks
  void (*outputMuBuTrackData)(MaxPiPoProcControlInterface *self, int streamIndex, MuBuTrackT *track, int startIndex, int numFrames, int loopback);
  void (*outputBufferTildaData)(MaxPiPoProcControlInterface *self, int streamIndex, t_buffer_obj *bufferTilda, int startIndex, int numFrames, int loopback);
};



/***********************************************************************
 *
 * receiver of MaxPiPoProc's pipochain: store result attrs and data
 *
 */

class PiPoProcReceiver : public PiPo
{
public:
  PiPoProcReceiver(PiPo::Parent *parent);
  MaxPiPoProc *getProc();

  // called as receiver of pipochain: transmit output stream attributes
  int streamAttributes(bool hasTimeTags, double rate, double offset, unsigned int width, unsigned int size, const char **labels, bool hasVarSize, double domain, unsigned int maxFrames);
  int reset(void);
  int frames(double time, double weight, PiPoValue *values, unsigned int num, unsigned int numFrames);
  int finalize(double inputEnd);
};


/***************************************************************************************
 *
 *  pipo process
 *
 */

#pragma mark -
#pragma mark proc

#define NO_PENDING_TRACK ((MuBuTrackT *)((long)-1))

enum ProcPriorityE { PriorityLowest, PriorityLow, PriorityDefault, PriorityHigh, PriorityHighest, PrioritySync };
enum ProgressOutputModeE { ProgressOutputOff, ProgressOutputTime, ProgressOutputIndex, ProgressOutputFill, ProgressOutputInput };

class MaxPiPoProc : public PiPo::Parent
{
public:
  class InputTrack;
  class MuBuTrackSlot;
  class BufferTildaSlot;

  /// threadStatus serves to communicate between parent MaxPiPoProc and launched thread
  enum ProcStatusE {
    ProcStart = 0,	///< started by parent (will set itself to running)
    ProcRunning = 1,	///< thread sets itself to running after started from parent
    ProcStop = 2,	///< stopped by parent (will set itself to halted)
    ProcFinalized = 3,
    ProcHalted = 4,	///< thread sets itself to halted after stop from parent
    ProcTerminate = 5	///< terminated by parent (will quit)
  };
  
  MaxPiPoProcControlInterface *processExt; /**< pointer back to parent process external */
  unsigned int streamId;
  
  MaxMuBuContainerT *container;
  int inputTrackIndex;
  int outputTrackIndex;
  
  PiPoChain pipoChain;
  PiPo *pipo;
  PiPo *receiver;
  
  InputTrack *inputTrack;
  MuBuTrackSlot *outputTrack;
  MuBuTrackDescriptionT outputTrackDescr;
  
  int inputIndex ;
  int outputIndex;
  int maxFrames;
  std::vector<float> zeroVec;
  
  bool threadLaunched;		///< true if thread has been physically started
  t_atom_long threadStatus;	///< takes values of enum ProcStatusE
  t_systhread_mutex statusMutex;
  t_systhread_mutex mutex;
  t_systhread_cond cond;
  t_systhread thread;
  
  void *qelem_done;
  void *qelem_allDone;
  
  double progress;

  // constructor
  MaxPiPoProc(MaxPiPoProcControlInterface *processExt, int streamId);
  
  // destructor
  virtual ~MaxPiPoProc(void);

  virtual bool setupChain (PiPo *pipo, const PiPoStreamAttributes &attr);
  void set (MaxMuBuContainerT *container, int inputTrackIndex) throw();
  void setPriority(int priority);
  void stopAndWaitForHalted();
  void startOrRestart();
  void terminateAndWaitForExited();
  void launchThread(int start);
  void continueProcessing();
  static void threadFun(MaxPiPoProc *proc);
  void killThread(void);
  void resetStream(InputTrack *inputTrack);
  int inputStream(InputTrack *track);
  void finalizeStream(double inputEnd);
  double getProgress(int outputMode);
  
  virtual void processSynchronously(int reset);
  virtual void processAsynchronously(void);
  // stream frames through pipo/mimo, augmenting inputIndex
  virtual int feed (double *timetags, PiPoValue *data, int width, int numFrames); 
  virtual int feed (double starttime, PiPoValue *data, int width, int numFrames);

  int streamAttributes(bool hasTimeTags, double rate, double offset, unsigned int width, unsigned int size, const char **labels, bool hasVarSize, double domain, unsigned int maxFrames);
  void streamAttributesChanged(PiPo *pipo, PiPo::Attr *attr);
  void signalError(PiPo *pipo, std::string *errorMsg);
  void signalWarning (PiPo *pipo, std::string *errorMsg);
  int reset(void);
  int frames(double time, double weight, float *values, unsigned int num, unsigned int numFrames);
  int finalize(double inputEnd);
};

class MaxPiPoProc::MuBuTrackSlot
{
public:
  MuBuTrackT *current;
  MuBuTrackT *pending;
  MuBuTrackT *trash;
  void *listener;
  void (*callback)(void *, MuBuTrackT *, int, int, int);
    
  MuBuTrackSlot(void);
  ~MuBuTrackSlot(void);
  void listenMuBuTrack(void *listener, void (*callback)(void *, MuBuTrackT *, int, int, int));
  int setMuBuTrack(MuBuTrackT *track);
  void reset(void);
  MuBuTrackT *getMuBuTrack(void);
  MuBuTrackT *getBackMuBuTrack(void);
};

// @TODO: buffertildarize
class MaxPiPoProc::BufferTildaSlot
{
  t_buffer_obj *audioBuffer;
  AudioBufferReferenceT *audioBufferRef;
  int streamId;

public:
  BufferTildaSlot(AudioBufferReferenceT *audioBufferRef);
  ~BufferTildaSlot();
  bool setBufferTilda(int streamId);
  t_buffer_obj *getBufferTilda();
};

class MaxPiPoProc::InputTrack
{
  MuBuTrackSlot *mubuTrackSlot;
  BufferTildaSlot *bufferTildaSlot;
    
public:
  InputTrack();
  ~InputTrack();
  bool isMubuTrack();
  bool isValid();
  bool set(MaxPiPoProc *proc, MaxMuBuContainerT *container, int streamId, int inputTrackIndex);
  bool hasTimeTags();
  double getSampleRate();
  double getSamplePeriod();
  double getSampleOffset();
  int getNumCols();
  int getNumRows();
  bool hasVarRows();
  int getMaxSize();
  int getSize();
  double getEnd();
  void reset();
  bool isCurrent(MuBuTrackT *mubuTrack);
  MuBuTrackT *getMuBuTrack();
  t_buffer_obj *getBufferTilda();
  int processInputStream(MaxPiPoProc *proc);
  int outputInputStream(MaxPiPoProc *proc);

  /* this callback is called by ..... */
  static void inputMuBuTrackCallback(void *listener, MuBuTrackT *track, int bufferIndex, int trackIndex, int flags);
  static void inputBufferTildaCallback(void *listener);
}; // end class InputTrack

/* callbacks for MaxPiPoProc */
void outputDone(MaxPiPoProc *self);
void outputAllDone(MaxPiPoProc *self);

#endif // _PIPOPROC_H_
