#include "ss2_local_multiplayer.h"
#include "multiplayer_launch.h"
#include "multiplayer_session.h"
#include "multiplayer_pacing.h"
#include "gba_bios.h"
#include "online/storage_worker.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <windows.h>

namespace ss2 {
namespace {
struct Surface {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* texture=nullptr;
    SDL_GameController* controller=nullptr;
    SDL_Joystick* joystick=nullptr;
    unsigned width=0;
    SDL_JoystickGUID guid{};
    bool guid_selected=false;
    ~Surface() {
        if (controller) SDL_GameControllerClose(controller);
        else if (joystick) SDL_JoystickClose(joystick);
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
    }
};
SDL_Rect cell(SDL_Rect r,int count,int seat,int layout) {
    if (count==1) return r;
    if (count==2 || layout==1 || layout==2) {
        const bool columns=layout==1;
        if (columns) { int x0=r.w*seat/count,x1=r.w*(seat+1)/count; return {r.x+x0,r.y,x1-x0,r.h}; }
        int y0=r.h*seat/count,y1=r.h*(seat+1)/count; return {r.x,r.y+y0,r.w,y1-y0};
    }
    int x=seat%2,y=seat/2;
    int x0=r.w*x/2,x1=r.w*(x+1)/2,y0=r.h*y/2,y1=r.h*(y+1)/2;
    if (count==3 && seat==2) {x0=0; x1=r.w;}
    return {r.x+x0,r.y+y0,x1-x0,y1-y0};
}
std::uint16_t keys(Surface& s,int requested,int deadzone,const std::array<SDL_JoystickID,4>& claimed) {
    if (s.joystick && !SDL_JoystickGetAttached(s.joystick)) {
        if (s.controller) SDL_GameControllerClose(s.controller);
        else SDL_JoystickClose(s.joystick);
        s.controller=nullptr; s.joystick=nullptr;
    }
    if (!s.joystick && requested>=0 && (s.guid_selected || requested<SDL_NumJoysticks())) {
        if(s.guid_selected) {
            requested=-1;
            for(int d=0;d<SDL_NumJoysticks();++d) {
                auto guid=SDL_JoystickGetDeviceGUID(d); auto id=SDL_JoystickGetDeviceInstanceID(d);
                if(std::memcmp(&guid,&s.guid,sizeof(guid))==0 && std::find(claimed.begin(),claimed.end(),id)==claimed.end()) {requested=d;break;}
            }
        }
        if(requested>=0 && std::find(claimed.begin(),claimed.end(),SDL_JoystickGetDeviceInstanceID(requested))==claimed.end()) {
        if (SDL_IsGameController(requested)) {
            s.controller=SDL_GameControllerOpen(requested);
            if (s.controller) s.joystick=SDL_GameControllerGetJoystick(s.controller);
        } else s.joystick=SDL_JoystickOpen(requested);
        if(s.joystick) {s.guid=SDL_JoystickGetGUID(s.joystick);s.guid_selected=true;}
        }
    }
    std::uint16_t result=0;
    if (s.controller) {
        constexpr SDL_GameControllerButton buttons[]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,
            SDL_CONTROLLER_BUTTON_BACK,SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
            SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_LEFTSHOULDER};
        for (unsigned b=0;b<10;++b) if(SDL_GameControllerGetButton(s.controller,buttons[b])) result|=1u<<b;
        auto x=SDL_GameControllerGetAxis(s.controller,SDL_CONTROLLER_AXIS_LEFTX);
        auto y=SDL_GameControllerGetAxis(s.controller,SDL_CONTROLLER_AXIS_LEFTY);
        if(x>deadzone) result|=16; if(x<-deadzone) result|=32;
        if(y>deadzone) result|=128; if(y<-deadzone) result|=64;
    } else if (s.joystick) {
        constexpr int buttons[]={0,1,8,9,5,4};
        constexpr unsigned bits[]={0,1,2,3,8,9};
        for(unsigned b=0;b<6;++b) if(buttons[b]<SDL_JoystickNumButtons(s.joystick) && SDL_JoystickGetButton(s.joystick,buttons[b])) result|=1u<<bits[b];
        if(SDL_JoystickNumAxes(s.joystick)>=2) {
            auto x=SDL_JoystickGetAxis(s.joystick,0),y=SDL_JoystickGetAxis(s.joystick,1);
            if(x>deadzone) result|=16; if(x<-deadzone) result|=32;
            if(y>deadzone) result|=128; if(y<-deadzone) result|=64;
        }
        if(SDL_JoystickNumHats(s.joystick)) {
            auto hat=SDL_JoystickGetHat(s.joystick,0);
            if(hat&SDL_HAT_RIGHT) result|=16; if(hat&SDL_HAT_LEFT) result|=32;
            if(hat&SDL_HAT_UP) result|=64; if(hat&SDL_HAT_DOWN) result|=128;
        }
    }
    if(SDL_GetKeyboardFocus()==s.window) {
        constexpr SDL_Scancode bindings[]={SDL_SCANCODE_X,SDL_SCANCODE_Z,SDL_SCANCODE_RSHIFT,SDL_SCANCODE_RETURN,
            SDL_SCANCODE_RIGHT,SDL_SCANCODE_LEFT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_V,SDL_SCANCODE_C};
        auto keyboard=SDL_GetKeyboardState(nullptr);
        for(unsigned b=0;b<10;++b) if(keyboard[bindings[b]]) result|=1u<<b;
    }
    return result;
}
void write_save(const std::filesystem::path& path,const std::vector<std::uint8_t>& bytes) {
    auto temporary=path; temporary+=L".tmp";
    { std::ofstream f(temporary,std::ios::binary|std::ios::trunc);
      if(!f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()) || !f.flush())
          throw std::runtime_error("cannot write local player save"); }
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot replace local player save");
}
}
int run_local_multiplayer(const gbarecomp::GbaNetplayLaunch& launch,const gbarecomp::GbaNetplayBoot& boot) {
    using namespace gbarecomp;
    auto options=local_multiplayer_options;
    if(options.players<1 || options.players>4 || !boot.bios || !boot.rom) throw std::runtime_error("invalid local multiplayer launch");
    SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN,"1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,"0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_JOYSTICK)) throw std::runtime_error(SDL_GetError());
    struct Quit { ~Quit(){SDL_Quit();} } quit;
    const bool testing=boot.finish_tick && std::getenv("SS2_LOCAL_TEST");
    struct VirtualPads {
        std::vector<SDL_Joystick*> handles;
        ~VirtualPads() {
            for(auto pad:handles) SDL_JoystickClose(pad);
            for(int d=SDL_NumJoysticks()-1;d>=0;--d)
                if(SDL_JoystickIsVirtual(d)) SDL_JoystickDetachVirtual(d);
        }
    } virtual_pads;
    if(testing && std::getenv("SS2_LOCAL_VIRTUAL_PADS")) {
        for(int i=0;i<options.players;++i) {
            SDL_VirtualJoystickDesc description{};
            description.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
            description.type=SDL_JOYSTICK_TYPE_UNKNOWN;
            description.naxes=2; description.nbuttons=10; description.nhats=1;
            description.vendor_id=0xfefe; description.product_id=0x100+i;
            description.name="SS2 isolated test controller";
            int d=SDL_JoystickAttachVirtualEx(&description);
            if(d<0) throw std::runtime_error(SDL_GetError());
            options.controllers[i]=d;
            if(i%2==0) {
                char guid[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(d),guid,sizeof(guid));
                std::string mapping=std::string(guid)+",Local test pad,a:b0,b:b1,back:b8,start:b9,leftshoulder:b4,rightshoulder:b5,leftx:a0,lefty:a1,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,";
                if(SDL_GameControllerAddMapping(mapping.c_str())<0) throw std::runtime_error(SDL_GetError());
            }
            auto pad=SDL_JoystickOpen(d);
            if(!pad) throw std::runtime_error(SDL_GetError());
            virtual_pads.handles.push_back(pad);
            // Distinct directions exercise routing without advancing menus.
            SDL_JoystickSetVirtualHat(pad,0,static_cast<Uint8>(1u<<i));
        }
    }
    SDL_Rect bounds{0,0,1280,720};
    int display=options.display<0 ? (SDL_GetNumVideoDisplays()>1 ? 1:0) : options.display;
    if(display>=SDL_GetNumVideoDisplays() || SDL_GetDisplayUsableBounds(display,&bounds)) throw std::runtime_error("invalid local multiplayer display");
    GbaSessionConfig config; config.links={{GbaLinkMedium::Cable,{}}};
    // The shared cable backend requires two endpoints. A one-window session
    // keeps its unused endpoint halted; no second player or window is created.
    const unsigned machines=std::max(2,options.players);
    for(unsigned i=0;i<machines;++i) {
        config.machines.push_back({i,launch.program_id,boot.expected_rom_sha1});
        config.links[0].machines.push_back(i); config.input_machines.push_back(i);
    }
    GbaMultiplayerSession simulation(std::move(config)); simulation.set_native_slices(true);
    std::array<std::filesystem::path,4> saves;
    auto root=boot.config_directory/"saves/local";
    if(testing) if(auto p=std::getenv("SS2_LOCAL_DIRECTORY")) root=p;
    for(unsigned i=0;i<machines;++i) {
        auto& machine=simulation.machine(i);
        machine.bus.set_bios(boot.bios); machine.bus.set_rom(boot.rom->data(),boot.rom->size());
        machine.bus.save().configure_flash(65536); machine.ppu.set_rasterization_enabled(false);
        if(launch.setup_instance) launch.setup_instance(machine);
        if(i>=static_cast<unsigned>(options.players)) {machine.bus.io().write8(0x301,0); continue;}
        saves[i]=root/("player-"+std::to_string(i+1))/"shining_soul_2.sav";
        std::filesystem::create_directories(saves[i].parent_path());
        if(std::filesystem::exists(saves[i])) {
            std::ifstream f(saves[i],std::ios::binary); std::vector<std::uint8_t> bytes(65536);
            if(!f.read(reinterpret_cast<char*>(bytes.data()),bytes.size()) || f.peek()!=std::char_traits<char>::eof() ||
                !machine.bus.save().load_flash_bytes(bytes.data(),bytes.size())) throw std::runtime_error("invalid local player save");
            machine.bus.save().clear_dirty();
        }
    }
    std::array<std::unique_ptr<Surface>,4> surfaces;
    std::array<std::unique_ptr<GbaNetplayPresentation>,4> views;
    for(int i=0;i<options.players;++i) {
        auto r=cell(bounds,options.players,i,options.layout);
        auto s=std::make_unique<Surface>();
        auto title="ShiningSoul2Recomp - Local - Jugador "+std::to_string(i+1);
        s->window=SDL_CreateWindow(title.c_str(),r.x,r.y,r.w,r.h,SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIDDEN);
        if(!s->window) throw std::runtime_error(SDL_GetError());
        s->renderer=SDL_CreateRenderer(s->window,-1,SDL_RENDERER_ACCELERATED);
        if(!s->renderer) s->renderer=SDL_CreateRenderer(s->window,-1,SDL_RENDERER_SOFTWARE);
        if(!s->renderer) throw std::runtime_error(SDL_GetError());
        SDL_RenderSetIntegerScale(s->renderer,options.integer_scale?SDL_TRUE:SDL_FALSE);
        views[i]=std::make_unique<GbaNetplayPresentation>(launch.view_policy,boot.affine_filter);
        simulation.machine(i).ppu.set_presentation_observer(views[i].get());
        if(!testing) SDL_ShowWindow(s->window);
        std::printf("local window seat=%d display=%d rect=%d,%d,%d,%d controller=%d save=%s\n",i,display,r.x,r.y,r.w,r.h,options.controllers[i],saves[i].string().c_str());
        surfaces[i]=std::move(s);
    }
    struct Detach { GbaMultiplayerSession& sim; unsigned count; ~Detach(){for(unsigned i=0;i<count;++i) sim.machine(i).ppu.set_presentation_observer(nullptr);} } detach{simulation,static_cast<unsigned>(options.players)};
    SDL_AudioDeviceID device=0; SDL_AudioSpec obtained{};
    if(boot.volume>0 && !testing) {SDL_AudioSpec desired{}; desired.freq=65536; desired.format=AUDIO_S16SYS; desired.channels=1; desired.samples=1024;
        device=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,SDL_AUDIO_ALLOW_FREQUENCY_CHANGE); if(device) SDL_PauseAudioDevice(device,0);}
    struct Audio { SDL_AudioDeviceID& id; SDL_AudioStream* stream=nullptr; ~Audio(){if(stream)SDL_FreeAudioStream(stream);if(id)SDL_CloseAudioDevice(id);} } audio{device};
    int source_rate=0;
    online::StorageWorker storage;
    GbaNetplayPacer pacer;
    std::array<std::uint16_t,4> buttons{};
    bool running=true; unsigned tick=0;
    auto persist=[&](bool force) {for(int i=0;i<options.players;++i) {auto& save=simulation.machine(i).bus.save();
        if(force || save.dirty()) {auto bytes=save.flash_bytes(); auto count=bytes.size();
            storage.submit(count,[path=saves[i],bytes=std::move(bytes)]{write_save(path,bytes);}); save.clear_dirty();}}};
    while(running && (!boot.finish_tick || tick<boot.finish_tick)) {
        storage.check(); SDL_Event event;
        while(SDL_PollEvent(&event)) if(event.type==SDL_QUIT || (event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_CLOSE) ||
            (event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_q && (event.key.keysym.mod&KMOD_CTRL))) running=false;
        if(!running) break;
        auto now=GbaNetplayPacer::Clock::now(); if(!pacer.ready(now)) {pacer.idle(); continue;}
        for(int i=0;i<options.players;++i) {
            auto& s=*surfaces[i]; int w=0,h=0; SDL_GetRendererOutputSize(s.renderer,&w,&h);
            auto width=gba_netplay_view_width(GbaNetplayView::Adaptive,w,h,launch.view_policy.max_width);
            views[i]->request_width(width);
            std::array<SDL_JoystickID,4> claimed; claimed.fill(-1);
            for(int j=0;j<options.players;++j) if(j!=i && surfaces[j]->joystick && SDL_JoystickGetAttached(surfaces[j]->joystick))
                claimed[j]=SDL_JoystickInstanceID(surfaces[j]->joystick);
            buttons[i]=keys(s,options.controllers[i],options.deadzone,claimed);
            if(testing && tick==1) std::printf("local input seat=%d mask=%u mapped=%d\n",i,buttons[i],s.controller?1:0);
        }
        simulation.run_frame({buttons.data(),machines}); ++tick; pacer.forwarded(now);
        for(int i=0;i<options.players;++i) {
            auto& s=*surfaces[i]; auto width=views[i]->width(); auto pixels=views[i]->pixels(); if(!pixels) continue;
            if(s.width!=width) {if(s.texture)SDL_DestroyTexture(s.texture); s.width=width;
                s.texture=SDL_CreateTexture(s.renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,width,160);
                if(!s.texture)throw std::runtime_error(SDL_GetError()); SDL_RenderSetLogicalSize(s.renderer,width,160);
                if(testing) std::printf("local view seat=%d width=%u height=160 raw=1\n",i,width);}
            SDL_UpdateTexture(s.texture,nullptr,pixels,width*3); SDL_SetRenderDrawColor(s.renderer,0,0,0,255);
            SDL_RenderClear(s.renderer); SDL_RenderCopy(s.renderer,s.texture,nullptr,nullptr); SDL_RenderPresent(s.renderer);
        }
        auto& sound=simulation.machine(std::clamp(options.audio_player,0,options.players-1)).bus.audio();
        if(device) {
            if(source_rate!=static_cast<int>(sound.sample_rate())) {if(audio.stream)SDL_FreeAudioStream(audio.stream); source_rate=sound.sample_rate();
                audio.stream=SDL_NewAudioStream(AUDIO_S16SYS,1,source_rate,obtained.format,obtained.channels,obtained.freq);}
            std::array<std::int16_t,4096> samples{};
            while(auto n=sound.drain_samples(samples.data(),samples.size())) {for(std::size_t k=0;k<n;++k)samples[k]=samples[k]*boot.volume/100;
                if(audio.stream)SDL_AudioStreamPut(audio.stream,samples.data(),n*2);}
            if(audio.stream) while(SDL_AudioStreamAvailable(audio.stream)>0) {int n=SDL_AudioStreamGet(audio.stream,samples.data(),samples.size()*2);
                if(n<=0)break; if(SDL_GetQueuedAudioSize(device)>static_cast<unsigned>(obtained.freq/2))SDL_ClearQueuedAudio(device); SDL_QueueAudio(device,samples.data(),n);}
        }
        simulation.discard_audio_output();
        if(tick%300==0)persist(false);
    }
    persist(true); storage.flush();
    if(testing) {for(int i=0;i<options.players;++i) {auto& s=*surfaces[i]; int w,h; SDL_GetRendererOutputSize(s.renderer,&w,&h);
        auto screenshot=SDL_CreateRGBSurfaceWithFormat(0,w,h,24,SDL_PIXELFORMAT_RGB24);
        if(screenshot) {SDL_RenderReadPixels(s.renderer,nullptr,screenshot->format->format,screenshot->pixels,screenshot->pitch);
            SDL_SaveBMP(screenshot,(root/("player-"+std::to_string(i+1)+".bmp")).string().c_str()); SDL_FreeSurface(screenshot);}}}
    std::printf("local complete players=%d frames=%u state_hash=%08x\n",options.players,tick,simulation.state_hash());
    return 0;
}
}
