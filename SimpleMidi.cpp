// xmidi.cpp : Defines the entry point for the console application.
//
#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <map>

#include "audio.h"
#include "midi.h"

#include "inst-piano.h"

Instrument *pPiano = GetPiano();
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


   GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL2 example", NULL, NULL);
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

class drawInst : public Instrument
{
public:
    ImVec2 canvas_p0;
    ImVec2 canvas_p1;
    ImVec4 m_color;
    float m_time;

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

            //if (channel==9)
            //    return;
            //printf("%i %i, %i %i\n", subType, channel, key, speed);

            if (subType == 8)
            {
                float time = m_key[key];

                float y = key - 10;
                draw_list->AddRectFilled(
                    ImVec2(canvas_p0.x + time       , canvas_p0.y + y * 5.0f), 
                    ImVec2(canvas_p0.x + m_time, canvas_p0.y + (y+1) * 5.0f),
                    IM_COL32(m_color.x*255, m_color.y*255, m_color.z*255, m_color.w*255));
            }
            else if (subType == 9)
            {
                m_key[key] = m_time;
            }
        }        
    }
    virtual void render_samples(int n_samples, float* pOut) {}
    virtual void printKeyboard() {}
};            


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


    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        for (uint32_t t = 0; t < pMidi->GetTrackCount(); t++)
        {
            MidiTrack *pTrack = pMidi->GetTrack(t);
            //ImGui::Text("%i %s %s", pTrack->m_channel, pTrack->m_TrackName, pTrack->m_InstrumentName);
        }

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        {
            static ImVector<ImVec2> points;
            static ImVec2 scrolling(0.0f, 0.0f);
            static bool opt_enable_grid = true;
            static bool opt_enable_context_menu = true;
            static bool adding_line = false;

            ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
            ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
            if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
            if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
            ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

           // Draw border and background color
            ImGuiIO& io = ImGui::GetIO();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
            draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

            // This will catch our interactions
            ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const bool is_hovered = ImGui::IsItemHovered(); // Hovered
            const bool is_active = ImGui::IsItemActive();   // Held
            const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y); // Lock scrolled origin
            const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

            // Pan (we use a zero mouse threshold when there's no context menu)
            // You may decide to make that threshold dynamic based on whether the mouse is hovering something etc.
            const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
            if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
            {
                scrolling.x += io.MouseDelta.x;
                scrolling.y += io.MouseDelta.y;
            }

            // Draw grid + all lines in the canvas
            draw_list->PushClipRect(canvas_p0, canvas_p1, true);
            if (opt_enable_grid)
            {
                const float GRID_STEP = 64.0f;
                for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x; x += GRID_STEP)
                    draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
                for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y; y += GRID_STEP)
                    draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(200, 200, 200, 40));
            }
            for (int n = 0; n < points.Size; n += 2)
                draw_list->AddLine(ImVec2(origin.x + points[n].x, origin.y + points[n].y), ImVec2(origin.x + points[n + 1].x, origin.y + points[n + 1].y), IM_COL32(255, 255, 0, 255), 2.0f);

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
            
            for (uint32_t t = 0; t < pMidi->GetTrackCount(); t++)
            {
                MidiTrack m(*pMidi->GetTrack(t));
                drawInst d;
                d.m_color = saved_palette[t];
                d.canvas_p0 = origin;
                d.canvas_p1 = canvas_p1;
                m.pPiano = &d;
                uint32_t midi_ticks = 0;
                while (midi_ticks!=0xffffffff)
                {
                    d.m_time = midi_ticks/10.0f;

                    midi_ticks = m.play(midi_ticks);
                }
            }


            draw_list->PopClipRect();            
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

int32_t play_callback(int32_t frames_to_deliver, float *buf)
{
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
    //const char* filename = "32175_Yie-Ar-KungFu.mid";
    //const char* filename = "slowman.mid";
    //const char *filename = "AroundTheWorld.mid";
    //const char *filename = "Tetris - Tetris Main Theme.mid";
    //const char* filename = "darude-sandstorm.mid";
    //const char* filename = "Gigi_Dagostino__Lamour_Toujours.mid";
    //const char* filename = "Never-Gonna-Give-You-Up-3.mid";
    //const char *filename = "stage-1.mid";
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
    const char *filename = "Theme_of_tara_jannee2.mid";

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

    if (midi.LoadMidi(pMidi_buffer, midi_size))
    {
        for (uint32_t t = 0; t < midi.GetTrackCount(); t++)
        {
            midi.GetTrack(t)->pPiano = GetPiano();
        }

        while(!glfwWindowShouldClose(window))
        {
            doui(window, &midi);
        }


        playLoop(0.1f, 48000, play_callback);
        free(pMidi_buffer);
    }

    return 0;
}
