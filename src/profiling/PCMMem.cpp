#include "PCMMem.hpp"

// constexpr uint32 max_sockets = 256;
// uint32 max_imc_channels = ServerUncoreCounterState::maxChannels;
// const uint32 max_edc_channels = ServerUncoreCounterState::maxChannels;
// const uint32 max_imc_controllers = ServerUncoreCounterState::maxControllers;
// bool SPR_CXL = false;

bool anyPmem(const ServerUncoreMemoryMetrics & metrics)
{
    return (metrics == Pmem) || (metrics == PmemMixedMode) || (metrics == PmemMemoryMode);
}

bool skipInactiveChannels = true;
bool enforceFlush = false;

void print_help(const string & prog_name)
{
    cout << "\n Usage: \n " << prog_name
         << " --help | [delay] [options] [-- external_program [external_program_options]]\n";
    cout << "   <delay>                           => time interval to sample performance counters.\n";
    cout << "                                        If not specified, or 0, with external program given\n";
    cout << "                                        will read counters only after external program finishes\n";
    cout << " Supported <options> are: \n";
    cout << "  -h    | --help  | /h               => print this help and exit\n";
    cout << "  -rank=X | /rank=X                  => monitor DIMM rank X. At most 2 out of 8 total ranks can be monitored simultaneously.\n";
    cout << "  -pmm | /pmm | -pmem | /pmem        => monitor PMM memory bandwidth and DRAM cache hit rate in Memory Mode (default on systems with PMM support).\n";
    cout << "  -mm                                => monitor detailed PMM Memory Mode metrics per-socket.\n";
    cout << "  -mixed                             => monitor PMM mixed mode (AppDirect + Memory Mode).\n";
    cout << "  -partial                           => monitor partial writes instead of PMM (default on systems without PMM support).\n";
    cout << "  -nc   | --nochannel | /nc          => suppress output for individual channels.\n";
    cout << "  -csv[=file.csv] | /csv[=file.csv]  => output compact CSV format to screen or\n"
         << "                                        to a file, in case filename is provided\n";
    cout << "  -columns=X | /columns=X            => Number of columns to display the NUMA Nodes, defaults to 2.\n";
    cout << "  -all | /all                        => Display all channels (even with no traffic)\n";
    cout << "  -i[=number] | /i[=number]          => allow to determine number of iterations\n";
    cout << "  -silent                            => silence information output and print only measurements\n";
    cout << "  --version                          => print application version\n";
    cout << "  -u                                 => update measurements instead of printing new ones\n";
    print_enforce_flush_option_help();
#ifdef _MSC_VER
    cout << "  --uninstallDriver | --installDriver=> (un)install driver\n";
#endif
    cout << " Examples:\n";
    cout << "  " << prog_name << " 1                  => print counters every second without core and socket output\n";
    cout << "  " << prog_name << " 0.5 -csv=test.log  => twice a second save counter values to test.log in CSV format\n";
    cout << "  " << prog_name << " /csv 5 2>/dev/null => one sample every 5 seconds, and discard all diagnostic output\n";
    cout << "\n";
}

void printSocketBWHeader(uint32 no_columns, uint32 skt, const bool show_channel_output)
{
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << "\n";
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--             Socket " << setw(2) << i << "             --|";
    }
    cout << "\n";
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << "\n";
    if (show_channel_output) {
       for (uint32 i=skt; i<(no_columns+skt); ++i) {
           cout << "|--     Memory Channel Monitoring     --|";
       }
       cout << "\n";
       for (uint32 i=skt; i<(no_columns+skt); ++i) {
           cout << "|---------------------------------------|";
       }
       cout << "\n";
    }
}

void printSocketRankBWHeader(uint32 no_columns, uint32 skt)
{
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << "\n";
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--               Socket " << setw(2) << i << "               --|";
    }
    cout << "\n";
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << "\n";
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--           DIMM Rank Monitoring        --|";
    }
    cout << "\n";
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << "\n";
}

void printSocketRankBWHeader_cvt(const uint32 numSockets, const uint32 num_imc_channels, const int rankA, const int rankB)
{
    printDateForCSV(Header1);
    for (uint32 skt = 0 ; skt < (numSockets) ; ++skt) {
        for (uint32 channel = 0; channel < num_imc_channels; ++channel) {
            if (rankA >= 0)
                cout << "SKT" << skt << "," << "SKT" << skt << ",";
            if (rankB >= 0)
                cout << "SKT" << skt << "," << "SKT" << skt << ",";
        }
    }
    cout << endl;

    printDateForCSV(Header2);
    for (uint32 skt = 0 ; skt < (numSockets) ; ++skt) {
        for (uint32 channel = 0; channel < num_imc_channels; ++channel) {
            if (rankA >= 0) {
                cout << "Mem_Ch" << channel << "_R" << rankA << "_reads,"
                     << "Mem_Ch" << channel << "_R" << setw(1) << rankA << "_writes,";
            }
            if (rankB >= 0) {
                cout << "Mem_Ch" << channel << "_R" << rankB << "_reads,"
                     << "Mem_Ch" << channel << "_R" << setw(1) << rankB << "_writes,";
            }
        }
    }
    cout << endl;
}

