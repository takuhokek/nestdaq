
#include <thread>
#include <vector>
#include <numeric>
#include <fairmq/runFairMQDevice.h>
#include <string>
#include <chrono>
#include <array>
#include <iostream>

#include "AmQStrTdcData.h"
#include "FileSinkHeader.h"
#include "FileSinkTrailer.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "utility/HexDump.h"

#include "Recbe2Slowdash.h"

static constexpr std::string_view MyClass{"Recbe2Slowdash"};

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description &options)
{
    using opt = Recbe2Slowdash::OptionKey;
    options.add_options()
    (opt::InputChannelName.data(), bpo::value<std::string>()->default_value(opt::InputChannelName.data()), "Name of input channel\n")
    //
    (opt::Multipart.data(), bpo::value<std::string>()->default_value("true"), "Handle multipart message\n");
}

//_____________________________________________________________________________
FairMQDevicePtr getDevice(const FairMQProgOptions &)
{
    return new Recbe2Slowdash;
}

//_____________________________________________________________________________
void PrintConfig(const fair::mq::ProgOptions* config, std::string_view name, std::string_view funcname)
{
    auto c = config->GetPropertiesAsStringStartingWith(name.data());
    std::ostringstream ss;
    ss << funcname << "\n\t " << name << "\n";
    for (const auto &[k, v] : c) {
        ss << "\t key = " << k << ", value = " << v << "\n";
    }
    LOG(debug) << ss.str();
}

//_____________________________________________________________________________
bool Recbe2Slowdash::HandleData(FairMQMessagePtr &msg, int index)
{
    const auto ptr = reinterpret_cast<unsigned char*>(msg->GetData());
    //  std::string s(ptr, ptr+msg->GetSize());
    //  LOG(debug) << __FUNCTION__ << " received = " << s << " [" << index << "] " << fNumMessages;

    LOG(debug) << __FUNCTION__ << " received = " << " [" << index << "] " << fNumMessages;

    for(size_t i = 0; i < msg->GetSize(); i++) {
        printf("%03d ", ptr[i]);
        if( ( (i+1)%8 )==0 )
            printf("\n");
    }

    ++fNumMessages;
    return true;
}

//_____________________________________________________________________________
uint64_t Recbe2Slowdash::TotalLength(const FairMQParts& parts)
{
    auto & c = const_cast<FairMQParts&>(parts);

    return std::accumulate(c.begin(), c.end(), static_cast<uint64_t>(0),
    [](uint64_t init, auto& m) -> uint64_t {
        return (!m) ? init : init + m->GetSize();
    });
}

//_____________________________________________________________________________
bool Recbe2Slowdash::HandleMultipartData(FairMQParts &msgParts, int index)
{
    namespace TF  = TimeFrame;
    namespace STF = SubTimeFrame;
    namespace FSH = FileSinkHeader;
////////////////////////////////////////////////////////////////////

    Data_array array = {};
    Hit_array harray = {};
    bool Endianfix = true;

////////////////////////////////////////////////////////////////////

    auto length = TotalLength(msgParts);
    // std::vector<char> rawbuf;
    // rawbuf.reserve(length);
    // int head = sizeof(TF::Header) + sizeof(STF::Header);
    // std::cout << "length: "<< length << std::endl;
    std::cout << "PartsCount" << msgParts.Size()  << std::endl;
    unsigned char Packettype = 0, BoardID = 0;
    // unsigned short c_SentNumber = 0, c_Trigtime = 0, c_Datalength = 0, c_Upperbit = 0, c_Lowerbit = 0;
    for (const auto& msg : msgParts) {
        const auto ptr = reinterpret_cast<unsigned char*>(msg->GetData());
        size_t gsize = msg->GetSize();
        // std::cout <<"GetSize: " << gsize << std::endl;
        unsigned int SampleclockNum = 0, AorTdcNum = 0, ChNum = 0;
        switch(gsize){
            case 24:
                break;

            case 48:
                break;

            default :
                SampleclockNum = 0, AorTdcNum = 0, ChNum = 0;
                // Packettype  = ptr[0];
                BoardID     = ptr[1];
                // unsigned short SentNumber = *reinterpret_cast<unsigned short*>(&ptr[2]);
                // unsigned short Trigtime   = *reinterpret_cast<unsigned short*>(&ptr[4]);
                // unsigned short Datalength = *reinterpret_cast<unsigned short*>(&ptr[6]);
                // unsigned short Upperbit   = *reinterpret_cast<unsigned short*>(&ptr[8]);
                // unsigned short Lowerbit   = *reinterpret_cast<unsigned short*>(&ptr[10]);
                // c_SentNumber  = (!Endianfix) ? SentNumber : swapEndian(SentNumber); 
                // c_Trigtime    = (!Endianfix) ? Trigtime : swapEndian(Trigtime);
                // c_Datalength  = (!Endianfix) ? Datalength : swapEndian(Datalength);
                // c_Upperbit    = (!Endianfix) ? Upperbit : swapEndian(Upperbit);
                // c_Lowerbit    = (!Endianfix) ? Lowerbit : swapEndian(Lowerbit);
                // printf("\npacket: 0x%4X\n", Packettype);
                // printf("boardid: 0x%4X\n", BoardID);
                // printf("sentnum: 0x%4X\n", SentNumber);
                // printf("trigtime: 0x%4d\n", c_Trigtime);
                // printf("datalength: 0x%4X\n", Datalength);
                // printf("ubit: 0x%4X\n", Upperbit);
                // printf("lbit: 0x%4X\n", Lowerbit);
                for(size_t i = 12; i < msg->GetSize(); i+=2) {
                    //Pakking Part////////////////////////////////////////////
                    // std::cout << "DATA Adress check : " << i << std::endl;
                    if (ChNum == 48){
                        AorTdcNum++;
                        ChNum = 0;
                    }
                    if (AorTdcNum == 2){
                        SampleclockNum ++;
                        AorTdcNum = 0;
                    }
                    if (SampleclockNum == 32){
                        LOG(debug2) << "Error (Diffelent of Sampleclock or Dataformat)";
                        break;
                    }
                    unsigned short main_data = *reinterpret_cast<unsigned short*>(&ptr[i]);
                    unsigned short c_main_data = (!Endianfix) ? main_data : swapEndian(main_data);
                    // printf("SampleclockNum: 0x%X\n", SampleclockNum);
                    // printf("AorTdc: 0x%X\n", AorTdcNum);
                    // printf("ChNum: 0x%X\n", ChNum);
                    // printf("Data: 0x%X\n", main_data);
                    // printf("Data10: 0d%d\n", c_main_data);
                    if(AorTdcNum == 1){
                        harray[ChNum] = (c_main_data >> 15) & 1;
                    }else{
                        array[SampleclockNum][AorTdcNum][ChNum] = c_main_data;
                    }
                    ChNum++;
                }
                // SaveDatatoBuf(harray, BoardID);
                SaveDatatoBuf2(array, harray, BoardID);
                break;
        }
        ++fNumMessages;
    }//msg
    writeredis3();
    // writeredis();
    return true;
}

