#pragma once

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include "imgui.h"

struct SDL_Window;
typedef union SDL_Event SDL_Event;

IMGUI_API void ImGui_Impl_Render(ImDrawData *draw_data);
IMGUI_API bool ImGui_ImplSdl_Init();
IMGUI_API void ImGui_ImplSdl_Shutdown();
IMGUI_API void ImGui_ImplSdl_NewFrame(SDL_Window *window);
IMGUI_API bool ImGui_ImplSdl_ProcessEvent(SDL_Event *event);
IMGUI_API bool ImGui_Impl_CreateFontsTexture(ImFontAtlas *font);
IMGUI_API void ImGui_Impl_DestroyFontsTexture(ImFontAtlas *font);