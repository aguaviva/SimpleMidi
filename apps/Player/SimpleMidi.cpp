// xmidi.cpp : Defines the entry point for the console application.
//
#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"
#include "midi.h"
#include "inst-piano.h"
#include "misc.h"
#include <thread>

Instrument *GetMT32();

const uint32_t sample_rate = 48000;

size_t total_rendered_samples = 0;
bool bExit = true;

int32_t play_callback(int32_t frames_to_deliver, const void* object, float* buffer);

class MidiPlayer
{
    size_t midi_size;
    uint8_t* pMidi_buffer;

    std::thread m_thread;

    public:
    MidiSong midi;
    MidiSendFunc pFunc[50];
    Instrument *instruments[50];
    uint32_t m_event_samples_to_render;

    bool load(char *pFilename)
    {
        stop();            
        m_event_samples_to_render = 0;

        load_file(pFilename, &pMidi_buffer, &midi_size);

        midi.Reset();
        
        return midi.LoadMidi(pMidi_buffer, midi_size);
    }

    void unload()
    {
        stop();
        
        for (uint32_t t = 0; t < midi.GetTrackCount(); t++)
        {
            pFunc[t]=NULL;

            delete instruments[t];
            instruments[t] = NULL;
        }
        free(pMidi_buffer);
    }

    void set_instruments()
    {
        for (uint32_t t = 0; t < midi.GetTrackCount(); t++)
        {
#ifdef HAS_MT32EMU
            Instrument *instrument = GetMT32();
//#elifdef MSFA_FOUND  
//            Instrument *instrument = GetDX7("./rom1a.syx");
#else            
            Instrument *instrument = GetNewPiano();
#endif
            instruments[t] = instrument;
            pFunc[t] = [instrument](uint8_t* pdata, size_t size) {
                    instrument->send_midi(pdata, size);
            };
        } 
    }

    void play()
    {
        if (bExit==true)
        {
            bExit = false;
            const void *pData = (void *)&midi;
            std::thread t1(playLoop, .1f, sample_rate, pData, play_callback);
            m_thread.swap(t1);
        }
        else
        {
            bExit = true;
            m_thread.join();
        }
    }

    void stop()
    {
        if (bExit==false)
        {
            bExit = true;
            m_thread.join();
            midi.Reset();
            total_rendered_samples = 0;
            m_event_samples_to_render = 0;
            for (uint32_t t = 0; t < midi.GetTrackCount(); t++)
            {
                instruments[t]->reset();
            }
        }
    }

    void join()
    {
        m_thread.join();
    }

    uint32_t seek(float t)
    {
        stop();
        uint32_t micros = midi.seek(t*1e6, pFunc);
        total_rendered_samples = ((uint64_t)micros * sample_rate) / 1000000;
        play();
        return micros;
    }

    uint32_t render(int32_t frames_to_deliver, float* buffer)
    {
        if (bExit==true)
            return 0;

        uint32_t offset = 0;
        uint32_t remaining = frames_to_deliver;
        while (remaining > 0) 
        {
            if (m_event_samples_to_render == 0)
            {
                uint64_t microseconds_to_render = midi.step(pFunc);
                if (microseconds_to_render == NO_EVENTS)
                    return 0;

                m_event_samples_to_render = (uint64_t)(sample_rate * microseconds_to_render)/ 1000000;
            }

            uint32_t rendered = MIN(remaining, m_event_samples_to_render);
            m_event_samples_to_render -= rendered;

            for (uint32_t track = 0; track < midi.GetTrackCount(); ++track) 
            {
                instruments[track]->render_samples(rendered, buffer + 2 * offset);
            }

            remaining -= rendered;
            offset += rendered;
        }
        return offset;
    }
};

MidiPlayer midi_player;

