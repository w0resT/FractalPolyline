// Backend + GUI
#include "imgui/imgui.h"
#include "imgui/backend/imgui_impl_dx9.h"
#include "imgui/backend/imgui_impl_win32.h"
#include "implot/implot.h"
#include <d3d9.h>
#include <tchar.h>

#pragma comment(lib, "d3d9.lib")

// Other includes
#include <iostream>
#include <vector>
#include <random> 

#include "types/vec2.h"

namespace globals {
    std::vector<vec2> g_points;
    std::vector<vec2> g_fpl;
}

namespace vars {
    int v_recurs = 2;
    int v_delta = 2;

    int v_n = 50;

    int v_gen_type = 1; // 0 - normal, 1 - uniform

    namespace normal {
        float v_stddev = 0.2f;
    }

    namespace uniform {
        int v_j = 30;
        float v_sj = 0.01f;
    }
}

// Data
static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D( HWND hWnd );
void CleanupDeviceD3D( );
void ResetDevice( );
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

/*
ImVector<ImVec2> FPLrec( ImVector<ImVec2> _points, ImVec2 _vec, int _r, int _delta, float _mean, float _stand_dev, int _gen_type, float _s) {
    ImVector<ImVec2> points;

    auto p = _points;
    //auto r = _r;

    auto vec_b = _vec;
    auto vec_a = p.back( );

    ImVec2 buff_v( vec_b.x - vec_a.x, vec_b.y - vec_a.y );
    //buff_v = NormalizeVec( buff_v.x, buff_v.y );

    auto V = VecLength( buff_v.x, buff_v.y );

    if ( _r == 0 || std::fabsf( V ) < _delta ) {
        p.push_back( vec_b );

        return p;
    }

    std::random_device rd {};
    std::mt19937 gen { rd( ) };
    float rf = 0.f; //dis( gen );

    if ( _gen_type == 0 ) {
        std::normal_distribution<float> dis( _mean, _stand_dev );
        rf = dis( gen );
    }
    else {
        std::uniform_real_distribution<> dis( -_s, _s );
        rf = dis( gen );
    }


    auto c = ImVec2( (vec_a.x + vec_b.x) / 2, (vec_a.y + vec_b.y) / 2 );
    auto rotv = VecRotate( buff_v.x, buff_v.y, 90.f );

    auto d = ImVec2( c.x + rf * rotv.x, c.y + rf * rotv.y );

    p = FPLrec( p, d, --_r, _delta, _mean, _stand_dev, _gen_type, _s );

    return FPLrec( p, vec_b, _r, _delta, _mean, _stand_dev, _gen_type, _s );
}
*/

float get_rf( float stddev, float s ) {
    std::random_device rd {};
    std::mt19937 gen { rd( ) };

    float ret = 0.f;

    // Normal dist
    if ( vars::v_gen_type == 0 ) {
        std::normal_distribution<float> dis( 0.f, stddev );
        ret = dis( gen );
    }
    // Uniform dist
    else if ( vars::v_gen_type == 1 ) {
        std::uniform_real_distribution<float> dis( -s, s );
        ret = dis( gen );
    }
    
    return ret;
}

std::vector<vec2> FPLrec( std::vector<vec2> list_a, vec2 point_b, int r, int delta, float stddev, float s ) {
    auto vec_a = list_a.back( );
    auto vec_b = point_b;

    // Getting the length of the segment ab
    auto vec_v = vec_b - vec_a;
    auto v_len = vec_v.length( );

    // Recursion stop condition
    if ( r == 0 || v_len < delta ) {
        list_a.push_back( vec_b );
        return list_a;
    }

    // Middle point
    auto c = ( vec_a + vec_b ) / 2;

    // Middle points offset
    auto rotv = vec_v.rotate( 90.f );
    auto rf = get_rf( stddev, s );
    auto d = vec2( c.x + rf * rotv.x, c.y + rf * rotv.y );

    // Splitting the segment ad
    list_a = FPLrec( list_a, d, --r, delta, stddev, s );

    // Splitting the segment ab
    return FPLrec( list_a, vec_b, r, delta, stddev, s );
}

