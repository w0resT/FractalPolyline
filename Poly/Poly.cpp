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
#include <numeric>
#include <sstream>

#include "types/vec2.h"

namespace globals {
    std::vector<vec2> g_points;
    std::vector<vec2> g_fpl;
}

namespace vars {
    int v_recurs = 2;
    int v_delta = 2;

    int v_n = 25;

    int v_gen_type = 1; // 0 - normal, 1 - uniform

    namespace normal {
        float v_stddev = 0.2f;
    }

    namespace uniform {
        int v_j = 30;
        float v_sj = 0.01f;
    }
}

namespace plots {
    std::vector<float> pl_x;
    std::vector<float> pl_max;
    std::vector<float> pl_mean;
    std::vector<float> pl_elong;

    std::vector<float> pl3_log2elong;
    std::vector<int> pl3_x;

    float ar_x[ 256 ] = {};
    float ar_max[ 256 ] = {};
    float ar_mean[ 256 ] = {};
    float ar_elong[ 256 ] = {};
    float ar_log2elong[ 256 ] = {};

    float ar3_log2elong[ 256 ] = {};
    float ar3_x[ 256 ] = {};
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

void clear_plots( ) {
    // Clears plots stuff
    plots::pl_x.clear( );
    plots::pl_max.clear( );
    plots::pl_mean.clear( );
    plots::pl_elong.clear( );

    plots::pl3_log2elong.clear( );
    plots::pl3_x.clear( );

    for ( int i = 0; i < 256; ++i ) {
        plots::ar_x[ i ] = 0.f;
        plots::ar_max[ i ] = 0.f;
        plots::ar_mean[ i ] = 0.f;
        plots::ar_elong[ i ] = 0.f;
        plots::ar_log2elong[ i ] = 0.f;

        plots::ar3_log2elong[ i ] = 0.f;
        plots::ar3_x[ i ] = 0.f;
    }
}

float get_avg( const std::vector<float> &vec ) {
    if ( vec.empty( ) ) {
        return 0.f;
    }

    const auto count = static_cast< float >( vec.size( ) );
    return std::reduce( vec.begin( ), vec.end( ) ) / count;
}

float get_max( const std::vector<vec2> &points, const float &_y = 0.f ) {
    auto ret = *std::max_element( points.begin( ), points.end( ), [ & ]( const vec2 &a, const vec2 &b ) {
        auto a_y = std::fabsf( a.y - _y );
        auto b_y = std::fabsf( b.y - _y );
        return a_y < b_y;
    } );

    return std::fabsf( ret.y - _y );
}

float get_mean( const std::vector<vec2> &points, const float &_y = 0.f ) {
    float sum = 0.f;
    for ( const auto &p : points ) {
        sum += std::fabsf( p.y - _y );
    }

    auto ret = sum / points.size( );
    return ret;
}

float get_elong( const std::vector<vec2> &points ) {
    float sum = 0.f;
    for ( size_t i = 0; i < points.size( ) - 1; ++i ) {
        auto vec = points[ i ] - points[ i + 1 ];
        sum += vec.length( );
    }

    auto vec_ab = points[ 0 ] - points.back();
    auto vec_ab_len = vec_ab.length( );

    auto ret = sum / vec_ab_len;
    return ret;
}

std::tuple<float, float, float> do_stat( const std::vector<vec2> &src_points, const std::vector<vec2> &fpl_points ) {
    // Doing statistics only for line segment
    if ( src_points.size( ) != 2 ) {
        return std::make_tuple( 0.f, 0.f, 0.f );
    }

    // Check if we have any FPL's
    if ( fpl_points.empty( ) ) {
        return std::make_tuple( 0.f, 0.f, 0.f );
    }

    // Current y = (a.y - b.y) / 2
    auto y = ( src_points[ 0 ].y + src_points[ 1 ].y ) / 2;
    
    // Getting max dev
    auto max_dev = get_max( fpl_points, y );

    // Getting mean dev
    auto mean_dev = get_mean( fpl_points, y );

    // Getting elongation factor
    auto elong_fact = get_elong( fpl_points );

    return std::make_tuple( max_dev, mean_dev, elong_fact );
}

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

std::vector<vec2> do_fpl( int r, int delta, float stddev, float s ) {
    std::vector<vec2> fpl;

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
                fpl.push_back( fpl_points[ n ] );
                break;
            }

            // If we have the same coords: src(x,y) = dst(x,y) -> skip
            if ( !fpl.empty() && fpl.back() == fpl_points[ n ] ) {
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

            fpl.push_back( fpl_points[ n ] );
        }