void printSocketChannelBW(PCM *, memdata_t *md, uint32 no_columns, uint32 skt)
{
    for (uint32 channel = 0; channel < max_imc_channels; ++channel) {
        // check all the sockets for bad channel "channel"
        unsigned bad_channels = 0;
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            if (md->iMC_Rd_socket_chan[i][channel] < 0.0 || md->iMC_Wr_socket_chan[i][channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                ++bad_channels;
        }
        if (bad_channels == no_columns) { // the channel is missing on all sockets in the row
            continue;
        }
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- Mem Ch " << setw(2) << channel << ": Reads (MB/s): " << setw(8) << md->iMC_Rd_socket_chan[i][channel] << " --|";
        }
        cout << "\n";
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|--            Writes(MB/s): " << setw(8) << md->iMC_Wr_socket_chan[i][channel] << " --|";
        }
        cout << "\n";
        if (md->metrics == Pmem)
        {
            for (uint32 i=skt; i<(skt+no_columns); ++i) {
                cout << "|--      PMM Reads(MB/s)   : " << setw(8) << md->iMC_PMM_Rd_socket_chan[i][channel] << " --|";
            }
            cout << "\n";
            for (uint32 i=skt; i<(skt+no_columns); ++i) {
                cout << "|--      PMM Writes(MB/s)  : " << setw(8) << md->iMC_PMM_Wr_socket_chan[i][channel] << " --|";
            }
            cout << "\n";
        }
    }
}

void printSocketChannelBW(uint32 no_columns, uint32 skt, uint32 num_imc_channels, const std::vector<ServerUncoreCounterState>& uncState1, const std::vector<ServerUncoreCounterState>& uncState2, uint64 elapsedTime, int rankA, int rankB)
{
    for (uint32 channel = 0; channel < num_imc_channels; ++channel) {
        if(rankA >= 0) {
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|-- Mem Ch " << setw(2) << channel << " R " << setw(1) << rankA << ": Reads (MB/s): " << setw(8) << (float) (getMCCounter(channel,ServerUncorePMUs::EventPosition::READ_RANK_A,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0)) << " --|";
          }
          cout << "\n";
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|--                Writes(MB/s): " << setw(8) << (float) (getMCCounter(channel,ServerUncorePMUs::EventPosition::WRITE_RANK_A,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0)) << " --|";
          }
          cout << "\n";
        }
        if(rankB >= 0) {
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|-- Mem Ch " << setw(2) << channel << " R " << setw(1) << rankB << ": Reads (MB/s): " << setw(8) << (float) (getMCCounter(channel,ServerUncorePMUs::EventPosition::READ_RANK_B,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0)) << " --|";
          }
          cout << "\n";
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|--                Writes(MB/s): " << setw(8) << (float) (getMCCounter(channel,ServerUncorePMUs::EventPosition::WRITE_RANK_B,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0)) << " --|";
          }
          cout << "\n";
        }
    }
}

void printSocketChannelBW_cvt(const uint32 numSockets, const uint32 num_imc_channels, const std::vector<ServerUncoreCounterState>& uncState1,
    const std::vector<ServerUncoreCounterState>& uncState2, const uint64 elapsedTime, const int rankA, const int rankB)
{
    printDateForCSV(Data);
    for (uint32 skt = 0 ; skt < numSockets; ++skt) {
        for (uint32 channel = 0 ; channel < num_imc_channels ; ++channel) {
            if(rankA >= 0) {
                cout << (float) (getMCCounter(channel,ServerUncorePMUs::EventPosition::READ_RANK_A,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0))
                << "," << (float) (getMCCounter(channel,ServerUncorePMUs::EventPosition::WRITE_RANK_A,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0)) << ",";
            }
            if(rankB >= 0) {
                cout << (float) (getMCCounter(channel,ServerUncorePMUs::EventPosition::READ_RANK_B,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0))
                << "," << (float) (getMCCounter(channel,ServerUncorePMUs::EventPosition::WRITE_RANK_B,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0)) << ",";
            }
        }
    }
    cout << endl;
}

uint32 getNumCXLPorts(PCM* m)
{
    static int numPorts = -1;
    if (numPorts < 0)
    {
        for (uint32 s = 0; s < m->getNumSockets(); ++s)
        {
            numPorts = (std::max)(numPorts, (int)m->getNumCXLPorts(s));
        }
        assert(numPorts >= 0);
    }
    return (uint32)numPorts;
}

void printSocketCXLBW(PCM* m, memdata_t* md, uint32 no_columns, uint32 skt)
{
    uint32 numPorts = getNumCXLPorts(m);
    if (numPorts > 0)
    {
        for (uint32 i = skt; i < (no_columns + skt); ++i) {
            cout << "|---------------------------------------|";
        }
        cout << "\n";
        for (uint32 i = skt; i < (no_columns + skt); ++i) {
            cout << "|--        CXL Port Monitoring        --|";
        }
        cout << "\n";
        for (uint32 i = skt; i < (no_columns + skt); ++i) {
            cout << "|---------------------------------------|";
        }
        cout << "\n";
    }
    for (uint32 port = 0; port < numPorts; ++port) {
        for (uint32 i = skt; i < (skt + no_columns); ++i) {
            cout << "|-- .mem                              --|";
        }
        cout << "\n";
        for (uint32 i = skt; i < (skt + no_columns); ++i) {
            cout << "|--            Writes(MB/s): " << setw(8) << md->CXLMEM_Wr_socket_port[i][port] << " --|";
        }
        cout << "\n";
        for (uint32 i = skt; i < (skt + no_columns); ++i) {
            cout << "|-- .cache                            --|";
        }
        cout << "\n";
        for (uint32 i = skt; i < (skt + no_columns); ++i) {
            cout << "|--           hst->dv(MB/s): " << setw(8) << md->CXLCACHE_Wr_socket_port[i][port] << " --|";
        }
        cout << "\n";
    }
}

float AD_BW(const memdata_t *md, const uint32 skt)
{
    const auto totalPMM = md->iMC_PMM_Rd_socket[skt] + md->iMC_PMM_Wr_socket[skt];
    return (max)(totalPMM - md->iMC_PMM_MemoryMode_Miss_socket[skt], float(0.0));
}

