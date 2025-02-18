/*
 * @file EventBuilder.h
 * @brief EventBuilder for NestDAQ
 * @date Created :
 *       Last Modified : 2023/07/21 10:06:28
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */

#include "EventBuilder.h"
#include "fairmq/runDevice.h"

#include "utility/MessageUtil.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "UnpackTdc.h"


constexpr int  nTimeBins = 131072;

using nestdaq::EventBuilder;
namespace bpo = boost::program_options;



EventBuilder::EventBuilder()
{

}

// initialize options
void EventBuilder::InitTask()
{

    using opt = OptionKey;

    fInputChannelName  = fConfig->GetValue<std::string>(opt::InputChannelName.data());
    fOutputChannelName = fConfig->GetValue<std::string>(opt::OutputChannelName.data());
    // inptut channels
    LOG(info)
            << "InitTask : Input Channel  = " << fInputChannelName
            << "InitTask : Output Channel = " << fOutputChannelName;

    // identity
    fName = fConfig->GetProperty<std::string>("id");
    std::istringstream ss(fName.substr(fName.rfind("-") + 1));
    ss >> fId;


    fNumDestination = GetNumSubChannels(fOutputChannelName);
    fPollTimeoutMS  = std::stoi(fConfig->GetProperty<std::string>(opt::PollTimeout.data()));
    fTrefID  = std::stol(fConfig->GetProperty<std::string>(opt::TrefID.data()),nullptr,16);
//    fTrefID  = 0xc0a802a8;
    fTrefCH  = std::stoi(fConfig->GetProperty<std::string>(opt::TrefCH.data()));

    LOG(info) << "TrefID : 0x" << std::hex << fTrefID << ", TrefCH : " << fTrefCH;
    LOG(info) << "InitTask : id = " << fName;

    // prepare time bin with 4 ns resolution

    fTref.resize(0);
    fTDCIdx.resize(nTimeBins,-1);
}




