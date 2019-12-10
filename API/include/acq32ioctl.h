/*****************************************************************************
 *
 * File: acq32ioctl.h - Application interface to acq32 Linux driver
 *
 * $RCSfile: acq32ioctl.h,v $
 * 
 * Copyright (C) 1999 D-TACQ Solutions Ltd
 * not to be used without owner's permission
 *
 * Description:
 *
 * $Id: acq32ioctl.h,v 1.38 2002/11/06 14:39:20 pgm Exp $
 * $Log: acq32ioctl.h,v $
 * Revision 1.38  2002/11/06 14:39:20  pgm
 * *** empty log message ***
 *
 * Revision 1.37  2002/11/06 14:39:01  pgm
 * *** empty log message ***
 *
 * Revision 1.36  2002/08/09 14:13:20  pgm
 * *** empty log message ***
 *
 * Revision 1.35  2002/06/16 16:31:39  pgm
 * *** empty log message ***
 *
 * Revision 1.34  2002/07/29 08:07:33  pgm
 * *** empty log message ***
 *
 * Revision 1.33  2002/07/24 07:20:59  pgm
 * MAXCHUNKSIZE
 *
 * Revision 1.32  2002/07/19 18:40:04  pgm
 * add irq
 *
 * Revision 1.31  2002/05/31 08:32:50  pgm
 * rh7.3 compile works
 *
 * Revision 1.30  2002/02/27 03:09:48  pgm
 * add help part 1
 *
 * Revision 1.29  2002/02/27 02:09:06  pgm
 * getSyncRoute
 *
 * Revision 1.28  2002/02/14 16:26:23  pgm
 * DIx arg for setModeTriggeredContinuous
 *
 * Revision 1.27  2002/02/10 18:16:05  pgm
 * FAST_MULTI opt
 *
 * Revision 1.26  2002/02/02 14:44:42  pgm
 * waitEvent (not complete), multimaster updates
 *
 * Revision 1.25  2001/10/21 09:08:40  pgm
 * selectThresholdDetector etc
 *
 * Revision 1.24  2001/10/20 20:44:42  pgm
 * *** empty log message ***
 *
 * Revision 1.23  2001/10/14 09:07:15  pgm
 * *** empty log message ***
 *
 * Revision 1.22  2001/10/13 15:30:10  pgm
 * ACQ32_IOG_READ/WRITE funcs for ioctl target mem access
 *
 * Revision 1.21  2001/09/07 09:54:54  pgm
 * new keywords
 *
 * Revision 1.20  2001/08/19 20:24:24  pgm
 * threshold commands
 *
 * Revision 1.19  2001/08/18 20:06:41  pgm
 * resetGUT
 *
 * Revision 1.18  2001/06/26 18:51:19  pgm
 * getConfig etc
 *
 * Revision 1.17  2001/06/23 11:22:40  pgm
 *  for release
 *
 * Revision 1.16  2001/06/04 19:28:38  pgm
 * *** empty log message ***
 *
 * Revision 1.15  2001/05/25 17:17:59  pgm
 * time for release now
 *
 * Revision 1.14  2001/05/03 18:52:11  pgm
 * docs DUMDMA
 *
 * Revision 1.13  2001/04/16 15:37:48  pgm
 * ready for release
 *
 * Revision 1.12  2001/03/30 20:17:01  pgm
 * UDMA first cut working
 *
 * Revision 1.11  2001/03/25 19:19:04  pgm
 * first cut i2o command processing
 *
 * Revision 1.10  2000/10/28 21:19:15  pgm
 * getSetNumSamples
 *
 * Revision 1.9  2000/10/08 19:57:01  pgm
 * TriggeredContinuous etc
 *
 * Revision 1.8  2000/10/01 21:53:51  pgm
 * bigdump, sping, debug etc
 *
 * Revision 1.7  2000/09/26 19:46:52  pgm
 * GATED_CONTINUOUS pre/post, lseek, spin
 *
 * Revision 1.6  2000/07/01 07:25:39  pgm
 * SOFT_CONTINUOUS
 *
 * Revision 1.5  2000/06/11 19:29:29  pgm
 * getChannelMask
 *
 * Revision 1.4  2000/06/04 18:41:00  pgm
 * Signal Conditioning protocol implemented
 *
 * Revision 1.3  2000/05/20 08:27:03  pgm
 * ICD REV6 function codes
 *
 * Revision 1.2  1999/11/20 21:20:30  pgm
 * 991120 time for bed
 *
 * Revision 1.1.1.1  1999/10/25 13:14:08  pgm
 * linux include
 *
 * Revision 1.7  1999/10/20 19:57:16  pgm
 * mixed in ebsa driver funcs + new rom offset hakker
 *
 * Revision 1.6  1999/10/14 22:51:50  pgm
 * simul engine runs - SHIP IT QUICKcvs status | grep Status
 *
 * Revision 1.5  1999/10/14 21:10:28  pgm
 * master command handling works
 *
 * Revision 1.4  1999/10/12 21:24:20  pgm
 * master command handling top down connection OK
 *
 * Revision 1.3  1999/10/12 10:23:47  pgm
 * basic master command/readback struct WORKS
 *
 * Revision 1.2  1999/10/02 21:27:14  pgm
 * first pass compile OK. /proc/acq32 stuffed
 *
 * Revision 1.1  1999/10/02 19:33:46  pgm
 * lets go
 *
 *
\*****************************************************************************/

