// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef __ANDROID__

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_format.h>
#include <base/posix/eintr_wrapper.h>
#include <imgui.h>
#include <imgui_impl_android.h>
#include <imgui_impl_opengl3.h>
#include <openssl/crypto.h>

#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <atomic>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <deque>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "core/logging.hpp"
#include "i18n/icu_util.hpp"
#include "core/utils.hpp"
#include "cli/cli_worker.hpp"
#include "config/config.hpp"
#include "crashpad_helper.hpp"

// Data
static EGLDisplay           g_EglDisplay = EGL_NO_DISPLAY;
static EGLSurface           g_EglSurface = EGL_NO_SURFACE;
static EGLContext           g_EglContext = EGL_NO_CONTEXT;
static std::atomic_bool     g_Initialized = false;
static std::string          g_IniFilename = "";
static int                  g_NotifyUnicodeFd[2] = {-1, -1};

// Forward declarations of helper functions
static void Init(struct android_app* app);
static void Shutdown();
static void MainLoopStep();
static int ShowSoftKeyboardInput();
static int GetAssetData(const char* filename, void** out_data);
// returning in host byte order
[[nodiscard]]
static int32_t GetIpAddress();
static int SetJavaThreadName(const std::string& thread_name);
[[maybe_unused]]
static int GetJavaThreadName(std::string* thread_name);
[[maybe_unused]]
static int GetNativeLibraryDirectory(std::string* result);
[[maybe_unused]]
static int GetCacheLibraryDirectory(std::string* result);
[[maybe_unused]]
static int GetDataLibraryDirectory(std::string* result);
[[maybe_unused]]
static int GetCurrentLocale(std::string* result);

extern "C"
JNIEXPORT void JNICALL Java_it_gui_yass_YassActivity_notifyUnicodeChar(JNIEnv *env, jobject obj, jint unicode);

static std::unique_ptr<Worker> g_worker;

enum StartState {
  STOPPED = 0,
  STOPPING,
  STARTING,
  STARTED,
};

static const char* state_to_str(StartState state) {
  switch(state) {
    case STOPPED:
      return "STOPPED";
    case STOPPING:
      return "STOPPING";
    case STARTING:
      return "STARTING";
    case STARTED:
      return "STARTED";
  }
}
static std::atomic<StartState> g_state(STOPPED);

static void ToggleWorker() {
  switch(g_state) {
    case STOPPED:
      g_state = STARTING;
      g_worker->Start([&](asio::error_code ec) {
        if (ec) {
          LOG(WARNING) << "Start Failed: " << ec;
          g_state = STOPPED;
        } else {
          LOG(WARNING) << "Started";
          config::SaveConfig();
          g_state = STARTED;
        }
      });
      break;
    case STOPPING:
      LOG(WARNING) << "Stopping, please wait";
      break;
    case STARTING:
      LOG(WARNING) << "Starting, please wait";
      break;
    case STARTED:
      g_state = STOPPING;
      g_worker->Stop([&]() {
        LOG(WARNING) << "Stopped";
        g_state = STOPPED;
      });
      break;
  }
}

static void WorkFunc() {
  while (true) {
    // Read all pending events.
    int ident;
    int events;
    struct android_poll_source* source;

    while ((ident=ALooper_pollAll(g_Initialized ? 67 /* limit to 15 fps */ : -1,
                                  nullptr, &events,
                                  (void**)&source)) >= 0) {

      // Process this event.
      if (source != nullptr) {
        source->process(a_app, source);
      }

      if (ident == LOOPER_ID_USER) {
        LOG(INFO) << "LOOPER_ID_USER";
      }

      // Check if we are exiting.
      if (a_app->destroyRequested != 0) {
        return;
      }
    }

    // Initiate a new frame
    MainLoopStep();
  }
}

// Main code
static void handleAppCmd(struct android_app* app, int32_t appCmd) {
  switch (appCmd)
  {
  case APP_CMD_SAVE_STATE:
    break;
  case APP_CMD_INIT_WINDOW:
    Init(app);
    break;
  case APP_CMD_TERM_WINDOW:
    Shutdown();
    break;
  case APP_CMD_GAINED_FOCUS:
  case APP_CMD_LOST_FOCUS:
    break;
  }
}

static int32_t handleInputEvent(struct android_app* app, AInputEvent* inputEvent) {
  return ImGui_ImplAndroid_HandleInputEvent(inputEvent);
}

