/**
 * GTA V Data Recorder - COLMAP Format Compatible
 * Records camera pose (Tcw), screenshots, and gamepad inputs for 3D reconstruction datasets
 */

#include "..\\SDK\\inc\\main.h"
#include "..\\SDK\\inc\\natives.h"
#include "..\\SDK\\inc\\types.h"

#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <direct.h>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

// ============ Configuration ============
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const float TARGET_FPS = 30.0f;
const float FRAME_INTERVAL_MS = 1000.0f / TARGET_FPS;
const char* OUTPUT_DIR = "GTAV_Recorder_Output";

// Screenshot quality (0-100)
const int JPEG_QUALITY = 95;

// ============ Logging System ============
class Logger {
private:
    std::ofstream logFile;
    
public:
    Logger() {
        logFile.open("GTAV_Recorder_Debug.log", std::ios::app);
        if (logFile.is_open()) {
            time_t now = time(0);
            char buf[26];
            ctime_s(buf, sizeof(buf), &now);
            logFile << "\n========== GTAV Recorder Started ==========\n";
            logFile << "Time: " << buf;
            logFile.flush();
        }
    }
    
    ~Logger() {
        if (logFile.is_open()) {
            logFile << "========== End ==========\n\n";
            logFile.close();
        }
    }
    
    void Log(const std::string& msg) {
        if (logFile.is_open()) {
            DWORD tick = GetTickCount();
            logFile << "[" << tick << "] " << msg << "\n";
            logFile.flush();
        }
    }
};

static Logger g_logger;

// ============ Game Notifications ============
void Notify(const char* msg) {
    UI::_SET_NOTIFICATION_TEXT_ENTRY((char*)"STRING");
    UI::_ADD_TEXT_COMPONENT_STRING((char*)msg);
    UI::_DRAW_NOTIFICATION(false, false);
}

void Notify(const std::string& msg) {
    Notify(msg.c_str());
}

// ============ Key Input Handler ============
class KeyHandler {
private:
    bool prevState[256];
    
public:
    KeyHandler() {
        memset(prevState, 0, sizeof(prevState));
    }
    
    bool JustPressed(int vk) {
        bool curr = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool result = curr && !prevState[vk];
        prevState[vk] = curr;
        return result;
    }
    
    bool IsHeld(int vk) {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }
};

static KeyHandler g_keys;

// ============ Gamepad Handler ============
class GamepadHandler {
private:
    bool buttonPrevState[14];
    float stickPrevLX, stickPrevLY, stickPrevRX, stickPrevRY;
    float triggerPrevL, triggerPrevR;
    bool initialized;
    
public:
    GamepadHandler() : initialized(false), stickPrevLX(0), stickPrevLY(0), 
                       stickPrevRX(0), stickPrevRY(0), triggerPrevL(0), triggerPrevR(0) {
        memset(buttonPrevState, 0, sizeof(buttonPrevState));
    }
    
    // Check if gamepad is connected - use alternative method for older SDK
    bool IsConnected() {
        // Check if control is available for controller (input group 2)
        return CONTROLS::IS_CONTROL_ENABLED(0, 218) || CONTROLS::IS_CONTROL_ENABLED(0, 219);
    }
    
    // Get left stick X (-1 to 1)
    float GetLeftStickX() {
        return CONTROLS::GET_CONTROL_NORMAL(0, 218); // INPUT_SCRIPT_LEFT_AXIS_X
    }
    
    // Get left stick Y (-1 to 1)
    float GetLeftStickY() {
        return CONTROLS::GET_CONTROL_NORMAL(0, 219); // INPUT_SCRIPT_LEFT_AXIS_Y
    }
    
    // Get right stick X (-1 to 1)
    float GetRightStickX() {
        return CONTROLS::GET_CONTROL_NORMAL(0, 220); // INPUT_SCRIPT_RIGHT_AXIS_X
    }
    
    // Get right stick Y (-1 to 1)
    float GetRightStickY() {
        return CONTROLS::GET_CONTROL_NORMAL(0, 221); // INPUT_SCRIPT_RIGHT_AXIS_Y
    }
    
    // Get left trigger (0 to 1)
    float GetLeftTrigger() {
        return CONTROLS::GET_CONTROL_NORMAL(0, 225); // INPUT_SCRIPT_LT
    }
    
