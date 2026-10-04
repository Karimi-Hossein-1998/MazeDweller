#include "src/Agent.hpp"
#include "src/App.hpp"
#include "src/Level.hpp"
#include "src/MazeView.hpp"
#include "src/Player.hpp"
#include "src/Projectiles.hpp"
#include "src/TouchControls.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <SDL3/SDL_main.h>  // entry-point handling (required for Android/web)

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

enum class GameState { MainMenu, PlayMenu, LevelSelect, Playing, Paused, Completed, GameOver };

static const float kProjectileRange = 600.0f;
static const float kEnemyShootInterval = 1.5f;

static bool SaveProgress(const char* path, int levelNumber, int px, int py, int playerLevel, int exP, int highestWon)
{
    std::ofstream out(path);
    if (!out)
        return false;
    out << "level " << levelNumber << "\n";
    out << "player " << px << " " << py << "\n";
    out << "exp " << playerLevel << " " << exP << "\n";
    out << "highest " << highestWon << "\n";
    return (bool)out;
}

static bool LoadProgress(const char* path, int& levelNumber, int& px, int& py, int& playerLevel, int& exP, int& highestWon)
{
    std::ifstream in(path);
    if (!in)
        return false;
    std::string key;
    bool gotLevel = false;
    bool gotPlayer = false;
    bool gotExp = false;
    bool gotHighest = false;
    while (in >> key)
    {
        if (key == "level") { in >> levelNumber; gotLevel = true; }
        else if (key == "player") { in >> px >> py; gotPlayer = true; }
        else if (key == "exp") { in >> playerLevel >> exP; gotExp = true; }
        else if (key == "highest") { in >> highestWon; gotHighest = true; }
    }
    if (!gotHighest) highestWon = -1;
    return gotLevel && gotPlayer && gotExp;
}

// Scale menus down on small screens (e.g. mobile landscape) so they fit.
static float MenuScale()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float smallest = std::min(display.x, display.y);
    return std::clamp(smallest / 600.0f, 0.55f, 1.0f);
}

// Begin a centered, borderless menu window.
static void BeginMenuWindow(const char* name, ImVec2 size)
{
    const float s = MenuScale();
    size.x *= s;
    size.y *= s;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2((display.x - size.x) * 0.5f, (display.y - size.y) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
}

// Draw a large button centered on the current line.
static bool MenuButton(const char* label, ImVec2 size)
{
    const float s = MenuScale();
    size.x *= s;
    size.y *= s;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - size.x) * 0.5f);
    return ImGui::Button(label, size);
}

// Fire interval for continuous shooting, slowing as HP drops through
// 50%, 25%, 12.5%, 6%, and 3% of max HP.
static float ShootCooldown(const Player& player)
{
    const float hpFrac = player.MaxHitPoints() > 0 ? (float)player.HitPoints() / (float)player.MaxHitPoints() : 0.0f;
    const float base = 0.25f;
    if (hpFrac >= 0.50f) return base;
    if (hpFrac >= 0.25f) return base * 2.0f;
    if (hpFrac >= 0.125f) return base * 4.0f;
    if (hpFrac >= 0.06f) return base * 8.0f;
    if (hpFrac >= 0.03f) return base * 16.0f;
    return base * 32.0f;
}

// All game state lives here (heap-allocated once, kept for the program's
// lifetime) so it can be accessed from the frame function on every platform.
struct Game
{
    App*   app = nullptr;
    std::string savePath;
    bool   isAndroid = false;
    TouchControls touch;

    GameState state = GameState::MainMenu;
    int       currentLevel = 0;
    int       highestWon = -1;   // highest level completed (0-indexed)
    Player    player;

    int       tunnelWidth = 100;
    int       wallThickness = 20;

    Level    level;
    MazeView* view = nullptr;
    Agent*   agent = nullptr;

    SDL_Color tunnelColor = { 0, 0, 0, 255 };               // tunnels
    SDL_Color pathColor   = { 0, 150, 255, 255 };           // path overlay
    SDL_Color exitColor   = { 255, 200, 0, 255 };           // golden exit
    ImVec4    gameClear   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // walls
    ImVec4    menuClear   = ImVec4(0.08f, 0.08f, 0.13f, 1.0f);

    bool   showPath = false;
    std::vector<int> path;
    bool   prevH = false;
    bool   prevEscape = false;
    bool   prevMenuTap = false;
    float  shootCooldown = 0.0f;
    float  elapsed = 0.0f;
    float  levelTime = 0.0f;
    float  enemyShootTimer = 0.0f;
    Projectiles projectiles;