int32_t play_callback(int32_t frames_to_deliver, const void* object, float* buffer) {
    if (bExit) {
        return 0;
    }
    memset(buffer, 0, static_cast<size_t>(frames_to_deliver) * 2 * sizeof(float));

    uint32_t offset = midi_player.render(frames_to_deliver, buffer);
    total_rendered_samples+=offset;
    return static_cast<int32_t>(offset);
}

#ifdef SIMPLEMIDI_CLI_ONLY

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: SimpleMidi <song.mid>\n");
        return 2;
    }

    if (!midi_player.load(argv[1])) {
        return 1;
    }

    midi_player.set_instruments();
    midi_player.play();
    midi_player.join();

    return 0;
}

#else

#include <thread>
#include <map>
#include <vector>
#include <string>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

Instrument *GetDX7(char *pFilename_bank);


class drawInst : public Instrument
{
public:
    std::vector<ImVec2> pos;

    float m_time = 0;

    std::map<int32_t,float> m_key; 
    
    uint8_t m_min_key = 255;
    uint8_t m_max_key = 0;

    virtual void send_midi(uint8_t* pdata, size_t size)
    {
        unsigned char subType = pdata[0] >> 4;
        unsigned char channel = pdata[0] & 0xf; 

        if (subType == 0x8 || subType == 0x9)
        {
            uint8_t key = pdata[1];
            uint8_t speed = pdata[2];

            m_min_key = MIN(m_min_key, key);
            m_max_key = MAX(m_max_key, key);

            if (subType == 8 || speed==0)
            {
                float time = m_key[key];
                pos.push_back(ImVec2(  time, (key+0)));                
                pos.push_back(ImVec2(m_time, (key+0)));
            }
            else if (subType == 9)
            {
                m_key[key] = m_time;
            }
        }        
    }

    void draw(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 canvas_p1, float time_ini, float time_fin, float time_current, ImU32 color)
    {
        for(int i=0;i<pos.size();i+=2)
        {
            float t;

            t = unlerp(pos[i+0].y+0, (float)m_min_key, (float)m_max_key+1);
            float y0 = lerp(canvas_p1.y-1, canvas_p0.y+1, t);
            t = unlerp(pos[i+1].y+1, (float)m_min_key, (float)m_max_key+1);
            float y1 = lerp(canvas_p1.y-1, canvas_p0.y+1, t);

            float t1 = unlerp(pos[i+0].x, time_ini, time_fin);
            float t2 = unlerp(pos[i+1].x, time_ini, time_fin);
            float x0 = lerp(canvas_p0.x+1, canvas_p1.x-1, t1);
            float x1 = lerp(canvas_p0.x+1, canvas_p1.x-1, t2);

            if (time_current>pos[i+0].x && (time_current<pos[i+1].x))
                draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32_WHITE);
            else
                draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color);
            
        }
    }    

    virtual void render_samples(uint32_t n_samples, float* pOut) {}
};

drawInst *pI;
std::vector<float> ticks;
void create_piano_roll(MidiSong *pMidi)
{
    pI = new drawInst[pMidi->GetTrackCount()];
    for (uint32_t t = 0; t < pMidi->GetTrackCount(); t++)
    {
        Instrument *inst = &pI[t];
        midi_player.pFunc[t] = [inst](uint8_t* pdata, size_t size) {
            inst->send_midi(pdata, size);
        };
    }

    pMidi->Reset();

    while(pMidi->step(midi_player.pFunc)!=NO_EVENTS) 
    {
        for(int i=0;i<pMidi->GetTrackCount();i++)
        {
            pI[i].m_time = pMidi->get_elapsed_microseconds()/1000000.0f;
        }
    }

    ticks.clear();
    for (int i=0;i<200;i++)
    {
        //float milliseconds_per_midi_tick = (pMidi->m_midi_state.m_microSecondsPerMidiTick/ 1000.0f);
        ticks.push_back((i*pMidi->get_microseconds_per_quarter_note())/1000000.0f);
    }

    pMidi->Reset();
}

