#include "GameScene.h"
#include "../MeshLoader.h"
#include <cmath>
#include <algorithm>
#include <iostream>

static constexpr int   MAX_ROUNDS    = 10;
static constexpr float HIT_FLASH_DUR = 0.25f;
static constexpr float ROUND_PAUSE   = 1.5f;

// ---------------------------------------------------------------------------
// Default fallback camera – used if the GLB failed to load.
// Positioned at (4.48, 2.14, 0) looking toward -X, matching booth.glb values.
// ---------------------------------------------------------------------------
static Camera makeFallbackCamera() {
    Camera c;
    c.px = 4.479257f; c.py = 2.137995f; c.pz = -0.004508f;
    // ~90° rotation around Y  (quaternion from GLB)
    c.qx = 0.011083f; c.qy = 0.708664f; c.qz = -0.011134f; c.qw = 0.705372f;
    c.fovY      = 0.6911f;
    c.nearPlane = 0.1f;
    c.farPlane  = 1000.0f;
    c.valid     = true;
    return c;
}

// ---------------------------------------------------------------------------
GameScene::GameScene(GameContext& ctx)
    : m_ctx(ctx)
    , m_cam(ctx.camera.valid ? ctx.camera : makeFallbackCamera())
    , m_rng(std::random_device{}())
{
    // Spawn area: targets are in front of camera (camera looks in -X, so
    // targets have smaller X than the camera).
    m_spawnXMin = m_cam.px - 3.6f;   // far plane (small targets)
    m_spawnXMax = m_cam.px - 0.8f;   // near plane (large targets)
    m_spawnYMin = 0.3f;
    m_spawnYMax = m_cam.py * 1.8f;   // a little above camera height
    m_spawnZMin = -3.5f;
    m_spawnZMax =  3.5f;

    m_randX   = std::uniform_real_distribution<float>(m_spawnXMin, m_spawnXMax);
    m_randY   = std::uniform_real_distribution<float>(m_spawnYMin, m_spawnYMax);
    m_randSpd = std::uniform_real_distribution<float>(m_baseSpeed * 0.7f, m_baseSpeed * 1.3f);
    m_randVy  = std::uniform_real_distribution<float>(-0.3f, 0.3f);
    m_randDir = std::uniform_int_distribution<int>(0, 1);

    spawnWave();

    // Load booth mesh and pre-project it (camera never moves so we do this once)
    m_mesh = MeshLoader::loadMesh("assets/booth.glb");
    if (m_mesh.valid) buildMeshRenderData();
}

// ---------------------------------------------------------------------------
void GameScene::beginFrame() { m_gun.beginFrame(); }
void GameScene::handleEvent(const SDL_Event& e) { m_gun.handleEvent(e); }

