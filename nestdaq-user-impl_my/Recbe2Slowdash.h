

#ifndef Example_Recbe2Slowdash_h
#define Example_Recbe2Slowdash_h

#include <string>
#include <string_view>
#include <iostream>

#include "uhbook.cxx"
#include <iostream>
#include <string>
#include <vector>

#if __has_include(<fairmq/Device.h>)
#include <fairmq/Device.h> // since v1.4.34
#else
#include <fairmq/FairMQDevice.h>
#endif

#include <vector>
#include <string>
#include "uhbook.cxx"
#include "RedisDataStore.h"
#include "Slowdashify.h"



constexpr size_t Sampleclock = 32, AorTdc = 2, Ch = 48;
    using Data_array = std::array<std::array<std::array<unsigned short, Ch>, AorTdc>, Sampleclock>;
    using Hit_array = std::array<bool, Ch>;

// UHbookヒストグラム作成: 横軸をチャンネル、縦軸をカウントに設定

std::vector<int> IDcheck = {};
static RedisDataStore* gDB = nullptr;
const std::string hist_name  = "Hitmap";
const std::string hist_name2 = "Hitmap_each";
const std::string breakCh_name    = "Break_Ch";
const std::string breakNum_name   = "Break_Num";

// バッファを用意
std::vector<std::pair<int, size_t>> buffer;
std::vector<std::pair<int, size_t>> breakbuf;
int breakNum = 0;

const char maxID = 106;
UH2Book uhhist(hist_name.c_str(), maxID, 0.0, static_cast<double>(maxID), Ch, 0.0, static_cast<double>(Ch));
UH2Book uhhist2(hist_name2.c_str(), maxID, 0.0, static_cast<double>(maxID), Ch, 0.0, static_cast<double>(Ch));
UH2Book breakhist(breakCh_name.c_str(), maxID, 0.0, static_cast<double>(maxID), Ch, 0.0, static_cast<double>(Ch));

void Initialize(const std::string& serverUri) {
    if (!gDB) { // すでに初期化されていない場合のみ初期化
        gDB = new RedisDataStore(serverUri);
    }
}

void Finalize() {
    if (gDB) {
        delete gDB;
        gDB = nullptr;
    }
}

void ResetUHhist2() {
    uhhist2 = UH2Book(hist_name2.c_str(), maxID, 0.0, static_cast<double>(maxID), Ch, 0.0, static_cast<double>(Ch));
}
void Resetbreakhist() {
    breakhist = UH2Book(breakCh_name.c_str(), maxID, 0.0, static_cast<double>(maxID), Ch, 0.0, static_cast<double>(Ch));
}

void SaveDatatoBuf(const Hit_array& harray, const char BoardID) 
{
    const size_t maxCh = harray.size(); // チャンネル数
    int boardIdInt = static_cast<int>(BoardID);
    if (boardIdInt >= maxID) {
        std::cerr << "ERROR: Invalid BoardID: " << boardIdInt << std::endl;
    }
    // 有効なデータをバッファに追加
    for (size_t i = 0; i < maxCh; ++i) {
        if (harray[i] > 0) {
            buffer.emplace_back(boardIdInt, i); // バッファに BoardID とチャンネル番号を追加
        }
    }
    return;
}

void SaveDatatoBuf2(const Data_array& array, const Hit_array& harray, const char BoardID) 
{
    const size_t maxCh = harray.size(); // チャンネル数
    int boardIdInt = static_cast<int>(BoardID);
    if (boardIdInt >= maxID) {
        std::cerr << "ERROR: Invalid BoardID: " << boardIdInt << std::endl;
    }
    // hitデータをバッファに追加
    for (size_t i = 0; i < maxCh; ++i) {
        if (harray[i] > 0) {
            buffer.emplace_back(boardIdInt, i); // バッファに BoardID とチャンネル番号を追加
        }
    }
    // breakデータをバッファに追加
    for (size_t i = 0; i < maxCh; ++i) {
        const unsigned short* dataStart = &array[0][0][i];
        const size_t dataSize = Sampleclock * sizeof(unsigned short);
        
        // サンプル全体が 0 の場合をチェック
        if (std::memcmp(dataStart, std::vector<unsigned short>(Sampleclock, 0).data(), dataSize) == 0) {
            breakNum += 1;
            breakbuf.emplace_back(boardIdInt, i);
        }
    }
    return;
}
void writeredis(){//hitmap1
    // バッファ内容をヒストグラムに反映
    for (const auto& entry : buffer) {
        uhhist.Fill(entry.first, entry.second); // BoardIDを横軸、チャンネルを縦軸として追加
    }
    // データベースに書き込む
    if (gDB) {
        gDB->write(hist_name.c_str(), Slowdashify(uhhist));
    } else {
        std::cerr << "Error: gDB is not initialized." << std::endl;
    }
    buffer.clear();
    return;
}

void writeredis2(){//hitmap2
    ResetUHhist2();
    // バッファ内容をヒストグラムに反映
    for (const auto& entry : buffer) {
        uhhist2.Fill(entry.first, entry.second); // BoardIDを横軸、チャンネルを縦軸として追加
    }
    // データベースに書き込む
    if (gDB) {
        gDB->write(hist_name2.c_str(), Slowdashify(uhhist2));
        std::cout << "writed to redis" << hist_name2.c_str() << std::endl;
    } else {
        std::cerr << "Error: gDB is not initialized." << std::endl;
    }
    writeredis();
    // buffer.clear();
    return;
}

void writeredis3(){
    // Resetbreakhist();
    // バッファ内容をヒストグラムに反映
    for (const auto& entry : breakbuf) {
        breakhist.Fill(entry.first, entry.second); // BoardIDを横軸、チャンネルを縦軸として追加
    }
    // データベースに書き込む
    if (gDB) {
        gDB->write(breakCh_name.c_str(), Slowdashify(breakhist));
        
    } else {
        std::cerr << "Error: gDB is not initialized." << std::endl;
    }
    writeredis2();
    return;
}

unsigned short swapEndian(unsigned short val) {
    return (val >> 8) | (val << 8);
}

class Recbe2Slowdash : public FairMQDevice {
public:

    struct OptionKey {
        static constexpr std::string_view InputChannelName{"in"};
        static constexpr std::string_view Multipart{"multipart"};
    };

    Recbe2Slowdash() = default;
    Recbe2Slowdash(const Recbe2Slowdash&) = delete;
    Recbe2Slowdash &operator=(const Recbe2Slowdash&) = delete;
    ~Recbe2Slowdash() = default;

private:
    bool HandleData(FairMQMessagePtr &msg, int index);
    bool HandleMultipartData(FairMQParts &msgParts, int index);
    void Init() override;
    void InitTask() override;
    void PostRun() override;
    void PreRun() override;

    std::string fInputChannelName;
    uint64_t fNumMessages{0};
    uint64_t TotalLength(const FairMQParts& parts);

    std::ofstream fstr;
    // std::vector<std::string> idvec = {};
};

#endif
