// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <stdio.h>
#include <math.h>
#include <string>
#include <vector>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif
const int slave_num = 2;

static void glfw_error_callback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
void DemoHeader(const char* label, void(*demo)()) {
  if (ImGui::TreeNodeEx(label)) {
    demo();
    ImGui::TreePop();
  }
}
template <typename T>
inline T RandomRange(T min, T max) {
  T scale = rand() / (T) RAND_MAX;
  return min + scale * ( max - min );
}

ImVec4 RandomColor() {
  ImVec4 col;
  col.x = RandomRange(0.0f,1.0f);
  col.y = RandomRange(0.0f,1.0f);
  col.z = RandomRange(0.0f,1.0f);
  col.w = 1.0f;
  return col;
}
// convenience struct to manage DND items; do this however you like
class PlotItem {
public:
    int              Plt;
    ImAxis           Yax;
    ImVector<ImVec2> Data;
    ImVec4           Color;
    std::string Label;  // std::string으로 변경
    PlotItem(const std::string& label) : Label(label) {
      Plt = 0;
      Yax = ImAxis_Y1;
      if (label == "pos"){
        Color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
      } else if (label == "vel"){
        Color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
      } else if (label == "torque"){
        Color = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
      } else if (label == "torque_ref"){
        Color = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
      } else if (label == "mode"){
        Color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
      } else if (label == "error"){
        Color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
      }
      Data.reserve(1001);

      for (int k = 0; k < 1001; ++k) {
        float t = k * 1.0f / 999;
        Data.push_back(ImVec2(t, 0.5f + 0.5f * sinf(2*3.14f*t)));
      }
      // 생성자에서 std::string을 직접 할당
    }
    void Reset() { Plt = 0; Yax = ImAxis_Y1; }
};
class SlaveData{
public:
    std::vector<PlotItem> items;
    SlaveData(){
      PlotItem item = PlotItem("pos");
      items.emplace_back(item);
      PlotItem item1 = PlotItem("vel");
      items.emplace_back(item1);
      PlotItem item2 = PlotItem("torque");
      items.emplace_back(item2);
      PlotItem item3 = PlotItem("torque_ref");
      items.emplace_back(item3);
      PlotItem item4 = PlotItem("mode");
      items.emplace_back(item4);
      PlotItem item5 = PlotItem("error");
      items.emplace_back(item5);
    }
    int element_num = 6;
    bool is_pop = false;
};
SlaveData slave;
// utility structure for realtime plot
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer(int max_size = 2000) {
      MaxSize = max_size;
      Offset  = 0;
      Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y) {
      if (Data.size() < MaxSize)
        Data.push_back(ImVec2(x,y));
      else {
        Data[Offset] = ImVec2(x,y);
        Offset =  (Offset + 1) % MaxSize;
      }
    }
    void Erase() {
      if (Data.size() > 0) {
        Data.shrink(0);
        Offset  = 0;
      }
    }
};

// utility structure for realtime plot
struct RollingBuffer {
    float Span;
    ImVector<ImVec2> Data;
    RollingBuffer() {
      Span = 10.0f;
      Data.reserve(2000);
    }
    void AddPoint(float x, float y) {
      float xmod = fmodf(x, Span);
      if (!Data.empty() && xmod < Data.back().x)
        Data.shrink(0);
      Data.push_back(ImVec2(xmod, y));
    }
};