float PMM_MM_Ratio(const memdata_t *md, const uint32 skt)
{
    const auto dram = md->iMC_Rd_socket[skt] + md->iMC_Wr_socket[skt];
    return md->iMC_PMM_MemoryMode_Miss_socket[skt] / dram;
}

void printSocketBWFooter(uint32 no_columns, uint32 skt, const memdata_t *md)
{
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE" << setw(2) << i << " Mem Read (MB/s) :" << setw(9) << md->iMC_Rd_socket[i] << " --|";
    }
    cout << "\n";
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE" << setw(2) << i << " Mem Write(MB/s) :" << setw(9) << md->iMC_Wr_socket[i] << " --|";
    }
    cout << "\n";
    if (anyPmem(md->metrics))
    {
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- NODE" << setw(2) << i << " PMM Read (MB/s):  " << setw(8) << md->iMC_PMM_Rd_socket[i] << " --|";
        }
        cout << "\n";
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- NODE" << setw(2) << i << " PMM Write(MB/s):  " << setw(8) << md->iMC_PMM_Wr_socket[i] << " --|";
        }
        cout << "\n";
    }
    if (md->metrics == PmemMixedMode)
    {
        for (uint32 i = skt; i < (skt + no_columns); ++i)
        {
            cout << "|-- NODE" << setw(2) << i << " PMM AD Bw(MB/s):  " << setw(8) << AD_BW(md, i) << " --|";
        }
        cout << "\n";
        for (uint32 i = skt; i < (skt + no_columns); ++i)
        {
            cout << "|-- NODE" << setw(2) << i << " PMM MM Bw(MB/s):  " << setw(8) << md->iMC_PMM_MemoryMode_Miss_socket[i] << " --|";
        }
        cout << "\n";
        for (uint32 i = skt; i < (skt + no_columns); ++i)
        {
            cout << "|-- NODE" << setw(2) << i << " PMM MM Bw/DRAM Bw:" << setw(8) << PMM_MM_Ratio(md, i) << " --|";
        }
        cout << "\n";
    }
    else if (md->metrics == Pmem && md->M2M_NM_read_hit_rate_supported)
    {
        for (uint32 ctrl = 0; ctrl < max_imc_controllers; ++ctrl)
        {
            for (uint32 i=skt; i<(skt+no_columns); ++i) {
                cout << "|-- NODE" << setw(2) << i << "." << ctrl << " NM read hit rate :" << setw(6) << md->M2M_NM_read_hit_rate[i][ctrl] << " --|";
            }
            cout << "\n";
        }
    }
    if (md->metrics == PmemMemoryMode && md->iMC_NM_hit_rate_supported)
    {
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- NODE" << setw(2) << i << " NM hit rate:        " << setw(6) << md->iMC_NM_hit_rate[i] << " --|";
        }
        cout << "\n";
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- NODE" << setw(2) << i << " NM hits   (M/s):   " << setw(7) << (md->iMC_PMM_MemoryMode_Hit_socket[i])/1000000. << " --|";
        }
        cout << "\n";
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- NODE" << setw(2) << i << " NM misses (M/s):   " << setw(7) << (md->iMC_PMM_MemoryMode_Miss_socket[i])/1000000. << " --|";
        }
        cout << "\n";
    }
    if (md->metrics == PartialWrites)
    {
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- NODE" << setw(2) << i << " P. Write (T/s): " << dec << setw(10) << md->partial_write[i] << " --|";
        }
        cout << "\n";
    }
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE" << setw(2) << i << " Memory (MB/s): " << setw(11) << right << (md->iMC_Rd_socket[i]+md->iMC_Wr_socket[i]+
              md->iMC_PMM_Rd_socket[i]+md->iMC_PMM_Wr_socket[i]) << " --|";
    }
    cout << "\n";
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << "\n";
}