// ---------------------------------------------------------------------------
void GameScene::update(float dt) {

    if (m_inRoundPause) {
        m_roundPauseTimer -= dt;
        if (m_roundPauseTimer <= 0.0f) {
            m_inRoundPause = false;
            advanceRound();
            spawnWave();
        }
        return;
    }

    // Stagger spawns
    if (m_spawnedCount < m_waveSize) {
        m_spawnTimer -= dt;
        if (m_spawnTimer <= 0.0f) {
            spawnTarget();
            m_spawnTimer = m_spawnInterval;
        }
    }

    // Process shot
    if (m_gun.fired()) {
        m_flashAlpha = 40.0f;
        for (auto& t : m_targets) {
            if (!t.alive || t.hit) continue;
            if (testShot(m_gun.x(), m_gun.y(), t)) {
                t.hit      = true;
                t.hitTimer = HIT_FLASH_DUR;
                m_score   += t.points;
                break;
            }
        }
    }

    // Move targets
    for (auto& t : m_targets) {
        if (!t.alive) continue;

        if (t.hit) {
            t.hitTimer -= dt;
            if (t.hitTimer <= 0.0f) { t.alive = false; --m_targetsRemaining; }
            continue;
        }

        t.z += t.vz * dt;
        t.y += t.vy * dt;

        // Bounce vertically
        if (t.y < m_spawnYMin) { t.y = m_spawnYMin; t.vy =  std::abs(t.vy); }
        if (t.y > m_spawnYMax) { t.y = m_spawnYMax; t.vy = -std::abs(t.vy); }

        // Escaped off the side (flew past the Z bounds)
        bool escaped = (t.vz > 0.0f && t.z > m_spawnZMax + 1.0f)
                    || (t.vz < 0.0f && t.z < m_spawnZMin - 1.0f);
        if (escaped) {
            t.alive = false;
            --m_targetsRemaining;
            --m_lives;
        }
    }

    // Fade screen flash
    if (m_flashAlpha > 0.0f) m_flashAlpha -= 200.0f * dt;

    // Game over?
    if (m_lives <= 0) {
        m_ctx.finalScore = m_score;
        m_ctx.finalRound = m_round;
        transitionTo(SceneID::EndGame);
        return;
    }

    // Wave complete?
    if (m_targetsRemaining <= 0 && m_spawnedCount >= m_waveSize) {
        if (m_round >= MAX_ROUNDS) {
            m_ctx.finalScore = m_score;
            m_ctx.finalRound = m_round;
            transitionTo(SceneID::EndGame);
        } else {
            m_inRoundPause    = true;
            m_roundPauseTimer = ROUND_PAUSE;
        }
    }
}

// ---------------------------------------------------------------------------
void GameScene::render(SDL_Renderer* renderer) {
    drawBackground(renderer);
    drawMesh(renderer);      // booth geometry drawn on top of sky/ground

    // Sort targets back-to-front so closer ones draw on top
    std::vector<Target3D*> visible;
    for (auto& t : m_targets)
        if (t.alive) visible.push_back(&t);

    // Sort by X descending (further targets have smaller X = further from camera)
    std::sort(visible.begin(), visible.end(),
              [](const Target3D* a, const Target3D* b){ return a->x < b->x; });

    for (auto* t : visible)
        drawTarget(renderer, *t);

    drawHUD(renderer);
    drawLives(renderer);
    drawFlash(renderer);

    if (m_inRoundPause) {
        SDL_FRect overlay = { 0, 0, (float)m_ctx.windowWidth, (float)m_ctx.windowHeight };
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
        SDL_RenderFillRect(renderer, &overlay);
        // TODO: SDL_ttf "ROUND CLEAR!"
    }

    m_gun.renderCrosshair(renderer);
}

// ---------------------------------------------------------------------------
// Near-plane clipping helpers
// ---------------------------------------------------------------------------

// A point in camera space (before perspective divide).
struct CamPt { float cx, cy, cz; };

// Transform a world-space point into camera space.
static CamPt worldToCamera(const Camera& cam, float wx, float wy, float wz)
{
    float rx = wx - cam.px, ry = wy - cam.py, rz = wz - cam.pz;
    // Inverse quaternion rotation: negate xyz, keep w (conjugate of unit quat)
    float qx = -cam.qx, qy = -cam.qy, qz = -cam.qz, qw = cam.qw;
    float tx = 2.f*(qy*rz - qz*ry);
    float ty = 2.f*(qz*rx - qx*rz);
    float tz = 2.f*(qx*ry - qy*rx);
    return { rx + qw*tx + (qy*tz - qz*ty),
             ry + qw*ty + (qz*tx - qx*tz),
             rz + qw*tz + (qx*ty - qy*tx) };
}

// Project a camera-space point to screen coordinates.
// Returns false only if the point is behind the camera (cz >= -near).
static bool camToScreen(const Camera& cam, const CamPt& p, int W, int H,
                        float& sx, float& sy, float& depth)
{
    if (p.cz >= -cam.nearPlane) return false;
    depth         = -p.cz;
    float aspect  = (float)W / (float)H;
    float f       = 1.f / std::tan(cam.fovY * 0.5f);
    float ndcX    = (p.cx / depth) * (f / aspect);
    float ndcY    = (p.cy / depth) * f;
    // Loose guard — SDL clips to viewport, we only reject extreme values that
    // would cause float overflow in the renderer.
    if (std::abs(ndcX) > 100.f || std::abs(ndcY) > 100.f) return false;
    sx = (ndcX + 1.f) * 0.5f * (float)W;
    sy = (1.f - ndcY) * 0.5f * (float)H;
    return true;
}