static void ReadSequentialFromFd(int fd, std::deque<char> *out_vec) {
  std::array<char, 140> buf;
  while (true) {
    ssize_t ret = read(fd, buf.data(), buf.size());
    if (ret < 0 && errno == EINTR)
      continue;
    // system error
    if (ret < 0) {
      break;
    }
    // EOF, impossible
    if (ret == 0) {
      break;
    }
    out_vec->insert(out_vec->end(), buf.begin(), buf.begin() + ret);
  }
}

/* Invoked by ALooper to process a message */
// called from main thread
static int receiveUnicodeCharCallback(int fd, int events, void* user) {
  ImGuiIO& io = ImGui::GetIO();
  static std::deque<char> buffer;
  ReadSequentialFromFd(fd, &buffer);
  while (buffer.size() >= sizeof(jint)) {
    auto unicode_character = reinterpret_cast<jint*>(&*buffer.begin());
    io.AddInputCharacter(*unicode_character);
    for (unsigned int i = 0; i < sizeof(jint); ++i) {
      buffer.pop_front();
    }
  }

  return 1;
}

void Init(struct android_app* app) {
  if (g_Initialized)
    return;

  LOG(INFO) << "android: Initialize";

#ifdef HAVE_CRASHPAD
  // FIXME correct the path
  std::string lib_path;
  CHECK_EQ(0, GetNativeLibraryDirectory(&lib_path));
  CHECK(InitializeCrashpad(lib_path + "/libnative-lib.so"));
#endif

#ifdef HAVE_ICU
  if (!InitializeICU()) {
    LOG(WARNING) << "Failed to initialize icu component";
    return;
  }
#endif

  CRYPTO_library_init();

  SetJavaThreadName("Native Thread");
#ifndef NDEBUG
  std::string thread_name;
  GetJavaThreadName(&thread_name);
  LOG(INFO) << "Current Java Thread Name: " << thread_name;
#endif

  config::ReadConfigFileOption(0, nullptr);
  config::ReadConfig();

  // Create Yass Worker after ReadConfig
  g_worker = std::make_unique<Worker>();

  /* Call this from your main thread to set up the callback pipe. */
  int ret = pipe2(g_NotifyUnicodeFd, O_NONBLOCK | O_CLOEXEC);
  CHECK_NE(ret, -1) << "create unicode char notify fd failed";

  /* Register the file descriptor to listen on. */
  CHECK_EQ(1, ALooper_addFd(a_app->looper, g_NotifyUnicodeFd[0], LOOPER_ID_USER,
                            ALOOPER_EVENT_INPUT, receiveUnicodeCharCallback, nullptr));

  ANativeWindow_acquire(a_app->window);

  // Initialize EGL
  // This is mostly boilerplate code for EGL...
  {
    g_EglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_EglDisplay == EGL_NO_DISPLAY)
      LOG(ERROR) << "eglGetDisplay(EGL_DEFAULT_DISPLAY) returned EGL_NO_DISPLAY";

    if (eglInitialize(g_EglDisplay, 0, 0) != EGL_TRUE)
      LOG(ERROR) << "eglInitialize() returned with an error";

    const EGLint egl_attributes[] = { EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    EGLint num_configs = 0;
    if (eglChooseConfig(g_EglDisplay, egl_attributes, nullptr, 0, &num_configs) != EGL_TRUE)
      LOG(ERROR) << "eglChooseConfig() returned with an error";
    if (num_configs == 0)
      LOG(ERROR) << "eglChooseConfig() returned 0 matching config";

    // Get the first matching config
    EGLConfig egl_config;
    eglChooseConfig(g_EglDisplay, egl_attributes, &egl_config, 1, &num_configs);
    EGLint egl_format;
    eglGetConfigAttrib(g_EglDisplay, egl_config, EGL_NATIVE_VISUAL_ID, &egl_format);
    ANativeWindow_setBuffersGeometry(a_app->window, 0, 0, egl_format);

    const EGLint egl_context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    g_EglContext = eglCreateContext(g_EglDisplay, egl_config, EGL_NO_CONTEXT, egl_context_attributes);

    if (g_EglContext == EGL_NO_CONTEXT)
      LOG(ERROR) << "eglCreateContext() returned EGL_NO_CONTEXT";

    g_EglSurface = eglCreateWindowSurface(g_EglDisplay, egl_config, a_app->window, nullptr);
    eglMakeCurrent(g_EglDisplay, g_EglSurface, g_EglSurface, g_EglContext);
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  // Redirect loading/saving of .ini file to our location.
  // Make sure 'g_IniFilename' persists while we use Dear ImGui.
  g_IniFilename = std::string(app->activity->internalDataPath) + "/imgui.ini";
  io.IniFilename = g_IniFilename.c_str();;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplAndroid_Init(a_app->window);
  ImGui_ImplOpenGL3_Init("#version 300 es");

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
  // - Android: The TTF files have to be placed into the assets/ directory (android/app/src/main/assets), we use our GetAssetData() helper to retrieve them.

  // We load the default font with increased size to improve readability on many devices with "high" DPI.
  // FIXME: Put some effort into DPI awareness.
  // Important: when calling AddFontFromMemoryTTF(), ownership of font_data is transfered by Dear ImGui by default (deleted is handled by Dear ImGui), unless we set FontDataOwnedByAtlas=false in ImFontConfig
  //ImFontConfig font_cfg;
  //font_cfg.SizePixels = 22.0f;
  //io.Fonts->AddFontDefault(&font_cfg);
  void* font_data;
  int font_data_size;
  ImFont* font;
  //font_data_size = GetAssetData("segoeui.ttf", &font_data);
  //font = io.Fonts->AddFontFromMemoryTTF(font_data, font_data_size, 16.0f);
  //IM_ASSERT(font != nullptr);
  font_data_size = GetAssetData("DroidSans.ttf", &font_data);
  font = io.Fonts->AddFontFromMemoryTTF(font_data, font_data_size, 16.0f * 3.0f);
  IM_ASSERT(font != nullptr);
  //font_data_size = GetAssetData("Roboto-Medium.ttf", &font_data);
  //font = io.Fonts->AddFontFromMemoryTTF(font_data, font_data_size, 16.0f);
  //IM_ASSERT(font != nullptr);
  //font_data_size = GetAssetData("Cousine-Regular.ttf", &font_data);
  //font = io.Fonts->AddFontFromMemoryTTF(font_data, font_data_size, 15.0f);
  //IM_ASSERT(font != nullptr);
  //font_data_size = GetAssetData("ArialUni.ttf", &font_data);
  //font = io.Fonts->AddFontFromMemoryTTF(font_data, font_data_size, 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
  //IM_ASSERT(font != nullptr);

  // Arbitrary scale-up
  // FIXME: Put some effort into DPI awareness
  ImGui::GetStyle().ScaleAllSizes(3.0f);

  g_Initialized = true;

  LOG(INFO) << "android: Initialized";
}

void MainLoopStep()
{
  ImGuiIO& io = ImGui::GetIO();
  if (g_EglDisplay == EGL_NO_DISPLAY)
    return;

  // Our state
  // (we use static, which essentially makes the variable globals, as a convenience to keep the example code easy to follow)
  static bool show_option_window = false;
  static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Open on-screen (soft) input if requested by Dear ImGui
  static bool WantTextInputLast = false;
  if (io.WantTextInput && !WantTextInputLast)
    ShowSoftKeyboardInput();
  WantTextInputLast = io.WantTextInput;

  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplAndroid_NewFrame();
  ImGui::NewFrame();

  {
    std::vector<const char*> methods = {
#define XX(num, name, string) CRYPTO_##name##_STR,
CIPHER_METHOD_VALID_MAP(XX)
#undef XX
    };

    std::vector<cipher_method> methods_idxes = {
#define XX(num, name, string) CRYPTO_##name,
CIPHER_METHOD_VALID_MAP(XX)
#undef XX
    };
    static char server_host[140];
    static int server_port;
    static char username[140];
    static char password[140];
    static int method_idx = [&](cipher_method method) -> int {
      auto it = std::find(methods_idxes.begin(), methods_idxes.end(), method);
      if (it == methods_idxes.end()) {
        return 0;
      }
      return it - methods_idxes.begin();
    }(absl::GetFlag(FLAGS_method).method);
    static char local_host[140];
    static int local_port;
    static int timeout;
    static std::string ipaddress = asio::ip::make_address_v4(GetIpAddress()).to_string();
    static bool _init = [&]() -> bool {
      strcpy(server_host, absl::GetFlag(FLAGS_server_host).c_str());
      server_port = absl::GetFlag(FLAGS_server_port);
      strcpy(username, absl::GetFlag(FLAGS_username).c_str());
      strcpy(password, absl::GetFlag(FLAGS_password).c_str());
      strcpy(local_host, absl::GetFlag(FLAGS_local_host).c_str());
      local_port = absl::GetFlag(FLAGS_local_port);
      timeout = absl::GetFlag(FLAGS_connect_timeout);

      // FIXME the thread don't set correctly if it is done after
      // GetIpAddress call
      SetJavaThreadName("Native Thread");
      return true;
    }();

    ImGui::Begin("Yet Another Shadow Socket");

    ImGui::InputText("Server Host", server_host, IM_ARRAYSIZE(server_host));
    ImGui::InputInt("Server Port", &server_port);
    ImGui::InputText("Username", username, IM_ARRAYSIZE(username));
    ImGui::InputText("Password", password, IM_ARRAYSIZE(password),
                     ImGuiInputTextFlags_Password);

    ImGui::ListBox("Cipher", &method_idx,
                   &methods[0], static_cast<int>(methods.size()), 2);
    ImGui::InputText("Local Host", local_host, IM_ARRAYSIZE(local_host));
    ImGui::InputInt("Local Port", &local_port);
    ImGui::InputInt("Timeout", &timeout);
    ImGui::Text("Current Ip Address: %s", ipaddress.c_str());

    ImGui::Checkbox("Option Window", &show_option_window);

    if (ImGui::Button("Toggle Service")) {
      absl::SetFlag(&FLAGS_server_host, server_host);
      absl::SetFlag(&FLAGS_server_port, server_port);
      absl::SetFlag(&FLAGS_username, username);
      absl::SetFlag(&FLAGS_password, password);
      absl::SetFlag(&FLAGS_method, methods_idxes[method_idx]);
      absl::SetFlag(&FLAGS_local_host, local_host);
      absl::SetFlag(&FLAGS_local_port, local_port);
      absl::SetFlag(&FLAGS_connect_timeout, timeout);

      ToggleWorker();
    }
    ImGui::SameLine();
    ImGui::Text("Status = %s", state_to_str(g_state));

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
  }

  if (show_option_window)
  {
    static bool tcp_keep_alive = absl::GetFlag(FLAGS_tcp_keep_alive);
    static int tcp_keep_alive_cnt = absl::GetFlag(FLAGS_tcp_keep_alive_cnt);
    static int tcp_keep_alive_idle_timeout = absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
    static int tcp_keep_alive_interval = absl::GetFlag(FLAGS_tcp_keep_alive_interval);

    ImGui::Begin("Options Window", &show_option_window);
    ImGui::Checkbox("TCP keep alive", &tcp_keep_alive);
    ImGui::InputInt("The number of TCP keep-alive probes", &tcp_keep_alive_cnt);
    ImGui::InputInt("TCP keep alive after idle", &tcp_keep_alive_idle_timeout);
    ImGui::InputInt("TCP keep alive interval", &tcp_keep_alive_interval);
    if (ImGui::Button("Close Me")) {
      absl::SetFlag(&FLAGS_tcp_keep_alive, tcp_keep_alive);
      absl::SetFlag(&FLAGS_tcp_keep_alive_cnt, tcp_keep_alive_cnt);
      absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, tcp_keep_alive_idle_timeout);
      absl::SetFlag(&FLAGS_tcp_keep_alive_interval, tcp_keep_alive_interval);

      show_option_window = false;
    }
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  eglSwapBuffers(g_EglDisplay, g_EglSurface);
}

void Shutdown()
{
  if (!g_Initialized)
    return;
  LOG(INFO) << "android: Shutdown";

  g_Initialized = false;

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplAndroid_Shutdown();
  ImGui::DestroyContext();

  if (g_EglDisplay != EGL_NO_DISPLAY)
  {
    eglMakeCurrent(g_EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (g_EglContext != EGL_NO_CONTEXT)
      eglDestroyContext(g_EglDisplay, g_EglContext);

    if (g_EglSurface != EGL_NO_SURFACE)
      eglDestroySurface(g_EglDisplay, g_EglSurface);

    eglTerminate(g_EglDisplay);
  }

  g_EglDisplay = EGL_NO_DISPLAY;
  g_EglContext = EGL_NO_CONTEXT;
  g_EglSurface = EGL_NO_SURFACE;
  ANativeWindow_release(a_app->window);

  /* UnRegister the file descriptor to listen on. */
  CHECK_EQ(1, ALooper_removeFd(a_app->looper, g_NotifyUnicodeFd[0]));

  IGNORE_EINTR(close(g_NotifyUnicodeFd[0]));
  IGNORE_EINTR(close(g_NotifyUnicodeFd[1]));
  g_NotifyUnicodeFd[0] = -1;
  g_NotifyUnicodeFd[1] = -1;

  g_worker.reset();

  LOG(INFO) << "android: Shutdown finished";
}

// Helper functions

// Unfortunately, there is no way to show the on-screen input from native code.
// Therefore, we call ShowSoftKeyboardInput() of the main activity implemented in MainActivity.kt via JNI.
static int ShowSoftKeyboardInput()
{
  JavaVM* java_vm = a_app->activity->vm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass native_activity_clazz = java_env->GetObjectClass(a_app->activity->clazz);
  if (native_activity_clazz == nullptr)
    return -3;

  jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "showSoftInput", "()V");
  if (method_id == nullptr)
    return -4;

  java_env->CallVoidMethod(a_app->activity->clazz, method_id);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -5;

  return 0;
}

// Helper to retrieve data placed into the assets/ directory (android/app/src/main/assets)
static int GetAssetData(const char* filename, void** outData)
{
  int num_bytes = 0;
  AAsset* asset_descriptor = AAssetManager_open(a_app->activity->assetManager, filename, AASSET_MODE_BUFFER);
  if (asset_descriptor)
  {
    num_bytes = AAsset_getLength(asset_descriptor);
    *outData = IM_ALLOC(num_bytes);
    int64_t num_bytes_read = AAsset_read(asset_descriptor, *outData, num_bytes);
    AAsset_close(asset_descriptor);
    IM_ASSERT(num_bytes_read == num_bytes);
  }
  return num_bytes;
}

static int32_t GetIpAddress()
{
  JavaVM* java_vm = a_app->activity->vm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return 0;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return 0;

  jclass native_activity_clazz = java_env->GetObjectClass(a_app->activity->clazz);
  if (native_activity_clazz == nullptr)
    return 0;

  jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "getIpAddress", "()I");
  if (method_id == nullptr)
    return 0;

  jint ip_address = java_env->CallIntMethod(a_app->activity->clazz, method_id);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return 0;

  return ntohl(ip_address);
}