void display_bandwidth(PCM *m, memdata_t *md, const uint32 no_columns, const bool show_channel_output, const bool print_update, const float CXL_Read_BW)
{
    float sysReadDRAM = 0.0, sysWriteDRAM = 0.0, sysReadPMM = 0.0, sysWritePMM = 0.0;
    uint32 numSockets = m->getNumSockets();
    uint32 skt = 0;
    cout.setf(ios::fixed);
    cout.precision(2);

    if (print_update)
        clear_screen();

    while (skt < numSockets)
    {
        auto printHBM = [&]()
        {
                cout << "\
                    \r|---------------------------------------||---------------------------------------|\n\
                    \r|--                              Processor socket "
                     << skt << "                            --|\n\
                    \r|---------------------------------------||---------------------------------------|\n\
                    \r|--       DRAM Channel Monitoring     --||--        HBM Channel Monitoring     --|\n\
                    \r|---------------------------------------||---------------------------------------|\n\
                    \r";
                const uint32 max_channels = (std::max)(max_edc_channels, max_imc_channels);
                if (show_channel_output)
                {
                    float iMC_Rd, iMC_Wr, EDC_Rd, EDC_Wr;
                    for (uint64 channel = 0; channel < max_channels; ++channel)
                    {
                        if (channel < max_imc_channels)
                        {
                            iMC_Rd = md->iMC_Rd_socket_chan[skt][channel];
                            iMC_Wr = md->iMC_Wr_socket_chan[skt][channel];
                        }
                        else
                        {
                            iMC_Rd = -1.0;
                            iMC_Wr = -1.0;
                        }
                        if (channel < max_edc_channels)
                        {
                            EDC_Rd = md->EDC_Rd_socket_chan[skt][channel];
                            EDC_Wr = md->EDC_Wr_socket_chan[skt][channel];
                        }
                        else
                        {
                            EDC_Rd = -1.0;
                            EDC_Wr = -1.0;
                        }

                        if (iMC_Rd >= 0.0 && iMC_Wr >= 0.0 && EDC_Rd >= 0.0 && EDC_Wr >= 0.0)
                            cout << "|-- DRAM Ch " << setw(2) << channel << ": Reads (MB/s):" << setw(8) << iMC_Rd
                                 << " --||-- HBM Ch " << setw(2) << channel << ": Reads (MB/s):" << setw(9) << EDC_Rd
                                 << " --|\n|--             Writes(MB/s):" << setw(8) << iMC_Wr
                                 << " --||--            Writes(MB/s):" << setw(9) << EDC_Wr
                                 << " --|\n";
                        else if ((iMC_Rd < 0.0 || iMC_Wr < 0.0) && EDC_Rd >= 0.0 && EDC_Wr >= 0.0)
                            cout << "|--                                  "
                                 << " --||-- HBM Ch " << setw(2) << channel << ": Reads (MB/s):" << setw(9) << EDC_Rd
                                 << " --|\n|--                                  "
                                 << " --||--            Writes(MB/s):" << setw(9) << EDC_Wr
                                 << " --|\n";

                        else if (iMC_Rd >= 0.0 && iMC_Wr >= 0.0 && (EDC_Rd < 0.0 || EDC_Wr < 0.0))
                            cout << "|-- DRAM Ch " << setw(2) << channel << ": Reads (MB/s):" << setw(8) << iMC_Rd
                                 << " --||--                                  "
                                 << " --|\n|--             Writes(MB/s):" << setw(8) << iMC_Wr
                                 << " --||--                                  "
                                 << " --|\n";
                        else
                            continue;
                    }
                }
                cout << "\
                    \r|-- DRAM Mem Read  (MB/s):"
                     << setw(11) << md->iMC_Rd_socket[skt] << " --||-- HBM Read (MB/s):" << setw(14+3) << md->EDC_Rd_socket[skt] << " --|\n\
                    \r|-- DRAM Mem Write (MB/s):"
                     << setw(11) << md->iMC_Wr_socket[skt] << " --||-- HBM Write(MB/s):" << setw(14+3) << md->EDC_Wr_socket[skt] << " --|\n\
                    \r|-- DRAM Memory (MB/s)   :"
                     << setw(11) << md->iMC_Rd_socket[skt] + md->iMC_Wr_socket[skt] << " --||-- HBM (MB/s)     :" << setw(14+3) << md->EDC_Rd_socket[skt] + md->EDC_Wr_socket[skt] << " --|\n\
                    \r|---------------------------------------||---------------------------------------|\n\
                    \r";

                sysReadDRAM += (md->iMC_Rd_socket[skt] + md->EDC_Rd_socket[skt]);
                sysWriteDRAM += (md->iMC_Wr_socket[skt] + md->EDC_Wr_socket[skt]);
                skt += 1;
            };
        auto printRow = [&skt,&show_channel_output,&m,&md,&sysReadDRAM,&sysWriteDRAM, &sysReadPMM, &sysWritePMM](const uint32 no_columns)
        {
            printSocketBWHeader(no_columns, skt, show_channel_output);
            if (show_channel_output)
                printSocketChannelBW(m, md, no_columns, skt);
            printSocketBWFooter(no_columns, skt, md);
            printSocketCXLBW(m, md, no_columns, skt);
            for (uint32 i = skt; i < (skt + no_columns); i++)
            {
                sysReadDRAM += md->iMC_Rd_socket[i];
                sysWriteDRAM += md->iMC_Wr_socket[i];
                sysReadPMM += md->iMC_PMM_Rd_socket[i];
                sysWritePMM += md->iMC_PMM_Wr_socket[i];
            }
            skt += no_columns;
        };
        if (m->HBMmemoryTrafficMetricsAvailable())
        {
            printHBM(); // no_columns is ignored, always 1 socket at a time
        }
        else if ((skt + no_columns) <= numSockets) // Full row
        {
            printRow(no_columns);
        }
        else //Display the remaining sockets in this row
        {
            printRow(numSockets - skt);
        }
    }
    {
        cout << "\
            \r|---------------------------------------||---------------------------------------|\n";
        if (anyPmem(md->metrics))
        {
            cout << "\
            \r|--            System DRAM Read Throughput(MB/s):" << setw(14) << sysReadDRAM <<                                     "                --|\n\
            \r|--           System DRAM Write Throughput(MB/s):" << setw(14) << sysWriteDRAM <<                                    "                --|\n\
            \r|--             System PMM Read Throughput(MB/s):" << setw(14) << sysReadPMM <<                                      "                --|\n\
            \r|--            System PMM Write Throughput(MB/s):" << setw(14) << sysWritePMM <<                                     "                --|\n";
        }
        if (SPR_CXL)
        {
            cout << "\
            \r|--             System CXL Read Throughput(MB/s):" << setw(14) << CXL_Read_BW << "                --|\n";
        }
        cout << "\
            \r|--                 System Read Throughput(MB/s):" << setw(14) << sysReadDRAM+sysReadPMM <<                          "                --|\n\
            \r|--                System Write Throughput(MB/s):" << setw(14) << sysWriteDRAM+sysWritePMM <<                        "                --|\n\
            \r|--               System Memory Throughput(MB/s):" << setw(14) << sysReadDRAM+sysReadPMM+sysWriteDRAM+sysWritePMM << "                --|\n\
            \r|---------------------------------------||---------------------------------------|\n";
    }
}

constexpr float CXLBWWrScalingFactor = 0.5;

