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
#include "Adafruit/MotorShield.hpp"
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
    static int winx = 800, winy = 640;
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
    bool force_stop = false;
    bool ser_running = false;
    bool mot_save_data = false;
    bool ser_save = false;
    static int framectr = 0;


    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        framectr++;
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

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        glfwGetWindowSize(window, &winx, &winy);
        ImGui::SetNextWindowSize(ImVec2(winx, winy), ImGuiCond_Always);
        ImGui::Begin("Control Panel", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        {
            ImGui::BeginChild("Position Acquisition", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.36f, 220), ImGuiWindowFlags_None | ImGuiWindowFlags_ChildWindow);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0.6, 0.6, 1));
            ImGui::Text("Encoder Control");
            ImGui::PopStyleColor();
            ImGui::Separator();
            static char ser_name[50] = "/dev/ttyUSB0";
            static char save_file[20] = "encoder";
            static std::string errmsg = "";
            static bool err = false;
            ImGui::InputText("Serial Port", ser_name, IM_ARRAYSIZE(ser_name), ser_running ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AutoSelectAll);
            ImGui::InputText("Save File", save_file, IM_ARRAYSIZE(save_file), ser_running ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AutoSelectAll);
            ImGui::Checkbox("Save Data##1", &ser_save);
            ImGui::SameLine(ImGui::GetWindowWidth() - 140);
            ImGui::PushItemWidth(-FLT_MIN);
            if (!ser_running)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.65, 0, 1));
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
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65, 0.0, 0.0, 1));
                if (ImGui::Button("Stop Acquisition") || force_stop)
                {
                    force_stop = false;
                    if (enc != nullptr)
                        delete enc;
                    enc = nullptr;
                    ser_running = false;
                }
            }
            ImGui::PopStyleColor();
            ImGui::PopItemWidth();
            ImGui::Separator();
            if (err)
            {
                ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - 50);
                ImGui::TextWrapped("%s", errmsg.c_str());
                ImGui::PopItemWidth();
                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                if (ImGui::Button("Clear##1"))
                    err = false;
            }
            ImGui::EndChild();
        }
        ImGui::SameLine();
        {
            ImGui::BeginChild("Motor Control", ImVec2(-1, 220), ImGuiWindowFlags_None | ImGuiWindowFlags_ChildWindow);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0.6, 0.6, 1));
            ImGui::Text("Motor Control");
            ImGui::PopStyleColor();
            ImGui::Separator();
            static std::string errmsg = "";
            static bool err = false;
            static bool afms_ready = false;
            static bool moving = false;
            static Adafruit::MotorShield *afms = nullptr;
            static Adafruit::StepperMotor *mot = nullptr;
            static int bus = 1, address = 0x60, port = 0, stp_rev = 200;
            static char addrstr[50] = "0x60";
            const char *portlist[] = {(char *)"1", (char *)"2"};
            static Adafruit::MotorDir dir = Adafruit::MotorDir::FORWARD;
            static Adafruit::MotorStyle style = Adafruit::MotorStyle::MICROSTEP;
            static Adafruit::MicroSteps msteps = Adafruit::MicroSteps::STEP64;
            if (mot != nullptr)
            {
                moving = mot->isMoving();
            }
            ImGui::SetNextItemWidth(ImGui::GetWindowContentRegionWidth() - 100); // create the table
            if (ImGui::BeginTable("##split_add_num", 4, ImGuiTableFlags_None))
            {
                ImGui::TableNextColumn();
                ImGui::Text("I2C Bus");
                ImGui::TableNextColumn();
                ImGui::Text("Address");
                ImGui::TableNextColumn();
                ImGui::Text("Port");
                ImGui::TableNextColumn();
                ImGui::Text("Steps/rev");
                ImGui::TableNextColumn();
                if (ImGui::InputInt("##i2cbus", &bus, 0, 0, afms_ready ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    if (bus < 0)
                        bus = 0;
                }
                ImGui::TableNextColumn();
                if (ImGui::InputText("##i2caddr", addrstr, afms_ready ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    address = strtol(addrstr, NULL, 16);
                    if (address < 7)
                        address = 7;
                    if (address > 128)
                        address = 128;
                }
                ImGui::TableNextColumn();
                ImGui::Combo("##portsel", &port, portlist, IM_ARRAYSIZE(portlist));
                ImGui::TableNextColumn();
                if (ImGui::InputInt("##stepsperrev", &stp_rev, 0, 0, afms_ready ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    if (stp_rev < 0)
                        stp_rev = 50;
                    if (stp_rev > 400)
                        stp_rev = 400;
                }
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();

                if (!moving && !afms_ready) // can open afms
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.6, 0, 1));
                    if (ImGui::Button("Initialize"))
                    {
                        try
                        {
                            afms = new Adafruit::MotorShield(address, bus);
                            afms->begin();
                            mot = afms->getStepper(stp_rev, port + 1, msteps);
                        }
                        catch (const std::exception &e)
                        {
                            errmsg = e.what();
                            err = true;
                            if (afms != nullptr)
                            {
                                delete afms;
                                afms = nullptr;
                            }
                            if (mot != nullptr)
                            {
                                mot = nullptr;
                            }
                        }
                        if (afms != nullptr)
                        {
                            afms_ready = true;
                        }
                    }
                    ImGui::PopStyleColor();
                }
                else if (!moving) // can close afms
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.066, 0.66, 0.6, 1));
                    if (ImGui::Button("Disconnect"))
                    {
                        delete afms;
                        afms = nullptr;
                        afms_ready = false;
                    }
                    ImGui::PopStyleColor();
                }
                else // button disabled
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3, 0.3, 0.3, 1));
                    ImGui::Button("Disconnect");
                    ImGui::PopStyleColor();
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
            // motor control
            {
                static int nsteps = 0;
                static int motStyle = 2; // default
                static char *motStyleStr[] = {(char *)"Single", (char *)"Double", (char *)"Microstep"};
                static int motDir = 0; // default
                static char *motDirStr[] = {(char *)"Forward", (char *)"Backward"};
                static int nMicroSteps = 3; // default
                static char *microStepStr[] = {(char *)"8", (char *)"16", (char *)"32", (char *)"64", (char *)"128", (char *)"256", (char *)"512"};
                static float speed = 0.1; // motor speed in rpm
                static int mot_started = 0;
                bool inputReady = !moving && afms_ready;
                if ((mot_started - framectr) > 100 && !moving && ser_save && mot_save_data) // after 100 frames of pressing start
                    force_stop = true;
                if (ImGui::BeginTable("##split_mot_props", 4, ImGuiTableFlags_None))
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("Steps");
                    ImGui::TableNextColumn();
                    ImGui::Text("Direction");
                    ImGui::TableNextColumn();
                    ImGui::Text("Style");
                    ImGui::TableNextColumn();
                    ImGui::Text("Microsteps");

                    ImGui::TableNextColumn();
                    if (ImGui::InputInt("##NumSteps", &nsteps, 0, 0, inputReady ? ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly))
                    {
                        if (nsteps < 0)
                            nsteps = 0;
                        if (nsteps > UINT16_MAX)
                            nsteps = 0;
                    }

                    ImGui::TableNextColumn();
                    static int _motDir;
                    _motDir = motDir;
                    if (ImGui::Combo("##MotorDir", &motDir, motDirStr, IM_ARRAYSIZE(motDirStr)))
                    {
                        if (!inputReady)
                        {
                            motDir = _motDir;
                        }
                        if (motDir == 0)
                            dir = Adafruit::MotorDir::FORWARD;
                        else if (motDir == 1)
                            dir = Adafruit::MotorDir::BACKWARD;
                        else
                        {
                            dir = Adafruit::MotorDir::FORWARD;
                            motDir = 0;
                        }
                    }

                    ImGui::TableNextColumn();
                    static int _motStyle;
                    _motStyle = motStyle;
                    if (ImGui::Combo("##MotorStyle", &motStyle, motStyleStr, IM_ARRAYSIZE(motStyleStr)))
                    {
                        if (!inputReady)
                        {
                            motStyle = _motStyle;
                        }
                        if (motStyle == 0)
                        {
                            style = Adafruit::MotorStyle::SINGLE;
                        }
                        else if (motStyle == 1)
                        {
                            style = Adafruit::MotorStyle::DOUBLE;
                        }
                        else if (motStyle == 2)
                        {
                            style = Adafruit::MotorStyle::MICROSTEP;
                        }
                        else
                        {
                            style = Adafruit::MotorStyle::MICROSTEP;
                            motStyle = 2;
                        }
                    }

                    ImGui::TableNextColumn();
                    static int _nMicroSteps;
                    _nMicroSteps = nMicroSteps;
                    static bool microStepsApplied = false;
                    if (ImGui::Combo("##MicroSteps", &nMicroSteps, microStepStr, IM_ARRAYSIZE(microStepStr)))
                    {
                        if (!inputReady)
                        {
                            nMicroSteps = _nMicroSteps;
                        }
                        switch (nMicroSteps)
                        {
                        case 0:
                            msteps = Adafruit::MicroSteps::STEP8;
                            break;
                        case 1:
                            msteps = Adafruit::MicroSteps::STEP16;
                            break;
                        case 2:
                            msteps = Adafruit::MicroSteps::STEP32;
                            break;
                        case 3:
                            msteps = Adafruit::MicroSteps::STEP64;
                            break;
                        case 4:
                            msteps = Adafruit::MicroSteps::STEP128;
                            break;
                        case 5:
                            msteps = Adafruit::MicroSteps::STEP256;
                            break;
                        case 6:
                            msteps = Adafruit::MicroSteps::STEP512;
                            break;
                        default:
                            nMicroSteps = 3;
                            msteps = Adafruit::MicroSteps::STEP64;
                            break;
                        }
                        microStepsApplied = false;
                    }
                    if (microStepsApplied == false && inputReady)
                    {
                        mot->setStep(msteps);
                        microStepsApplied = true;
                    }
                    ImGui::EndTable();
                }
                ImGui::PushItemWidth(80);
                if (ImGui::InputFloat("Speed##mot", &speed, 0, 0, "%.4f", inputReady ? ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly))
                {
                    if (speed < 0)
                        speed = 0.1;
                    if (speed > 100)
                        speed = 100;
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                static char motFilePref[20] = "motor";
                ImGui::PushItemWidth(100);
                ImGui::InputText("Prefix##mot", motFilePref, IM_ARRAYSIZE(motFilePref), inputReady ? ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly);
                ImGui::PopItemWidth();
                ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 160);
                ImGui::Checkbox("Save Data", &mot_save_data);
                ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 50);
                if (!afms_ready && !moving)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3, 0.3, 0.3, 1));
                    ImGui::Button("Start");
                }
                else if (afms_ready && !moving)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.6, 0, 1));
                    if (ImGui::Button("Start")) // start moving
                    {
                        try
                        {
                            mot->setSpeed(speed);
                            mot->step(nsteps, dir, style, false, mot_save_data, motFilePref);
                        }
                        catch (const std::exception &e)
                        {
                            errmsg = e.what();
                            err = true;
                        }
                        mot_started = framectr;
                    }
                }
                else if (afms_ready && moving)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0, 0, 1));
                    if (ImGui::Button("Stop"))
                    {
                        mot->stopMotor();
                    }
                }
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
            if (err)
            {
                ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - 50);
                ImGui::TextWrapped("%s", errmsg.c_str());
                ImGui::PopItemWidth();
                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                if (ImGui::Button("Clear##2"))
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
