#include "include/window_manager/window_manager_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <codecvt>
#include <map>
#include <memory>
#include <sstream>

namespace {
    static bool window_fullscreened = false;
    static RECT window_before_fullscreen;

    class WindowManagerPlugin : public flutter::Plugin {
    public:
        static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

        WindowManagerPlugin();

        virtual ~WindowManagerPlugin();

    private:
        // Called when a method is called on this plugin's channel from Dart.
        void HandleMethodCall(
            const flutter::MethodCall<flutter::EncodableValue>& method_call,
            std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
    };

    // static
    void WindowManagerPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarWindows* registrar) {
        auto channel =
            std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
                registrar->messenger(), "window_manager",
                &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<WindowManagerPlugin>();

        channel->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

        registrar->AddPlugin(std::move(plugin));
    }

    WindowManagerPlugin::WindowManagerPlugin() {}

    WindowManagerPlugin::~WindowManagerPlugin() {}

    void GetBounds(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        const flutter::EncodableMap& args = std::get<flutter::EncodableMap>(*method_call.arguments());

        double devicePixelRatio = std::get<double>(args.at(flutter::EncodableValue("devicePixelRatio")));

        flutter::EncodableMap resultMap = flutter::EncodableMap();
        HWND mainWindow = GetActiveWindow();
        RECT rect;
        if (GetWindowRect(mainWindow, &rect))
        {
            double x = rect.left / devicePixelRatio * 1.0f;
            double y = rect.top / devicePixelRatio * 1.0f;
            double width = (rect.right - rect.left) / devicePixelRatio * 1.0f;
            double height = (rect.bottom - rect.top) / devicePixelRatio * 1.0f;

            resultMap[flutter::EncodableValue("x")] = flutter::EncodableValue(x);
            resultMap[flutter::EncodableValue("y")] = flutter::EncodableValue(y);
            resultMap[flutter::EncodableValue("width")] = flutter::EncodableValue(width);
            resultMap[flutter::EncodableValue("height")] = flutter::EncodableValue(height);
        }
        result->Success(flutter::EncodableValue(resultMap));
    }

    void SetBounds(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        const flutter::EncodableMap& args = std::get<flutter::EncodableMap>(*method_call.arguments());

        double devicePixelRatio = std::get<double>(args.at(flutter::EncodableValue("devicePixelRatio")));
        // double x = std::get<double>(args.at(flutter::EncodableValue("origin_x")));
        // double y = std::get<double>(args.at(flutter::EncodableValue("origin_y")));
        double width = std::get<double>(args.at(flutter::EncodableValue("width")));
        double height = std::get<double>(args.at(flutter::EncodableValue("height")));

        HWND mainWindow = GetActiveWindow();
        SetWindowPos(mainWindow, HWND_TOP, 0, 0, int(width * devicePixelRatio), int(height * devicePixelRatio), SWP_NOMOVE);

        result->Success(flutter::EncodableValue(true));
    }

    void SetMinimumSize(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        result->NotImplemented();
    }

    void SetMaximumSize(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        result->NotImplemented();
    }

    void IsAlwaysOnTop(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        
        HWND mainWindow = GetActiveWindow();
        DWORD dwExStyle = GetWindowLong(mainWindow, GWL_EXSTYLE);

        flutter::EncodableMap resultMap = flutter::EncodableMap();
        if ((dwExStyle & WS_EX_TOPMOST) != 0) {
            resultMap[flutter::EncodableValue("isAlwaysOnTop")] = flutter::EncodableValue(true);
        } else {
            resultMap[flutter::EncodableValue("isAlwaysOnTop")] = flutter::EncodableValue(false);
        }
        result->Success(flutter::EncodableValue(resultMap));
    }

    void SetAlwaysOnTop(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        const flutter::EncodableMap& args = std::get<flutter::EncodableMap>(*method_call.arguments());

        bool isAlwaysOnTop = std::get<bool>(args.at(flutter::EncodableValue("isAlwaysOnTop")));

        HWND mainWindow = GetActiveWindow();
        SetWindowPos(mainWindow, isAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        result->Success(flutter::EncodableValue(true));
    }

    // Ported from https://github.com/alexmercerind/flutter-desktop-embedding/blob/da98a3b5a0e2b9425fbcb2a3e4b4ba50754abf93/plugins/window_size/windows/window_size_plugin.cpp#L255
    void SetFullScreen(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        const flutter::EncodableMap& args = std::get<flutter::EncodableMap>(*method_call.arguments());

        bool setFull = std::get<bool>(args.at(flutter::EncodableValue("isFullScreen")));
        if (setFull == window_fullscreened) {
            result->Success();
            return;
        }

        HWND mainWindow = GetActiveWindow();
        HMONITOR windowMonitor = MonitorFromWindow(mainWindow, MONITOR_DEFAULTTONEAREST);

        if (setFull) {
            MONITORINFO info;
            info.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(windowMonitor, &info);
            SetWindowLongPtr(mainWindow, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            GetWindowRect(mainWindow, &window_before_fullscreen);
            SetWindowPos(
                mainWindow,
                HWND_TOPMOST,
                info.rcMonitor.left, info.rcMonitor.top,
                info.rcMonitor.right - info.rcMonitor.left,
                info.rcMonitor.bottom - info.rcMonitor.top, 
                SWP_SHOWWINDOW
            );
            ShowWindow(mainWindow, SW_MAXIMIZE);
            window_fullscreened = true;
        } else {
            SetWindowLongPtr(mainWindow, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowPos(
                mainWindow,
                HWND_NOTOPMOST,
                window_before_fullscreen.left,
                window_before_fullscreen.top,
                window_before_fullscreen.right - window_before_fullscreen.left,
                window_before_fullscreen.bottom - window_before_fullscreen.top,
                SWP_SHOWWINDOW
            );
            ShowWindow(mainWindow, SW_RESTORE);
            window_fullscreened = false;
        }

        result->Success(flutter::EncodableValue(window_fullscreened));
    }

    void IsFullScreen(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        result->Success(flutter::EncodableValue(window_fullscreened));
    }

    void WindowManagerPlugin::HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        if (method_call.method_name().compare("getBounds") == 0) {
            GetBounds(method_call, std::move(result));
        }
        else if (method_call.method_name().compare("setBounds") == 0) {
            SetBounds(method_call, std::move(result));
        }
        else if (method_call.method_name().compare("setMinimumSize") == 0) {
            SetMinimumSize(method_call, std::move(result));
        }
        else if (method_call.method_name().compare("setMaximumSize") == 0) {
            SetMaximumSize(method_call, std::move(result));
        }
        else if (method_call.method_name().compare("isAlwaysOnTop") == 0) {
            IsAlwaysOnTop(method_call, std::move(result));
        }
        else if (method_call.method_name().compare("setAlwaysOnTop") == 0) {
            SetAlwaysOnTop(method_call, std::move(result));
        }
        else if (method_call.method_name().compare("setFullScreen") == 0) {
            SetFullScreen(method_call, std::move(result));
        }
        else if (method_call.method_name().compare("isFullScreen") == 0) {
            IsFullScreen(method_call, std::move(result));
        }
        else {
            result->NotImplemented();
        }
    }

}  // namespace

void WindowManagerPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
    WindowManagerPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarManager::GetInstance()
        ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
