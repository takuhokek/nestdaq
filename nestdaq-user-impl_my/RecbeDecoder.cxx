#include <functional>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <string>
#include <fstream>
#include <iostream>
#include <type_traits>
#include <array>
#include <TH1F.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TTree.h>

#include "FileSinkHeader.h"
#include "FileSinkTrailer.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"

template <typename T>
T getBitRange(const char* data, size_t startBit, size_t bitCount) {
    static_assert(std::is_integral<T>::value, "T must be an integral type"); 
    if (bitCount > sizeof(T) * 8) {
        throw std::out_of_range("bitCount exceeds the size of the output type");
    }

    T result = 0;
    for (size_t i = 0; i < bitCount; ++i) {
        size_t currentBit = startBit + i;            
        size_t byteIndex = currentBit / 8;          
        size_t bitOffset = currentBit % 8;          

        unsigned char bitValue = (data[byteIndex] >> bitOffset) & 0x01;
        result |= (static_cast<T>(bitValue) << i);  
    }
    return result;
}

unsigned short swapEndian(unsigned short val) {
    return (val >> 8) | (val << 8);
}

//_____________________________________________________________________________
int main(int argc, char** argv)
{
    namespace TF  = TimeFrame;
    namespace STF = SubTimeFrame;
    namespace FSH = FileSinkHeader;
    namespace FST = FileSinkTrailer;


    bool Endianfix  = true;

    std::string fInputFileName;
    std::string foutputFileName;
    std::string treename = "datatree";
    std::ifstream fInputFile;

    fInputFileName = argv[1];
    if(argc == 3){
        foutputFileName = std::string(argv[2]) + ".root";
    }else if(argc > 3){
        std::cout << "ERROR :引数" << std::endl;
        return -1;
    }else{
        foutputFileName = "/home/dugon/Works/nestdaq/outroot/sample_cosmicray_test.root"; 
    }

    TFile file(foutputFileName.c_str(), "RECREATE"); 
    TTree tree(treename.c_str(), "tree");
    


    std::cout << "Input : File name   : " << fInputFileName  << std::endl;
    std::cout << "Output: File name   : " << foutputFileName << std::endl;

    fInputFile.open(fInputFileName.data(), std::ios::binary);
    if (!fInputFile) {
        std::cout << " failed to open file = " << fInputFileName << std::endl;
        return -1;
    }

    // check FileSinkHeader
    uint64_t bufFS{0};
    fInputFile.read(reinterpret_cast<char*>(&bufFS), sizeof(bufFS));
    if (fInputFile.gcount() != sizeof(bufFS)) {
        std::cout << "Failed to read the first 8 bytes" << std::endl;
    }
    std::cout << "check FS header" << std::endl;
    if (bufFS == TimeFrame::MAGIC) {
        fInputFile.seekg(0, std::ios_base::beg);
        fInputFile.clear();
        std::cout << "No FS header" << std::endl;
    } else if (bufFS == FileSinkHeader::v0::MAGIC) { /* For new FileSinkHeader after 2023.06.15 */
        uint64_t hsize{0};
        fInputFile.read(reinterpret_cast<char*>(&hsize), sizeof(hsize));
        std::cout << "New FS header (Order: MAGIC + FS header size)" << std::endl;
        fInputFile.seekg(hsize - 2*sizeof(uint64_t), std::ios_base::cur);
    } else if (bufFS == FileSinkHeader::v1::MAGIC) { /* For new FileSinkHeader after 2023.06.15 */
        uint32_t hsize{0};
        fInputFile.read(reinterpret_cast<char*>(&hsize), sizeof(hsize));
        std::cout << "New FS header (New: MAGIC + FS(32) header size)" << std::endl;
        fInputFile.seekg(hsize - sizeof(uint64_t) - sizeof(uint32_t), std::ios_base::cur);
    } else { /* For old FileSinkHeader before 2023.06.15 */
        uint64_t magic{0};
        fInputFile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic == FileSinkHeader::MAGIC) {
            std::cout << "Old FS header (Order: FS header size + MAGIC)" << std::endl;
            fInputFile.seekg(bufFS - 2*sizeof(uint64_t), std::ios_base::cur);
        }
    }


    // Data構造体
    struct Data {
        std::array<unsigned char, 105> Packettype;
        std::array<unsigned char, 105> BoardID;
        std::array<unsigned short, 105> SentNumber;
        std::array<unsigned short, 105> Trigtime;
        std::array<unsigned short, 105> Datalength;
        std::array<unsigned short, 105> Upperbit;
        std::array<unsigned short, 105> Lowerbit;
        std::array<std::array<unsigned short, 32>, 5040> adc; // 2D配列の使用
        std::array<std::array<unsigned short, 32>, 5040> tdc; // 2D配列の使用
    } data;

    // TreeData構造体
    struct TreeData {
        std::array<unsigned char, 105> t_Packettype;
        std::array<unsigned char, 105> t_BoardID;
        std::array<unsigned short, 105> t_SentNumber;
        std::array<unsigned short, 105> t_Trigtime;
        std::array<unsigned short, 105> t_Datalength;
        std::array<unsigned short, 105> t_Upperbit;
        std::array<unsigned short, 105> t_Lowerbit;
        std::array<std::array<unsigned short, 32>, 5040> t_adc; // 2D配列の使用
        std::array<std::array<unsigned short, 32>, 5040> t_tdc; // 2D配列の使用
    } treedata;

    tree.Branch("Packettype", &treedata.t_Packettype, "Packettype[105]/b");
    tree.Branch("BoardID", &treedata.t_BoardID, "BoardID[105]/b");
    tree.Branch("SentNumber", &treedata.t_SentNumber, "SentNumber[105]/s");
    tree.Branch("Trigtime", &treedata.t_Trigtime, "Trigtime[105]/s");
    tree.Branch("Datalength", &treedata.t_Datalength, "Datalength[105]/s");
    tree.Branch("Upperbit", &treedata.t_Upperbit, "Upperbit[105]/s");
    tree.Branch("Lowerbit", &treedata.t_Lowerbit, "Lowerbit[105]/s");
    tree.Branch("adc", &treedata.t_adc, "adc[5040][32]/s");
    tree.Branch("tdc", &treedata.t_tdc, "tdc[5040][32]/s");


    size_t eventroop = 0;
    while(true){
        if (fInputFile.eof()) {
            std::cout << "Reached end of input file. stop RUNNING" << std::endl;
            break;  // ループを終了
        }
        //TFHEADER Part //////////////////////////////////////////////////////////////////////////////////////////
        std::vector<uint8_t> msgTFHeader(sizeof(TF::Header)); // TF::Header のサイズ分のバッファを確保
        uint64_t magic;
        char tempbuf[sizeof(struct Filter::Header)];
        fInputFile.read(reinterpret_cast<char*>(&magic), sizeof(uint64_t));
        if (! static_cast<bool>(fInputFile)) {
            std::cout << "Fail to read " << fInputFileName << std::endl;
        break;
        }
        if (magic == Filter::MAGIC) {
            fInputFile.read(tempbuf, sizeof(struct Filter::Header) - sizeof(uint64_t));
        } else if (magic == FSH::MAGIC) {
            FSH::Header fsh;
            fsh.magic = magic;
            char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            for (int i = 0 ; i < 8 ; i++) hmagic[i] = *(reinterpret_cast<char *>(&magic) + i);
            fInputFile.read(reinterpret_cast<char *>(&fsh) + sizeof(uint64_t),
                            sizeof(FSH::Header) - sizeof(uint64_t));
            if (fInputFile.eof()) break;
            std::cout << "FileSink Header magic : " << std::hex << fsh.magic
                    << "(" << hmagic << ")" << " size : " << std::dec << fsh.size << std::endl;
            return true;
        } else if (magic == FST::MAGIC) {
            FST::Trailer fst;
            fst.magic = magic;
            char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            for (int i = 0 ; i < 8 ; i++) hmagic[i] = *(reinterpret_cast<char *>(&magic) + i);
            fInputFile.read(reinterpret_cast<char *>(&fst) + sizeof(uint64_t),
                            sizeof(FST::Trailer) - sizeof(uint64_t));
            if (fInputFile.eof()) break;
            std::cout << "FileSink Trailer magic : " << std::hex << fst.magic
                    << "(" << hmagic << ")" << " size : " << std::dec << fst.size << std::endl;
        } else if (magic != TF::MAGIC) {
            char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            for (int i = 0 ; i < 8 ; i++) {
                char c = *(reinterpret_cast<char *>(&magic) + i);
                if  ((c >= 0x20) && (c < 0x7f)) {
                    hmagic[i] = c;
                } else {
                    hmagic[i] = '.';
                }
            }
            std::cout << "Unknown magic = " << std::hex << magic << "(" << hmagic << ")" << std::endl;
            break;
        }
        if (magic == TF::MAGIC) {
            if (msgTFHeader.size() < sizeof(uint64_t)) {
                std::cerr << "Buffer size is less than expected. Aborting." << std::endl;
                break; 
            }
            *reinterpret_cast<uint64_t*>(msgTFHeader.data()) = magic;
            fInputFile.read(reinterpret_cast<char*>(msgTFHeader.data()) + sizeof(uint64_t),
                            msgTFHeader.size() - sizeof(uint64_t));
            if (fInputFile.eof()) break;

            // std::cout << "TF Header : " << std::hex << magic << std::dec << std::endl;
        } else {
            fInputFile.read(reinterpret_cast<char*>(msgTFHeader.data()),msgTFHeader.size());
            if (fInputFile.eof()) break;
        }
        if (static_cast<size_t>(fInputFile.gcount()) < (msgTFHeader.size() - sizeof(uint64_t))) {
            std::cout << "No data read. request = " << msgTFHeader.size()
                    << " bytes. gcount = " << fInputFile.gcount() << std::endl;
            break;
        }
        auto tfHeader = reinterpret_cast<TF::Header*>(msgTFHeader.data());
        //TFHeader Part ////////////////////////////////////////////////////////////////////////////
        //STFheader Part///////////////////////////////////////////////////////////////////////////
        unsigned int adcNum = 0, tdcNum = 0, NumBuf = 0; 
        for (auto i=0u; i<tfHeader->numSource; ++i) {
            std::vector<char> msgSTFHeader(sizeof(STF::Header));
            // std::cout << bufBegin << std::endl;
            fInputFile.read(msgSTFHeader.data(), msgSTFHeader.size());
            //STFHeader Part End///////////////////////////////////////////////////////////
            // std::cout
            //         << "STF header: magic = 0x" << std::hex <<  stfHeader->magic
            //         << ", tf-id = " << std::dec << stfHeader->timeFrameId
            //         << "  bytes = " << std::dec <<stfHeader->length
            //         << ", n-msg = " << stfHeader->numMessages
            //         << ", sec = " << stfHeader->timeSec
            //         << ", usec = " << stfHeader->timeUSec
            // << std::endl;
            //Packing Part///////////////////////////////////////////////////////////////////
            unsigned char r_Packettype = 0, r_BoardID = 0;
            unsigned short r_SentNumber = 0, r_Trigtime = 0, r_Datalength = 0, r_Upperbit = 0, r_Lowerbit = 0;


            fInputFile.read(reinterpret_cast<char*>(&r_Packettype), 1);
            fInputFile.read(reinterpret_cast<char*>(&r_BoardID)   , 1);
            fInputFile.read(reinterpret_cast<char*>(&r_SentNumber), 2);
            fInputFile.read(reinterpret_cast<char*>(&r_Trigtime)  , 2);
            fInputFile.read(reinterpret_cast<char*>(&r_Datalength), 2);
            fInputFile.read(reinterpret_cast<char*>(&r_Upperbit)  , 2);
            fInputFile.read(reinterpret_cast<char*>(&r_Lowerbit)  , 2);

            data.Packettype[i]  = r_Packettype;
            data.BoardID[i]     = r_BoardID;
            data.SentNumber[i]  = (!Endianfix) ? r_SentNumber : swapEndian(r_SentNumber); 
            data.Trigtime[i]    = (!Endianfix) ? r_Trigtime   : swapEndian(r_Trigtime);
            data.Datalength[i]  = (!Endianfix) ? r_Datalength : swapEndian(r_Datalength);
            data.Upperbit[i]    = (!Endianfix) ? r_Upperbit   : swapEndian(r_Upperbit);
            data.Lowerbit[i]    = (!Endianfix) ? r_Lowerbit   : swapEndian(r_Lowerbit);
            // std::cout << "Type size of r_Datalength: " << swapEndian(r_Datalength) << std::endl;
            // std::cout << "Type size of data.SentNumber[" << i << "]: " << data.Datalength[i] << std::endl;
            // printf("\npacket: 0x%4X\n", Packettype);
            // printf("boardid: 0x%4X\n", BoardID);
            // printf("sentnum: 0x%4X\n", SentNumber);
            // printf("trigtime: 0x%4d\n", c_Trigtime);
            // printf("datalength: 0x%4X\n", Datalength);
            // printf("ubit: 0x%4X\n", Upperbit);
            // printf("lbit: 0x%4X\n", Lowerbit);
            unsigned int SampleclockNum = 0, AorTdcNum = 0, ChNum = 0;
            for(size_t k = 0; k < 6156; k+=2){
                // printf("k: 0d%4d\n", k);
                //Pakking Part////////////////////////////////////////////
                // std::cout << "DATA Adress check : " << k << std::endl;
                if (ChNum == 48){
                    AorTdcNum++;
                    ChNum = 0;
                }
                if (AorTdcNum == 2){
                    SampleclockNum ++;
                    AorTdcNum = 0;
                }
                if (SampleclockNum == 32){
                    NumBuf += 48;
                    break;
                }
                unsigned short main_data = 0;
                adcNum = ChNum + NumBuf;
                tdcNum = ChNum + NumBuf;
                fInputFile.read(reinterpret_cast<char*>(&main_data), 2);
                unsigned short c_main_data = (!Endianfix) ? main_data : swapEndian(main_data);
                // std::cout << "CH :" << ChNum << "    adcNum :" << adcNum << "    main :" << c_main_data << std::endl;

                if(AorTdcNum == 1){
                    data.tdc[tdcNum][SampleclockNum] = getBitRange<unsigned short>(reinterpret_cast<const char*>(&c_main_data), 0, 14);
                    // std::cout << data.tdc[adcNum][SampleclockNum] << std::endl;
                }else{
                    data.adc[adcNum][SampleclockNum] = c_main_data;
                    // std::cout << "CH :" << ChNum << "    adcNum :" << adcNum << "    Data :" << data.adc[adcNum][SampleclockNum] << std::endl;
                }
                ChNum++;
            }
        }

        treedata.t_Packettype = data.Packettype;
        treedata.t_BoardID    = data.BoardID;
        treedata.t_SentNumber = data.SentNumber;
        treedata.t_Trigtime   = data.Trigtime;
        treedata.t_Datalength = data.Datalength;
        treedata.t_Upperbit   = data.Upperbit;
        treedata.t_Lowerbit   = data.Lowerbit;
        treedata.t_adc        = data.adc;
        treedata.t_tdc        = data.tdc;
        // std::cout  << data.adc[500][30] << std::endl;
        tree.Fill();
        std::cout << "FILL :"  << eventroop << std::endl;
        eventroop++;
    }

    //End Part //////////////////////////////////////////////////////////////////////////////////
    if (fInputFile.is_open()) {
        std::cout << "close input file" << std::endl;
        fInputFile.close();
        fInputFile.clear();
    }
    tree.Write();
    file.Close();
    std::cout << "close root file" << std::endl;

    return 0;
}