bool EventBuilder::ConditionalRun()
{
#if 0  
    LOG(info) << "Let's build";
#endif    
    std::chrono::system_clock::time_point sw_start, sw_end;

    // Receive data

    FairMQParts inParts;
    FairMQParts outParts;

    if (Receive(inParts, fInputChannelName,0,1) <= 0) return true;
    // inParts should have at least two messages
    assert (inParts.Size() >= 2);

    ////////////////////////////////////////////////////
    // analyze the data
    ////////////////////////////////////////////////////

    struct stf {
        struct SubTimeFrame::Header *header;
        std::vector<uint64_t*> data;
        std::vector<uint64_t> datasize;
        std::vector<uint64_t*> hbd;
        std::vector<std::vector<uint32_t>> trefs;
        FairMQMessage* headerPtr;
        std::vector<FairMQMessage*> hbdPtr;
    };

    std::vector<std::vector<uint32_t>> target = { {fTrefID, fTrefCH} };

    std::vector<struct stf> stfs;
    FairMQMessage* tfbhdrPtr;

    for (uint32_t iPart = 0, nParts = inParts.Size(); iPart < nParts; ++iPart) {
#if 0
        LOG(info) << "part " << iPart << " being analyzied";
#endif 	
        auto& part = inParts[iPart];
        auto headertype = *reinterpret_cast<uint64_t *>(part.GetData());

        if (headertype == Filter::MAGIC) {
            continue;
        }

        // count the number of subtime frames and the heartbeat frames for  each FEM
        if (headertype == TimeFrame::MAGIC) {

            sw_start = std::chrono::system_clock::now();

            auto tfbheader = reinterpret_cast<struct TimeFrame::Header*>(part.GetData());
            tfbhdrPtr = &part;
#if 0
            LOG(info) << "TFB  found " << std::hex << tfbheader->magic << std::dec << " in Part[" << iPart << "]";
            LOG(info) << "    numSources = " << tfbheader->numSource << ", length = " << tfbheader->length ;
#endif
            stfs.resize(tfbheader->numSource);

            // loop for all the sources
            for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
                iPart++;
                if (iPart >= nParts) {
                    LOG(warn) << "num part is not different";
                    break;
                }
                auto& part = inParts[iPart];
                auto header = reinterpret_cast<struct SubTimeFrame::Header*>(part.GetData());
                if (header->magic != SubTimeFrame::MAGIC) {
                    LOG(error) << "header magic is required " << std::hex << header->magic;

                    for (int i = 0 ; i < inParts.Size() ; i++) {
                        uint64_t *pmsgdata = reinterpret_cast<uint64_t *>(inParts[i].GetData());
                        std::cout << "#D Msg top: " << i << ":" << std::hex << pmsgdata[0];
                        if (pmsgdata[0] == SubTimeFrame::MAGIC) {
                            auto hh = reinterpret_cast<struct SubTimeFrame::Header*>(inParts[i].GetData());
                            std::cout << " Nmsg: " << hh->numMessages << std::endl;
                        } else {
                            std::cout << std::endl;
                        }
                    }

                    //break;
		    return true;
                }
#if 0
                LOG(info) << "STFB found " << std::hex <<  header->magic << std::dec << " in Part[" << iPart << "]";
                LOG(info) << "    femType = " << header->femType << ", femID = " << std::hex <<  header->femId ;
                LOG(info) << "    numMessages = " << header->numMessages << ", length = " << header->length ;
#endif
                auto& stf = stfs[iSrc];
                stf.header = header;
                stf.headerPtr = &part;

                // loop for all the messages
                for (int iMsg = 0, nMsg = header->numMessages - 1; iMsg < nMsg; ++iMsg) {
                    iPart++;
                    if (iPart >= nParts) {
                        LOG(warn) << "num part is different";
                        break;
                    }
                    auto& part = inParts[iPart];
                    auto header = reinterpret_cast<struct SubTimeFrame::Header*>(part.GetData());
                    if (header->magic == SubTimeFrame::MAGIC) {
                        LOG(warn) << "numMessages is not correct used " << iMsg << " instead of  " << header->numMessages;
                        iPart--;
                        break;
                    }

                    // check if the heartbeat delimiter or spill-end delimiter exists

                    auto  data = reinterpret_cast<uint64_t*>(part.GetData());
#if 0
                    LOG(info) << "  data? " << std::hex << *data << std::dec << " in Part[" << iPart << "]";
#endif
                    if (!IsEndDelimiter(*data)) {
                        stf.datasize.emplace_back(part.GetSize());
                        stf.data.emplace_back(data);
                        iMsg++;
                        iPart++;
                        data = reinterpret_cast<uint64_t*>(inParts[iPart].GetData());
#if 0
                        LOG(info) << "  hbd?  " << std::hex << *data << std::dec << " in Part[" << iPart << "]";
#endif
                        stf.hbd.emplace_back(data);
                        stf.hbdPtr.emplace_back(&inParts[iPart]);
                    } else {
                        stf.data.emplace_back(nullptr);
                        stf.datasize.emplace_back(0);
                        stf.hbd.emplace_back(data);
                        stf.hbdPtr.emplace_back(&part);
                    }
                }
            }
            sw_end = std::chrono::system_clock::now();
            uint32_t elapse = std::chrono::duration_cast<std::chrono::microseconds>(
                                  sw_end - sw_start).count();
            //      std::cout << "#Elapse: " << std::dec << elapse << " us" << std::endl;

            sw_start = std::chrono::system_clock::now();
            // calculate time references
            struct tdc64 tdc;
            int (* unpack[4]) (uint64_t data, struct tdc64 *tdc) = {
                nullptr, TDC64L::v1::Unpack, TDC64H::Unpack, TDC64L::v2::Unpack
            };

            int nframe = stfs[0].data.size();

            // search for TDC data for each heartbeat frames
            // printf("searching heartbeat frames\n");

            for (int iframe = 0; iframe < nframe; ++iframe) {
                LOG(info) << "Frame " << iframe << " start ";
                fTref.clear();
                fTref.resize(0);
                fTDCData.clear();
                fTDCData.resize(0);
                fTDCIdx.assign(nTimeBins,-1);
                sw_end = std::chrono::system_clock::now();
                elapse = std::chrono::duration_cast<std::chrono::microseconds>(sw_end - sw_start).count();
                //		  std::cout << "#clear Elapse: " << std::dec << elapse << " us" << std::endl;
                sw_start = std::chrono::system_clock::now();

                // store TDC data and time references
                for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
                    const auto& stf = stfs[iSrc];
#if   0
                    LOG(info) << "[" << iSrc << "]" << "num data = " << stf.data.size() << " in " << std::hex << stf.header->femId
                        << "(type = " << stf.header->femType << ")";
#endif
                    printf("iSrc = %d\n",iSrc);
                    auto femtype = stf.header->femType;

                    uint32_t size = stf.datasize[iframe]/sizeof(uint64_t);
                    //	  printf("size[%d][%d] = %d\n",iSrc,iframe,size);

                    for (int idata = 0; idata < size; ++idata) {
                        // if (idata == 0) printf ("%lx\n",stf.data[iframe][idata]);

                        unpack[femtype](stf.data[iframe][idata],&tdc);

//	 		if((idata%100) == 0 || idata > size - 10) {
//			    printf("unpacked %lx idata = %d / %d, tdc.tdc4n = %d\n",
//			    stf.data[iframe][idata],idata,size,tdc.tdc4n);
//			}
                        if (tdc.tdc == -1) continue;  // not a TDC data
                        if (tdc.tdc4n >= fTDCIdx.size() || tdc.tdc4n < 0) {
                            printf("tdc.tdc4n = %d %x\n",tdc.tdc4n,stf.data[iframe][idata]);
                        }
                        if (fTDCIdx[tdc.tdc4n] == -1) {
                            fTDCData.emplace_back(std::vector<std::vector<uint64_t>>(nSrc,std::vector<uint64_t>()));
                            fTDCIdx[tdc.tdc4n] = fTDCData.size() - 1;
                        }
                        fTDCData[fTDCIdx[tdc.tdc4n]][iSrc].push_back(stf.data[iframe][idata]);
                        if (stf.header->femId != target[0][0]) continue;
                        if (tdc.ch != target[0][1]) continue;
#if 0
                        LOG(info) << "target found at " << iSrc << "/" << iframe << "/"
                            << " tdc.ch = " << tdc.ch << " tdc.tdc = " << tdc.tdc;
#endif
                        fTref.emplace_back(tdc.tdc4n);
                    }
                    //	  printf("analyzed %d\n",iSrc);

                }
#if 0
                LOG(info) << fTref.size() << " targets found";
#endif
                sw_end = std::chrono::system_clock::now();
                elapse = std::chrono::duration_cast<std::chrono::microseconds>(
                             sw_end - sw_start).count();
                //	std::cout << "#Calc tref Elapse: " << std::dec << elapse << " us" << std::endl;

                sw_start = std::chrono::system_clock::now();

                //////////////////////////////////////////////////
                // prepare output for this frame
                //////////////////////////////////////////////////
                // prepare timeframe for HBD block
                FairMQMessagePtr msgTFBHDR(NewMessage(sizeof(TimeFrame::Header)));
                auto tfbhdr = reinterpret_cast<struct TimeFrame::Header*>(msgTFBHDR->GetData());
                *tfbhdr = *tfbheader;
                outParts.AddPart(std::move(msgTFBHDR));
                int totalSize = sizeof(TimeFrame::Header);
                // prepare heartbeat frames
                for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
                    //	  std::cout << "iSrc = " << iSrc << std::endl;
                    FairMQMessagePtr msgSTFBHDR(NewMessage(sizeof(SubTimeFrame::Header)));
                    auto stfheader = reinterpret_cast<struct SubTimeFrame::Header*>(msgSTFBHDR->GetData());
                    *stfheader = *stfs[iSrc].header;
                    stfheader->length = sizeof(SubTimeFrame::Header) + sizeof(uint64_t) * 2;
                    //	  std::cout << ". iSrc = " << iSrc << std::endl;
                    totalSize += stfheader->length;
                    outParts.AddPart(std::move(msgSTFBHDR));
                    FairMQMessagePtr msgHBD(fTransportFactory->CreateMessage());
                    //	  std::cout << ".. iSrc = " << iSrc << std::endl;
                    msgHBD->Copy(*stfs[iSrc].hbdPtr[iframe]);
                    outParts.AddPart(std::move(msgHBD));
                }
                tfbhdr->length = totalSize;

                //	std::cout << "Heartbeat frame block is prepared" << std::endl;


                // prepare timeframe for data block block
                auto outdataptr = std::make_unique<std::vector<uint32_t>>();
                auto& outdata = *outdataptr;
#if 0		
                LOG(info) << "prepare output";
#endif		
                for (int iref = 0, nrefs = fTref.size(); iref < nrefs; iref++) {
                    int tref = fTref[iref];
                    //////////////////////////////////////////////////
                    // create header message
                    int nw = sizeof(TimeFrame::Header)/sizeof(uint32_t);
                    outdata.resize(outdata.size() + nw);
                    //FairMQMessagePtr msgTFBHDR(NewMessage(sizeof(TimeFrame::Header)));
                    //auto tfbhdr = reinterpret_cast<struct TimeFrame::Header*>(msgTFBHDR->GetData());
                    auto tfbhdr = reinterpret_cast<struct TimeFrame::Header*>(&(outdata[outdata.size()-nw]));
                    int tfbhdridx = outdata.size()-nw;
                    *tfbhdr = *tfbheader;
#if 0		    
                    LOG(info) << "time frame header is ready for iref = " << iref;
#endif		    
                    //outParts.AddPart(std::move(msgTFBHDR));
                    totalSize = sizeof(TimeFrame::Header);
                    // int idx = outParts.Size() - 1;
                    for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
                        //////////////////////////////////////////////////
                        // create header message
                        nw = sizeof(SubTimeFrame::Header)/sizeof(uint32_t);
                        outdata.resize(outdata.size() + nw);
                        auto stfheader = reinterpret_cast<struct SubTimeFrame::Header*>(&(outdata[outdata.size()-nw]));
                        // FairMQMessagePtr msgSTFBHDR(NewMessage(sizeof(SubTimeFrame::Header)));
                        // auto stfheader = reinterpret_cast<struct SubTimeFrame::Header*>(msgSTFBHDR->GetData());
                        *stfheader = *stfs[iSrc].header;
                        stfheader->length = sizeof(SubTimeFrame::Header) + sizeof(uint64_t) * 2;
                        int stflength = sizeof(SubTimeFrame::Header) + sizeof(uint64_t) * 2;
                        std::vector<uint64_t> outtdc;
//                        LOG(info) << "sub time frame header is ready";
                        for (int itdc = std::max(tref - 250, 0), ntdc = std::min(nTimeBins,tref + 250); itdc < ntdc; ++itdc) {

                            if (fTDCIdx[itdc] == -1 || fTDCData[fTDCIdx[itdc]][iSrc].size() == 0) continue;
#if 0
                            LOG(info) << fTDCData[itdc][iSrc].size() << " found in iSrc = " << iSrc ;
#endif
                            for (int idata = 0, ndata = fTDCData[fTDCIdx[itdc]][iSrc].size(); idata < ndata; ++idata ) {
                                outtdc.emplace_back(fTDCData[fTDCIdx[itdc]][iSrc][idata]);
                            }
                        }
                        stfheader->length = sizeof(SubTimeFrame::Header) + sizeof(uint64_t) * outtdc.size();
                        // outParts.AddPart(std::move(msgSTFBHDR));
                        totalSize += stfheader->length;
                        outdata.resize(outdata.size()+outtdc.size()*2);
                        auto outdata64 = (uint64_t*)(&outdata[outdata.size() - outtdc.size()*2]);
                        if (outtdc.size() > 0) {
                            //////////////////////////////////////////////////
                            // create data message
                            // FairMQMessagePtr msgTDC(NewMessage(sizeof(uint64_t) * outtdc.size()));
                            for (uint32_t itdc = 0, ntdc = outtdc.size(); itdc < ntdc; ++itdc) {
                                // *(((uint64_t*)msgTDC->GetData()) + itdc) = outtdc[itdc];
                                outdata64[itdc] = outtdc[itdc];
                            }
                            //outParts.AddPart(std::move(msgTDC));
                        }
//                        LOG(info) << "tdc date is ready";

                    }
                    tfbhdr = reinterpret_cast<struct TimeFrame::Header*>(&(outdata[tfbhdridx]));
//                    int tfbhdridx = outdata.size()-nw;
                    tfbhdr->length = totalSize;
                }