// Set native thread name inside
int SetJavaThreadName(const std::string& thread_name) {
  JavaVM* java_vm = a_app->activity->vm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass thread_clazz = java_env->FindClass("java/lang/Thread");
  if (thread_clazz == nullptr )
    return -3;

  jmethodID method_id = java_env->GetStaticMethodID(thread_clazz, "currentThread", "()Ljava/lang/Thread;");
  if (method_id == nullptr)
    return -4;

  jobject thread_obj = java_env->CallStaticObjectMethod(thread_clazz, method_id);

  if (thread_obj == nullptr)
    return -5;

  jstring thread_name_obj = java_env->NewStringUTF(thread_name.c_str());
  if (thread_name_obj == nullptr)
    return -6;

  jmethodID set_method_id = java_env->GetMethodID(thread_clazz, "setName", "(Ljava/lang/String;)V");
  if (set_method_id == nullptr)
    return -7;

  java_env->CallVoidMethod(thread_obj, set_method_id, thread_name_obj);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -8;

  return 0;
}

int GetJavaThreadName(std::string* thread_name) {
  JavaVM* java_vm = a_app->activity->vm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass thread_clazz = java_env->FindClass("java/lang/Thread");
  if (thread_clazz == nullptr )
    return -3;

  jmethodID method_id = java_env->GetStaticMethodID(thread_clazz, "currentThread", "()Ljava/lang/Thread;");
  if (method_id == nullptr)
    return -4;

  jobject thread_obj = java_env->CallStaticObjectMethod(thread_clazz, method_id);

  if (thread_obj == nullptr)
    return -5;

  jmethodID get_method_id = java_env->GetMethodID(thread_clazz, "getName", "()Ljava/lang/String;");
  if (get_method_id == nullptr)
    return -6;

  jobject name = java_env->CallObjectMethod(thread_obj, get_method_id);
  if (name == nullptr)
    return -7;

  const char* name_str;
  name_str = java_env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -8;

  *thread_name = name_str;

  java_env->ReleaseStringUTFChars((jstring)name, name_str);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -8;

  return 0;
}