// Linearly interpolate two camera-space points.
static CamPt lerpCam(const CamPt& a, const CamPt& b, float t) {
    return { a.cx + t*(b.cx-a.cx), a.cy + t*(b.cy-a.cy), a.cz + t*(b.cz-a.cz) };
}

// Clip one triangle against the near plane (cz < -near is "in front").
// Returns the number of output triangles (0, 1, or 2).
// Each output triangle occupies 3 consecutive entries in out[].
static int clipNear(const CamPt v[3], float near, CamPt out[2][3])
{
    bool ok[3] = { v[0].cz < -near, v[1].cz < -near, v[2].cz < -near };
    int n = (int)ok[0] + (int)ok[1] + (int)ok[2];
    if (n == 0) return 0;
    if (n == 3) { out[0][0]=v[0]; out[0][1]=v[1]; out[0][2]=v[2]; return 1; }

    // Returns the point where edge (behind→front) crosses the near plane.
    auto clip = [&](const CamPt& behind, const CamPt& front) -> CamPt {
        float t = (-near - behind.cz) / (front.cz - behind.cz);
        return lerpCam(behind, front, t);
    };

    if (n == 1) {
        // One vertex in front → one output triangle
        int fi = ok[0] ? 0 : (ok[1] ? 1 : 2);
        CamPt nc0 = clip(v[(fi+1)%3], v[fi]);
        CamPt nc1 = clip(v[(fi+2)%3], v[fi]);
        out[0][0] = v[fi]; out[0][1] = nc0; out[0][2] = nc1;
        return 1;
    } else { // n == 2
        // Two vertices in front → two output triangles (a quad split into 2)
        int bi = !ok[0] ? 0 : (!ok[1] ? 1 : 2);
        int f1 = (bi+1)%3, f2 = (bi+2)%3;
        CamPt nc1 = clip(v[bi], v[f1]);
        CamPt nc2 = clip(v[bi], v[f2]);
        out[0][0]=v[f1]; out[0][1]=v[f2]; out[0][2]=nc2;
        out[1][0]=v[f1]; out[1][1]=nc2;   out[1][2]=nc1;
        return 2;
    }
}