#ifndef _ACQ32IOCTL_H_
#define _ACQ32IOCTL_H_

#include <asm/ioctl.h>
#include <asm/types.h>

/*
 * whence arg for seek
 */

#define SEEK_TRIG (SEEK_END+1)

/*
 * Ioctl definitions
 */

#define ACQ32_IOC_MAGIC  'a'

#define ACQ32_IOCRESET    _IO(ACQ32_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */

#define ACQ32_IOCGMEM      _IOR(ACQ32_IOC_MAGIC,   1, sizeof(void*))
#define ACQ32_IOSLIMIT     _IO (ACQ32_IOC_MAGIC,  2 ) // set readout limit

// arg for LIMIT is limit count of samples per read

#define ACQ32_IOSFORMAT    _IO (ACQ32_IOC_MAGIC,  3 ) // set output format

#define ACQ32_IOARMSPIN    _IO(ACQ32_IOC_MAGIC,  4 ) // make ARM spin 
#define ACQ32_IOARMRESET   _IO(ACQ32_IOC_MAGIC,  5 ) // reset ARM

#define ACQ32_IOSDEBUG     _IOW(ACQ32_IOC_MAGIC, 6, 4) // set debug level
// arg for format is

enum ChannelMode {
    CM_BINARY,
    CM_HEX,
    CM_DEC,
    CM_VOLTS,
    CM_BIGEND,

    CM_EXCLUSIVE_OPTS = 0xf,

    CM_LINENUMS = 0x40,
    CM_DOS_LINES = 0x80
};

/*
 * access the mailboxes 0 <= ix <= 3
 */

#define ACQ32_IO_MBX_NR 10

#define ACQ32_IOSMBX( ix ) \
    _IO (ACQ32_IOC_MAGIC,  ACQ32_IO_MBX_NR+(ix) )

#define ACQ32_IOGMBX( ix ) \
    _IOR(ACQ32_IOC_MAGIC, ACQ32_IO_MBX_NR +(ix), sizeof(unsigned*))

#define ACQ32_IOSROM_WORD_A01     _IO( ACQ32_IOC_MAGIC, 14 )
#define ACQ32_IOCHARDRESET        _IO (ACQ32_IOC_MAGIC, 15 ) /* debugging tool */
#define ACQ32_IOSENDI2O           _IO (ACQ32_IOC_MAGIC, 16 )
#define ACQ32_IOREAD_LOCALBUF     _IO (ACQ32_IOC_MAGIC, 17 )
#define ACQ32_IOREAD_GETPHYSICAL  _IO (ACQ32_IOC_MAGIC, 18 )
#define ACQ32_IOG_PCIREGS_OFFSET  _IOR(ACQ32_IOC_MAGIC, 19, sizeof(unsigned*) )

/*
 * acq32 memory read/write
 */
 
struct ACQ32_RW_DEF {
    unsigned offset;
    union {
        __u16 w;
	    __u32 l;
    }
        data;
};

#define ACQ32_IOG_READ32          _IO (ACQ32_IOC_MAGIC, 20 )
#define ACQ32_IOS_WRITE32         _IO (ACQ32_IOC_MAGIC, 21 )
#define ACQ32_IOG_READ16          _IO (ACQ32_IOC_MAGIC, 22 )
#define ACQ32_IOS_WRITE16         _IO (ACQ32_IOC_MAGIC, 23 )

