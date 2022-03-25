// Dear ImGui: standalone example application for GLFW + OpenGL2, using legacy fixed pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_glfw_opengl2/ folder**
// See imgui_impl_glfw.cpp for details.

#include "imgui/imgui.h"
#include "backend/imgui_impl_glfw.h"
#include "backend/imgui_impl_opengl2.h"
#include "implot/implot.h"
#include "SerEncoder.hpp"
#include <stdio.h>
#define eprintf(str, ...)                                                         \
    {                                                                             \
        fprintf(stderr, "%s(),%d: " str "\n", __func__, __LINE__, ##__VA_ARGS__); \
        fflush(stderr);                                                           \
    }
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define IMGUI_ON_WINDOWS
#include <windows.h>
#include <tchar.h>
#endif

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Shell32.lib")
#endif

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

struct ScrollingBuffer
{
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer(int max_size = 2000)
    {
        MaxSize = max_size;
        Offset = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y)
    {
        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x, y));
        else
        {
            Data[Offset] = ImVec2(x, y);
            Offset = (Offset + 1) % MaxSize;
        }
    }
    void Erase()
    {
        if (Data.size() > 0)
        {
            Data.shrink(0);
            Offset = 0;
        }
    }

    float Min(float xmin, float xmax)
    {
        float res = 0;
        ImVector<int> index;
        index.reserve(MaxSize);
        for (int i = 0; i < Data.size(); i++) // find valid indices
        {
            float x = Data[i].x;
            if (x > xmin && x < xmax)
                index.push_back(i);
        }
        if (index.size() > 0) // if valid indices found
        {
            res = Data[index[0]].y;
            for (int i = 1; i < index.size(); i++)
            {
                float y = Data[index[i]].y;
                if (res > y)
                {
                    res = y;
                }
            }
        }
        return res;
    }

    float Max(float xmin, float xmax)
    {
        float res = 0;
        ImVector<int> index;
        index.reserve(MaxSize);
        for (int i = 0; i < Data.size(); i++) // find valid indices
        {
            float x = Data[i].x;
            if (x > xmin && x < xmax)
                index.push_back(i);
        }
        if (index.size() > 0) // if valid indices found
        {
            res = Data[index[0]].y;
            for (int i = 1; i < index.size(); i++)
            {
                float y = Data[index[i]].y;
                if (res < y)
                {
                    res = y;
                }
            }
        }
        return res;
    }
};

#if !defined(DEBUG_CONSOLE) && defined(IMGUI_ON_WINDOWS)
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow)
#else
int main(int, char **)
#endif
{
    SerEncoder *enc = nullptr;
    ScrollingBuffer *buf = new ScrollingBuffer(2000);
    if (buf == nullptr)
        throw std::runtime_error("Hands in the air!");
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    static int winx = 720, winy = 720;
    GLFWwindow *window = glfwCreateWindow(winx, winy, "Stepper Motor Testing Utility", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != NULL);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
        glfwGetWindowSize(window, &winx, &winy);
        ImGui::SetNextWindowSize(ImVec2(winx, winy), ImGuiCond_Always);
        ImGui::Begin("Control Panel");
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        {
            ImGui::BeginChild("Position Acquisition", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.5f, 100), ImGuiWindowFlags_None | ImGuiWindowFlags_ChildWindow);
            static bool ser_running = false;
            static char ser_name[50] = "/dev/ttyUSB0";
            static std::string errmsg = "";
            static bool err = false;
            ImGui::InputText("Serial Device", ser_name, IM_ARRAYSIZE(ser_name), ser_running ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AutoSelectAll);
            static bool ser_save = false;
            ImGui::Checkbox("Save Data##1", &ser_save);
            ImGui::SameLine(ImGui::GetWindowWidth() - 140);
            ImGui::PushItemWidth(-FLT_MIN);
            if (!ser_running)
            {
                if (ImGui::Button("Start Acquisition"))
                {
                    try
                    {
                        enc = new SerEncoder(ser_name, ser_save);
                    }
                    catch (const std::exception &e)
                    {
                        errmsg = e.what();
                        err = true;
                    }
                    if (enc != nullptr)
                        ser_running = true;
                }
            }
            else
            {
                if (ImGui::Button("Stop Acquisition"))
                {
                    delete enc;
                    enc = nullptr;
                    ser_running = false;
                }
            }
            ImGui::PopItemWidth();
            if (err)
            {
                ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - 50);
                ImGui::TextWrapped("%s", errmsg.c_str());
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (ImGui::Button("Clear##1"))
                    err = false;
            }
            ImGui::EndChild();
        }
        ImGui::PopStyleVar();

        ImGui::Separator();

        if (enc == nullptr || !enc->hasData())
        {
            ImGui::Text("Data not available");
        }
        else
        {
            static float t = 0, hist = 100;
            t += ImGui::GetIO().DeltaTime;
            static uint64_t ts;
            static SerEncoder_Flags flag;
            static SerEncoder_Flags lastflag;
            static int val;
            enc->getData(ts, val, flag);
            if (flag.val != 0)
                lastflag = flag;
            buf->AddPoint(t, (float)val);
            ImGui::Text("Data: %d - 0x%02x\tLast error: 0x%02x", val, flag.val, lastflag.val);
            ImPlot::SetNextPlotLimitsX(t - hist, t, ImGuiCond_Always);
            ImPlot::SetNextPlotLimitsY(buf->Min(t - hist, t), buf->Max(t - hist, t), ImGuiCond_Always);
            ImGui::SliderFloat("Points", &hist, 10, 1000, "%.1f");
            if (ImPlot::BeginPlot("Encoder Data", "Time", "Position", ImVec2(-1, 300)))
            {
                ImPlot::PlotLine("##Line", &buf->Data[0].x, &buf->Data[0].y, buf->Data.size(), buf->Offset, 2 * sizeof(float));
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // If you are using this code with non-legacy OpenGL header/contexts (which you should not, prefer using imgui_impl_opengl3.cpp!!),
        // you may need to backup/reset/restore current shader using the commented lines below.
        // GLint last_program;
        // glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        // glUseProgram(0);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        // glUseProgram(last_program);

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    if (enc != nullptr)
        delete enc;

    return 0;
}