#if 0		
                LOG(info) << "output part is prepared with size of " << outdataptr->size();;
#endif		
                FairMQMessagePtr msgall(MessageUtil::NewMessage(*this,std::move(outdataptr)));
                outParts.AddPart(std::move(msgall));
                sw_end = std::chrono::system_clock::now();
                elapse = std::chrono::duration_cast<std::chrono::microseconds>(sw_end - sw_start).count();
                //	std::cout << "#Output  Elapse: " << std::dec << elapse << " us" << std::endl;
            }

            //

            continue;
        }

        //      LOG(info) << "Data found with a size of " << part.GetSize()<< " in Part[" << iPart << "]";
        LOG(warn) << "Unexpected";
    }
#if 0
    LOG(info) << "sending the data";
    ////////////////////////////////////////////////////
    // (test) copy all the data
    ////////////////////////////////////////////////////
    unsigned int msg_size = inParts.Size();
    for (unsigned int ii = 0 ; ii < msg_size ; ii++) {
        FairMQMessagePtr msgCopy(fTransportFactory->CreateMessage());
        msgCopy->Copy(inParts.AtRef(ii));
        outParts.AddPart(std::move(msgCopy));
    }
#endif
    ////////////////////////////////////////////////////
    // Transfer the data to all of output channel
    ////////////////////////////////////////////////////
    auto poller = NewPoller(fOutputChannelName);
    while (!NewStatePending()) {
        auto direction = (fDirection++) % fNumDestination;
        poller->Poll(fPollTimeoutMS);
        if (poller->CheckOutput(fOutputChannelName, direction)) {
            if (Send(outParts, fOutputChannelName, direction) > 0) {
                // successfully sent
                break;
            } else {
                LOG(error) << "Failed to queue output-channel";
            }
        }
        if (fNumDestination==1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
#if 0    
    LOG(info) << "done";
#endif    
    return true;
}

void EventBuilder::PostRun()
{

}


////////////////////////////////////////////////////
// override runDevice
////////////////////////////////////////////////////

void addCustomOptions(bpo::options_description& options)
{
    using opt = EventBuilder::OptionKey;

    options.add_options()
    //("max-iterations", bpo::value<uint64_t>()->default_value(0),
    //"Maximum number of iterations of Run/ConditionalRun/OnData (0 - infinite)")

    (opt::InputChannelName.data(),
     bpo::value<std::string>()->default_value("in"),
     "Name of the input channel")
    (opt::OutputChannelName.data(),
     bpo::value<std::string>()->default_value("out"),
     "Name of the output channel")
    (opt::DQMChannelName.data(),
     bpo::value<std::string>()->default_value("dqm"),
     "Name of the data quality monitoring channel")
    (opt::DataSuppress.data(),
     bpo::value<std::string>()->default_value("true"),
     "Data suppression enable")
    //		(opt::RemoveHB.data(),
    //			bpo::value<std::string>()->default_value("false"),
    //			"Remove HB without hit")
    (opt::PollTimeout.data(),
     bpo::value<std::string>()->default_value("1"),
     "Timeout of polling (in msec)")
    (opt::SplitMethod.data(),
     bpo::value<std::string>()->default_value("1"),
     "STF split method")
    (opt::TrefID.data(),
     bpo::value<std::string>()->default_value("0xc0a802a8"),
     "FEM ID (hex) for time reference")
    (opt::TrefCH.data(),
     bpo::value<std::string>()->default_value("42"),
     "FEM Channel for time reference")
    ;
}


std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<EventBuilder>();
}