int GetNativeLibraryDirectory(std::string* lib_path) {
  JavaVM* java_vm = a_app->activity->vm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass native_activity_clazz = java_env->GetObjectClass(a_app->activity->clazz);
  if (native_activity_clazz == nullptr)
    return -3;

  jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "getNativeLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -4;

  jobject name = java_env->CallObjectMethod(a_app->activity->clazz, method_id);
  if (name == nullptr)
    return -5;

  const char* name_str;
  name_str = java_env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -6;

  *lib_path = name_str;

  java_env->ReleaseStringUTFChars((jstring)name, name_str);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -7;

  return 0;
}

int GetCacheLibraryDirectory(std::string* cache_dir) {
  JavaVM* java_vm = a_app->activity->vm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass native_activity_clazz = java_env->GetObjectClass(a_app->activity->clazz);
  if (native_activity_clazz == nullptr)
    return -3;

  jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "getCacheLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -4;

  jobject name = java_env->CallObjectMethod(a_app->activity->clazz, method_id);
  if (name == nullptr)
    return -5;

  const char* name_str;
  name_str = java_env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -6;

  *cache_dir = name_str;

  java_env->ReleaseStringUTFChars((jstring)name, name_str);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -7;

  return 0;
}

int GetDataLibraryDirectory(std::string* data_dir) {
  JavaVM* java_vm = a_app->activity->vm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass native_activity_clazz = java_env->GetObjectClass(a_app->activity->clazz);
  if (native_activity_clazz == nullptr)
    return -3;

  jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "getDataLibraryDirectory", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -4;

  jobject name = java_env->CallObjectMethod(a_app->activity->clazz, method_id);
  if (name == nullptr)
    return -5;

  const char* name_str;
  name_str = java_env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -6;

  *data_dir = name_str;

  java_env->ReleaseStringUTFChars((jstring)name, name_str);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -7;

  return 0;
}