// ---------------------------------------------------------------------------
// buildMeshRenderData
//   Transforms every triangle into camera space, clips against the near plane,
//   then projects and flat-shades the resulting triangles.
//   Called once at construction because the camera never moves.
//   Triangles are sorted back-to-front for correct painter's-algorithm order.
// ---------------------------------------------------------------------------
void GameScene::buildMeshRenderData()
{
    const int W = m_ctx.windowWidth;
    const int H = m_ctx.windowHeight;

    // Directional light (world space) – from above-right, slightly forward
    const float LX = 0.309f, LY = 0.904f, LZ = 0.196f;

    struct SortTri {
        SDL_Vertex verts[3];
        float      depth; // centroid depth (positive, larger = further from cam)
    };
    std::vector<SortTri> tris;
    // Each input triangle can produce at most 2 after near-plane clipping.
    tris.reserve(m_mesh.indices.size() / 3 * 2);

    const auto& verts = m_mesh.vertices;

    // Debug: print vertex bounds to help verify the model is where we expect.
    float minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f,minZ=1e9f,maxZ=-1e9f;
    for (const auto& v : verts) {
        minX=std::min(minX,v.x); maxX=std::max(maxX,v.x);
        minY=std::min(minY,v.y); maxY=std::max(maxY,v.y);
        minZ=std::min(minZ,v.z); maxZ=std::max(maxZ,v.z);
    }
    std::cout << "[GameScene] Mesh bounds:"
              << " X[" << minX << "," << maxX << "]"
              << " Y[" << minY << "," << maxY << "]"
              << " Z[" << minZ << "," << maxZ << "]\n";
    std::cout << "[GameScene] Camera at ("
              << m_cam.px << "," << m_cam.py << "," << m_cam.pz << ")"
              << " fovY=" << m_cam.fovY << "\n";

    for (size_t ti = 0; ti + 2 < m_mesh.indices.size(); ti += 3) {
        const auto& v0 = verts[m_mesh.indices[ti + 0]];
        const auto& v1 = verts[m_mesh.indices[ti + 1]];
        const auto& v2 = verts[m_mesh.indices[ti + 2]];

        // Face normal in world space (for flat shading — use original vertices)
        float ax=v1.x-v0.x, ay=v1.y-v0.y, az=v1.z-v0.z;
        float bx=v2.x-v0.x, by=v2.y-v0.y, bz=v2.z-v0.z;
        float nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
        float len=std::sqrt(nx*nx+ny*ny+nz*nz);
        if (len > 1e-6f) { nx/=len; ny/=len; nz/=len; }
        float diffuse = std::abs(nx*LX + ny*LY + nz*LZ);
        float light   = 0.30f + 0.70f * diffuse;

        // Use the material base colour loaded from the GLB, flat-shaded.
        // All 3 vertices share the same material so v0 colour is representative.
        SDL_FColor col = {
            std::min(1.f, v0.r * light),
            std::min(1.f, v0.g * light),
            std::min(1.f, v0.b * light),
            v0.a
        };

        // Transform all three vertices to camera space
        CamPt cv[3] = {
            worldToCamera(m_cam, v0.x, v0.y, v0.z),
            worldToCamera(m_cam, v1.x, v1.y, v1.z),
            worldToCamera(m_cam, v2.x, v2.y, v2.z)
        };

        // Clip the triangle against the near plane
        CamPt clipped[2][3];
        int nOut = clipNear(cv, m_cam.nearPlane, clipped);

        // Project each clipped sub-triangle and add to the render list
        for (int t = 0; t < nOut; ++t) {
            float sx0,sy0,d0, sx1,sy1,d1, sx2,sy2,d2;
            if (!camToScreen(m_cam, clipped[t][0], W, H, sx0, sy0, d0)) continue;
            if (!camToScreen(m_cam, clipped[t][1], W, H, sx1, sy1, d1)) continue;
            if (!camToScreen(m_cam, clipped[t][2], W, H, sx2, sy2, d2)) continue;

            SortTri tri;
            tri.verts[0] = { {sx0, sy0}, col, {0, 0} };
            tri.verts[1] = { {sx1, sy1}, col, {0, 0} };
            tri.verts[2] = { {sx2, sy2}, col, {0, 0} };
            tri.depth    = (d0 + d1 + d2) / 3.f;
            tris.push_back(tri);
        }
    }

    // Sort back-to-front (painter's algorithm)
    std::sort(tris.begin(), tris.end(),
              [](const SortTri& a, const SortTri& b){ return a.depth > b.depth; });

    // Flatten into a simple vertex array (3 verts per triangle, no index buffer)
    m_meshVerts.clear();
    m_meshVerts.reserve(tris.size() * 3);
    for (const auto& tri : tris)
        for (int i = 0; i < 3; ++i)
            m_meshVerts.push_back(tri.verts[i]);

    std::cout << "[GameScene] Mesh render data built: "
              << tris.size() << " visible triangles\n";
}

// ---------------------------------------------------------------------------
void GameScene::drawMesh(SDL_Renderer* r) const
{
    if (m_meshVerts.empty()) return;
    SDL_RenderGeometry(r, nullptr,
                       m_meshVerts.data(),
                       static_cast<int>(m_meshVerts.size()),
                       nullptr, 0);
}