void do_fpl( ) {
    // Doing FPL only if we have start points
    if ( globals::g_points.size() < 2 ) {
        return;
    }

    // Clear prev stuff
    if ( !globals::g_fpl.empty( ) ) {
        globals::g_fpl.clear( );
    }

    float stddev = vars::normal::v_stddev;
    float s = vars::uniform::v_j * vars::uniform::v_sj;

    int r = vars::v_recurs;
    int delta = vars::v_delta;

    std::vector<vec2> temp_fpl;

    // Main loop (proc 2 points - i and i + 1)
    for ( size_t i = 0; i < globals::g_points.size( ); i += 2 ) {
        std::vector<vec2> Lp;
        Lp.push_back( globals::g_points[ i ] ); // Point a
        auto vec_b = globals::g_points[ i + 1 ]; // Point b

        // Getting FPLs
        auto fpl_points = FPLrec( Lp, vec_b, r, delta, stddev, s );

        // Processing FPL points
        for ( size_t n = 0; n < fpl_points.size( ); ++n ) {
            // It's a last coord (point b)
            if ( n + 1 >= fpl_points.size( ) ) {
                globals::g_fpl.push_back( fpl_points[ n ] );
                break;
            }

            // If we have the same coords: src(x,y) = dst(x,y) -> skip
            if ( !globals::g_fpl.empty() && globals::g_fpl.back() == fpl_points[ n ] ) {
                continue;
            }

            // !Probably never called here!
            // Search the main points ab and remove them
            auto it_a = std::find( globals::g_points.begin( ), globals::g_points.end( ), fpl_points[ n ] );
            auto it_b = std::find( globals::g_points.begin( ), globals::g_points.end( ), fpl_points[ n + 1 ] );          
            
            // Skip if its points from a main lines
            if ( it_a != globals::g_points.end( ) && it_b != globals::g_points.end( ) ) {
                // Inc iterator a to get it equal to it_b
                it_a++;

                if ( it_a != globals::g_points.end( ) && it_a == it_b ) {
                    continue;
                }
            }

            globals::g_fpl.push_back( fpl_points[ n ] );
        }

        // Clear buffer
        Lp.clear( );
    }

    // Clear temp buffer
    temp_fpl.clear( );
}

