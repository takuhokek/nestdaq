#include "AmQTdcData.h"
#include "AmQTdc.h"
#include <random>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <cstring>
#include <algorithm>
#include <time.h>

AmQTdc::AmQTdc()
    : mdebug(false) {

    nWordCount = 256;
    HBrate = 300;
    nseq = 0;
    hbframe_count = 0;
    spill_count = 0;

    TotalSize = 0;
    StackSize = 0;
}

void AmQTdc::Delete() {
    delete[] StackBuf;
    delete[] databuf;

}
void AmQTdc::Init() {

    StackBuf = new unsigned char[HBrate * (maxTdcHits*fnByte + maxNdelimByte)];
    databuf = new unsigned char[maxTdcHits*fnByte + maxNdelimByte];
    // bytes 128*4*8 + 32, for one event

    nseq = 0;
    hbframe_count = 0;
    spill_count = 0;

    TotalSize = 0;
    StackSize = 0;
    std::cout << "nseq        : " << nseq << std::endl;
    std::cout << "hbframe     : " << hbframe_count << std::endl;
    std::cout << "spill_count : " << spill_count << std::endl;
    std::cout << "TotalSize : "   << TotalSize << std::endl;
    std::cout << "StackSize : "   << StackSize << std::endl;
}

bool AmQTdc::LRcompare(const LRTdc::AmQTdcWord& a1, const LRTdc::AmQTdcWord& a2) {

    if(a1.tdc == a2.tdc) {
        if(a1.ch == a2.ch) {

            return a1.tot < a2.tot;
        } else {
            return a1.ch < a2.ch;
        }
    } else {
        return a1.tdc < a2.tdc;
    }

}

bool AmQTdc::HRcompare(const HRTdc::AmQTdcWord& a1, const HRTdc::AmQTdcWord& a2) {

    if(a1.tdc == a2.tdc) {
        if(a1.ch == a2.ch) {

            return a1.tot < a2.tot;
        } else {
            return a1.ch < a2.ch;
        }
    } else {
        return a1.tdc < a2.tdc;
    }

}

int AmQTdc::packet_generator(int type, unsigned char *sbuf) {

    int sendBufSize =  nWordCount * fnByte;

    if(nseq > (HBrate + 30) ) {
        nseq = 0;
    }
    int wcount = 0;

    if(type==1 || type==3) {
        wcount = LRgenerator(nseq, databuf);
    } else if(type==2) {
        wcount = HRgenerator(nseq, databuf);
    } else {
        std::cerr << "error: TDC type is wrong.. "<< std::endl;
        return 0;
    }

    nseq++;

    TotalSize = TotalSize + wcount*sizeof(char) * fnByte;

    if(TotalSize > sendBufSize || TotalSize == sendBufSize) {

        memcpy(&StackBuf[StackSize], &databuf[0], sendBufSize - StackSize);
        memcpy(&sbuf[0], &StackBuf[0], sendBufSize);

        if( mdebug ) {
            std::cout << " packet gene " << std::endl;
            for(int ia=0; ia<sendBufSize; ia++) {
                printf("%02x ", sbuf[ia]);
                if( ((ia+1)%8) == 0 ) {
                    printf("\n");
                }
            }
        }

        memcpy(&StackBuf[0], &databuf[sendBufSize - StackSize], TotalSize - sendBufSize);

        TotalSize = TotalSize - sendBufSize;
        StackSize = TotalSize;

        return sendBufSize; //byte

    } else if(TotalSize < sendBufSize && wcount!=0) {

        memcpy(&StackBuf[StackSize], &databuf[0], wcount*sizeof(char)*fnByte);
        StackSize = StackSize + wcount*sizeof(char)*fnByte;

        return -1;

    } else if(wcount == 0) { // spill off

        std::cout << "=====spill off====== from emulator" << std::endl;
        return -4;
    }

    return 0;
}