        // Clear buffer
        Lp.clear( );
    }

    return fpl;
}

void get_stats( int r, int delta, float stddev, float s ) {
    // Doing charts stuff
    // Uniform div
    if ( vars::v_gen_type == 1 ) {
        // From sj to sj * j to charts 1, 2
        for ( int j = 1; j <= vars::uniform::v_j; ++j ) {
            float sj = vars::uniform::v_sj * j;

            std::vector<float> tmp_max;
            std::vector<float> tmp_mean;
            std::vector<float> tmp_elong;

            // Makes N's FPL's
            for ( int i = 0; i < vars::v_n; ++i ) {
                auto fpls = do_fpl( r, delta, stddev, sj );

                // Calc stats
                float t_max = 0.f, t_mean = 0.f, t_elong = 0.f;
                std::tie( t_max, t_mean, t_elong ) = do_stat( globals::g_points, fpls );

                // Failed to get stats
                if ( t_max == 0.f && t_mean == 0.f && t_elong == 0.f ) {
                    std::cout << "[error] stats = 0! Line: " << __LINE__ << std::endl;
                    return;
                }

                tmp_max.push_back( t_max );
                tmp_mean.push_back( t_mean );
                tmp_elong.push_back( t_elong );
            }

            float avg_max = get_avg( tmp_max );
            float avg_mean = get_avg( tmp_mean );
            float avg_elong = get_avg( tmp_elong );

            plots::pl_max.push_back( avg_max );
            plots::pl_mean.push_back( avg_mean );
            plots::pl_elong.push_back( avg_elong );

            plots::pl_x.push_back( sj );
        }

        // From 1 to r for chart 3
        for ( int i = 1; i <= r; ++i ) {
            int ri = i;

            std::vector<float> tmp_log2elong;

            // Makes N's FPL's
            for ( int i = 0; i < vars::v_n; ++i ) {
                auto fpls = do_fpl( ri, delta, stddev, s );

                // Calc stats
                float t_max = 0.f, t_mean = 0.f, t_elong = 0.f;
                std::tie( t_max, t_mean, t_elong ) = do_stat( globals::g_points, fpls );

                // Failed to get stats
                if ( t_max == 0.f && t_mean == 0.f && t_elong == 0.f ) {
                    std::cout << "[error] stats = 0! Line: " << __LINE__ << std::endl;
                    return;
                }

                tmp_log2elong.push_back( std::log2f( t_elong ) );
            }

            float avg_log2elong = get_avg( tmp_log2elong );

            plots::pl3_log2elong.push_back( avg_log2elong );

            plots::pl3_x.push_back( ri );
        }

        // Updating plots arrays
        for ( size_t i = 0; i < plots::pl_x.size( ); ++i ) {
            plots::ar_x[ i ] = plots::pl_x[ i ];
            plots::ar_max[ i ] = plots::pl_max[ i ];
            plots::ar_mean[ i ] = plots::pl_mean[ i ];
            plots::ar_elong[ i ] = plots::pl_elong[ i ];
            plots::ar_log2elong[ i ] = std::log2f( plots::pl_elong[ i ] );
        }

        for ( size_t i = 0; i < plots::pl3_x.size( ); ++i ) {
            plots::ar3_x[ i ] = static_cast< int >( plots::pl3_x[ i ] );
            plots::ar3_log2elong[ i ] = plots::pl3_log2elong[ i ];
        }
    }
    else if ( vars::v_gen_type == 0 ) {
        // From 0 to stddev with step 0.01 | For chart 1, 2
        for ( float i = 0.01f; i <= vars::normal::v_stddev; i += 0.01f ) {
            float stddevi = i;

            std::vector<float> tmp_max;
            std::vector<float> tmp_mean;
            std::vector<float> tmp_elong;

            // Makes N's FPL's
            for ( int i = 0; i < vars::v_n; ++i ) {
                auto fpls = do_fpl( r, delta, stddevi, s );

                // Calc stats
                float t_max = 0.f, t_mean = 0.f, t_elong = 0.f;
                std::tie( t_max, t_mean, t_elong ) = do_stat( globals::g_points, fpls );

                // Failed to get stats
                if ( t_max == 0.f && t_mean == 0.f && t_elong == 0.f ) {
                    std::cout << "[error] stats = 0! Line: " << __LINE__ << std::endl;
                    return;
                }

                tmp_max.push_back( t_max );
                tmp_mean.push_back( t_mean );
                tmp_elong.push_back( t_elong );
            }

            float avg_max = get_avg( tmp_max );
            float avg_mean = get_avg( tmp_mean );
            float avg_elong = get_avg( tmp_elong );

            plots::pl_max.push_back( avg_max );
            plots::pl_mean.push_back( avg_mean );
            plots::pl_elong.push_back( avg_elong );

            plots::pl_x.push_back( stddevi );
        }

        // From 1 to r | Chart 3
        for ( int i = 1; i <= r; ++i ) {
            int ri = i;

            std::vector<float> tmp_log2elong;

            // Makes N's FPL's
            for ( int i = 0; i < vars::v_n; ++i ) {
                auto fpls = do_fpl( ri, delta, stddev, s );

                // Calc stats
                float t_max = 0.f, t_mean = 0.f, t_elong = 0.f;
                std::tie( t_max, t_mean, t_elong ) = do_stat( globals::g_points, fpls );

                // Failed to get stats
                if ( t_max == 0.f && t_mean == 0.f && t_elong == 0.f ) {
                    std::cout << "[error] stats = 0! Line: " << __LINE__ << std::endl;
                    return;
                }

                tmp_log2elong.push_back( std::log2f( t_elong ) );
            }

            float avg_log2elong = get_avg( tmp_log2elong );

            plots::pl3_log2elong.push_back( avg_log2elong );

            plots::pl3_x.push_back( ri );
        }

        // Updating plots arrays
        for ( size_t i = 0; i < plots::pl_x.size( ); ++i ) {
            plots::ar_x[ i ] = plots::pl_x[ i ];
            plots::ar_max[ i ] = plots::pl_max[ i ];
            plots::ar_mean[ i ] = plots::pl_mean[ i ];
            plots::ar_elong[ i ] = plots::pl_elong[ i ];
            plots::ar_log2elong[ i ] = std::log2f( plots::pl_elong[ i ] );
        }

        for ( size_t i = 0; i < plots::pl3_x.size( ); ++i ) {
            plots::ar3_x[ i ] = static_cast< int >( plots::pl3_x[ i ] );
            plots::ar3_log2elong[ i ] = plots::pl3_log2elong[ i ];
        }
    }
}

