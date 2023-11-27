/*
 * Copyright 2023 Rémi Bernon for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "wglgears_private.h"

#include "wingdi.h"
#include "winuser.h"

#include "d3d9.h"

#include <stdlib.h>

struct d3d_window_data
{
    IDirect3D9 *d3d;
    IDirect3DDevice9 *d3ddev;
    IDirect3DVertexBuffer9 *v_buffer;
};

struct CUSTOMVERTEX
{
    FLOAT x, y, z, rhw; // from the D3DFVF_XYZRHW flag
    DWORD color;        // from the D3DFVF_DIFFUSE flag
};

#define CUSTOMFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

static struct d3d_window_data *init_d3d_window( HWND hwnd, int width, int height, int flip )
{
    D3DPRESENT_PARAMETERS d3dpp =
    {
        .Windowed = TRUE,
        .SwapEffect = D3DSWAPEFFECT_DISCARD,
        .hDeviceWindow = hwnd,
        .BackBufferFormat = D3DFMT_X8R8G8B8,
        .BackBufferWidth = width,
        .BackBufferHeight = height,
    };
    struct CUSTOMVERTEX vertices[] =
    {
        {width / 2, 0, 0.5f, 1.0f, D3DCOLOR_XRGB( 0, 0, 255 )},
        {width, height, 0.5f, 1.0f, D3DCOLOR_XRGB( 0, 255, 0 )},
        {0, height, 0.5f, 1.0f, D3DCOLOR_XRGB( 255, 0, 0 )},
    };
    struct CUSTOMVERTEX flip_vertices[] =
    {
        {width / 2, height, 0.5f, 1.0f, D3DCOLOR_XRGB( 0, 0, 255 )},
        { 0, 0, 0.5f, 1.0f, D3DCOLOR_XRGB( 0, 255, 0 )},
        { width, 0, 0.5f, 1.0f, D3DCOLOR_XRGB( 255, 0, 0 )},
    };
    struct d3d_window_data *data;
    void *buffer_data;

    if (!(data = malloc( sizeof(struct d3d_window_data) ))) return NULL;
    data->d3d = Direct3DCreate9( D3D_SDK_VERSION );

    IDirect3D9_CreateDevice( data->d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                             D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &data->d3ddev );
    IDirect3DDevice9_CreateVertexBuffer( data->d3ddev, 3 * sizeof(struct CUSTOMVERTEX), 0,
                                         CUSTOMFVF, D3DPOOL_MANAGED, &data->v_buffer, NULL );

    IDirect3DVertexBuffer9_Lock( data->v_buffer, 0, 0, (void **)&buffer_data, 0 );
    memcpy( buffer_data, flip ? flip_vertices : vertices, sizeof(vertices) );
    IDirect3DVertexBuffer9_Unlock( data->v_buffer );
    return data;
}

static void draw_d3d_window( struct d3d_window_data *data )
{
    IDirect3DDevice9_Clear( data->d3ddev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB( 0, 0, 0 ), 1.0f, 0 );
    /*
      IDirect3DDevice9_BeginScene(data->d3ddev);
      IDirect3DDevice9_SetFVF(data->d3ddev, CUSTOMFVF);
      IDirect3DDevice9_SetStreamSource(data->d3ddev, 0, data->v_buffer, 0, sizeof(struct
      CUSTOMVERTEX)); IDirect3DDevice9_DrawPrimitive(data->d3ddev, D3DPT_TRIANGLELIST, 0, 1);
      IDirect3DDevice9_EndScene(data->d3ddev);
    */
    IDirect3DDevice9_Present( data->d3ddev, NULL, NULL, NULL, NULL );
}

static void fini_d3d_window( struct d3d_window_data *data )
{
    IDirect3DVertexBuffer9_Release( data->v_buffer ); // close and release the vertex buffer
    IDirect3DDevice9_Release( data->d3ddev );         // close and release the 3D device
    IDirect3D9_Release( data->d3d );                  // close and release Direct3D
    free( data );
}