// ---------------------------------------------------------------------------
void GameScene::drawBackground(SDL_Renderer* r) const {
    const int W = m_ctx.windowWidth;
    const int H = m_ctx.windowHeight;

    // The camera looks nearly horizontally into the booth, so the ground plane
    // sits below the field of view.  Project a point at camera height to find
    // the screen centre, then put sky above and a dark "floor" below.
    float cx, cy, cd;
    float midY = H * 0.5f;  // fallback: screen centre
    if (m_cam.projectPoint(m_cam.px - 2.0f, m_cam.py, m_cam.pz, W, H, cx, cy, cd))
        midY = std::clamp(cy, 0.0f, (float)H);

    // Sky / backdrop (visible through the booth opening)
    SDL_FRect sky = { 0, 0, (float)W, midY };
    SDL_SetRenderDrawColor(r, 100, 170, 230, 255);   // lighter sky
    SDL_RenderFillRect(r, &sky);

    // Ground / lower backdrop
    SDL_FRect ground = { 0, midY, (float)W, (float)H - midY };
    SDL_SetRenderDrawColor(r, 60, 45, 30, 255);       // dark earth tone
    SDL_RenderFillRect(r, &ground);
}

// ---------------------------------------------------------------------------
void GameScene::drawTarget(SDL_Renderer* r, const Target3D& t) const {
    const int W = m_ctx.windowWidth;
    const int H = m_ctx.windowHeight;

    float sx, sy, depth;
    if (!m_cam.projectPoint(t.x, t.y, t.z, W, H, sx, sy, depth))
        return;

    // Screen size scales with depth: closer = bigger
    float halfW = m_cam.worldScreenSize(t.worldRadius,       depth, H);
    float halfH = m_cam.worldScreenSize(t.worldRadius * 0.6f, depth, H);

    uint8_t alpha = t.hit ? 100 : 255;

    // Body
    SDL_FRect body = { sx - halfW, sy - halfH, halfW * 2.0f, halfH * 2.0f };
    SDL_SetRenderDrawColor(r, t.r, t.g, t.b, alpha);
    SDL_RenderFillRect(r, &body);

    // Hit flash
    if (t.hit) {
        SDL_FRect flash = { sx - halfW * 0.6f, sy - halfH * 0.6f,
                            halfW * 1.2f, halfH * 1.2f };
        SDL_SetRenderDrawColor(r, 255, 255, 255, 180);
        SDL_RenderFillRect(r, &flash);
    }

    // Outline
    float bord = std::max(1.0f, halfW * 0.08f);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_FRect borders[4] = {
        { sx - halfW,         sy - halfH,         halfW * 2.0f, bord         },
        { sx - halfW,         sy + halfH - bord,  halfW * 2.0f, bord         },
        { sx - halfW,         sy - halfH,          bord,        halfH * 2.0f },
        { sx + halfW - bord,  sy - halfH,          bord,        halfH * 2.0f }
    };
    for (auto& b : borders) SDL_RenderFillRect(r, &b);
}

// ---------------------------------------------------------------------------
bool GameScene::testShot(float gx, float gy, const Target3D& t) const {
    float sx, sy, depth;
    if (!m_cam.projectPoint(t.x, t.y, t.z,
                            m_ctx.windowWidth, m_ctx.windowHeight,
                            sx, sy, depth))
        return false;

    float halfW = m_cam.worldScreenSize(t.worldRadius,        depth, m_ctx.windowHeight);
    float halfH = m_cam.worldScreenSize(t.worldRadius * 0.6f, depth, m_ctx.windowHeight);

    return gx >= sx - halfW && gx <= sx + halfW &&
           gy >= sy - halfH && gy <= sy + halfH;
}

// ---------------------------------------------------------------------------
void GameScene::spawnWave() {
    m_targets.clear();
    m_targets.reserve(m_waveSize);
    m_spawnedCount     = 0;
    m_spawnTimer       = 0.0f;
    m_targetsRemaining = m_waveSize;
}