    // Get right trigger (0 to 1)
    float GetRightTrigger() {
        return CONTROLS::GET_CONTROL_NORMAL(0, 226); // INPUT_SCRIPT_RT
    }
    
    // Check button just pressed
    bool ButtonJustPressed(int control) {
        bool curr = CONTROLS::IS_CONTROL_JUST_PRESSED(0, control);
        return curr;
    }
    
    // Check button held
    bool ButtonHeld(int control) {
        return CONTROLS::IS_CONTROL_PRESSED(0, control);
    }
    
    // Get stick movement delta
    void GetStickDelta(float& lx, float& ly, float& rx, float& ry) {
        float curLX = GetLeftStickX();
        float curLY = GetLeftStickY();
        float curRX = GetRightStickX();
        float curRY = GetRightStickY();
        
        lx = curLX - stickPrevLX;
        ly = curLY - stickPrevLY;
        rx = curRX - stickPrevRX;
        ry = curRY - stickPrevRY;
        
        stickPrevLX = curLX;
        stickPrevLY = curLY;
        stickPrevRX = curRX;
        stickPrevRY = curRY;
    }
    
    // Get trigger delta
    void GetTriggerDelta(float& lt, float& rt) {
        float curLT = GetLeftTrigger();
        float curRT = GetRightTrigger();
        
        lt = curLT - triggerPrevL;
        rt = curRT - triggerPrevR;
        
        triggerPrevL = curLT;
        triggerPrevR = curRT;
    }
};

static GamepadHandler g_gamepad;

// ============ Screenshot System ============
class ScreenshotSystem {
private:
    // Simple JPEG encoder - we'll use GTA's built-in screenshot for now
    // and save with our naming convention
    
public:
    // Capture screen using GDI - GTA screenshot native not available in old SDK
    bool CaptureScreenGDI(const std::string& outputPath) {
        HDC hScreenDC = GetDC(NULL);
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        
        int width = GetDeviceCaps(hScreenDC, HORZRES);
        int height = GetDeviceCaps(hScreenDC, VERTRES);
        
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
        
        BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);
        
        // Save as BMP first (simplest), then we can convert or keep as is
        bool result = SaveBMP(hBitmap, hMemoryDC, width, height, outputPath);
        
        SelectObject(hMemoryDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        
        return result;
    }
    
    bool SaveBMP(HBITMAP hBitmap, HDC hDC, int width, int height, const std::string& path) {
        BITMAPFILEHEADER fileHeader;
        BITMAPINFOHEADER infoHeader;
        
        // Get bitmap info
        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);
        
        // Fill headers
        infoHeader.biSize = sizeof(BITMAPINFOHEADER);
        infoHeader.biWidth = width;
        infoHeader.biHeight = height;
        infoHeader.biPlanes = 1;
        infoHeader.biBitCount = 24;
        infoHeader.biCompression = BI_RGB;
        infoHeader.biSizeImage = 0;
        infoHeader.biXPelsPerMeter = 0;
        infoHeader.biYPelsPerMeter = 0;
        infoHeader.biClrUsed = 0;
        infoHeader.biClrImportant = 0;
        
        int rowSize = ((width * 24 + 31) / 32) * 4;
        int imageSize = rowSize * height;
        infoHeader.biSizeImage = imageSize;
        
        fileHeader.bfType = 0x4D42; // 'BM'
        fileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;
        fileHeader.bfReserved1 = 0;
        fileHeader.bfReserved2 = 0;
        fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        
        // Get pixel data
        std::vector<BYTE> pixels(imageSize);
        BITMAPINFO bmpInfo;
        bmpInfo.bmiHeader = infoHeader;
        
        if (!GetDIBits(hDC, hBitmap, 0, height, pixels.data(), &bmpInfo, DIB_RGB_COLORS)) {
            return false;
        }
        
        // Write to file
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;
        
        file.write((char*)&fileHeader, sizeof(fileHeader));
        file.write((char*)&infoHeader, sizeof(infoHeader));
        file.write((char*)pixels.data(), imageSize);
        
        return file.good();
    }
};

static ScreenshotSystem g_screenshot;

// ============ Math Utilities ============
struct Quat {
    float w, x, y, z;
};