std::vector<std::string> midi_files;

int inst[20];
bool midi_reload = false;
const char *midi_folder = ".";
static const char* current_midi = NULL;

void GenerateTrack(ImVec2 *pP0, ImVec2 *pP1)
{
    *pP0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
    canvas_sz.y = 75;
    //if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    //if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    *pP1 = ImVec2(pP0->x + canvas_sz.x, pP0->y + canvas_sz.y);

    ImGui::InvisibleButton("my_button_id", canvas_sz);
}

bool MouseInRectangle(ImVec2 p0, ImVec2 p1, float *pX)
{
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 canvas_sz = ImVec2(p1.x - p0.x, p1.y - p0.y);
    const ImVec2 mouse_pos_in_canvas((io.MousePos.x - p0.x) / canvas_sz.x, -(io.MousePos.y - p1.y) / canvas_sz.y);

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (
            mouse_pos_in_canvas.x>=0.0f && 
            mouse_pos_in_canvas.x<=1.0f &&
            mouse_pos_in_canvas.y>=0.0f && 
            mouse_pos_in_canvas.y<=1.0f)
        {
            *pX = mouse_pos_in_canvas.x;
            return true;
        }
    }
    return false;
}

void draw_track(drawInst *pInstDraw, const ImVec2 &canvas_p0, const ImVec2 &canvas_p1, float time_ini, float time_fin, float global_time, ImVec4 color)
{
#ifdef MSFA_FOUND            
    //allows changing instrument
    ImGui::PushID(t);
    if (ImGui::SliderInt("instrument", &inst[t], 0,31))
    {
        uint8_t data[2] = { 0xc0 , (uint8_t)inst[t] };
        pTrack->pPiano->send_midi(data, 2);
    }
    ImGui::Checkbox("", &pTrack->pPiano->m_mute)); 
    ImGui::SameLine();
