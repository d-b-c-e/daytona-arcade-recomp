// Actual Controls + masked M2Board I/O, with no SDL library or guest program.
#include "app/scripted_input.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
extern unsigned fake_sdl_calls;
namespace rt::tgpgen {
void run(Tgp &, uint64_t) { std::abort(); } // R0 must never execute guest geometry code
extern const uint32_t program_crc32 = 0, program_words = 0;
}
static unsigned checks = 0;
void check(bool ok) { if (!ok) {std::fprintf(stderr,"scripted input assertion %u failed\n",checks+1);std::exit(1);} ++checks; }
template<class F> void refuses(F f) { bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed); }
const std::string session(32,'a');
std::string request(const std::string &body="10 20 accelerate\n") {return "session "+session+"\nexpires_ms 200000\nframes 6000\n"+body;}
std::string guard() {return "{\"session\":\""+session+"\",\"unix\":100,\"ok\":true,\"pending\":false,\"cleanup\":false,\"adopted\":true}";}
int main(int argc,char **argv) {
    if(argc==4 && std::string(argv[1])=="--watch-guard") {
        const auto start=std::chrono::steady_clock::now();uint64_t reads=0,errors=0;
        while(std::chrono::steady_clock::now()-start<std::chrono::seconds(3)) {
            try{app::InputGuard::parse(app::read_shared_bounded(argv[2],4096));++reads;}catch(const std::exception&){++errors;}
        }
        std::ofstream out(argv[3]);out<<"{\"reads\":"<<reads<<",\"errors\":"<<(errors?"[\"reader failure\"]":"[]")<<"}";
        return errors || !reads;
    }
    app::Controls c;c.set_defaults();app::Devices devices;
    std::array<bool,SDL_SCANCODE_COUNT> keys{};
    auto sample=[&]{return c.sample(keys.data(),devices);};
    auto in=sample();check(in.steer==128&&in.accel==32&&in.brake==32);
    keys[SDL_SCANCODE_RIGHT]=true;for(int i=0;i<10;++i)in=sample();check(in.steer==224);
    keys={};for(int i=0;i<10;++i)in=sample();check(in.steer==128);
    keys[SDL_SCANCODE_LEFT]=true;for(int i=0;i<10;++i)in=sample();check(in.steer==32);
    c.steer_invert=true;for(int i=0;i<20;++i)in=sample();check(in.steer==224);c.steer_invert=false;
    keys={};keys[SDL_SCANCODE_UP]=keys[SDL_SCANCODE_DOWN]=true;in=sample();check(in.accel==224&&in.brake==224);
    keys={};in=sample();check(in.accel==32&&in.brake==32);
    keys[SDL_SCANCODE_5]=keys[SDL_SCANCODE_RETURN]=true;in=sample();check((in.in0&0x11)==0);
    keys={};in=sample();check((in.in0&0x11)==0x11);
    c.gear=1;keys[SDL_SCANCODE_W]=true;in=sample();check(c.gear==2);for(int i=0;i<5;++i)sample();check(c.gear==2);
    keys={};sample();keys[SDL_SCANCODE_Q]=true;sample();check(c.gear==1);
    keys={};sample();keys[SDL_SCANCODE_2]=keys[SDL_SCANCODE_W]=true;in=sample();check(c.gear==2);check(((in.in1>>4)&7)==1);
    keys[SDL_SCANCODE_2]=false;sample();check(c.gear==2);keys={};sample();keys[SDL_SCANCODE_W]=true;sample();check(c.gear==3);
    check(fake_sdl_calls==0); // all real sampler paths above use only the private key array
    if(argc==2) {
        app::ScriptedInput full;full.load("session "+session+"\nexpires_ms 200000\n"+app::read_shared_bounded(argv[1],65536),session,6000,100000);
        app::Controls ordinary;ordinary.set_defaults();
        for(uint64_t frame=1;frame<=6000;++frame) {
            const auto local=full.keys(frame,ordinary);const auto observed=ordinary.sample(local.data(),devices);
            if(frame==1500||frame==3375)check(observed.steer==224);
            if(frame==3235)check(observed.steer==32);
            if(frame==3000)check(observed.accel==224&&ordinary.gear==1);
            if(frame==3300)check(ordinary.gear==2);
            if(frame==3550)check(ordinary.gear==3);
            if(frame==3650)check(ordinary.gear==2);
            if(frame==3950)check(observed.accel==32&&observed.brake==224);
            if(frame==4400)check(observed.accel==224&&ordinary.gear==3);
            if(frame==4600)check(ordinary.gear==4);
            if(frame==6000)check(observed.steer==128&&observed.accel==32&&observed.brake==32&&observed.in0==255);
        }
        check(fake_sdl_calls==0);
    }

    app::ScriptedInput program;program.load(request(),session,6000,100000);
    check(!program.keys(9,c)[SDL_SCANCODE_UP]&&program.keys(10,c)[SDL_SCANCODE_UP]&&!program.keys(21,c)[SDL_SCANCODE_UP]);
    for(auto body:{"0 2 start\n","20 10 start\n","1 6001 start\n","1 2 test\n","1 2 unknown\n","1 2 start extra\n","1 9 brake\n8 10 brake\n"})
        refuses([&]{app::ScriptedInput bad;bad.load(request(body),session,6000,100000);});
    refuses([&]{app::ScriptedInput bad;bad.load(request()+"session "+session+"\n",session,6000,100000);});
    refuses([&]{app::ScriptedInput bad;bad.load(request(),std::string(32,'b'),6000,100000);});
    refuses([&]{app::ScriptedInput bad;bad.load(request(),session,6000,200001);});
    refuses([&]{app::ScriptedInput bad;bad.load(std::string(65537,' '),session,6000,100000);});
    refuses([&]{app::ScriptedInput bad;bad.load(request()+char(0xff),session,6000,100000);});
    auto g=app::InputGuard::parse(guard());check(g.ok&&!g.pending&&g.adopted);
    refuses([&]{app::InputGuard::parse(guard()+"garbage");});
    auto duplicate=guard();duplicate.insert(duplicate.size()-1,",\"ok\":true");refuses([&]{app::InputGuard::parse(duplicate);});
    auto incomplete=guard();incomplete.erase(incomplete.find(",\"adopted\""));incomplete+='}';refuses([&]{app::InputGuard::parse(incomplete);});
    g.adopted=false;check(!program.admit(g,100000,true)&&!program.armed&&!program.revoked);
    g.adopted=true;check(program.admit(g,100000,true)&&program.armed);
    g.pending=true;check(!program.admit(g,100000,true)&&program.revoked);g.pending=false;check(!program.admit(g,100000,true));
    const auto first_reason=program.reason;program.revoke("later failure");check(program.reason==first_reason);
    for(int which=0;which<5;++which){app::ScriptedInput p;p.load(request(),session,6000,100000);auto f=app::InputGuard::parse(guard());
        if(which==0)f.session=std::string(32,'b');if(which==1)f.ok=false;if(which==2)f.cleanup=true;if(which==3)f.unix_seconds=97;if(which==4)f.unix_seconds=101;
        check(!p.admit(f,100000,true)&&p.revoked);}
    {app::ScriptedInput p;p.load(request(),session,6000,100000);check(!p.admit(app::InputGuard::parse(guard()),200000,true)&&p.revoked);}
    const auto file=std::filesystem::absolute("scripted-input-fixture.txt");check(!std::filesystem::exists(file));
    {std::ofstream out(file);out<<guard();}check(app::read_shared_bounded(file.string(),4096)==guard());
    refuses([&]{app::read_shared_bounded(file.string(),10);});std::filesystem::remove(file);
    app::ScriptedInput file_reader;file_reader.load(request(),session,6000,100000);file_reader.guard_path=file.string();
    check(!file_reader.tick(10,c,100000)[SDL_SCANCODE_UP]&&file_reader.revoked);
    {std::ofstream out(file);out<<guard();}check(!file_reader.tick(10,c,100000)[SDL_SCANCODE_UP]&&file_reader.revoked);std::filesystem::remove(file);

    rt::M2Board::Images images;images.program.resize(0x200000);images.main_data.resize(0x2000000);
    images.copro_tables.resize(0x40000);images.copro_data.resize(0x800000);images.polygons.resize(4);images.textures.resize(2);
    rt::M2Board board(std::move(images));auto &io=board.io();io.inputs={32,224,32,0xee,0xdf,0xff};
    board.write_byte(0x1c00040,1);board.read_byte(0x1c00000);check(io.observation.polls==0&&io.observation.reads[0]==0);
    io.observation.enabled=true;io.observation.offered_frame=17;board.write_byte(0x1c00040,1);
    check(io.observation.polls==1&&io.observation.latched_frame==17);
    check(board.read_byte(0x1c00000)==32);check(io.observation.reads[0]==1&&io.observation.reads[1]==0);
    check(board.read_word(0x1c00002)==224);check(io.observation.reads[1]==1);
    board.read_byte(0x1c00001);check(io.observation.reads[0]==1&&io.observation.reads[1]==1);
    board.read_dword(0x1c00000);check(io.observation.reads[0]==2&&io.observation.reads[1]==2);
    check(io.observation.values[0]==32&&io.observation.read_frames[0]==17);io.read(0);check(io.observation.reads[0]==2);
    const auto before=io.observation.reads;std::ostringstream trace;program.write_trace(trace,17,io.inputs,c,board);
    check(trace.str().find("\"frame\":17")!=std::string::npos&&before==io.observation.reads);check(fake_sdl_calls==0);
    std::printf("scripted input: %u assertions passed; zero SDL calls, no guest instructions\n",checks);
}