int AmQTdc::LRgenerator(unsigned int iseq, unsigned char* bufptr) {

    LRTdc::HeartBeatWord hb_delmt_;
    LRTdc::SpillWord     spill_delmt_;
    LRTdc::AmQTdcWord    tdc_data_[128*4];
    LRTdc::AmQHeader     header_;

    if( mdebug ) {
        std::cout << "start gene. "<< std::endl;
        std::cout << "iseq: " << iseq << std::endl;
    }
    if( iseq < HBrate ) {

        if( iseq == 0 ) {
            hbframe_count = 0;
        } else {
            hbframe_count++;
        }

    } else if( iseq >= HBrate ) {
        return 0;
    }

    if(sflag == 0) {
        std::cout << "iseq: " << iseq << std::endl;
        std::cout << "hbframe: " << hbframe_count << std::endl;
    }

    sflag = 1;

    hb_delmt_.hb_frame = hbframe_count;
    //  std::cout << "hbframe: " << hbframe_count << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());

    // # of PS trigger ch#0~ch#3
    std::uniform_int_distribution<int> gntrig(0,3);
    // # of TDC hits 20 ~ 40 : max 40*8 Byte = 320
    std::uniform_int_distribution<int> gntdc(20,40);

    // fired channel distributon
    std::uniform_int_distribution<int> gvch(4,127);
    // tdc tot distribution
    std::uniform_int_distribution<int> gvtot(40,100);

    unsigned int pre_=0;
    unsigned int count_=0;
    int ntdc = gntdc(gen); // 20~40

    if(mdebug)
        std::cout << "# of tdc: " << ntdc << std::endl;

    struct timespec ts;
    double t0=0.;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    //  t0 = (ts.tv_sec*1.) + (ts.tv_nsec/1000000000.);
    t0 = ts.tv_nsec;
    //  uint32_t tmp = (uint32_t)(t0%1000000)
    double tmp = ts.tv_nsec%1000000;// - 524287;
    if(tmp > 524287) {
        tmp = tmp - 524287;
    }
    int vtrig = (int)tmp;

    if(mdebug) {
        std::cout << "t0: "<< t0 << std::endl;
        std::cout << " tv_sec : " << ts.tv_sec
                  << " tv_nsec: " << ts.tv_nsec << std::endl;

        std::cout << "tmp: "<< tmp << std::endl;
        std::cout << "vtrig: " << vtrig << std::endl;
    }

    // set the trig channels 0~3
    for(int itrig=0; itrig < 4; itrig++) {

        int ich = gntrig(gen);
        tdc_data_[itrig].ch = ich;

        if( ich == 0 || ich == 1)
            tdc_data_[itrig].tdc = vtrig;
        else if( ich == 2 || ich == 3)
            tdc_data_[itrig].tdc = vtrig + 1;

        tdc_data_[itrig].tot = 80;
        //    std::cout << "trig tdc: "<< tdc_data_[itrig].tdc << std::endl;

    }

    // # of conicident channel
    std::uniform_int_distribution<int> gnCoin(8,10);
    int nCoin = gnCoin(gen);

    // TDC value
    std::uniform_int_distribution<int> gvtdc(1,524287);

    for(int itdc = 3; itdc < ntdc; itdc++) {

        /* generate TDC data packet */
        tdc_data_[itdc].ch  = gvch(gen);

        if( nCoin != 0 ) {
            std::uniform_int_distribution<int> gdiff(3,6);
            int diff = gdiff(gen);

            tdc_data_[itdc].tdc  = vtrig + diff;
            nCoin--;
        } else {
            tdc_data_[itdc].tdc = gvtdc(gen);
        }

        tdc_data_[itdc].tot = gvtot(gen);

        if(mdebug) {
            std::cout << "ch : " << tdc_data_[itdc].ch << std::endl;
            std::cout << "tot : " << tdc_data_[itdc].tot << std::endl;
            std::cout << "tdc : " << tdc_data_[itdc].tdc << std::endl;

            printf("tot:  %08x\n", tdc_data_[itdc].tot);
            printf("tdc:  %08x\n", tdc_data_[itdc].tdc);
        }

    }//ntdc

    std::sort(tdc_data_, tdc_data_ + ntdc, LRcompare);

    if(mdebug) std::cout << "sorting"<< std::endl;
    for(int itdc=0; itdc<ntdc; itdc++) {

        if( mdebug ) {
            std::cout << "ch : " << tdc_data_[itdc].ch << std::endl;
            std::cout << "tot : " << tdc_data_[itdc].tot << std::endl;
            std::cout << "tdc : " << tdc_data_[itdc].tdc << std::endl;

            printf("tot:  %08x\n", tdc_data_[itdc].tot);
            printf("tdc:  %08x\n", tdc_data_[itdc].tdc);
        }

        uint8_t word_buf[8] = {0};
        word_buf[7] |= ( header_.tdc & 0x3f ) << 2;
        word_buf[7] |= ( tdc_data_[itdc].ch >> 5 ) & 0x03;

        word_buf[6] |= ( tdc_data_[itdc].ch & 0x1f ) << 3 ;
        word_buf[6] |= ( tdc_data_[itdc].tot >> 5 ) & 0x07;

        word_buf[5] |= ( tdc_data_[itdc].tot & 0x1f ) << 3;
        word_buf[5] |= ( tdc_data_[itdc].tdc >> 16 ) & 0x07;

        word_buf[4] = ( tdc_data_[itdc].tdc >> 8  )  & 0xff;
        word_buf[3] = tdc_data_[itdc].tdc & 0xff;

        if( mdebug ) {
            std::cout << "word" << std::endl;
            for( int k=0; k<fnByte; k++)
                printf("%02x ", word_buf[7-k]);

            printf("\n");
        }

        memcpy(&bufptr[pre_], &word_buf[0], sizeof(char)*fnByte);
        pre_ = pre_ + fnByte;
        count_++;

    }


    /* generate heartbeat delimiter */
    uint8_t hb_buf[8] = {0};

    hb_delmt_.flag = 0;

    hb_buf[7] |= (header_.heartbeat & 0x3f) << 2;
    hb_buf[7] |= (hb_delmt_.flag >> 8) & 0x03;
    hb_buf[6] |=  hb_delmt_.flag  & 0xff;
    hb_buf[5] |=  spill_count & 0xff;
    hb_buf[4] |= (hb_delmt_.hb_frame >> 8) & 0xff;
    hb_buf[3] |=  hb_delmt_.hb_frame & 0xff;

    memcpy(&bufptr[pre_], &hb_buf[0], sizeof(char)*fnByte);
    pre_ = pre_ + fnByte;
    memcpy(&bufptr[pre_], &hb_buf[0], sizeof(char)*fnByte);
    pre_ = pre_ + fnByte;
    count_=count_ + 2;

    /* generate spill on/off delimiter */
    if(iseq == 0 || iseq == (HBrate-1) ) {
        uint8_t sp_buf[8] = {0};
        //    delmt_buf[8] = {0};

        spill_delmt_.hb_count = 32000; // 32K
        spill_delmt_.flag = 0;

        if(hb_delmt_.hb_frame == 0) {
            sp_buf[7] |= (header_.spillon & 0x3f) << 2;

        } else if( hb_delmt_.hb_frame == (HBrate-1) ) {
            sp_buf[7] |= (header_.spilloff & 0x3f) << 2;
        }

        sp_buf[7] |= (spill_delmt_.flag >> 8) & 0x03;
        sp_buf[6] |=  spill_delmt_.flag & 0xff;
        sp_buf[5] |=  spill_count & 0xff;
        sp_buf[4] |= (spill_delmt_.hb_count >> 8) & 0xff;
        sp_buf[3] |=  spill_delmt_.hb_count & 0xff;

        memcpy(&bufptr[pre_], &sp_buf[0], sizeof(char)*fnByte);
        pre_ = pre_ + fnByte;
        memcpy(&bufptr[pre_], &sp_buf[0], sizeof(char)*fnByte);
        pre_ = pre_ + fnByte;

        count_=count_ + 2;

        if(iseq == (HBrate-1)) {
            if( spill_count == 255 ) {
                // to avoid overflow 2^8=255
                spill_count = 0;
            } else {
                spill_count++;
            }
        }

    }

    if(mdebug) {
        std::cout << "in gene 64 bit word count: "<< count_ << std::endl;
        for(unsigned int i=0; i< count_ * 8; i++) {
            printf("%02x ", bufptr[i]);
            if( ((i+1)%8) == 0 ) {
                printf("\n");
            }
        }
    }

    //  delete[] bufptr;

    return count_;
}