static void ShowMainWindow( bool *p_open ) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport( );
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;
    ImGui::SetNextWindowPos( work_pos );
    ImGui::SetNextWindowSize( work_size );
    if ( !ImGui::Begin( "Yurchuk | Building a fractal polyline on a segment and a polygon", NULL, ImGuiWindowFlags_NoMove 
         | ImGuiWindowFlags_NoResize 
         | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings 
         | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) ) {
        ImGui::End( );
        return;
    }
    
    if ( ImGui::BeginTable( "##main_page.table", 2, ImGuiTableFlags_NoSavedSettings ) ) {
        // First column
        ImGui::TableNextColumn();
        {
            ImGui::Text( "Options" );

            // Left up
            if ( ImGui::BeginChild( "##main_page.child.left.up", ImVec2( 0, work_size.y / 2 - 38 ), true, ImGuiWindowFlags_NoSavedSettings ) ) {
                static float bt_sz_x = 160.f;
                static float bt_sz_y = 0;

                if ( ImGui::Button( "Clear canvas", ImVec2( bt_sz_x, bt_sz_y ) ) ) {
                    globals::g_points.clear( );
                    globals::g_fpl.clear( );
                }

                ImGui::SameLine( );

                if ( ImGui::Button( "Clear FPL's", ImVec2( bt_sz_x, bt_sz_y ) ) ) {
                    globals::g_fpl.clear( );
                }

                if ( ImGui::Button( "Do FPL", ImVec2( bt_sz_x, bt_sz_y ) ) ) {
                    do_fpl( );
                }

                // Recursion
                if ( ImGui::SliderInt( "R", &vars::v_recurs, 1, 10 ) ) {
                    // Update FPL only if we already drew it
                    if ( !globals::g_fpl.empty( ) ) {
                        do_fpl( );
                    }
                }

                // Min delta
                if ( ImGui::SliderInt( "Delta", &vars::v_delta, 1, 10 ) ) {
                    // Update FPL only if we already drew it
                    if ( !globals::g_fpl.empty( ) ) {
                        do_fpl( );
                    }
                }

                ImGui::Separator( );
                if ( ImGui::Combo( "Generator Type", &vars::v_gen_type, "Normal\0Uniform\0\0" ) ) {
                    // Update FPL only if we already drew it
                    if ( !globals::g_fpl.empty( ) ) {
                        do_fpl( );
                    }
                }

                ImGui::Separator( );

                // Normal dist
                if ( vars::v_gen_type == 0 ) {
                    // Standart deviation
                    if ( ImGui::SliderFloat( "Std dev", &vars::normal::v_stddev, 0.1f, 1.f ) ) {
                        // Update FPL only if we already drew it
                        if ( !globals::g_fpl.empty( ) ) {
                            do_fpl( );
                        }
                    }
                }
                // Uniform dist
                else if ( vars::v_gen_type == 1 ) {
                    ImGui::Text( "S = Sj * J = %f", ( vars::uniform::v_j * vars::uniform::v_sj ) );
                    
                    if ( ImGui::SliderInt( "J", &vars::uniform::v_j, 1, 50 ) ) {
                        // Update FPL only if we already drew it
                        if ( !globals::g_fpl.empty( ) ) {
                            do_fpl( );
                        }
                    }

                    if ( ImGui::SliderFloat( "Sj", &vars::uniform::v_sj, 0.01f, 0.1f ) ) {
                        // Update FPL only if we already drew it
                        if ( !globals::g_fpl.empty( ) ) {
                            do_fpl( );
                        }
                    }
                }

                ImGui::Separator( );
                ImGui::Text( "For charts" );
                ImGui::Separator( );

                // Min delta
                if ( ImGui::SliderInt( "N", &vars::v_n, 1, 100 ) ) {
                    // Update FPL only if we already drew it
                    if ( !globals::g_fpl.empty( ) ) {
                        do_fpl( );
                    }
                }

                ImGui::EndChild( );
            }

            // Left down
            if ( ImGui::BeginChild( "##main_page.child.left.down", ImVec2( 0, 0 ), true, ImGuiWindowFlags_NoSavedSettings ) ) {
                const ImU32 main_line_color_u32 = ImColor( 255, 255, 102, 255 );
                const ImU32 new_line_color_u32 = ImColor( 255, 179, 102, 255 );

                ImGui::Text( "Mouse Left: click to add point." );

                // Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use IsItemHovered()/IsItemActive()
                ImVec2 canvas_p0 = ImGui::GetCursorScreenPos( );      // ImDrawList API uses screen coordinates!
                ImVec2 canvas_sz = ImGui::GetContentRegionAvail( );   // Resize canvas to what's available
                if ( canvas_sz.x < 50.0f ) canvas_sz.x = 50.0f;
                if ( canvas_sz.y < 50.0f ) canvas_sz.y = 50.0f;
                ImVec2 canvas_p1 = ImVec2( canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y );

                // Draw border and background color
                ImGuiIO &io = ImGui::GetIO( );
                ImDrawList *draw_list = ImGui::GetWindowDrawList( );
                draw_list->AddRectFilled( canvas_p0, canvas_p1, IM_COL32( 50, 50, 50, 255 ) );
                draw_list->AddRect( canvas_p0, canvas_p1, IM_COL32( 255, 255, 255, 255 ) );

                // This will catch our interactions
                ImGui::InvisibleButton( "canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight );
                const bool is_hovered = ImGui::IsItemHovered( ); // Hovered
                const bool is_active = ImGui::IsItemActive( );   // Held
                const vec2 origin( canvas_p0.x, canvas_p0.y ); // Lock scrolled origin
                const vec2 mouse_pos_in_canvas( io.MousePos.x - origin.x, io.MousePos.y - origin.y );

                // Add first and second point
                if ( is_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ) {
                    vec2 prev_b;

                    if ( globals::g_points.size() > 1 ) {
                        prev_b = globals::g_points.back( );
                    }

                    // If we already have a line the we need only one point (dest)
                    if ( globals::g_points.size() > 1 ) {
                        // Point a
                        globals::g_points.push_back( prev_b );
                    }

                    // Point b
                    globals::g_points.push_back( mouse_pos_in_canvas );
                }

                // Draw grid + all lines in the canvas
                draw_list->PushClipRect( canvas_p0, canvas_p1, true );

                // Drawing grid
                const float GRID_STEP = 54.0f;
                for ( float x = GRID_STEP; x < canvas_sz.x; x += GRID_STEP )
                    draw_list->AddLine( ImVec2( canvas_p0.x + x, canvas_p0.y ), ImVec2( canvas_p0.x + x, canvas_p1.y ), IM_COL32( 200, 200, 200, 40 ) );
                for ( float y = GRID_STEP; y < canvas_sz.y; y += GRID_STEP )
                    draw_list->AddLine( ImVec2( canvas_p0.x, canvas_p0.y + y ), ImVec2( canvas_p1.x, canvas_p0.y + y ), IM_COL32( 200, 200, 200, 40 ) );

                // Drawing main lines
                if ( globals::g_points.size() > 1 ) {
                    for ( size_t n = 0; n < globals::g_points.size(); n += 2 )
                        if ( globals::g_points.size( ) > n + 1 ) {
                            draw_list->AddLine( ImVec2( origin.x + globals::g_points[ n ].x, origin.y + globals::g_points[ n ].y ), 
                                                ImVec2( origin.x + globals::g_points[ n + 1 ].x, origin.y + globals::g_points[ n + 1 ].y ), main_line_color_u32, 2.0f );
                        }
                }

                // Drawing FPL's lines
                if ( globals::g_fpl.size( ) > 1 ) {
                    for ( size_t n = 0; n < globals::g_fpl.size( ) - 1; ++n )
                        if ( globals::g_fpl.size( ) > n + 1 ) {
                            draw_list->AddLine( ImVec2( origin.x + globals::g_fpl[ n ].x, origin.y + globals::g_fpl[ n ].y ),
                                                ImVec2( origin.x + globals::g_fpl[ n + 1 ].x, origin.y + globals::g_fpl[ n + 1 ].y ), new_line_color_u32, 2.0f );
                        }
                }

                // Drawing circle on dots
                for ( size_t n = 0; n < globals::g_points.size(); ++n ) {
                    draw_list->AddCircle( ImVec2( origin.x + globals::g_points[ n ].x, origin.y + globals::g_points[ n ].y ), 3.f, IM_COL32( 59, 184, 42, 255 ), 0, 3.f );
                }

                draw_list->PopClipRect( );

                ImGui::EndChild( );
            }
        }

        // Second column
        ImGui::TableNextColumn( );
        {
            ImGui::Text( "Information" );
            if ( ImGui::BeginChild( "##main_page.child.right", ImVec2( work_size.x / 2 - 12 , 0 ), true, ImGuiWindowFlags_NoSavedSettings ) ) {
                ImGui::Text( "To display charts you need to generate a FPL" );
                ImGui::Separator( );

                if ( ImGui::BeginTable( "##main_page.table", 2, ImGuiTableFlags_NoSavedSettings ) ) {
                    // First column
                    ImGui::TableNextColumn( );
                    {
                        // Main lines coords
                        ImGui::Text( "Main lines coordinates: a(x,y) b(x,y)" );
                        ImGui::Text( "Count of the lines: %d", globals::g_points.size( ) / 2 );
                        if ( ImGui::BeginListBox( "##MainLinesCoords" ) ) {
                            for ( size_t i = 0; i < globals::g_points.size( ); i += 2 ) {
                                // Point a
                                ImGui::Text( "(%.f, %.f)", globals::g_points[ i ].x, globals::g_points[ i ].y );

                                // Point b
                                if ( globals::g_points.size( ) > i + 1 ) {
                                    ImGui::SameLine( );
                                    ImGui::Text( "(%.f, %.f)", globals::g_points[ i + 1 ].x, globals::g_points[ i + 1 ].y );
                                }
                            }

                            ImGui::EndListBox( );
                        }
                    }

                    // Second column
                    ImGui::TableNextColumn( );
                    {
                        auto fpl_size = globals::g_fpl.size( );
                        if ( fpl_size > 0 ) { fpl_size -= 1; }

                        // FPL lines coords
                        ImGui::Text( "FPL's coordinates: a(x,y) b(x,y)" );
                        ImGui::Text( "Count of the lines: %d", fpl_size );
                        if ( ImGui::BeginListBox( "##FPLCoords" ) ) {
                            for ( size_t i = 0; i < fpl_size; ++i ) {
                                // Point a
                                ImGui::Text( "(%.f, %.f)", globals::g_fpl[ i ].x, globals::g_fpl[ i ].y );

                                ImGui::SameLine( );

                                // Point b
                                ImGui::Text( "(%.f, %.f)", globals::g_fpl[ i + 1 ].x, globals::g_fpl[ i + 1 ].y );
                            }

                            ImGui::EndListBox( );
                        }
                    }

                    ImGui::EndTable( );
                }
                
                ImGui::EndChild( );
            }
        }

        ImGui::EndTable( );
    }

    ImGui::End( );
}