int GetCurrentLocale(std::string* locale_name) {
  JavaVM* java_vm = a_app->activity->vm;
  JNIEnv* java_env = nullptr;

  jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
  if (jni_return == JNI_ERR)
    return -1;

  jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
  if (jni_return != JNI_OK)
    return -2;

  jclass native_activity_clazz = java_env->GetObjectClass(a_app->activity->clazz);
  if (native_activity_clazz == nullptr)
    return -3;

  jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "getCurrentLocale", "()Ljava/lang/String;");
  if (method_id == nullptr)
    return -4;

  jobject name = java_env->CallObjectMethod(a_app->activity->clazz, method_id);
  if (name == nullptr)
    return -5;

  const char* name_str;
  name_str = java_env->GetStringUTFChars((jstring)name, nullptr);
  if (name_str == nullptr)
    return -6;

  *locale_name = name_str;

  java_env->ReleaseStringUTFChars((jstring)name, name_str);

  jni_return = java_vm->DetachCurrentThread();
  if (jni_return != JNI_OK)
    return -7;

  return 0;
}

// called from java thread
JNIEXPORT void JNICALL Java_it_gui_yass_YassActivity_notifyUnicodeChar(JNIEnv *env, jobject obj, jint unicode) {
  const uint8_t *remaining_buffer = reinterpret_cast<uint8_t*>(&unicode);
  ssize_t remaining_length = sizeof(unicode);
  while (g_Initialized && remaining_length > 0) {
    ssize_t written = write(g_NotifyUnicodeFd[1], remaining_buffer, remaining_length);
    if (written < 0 && (errno == EINTR || errno == EAGAIN))
      continue;
    if (written < 0) {
      PLOG(WARNING) << "notifyUnicode write error";
      break;
    }
    DCHECK_LE(written, remaining_length);
    remaining_buffer += written;
    remaining_length -= written;
    break;
  }
}

void android_main(android_app* app) {
  CHECK(app);
  app->onAppCmd = handleAppCmd;
  app->onInputEvent = handleInputEvent;
  a_app = app;

  // before any log calls
  std::string cache_path;
  CHECK_EQ(0, GetCacheLibraryDirectory(&cache_path));
  a_cache_dir = cache_path;

  std::string exe_path;
  GetExecutablePath(&exe_path);
  SetExecutablePath(exe_path);

  std::string data_path;
  CHECK_EQ(0, GetDataLibraryDirectory(&data_path));
  a_data_dir = data_path;

  LOG(INFO) << "exe path: " << exe_path;
  LOG(INFO) << "cache dir: " << cache_path;
  LOG(INFO) << "data dir: " << data_path;
  LOG(INFO) << "internal data path: " << a_app->activity->internalDataPath;

  // possible values: en_US, zh_SG_#Hans, zh_CN_#Hans, zh_HK_#Hant
  std::string locale_name;
  CHECK_EQ(0, GetCurrentLocale(&locale_name));
  LOG(INFO) << "current locale: " << locale_name;

  WorkFunc();
}

#endif // __ANDROID__