void Demo_DragAndDrop() {


//  const int         k_dnd = 20;
//  static MyDndItem  dnd[k_dnd];
//  static MyDndItem* dndx = nullptr; // for plot 2
//  static MyDndItem* dndy = nullptr; // for plot 2

  // child window to serve as initial source for our DND items
  ImGui::BeginChild("DND_LEFT",ImVec2(100,400));
  if (ImGui::Button("Clear Graph")) {
    for (int k = 0; k < slave.items.size(); ++k)
      slave.items[k].Reset();
  }
  for (int k = 0; k < slave.items.size(); ++k){
    if (slave.items[k].Plt > 0)
      continue;
    ImPlot::ItemIcon(slave.items[k].Color); ImGui::SameLine();
    ImGui::Selectable(slave.items[k].Label.c_str(), false, 0, ImVec2(100, 0));
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      ImGui::SetDragDropPayload("MY_DND", &k, sizeof(int));
      ImPlot::ItemIcon(slave.items[k].Color); ImGui::SameLine();
      ImGui::TextUnformatted(slave.items[k].Label.c_str());
      ImGui::EndDragDropSource();
    }
  }
  ImGui::EndChild();
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MY_DND")) {
      int i = *(int*)payload->Data; slave.items[i].Reset();
    }
    ImGui::EndDragDropTarget();
  }

  ImGui::SameLine();
  ImGui::BeginChild("DND_RIGHT",ImVec2(-1,800));
  static float t = 0;
  static float graph_time = 0;
  t += ImGui::GetIO().DeltaTime;
  static float history = 10.0f;
  static bool graph_moving = true;
  ImGui::SliderFloat("History",&history,1,30,"%.1f s");
  ImGui::SameLine();
  static ScrollingBuffer sdata1, sdata2;
  ImVec2 mouse = ImGui::GetMousePos();
  static ImPlotRect rect(graph_time - 0.75*history, graph_time-0.25*history,0,0.5);

  if (graph_moving){
    if (ImGui::Button("Stop")) {
      graph_moving = false;
      rect.X.Min = graph_time - 0.75*history;
      rect.X.Max = graph_time - 0.25*history;
      rect.Y.Min = 0;
      rect.Y.Max = 0.5;
    }
    graph_time = t;
    sdata1.AddPoint(graph_time, mouse.x * 0.0005f);
    sdata2.AddPoint(graph_time, mouse.y * 0.0005f);
  }else {
    if (ImGui::Button("Resume")) {
      graph_moving = true;
    }
  }

  // plot 1 (time series)
  ImPlotAxisFlags flags = ImPlotAxisFlags_None;
  if (ImPlot::BeginPlot("##DND1", ImVec2(-1,195))) {
    ImPlot::SetupAxis(ImAxis_X1, nullptr, flags);
    ImPlot::SetupAxis(ImAxis_Y1, "Value", flags);

    ImPlot::SetupAxisLimits(ImAxis_X1,graph_time - history, graph_time, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1,0,1);
    for (int k = 0; k < slave.items.size(); ++k) {
      if (slave.items[k].Plt == 1 && slave.items[k].Data.size() > 0) {
        ImPlot::SetAxis(slave.items[k].Yax);
        ImPlot::SetNextLineStyle(slave.items[k].Color);
        ImPlot::PlotLine(slave.items[k].Label.c_str(), &sdata2.Data[0].x, &sdata2.Data[0].y, sdata2.Data.size(), 0, sdata2.Offset, 2*sizeof(float));
        // allow legend item labels to be DND sources
        if (ImPlot::BeginDragDropSourceItem(slave.items[k].Label.c_str())) {
          ImGui::SetDragDropPayload("MY_DND", &k, sizeof(int));
          ImPlot::ItemIcon(slave.items[k].Color); ImGui::SameLine();
          ImGui::TextUnformatted(slave.items[k].Label.c_str());
          ImPlot::EndDragDropSource();
        }
      }
    }
    // allow the main plot area to be a DND target
    if (ImPlot::BeginDragDropTargetPlot()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MY_DND")) {
        int i = *(int*)payload->Data; slave.items[i].Plt = 1; slave.items[i].Yax = ImAxis_Y1;
      }
      ImPlot::EndDragDropTarget();
    }
    ImPlot::EndPlot();
  }

  static ImPlotDragToolFlags flag2 = ImPlotDragToolFlags_None;
  bool clicked = false;
  bool hovered = false;
  bool held = false;

  if (ImPlot::BeginPlot("##DND2", ImVec2(-1,195))) {
    ImPlot::SetupAxis(ImAxis_X1, nullptr, flags|ImPlotAxisFlags_Lock);
    ImPlot::SetupAxis(ImAxis_Y1, "Value", flags);
    ImPlot::SetupAxisLimits(ImAxis_X1,graph_time - history, graph_time, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1,0,1);
    for (int k = 0; k < slave.items.size(); ++k) {
      if (slave.items[k].Plt == 2 && slave.items[k].Data.size() > 0) {
        ImPlot::SetAxis(slave.items[k].Yax);
        ImPlot::SetNextLineStyle(slave.items[k].Color);
        ImPlot::PlotLine(slave.items[k].Label.c_str(), &sdata2.Data[0].x, &sdata2.Data[0].y, sdata2.Data.size(), 0, sdata2.Offset, 2*sizeof(float));
        ImPlot::DragRect(0,&rect.X.Min,&rect.Y.Min,&rect.X.Max,&rect.Y.Max,ImVec4(1,1,1,0.5),flags, &clicked, &hovered, &held);

        // allow legend item labels to be DND sources
        if (ImPlot::BeginDragDropSourceItem(slave.items[k].Label.c_str())) {
          ImGui::SetDragDropPayload("MY_DND", &k, sizeof(int));
          ImPlot::ItemIcon(slave.items[k].Color); ImGui::SameLine();
          ImGui::TextUnformatted(slave.items[k].Label.c_str());
          ImPlot::EndDragDropSource();
        }
      }
    }
    // allow the main plot area to be a DND target
    if (ImPlot::BeginDragDropTargetPlot()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MY_DND")) {
        int i = *(int*)payload->Data; slave.items[i].Plt = 2; slave.items[i].Yax = ImAxis_Y1;
      }
      ImPlot::EndDragDropTarget();
    }
    ImPlot::EndPlot();
  }
  ImVec4 bg_col = held ? ImVec4(0.5f,0,0.5f,1) : (hovered ? ImVec4(0.25f,0,0.25f,1) : ImPlot::GetStyle().Colors[ImPlotCol_PlotBg]);
  ImPlot::PushStyleColor(ImPlotCol_PlotBg, bg_col);
  if (ImPlot::BeginPlot("##rect",ImVec2(-1,250), ImPlotFlags_CanvasOnly)) {
    ImPlot::SetupAxesLimits(rect.X.Min, rect.X.Max, rect.Y.Min, rect.Y.Max, ImGuiCond_Always);

    for (int k = 0; k < slave.items.size(); ++k) {
      if (slave.items[k].Plt == 2 && slave.items[k].Data.size() > 0) {
        ImPlot::SetAxis(slave.items[k].Yax);
        ImPlot::SetNextLineStyle(slave.items[k].Color);
        ImPlot::PlotLine(slave.items[k].Label.c_str(), &sdata2.Data[0].x, &sdata2.Data[0].y, sdata2.Data.size(), 0, sdata2.Offset, 2*sizeof(float));

      }
    }
    ImPlot::EndPlot();
  }
  ImPlot::PopStyleColor();
  ImGui::EndChild();
}
// Main code
int main(int, char**)
{
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

  // Create window with graphics context
  GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
  if (window == nullptr)
    return 1;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();

  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
  //io.ConfigViewportsNoAutoMerge = true;
  //io.ConfigViewportsNoTaskBarIcon = true;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
  ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
  // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
  // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
  // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
  //io.Fonts->AddFontDefault();
  //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
  //IM_ASSERT(font != nullptr);

  // Our state
  bool show_graph_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Main loop
#ifdef __EMSCRIPTEN__
  // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
  while (!glfwWindowShouldClose(window))
#endif
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).


    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
      static float f = 0.0f;

      ImGui::Begin("Sample Motor Control App");                          // Create a window called "Hello, world!" and append into it.
      ImGui::Text("# of slaves = %d", slave_num);
      ImGui::Text("Slave Status");

      static int temp_mode_idx[12] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

      static int temp_control_mode[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
      static int control_mode[12] = {0,0,0,0,0,0,0,0,0,0,0,0};

      static double temp_ref[12] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
      static double torque_ref[12] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};

      for (int i=0; i<slave_num; i++){
        ImGui::BulletText("Slave %d, Control mode: ",  i);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        std::string element = "##Control Mode("+std::to_string(i ) +")";
        if(ImGui::Combo(element.c_str(), &temp_mode_idx[i], "PWM_DUTY_ZERO\0COMMUTATION_ID\0TORQUE_CONTROL\0\0")){
          switch (temp_mode_idx[i]){
            case 0: temp_control_mode[i] = 0; break;
            case 1: temp_control_mode[i] = 15; break;
            case 2: temp_control_mode[i] = 10; break;
            default: temp_control_mode[i] = 0; break;
          }
        }
        ImGui::SameLine();
        ImGui::Text(" Torque ref:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(30);
        std::string element_ref = "##Target("+std::to_string(i ) +")";
        ImGui::InputDouble(element_ref.c_str(), &temp_ref[i], -20.0f, 20.0f, "%.0f");

        ImGui::SameLine();
        std::string arrow_ref = "##Arrow("+std::to_string(i ) +")";
        if (ImGui::ArrowButton(arrow_ref.c_str(), ImGuiDir_Right)) {
          torque_ref[i] = temp_ref[i];
          control_mode[i] = temp_control_mode[i];
        }

        ImGui::SameLine();
        std::string stop_text = "Stop##("+std::to_string(i ) +")";
        if (ImGui::SmallButton(stop_text.c_str())){
          torque_ref[i] = 0.0;
          control_mode[i] = 0;
        }
        ImGui::Text("    Pos: %.2f, Vel: %.2f, Torque: %.2f, Mode: %d, Error: %d", f*-1, f*-1, torque_ref[i], control_mode[i], slave_num);
      }


      ImGui::End();
    }



    ImGui::Begin("ImPlot Demo", &show_graph_window, ImGuiWindowFlags_MenuBar);
    {
      Demo_DragAndDrop();

    }
