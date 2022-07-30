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

#include "audio.h"
#include "midi.h"

#include "inst-piano.h"

Midi midi;

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

bool p_open = true;

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


class InstrumentMinMax : public Instrument
{    
public:
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
        }
    };
    virtual void render_samples(uint32_t n_samples, float* pOut) {};
};

float time_ini = 0.0f;
float time_fin = 10.0f;
float time_width = 10.0f;

class drawInst : public Instrument
{
public:
    ImVec2 canvas_p0;
    ImVec2 canvas_p1;
    ImVec4 m_color;
    float m_time;

    uint8_t m_min_key = 0;
    uint8_t m_max_key = 0;

    std::map<int32_t,float> m_key; 

    virtual void send_midi(uint8_t* pdata, size_t size)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        unsigned char subType = pdata[0] >> 4;
        unsigned char channel = pdata[0] & 0xf; 

        if (subType == 0x8 || subType == 0x9)
        {
            uint8_t key = pdata[1];
            uint8_t speed = pdata[2];

            if (subType == 8 || speed==0)
            {
                float time = m_key[key];

                float t;
                t = unlerp(key  , m_min_key, m_max_key);
                float y0 = lerp(t, canvas_p1.y, canvas_p0.y);
                t = unlerp(key+1, m_min_key, m_max_key);
                float y1 = lerp(t, canvas_p1.y, canvas_p0.y);

                draw_list->AddRectFilled(
                    ImVec2(time, y0), 
                    ImVec2(m_time, y1),
                    IM_COL32(m_color.x*255, m_color.y*255, m_color.z*255, m_color.w*255));
            }
            else if (subType == 9)
            {
                m_key[key] = m_time;
            }
        }        
    }
    virtual void render_samples(uint32_t n_samples, float* pOut) {}
};            

void GetTimingChanges(Midi *pMidi)
{
    for (int t=0;t<pMidi->GetTrackCount();t++)
    {
        MidiTrack *pTrackOrg = pMidi->GetTrack(t);
    }
}

