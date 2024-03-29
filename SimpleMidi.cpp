// xmidi.cpp : Defines the entry point for the console application.
//
#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <map>
#include <vector>
#include <string>
#include "audio.h"
#include "midi.h"

#include "inst-piano.h"
Instrument *GetDX7(char *pFilename_bank);

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#define MIDI_FOLDER "./songs"

class drawInst : public Instrument
{
public:
    std::vector<ImVec2> pos;

    float m_time;

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
                pos.push_back(ImVec2(m_time, (key+0)));
                pos.push_back(ImVec2(  time, (key+1)));                
            }
            else if (subType == 9)
            {
                m_key[key] = m_time;
            }
        }        
    }

    void draw(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 canvas_p1, float time_ini, float time_fin, ImU32 color)
    {
        for(int i=0;i<pos.size();i+=2)
        {
            float t;

            t = unlerp(pos[i+0].y, (float)m_min_key, (float)m_max_key);
            float y0 = lerp(t, canvas_p1.y, canvas_p0.y);
            t = unlerp(pos[i+1].y, (float)m_min_key, (float)m_max_key);
            float y1 = lerp(t, canvas_p1.y, canvas_p0.y);

            t = unlerp(pos[i+0].x, time_ini, time_fin);
            float x0 = lerp(t, canvas_p0.x, canvas_p1.x);
            t = unlerp(pos[i+1].x, time_ini, time_fin);
            float x1 = lerp(t, canvas_p0.x, canvas_p1.x);

            draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color);
        }
    }    

    virtual void render_samples(uint32_t n_samples, float* pOut) {}
};

std::vector<float> ticks;

void create_piano_roll(MidiSong *pMidi)
{
    drawInst *pI = new drawInst[pMidi->GetTrackCount()];

    for (uint32_t t = 0; t < pMidi->GetTrackCount(); t++)
    {
        pMidi->GetTrack(t)->pPiano = &pI[t];
        pMidi->GetTrack(t)->set_private_data(&pI[t]);
    }

    pMidi->Reset();

    while(pMidi->step()!=NO_EVENTS) 
    {
        for(int i=0;i<pMidi->GetTrackCount();i++)
        {
            pI[i].m_time = pMidi->get_elapsed_milliseconds()/1000.0f;
        }
    }

    ticks.clear();
    for (int i=0;i<200;i++)
    {
        float milliseconds_per_midi_tick = (pMidi->m_midi_state.m_microSecondsPerMidiTick/ 1000.0f);
        ticks.push_back(i*(pMidi->m_midi_state.m_ticksPerQuarterNote * milliseconds_per_midi_tick)/1000.0f);
    }

    pMidi->Reset();
}

std::vector<std::string> midi_files;

size_t total_rendered_samples = 0;

bool bExit = true;
int32_t play_callback(int32_t frames_to_deliver, const void *object, float *buf)
{
    if (bExit)
        return 0;

    for (uint32_t i=0;i<frames_to_deliver*2;i++)
        buf[i]=0;

    size_t rendered_samples =  ((MidiSong*)object)->RenderMidi(48000, frames_to_deliver, buf);

    total_rendered_samples += rendered_samples;

    return rendered_samples;    
}

int inst[20];
class MidiPlayer
{
    size_t midi_size;
    uint8_t* pMidi_buffer;

    std::thread m_thread;

    public:
    MidiSong midi;

    bool load(char *pFilename)
    {
        stop();            

        load_file(pFilename, &pMidi_buffer, &midi_size);

        midi.Reset();

        return midi.LoadMidi(pMidi_buffer, midi_size);
    }

    void unload()
    {
        stop();
        free(pMidi_buffer);
    }

    void set_instruments()
    {
        for (uint32_t t = 0; t < midi.GetTrackCount(); t++)
        {
#ifdef MSFA_FOUND            
            midi.GetTrack(t)->pPiano = GetDX7("./rom1a.syx");
#else    
            midi.GetTrack(t)->pPiano = GetNewPiano();
#endif            
            //
        }
    }

    void play()
    {
        if (bExit==true)
        {
            bExit = false;
            const void *pData = (void *)&midi;
            std::thread t1(playLoop, .1f, 48000, pData, play_callback);
            m_thread.swap(t1);
        }
    }

    void stop()
    {
        if (bExit==false)
        {
            bExit = true;
            m_thread.join();
        }
    }
};

MidiPlayer midi_player;
bool midi_reload = true;
static const char* current_midi = NULL;