void RotationMatrixToQuat(const float R[3][3], Quat& q) {
    float trace = R[0][0] + R[1][1] + R[2][2];
    
    if (trace > 0.0f) {
        float s = 0.5f / sqrtf(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (R[2][1] - R[1][2]) * s;
        q.y = (R[0][2] - R[2][0]) * s;
        q.z = (R[1][0] - R[0][1]) * s;
    } else {
        if (R[0][0] > R[1][1] && R[0][0] > R[2][2]) {
            float s = 2.0f * sqrtf(1.0f + R[0][0] - R[1][1] - R[2][2]);
            q.w = (R[2][1] - R[1][2]) / s;
            q.x = 0.25f * s;
            q.y = (R[0][1] + R[1][0]) / s;
            q.z = (R[0][2] + R[2][0]) / s;
        } else if (R[1][1] > R[2][2]) {
            float s = 2.0f * sqrtf(1.0f + R[1][1] - R[0][0] - R[2][2]);
            q.w = (R[0][2] - R[2][0]) / s;
            q.x = (R[0][1] + R[1][0]) / s;
            q.y = 0.25f * s;
            q.z = (R[1][2] + R[2][1]) / s;
        } else {
            float s = 2.0f * sqrtf(1.0f + R[2][2] - R[0][0] - R[1][1]);
            q.w = (R[1][0] - R[0][1]) / s;
            q.x = (R[0][2] + R[2][0]) / s;
            q.y = (R[1][2] + R[2][1]) / s;
            q.z = 0.25f * s;
        }
    }
}

void EulerToRotationMatrix(float pitch, float roll, float yaw, float R[3][3]) {
    float p = pitch * 3.14159265f / 180.0f;
    float r = roll * 3.14159265f / 180.0f;
    float y = yaw * 3.14159265f / 180.0f;
    
    float cy = cosf(y), sy = sinf(y);
    float cp = cosf(p), sp = sinf(p);
    float cr = cosf(r), sr = sinf(r);
    
    R[0][0] = cy * cr + sy * sp * sr;
    R[0][1] = sy * cp;
    R[0][2] = cy * sr - sy * sp * cr;
    
    R[1][0] = -sy * cr + cy * sp * sr;
    R[1][1] = cy * cp;
    R[1][2] = -sy * sr - cy * sp * cr;
    
    R[2][0] = -cp * sr;
    R[2][1] = sp;
    R[2][2] = cp * cr;
}

void ConvertGTAtoOpenCV(float gtaX, float gtaY, float gtaZ,
                        float& cvX, float& cvY, float& cvZ) {
    cvX = gtaX;
    cvY = -gtaZ;
    cvZ = gtaY;
}

void GetCameraPoseTcw(float gtaX, float gtaY, float gtaZ,
                      float gtaPitch, float gtaRoll, float gtaYaw,
                      Quat& q, float& tx, float& ty, float& tz) {
    float cvX, cvY, cvZ;
    ConvertGTAtoOpenCV(gtaX, gtaY, gtaZ, cvX, cvY, cvZ);
    
    float Rwc[3][3];
    EulerToRotationMatrix(gtaPitch, gtaRoll, gtaYaw, Rwc);
    
    float Rcw[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            Rcw[i][j] = Rwc[j][i];
        }
    }
    
    RotationMatrixToQuat(Rcw, q);
    
    tx = -(Rcw[0][0] * cvX + Rcw[0][1] * cvY + Rcw[0][2] * cvZ);
    ty = -(Rcw[1][0] * cvX + Rcw[1][1] * cvY + Rcw[1][2] * cvZ);
    tz = -(Rcw[2][0] * cvX + Rcw[2][1] * cvY + Rcw[2][2] * cvZ);
}

// ============ Input Recording ============
struct InputEvent {
    std::string type;
    float startTime;
    float endTime;
    std::string key;
    float stickLX, stickLY, stickRX, stickRY;
    float triggerL, triggerR;
    int buttonMask;
};

// ============ Recorder ============
class Recorder {
private:
    bool recording;
    int frameNum;
    float startTime;
    std::ofstream imgFile;
    std::ofstream camFile;
    std::ofstream inputFile;
    std::vector<InputEvent> events;
    
    bool keyState[256];
    float keyStart[256];
    
    std::string basePath;
    std::string sessionName;
    
public:
    Recorder() : recording(false), frameNum(0) {
        memset(keyState, 0, sizeof(keyState));
        memset(keyStart, 0, sizeof(keyStart));
    }
    
    ~Recorder() { Stop(); }
    
    bool IsRecording() { return recording; }
    int GetFrameNum() { return frameNum; }
    std::string GetSessionName() { return sessionName; }
    