static bool opt_enable_grid = true;
static bool opt_enable_context_menu = true;
void draw_track(int t, Midi *pMidi, float global_time, ImVec4 color)
{
    MidiTrack *pTrackOrg = pMidi->GetTrack(t);

    MidiTrack track(*pTrackOrg);
    track.m_pMidiState = NULL;

    std::vector<float> m_grid;
    InstrumentMinMax I;
    track.pPiano = &I;
    {
        track.Reset();
        uint32_t last_metronome_click = 0;
        uint32_t midi_ticks = 0;
        uint32_t time = 0;
        m_grid.push_back(0);
        while (midi_ticks!=0xffffffff)
        {
            if ( last_metronome_click<= time)// && milliseconds<=(last_metronome_click + interval_metronome_click))
            {
                uint32_t interval_metronome_click = midi.m_midi_state.m_microSecondsPerMidiTick * midi.m_midi_state.m_midi_ticks_per_metronome_tick;
                last_metronome_click += interval_metronome_click;

                m_grid.push_back(last_metronome_click/1000000.0f);
            }

            uint32_t old_time = midi_ticks;
            midi_ticks = track.play(midi_ticks);

            time += midi.m_midi_state.m_microSecondsPerMidiTick * (midi_ticks - old_time);
        }        
    }
    if (I.m_min_key>I.m_max_key)
        return;


    ImGui::PushID(t);
    ImGui::Checkbox("", &pTrackOrg->pPiano->m_mute); 
    ImGui::SameLine();
    ImGui::Text("%i %s %s", pTrackOrg->m_channel, pTrackOrg->m_TrackName, pTrackOrg->m_InstrumentName);
    ImGui::SameLine();
    ImGui::Text("time sig: %i/%i", pTrackOrg->m_timesignatureNum, pTrackOrg->m_timesignatureDen);
    ImGui::PopID();

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
    canvas_sz.y = 80;
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
    const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
    {
        //scrolling.x += io.MouseDelta.x;
        //scrolling.y += io.MouseDelta.y;
    }

    // Draw grid + all lines in the canvas
    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    if (opt_enable_grid)
    {
        for (float i = 0; i < m_grid.size(); i++)
        {
            float t = unlerp(m_grid[i], time_ini, time_fin);            
            float x = lerp(t, canvas_p0.x, canvas_p1.x);
            draw_list->AddLine(ImVec2(x, canvas_p0.y), ImVec2(x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
        }
    }

    {
        drawInst d;
        d.m_min_key = I.m_min_key;
        d.m_max_key = I.m_max_key;
        d.m_color = color;
        d.canvas_p0 = top_left;
        d.canvas_p1 = bottom_right;
        track.pPiano = &d;
        track.Reset();
        uint32_t midi_ticks = 0;
        uint32_t time = 0;
        while (midi_ticks!=0xffffffff)
        {
            uint32_t old_time = midi_ticks;
            midi_ticks = track.play(midi_ticks);
            time += (midi.m_midi_state.m_microSecondsPerMidiTick * (midi_ticks - old_time));

            float t = unlerp(((float)time)/1000000.0f, time_ini, time_fin);                   
            float x = lerp(t, canvas_p0.x, canvas_p1.x);
            d.m_time = x; 
        }
    }

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

void doui(GLFWwindow* window, Midi *pMidi)
{
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Generate a default palette. The palette will persist and can be edited.
    static bool saved_palette_init = true;
    static ImVec4 saved_palette[32] = {};
    if (saved_palette_init)
    {
        for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++)
        {
            ImGui::ColorConvertHSVtoRGB(n / 31.0f, 0.8f, 0.8f,
                saved_palette[n].x, saved_palette[n].y, saved_palette[n].z);
            saved_palette[n].w = 1.0f; // Alpha
        }
        saved_palette_init = false;
    }

    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
/*
        for (uint32_t t = 0; t < pMidi->GetTrackCount(); t++)
        {
            MidiTrack *pTrack = pMidi->GetTrack(t);
            ImGui::Button("##",ImVec2(20,20));
            ImGui::SameLine();
            ImGui::Text("%i %s %s", pTrack->m_channel, pTrack->m_TrackName, pTrack->m_InstrumentName);
        }
*/        
/*
        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
*/

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("m_ticksPerQuarterNote = %d", pMidi->m_midi_state.m_ticksPerQuarterNote);
        ImGui::Text("m_microSecondsPerMidiTick = %d", pMidi->m_midi_state.m_microSecondsPerMidiTick);
        ImGui::Text("ms per quarter note = %d", pMidi->m_midi_state.m_microSecondsPerMidiTick * pMidi->m_midi_state.m_ticksPerQuarterNote);


        static float time = 0;
        time += ImGui::GetIO().DeltaTime;

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

        ImGui::SliderFloat("time_width", &time_width, 0.0f, 120.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

        ImGui::Text("time = %f", time);
        for (uint32_t t = 0; t < pMidi->GetTrackCount(); t++)
        {
            draw_track(t, pMidi, time, saved_palette[t]);
        }

        ImGui::End();
    }

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}

bool bExit = false;

int32_t play_callback(int32_t frames_to_deliver, float *buf)
{
    if (bExit)
        return 0;

    for (uint32_t i=0;i<frames_to_deliver*2;i++)
        buf[i]=0;
    return midi.RenderMidi(48000, 2, frames_to_deliver, buf);
}

///////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
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
    const char *filename = "John Lennon - Imagine.mid";
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
    //const char *filename = "Simpsonstheme.mid";
    //const char *filename = "romanceforclassicalguitar.mid";
    //const char *filename = "Greek_Zorba.mid";
    //const char *filename = "Soundtrack_Ghostbusters.mid";
    //const char *filename = "MADONNA.Holiday K.mid";
    //const char *filename = "Ennio_Morricone__Thegoodthebadandtheugly.mid";

    if (argc > 1)
    {
        filename = argv[1];
    }

    size_t midi_size;
    uint8_t* pMidi_buffer;
    char path[256];
    strcpy(path, "../songs/");
    strcat(path, filename);
    load_file(path, &pMidi_buffer, &midi_size);

    GLFWwindow* window = initui();

    if (midi.LoadMidi(pMidi_buffer, midi_size) == false)
        return 0;

    for (uint32_t t = 0; t < midi.GetTrackCount(); t++)
    {
        midi.GetTrack(t)->pPiano = GetNewPiano();
    }

    std::thread t1(playLoop, 0.2f, 48000, play_callback);

    while(!glfwWindowShouldClose(window))
    {
        doui(window, &midi);
    }

    bExit=true;

    t1.join();

    free(pMidi_buffer);

    return 0;
}