    int  screenW = 0;
    int  screenH = 0;
    bool hasTouch = false;
};

static Game* gGame = nullptr;

static void StartLevel(Game& g, int l)
{
    g.currentLevel = l;
    g.level.GenerateLevel(l);
    g.agent->Reset(g.level.PlayerStartX(), g.level.PlayerStartY());
    g.player.HealToFull();  // checkpoint: each level starts at full HP
    g.projectiles.Clear();
    g.levelTime = 0.0f;
    g.enemyShootTimer = 0.0f;
    g.shootCooldown = 0.0f;
    g.showPath = false;
    g.path.clear();
    g.state = GameState::Playing;
}

static void Frame()
{
    Game& g = *gGame;

    if (!g.app->Running())
    {
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }

    if (!g.app->NewFrame())
        return;

    const bool* keys = SDL_GetKeyboardState(nullptr);

    if (g.state == GameState::MainMenu)
    {
        BeginMenuWindow("Main Menu", ImVec2(320, 230));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
        if (MenuButton("Play", ImVec2(240, 70))) g.state = GameState::PlayMenu;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12);
        if (MenuButton("Quit", ImVec2(240, 70))) g.app->Quit();
        ImGui::End();

        g.app->Render(g.menuClear);
    }
    else if (g.state == GameState::PlayMenu)
    {
        BeginMenuWindow("Play", ImVec2(320, 370));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
        if (MenuButton("New", ImVec2(240, 50)))
        {
            g.player.Reset();
            g.highestWon = -1;
            StartLevel(g, 0);
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        if (MenuButton("Load", ImVec2(240, 50)))
        {
            int l = 0;
            int px = 0;
            int py = 0;
            int pl = 0;
            int ex = 0;
            int hw = -1;
            if (LoadProgress(g.savePath.c_str(), l, px, py, pl, ex, hw))
            {
                g.player.SetProgression(pl, ex);
                g.highestWon = hw;
                StartLevel(g, l);
                g.agent->Reset(px, py);
            }
            else
            {
                g.player.Reset();
                g.highestWon = -1;
                StartLevel(g, 0);
            }
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        if (MenuButton("Level Select", ImVec2(240, 50)))
            g.state = GameState::LevelSelect;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        if (MenuButton("Back", ImVec2(240, 50)))
            g.state = GameState::MainMenu;
        ImGui::End();

        g.app->Render(g.menuClear);
    }
    else if (g.state == GameState::LevelSelect)
    {
        BeginMenuWindow("Select Level", ImVec2(320, 440));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16);
        ImGui::BeginChild("levels", ImVec2(0, 340), false);
        for (int l = 0; l <= g.highestWon + 1; ++l)
        {
            const std::string label = "Level " + std::to_string(l + 1);
            if (MenuButton(label.c_str(), ImVec2(240, 40)))
                StartLevel(g, l);
        }
        ImGui::EndChild();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        if (MenuButton("Back", ImVec2(240, 50)))
            g.state = GameState::PlayMenu;
        ImGui::End();

        g.app->Render(g.menuClear);
    }
    else  // Playing, Paused, Completed, or GameOver
    {
        const float dt = ImGui::GetIO().DeltaTime;
        g.elapsed += dt;

        // Escape toggles the pause menu (only during play).
        const bool escape = keys[SDL_SCANCODE_ESCAPE];
        if (g.state == GameState::Playing && escape && !g.prevEscape)
            g.state = GameState::Paused;
        else if (g.state == GameState::Paused && escape && !g.prevEscape)
            g.state = GameState::Playing;
        g.prevEscape = escape;

        SDL_GetWindowSize(g.app->Window(), &g.screenW, &g.screenH);

        // Touch state: controls show on Android always; on other platforms
        // only while a finger is actually touching.
        g.hasTouch = g.isAndroid || !g.app->Fingers().empty();

        if (g.state == GameState::Playing)
        {
            // Movement: keyboard + touch movepad.
            if (g.hasTouch)
                g.touch.Update(g.app->Fingers(), g.screenW, g.screenH);

            // Touch menu button toggles the pause menu.
            if (g.hasTouch && g.touch.MenuPressed() && !g.prevMenuTap)
                g.state = GameState::Paused;
            g.prevMenuTap = g.hasTouch && g.touch.MenuPressed();

            float dx = 0.0f;
            float dy = 0.0f;
            if (keys[SDL_SCANCODE_UP])    dy -= 1.0f;
            if (keys[SDL_SCANCODE_DOWN])  dy += 1.0f;
            if (keys[SDL_SCANCODE_LEFT])  dx -= 1.0f;
            if (keys[SDL_SCANCODE_RIGHT]) dx += 1.0f;
            if (g.hasTouch) { dx += g.touch.MoveX(); dy += g.touch.MoveY(); }
            g.agent->Move(dx, dy, g.level.GetMaze(), dt);
            g.levelTime += dt;

            // Collect ExP orbs.
            g.player.GainExP(g.level.GetOrbs().CollectAt(g.agent->CellX(), g.agent->CellY()));

            // First 1 second is a grace period: nobody shoots.
            if (g.levelTime >= 1.0f)
            {
                // Player continuous shooting (Space or shoot button) at the nearest enemy.
                const bool shooting = keys[SDL_SCANCODE_SPACE] || (g.hasTouch && g.touch.Shooting());
                if (shooting)
                {
                    g.shootCooldown -= dt;
                    if (g.shootCooldown <= 0.0f)
                    {
                        const int idx = g.level.GetEnemies().Nearest(g.agent->PosX(), g.agent->PosY(), *g.view);
                        if (idx >= 0)
                        {
                            float ex, ey;
                            g.level.GetEnemies().Center(idx, *g.view, ex, ey);
                            g.projectiles.Spawn(g.agent->PosX(), g.agent->PosY(), ex, ey, g.player.Damage(), kProjectileRange, Projectiles::Owner::Player);
                        }
                        g.shootCooldown = ShootCooldown(g.player);
                    }
                }
                else
                {
                    g.shootCooldown = 0.0f;
                }

                // Enemy shooting.
                g.enemyShootTimer += dt;
                if (g.enemyShootTimer >= kEnemyShootInterval)
                {
                    g.enemyShootTimer = 0.0f;
                    for (std::size_t i = 0; i < g.level.GetEnemies().Count(); ++i)
                    {
                        float ex, ey;
                        g.level.GetEnemies().Center(i, *g.view, ex, ey);
                        g.projectiles.Spawn(ex, ey, g.agent->PosX(), g.agent->PosY(), g.level.GetEnemies().GetAttack(i), kProjectileRange, Projectiles::Owner::Enemy);
                    }
                }
            }

            // Enemy movement (pursuers, triggered by sight).
            g.level.GetEnemies().Update(g.level.GetMaze(), *g.view, g.agent->PosX(), g.agent->PosY(), dt);

            // Projectiles: move, and resolve hits.
            g.projectiles.Update(*g.view, dt);
            int exPEarned = 0;
            const int incoming = g.projectiles.ResolveCollisions(g.level.GetEnemies(), g.agent->PosX(), g.agent->PosY(), *g.view, exPEarned);
            if (incoming > 0)
                g.player.TakeDamage(incoming);
            if (exPEarned > 0)
                g.player.GainExP(exPEarned);

            const bool h = keys[SDL_SCANCODE_H];
            if (h && !g.prevH)
                g.showPath = !g.showPath;
            g.prevH = h;

            if (g.showPath)
                g.path = g.level.GetMaze().FindPath(g.agent->CellX(), g.agent->CellY(), g.level.ExitX(), g.level.ExitY());

            if (g.player.HitPoints() <= 0)
            {
                g.state = GameState::GameOver;
            }
            else if (g.agent->CellX() == g.level.ExitX() && g.agent->CellY() == g.level.ExitY())
            {
                g.state = GameState::Completed;
                g.player.GainExP(10 * (g.currentLevel + 1));
                if (g.currentLevel > g.highestWon)
                    g.highestWon = g.currentLevel;
            }
        }

        g.view->SetViewport(g.screenW, g.screenH);
        g.view->CenterOn(g.agent->PosX(), g.agent->PosY());

        // Info (top-left): level, stats, bars, position, save.
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.60f));
        ImGui::Begin("MazeDweller", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Level %d  (%dx%d)", g.currentLevel + 1, g.level.GetMaze().Width(), g.level.GetMaze().Height());
        ImGui::Text("Lv. %d   ATK %d   HP %d/%d   ExP %d/%d",
            g.player.Level(), g.player.Damage(), g.player.HitPoints(), g.player.MaxHitPoints(), g.player.ExP(), g.player.ExPToNext());
        const float hpFrac = g.player.MaxHitPoints() > 0 ? (float)g.player.HitPoints() / (float)g.player.MaxHitPoints() : 0.0f;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.20f, 0.80f, 0.30f, 1.0f));
        ImGui::ProgressBar(hpFrac, ImVec2(280, 16));
        ImGui::PopStyleColor();
        const float expFrac = g.player.ExPToNext() > 0 ? (float)g.player.ExP() / (float)g.player.ExPToNext() : 0.0f;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.30f, 0.50f, 1.0f, 1.0f));
        ImGui::ProgressBar(expFrac, ImVec2(280, 16));
        ImGui::PopStyleColor();
        ImGui::Text("pos (%.0f, %.0f)  cell (%d, %d)", g.agent->PosX(), g.agent->PosY(), g.agent->CellX(), g.agent->CellY());
        ImGui::Text("exit (%d, %d)  enemies %zu  [h] path %s", g.level.ExitX(), g.level.ExitY(), g.level.GetEnemies().Count(), g.showPath ? "ON" : "OFF");
        ImGui::End();
        ImGui::PopStyleColor();

        if (g.state == GameState::Paused)
        {
            BeginMenuWindow("Paused", ImVec2(320, 300));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
            if (MenuButton("Resume", ImVec2(240, 50)))
                g.state = GameState::Playing;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
            if (MenuButton("Save", ImVec2(240, 50)))
                SaveProgress(g.savePath.c_str(), g.currentLevel, g.agent->CellX(), g.agent->CellY(), g.player.Level(), g.player.ExP(), g.highestWon);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
            if (MenuButton("Main Menu", ImVec2(240, 50)))
                g.state = GameState::MainMenu;
            ImGui::End();
        }

        if (g.state == GameState::Completed)
        {
            BeginMenuWindow("Level Complete!", ImVec2(400, 360));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24);
            ImGui::TextWrapped("You completed this level, congrats!");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12);
            if (MenuButton("Replay Level", ImVec2(300, 50)))
                StartLevel(g, g.currentLevel);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
            if (MenuButton("Next Level", ImVec2(300, 50)))
                StartLevel(g, g.currentLevel + 1);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
            if (MenuButton("Previous Level", ImVec2(300, 50)))
                StartLevel(g, std::max(0, g.currentLevel - 1));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
            if (MenuButton("Main Menu", ImVec2(300, 50)))
                g.state = GameState::MainMenu;
            ImGui::End();
        }

        if (g.state == GameState::GameOver)
        {
            BeginMenuWindow("Game Over", ImVec2(360, 300));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24);
            ImGui::TextWrapped("You died!");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12);
            if (MenuButton("Replay Level", ImVec2(280, 50)))
                StartLevel(g, g.currentLevel);  // back to checkpoint (full HP)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
            if (MenuButton("Previous Level", ImVec2(280, 50)))
                StartLevel(g, std::max(0, g.currentLevel - 1));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
            if (MenuButton("Main Menu", ImVec2(280, 50)))
                g.state = GameState::MainMenu;
            ImGui::End();
        }

        g.app->Render(
            [&g](SDL_Renderer* renderer)
            {
                g.view->Render(renderer, g.tunnelColor);
                if (g.showPath)
                    g.view->RenderPath(renderer, g.path, g.pathColor);
                g.view->RenderCell(renderer, g.level.ExitX(), g.level.ExitY(), g.exitColor);  // golden exit
                g.level.GetOrbs().Render(renderer, *g.view, g.elapsed);                        // ExP orbs
                g.level.GetEnemies().Render(renderer, *g.view);                                // enemies
                g.projectiles.Render(renderer, *g.view);                                       // projectiles
                g.agent->Render(renderer);
                if (g.hasTouch)
                    g.touch.Render(renderer, g.screenW, g.screenH);                            // touch controls
            },
            g.gameClear);
    }
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Force landscape on Android (SDL overrides the manifest's screenOrientation).
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

    gGame = new Game();
    Game& g = *gGame;

    g.app = new App("MazeDweller", 1280, 800);
    if (!g.app->Ok())
        return 1;

    // Writable save location (portable across Linux, Android, and web).
    const char* prefPath = SDL_GetPrefPath("MazeDweller", "MazeDweller");
    g.savePath = (prefPath ? std::string(prefPath) : std::string("./")) + "savegame.txt";

    // Touch controls: enabled on Android (always has a touchscreen) and on
    // other platforms whenever a touch device is present.
    g.isAndroid = (SDL_strcmp(SDL_GetPlatform(), "Android") == 0);

    g.view  = new MazeView(g.level.GetMaze(), g.tunnelWidth, g.wallThickness);
    g.agent = new Agent(0, 0, *g.view);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(Frame, 0, 0);
#else
    while (g.app->Running())
        Frame();
#endif

    return 0;
}