/*
 * waiting for events
 */
struct ACQ32_WAIT_DEF {
    unsigned event_mask;        // currently only 1 event is valid: OnStop == 1
    unsigned timeout;           // timeout in ticks
};

#define ACQ32_IOS_WAIT_EVENT      _IO (ACQ32_IOC_MAGIC, 24 )   

/*
 * interrupt masking (for realtime performance)
 * parameter is 32 bit mask for irq 0..31
 */

#define ACQ32_IOS_INTS_ENABLE _IOW(ACQ32_IOC_MAGIC, 25, 4 )
#define ACQ32_IOS_INTS_DISABLE _IOW(ACQ32_IOC_MAGIC, 26, 4 )

/*
 * how much kmem is there ? - use this ioctl with arg == order mask
 * eg 0x10 = O5, 0x18 = O5+O4
 * allocates until out of memory, reports in klog,
 * blocks, and frees up on signal
 */

#define ACQ32_IOS_SWALLOW_KMEM _IOW(ACQ32_IOC_MAGIC, 27, 4 )
#define ACQ32_IOS_VALIDATE_BIGBUF _IOW(ACQ32_IOC_MAGIC, 28, 4 )

#define ACQ32_IOC_MAXNR 29


/*
 * ioctls are less important that command strings:
 */

/* 
 * channel commands - apply to row dev (non-volatile)
 *                    or channel dev (volatile)
 */

#define CC_FORMAT          "format"
#define CC_FORMAT_BIN      "bin"
#define CC_FORMAT_BIGEND   "bigendian"
#define CC_FORMAT_HEX      "hex"
#define CC_FORMAT_DEC      "dec"
#define CC_FORMAT_VOLTS    "volts"
#define CC_FORMAT_LINENUMS "lines"
#define CC_FORMAT_DOS      "dos"

#define CC_LIMIT           "limit"   // limit n - limit output to n samples

#define CC_SEEK            "seek"    // seek {whence} n
#define CC_SEEK_BEGIN      "start"
#define CC_SEEK_END        "end"
#define CC_SEEK_CURRENT    "current"
#define CC_SEEK_TRIG       "trig"

#define CC_STREAM          "stream" // stream [stride [, mean]]


/*
 * master commands - apply to master dev only
 */
#define MC_GET_HELP         "help"
#define MC_GET_NUMCHANNELS  "getNumChannels"
#define MC_GET_NUMSAMPLES   "getNumSamples"
#define MC_GET_SETSAMPLES   "getSetNumSamples"
#define MC_GET_STATE        "getState"
    
#define MC_SET_CHANNEL_MASK "setChannelMask" // <mask> {0|1}*N
#define MC_GET_CHANNEL_MASK "getChannelMask"

#define MC_GET_MEMORY_DEPTH "getMemoryDepth" // <channel>
#define MC_SET_MODE         "setMode"        // <mode> <samples> [RUN_BLOCK]
#define MC_GET_MODE         "getMode"        // <mode> <samples>
#define MC_SET_ARM          "setArm"
#define MC_SET_ABORT        "setAbort"

#define MC_SET_DATA_ROUTE   "setDataRoute"    // raw, row, channel
  
#define MC_SET_DISTRIBUTOR  "setDistributor" // 0..n


#define MC_MODE_GATED_TRANSIENT  "GATED_TRANSIENT"
#define MC_MODE_GATED_CONTINUOUS "GATED_CONTINUOUS"
#define MC_MODE_SOFT_TRANSIENT   "SOFT_TRANSIENT"
#define MC_MODE_SOFT_CONTINUOUS  "SOFT_CONTINUOUS"
#define MC_MODE_TRIGGERED_CONTINUOUS "TRIGGERED_CONTINUOUS"

#define MC_SET_MODE_GC      "setModeGatedContinuous"     // <pre> <post>
#define MC_SET_MODE_TRC     "setModeTriggeredContinuous" // <pre> <post> [DIx]

#define DIX_DEFAULT 4  // for reference - this is the default DIx trigger line


#define MC_STATE_STOP            "ST_STOP"
#define MC_STATE_ARM             "ST_ARM"
#define MC_STATE_RUN             "ST_RUN"
#define MC_STATE_TRIGGER         "ST_TRIGGER"
#define MC_STATE_POSTPROCESS     "ST_POSTPROCESS"