int AmQTdc::HRgenerator(unsigned int iseq, unsigned char* bufptr) {

    HRTdc::HeartBeatWord hb_delmt_;
    HRTdc::SpillWord     spill_delmt_;
    HRTdc::AmQTdcWord    tdc_data_[128*4];
    HRTdc::AmQHeader     header_;

    if( mdebug ) {
        std::cout << "start HR gene. "<< std::endl;
        std::cout << "iseq: " << iseq << std::endl;
    }
    if( iseq < HBrate ) {

        if( iseq == 0 ) {
            hbframe_count = 0;

        } else {
            hbframe_count++;
        }

    } else if( iseq >= HBrate ) {
        return 0;
    }

    if(sflag == 0) {
        std::cout << "iseq: " << iseq << std::endl;
        std::cout << "hbframe: " << hbframe_count << std::endl;
    }

    sflag = 1;

    hb_delmt_.hb_frame = hbframe_count;

    std::random_device rd;
    std::mt19937 gen(rd());

    // # of PS trigger ch#0~ch#3
    std::uniform_int_distribution<int> gntrig(0,3);
    // # of TDC hits 20 ~ 40 : max 40*8 Byte = 320
    std::uniform_int_distribution<int> gntdc(20,40);

    // fired channel distributon
    std::uniform_int_distribution<int> gvch(4,127);
    // tdc tot distribution
    std::uniform_int_distribution<int> gvtot(40,100);

    unsigned int pre_=0;
    unsigned int count_=0;
    int ntdc = gntdc(gen); // 20~40

    if(mdebug)
        std::cout << "# of tdc: " << ntdc << std::endl;

    struct timespec ts;
    double t0=0.;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    //  t0 = (ts.tv_sec*1.) + (ts.tv_nsec/1000000000.);
    t0 = ts.tv_nsec;

    double tmp = ts.tv_nsec%100000;// - 65535;
    if(tmp > 65335) {
        tmp = tmp - 65335;
    }
    int vtrig = (int)tmp;

    if(mdebug) {
        std::cout << "t0: "<< t0 << std::endl;
        std::cout << " tv_sec : " << ts.tv_sec
                  << " tv_nsec: " << ts.tv_nsec << std::endl;

        std::cout << "tmp: "<< tmp << std::endl;
        std::cout << "vtrig: " << vtrig << std::endl;
    }

    // set the trig channels 0~3
    for(int itrig=0; itrig < 4; itrig++) {

        int ich = gntrig(gen);
        tdc_data_[itrig].ch = ich;

        if( ich == 0 || ich == 1)
            tdc_data_[itrig].tdc = vtrig;
        else if( ich == 2 || ich == 3)
            tdc_data_[itrig].tdc = vtrig + 1;

        tdc_data_[itrig].tot = 80;
        //    std::cout << "trig tdc: "<< tdc_data_[itrig].tdc << std::endl;

    }

    // # of conicident channel
    std::uniform_int_distribution<int> gnCoin(8,10);
    int nCoin = gnCoin(gen);

    // TDC value
    std::uniform_int_distribution<int> gvtdc(1,65335);

    for(int itdc = 3; itdc < ntdc; itdc++) {

        /* generate TDC data packet */
        tdc_data_[itdc].ch  = gvch(gen);

        if( nCoin != 0 ) {
            std::uniform_int_distribution<int> gdiff(3,6);
            int diff = gdiff(gen);

            tdc_data_[itdc].tdc  = vtrig + diff;
            nCoin--;
        } else {
            tdc_data_[itdc].tdc = gvtdc(gen);
        }

        tdc_data_[itdc].tot = gvtot(gen);

        if(mdebug) {
            std::cout << "ch : " << tdc_data_[itdc].ch << std::endl;
            std::cout << "tot : " << tdc_data_[itdc].tot << std::endl;
            std::cout << "tdc : " << tdc_data_[itdc].tdc << std::endl;

            printf("tot:  %08x\n", tdc_data_[itdc].tot);
            printf("tdc:  %08x\n", tdc_data_[itdc].tdc);
        }

    }//ntdc

    std::sort(tdc_data_, tdc_data_ + ntdc, HRcompare);

    if(mdebug) std::cout << "sorting"<< std::endl;
    for(int itdc=0; itdc<ntdc; itdc++) {

        if( mdebug ) {
            std::cout << "ch : " << tdc_data_[itdc].ch << std::endl;
            std::cout << "tot : " << tdc_data_[itdc].tot << std::endl;
            std::cout << "tdc : " << tdc_data_[itdc].tdc << std::endl;

            printf("tot:  %08x\n", tdc_data_[itdc].tot);
            printf("tdc:  %08x\n", tdc_data_[itdc].tdc);
        }

        uint8_t word_buf[8] = {0};
        //bit arrange
        word_buf[7] |= ( header_.tdc & 0x3f ) << 2;
        word_buf[7] |= ( tdc_data_[itdc].ch >> 5 ) & 0x03;

        word_buf[6] |= ( tdc_data_[itdc].ch & 0x1f ) << 3 ;
        word_buf[6] |= ( tdc_data_[itdc].tot >> 19 ) & 0x07;

        word_buf[5] |= ( tdc_data_[itdc].tot >> 11 ) & 0xff;
        word_buf[4] |= ( tdc_data_[itdc].tot >>  3 ) & 0xff;
        word_buf[3] |= ( tdc_data_[itdc].tot & 0x07) << 5;

        word_buf[3] |= ( tdc_data_[itdc].tdc >> 11 ) & 0x1f;
        word_buf[2] |= ( tdc_data_[itdc].tdc >>  3 ) & 0xff;
        word_buf[1] |= ( tdc_data_[itdc].tdc & 0x07) << 5;

        word_buf[1] |= ( tdc_data_[itdc].csemifine & 0x03) << 3;
        word_buf[1] |= ( tdc_data_[itdc].estimator >> 8) & 0x07;
        word_buf[0] |=   tdc_data_[itdc].estimator & 0xff;

        if( mdebug ) {
            std::cout << "word" << std::endl;
            for( int k=0; k<fnByte; k++)
                printf("%02x ", word_buf[7-k]);

            printf("\n");
        }

        memcpy(&bufptr[pre_], &word_buf[0], sizeof(char)*fnByte);
        pre_ = pre_ + fnByte;
        count_++;

    }


    /* generate heartbeat delimiter */
    uint8_t hb_buf[8] = {0};

    hb_delmt_.flag = 0;

    bool b_test = false;
    if( ((iseq == 100) || (iseq == 150) || (iseq == 200)) && b_test){
      
      hb_buf[7] |= (header_.heartbeat & 0x3f) << 2;
      hb_buf[7] |= (hb_delmt_.flag >> 8) & 0x03;
      hb_buf[6] |=  hb_delmt_.flag  & 0xff;
      hb_buf[5] |=  spill_count & 0xff;
      hb_buf[4] |= (hb_delmt_.hb_frame >> 8) & 0xff;
      hb_buf[3] |=  hb_delmt_.hb_frame & 0xff;

      memcpy(&bufptr[pre_], &hb_buf[0], sizeof(char)*fnByte);
      pre_ = pre_ + fnByte;
      count_ = count_+1;
      std::cout << "+++++++ Only one delimiter !!++++++"<< std::endl;            

    }else{

      hb_buf[7] |= (header_.heartbeat & 0x3f) << 2;
      hb_buf[7] |= (hb_delmt_.flag >> 8) & 0x03;
      hb_buf[6] |=  hb_delmt_.flag  & 0xff;
      hb_buf[5] |=  spill_count & 0xff;
      hb_buf[4] |= (hb_delmt_.hb_frame >> 8) & 0xff;
      hb_buf[3] |=  hb_delmt_.hb_frame & 0xff;

      memcpy(&bufptr[pre_], &hb_buf[0], sizeof(char)*fnByte);
      pre_ = pre_ + fnByte;
      count_ = count_+1;
      
      memcpy(&bufptr[pre_], &hb_buf[0], sizeof(char)*fnByte);
      pre_ = pre_ + fnByte;
      count_ = count_+1;
    }

    //    count_=count_ + 2;

    /* generate spill on/off delimiter */
    if(iseq == 0 || iseq == (HBrate-1) ) {

        for(int i=0; i<2; i++) {

            uint8_t sp_buf[8] = {0};
            //    delmt_buf[8] = {0};

            spill_delmt_.hb_count = 32000; // 32K
            spill_delmt_.flag = 0;

            if(hb_delmt_.hb_frame == 0) {
                sp_buf[7] |= (header_.spillon & 0x3f) << 2;

            } else if( hb_delmt_.hb_frame == (HBrate-1) ) {
                sp_buf[7] |= (header_.spilloff & 0x3f) << 2;
            }

            sp_buf[7] |= (spill_delmt_.flag >> 8) & 0x03;
            sp_buf[6] |=  spill_delmt_.flag & 0xff;
            sp_buf[5] |=  spill_count & 0xff;

            if( i==0 ) {
                sp_buf[4] |= (spill_delmt_.hb_count >> 8) & 0xff;
                sp_buf[3] |=  spill_delmt_.hb_count & 0xff;

            } else if( i==1 ) {
	        sp_buf[4] |= (hb_delmt_.hb_frame >> 8) & 0xff;
                sp_buf[3] |=  hb_delmt_.hb_frame & 0xff;
            }

            memcpy(&bufptr[pre_], &sp_buf[0], sizeof(char)*fnByte);
            pre_ = pre_ + fnByte;

            count_=count_ + 1;
        }

        if(iseq == (HBrate-1)) {
            if( spill_count == 255 ) {
                // to avoid overflow 2^8=255
                spill_count = 0;
            } else {
                spill_count++;
            }
            if( mdebug ) {
                std::cout << "n_spill: "<< spill_count << std::endl;
            }
        }

    }

    if(mdebug) {
    //    if(iseq == 100) {
        std::cout << "in gene 64 bit word count: "<< count_ << std::endl;
        for(unsigned int i=0; i< count_ * 8; i++) {
            printf("%02x ", bufptr[i]);
            if( ((i+1)%8) == 0 ) {
                printf("\n");
            }
        }
    }

    //  delete[] bufptr;

    return count_;
}