#endif

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));


    // Draw grid + all lines in the canvas
    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
   
    static bool opt_enable_grid = true;
    if (opt_enable_grid)
    {
        for (float i = 0; i < ticks.size(); i++)
        {
            float t = unlerp(ticks[i], time_ini, time_fin);            
            float x = lerp(canvas_p0.x, canvas_p1.x, t);
            draw_list->AddLine(ImVec2(x, canvas_p0.y), ImVec2(x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
        }
    }
    
    pInstDraw->draw(draw_list, canvas_p0, canvas_p1, time_ini, time_fin, global_time, IM_COL32(color.x*255, color.y*255, color.z*255, color.w*255));

    {
        float t = unlerp(global_time, time_ini, time_fin);                      
        float x = lerp(canvas_p0.x, canvas_p1.x, t);
        draw_list->AddLine(
            ImVec2(x, canvas_p0.y),
            ImVec2(x, canvas_p1.y),
            IM_COL32(255, 255, 0, 255), 2.0f);
    }

    draw_list->PopClipRect();                
}

ImVec4 saved_palette[32] = {};
float time_width = 40.0f;
float midi_length = 0;

void doui(GLFWwindow* window)
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    static bool p_open = true;
    ImGui::Begin("Hello, world!", &p_open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);                          // Create a window called "Hello, world!" and append into it.

    if (ImGui::BeginCombo("##combo", current_midi)) // The second parameter is the label previewed before opening the combo.
    {
        for (int n = 0; n < midi_files.size(); n++)
        {
            bool is_selected = (current_midi == midi_files[n].c_str()); // You can store your selection however you want, outside or inside your objects
            if (ImGui::Selectable(midi_files[n].c_str(), is_selected)) 
            {
                current_midi = midi_files[n].c_str();
                midi_reload = true;
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
        }
        ImGui::EndCombo();
    }

    if (midi_reload)
    {
        midi_reload = false;
        char path[256];
        sprintf(path, "%s/%s", midi_folder, current_midi);
	    printf("attempting to load %s\n", path);
        midi_player.unload();
        midi_player.load(path);
        total_rendered_samples = 0;

        //calculate piano rolls
        create_piano_roll(&midi_player.midi);
        midi_player.set_instruments();

        midi_length = midi_player.seek(10000)/1e6;
        midi_player.seek(0);
        midi_player.play();
    }

    MidiSong *pMidi = &midi_player.midi;

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    float ms_per_quarter_note = (pMidi->m_midi_state.m_microSecondsPerMidiTick * pMidi->m_midi_state.m_ticksPerQuarterNote)/1000.0f;
    int bpm = 60*1000/ms_per_quarter_note;
    float time = total_rendered_samples / (float)sample_rate;

    ImGui::Text("BPM: %d time: %f total: %f", bpm, time, midi_length);

    float time_ini, time_fin;
    {
        time_ini = 0.0f;
        time_fin = midi_length;
    }

    if (ImGui::Button("Stop"))
    {
        midi_player.stop();
    }

    ImGui::SameLine();

    if (ImGui::Button("Start"))
    {
        midi_player.play();
    }

    ImGui::SliderFloat("zoom", &time_width, 0.0f, 120.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

    for (uint32_t t = 0; t < pMidi->GetTrackCount(); t++)
    {
        if (pI[t].pos.size()==0)
            continue;

        MidiTrack *pTrack = pMidi->GetTrack(t);
        ImGui::PushID(t);
        ImGui::Checkbox("##", &midi_player.instruments[t]->m_mute);
        ImGui::SameLine();
        ImGui::Text("%i %s %s", t, pTrack->m_InstrumentName, pTrack->m_TrackName);

        ImVec2 p0, p1;
        GenerateTrack(&p0, &p1);
        
        float pX;
        if (MouseInRectangle(p0, p1, &pX))
        {
            midi_player.seek(pX*midi_length);
            printf("%f, %f\n", pX, pX*midi_length);
        }
        draw_track(&pI[t], p0, p1, time_ini, time_fin, time, saved_palette[t]);
        ImGui::PopID();
    }

    ImGui::End();
}


///////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

GLFWwindow* initui()
{
   // Setup window
   glfwSetErrorCallback(glfw_error_callback);

   if (!glfwInit())
       return NULL;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

   GLFWwindow* window = glfwCreateWindow(1280, 920, "Dear ImGui GLFW+OpenGL2 example", NULL, NULL);
   if (window == NULL)
       return NULL;
   glfwMakeContextCurrent(window);
   glfwSwapInterval(1); // Enable vsync
   
   // Setup Dear ImGui context
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGuiIO& io = ImGui::GetIO(); (void)io;
   //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
   //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
   
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
   return window;
}

int main(int argc, char **argv)
{
    //unit_tests();
    if (argc>=3) 
    {
        current_midi=NULL;
        for(int i=1;i<argc;i++)
        {
            if (strcmp(argv[i], "-m") == 0)
            {
                i++;
                current_midi = argv[i];
                midi_reload = true;
            }
            else if (strcmp(argv[i], "-f") == 0)
            {
                i++;
                midi_folder = argv[i];
                printf("Loading from folder: %s\n", midi_folder);

                get_midi_list(midi_folder, &midi_files);
            }
        }
    }
    else
    {
        printf("usage:\n\n");
        printf("- SimpleMidi -f <folder_with_midis> -m <midi_file>\n");
            exit(0);
    }

    // Generate a default palette. The palette will persist and can be edited.
    for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++)
    {
        ImGui::ColorConvertHSVtoRGB(n / 31.0f, 0.8f, 0.8f,
            saved_palette[n].x, saved_palette[n].y, saved_palette[n].z);
        saved_palette[n].w = 1.0f; // Alpha
    }

    GLFWwindow* window = initui();
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        doui(window);

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    midi_player.stop();

    return 0;
}

#endif
