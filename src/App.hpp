// App.hpp - minimal SDL3 + Dear ImGui wrapper.
//
// Bundles all the boilerplate (window, renderer, ImGui context, backends)
// into a small class so main.cpp only needs a handful of simple calls:
//
//     App app("Title", 1280, 800);
//     while (app.Running()) {
//         if (!app.NewFrame()) continue;   // false = window minimized, skip
//         ImGui::Begin("..."); ... ImGui::End();
//         app.Render(ImVec4(0.45f, 0.55f, 0.60f, 1.0f));
//     }
//
#pragma once

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL.h>
#include <functional>
#include <vector>

class App
{
    public:
        App(const char* title, int width, int height)
        {
            // --- SDL: init, window, renderer ---
            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
            {
                SDL_Log("SDL_Init failed: %s", SDL_GetError());
                return;
            }

            mScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

            mWindow = SDL_CreateWindow(title, (int)(width * mScale), (int)(height * mScale),
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
            if (!mWindow)
            {
                SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
                return;
            }

            // nullptr -> pick best driver; "software" -> force CPU rendering
            mRenderer = SDL_CreateRenderer(mWindow, nullptr);
            if (!mRenderer)
            {
                SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
                return;
            }
            SDL_SetRenderVSync(mRenderer, 1);

            // --- Dear ImGui: context, style, backends ---
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
            ImGui::StyleColorsDark();

            ImGuiStyle& style = ImGui::GetStyle();
            // On Android and Emscripten the SDL3 ImGui backend already handles
            // DPI via DisplayFramebufferScale, so scaling the style again would
            // double the UI size. Keep the desktop scaling behavior unchanged.
            const char* platform = SDL_GetPlatform();
            if (SDL_strcmp(platform, "Android") != 0 && SDL_strcmp(platform, "Emscripten") != 0)
            {
                style.ScaleAllSizes(mScale);
                style.FontScaleDpi = mScale;
            }

            ImGui_ImplSDL3_InitForSDLRenderer(mWindow, mRenderer);
            ImGui_ImplSDLRenderer3_Init(mRenderer);

            mOk = true;
        }

        ~App()
        {
            if (mOk)
            {
                ImGui_ImplSDLRenderer3_Shutdown();
                ImGui_ImplSDL3_Shutdown();
                ImGui::DestroyContext();
            }
            if (mRenderer)
                SDL_DestroyRenderer(mRenderer);
            if (mWindow)
                SDL_DestroyWindow(mWindow);
            SDL_Quit();
        }

        // ---- simple query helpers ----
        bool          Ok() const { return mOk; }           // did setup succeed?
        bool          Running() const { return mRunning; } // false when quit requested
        void          Quit() { mRunning = false; }
        SDL_Window*   Window() const { return mWindow; }
        SDL_Renderer* Renderer() const { return mRenderer; }
        float         Scale() const { return mScale; }
        const std::vector<SDL_FPoint>& Fingers() const { return mFingers; }

        // Poll events and begin a new ImGui frame.
        // Returns false if the window is minimized (caller should `continue`).
        bool NewFrame()
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT)
                    mRunning = false;
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(mWindow))
                    mRunning = false;

                // Track active touch fingers from events (works reliably on
                // Android even when SDL_GetTouchDevices doesn't enumerate the
                // touchscreen at startup).
                if (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION)
                {
                    bool found = false;
                    for (std::size_t i = 0; i < mFingerIds.size(); ++i)
                    {
                        if (mFingerIds[i] == event.tfinger.fingerID)
                        {
                            mFingers[i] = { event.tfinger.x, event.tfinger.y };
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        mFingerIds.push_back(event.tfinger.fingerID);
                        mFingers.push_back({ event.tfinger.x, event.tfinger.y });
                    }
                }
                else if (event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_CANCELED)
                {
                    for (std::size_t i = 0; i < mFingerIds.size(); ++i)
                    {
                        if (mFingerIds[i] == event.tfinger.fingerID)
                        {
                            mFingerIds.erase(mFingerIds.begin() + i);
                            mFingers.erase(mFingers.begin() + i);
                            break;
                        }
                    }
                }
            }

            if (SDL_GetWindowFlags(mWindow) & SDL_WINDOW_MINIMIZED)
            {
                SDL_Delay(10);
                return false;
            }

            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            return true;
        }

        // Finish the frame: clear, draw the scene, draw ImGui, present.
        void Render(const std::function<void(SDL_Renderer*)>& drawScene, const ImVec4& clearColor)
        {
            ImGui::Render();

            ImGuiIO& io = ImGui::GetIO();
            SDL_SetRenderScale(mRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            SDL_SetRenderDrawColorFloat(mRenderer, clearColor.x, clearColor.y, clearColor.z, clearColor.w);
            SDL_RenderClear(mRenderer);
            drawScene(mRenderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), mRenderer);
            SDL_RenderPresent(mRenderer);
        }

        // Convenience overload: no custom scene.
        void Render(const ImVec4& clearColor)
        {
            Render([](SDL_Renderer*) {}, clearColor);
        }

    private:
        SDL_Window*   mWindow   = nullptr;
        SDL_Renderer* mRenderer = nullptr;
        bool          mRunning  = true;
        bool          mOk       = false;
        float         mScale    = 1.0f;
        std::vector<SDL_FPoint>   mFingers;
        std::vector<SDL_FingerID> mFingerIds;
};