// Main code
int main( int, char ** ) {
    // Create application window
    WNDCLASSEX wc = { sizeof( WNDCLASSEX ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( NULL ), NULL, NULL, NULL, NULL, _T( "ImGui Example" ), NULL };
    ::RegisterClassEx( &wc );
    HWND hwnd = ::CreateWindow( wc.lpszClassName, _T( "Yurchuk | Building a fractal polyline on a segment and a polygon" ), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL );

    // Initialize Direct3D
    if ( !CreateDeviceD3D( hwnd ) ) {
        CleanupDeviceD3D( );
        ::UnregisterClass( wc.lpszClassName, wc.hInstance );
        return 1;
    }

    // Show the window
    ::ShowWindow( hwnd, SW_SHOWDEFAULT );
    ::UpdateWindow( hwnd );

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION( );
    ImGui::CreateContext( );
    ImPlot::CreateContext( );
    ImGuiIO &io = ImGui::GetIO( ); ( void )io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark( );
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle( );
    if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
        style.WindowRounding = 0.0f;
        style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init( hwnd );
    ImGui_ImplDX9_Init( g_pd3dDevice );

    // Our state
    bool show_app_main_window = true;
    ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );

    // Main loop
    bool done = false;
    while ( !done ) {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while ( ::PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) ) {
            ::TranslateMessage( &msg );
            ::DispatchMessage( &msg );
            if ( msg.message == WM_QUIT )
                done = true;
        }
        if ( done )
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame( );
        ImGui_ImplWin32_NewFrame( );
        ImGui::NewFrame( );

        if ( show_app_main_window ) {
            ShowMainWindow( &show_app_main_window );
        }

        // Rendering
        ImGui::EndFrame( );
        g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA( ( int )( clear_color.x * clear_color.w * 255.0f ), ( int )( clear_color.y * clear_color.w * 255.0f ), ( int )( clear_color.z * clear_color.w * 255.0f ), ( int )( clear_color.w * 255.0f ) );
        g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0 );
        if ( g_pd3dDevice->BeginScene( ) >= 0 ) {
            ImGui::Render( );
            ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData( ) );
            g_pd3dDevice->EndScene( );
        }

        // Update and Render additional Platform Windows
        if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            ImGui::UpdatePlatformWindows( );
            ImGui::RenderPlatformWindowsDefault( );
        }

        HRESULT result = g_pd3dDevice->Present( NULL, NULL, NULL, NULL );

        // Handle loss of D3D9 device
        if ( result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel( ) == D3DERR_DEVICENOTRESET )
            ResetDevice( );
    }

    ImGui_ImplDX9_Shutdown( );
    ImGui_ImplWin32_Shutdown( );
    ImPlot::DestroyContext( );
    ImGui::DestroyContext( );

    CleanupDeviceD3D( );
    ::DestroyWindow( hwnd );
    ::UnregisterClass( wc.lpszClassName, wc.hInstance );

    return 0;
}

