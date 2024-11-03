#include <iostream>
#ifdef _MSC_VER
#include <windows.h>
#include "windows/windriver.h"
#else
#include <unistd.h>
#include <signal.h>
#include <sys/time.h> // for gettimeofday()
#endif
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <assert.h>
#include "../../third-party/pcm/src/cpucounters.h"
#include "../../third-party/pcm/src/utils.h"

#define PCM_DELAY_DEFAULT 1.0 // in seconds
#define PCM_DELAY_MIN 0.015 // 15 milliseconds is practical on most modern CPUs

#define DEFAULT_DISPLAY_COLUMNS 2

using namespace std;
using namespace pcm;

#define max_sockets 256
#define max_imc_channels ServerUncoreCounterState::maxChannels
#define max_edc_channels ServerUncoreCounterState::maxChannels
#define max_imc_controllers ServerUncoreCounterState::maxControllers
#define SPR_CXL false
#define max_qpi ServerUncoreCounterState::maxXPILinks


typedef struct memdata {
    float iMC_Rd_socket_chan[max_sockets][ServerUncoreCounterState::maxChannels]{};
    float iMC_Wr_socket_chan[max_sockets][ServerUncoreCounterState::maxChannels]{};
    float iMC_PMM_Rd_socket_chan[max_sockets][ServerUncoreCounterState::maxChannels]{};
    float iMC_PMM_Wr_socket_chan[max_sockets][ServerUncoreCounterState::maxChannels]{};
    float iMC_PMM_MemoryMode_Miss_socket_chan[max_sockets][ServerUncoreCounterState::maxChannels]{};
    float iMC_Rd_socket[max_sockets]{};
    float iMC_Wr_socket[max_sockets]{};
    float iMC_PMM_Rd_socket[max_sockets]{};
    float iMC_PMM_Wr_socket[max_sockets]{};
    float CXLMEM_Rd_socket_port[max_sockets][ServerUncoreCounterState::maxCXLPorts]{};
    float CXLMEM_Wr_socket_port[max_sockets][ServerUncoreCounterState::maxCXLPorts]{};
    float CXLCACHE_Rd_socket_port[max_sockets][ServerUncoreCounterState::maxCXLPorts]{};
    float CXLCACHE_Wr_socket_port[max_sockets][ServerUncoreCounterState::maxCXLPorts]{};
    float iMC_PMM_MemoryMode_Miss_socket[max_sockets]{};
    bool iMC_NM_hit_rate_supported{};
    float iMC_PMM_MemoryMode_Hit_socket[max_sockets]{};
    bool M2M_NM_read_hit_rate_supported{};
    float iMC_NM_hit_rate[max_sockets]{};
    float M2M_NM_read_hit_rate[max_sockets][max_imc_controllers]{};
    float EDC_Rd_socket_chan[max_sockets][max_edc_channels]{};
    float EDC_Wr_socket_chan[max_sockets][max_edc_channels]{};
    float EDC_Rd_socket[max_sockets]{};
    float EDC_Wr_socket[max_sockets]{};
    uint64 partial_write[max_sockets]{};
    ServerUncoreMemoryMetrics metrics{};
} memdata_t;

bool anyPmem(const ServerUncoreMemoryMetrics & metrics);

void print_help(const string & prog_name);

void printSocketBWHeader(uint32 no_columns, uint32 skt, const bool show_channel_output);


void printSocketRankBWHeader(uint32 no_columns, uint32 skt);


void printSocketRankBWHeader_cvt(const uint32 numSockets, const uint32 num_imc_channels, const int rankA, const int rankB);


void printSocketChannelBW(PCM *, memdata_t *md, uint32 no_columns, uint32 skt);


void printSocketChannelBW(uint32 no_columns, uint32 skt, uint32 num_imc_channels, const std::vector<ServerUncoreCounterState>& uncState1, const std::vector<ServerUncoreCounterState>& uncState2, uint64 elapsedTime, int rankA, int rankB);


void printSocketChannelBW_cvt(const uint32 numSockets, const uint32 num_imc_channels, const std::vector<ServerUncoreCounterState>& uncState1,
    const std::vector<ServerUncoreCounterState>& uncState2, const uint64 elapsedTime, const int rankA, const int rankB);

uint32 getNumCXLPorts(PCM* m);


void printSocketCXLBW(PCM* m, memdata_t* md, uint32 no_columns, uint32 skt);

float AD_BW(const memdata_t *md, const uint32 skt);

float PMM_MM_Ratio(const memdata_t *md, const uint32 skt);

void printSocketBWFooter(uint32 no_columns, uint32 skt, const memdata_t *md);

void display_bandwidth(PCM *m, memdata_t *md, const uint32 no_columns, const bool show_channel_output, const bool print_update, const float CXL_Read_BW);

void display_bandwidth_csv(PCM *m, memdata_t *md, uint64 /*elapsedTime*/, const bool show_channel_output, const CsvOutputType outputType, const float CXL_Read_BW);

memdata_t calculate_bandwidth(PCM *m,
    const std::vector<ServerUncoreCounterState>& uncState1,
    const std::vector<ServerUncoreCounterState>& uncState2,
    const uint64 elapsedTime,
    const bool csv,
    bool & csvheader,
    uint32 no_columns,
    const ServerUncoreMemoryMetrics & metrics,
    const bool show_channel_output,
    const bool print_update,
    const uint64 SPR_CHA_CXL_Count);


void calculate_bandwidth_rank(PCM *m, const std::vector<ServerUncoreCounterState> & uncState1, const std::vector<ServerUncoreCounterState>& uncState2,
		const uint64 elapsedTime, const bool csv, bool &csvheader, const uint32 no_columns, const int rankA, const int rankB);

void readState(std::vector<ServerUncoreCounterState>& state);