    void Start(const std::string& customName = "") {
        if (recording) return;
        
        // Create directory
        _mkdir(OUTPUT_DIR);
        
        // Generate filename with timestamp and optional custom name
        time_t now = time(0);
        struct tm ti;
        localtime_s(&ti, &now);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &ti);
        
        if (customName.empty()) {
            sessionName = ts;
        } else {
            sessionName = std::string(ts) + "_" + customName;
        }
        
        basePath = std::string(OUTPUT_DIR) + "\\" + sessionName;
        
        // Create subdirectory for images
        std::string imgDir = basePath + "_images";
        _mkdir(imgDir.c_str());
        
        // Open images.txt (COLMAP format)
        imgFile.open(basePath + "_images.txt");
        if (!imgFile) {
            g_logger.Log("ERROR: Cannot create images.txt");
            return;
        }
        imgFile << std::fixed << std::setprecision(6);
        imgFile << "# Image list with two lines of data per image:\n";
        imgFile << "#   IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME\n";
        imgFile << "#   POINTS2D[] as (X, Y, POINT3D_ID)\n";
        imgFile << "# Number of images: 0, mean observations per image: 0\n";
        
        // Open cameras.txt
        camFile.open(basePath + "_cameras.txt");
        if (!camFile) {
            g_logger.Log("ERROR: Cannot create cameras.txt");
            imgFile.close();
            return;
        }
        camFile << "# Camera list with one line of data per camera:\n";
        camFile << "#   CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]\n";
        camFile << "# Number of cameras: 1\n";
        
        float fov = CAM::GET_GAMEPLAY_CAM_FOV();
        float fx = (SCREEN_WIDTH / 2.0f) / tanf(fov * 3.14159265f / 360.0f);
        float fy = fx;
        float cx = SCREEN_WIDTH / 2.0f;
        float cy = SCREEN_HEIGHT / 2.0f;
        
        camFile << "1 PINHOLE " << SCREEN_WIDTH << " " << SCREEN_HEIGHT 
                << " " << fx << " " << fy << " " << cx << " " << cy << "\n";
        camFile.close();
        
        // Open input.json
        inputFile.open(basePath + "_input.json");
        if (!inputFile) {
            g_logger.Log("ERROR: Cannot create input.json");
            imgFile.close();
            return;
        }
        
        recording = true;
        frameNum = 0;
        startTime = GetTickCount() / 1000.0f;
        events.clear();
        
        g_logger.Log("Recording started: " + basePath);
        g_logger.Log("Session: " + sessionName);
        g_logger.Log("FOV: " + std::to_string(fov));
        
        Notify("~g~Recording Started!");
        Notify("~y~Session: " + sessionName);
        