// Helper functions

bool CreateDeviceD3D( HWND hWnd ) {
    if ( ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) == NULL )
        return false;

    // Create the D3DDevice
    ZeroMemory( &g_d3dpp, sizeof( g_d3dpp ) );
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if ( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice ) < 0 )
        return false;

    return true;
}

void CleanupDeviceD3D( ) {
    if ( g_pd3dDevice ) { g_pd3dDevice->Release( ); g_pd3dDevice = NULL; }
    if ( g_pD3D ) { g_pD3D->Release( ); g_pD3D = NULL; }
}

void ResetDevice( ) {
    ImGui_ImplDX9_InvalidateDeviceObjects( );
    HRESULT hr = g_pd3dDevice->Reset( &g_d3dpp );
    if ( hr == D3DERR_INVALIDCALL )
        IM_ASSERT( 0 );
    ImGui_ImplDX9_CreateDeviceObjects( );
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
    if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
        return true;

    switch ( msg ) {
    case WM_SIZE:
        if ( g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED ) {
            g_d3dpp.BackBufferWidth = LOWORD( lParam );
            g_d3dpp.BackBufferHeight = HIWORD( lParam );
            ResetDevice( );
        }
        return 0;
    case WM_SYSCOMMAND:
        if ( ( wParam & 0xfff0 ) == SC_KEYMENU ) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage( 0 );
        return 0;
    case WM_DPICHANGED:
        if ( ImGui::GetIO( ).ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports ) {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT *suggested_rect = ( RECT * )lParam;
            ::SetWindowPos( hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE );
        }
        break;
    }
    return ::DefWindowProc( hWnd, msg, wParam, lParam );
}