// ---------------------------------------------------------------------------
void GameScene::spawnTarget() {
    Target3D t;

    bool leftToRight = (m_randDir(m_rng) == 0);
    float spd = m_randSpd(m_rng) * m_speedScale;

    t.x  = m_randX(m_rng);
    t.y  = m_randY(m_rng);
    t.z  = leftToRight ? m_spawnZMin - 0.3f : m_spawnZMax + 0.3f;
    t.vz = leftToRight ? spd : -spd;
    t.vy = m_randVy(m_rng);

    // Scale radius slightly with depth so near targets aren't enormous
    float normDepth = (t.x - m_spawnXMin) / std::max(0.01f, m_spawnXMax - m_spawnXMin);
    t.worldRadius = 0.08f + normDepth * 0.06f;  // 0.08 near, 0.14 far

    // Colour by round
    static const uint8_t PALETTE[][3] = {
        {200,180, 80},{80,180,200},{200, 80, 80},{80,200, 80},{200, 80,200},
        {255,160, 20},{20,200,160},{255,255, 80},{180, 80,255},{255,100,100}
    };
    int ci = std::min(m_round - 1, 9);
    t.r = PALETTE[ci][0]; t.g = PALETTE[ci][1]; t.b = PALETTE[ci][2];

    t.points = 100 * m_round;

    m_targets.push_back(t);
    ++m_spawnedCount;
}

// ---------------------------------------------------------------------------
void GameScene::advanceRound() {
    ++m_round;
    m_speedScale    = 1.0f + (m_round - 1) * 0.18f;
    m_waveSize      = 3 + (m_round - 1);
    m_spawnInterval = std::max(0.35f, 1.2f - (m_round - 1) * 0.09f);
}

// ---------------------------------------------------------------------------
void GameScene::drawHUD(SDL_Renderer* r) const {
    // Score bar
    float barW = 200.0f, barH = 36.0f, pad = 10.0f;
    SDL_FRect sPanel = { pad, pad, barW, barH };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_RenderFillRect(r, &sPanel);

    float maxScore = 1000.0f * m_round;
    float fill = std::min(1.0f, m_score / maxScore);
    SDL_FRect sFill = { pad + 2, pad + 2, (barW - 4) * fill, barH - 4 };
    SDL_SetRenderDrawColor(r, 255, 215, 0, 220);
    SDL_RenderFillRect(r, &sFill);
    // TODO: SDL_ttf "SCORE: X"

    // Round indicator
    SDL_FRect rPanel = { pad * 2 + barW, pad, 100.0f, barH };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_RenderFillRect(r, &rPanel);
    SDL_FRect rFill = { pad * 2 + barW + 2, pad + 2, 96.0f, barH - 4 };
    SDL_SetRenderDrawColor(r, 80, 160, 255, 220);
    SDL_RenderFillRect(r, &rFill);
    // TODO: SDL_ttf "RND X"
}

// ---------------------------------------------------------------------------
void GameScene::drawLives(SDL_Renderer* r) const {
    constexpr float PIP = 20.0f, GAP = 8.0f;
    float startX = m_ctx.windowWidth - (PIP + GAP) * 3 - 10.0f;
    for (int i = 0; i < 3; ++i) {
        SDL_FRect pip = { startX + i * (PIP + GAP), 10.0f, PIP, PIP };
        SDL_SetRenderDrawColor(r, i < m_lives ? 255 : 80,
                                  i < m_lives ?  60 : 40,
                                  i < m_lives ?  60 : 40, 255);
        SDL_RenderFillRect(r, &pip);
    }
}

// ---------------------------------------------------------------------------
void GameScene::drawFlash(SDL_Renderer* r) const {
    if (m_flashAlpha <= 0.0f) return;
    SDL_FRect full = { 0, 0, (float)m_ctx.windowWidth, (float)m_ctx.windowHeight };
    SDL_SetRenderDrawColor(r, 255, 255, 200,
                           static_cast<uint8_t>(std::min(255.0f, m_flashAlpha)));
    SDL_RenderFillRect(r, &full);
}