//_____________________________________________________________________________
void Recbe2Slowdash::Init()
{
    PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
    PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);

    fNumMessages = 0;
    // fopened = false;
    Initialize("tcp://127.0.0.1:6379/3");
}

//_____________________________________________________________________________
void Recbe2Slowdash::InitTask()
{
    PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
    PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);
    LOG(debug) << MyClass << " InitTask";

    using opt = OptionKey;
    fInputChannelName = fConfig->GetProperty<std::string>(opt::InputChannelName.data());
    LOG(debug) << " input channel = " << fInputChannelName;

    //////
    /*  FairMQMessagePtr msginfo(NewMessage());
    int nrecv=0;
    while(true){
      if (Receive(msginfo, fInputChannelName) <= 0) {
        LOG(debug) << __func__ << " Trying to get FEMIfo " << nrecv;
        nrecv++;
      }else{
        break;
      }
    }

    const auto ptr = reinterpret_cast<unsigned char*>(msginfo->GetData());

    for(size_t i = 0; i < msginfo->GetSize(); i++){
      printf("%02x", ptr[i]);
      if( ( (i+1)%8 )==0 )
        printf("\n");
    }
    */
    ////

    const auto &isMultipart = fConfig->GetProperty<std::string>(opt::Multipart.data());
    if (isMultipart=="true" || isMultipart=="1") {
        LOG(warn) << " set multipart data handler";
        OnData(fInputChannelName, &Recbe2Slowdash::HandleMultipartData);
    } else {
        LOG(warn) << " set data handler";
        OnData(fInputChannelName, &Recbe2Slowdash::HandleData);
    }

}

//_____________________________________________________________________________
void Recbe2Slowdash::PreRun()
{
    const auto rnumber = fConfig->GetProperty<std::string>("run_number");
    LOG(debug) << "run_number: " << rnumber;
}
//_____________________________________________________________________________
void Recbe2Slowdash::PostRun()
{
    using opt = OptionKey;
    LOG(debug) << __func__;
    int nrecv=0;
    while (true) {
        const auto &isMultipart = fConfig->GetProperty<std::string>(opt::Multipart.data());
        if (isMultipart=="true" || isMultipart=="1") {
            FairMQParts parts;
            if (Receive(parts, fInputChannelName) <= 0) {
                LOG(debug) << __func__ << " no data received " << nrecv;
                ++nrecv;
                if (nrecv>10) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            } else {
                LOG(debug) << __func__ << " print data";
                HandleMultipartData(parts, 0);
            }
        } else {
            FairMQMessagePtr msg(NewMessage());
            if (Receive(msg, fInputChannelName) <= 0) {
                LOG(debug) << __func__ << " no data received " << nrecv;
                ++nrecv;
                if (nrecv>10) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            } else {
                LOG(debug) << __func__ << " print data";
                HandleData(msg, 0);
            }
        }
        LOG(debug) << __func__ << " done";
    }
    Finalize();
}