void update_fpl( ) {
    // Doing FPL only if we have start points
    if ( globals::g_points.size( ) < 2 ) {
        return;
    }

    // Clear prev stuff
    if ( !globals::g_fpl.empty( ) ) {
        globals::g_fpl.clear( );

        clear_plots( );
    }

    // Variables for FPL
    float stddev = vars::normal::v_stddev;
    float s = vars::uniform::v_j * vars::uniform::v_sj;
    int r = vars::v_recurs;
    int delta = vars::v_delta;

    // Getting FPL's
    auto fpl_points = do_fpl( r, delta, stddev, s );
    if ( fpl_points.empty( ) ) {
        std::cout << "[error] fpls = 0! Line: " << __LINE__ << std::endl;
        return;
    }

    // Filling the main array with FPL
    globals::g_fpl = fpl_points;

    // Getting stats for charts
    get_stats( r, delta, stddev, s );
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
                    clear_plots( );
                }

                ImGui::SameLine( );

                if ( ImGui::Button( "Clear FPL's", ImVec2( bt_sz_x, bt_sz_y ) ) ) {
                    globals::g_fpl.clear( );
                    clear_plots( );
                }

                if ( ImGui::Button( "Do FPL", ImVec2( bt_sz_x, bt_sz_y ) ) ) {
                    update_fpl( );
                }

                // Recursion
                if ( ImGui::SliderInt( "R", &vars::v_recurs, 1, 10 ) ) {
                    // Update FPL only if we already drew it
                    if ( !globals::g_fpl.empty( ) ) {
                        update_fpl( );
                    }
                }

                // Min delta
                if ( ImGui::SliderInt( "Delta", &vars::v_delta, 1, 10 ) ) {
                    // Update FPL only if we already drew it
                    if ( !globals::g_fpl.empty( ) ) {
                        update_fpl( );
                    }
                }

                ImGui::Separator( );
                if ( ImGui::Combo( "Generator Type", &vars::v_gen_type, "Normal\0Uniform\0\0" ) ) {
                    // Update FPL only if we already drew it
                    if ( !globals::g_fpl.empty( ) ) {
                        update_fpl( );
                    }
                }

                ImGui::Separator( );

                // Normal dist
                if ( vars::v_gen_type == 0 ) {
                    // Standart deviation
                    if ( ImGui::SliderFloat( "Std dev", &vars::normal::v_stddev, 0.1f, 1.f ) ) {
                        // Update FPL only if we already drew it
                        if ( !globals::g_fpl.empty( ) ) {
                            update_fpl( );
                        }
                    }
                }
                // Uniform dist
                else if ( vars::v_gen_type == 1 ) {
                    ImGui::Text( "S = Sj * J = %f", ( vars::uniform::v_j * vars::uniform::v_sj ) );
                    
                    if ( ImGui::SliderInt( "J", &vars::uniform::v_j, 1, 50 ) ) {
                        // Update FPL only if we already drew it
                        if ( !globals::g_fpl.empty( ) ) {
                            update_fpl( );
                        }
                    }

                    if ( ImGui::SliderFloat( "Sj", &vars::uniform::v_sj, 0.01f, 0.1f ) ) {
                        // Update FPL only if we already drew it
                        if ( !globals::g_fpl.empty( ) ) {
                            update_fpl( );
                        }
                    }
                }

                ImGui::Separator( );
                ImGui::Text( "For charts" );
                ImGui::Separator( );

                // Min delta
                if ( ImGui::SliderInt( "N", &vars::v_n, 1, 50 ) ) {
                    // Update FPL only if we already drew it
                    if ( !globals::g_fpl.empty( ) ) {
                        update_fpl( );
                    }
                }

                ImGui::EndChild( );
            }

            // Left down
            if ( ImGui::BeginChild( "##main_page.child.left.down", ImVec2( 0, 0 ), true, ImGuiWindowFlags_NoSavedSettings ) ) {
                const ImU32 main_line_color_u32 = ImColor( 255, 255, 102, 255 );
                const ImU32 new_line_color_u32 = ImColor( 255, 179, 102, 255 );

                ImGui::Text( "Mouse Left: click to add point" );

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

                if ( ImGui::BeginTable( "##main_page.info.table", 2, ImGuiTableFlags_NoSavedSettings ) ) {
                    // First column -> main lines coords
                    ImGui::TableNextColumn( );
                    {
                        // Main lines coords
                        ImGui::Text( "Main lines coordinates: a(x,y) b(x,y)" );
                        ImGui::Separator( );
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

                    // Second column -> fpl lines coords
                    ImGui::TableNextColumn( );
                    {
                        auto fpl_size = globals::g_fpl.size( );
                        if ( fpl_size > 0 ) { fpl_size -= 1; }

                        // FPL lines coords
                        ImGui::Text( "FPL's coordinates: a(x,y) b(x,y)" );
                        ImGui::Separator( );
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

                ImGui::Separator( );

                if ( ImPlot::BeginPlot( "Line Plot 1" ) ) {
                    ImPlot::SetupAxes( "s", "value", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit );

                    ImPlot::PlotLine( "max", plots::ar_x, plots::ar_max, plots::pl_x.size( ) );
                    ImPlot::PlotLine( "mean", plots::ar_x, plots::ar_mean, plots::pl_x.size( ) );
                    ImPlot::PlotLine( "elong", plots::ar_x, plots::ar_elong, plots::pl_x.size( ) );

                    ImPlot::EndPlot( );
                }

                if ( ImGui::BeginTable( "##main_page.info.charts.table", 2, ImGuiTableFlags_NoSavedSettings ) ) {
                    // First column -> second chart
                    ImGui::TableNextColumn( );
                    {
                        std::stringstream ss;
                        ss << "r = " << vars::v_recurs;
                        if ( ImPlot::BeginPlot( "Line Plot 2" ) ) {
                            if ( vars::v_gen_type == 0 ) {
                                ImPlot::SetupAxes( "stddev", "value", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit );
                            }
                            else if ( vars::v_gen_type == 1 ) {
                                ImPlot::SetupAxes( "s", "value", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit );
                            }

                            ImPlot::PlotLine( ss.str().c_str(), plots::ar_x, plots::ar_log2elong, plots::pl_x.size( ) );

                            ImPlot::EndPlot( );
                        }
                    }

                    // Second column -> third chart
                    ImGui::TableNextColumn( );
                    {
                        std::stringstream ss;
                        if ( vars::v_gen_type == 0 ) {
                            ss << "stddev = " << vars::normal::v_stddev;
                        }
                        else if ( vars::v_gen_type == 1 ) {
                            ss << "s = " << vars::uniform::v_j * vars::uniform::v_sj;
                        }

                        if ( ImPlot::BeginPlot( "Line Plot 3" ) ) {
                            ImPlot::SetupAxes( "r", "value", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit );

                            ImPlot::PlotLine( ss.str( ).c_str( ), plots::ar3_x, plots::ar3_log2elong, plots::pl3_x.size( ) );

                            ImPlot::EndPlot( );
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