#define MC_MODE_RUN_BLOCK        "RUN_BLOCK" 

#define MC_SET_ARMED        "setArmed"
#define MC_SET_ABORT        "setAbort"

#define MC_GET_NUMSAMPLES   "getNumSamples"

#define MC_SET_INTERNAL_CLOCK "setInternalClock"        // <hz> [DOx]
#define MC_GET_INTERNAL_CLOCK "getInternalClock"
#define MC_SET_SAMPLE_TAGGING "setSampleTagging"
#define MC_SET_EXTERNAL_CLOCK "setExternalClock"        // <DIx> [DOx <div>]

#define MC_BIGDUMP         "bigdump"
#define MC_GET_FWREV       "getFwrev"
#define MC_GET_CONFIG      "getConfig"
#define MC_GET_CALINFO     "getCalinfo"
#define MC_GET_CAPTURESTATS "getCaptureStats"
#define MC_GET_DEBUG        "getDebug"

#define MC_GET_MAX_CHANNELS "getAvailableChannels"
#define MC_GET_VRANGE        "getVoltsRange"


#define MC_SET_CAL            "setCal"            // setCal <cal-image>
#define MC_WAIT_STATE         "waitState"         // waitState <STATE> <timeout>

#define MC_RESERVE_AO        "reserveAO"          // reserveAO <nsamples>
#define MC_GET_RESERVED_AO   "getReservedAO"      // get reservation size


/*
 * Threshold Triggering
 */
#define MC_TH_CLEAR_ALL    "clearThresholds"
#define MC_TH_SELECT       "selectThresholdDistributor" // select threshold [option]
#define MC_TH_SELECT1       "selectThresholdDetector"   // ditto, preferred
// options are 
#define MC_TH_BILEVEL        "BILEVEL"               // two levels, single board
#define MC_TH_BILEVEL_MULTI  "BILEVEL_MULTI"         // two levels, multi board
#define MC_TH_EDGE           "EDGE"                  // edge trigger, single board
#define MC_TH_EDGE_MULTI     "EDGE_MULTI"            // edge trigger, multi board
#define MC_TH_FAST_MULTI     "FAST_MULTI"            // multi board sink, trigger source defined eleswhere

#define MC_TH_CHANNEL_SET  "setChannelThreshold"        // BILEVEL: <channel> <above> <below> 
                                                        // EDGE:    <channel> <above> 0        rising
							                            // EDGE:    <channel> 0       <below>  falling
/*
 * additional Master commands, Compact PCI only
 */
 
#define MC_SET_SYNC_ROUTE    "setSyncRoute"  // <func> <dest-mask>
#define MC_GET_SYNC_ROUTE    "getSyncRoute"  // no-args - prints all routes

#define MC_SET_USER_LED      "setUserLed"    // setUserLed <led-num> <mode>
                                             // <led-num>:  { 3 or 4 }
                                             // <mode> : OFF, ON, FLASH         
#define MC_SET_USER_LED_ON    "ON"
#define MC_SET_USER_LED_OFF   "OFF"
#define MC_SET_USER_LED_FLASH "FLASH"

  
/*
 * signal conditioning commands/queries
 * valid on SC device
 */

#define SC_SET_DIO         "setDIO"     // setDIO <dio-mask>
#define SC_GET_DIO         "getDIO"     // getDIO
#define SC_SET_CHANNEL     "setChannel" // setChannel <ch>,<g1>,<g2>,<v_ex>
#define SC_SET_STREAMING   "stream"     // stream <stride> <mean>
#define DIO_MASK_INPUT     '-'
#define DIO_MASK_OUTPUT1   '1'
#define DIO_MASK_OUTPUT0   '0'
#define DIO_MASK_INPUT0    'L'
#define DIO_MASK_INPUT1    'H'
#define DIO_CT_ACTIVE      'A'
#define DIO_CT_INACTIVE    'x'

/*
 * response prefixes
 */
#define ACQ32_ACK           "ACQ32:"
#define ACQ32_ACK_FAIL      "ERROR"
#define SIGCOND_ACK         "SIGCOND:"

/*
 * acq32 specific error codes
 */
 