void draw_track(int t, MidiSong *pMidi, float time_ini, float time_fin, float global_time, ImVec4 color)
{
    MidiTrack *pTrack = pMidi->GetTrack(t);
    drawInst *pInstDraw = (drawInst *)(pTrack->get_private_data());

    if (pInstDraw->pos.size()==0)
        return;

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

    ImGui::Text("%i %s %s", t, pTrack->m_InstrumentName, pTrack->m_TrackName);
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
    canvas_sz.y = 200;
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    // Draw border and background color
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

    // This will catch our interactions
    ImGui::PushID(t);
    ImGui::InvisibleButton("##", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    ImGui::PopID();
    const bool is_hovered = ImGui::IsItemHovered(); // Hovered
    const bool is_active = ImGui::IsItemActive();   // Held
    const ImVec2 top_left(canvas_p0.x, canvas_p0.y); // Lock scrolled top_left
    const ImVec2 bottom_right(canvas_p1.x, canvas_p1.y); // Lock scrolled top_left
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - top_left.x, io.MousePos.y - top_left.y);

    // Pan (we use a zero mouse threshold when there's no context menu)
    // You may decide to make that threshold dynamic based on whether the mouse is hovering something etc.
    static bool opt_enable_context_menu = true;
    const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
    {
        //scrolling.x += io.MouseDelta.x;
        //scrolling.y += io.MouseDelta.y;
    }

    // Draw grid + all lines in the canvas
    draw_list->PushClipRect(canvas_p0, canvas_p1, true);

    //char str[1024];
    //sprintf(str, "%i %s %s", pTrackOrg->m_channel, pTrackOrg->m_TrackName, pTrackOrg->m_InstrumentName);
    //draw_list->AddText(ImVec2(canvas_p0.x+5, canvas_p0.y+5), IM_COL32(200, 200, 200, 1255), str);
    static bool opt_enable_grid = true;
    if (opt_enable_grid)
    {
        for (float i = 0; i < ticks.size(); i++)
        {
            float t = unlerp(ticks[i], time_ini, time_fin);            
            float x = lerp(t, canvas_p0.x, canvas_p1.x);
            draw_list->AddLine(ImVec2(x, canvas_p0.y), ImVec2(x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
        }
    }
    
    pInstDraw->draw(draw_list, canvas_p0, canvas_p1, time_ini, time_fin, IM_COL32(color.x*255, color.y*255, color.z*255, color.w*255));

    {
        float t = unlerp(global_time, time_ini, time_fin);                      
        float x = lerp(t, canvas_p0.x, canvas_p1.x);
        draw_list->AddLine(
            ImVec2(x, canvas_p0.y),
            ImVec2(x, canvas_p1.y),
            IM_COL32(255, 255, 0, 255), 2.0f);
    }

    draw_list->PopClipRect();                
}

ImVec4 saved_palette[32] = {};
float time_width = 10.0f;

void doui(GLFWwindow* window)
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    static bool p_open = true;
    ImGui::Begin("Hello, world!", &p_open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);                          // Create a window called "Hello, world!" and append into it.

    static bool midi_reload = true;
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
        sprintf(path, MIDI_FOLDER"/%s", current_midi);
        midi_player.load(path);
        total_rendered_samples = 0;

        //calculate piano rolls
        create_piano_roll(&midi_player.midi);

        midi_player.set_instruments();
        midi_player.play();
    }

    MidiSong *pMidi = &midi_player.midi;

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    //ImGui::Text("m_ticksPerQuarterNote = %d", pMidi->m_midi_state.m_ticksPerQuarterNote);
    //ImGui::Text("m_microSecondsPerMidiTick = %d", pMidi->m_midi_state.m_microSecondsPerMidiTick);
    float ms_per_quarter_note = (pMidi->m_midi_state.m_microSecondsPerMidiTick * pMidi->m_midi_state.m_ticksPerQuarterNote)/1000.0f;
    int bpm = 60*1000/ms_per_quarter_note;
    float time = total_rendered_samples / 48000.0f;

    ImGui::Text("BPM: %d time = %f", bpm, time);

    float time_ini, time_fin;
    if (time>2.0f)
    {
        time_ini = time-2.0f;
        time_fin = time+time_width-2.0f;
    }
    else
    {
        time_ini = 0.0f;
        time_fin = time_width;
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

    ImGui::SliderFloat("time_width", &time_width, 0.0f, 120.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

    for (uint32_t t = 0; t < pMidi->GetTrackCount(); t++)
    {
        draw_track(t, pMidi, time_ini, time_fin, time, saved_palette[t]);
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
    get_midi_list(MIDI_FOLDER, &midi_files);
    //const char *filename = "Mario-Sheet-Music-Overworld-Main-Theme.mid";
    //const char* filename = "slowman.mid";
    //const char *filename = "Theme From San Andreas.mid";
    //const char *filename = "Tetris - Tetris Main Theme.mid";
    //const char* filename = "darude-sandstorm.mid";
    //const char* filename = "Gigi_Dagostino__Lamour_Toujours.mid";
    //const char *filename = "doom.mid";
    //const char *filename = "TakeOnMe.mid";
    //const char *filename = "Guns n Roses - November Rain.mid";
    //const char *filename = "mozart-piano-concerto-21-2-elvira-madigan-piano-solo.mid";
    //const char *filename = "John Lennon - Imagine.mid";
    //const char *filename = "BadRomance.mid";
    //const char* filename = "The Legend of Zelda Ocarina of Time - Song of Storms.mid";
    //const char* filename = "The Legend of Zelda Ocarina of Time - New Ocarina Melody.mid";
    //const char *filename = "Guns n Roses - Sweet Child O Mine.mid";
    //const char *filename = "goonies.mid";
    //const char *filename = "kungfu.mid";
    //const char *filename = "metalgr1.mid";
    //const char *filename = "MetalGearMSX_OperationIntrudeN313.mid";
    //const char *filename = "Theme_of_tara_jannee2.mid";
    //const char *filename = "Dr_Dre_Still_Dre_Ft_Snoop_Dog.mid";
    //const char *filename = "Star_Wars__Imperial_March.mid";
    //const char *filename = "Indiana_Jones__Theme_Music.mid";
    //const char *filename = "Ennio_Morricone_OnceuponatimeinAmerica.mid";
    const char *filename = "piano_sonata_545_1_(c)oguri.mid";
    //const char *filename = "Simpsonstheme.mid";
    //const char *filename = "romanceforclassicalguitar.mid";
    //const char *filename = "Greek_Zorba.mid";
    //const char *filename = "Soundtrack_Ghostbusters.mid";
    //const char *filename = "MADONNA.Holiday K.mid";
    //const char *filename = "Ennio_Morricone__Thegoodthebadandtheugly.mid";

    current_midi = (argc > 1)?argv[1]:filename;

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