        UI::DISPLAY_HUD(false);
        UI::DISPLAY_RADAR(false);
        CAM::SET_FOLLOW_PED_CAM_VIEW_MODE(4);
    }
    
    void Stop() {
        if (!recording) return;
        recording = false;
        
        if (imgFile.is_open()) imgFile.close();
        
        if (inputFile.is_open()) {
            inputFile << "{\n  \"session\": \"" << sessionName << "\",\n";
            inputFile << "  \"total_frames\": " << frameNum << ",\n";
            inputFile << "  \"data\": [\n";
            for (size_t i = 0; i < events.size(); i++) {
                const auto& e = events[i];
                inputFile << "    {\n";
                inputFile << "      \"type\": \"" << e.type << "\",\n";
                inputFile << "      \"start_time\": " << e.startTime << ",\n";
                inputFile << "      \"end_time\": " << e.endTime;
                if (e.type == "key") {
                    inputFile << ",\n      \"key\": \"" << e.key << "\"";
                } else if (e.type == "gamepad") {
                    inputFile << ",\n      \"stick_lx\": " << e.stickLX << ",\n";
                    inputFile << "      \"stick_ly\": " << e.stickLY << ",\n";
                    inputFile << "      \"stick_rx\": " << e.stickRX << ",\n";
                    inputFile << "      \"stick_ry\": " << e.stickRY << ",\n";
                    inputFile << "      \"trigger_l\": " << e.triggerL << ",\n";
                    inputFile << "      \"trigger_r\": " << e.triggerR << ",\n";
                    inputFile << "      \"buttons\": " << e.buttonMask;
                }
                inputFile << "\n    }";
                if (i < events.size() - 1) inputFile << ",";
                inputFile << "\n";
            }
            inputFile << "  ]\n}\n";
            inputFile.close();
        }
        
        UI::DISPLAY_HUD(true);
        UI::DISPLAY_RADAR(true);
        
        g_logger.Log("Recording ended, frames: " + std::to_string(frameNum));
        
        std::stringstream ss;
        ss << "~g~Recording Ended! Frames: " << frameNum;
        Notify(ss.str());
    }
    
    void RecordFrame() {
        if (!recording) return;
        
        frameNum++;
        float t = GetTickCount() / 1000.0f - startTime;
        
        // Get camera pose
        Vector3 pos = CAM::GET_GAMEPLAY_CAM_COORD();
        Vector3 rot = CAM::GET_GAMEPLAY_CAM_ROT(2);
        
        Quat q;
        float tx, ty, tz;
        GetCameraPoseTcw(pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, q, tx, ty, tz);
        
        // Generate filename
        char fname[32];
        sprintf_s(fname, "%07d.jpg", frameNum);
        std::string imgPath = basePath + "_images\\" + fname;
        
        // Take screenshot
        bool screenshotOK = g_screenshot.CaptureScreenGDI(imgPath);
        if (!screenshotOK) {
            g_logger.Log("ERROR: Screenshot failed for frame " + std::to_string(frameNum));
        }
        
        // Write to images.txt (COLMAP format: two lines per image)
        imgFile << frameNum << " "
                << q.w << " " << q.x << " " << q.y << " " << q.z << " "
                << tx << " " << ty << " " << tz << " "
                << "1 " << fname << "\n";
        imgFile << "\n";
        
        // Record input
        RecordInput(t);
    }
    
    void RecordInput(float t) {
        // Check keyboard keys
        CheckKey('W', "W", t);
        CheckKey('A', "A", t);
        CheckKey('S', "S", t);
        CheckKey('D', "D", t);
        CheckKey(VK_SPACE, "SPACE", t);
        CheckKey(VK_SHIFT, "SHIFT", t);
        CheckKey(VK_CONTROL, "CTRL", t);
        CheckKey(VK_UP, "UP", t);
        CheckKey(VK_DOWN, "DOWN", t);
        CheckKey(VK_LEFT, "LEFT", t);
        CheckKey(VK_RIGHT, "RIGHT", t);
        
        // Record gamepad input
        RecordGamepad(t);
    }
    
    void CheckKey(int vk, const std::string& name, float t) {
        bool pressed = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (pressed && !keyState[vk]) {
            keyState[vk] = true;
            keyStart[vk] = t;
        } else if (!pressed && keyState[vk]) {
            keyState[vk] = false;
            InputEvent e;
            e.type = "key";
            e.startTime = keyStart[vk];
            e.endTime = t;
            e.key = name;
            e.stickLX = e.stickLY = e.stickRX = e.stickRY = 0;
            e.triggerL = e.triggerR = 0;
            e.buttonMask = 0;
            events.push_back(e);
        }
    }
    
    void RecordGamepad(float t) {
        // Get stick values
        float lx = g_gamepad.GetLeftStickX();
        float ly = g_gamepad.GetLeftStickY();
        float rx = g_gamepad.GetRightStickX();
        float ry = g_gamepad.GetRightStickY();
        float lt = g_gamepad.GetLeftTrigger();
        float rt = g_gamepad.GetRightTrigger();

        // Build button mask
        int buttonMask = 0;
        if (g_gamepad.ButtonHeld(191)) buttonMask |= 1;    // A
        if (g_gamepad.ButtonHeld(194)) buttonMask |= 2;    // B
        if (g_gamepad.ButtonHeld(193)) buttonMask |= 4;    // X
        if (g_gamepad.ButtonHeld(192)) buttonMask |= 8;    // Y
        if (g_gamepad.ButtonHeld(204)) buttonMask |= 16;   // LB
        if (g_gamepad.ButtonHeld(205)) buttonMask |= 32;   // RB
        if (g_gamepad.ButtonHeld(206)) buttonMask |= 64;   // LT (as button)
        if (g_gamepad.ButtonHeld(207)) buttonMask |= 128;  // RT (as button)
        if (g_gamepad.ButtonHeld(208)) buttonMask |= 256;  // Select/Back
        if (g_gamepad.ButtonHeld(209)) buttonMask |= 512;  // Start
        if (g_gamepad.ButtonHeld(210)) buttonMask |= 1024; // LS
        if (g_gamepad.ButtonHeld(211)) buttonMask |= 2048; // RS

        // Only record if there's significant input
        float stickThreshold = 0.05f;
        float triggerThreshold = 0.05f;

        bool hasInput = (fabs(lx) > stickThreshold || fabs(ly) > stickThreshold ||
                        fabs(rx) > stickThreshold || fabs(ry) > stickThreshold ||
                        lt > triggerThreshold || rt > triggerThreshold ||
                        buttonMask != 0);

        if (hasInput) {
            InputEvent e;
            e.type = "gamepad";
            e.startTime = t;
            e.endTime = t + (1.0f / TARGET_FPS);
            e.key = "";
            e.stickLX = lx;
            e.stickLY = ly;
            e.stickRX = rx;
            e.stickRY = ry;
            e.triggerL = lt;
            e.triggerR = rt;
            e.buttonMask = buttonMask;
            events.push_back(e);
        }
    }
};