enum {
    EACQ32_NO_INCOMING_I2O    = 3200,
    EACQ32_NO_MAPPING,
    EACQ32_OUT_OF_MAPPING_RANGE,
    EACQ32_OFFSET_NOT_ON_WHOLE_BUFFER_BOUNDARY
};

/*
 * ACQ32_IOREAD_LOCALBUF
 */
 
struct READ_LOCALBUF_DESCR {
    short* buffer;               /*  MUST be buffer returned from mmap() */
    int nsamples;                /*  max samples to return               */
    int istart;                  /*  zero based start point              */
    int istride;                 /*  samples to skip                     */
    int flags;                   /*  extra info                          */
};

#define READ_LOCALBUF_DESCR_FLAGS_NOWAIT    0x80000000

#define READ_LOCALBUF_MAXCHUNK 0x200000 // MAXCHUNKSIZE

/*
 * for ACQ32_IOREAD_GETPHYSICAL, arg must be the memory mapped buffer
 */
/*
 * args for SetSyncRoute
 */
 
// destinations - selection of
#define MI( n )                 "MI"#n             // n= { 0..5 }
#define MO( n )                 "MO"#n             // n= { 0..5 } 
#define J5( n )                 "J5"#n             // n= { 0..3 }
#define J3( n )                 "J3"#n             // n= { 4, 5 }
#define PXI_TRIG( n )           "PXI_TRIG"#n       // n= { 0..7 } 
#define PXI_STAR                "PXI_STAR"
#define PXI_CLOCK10             "PXI_CLOCK10"

// functions - one of 
#define DI( n )                 "DI"#n             // n = { 0..5 }
#define AICLK                   "AICLK"
#define AITRIG                  "AITRIG"
#define AOCLK                   "AOCLK"
#define AOTRIG                  "AOTRIG"
#define DOCLK                   "DOCLK"
#define DOTRIG                  "DOTRIG"


// used in ext clock, as well as DI
#define DO( n )                 "DO"#n             // n = { 0..5 }
/*
 * GUT - full flexible triggering
 */
 
#define GUT_FXN_AO    "AO"
#define GUT_FXN_AI    "AI"
#define GUT_FXN_DO    "DO"

#define GUT_PHASE( p ) "P"#p    // p { 1, 2 }
#define GUT_EVENT( e ) "E"#e    // e { 1, 2, 3 }

#define GUT_SETPHASE    "setPhase"    // setPhase [f] <p> <n> { p = 1, 2 }
#define GUT_GETPHASE    "getPhase"    // getPhase [f] <p> <key> { p = 1, 2 }


#define GUT_GETPHASE_KEY_REQUESTED_SAMPLES "requested-samples"
#define GUT_GETPHASE_KEY_ACTUAL_SAMPLES    "actual-samples"
#define GUT_GETPHASE_KEY_STATE             "state"

#define GUT_SETEVENT    "setEvent"    // setEvent [f] <e>  <or conditions> { e = 1, 2, 3 }
#define GUT_GETEVENT    "getEvent"    // getEvent [f] <e>  { e = 1, 2, 3 }
#define GUT_FIRE_EVENT  "fireEvent"   // fireEvent [f] <e>  { e = 1, 2, 3 }

#define SOFT_CLOCK         "SOFT_CLOCK"
#define INTERNAL_CLOCK     "INTERNAL_CLOCK"

#define GUT_SET_CLOCK   "setClock"    // setClock [f] { <d>, SOFT_CLOCK INTERNAL_CLOCK }

#define GUT_FIRE_CLOCK  "clockNow"

#define GUT_RESET        "resetGUT"


// event conditions

#define EV_TRUE            "EV_TRUE"             // - the event fires immediately
#define EV_SOFT            "EV_SOFT"             //- the event is fired "fireEvent" 
#define EV_TRIGGER_RISING  "EV_TRIGGER_RISING"   // <d> - the event is a rising edge of the external trigger signal
#define EV_TRIGGER_FALLING "EV_TRIGGER_FALLING"  // <d> - the event is a falling edge of the external trigger signal
#define EV_NONE            "EV_NONE"             // - clears all for this event
#define EV_DATA_EXCEEDS    "EV_DATA_EXCEEDS"     // <channel> <value>
#define EV_DATA_BELOW      "EV_DATA_BELOW"       // <channel> <value>


#endif //    _ACQ32IOCTL_H_