void display_bandwidth_csv(PCM *m, memdata_t *md, uint64 /*elapsedTime*/, const bool show_channel_output, const CsvOutputType outputType, const float CXL_Read_BW)
{
    const uint32 numSockets = m->getNumSockets();
    printDateForCSV(outputType);

    float sysReadDRAM = 0.0, sysWriteDRAM = 0.0, sysReadPMM = 0.0, sysWritePMM = 0.0;

    for (uint32 skt = 0; skt < numSockets; ++skt)
    {
        auto printSKT = [skt](int c = 1) {
            for (int i = 0; i < c; ++i)
                cout << "SKT" << skt << ',';
        };
        if (show_channel_output)
        {
            for (uint64 channel = 0; channel < max_imc_channels; ++channel)
            {
                bool invalid_data = false;
                if (md->iMC_Rd_socket_chan[skt][channel] < 0.0 && md->iMC_Wr_socket_chan[skt][channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                    invalid_data = true;

                choose(outputType,
                       [printSKT]() {
                           printSKT(2);
                       },
                       [&channel]() {
                           cout << "Ch" << channel << "Read,"
                                << "Ch" << channel << "Write,";
                       },
                       [&md, &skt, &channel, &invalid_data]() {
                           if (invalid_data)
                               cout << ",,";
                           else
                               cout << setw(8) << md->iMC_Rd_socket_chan[skt][channel] << ','
                                    << setw(8) << md->iMC_Wr_socket_chan[skt][channel] << ',';
                       });

                if (md->metrics == Pmem)
                {
                    choose(outputType,
                           [printSKT]() {
                               printSKT(2);
                           },
                           [&channel]() {
                               cout << "Ch" << channel << "PMM_Read,"
                                    << "Ch" << channel << "PMM_Write,";
                           },
                           [&skt, &md, &channel, &invalid_data]() {
                               if (invalid_data)
                                   cout << ",,";
                               else
                                   cout << setw(8) << md->iMC_PMM_Rd_socket_chan[skt][channel] << ','
                                        << setw(8) << md->iMC_PMM_Wr_socket_chan[skt][channel] << ',';
                           });
                }
            }
        }
        choose(outputType,
               [printSKT]() {
                   printSKT(2);
               },
               []() {
                   cout << "Mem Read (MB/s),Mem Write (MB/s),";
               },
               [&md, &skt]() {
                   cout << setw(8) << md->iMC_Rd_socket[skt] << ','
                        << setw(8) << md->iMC_Wr_socket[skt] << ',';
               });

        if (anyPmem(md->metrics))
        {
            choose(outputType,
                   [printSKT]() {
                       printSKT(2);
                   },
                   []() {
                       cout << "PMM_Read (MB/s), PMM_Write (MB/s),";
                   },
                   [&md, &skt]() {
                       cout << setw(8) << md->iMC_PMM_Rd_socket[skt] << ','
                            << setw(8) << md->iMC_PMM_Wr_socket[skt] << ',';
                   });
        }
        if (md->metrics == PmemMemoryMode && md->iMC_NM_hit_rate_supported)
        {
            choose(outputType,
                [printSKT]() {
                    printSKT(3);
                },
                []() {
                    cout << "NM hit rate,";
                    cout << "NM hits (M/s),";
                    cout << "NM misses (M/s),";
                },
                [&md, &skt]() {
                    cout << setw(8) << md->iMC_NM_hit_rate[skt]<< ',';
                    cout << setw(8) << md->iMC_PMM_MemoryMode_Hit_socket[skt]/1000000. << ',';
                    cout << setw(8) << md->iMC_PMM_MemoryMode_Miss_socket[skt]/1000000. << ',';
                });
        }
        if (md->metrics == Pmem && md->M2M_NM_read_hit_rate_supported)
        {
            for (uint32 c = 0; c < max_imc_controllers; ++c)
            {
                choose(outputType,
                    [printSKT]() {
                        printSKT();
                    },
                    [c]() {
                        cout << "iMC" << c << " NM read hit rate,";
                    },
                    [&md, &skt, c]() {
                        cout << setw(8) << md->M2M_NM_read_hit_rate[skt][c] << ',';
                    });
            }
        }
        if (md->metrics == PmemMixedMode)
        {
            choose(outputType,
                   [printSKT]() {
                       printSKT(3);
                   },
                   []() {
                       cout << "PMM_AD (MB/s), PMM_MM (MB/s), PMM_MM_Bw/DRAM_Bw,";
                   },
                   [&md, &skt]() {
                       cout << setw(8) << AD_BW(md, skt) << ','
                            << setw(8) << md->iMC_PMM_MemoryMode_Miss_socket[skt] << ','
                            << setw(8) << PMM_MM_Ratio(md, skt) << ',';
                   });
        }
        if (m->HBMmemoryTrafficMetricsAvailable() == false)
        {
            if (md->metrics == PartialWrites)
            {
                choose(outputType,
                       [printSKT]() {
                           printSKT();
                       },
                       []() {
                           cout << "P. Write (T/s),";
                       },
                       [&md, &skt]() {
                           cout << setw(10) << dec << md->partial_write[skt] << ',';
                       });
            }
        }

        choose(outputType,
               [printSKT]() {
                   printSKT();
               },
               []() {
                   cout << "Memory (MB/s),";
               },
               [&]() {
                   cout << setw(8) << md->iMC_Rd_socket[skt] + md->iMC_Wr_socket[skt] << ',';

                   sysReadDRAM += md->iMC_Rd_socket[skt];
                   sysWriteDRAM += md->iMC_Wr_socket[skt];
                   sysReadPMM += md->iMC_PMM_Rd_socket[skt];
                   sysWritePMM += md->iMC_PMM_Wr_socket[skt];
               });

        if (m->HBMmemoryTrafficMetricsAvailable())
        {
            if (show_channel_output)
            {
                for (uint64 channel = 0; channel < max_edc_channels; ++channel)
                {
                    if (md->EDC_Rd_socket_chan[skt][channel] < 0.0 && md->EDC_Wr_socket_chan[skt][channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                        continue;

                    choose(outputType,
                           [printSKT]() {
                               printSKT(2);
                           },
                           [&channel]() {
                               cout << "EDC_Ch" << channel << "Read,"
                                    << "EDC_Ch" << channel << "Write,";
                           },
                           [&md, &skt, &channel]() {
                               cout << setw(8) << md->EDC_Rd_socket_chan[skt][channel] << ','
                                    << setw(8) << md->EDC_Wr_socket_chan[skt][channel] << ',';
                           });
                }
            }

            choose(outputType,
                   [printSKT]() {
                       printSKT(3);
                   },
                   []() {
                       cout << "HBM Read (MB/s), HBM Write (MB/s), HBM (MB/s),";
                   },
                   [&]() {
                       cout << setw(8) << md->EDC_Rd_socket[skt] << ','
                            << setw(8) << md->EDC_Wr_socket[skt] << ','
                            << setw(8) << md->EDC_Rd_socket[skt] + md->EDC_Wr_socket[skt] << ',';

                       sysReadDRAM += md->EDC_Rd_socket[skt];
                       sysWriteDRAM += md->EDC_Wr_socket[skt];
                   });
        }
        for (uint64 port = 0; port < m->getNumCXLPorts(skt); ++port)
        {
            choose(outputType,
                [printSKT]() {
                    printSKT(2);
                },
                [&port]() {
                    cout 
                         << "CXL.mem_P" << port << "Write,"
                         << "CXL.cache_P" << port << "hst->dv,";
                },
                    [&md, &skt, &port]() {
                    cout << setw(8) << md->CXLMEM_Wr_socket_port[skt][port] << ','
                         << setw(8) << md->CXLCACHE_Wr_socket_port[skt][port] << ',';
                });
        }
    }

    if (anyPmem(md->metrics))
    {
        choose(outputType,
               []() {
                   cout << "System,System,System,System,";
               },
               []() {
                   cout << "DRAMRead,DRAMWrite,PMMREAD,PMMWrite,";
               },
               [&]() {
                   cout << setw(10) << sysReadDRAM << ','
                        << setw(10) << sysWriteDRAM << ','
                        << setw(10) << sysReadPMM << ','
                        << setw(10) << sysWritePMM << ',';
               });
    }

    if (SPR_CXL)
    {
        choose(outputType,
            []() {
                cout << "System,";
            },
            []() {
                cout << "CXLRead,";
            },
                [&]() {
                cout << setw(10) << CXL_Read_BW << ',';
            });
    }

    choose(outputType,
           []() {
               cout << "System,System,System\n";
           },
           []() {
               cout << "Read,Write,Memory\n";
           },
           [&]() {
               cout << setw(10) << sysReadDRAM + sysReadPMM << ','
                    << setw(10) << sysWriteDRAM + sysWritePMM << ','
                    << setw(10) << sysReadDRAM + sysReadPMM + sysWriteDRAM + sysWritePMM << "\n";
           });
}

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
    const uint64 SPR_CHA_CXL_Count)
{
    //const uint32 num_imc_channels = m->getMCChannelsPerSocket();
    //const uint32 num_edc_channels = m->getEDCChannelsPerSocket();
    memdata_t md;
    md.metrics = metrics;
    const auto cpu_model = m->getCPUModel();
    md.M2M_NM_read_hit_rate_supported = (cpu_model == PCM::SKX);
    md.iMC_NM_hit_rate_supported = (cpu_model == PCM::ICX);
    static bool mm_once = true;
    if (metrics == Pmem && md.M2M_NM_read_hit_rate_supported == false && md.iMC_NM_hit_rate_supported == true && mm_once)
    {
        cerr << "INFO: Use -mm option to monitor NM Memory Mode metrics\n";
        mm_once = false;
    }
    static bool mm_once1 = true;
    if (metrics == PmemMemoryMode && md.M2M_NM_read_hit_rate_supported == true && md.iMC_NM_hit_rate_supported == false && mm_once1)
    {
        cerr << "INFO: Use -pmem option to monitor NM Memory Mode metrics\n";
        mm_once1 = false;
    }

    for(uint32 skt = 0; skt < max_sockets; ++skt)
    {
        md.iMC_Rd_socket[skt] = 0.0;
        md.iMC_Wr_socket[skt] = 0.0;
        md.iMC_PMM_Rd_socket[skt] = 0.0;
        md.iMC_PMM_Wr_socket[skt] = 0.0;
        md.iMC_PMM_MemoryMode_Miss_socket[skt] = 0.0;
        md.iMC_PMM_MemoryMode_Hit_socket[skt] = 0.0;
        md.iMC_NM_hit_rate[skt] = 0.0;
        md.EDC_Rd_socket[skt] = 0.0;
        md.EDC_Wr_socket[skt] = 0.0;
        md.partial_write[skt] = 0;
		for (uint32 i = 0; i < max_imc_controllers; ++i)
		{
			md.M2M_NM_read_hit_rate[skt][i] = 0.;
		}
        for (size_t p = 0; p < ServerUncoreCounterState::maxCXLPorts; ++p)
        {
            md.CXLMEM_Rd_socket_port[skt][p] = 0.0;
            md.CXLMEM_Wr_socket_port[skt][p] = 0.0;
            md.CXLCACHE_Rd_socket_port[skt][p] = 0.0;
            md.CXLCACHE_Wr_socket_port[skt][p] = 0.0;
        }
    }

    auto toBW = [&elapsedTime](const uint64 nEvents)
    {
        return (float)(nEvents * 64 / 1000000.0 / (elapsedTime / 1000.0));
    };

    for(uint32 skt = 0; skt < m->getNumSockets(); ++skt)
    {
		const uint32 numChannels1 = (uint32)m->getMCChannels(skt, 0); // number of channels in the first controller

        if (m->HBMmemoryTrafficMetricsAvailable())
        {
            const float scalingFactor = ((float) m->getHBMCASTransferSize()) / float(64.);

            for (uint32 channel = 0; channel < max_edc_channels; ++channel)
            {
                if (skipInactiveChannels && getEDCCounter(channel, ServerUncorePMUs::EventPosition::READ, uncState1[skt], uncState2[skt]) == 0.0 && getEDCCounter(channel, ServerUncorePMUs::EventPosition::WRITE, uncState1[skt], uncState2[skt]) == 0.0)
                {
                    md.EDC_Rd_socket_chan[skt][channel] = -1.0;
                    md.EDC_Wr_socket_chan[skt][channel] = -1.0;
                    continue;
                }

                md.EDC_Rd_socket_chan[skt][channel] = scalingFactor * toBW(getEDCCounter(channel, ServerUncorePMUs::EventPosition::READ, uncState1[skt], uncState2[skt]));
                md.EDC_Wr_socket_chan[skt][channel] = scalingFactor * toBW(getEDCCounter(channel, ServerUncorePMUs::EventPosition::WRITE, uncState1[skt], uncState2[skt]));

                md.EDC_Rd_socket[skt] += md.EDC_Rd_socket_chan[skt][channel];
                md.EDC_Wr_socket[skt] += md.EDC_Wr_socket_chan[skt][channel];
            }
        }

        {
            for (uint32 channel = 0; channel < max_imc_channels; ++channel)
            {
                uint64 reads = 0, writes = 0, pmmReads = 0, pmmWrites = 0, pmmMemoryModeCleanMisses = 0, pmmMemoryModeDirtyMisses = 0;
                uint64 pmmMemoryModeHits = 0;
                reads = getMCCounter(channel, ServerUncorePMUs::EventPosition::READ, uncState1[skt], uncState2[skt]);
                writes = getMCCounter(channel, ServerUncorePMUs::EventPosition::WRITE, uncState1[skt], uncState2[skt]);
                if (metrics == Pmem)
                {
                    pmmReads = getMCCounter(channel, ServerUncorePMUs::EventPosition::PMM_READ, uncState1[skt], uncState2[skt]);
                    pmmWrites = getMCCounter(channel, ServerUncorePMUs::EventPosition::PMM_WRITE, uncState1[skt], uncState2[skt]);
                }
                else if (metrics == PmemMixedMode || metrics == PmemMemoryMode)
                {
                    pmmMemoryModeCleanMisses = getMCCounter(channel, ServerUncorePMUs::EventPosition::PMM_MM_MISS_CLEAN, uncState1[skt], uncState2[skt]);
                    pmmMemoryModeDirtyMisses = getMCCounter(channel, ServerUncorePMUs::EventPosition::PMM_MM_MISS_DIRTY, uncState1[skt], uncState2[skt]);
                }
                if (metrics == PmemMemoryMode)
                {
                    pmmMemoryModeHits = getMCCounter(channel, ServerUncorePMUs::EventPosition::NM_HIT, uncState1[skt], uncState2[skt]);
                }
                if (skipInactiveChannels && (reads + writes == 0))
                {
                    if ((metrics != Pmem) || (pmmReads + pmmWrites == 0))
                    {
                        if ((metrics != PmemMixedMode) || (pmmMemoryModeCleanMisses + pmmMemoryModeDirtyMisses == 0))
                        {

                            md.iMC_Rd_socket_chan[skt][channel] = -1.0;
                            md.iMC_Wr_socket_chan[skt][channel] = -1.0;
                            continue;
                        }
                    }
                }

                if (metrics != PmemMemoryMode)
                {
                    md.iMC_Rd_socket_chan[skt][channel] = toBW(reads);
                    md.iMC_Wr_socket_chan[skt][channel] = toBW(writes);

                    md.iMC_Rd_socket[skt] += md.iMC_Rd_socket_chan[skt][channel];
                    md.iMC_Wr_socket[skt] += md.iMC_Wr_socket_chan[skt][channel];
                }

                if (metrics == Pmem)
                {
                    md.iMC_PMM_Rd_socket_chan[skt][channel] = toBW(pmmReads);
                    md.iMC_PMM_Wr_socket_chan[skt][channel] = toBW(pmmWrites);

                    md.iMC_PMM_Rd_socket[skt] += md.iMC_PMM_Rd_socket_chan[skt][channel];
                    md.iMC_PMM_Wr_socket[skt] += md.iMC_PMM_Wr_socket_chan[skt][channel];

                    md.M2M_NM_read_hit_rate[skt][(channel < numChannels1) ? 0 : 1] += (float)reads;
                }
                else if (metrics == PmemMixedMode)
                {
                    md.iMC_PMM_MemoryMode_Miss_socket_chan[skt][channel] = toBW(pmmMemoryModeCleanMisses + 2 * pmmMemoryModeDirtyMisses);
                    md.iMC_PMM_MemoryMode_Miss_socket[skt] += md.iMC_PMM_MemoryMode_Miss_socket_chan[skt][channel];
                }
                else if (metrics == PmemMemoryMode)
                {
                    md.iMC_PMM_MemoryMode_Miss_socket[skt] += (float)((pmmMemoryModeCleanMisses + pmmMemoryModeDirtyMisses) / (elapsedTime / 1000.0));
                    md.iMC_PMM_MemoryMode_Hit_socket[skt] += (float)((pmmMemoryModeHits) / (elapsedTime / 1000.0));
                }
                else
                {
                    md.partial_write[skt] += (uint64)(getMCCounter(channel, ServerUncorePMUs::EventPosition::PARTIAL, uncState1[skt], uncState2[skt]) / (elapsedTime / 1000.0));
                }
            }
        }
        if (metrics == PmemMemoryMode)
        {
            const int64 imcReads = getFreeRunningCounter(ServerUncoreCounterState::ImcReads, uncState1[skt], uncState2[skt]);
            if (imcReads >= 0)
            {
                md.iMC_Rd_socket[skt] += toBW(imcReads);
            }
            const int64 imcWrites = getFreeRunningCounter(ServerUncoreCounterState::ImcWrites, uncState1[skt], uncState2[skt]);
            if (imcWrites >= 0)
            {
                md.iMC_Wr_socket[skt] += toBW(imcWrites);
            }
        }
        if (metrics == PmemMixedMode || metrics == PmemMemoryMode)
        {
            const int64 pmmReads = getFreeRunningCounter(ServerUncoreCounterState::PMMReads, uncState1[skt], uncState2[skt]);
            if (pmmReads >= 0)
            {
                md.iMC_PMM_Rd_socket[skt] += toBW(pmmReads);
            }
            else for(uint32 c = 0; c < max_imc_controllers; ++c)
            {
                md.iMC_PMM_Rd_socket[skt] += toBW(getM2MCounter(c, ServerUncorePMUs::EventPosition::PMM_READ, uncState1[skt],uncState2[skt]));
            }

            const int64 pmmWrites = getFreeRunningCounter(ServerUncoreCounterState::PMMWrites, uncState1[skt], uncState2[skt]);
            if (pmmWrites >= 0)
            {
                md.iMC_PMM_Wr_socket[skt] += toBW(pmmWrites);
            }
            else for(uint32 c = 0; c < max_imc_controllers; ++c)
            {
                md.iMC_PMM_Wr_socket[skt] += toBW(getM2MCounter(c, ServerUncorePMUs::EventPosition::PMM_WRITE, uncState1[skt],uncState2[skt]));;
            }
        }
        if (metrics == Pmem)
        {
            for(uint32 c = 0; c < max_imc_controllers; ++c)
            {
                if(md.M2M_NM_read_hit_rate[skt][c] != 0.0)
                {
                    md.M2M_NM_read_hit_rate[skt][c] = ((float)getM2MCounter(c, ServerUncorePMUs::EventPosition::NM_HIT, uncState1[skt],uncState2[skt]))/ md.M2M_NM_read_hit_rate[skt][c];
                }
            }
        }
        const auto all = md.iMC_PMM_MemoryMode_Miss_socket[skt] + md.iMC_PMM_MemoryMode_Hit_socket[skt];
        if (metrics == PmemMemoryMode && all != 0.0)
        {
            md.iMC_NM_hit_rate[skt] = md.iMC_PMM_MemoryMode_Hit_socket[skt] / all;
        }

        for (size_t p = 0; p < m->getNumCXLPorts(skt); ++p)
        {
            md.CXLMEM_Wr_socket_port[skt][p] = CXLBWWrScalingFactor * toBW(getCXLCMCounter((uint32)p, PCM::EventPosition::CXL_TxC_MEM, uncState1[skt], uncState2[skt]));
            md.CXLCACHE_Wr_socket_port[skt][p] = CXLBWWrScalingFactor * toBW(getCXLCMCounter((uint32)p, PCM::EventPosition::CXL_TxC_CACHE, uncState1[skt], uncState2[skt]));
        }
    }

    const auto CXL_Read_BW = toBW(SPR_CHA_CXL_Count);

    // For showing the output
    // if (csv)
    // {
    //     if (csvheader)
    //     {
    //         display_bandwidth_csv(m, &md, elapsedTime, show_channel_output, Header1, CXL_Read_BW);
    //         display_bandwidth_csv(m, &md, elapsedTime, show_channel_output, Header2, CXL_Read_BW);
    //         csvheader = false;
    //     }
    //     display_bandwidth_csv(m, &md, elapsedTime, show_channel_output, Data, CXL_Read_BW);
    // }
    // else
    // {
    //     display_bandwidth(m, &md, no_columns, show_channel_output, print_update, CXL_Read_BW);
    // }

    return md;
}

void calculate_bandwidth_rank(PCM *m, const std::vector<ServerUncoreCounterState> & uncState1, const std::vector<ServerUncoreCounterState>& uncState2,
		const uint64 elapsedTime, const bool csv, bool &csvheader, const uint32 no_columns, const int rankA, const int rankB)
{
    uint32 skt = 0;
    cout.setf(ios::fixed);
    cout.precision(2);
    uint32 numSockets = m->getNumSockets();

    if (csv) {
        if (csvheader) {
            printSocketRankBWHeader_cvt(numSockets, max_imc_channels, rankA, rankB);
            csvheader = false;
        }
        printSocketChannelBW_cvt(numSockets, max_imc_channels, uncState1, uncState2, elapsedTime, rankA, rankB);
    } else {
        while(skt < numSockets) {
            auto printRow = [&skt, &uncState1, &uncState2, &elapsedTime, &rankA, &rankB](const uint32 no_columns) {
                printSocketRankBWHeader(no_columns, skt);
                printSocketChannelBW(no_columns, skt, max_imc_channels, uncState1, uncState2, elapsedTime, rankA, rankB);
                for (uint32 i = skt; i < (no_columns + skt); ++i)
                    cout << "|-------------------------------------------|";
                cout << "\n";
                skt += no_columns;
            };
            // Full row
            if ((skt + no_columns) <= numSockets)
                printRow(no_columns);
            else //Display the remaining sockets in this row
                printRow(numSockets - skt);
        }
    }
}

void readState(std::vector<ServerUncoreCounterState>& state)
{
    auto* pcm = PCM::getInstance();
    assert(pcm);
    for (uint32 i = 0; i < pcm->getNumSockets(); ++i)
        state[i] = pcm->getServerUncoreCounterState(i);
};