//    {
//      static float x_data[512];
//      static float y_data1[512];
//      static float sampling_freq = 44100;
//      static float freq = 500;
//      bool clicked = false;
//      bool hovered = false;
//      bool held = false;
//      for (size_t i = 0; i < 512; ++i) {
//        const float t = i / sampling_freq;
//        x_data[i] = t;
//        const float arg = 2 * 3.14f * freq * t;
//        y_data1[i] = sinf(arg);
//      }
//      ImGui::BulletText("Click and drag the edges, corners, and center of the rect.");
//      ImGui::BulletText("Double click edges to expand rect to plot extents.");
//      static ImPlotRect rect(0.0025,0.0045,0,0.5);
//      static ImPlotDragToolFlags flags = ImPlotDragToolFlags_None;
//      ImGui::CheckboxFlags("NoCursors", (unsigned int*)&flags, ImPlotDragToolFlags_NoCursors); ImGui::SameLine();
//      ImGui::CheckboxFlags("NoFit", (unsigned int*)&flags, ImPlotDragToolFlags_NoFit); ImGui::SameLine();
//      ImGui::CheckboxFlags("NoInput", (unsigned int*)&flags, ImPlotDragToolFlags_NoInputs);
//
//      if (ImPlot::BeginPlot("##Main",ImVec2(-1,250))) {
//        ImPlot::SetupAxes(nullptr,nullptr,ImPlotAxisFlags_NoTickLabels,ImPlotAxisFlags_NoTickLabels);
//        ImPlot::SetupAxesLimits(0,0.01,-1,1);
//        ImPlot::PlotLine("Signal 1", x_data, y_data1, 512);
//        ImPlot::DragRect(0,&rect.X.Min,&rect.Y.Min,&rect.X.Max,&rect.Y.Max,ImVec4(1,1,1,0.5),flags, &clicked, &hovered, &held);
//        ImPlot::EndPlot();
//      }
//      ImVec4 bg_col = held ? ImVec4(0.5f,0,0.5f,1) : (hovered ? ImVec4(0.25f,0,0.25f,1) : ImPlot::GetStyle().Colors[ImPlotCol_PlotBg]);
//      ImPlot::PushStyleColor(ImPlotCol_PlotBg, bg_col);
//      if (ImPlot::BeginPlot("##rect",ImVec2(-1,250), ImPlotFlags_CanvasOnly)) {
//        ImPlot::SetupAxes(nullptr,nullptr,ImPlotAxisFlags_NoDecorations,ImPlotAxisFlags_NoDecorations);
//        ImPlot::SetupAxesLimits(rect.X.Min, rect.X.Max, rect.Y.Min, rect.Y.Max, ImGuiCond_Always);
//        ImPlot::PlotLine("Signal 1", x_data, y_data1, 512);
//        ImPlot::EndPlot();
//      }
//      ImPlot::PopStyleColor();
//      ImGui::Text("Rect is %sclicked, %shovered, %sheld", clicked ? "" : "not ", hovered ? "" : "not ", held ? "" : "not ");
//    }


    ImGui::End();


    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
      GLFWwindow* backup_current_context = glfwGetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window);
  }
#ifdef __EMSCRIPTEN__
  EMSCRIPTEN_MAINLOOP_END;
#endif

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();

  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