static Recorder g_rec;

// ============ Main Loop ============
void MainLoop() {
    g_logger.Log("MainLoop() started");
    
    while (DLC2::GET_IS_LOADING_SCREEN_ACTIVE()) {
        WAIT(0);
    }
    g_logger.Log("Game loaded");
    
    while (!PLAYER::IS_PLAYER_PLAYING(PLAYER::PLAYER_ID())) {
        WAIT(0);
    }
    g_logger.Log("Player ready");
    
    // Show hints
    Notify("~g~GTAV Recorder Loaded");
    Notify("~y~F9: Start/Stop Recording");
    Notify("~y~F8: Start with custom name");
    Notify("~y~F10: Toggle HUD");
    Notify("~y~F11: Toggle View");
    Notify("~y~End: Exit");
    
    DWORD lastFrame = GetTickCount();
    int frameCounter = 0;
    DWORD lastFpsTime = GetTickCount();
    
    while (true) {
        // F9 - Recording control
        if (g_keys.JustPressed(VK_F9)) {
            if (g_rec.IsRecording()) {
                g_rec.Stop();
            } else {
                g_rec.Start();
            }
        }
        
        // F8 - Start with custom name (for multiple sessions)
        if (g_keys.JustPressed(VK_F8)) {
            if (!g_rec.IsRecording()) {
                // Use a simple naming scheme - you can expand this
                static int sessionIdx = 1;
                g_rec.Start("session" + std::to_string(sessionIdx));
                sessionIdx++;
            }
        }
        
        // F10 - HUD toggle
        if (g_keys.JustPressed(VK_F10)) {
            static bool hidden = false;
            hidden = !hidden;
            UI::DISPLAY_HUD(!hidden);
            UI::DISPLAY_RADAR(!hidden);
            Notify(hidden ? "~y~HUD Hidden" : "~g~HUD Shown");
        }
        
        // F11 - View mode toggle
        if (g_keys.JustPressed(VK_F11)) {
            int mode = CAM::GET_FOLLOW_PED_CAM_VIEW_MODE();
            int newMode = (mode == 4) ? 0 : 4;
            CAM::SET_FOLLOW_PED_CAM_VIEW_MODE(newMode);
            Notify(newMode == 4 ? "~g~First Person" : "~g~Third Person");
        }
        
        // Record frame at target FPS
        if (g_rec.IsRecording()) {
            DWORD now = GetTickCount();
            if (now - lastFrame >= (DWORD)FRAME_INTERVAL_MS) {
                g_rec.RecordFrame();
                lastFrame = now;
                frameCounter++;
                
                if (now - lastFpsTime >= 1000) {
                    std::stringstream ss;
                    ss << "~g~Recording... Frame:" << g_rec.GetFrameNum() << " FPS:" << frameCounter;
                    Notify(ss.str());
                    frameCounter = 0;
                    lastFpsTime = now;
                }
            }
        }
        
        // End - Exit
        if (g_keys.JustPressed(VK_END)) {
            g_rec.Stop();
            Notify("~r~Exiting");
            break;
        }
        
        WAIT(0);
    }
    
    g_logger.Log("MainLoop() ended");
}

// Script Hook V Entry Point
extern "C" __declspec(dllexport) void ScriptMain() {
    MainLoop();
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        scriptRegister(hInstance, ScriptMain);
        break;
    case DLL_PROCESS_DETACH:
        scriptUnregister(hInstance);
        break;
    }
    return TRUE;
}
