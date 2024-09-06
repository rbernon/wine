/*
 * Copyright 2020 Alexandros Frantzis for Collabora Ltd.
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#include "x11drv.h"
#include "xcomposite.h"
#include "winternl.h"
#include "wine/debug.h"

#include <unistd.h>

#if defined(SONAME_LIBEGL)

WINE_DEFAULT_DEBUG_CHANNEL(wgl);

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/glxext.h>

#define WINE_VK_HOST
#include <vulkan/vulkan.h>

#include "wine/wgl_driver.h"
#include "wine/server.h"

static void *egl_handle;
static EGLDisplay egl_display;
static BOOL has_egl_ext_pixel_format_float;

#define DECL_FUNCPTR(f) static typeof(f) * p_##f
DECL_FUNCPTR(eglBindAPI);
DECL_FUNCPTR(eglChooseConfig);
DECL_FUNCPTR(eglCreateContext);
DECL_FUNCPTR(eglCreateImage);
DECL_FUNCPTR(eglCreateWindowSurface);
DECL_FUNCPTR(eglDestroyContext);
DECL_FUNCPTR(eglDestroyImage);
DECL_FUNCPTR(eglDestroySurface);
DECL_FUNCPTR(eglGetConfigAttrib);
DECL_FUNCPTR(eglGetCurrentContext);
DECL_FUNCPTR(eglGetCurrentSurface);
DECL_FUNCPTR(eglGetError);
DECL_FUNCPTR(eglGetPlatformDisplay);
DECL_FUNCPTR(eglGetProcAddress);
DECL_FUNCPTR(eglInitialize);
DECL_FUNCPTR(eglMakeCurrent);
DECL_FUNCPTR(eglQueryString);
DECL_FUNCPTR(eglSwapBuffers);
DECL_FUNCPTR(eglSwapInterval);
DECL_FUNCPTR(eglExportDMABUFImageQueryMESA);
DECL_FUNCPTR(eglExportDMABUFImageMESA);
#undef DECL_FUNCPTR

static BOOL has_extension(const char *list, const char *ext)
{
    size_t len = strlen(ext);
    const char *cur = list;

    while (cur && (cur = strstr(cur, ext)))
    {
        if ((!cur[len] || cur[len] == ' ') && (cur == list || cur[-1] == ' '))
            return TRUE;
        cur = strchr(cur, ' ');
    }

    return FALSE;
}

static void init_opengl(void)
{
    EGLint egl_version[2];
    const char *egl_client_exts, *egl_exts;

    if (!(egl_handle = dlopen(SONAME_LIBEGL, RTLD_NOW|RTLD_GLOBAL)))
    {
        ERR("Failed to load %s: %s\n", SONAME_LIBEGL, dlerror());
        return;
    }

#define LOAD_FUNCPTR_DLSYM(func) \
    do { \
        if (!(p_##func = dlsym(egl_handle, #func))) \
            { ERR("Failed to load symbol %s\n", #func); goto err; } \
    } while(0)
    LOAD_FUNCPTR_DLSYM(eglGetProcAddress);
    LOAD_FUNCPTR_DLSYM(eglQueryString);
#undef LOAD_FUNCPTR_DLSYM

    egl_client_exts = p_eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

#define REQUIRE_CLIENT_EXT(ext) \
    do { \
        if (!has_extension(egl_client_exts, #ext)) \
            { ERR("Failed to find required extension %s\n", #ext); goto err; } \
    } while(0)
    REQUIRE_CLIENT_EXT(EGL_KHR_client_get_all_proc_addresses);
    REQUIRE_CLIENT_EXT(EGL_KHR_platform_wayland);
#undef REQUIRE_CLIENT_EXT

#define LOAD_FUNCPTR_EGL(func) \
    do { \
        if (!(p_##func = (void *)p_eglGetProcAddress(#func))) \
            { ERR("Failed to load symbol %s\n", #func); goto err; } \
    } while(0)
    LOAD_FUNCPTR_EGL(eglBindAPI);
    LOAD_FUNCPTR_EGL(eglChooseConfig);
    LOAD_FUNCPTR_EGL(eglCreateContext);
    LOAD_FUNCPTR_EGL(eglCreateImage);
    LOAD_FUNCPTR_EGL(eglCreateWindowSurface);
    LOAD_FUNCPTR_EGL(eglDestroyContext);
    LOAD_FUNCPTR_EGL(eglDestroyImage);
    LOAD_FUNCPTR_EGL(eglDestroySurface);
    LOAD_FUNCPTR_EGL(eglGetConfigAttrib);
    LOAD_FUNCPTR_EGL(eglGetCurrentContext);
    LOAD_FUNCPTR_EGL(eglGetCurrentSurface);
    LOAD_FUNCPTR_EGL(eglGetError);
    LOAD_FUNCPTR_EGL(eglGetPlatformDisplay);
    LOAD_FUNCPTR_EGL(eglInitialize);
    LOAD_FUNCPTR_EGL(eglMakeCurrent);
    LOAD_FUNCPTR_EGL(eglSwapBuffers);
    LOAD_FUNCPTR_EGL(eglSwapInterval);
    LOAD_FUNCPTR_EGL(eglExportDMABUFImageQueryMESA);
    LOAD_FUNCPTR_EGL(eglExportDMABUFImageMESA);
#undef LOAD_FUNCPTR_EGL

    egl_display = p_eglGetPlatformDisplay( EGL_PLATFORM_X11_KHR, gdi_display, NULL );
    if (egl_display == EGL_NO_DISPLAY)
    {
        ERR("Failed to get EGLDisplay\n");
        goto err;
    }
    if (!p_eglInitialize(egl_display, &egl_version[0], &egl_version[1]))
    {
        ERR("Failed to initialized EGLDisplay with error %d\n", p_eglGetError());
        goto err;
    }
    TRACE("EGL version %u.%u\n", egl_version[0], egl_version[1]);

    egl_exts = p_eglQueryString(egl_display, EGL_EXTENSIONS);
#define REQUIRE_EXT(ext) \
    do { \
        if (!has_extension(egl_exts, #ext)) \
            { ERR("Failed to find required extension %s\n", #ext); goto err; } \
    } while(0)
    REQUIRE_EXT(EGL_KHR_create_context);
    REQUIRE_EXT(EGL_KHR_create_context_no_error);
    REQUIRE_EXT(EGL_KHR_no_config_context);
#undef REQUIRE_EXT

    has_egl_ext_pixel_format_float = has_extension(egl_exts, "EGL_EXT_pixel_format_float");

    return;

err:
    dlclose(egl_handle);
    egl_handle = NULL;
}

BOOL egl_import_pixmap(Pixmap pixmap)
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    struct client_surface_desc
    {
        UINT64 modifiers;
        UINT fourcc;
        UINT offset;
        UINT stride;
        int fd;
    } desc;
    EGLImage image;
    int planes;

    if (pthread_once(&init_once, init_opengl) || !egl_handle) return FALSE;

    image = p_eglCreateImage( egl_display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, (void *)(UINT_PTR)pixmap, NULL );
ERR("image %p\n", image);
p_eglExportDMABUFImageQueryMESA( egl_display, image, (int *)&desc.fourcc, &planes, &desc.modifiers );
ERR("fourcc %s\n", debugstr_fourcc(desc.fourcc));
p_eglExportDMABUFImageMESA( egl_display, image, &desc.fd, (int *)&desc.stride, (int *)&desc.offset );
ERR("fourcc %s modifiers %#jx fds %d strides %u offsets %u\n", debugstr_fourcc(desc.fourcc), desc.modifiers, desc.fd, desc.stride, desc.offset);
    p_eglDestroyImage( egl_display, image );

    image = p_eglCreateImage( egl_display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, (void *)(UINT_PTR)pixmap, NULL );
ERR("image %p\n", image);
p_eglExportDMABUFImageQueryMESA( egl_display, image, (int *)&desc.fourcc, &planes, &desc.modifiers );
ERR("fourcc %s\n", debugstr_fourcc(desc.fourcc));
p_eglExportDMABUFImageMESA( egl_display, image, &desc.fd, (int *)&desc.stride, (int *)&desc.offset );
ERR("fourcc %s modifiers %#jx fds %d strides %u offsets %u\n", debugstr_fourcc(desc.fourcc), desc.modifiers, desc.fd, desc.stride, desc.offset);
    p_eglDestroyImage( egl_display, image );

    return TRUE;
}

static inline GLuint import_texture_pixmap_egl( Pixmap pixmap )
{
    EGLImage image;
    GLuint texture;

    glGenTextures( 1, &texture );

    image = eglCreateImage( egl_display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)(UINT_PTR)pixmap, NULL );
    glEGLImageTargetTextureStorageEXT( texture, image, NULL );
    eglDestroyImage( egl_display, image );

    return texture;
}

static inline GLuint import_texture_pixmap_glx( Pixmap pixmap )
{
    GLXDrawable drawable;
    const int attribs[] =
    {
        GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
        GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
        GLX_NONE
    };
    GLint old_texture;
    GLuint texture;

    glGenTextures( 1, &texture );
    glGetIntegerv( GL_TEXTURE_2D_BINDING_EXT, &old_texture );

    drawable = glXCreatePixmap( gdi_display, 0, pixmap, attribs );
    glBindTexture( GL_TEXTURE_2D, texture );
    glXBindTexImageEXT( gdi_display, drawable, GLX_FRONT_LEFT_EXT, NULL );
    glXDestroyPixmap( gdi_display, drawable );

    glBindTexture( GL_TEXTURE_2D, old_texture );

    return texture;
}

static inline GLuint import_texture_resource( HANDLE handle )
{
    struct d3dkmt_desc desc;
    EGLAttrib attribs[17];
    EGLImage image;
    GLuint texture;
    UINT status;
    int fd;

    SERVER_START_REQ( get_d3dkmt_object_fd )
    {
        req->global = wine_server_obj_handle( handle );
        status = wine_server_call( req );
        desc = reply->desc;
    }
    SERVER_END_REQ;
    if (status) ERR("got status %#x\n", status);
    status = wine_server_handle_to_fd( handle, -1, &fd, NULL );
    if (status) ERR("got status %#x\n", status);

    glGenTextures( 1, &texture );

    if (desc.type == D3DKMT_FD_TYPE_UNIX_FD)
    {
        GLint dedicated = 1;
        GLuint memory;

        glCreateMemoryObjectsEXT( 1, &memory );
        glMemoryObjectParameterivEXT( memory, GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicated );
        glImportMemoryFdEXT( memory, desc.size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd );
        glTextureStorageMem2DEXT( texture, 1, desc.format, desc.width, desc.height, memory, 0 );
        glDeleteMemoryObjectsEXT( 1, &memory ); /* FIXME? */
    }
    else if (desc.type == D3DKMT_FD_TYPE_DMA_BUF)
    {
        attribs[0] = EGL_LINUX_DRM_FOURCC_EXT;
        attribs[1] = desc.format;
        attribs[2] = EGL_WIDTH;
        attribs[3] = desc.width;
        attribs[4] = EGL_HEIGHT;
        attribs[5] = desc.height;
        attribs[6] = EGL_DMA_BUF_PLANE0_FD_EXT;
        attribs[7] = fd;
        attribs[8] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
        attribs[9] = 0;
        attribs[10] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
        attribs[11] = desc.stride;
        attribs[12] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        attribs[13] = (UINT32)desc.flags;
        attribs[14] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        attribs[15] = desc.flags >> 32;
        attribs[16] = EGL_NONE;

        image = eglCreateImage( egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs );
        glEGLImageTargetTextureStorageEXT( texture, image, NULL );
        eglDestroyImage( egl_display, image );
    }
    else
    {
        assert(0);
    }

    close( fd );
    return texture;
}

static inline VkImage import_image_resource( VkDevice device, HANDLE handle )
{
    VkSubresourceLayout layout = {0};
    VkImageCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
    };
    VkImageDrmFormatModifierExplicitCreateInfoEXT drm_modifiers =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
        .drmFormatModifierPlaneCount = 1,
        .pPlaneLayouts = &layout,
    };
    VkExternalMemoryImageCreateInfo external = {.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    VkMemoryDedicatedAllocateInfo dedicated = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
    VkImportMemoryFdInfoKHR import =
    {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .pNext = &dedicated,
    };
    VkMemoryAllocateInfo allocate_info =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &import,
        .memoryTypeIndex = -1 /* FIXME */,
    };
    struct d3dkmt_desc desc;
    VkDeviceMemory memory;
    VkImage image;
    UINT status;
    int fd;

    SERVER_START_REQ( get_d3dkmt_object_fd )
    {
        req->global = wine_server_obj_handle( handle );
        status = wine_server_call( req );
        desc = reply->desc;
    }
    SERVER_END_REQ;
    if (status) ERR("got status %#x\n", status);
    status = wine_server_handle_to_fd( handle, -1, &fd, NULL );
    if (status) ERR("got status %#x\n", status);

    import.fd = fd;
    layout.size = desc.size;
    layout.rowPitch = desc.stride;
    create_info.extent.width = desc.width;
    create_info.extent.height = desc.height;
    drm_modifiers.drmFormatModifier = desc.flags;

    if (desc.type == D3DKMT_FD_TYPE_UNIX_FD)
    {
        external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    }
    else if (desc.type == D3DKMT_FD_TYPE_DMA_BUF)
    {
        external.pNext = &drm_modifiers;
        external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    }
    else
    {
        assert(0);
    }

    create_info.pNext = &external;
    vkCreateImage( device, &create_info, NULL, &image );

    dedicated.image = image;
    allocate_info.allocationSize = desc.size;
    vkAllocateMemory( device, &allocate_info, NULL, &memory );

    vkBindImageMemory( device, image, memory, 0 );

    close( fd );
    return image;
}

static inline Pixmap create_pixmap_dma_buf( int width, int height, HANDLE *handle )
{
    struct d3dkmt_desc desc = {.type = D3DKMT_FD_TYPE_DMA_BUF};
    int fd, fourcc, planes, strides[4], offsets[4];
    EGLImage image;
    Pixmap pixmap;
    UINT status;

    desc.width = width;
    desc.height = height;

    pixmap = XCreatePixmap( gdi_display, root_window, width, height, 32 );
    image = eglCreateImage( egl_display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)(UINT_PTR)pixmap, NULL );
    eglExportDMABUFImageQueryMESA( egl_display, image, &fourcc, &planes, &desc.flags );
    eglExportDMABUFImageMESA( egl_display, image, &fd, strides, offsets );
    eglDestroyImage( egl_display, image );

    desc.format = fourcc;
    desc.stride = strides[0];

    status = wine_server_fd_to_handle( fd, -1, desc.type|OBJ_KERNEL_HANDLE|OBJ_INHERIT, handle );
    if (status) ERR("got status %#x\n", status);
    SERVER_START_REQ( set_d3dkmt_object_fd )
    {
        req->handle = wine_server_obj_handle( handle );
        req->desc = desc;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (status) ERR("got status %#x\n", status);

    close( fd );
    return pixmap;
}

static inline GLuint create_renderbuffer_dma_buf( int width, int height, HANDLE *handle )
{
    struct d3dkmt_desc desc = {.type = D3DKMT_FD_TYPE_DMA_BUF};
    int fd, fourcc, planes, strides[4], offsets[4];
    GLuint renderbuffer;
    EGLImage image;
    UINT status;

    desc.width = width;
    desc.height = height;

    glGenRenderbuffers( 1, &renderbuffer );
    glNamedRenderbufferStorage( renderbuffer, GL_RGBA8, width, height );

    image = eglCreateImage( egl_display, EGL_NO_CONTEXT, EGL_GL_RENDERBUFFER, (EGLClientBuffer)(UINT_PTR)renderbuffer, NULL );
    eglExportDMABUFImageQueryMESA( egl_display, image, &fourcc, &planes, &desc.flags );
    eglExportDMABUFImageMESA( egl_display, image, &fd, strides, offsets );
    eglDestroyImage( egl_display, image );

    desc.format = fourcc;
    desc.stride = strides[0];

    status = wine_server_fd_to_handle( fd, -1, desc.type|OBJ_KERNEL_HANDLE|OBJ_INHERIT, handle );
    if (status) ERR("got status %#x\n", status);
    SERVER_START_REQ( set_d3dkmt_object_fd )
    {
        req->handle = wine_server_obj_handle( handle );
        req->desc = desc;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (status) ERR("got status %#x\n", status);

    close( fd );
    return renderbuffer;
}

static inline GLuint create_texture_dma_buf( int width, int height, HANDLE *handle )
{
    struct d3dkmt_desc desc = {.type = D3DKMT_FD_TYPE_DMA_BUF};
    int fd, fourcc, planes, strides[4], offsets[4];
    GLuint texture;
    EGLImage image;
    UINT status;

    desc.width = width;
    desc.height = height;

    glGenTextures( 1, &texture );
    glTextureStorage2D( texture, 1, GL_RGBA8, width, height );

    image = eglCreateImage( egl_display, EGL_NO_CONTEXT, EGL_GL_TEXTURE_2D, (EGLClientBuffer)(UINT_PTR)texture, NULL );
    eglExportDMABUFImageQueryMESA( egl_display, image, &fourcc, &planes, &desc.flags );
    eglExportDMABUFImageMESA( egl_display, image, &fd, strides, offsets );
    eglDestroyImage( egl_display, image );

    desc.format = fourcc;
    desc.stride = strides[0];

    status = wine_server_fd_to_handle( fd, -1, desc.type|OBJ_KERNEL_HANDLE|OBJ_INHERIT, handle );
    if (status) ERR("got status %#x\n", status);
    SERVER_START_REQ( set_d3dkmt_object_fd )
    {
        req->handle = wine_server_obj_handle( handle );
        req->desc = desc;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (status) ERR("got status %#x\n", status);

    close( fd );
    return texture;
}

static inline VkImage create_image_dma_buf( VkDevice device, int width, int height, HANDLE *handle )
{
    VkExternalMemoryImageCreateInfo external =
    {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo create =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external,
.flags = 0,
.imageType = 0,
.format = 0,
        .extent = {.width = width, .height = height},
.mipLevels = 0,
.arrayLayers = 0,
.samples = 0,
.tiling = 0,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
.sharingMode = 0,
.queueFamilyIndexCount = 0,
.pQueueFamilyIndices = 0,
.initialLayout = 0,
    };

    VkMemoryDedicatedAllocateInfo dedicated = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
    VkExportMemoryAllocateInfo export =
    {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkMemoryAllocateInfo allocate =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &export,
        .memoryTypeIndex = -1 /* FIXME */,
    };

    VkImageDrmFormatModifierPropertiesEXT properties = {.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT};
    VkMemoryRequirements requirements = {0};
    VkImageSubresource subresource = {0};
    VkSubresourceLayout layout = {0};

    VkMemoryGetFdInfoKHR get_fd =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    struct d3dkmt_desc desc = {.type = D3DKMT_FD_TYPE_DMA_BUF};
    VkDeviceMemory memory;
    VkImage image;
    UINT status;
    int fd;

    desc.width = width;
    desc.height = height;
    desc.format = -1 /* FIXME */;

    vkCreateImage( device, &create, NULL, &image );

    vkGetImageDrmFormatModifierPropertiesEXT( device, image, &properties );
    desc.flags = properties.drmFormatModifier;
    vkGetImageMemoryRequirements( device, image, &requirements );
    desc.size = requirements.size;
    vkGetImageSubresourceLayout( device, image, &subresource, &layout );
    desc.stride = layout.rowPitch;

    dedicated.image = image;
    allocate.memoryTypeIndex = requirements.memoryTypeBits;
    allocate.allocationSize = layout.size;
    vkAllocateMemory( device, &allocate, NULL, &memory );

    vkBindImageMemory( device, image, memory, 0 );

    get_fd.memory = memory;
    vkGetMemoryFdKHR( device, &get_fd, &fd );

    status = wine_server_fd_to_handle( fd, -1, desc.type|OBJ_KERNEL_HANDLE|OBJ_INHERIT, handle );
    if (status) ERR("got status %#x\n", status);
    SERVER_START_REQ( set_d3dkmt_object_fd )
    {
        req->handle = wine_server_obj_handle( handle );
        req->desc = desc;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (status) ERR("got status %#x\n", status);

    close( fd );
    return image;
}

static inline VkImage create_image_vk_mem( VkDevice device, int width, int height, HANDLE *handle )
{
    VkExternalMemoryImageCreateInfo external =
    {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
    };
    VkImageCreateInfo create =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external,
.flags = 0,
.imageType = 0,
.format = 0,
        .extent = {.width = width, .height = height},
.mipLevels = 0,
.arrayLayers = 0,
.samples = 0,
.tiling = 0,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
.sharingMode = 0,
.queueFamilyIndexCount = 0,
.pQueueFamilyIndices = 0,
.initialLayout = 0,
    };

    VkMemoryDedicatedAllocateInfo dedicated = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
    VkExportMemoryAllocateInfo export =
    {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
    };
    VkMemoryAllocateInfo allocate =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &export,
        .memoryTypeIndex = -1 /* FIXME */,
    };

    VkMemoryGetFdInfoKHR get_fd =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
    };
    struct d3dkmt_desc desc = {.type = D3DKMT_FD_TYPE_UNIX_FD};
    VkMemoryRequirements requirements = {0};
    VkDeviceMemory memory;
    VkImage image;
    UINT status;
    int fd;

    desc.width = width;
    desc.height = height;

    vkCreateImage( device, &create, NULL, &image );
    vkGetImageMemoryRequirements( device, image, &requirements );
    desc.size = requirements.size;

    dedicated.image = image;
    allocate.allocationSize = requirements.size;
    vkAllocateMemory( device, &allocate, NULL, &memory );

    vkBindImageMemory( device, image, memory, 0 );

    get_fd.memory = memory;
    vkGetMemoryFdKHR( device, &get_fd, &fd );

    wine_server_fd_to_handle( fd, -1, desc.type|OBJ_KERNEL_HANDLE|OBJ_INHERIT, handle );
    SERVER_START_REQ( set_d3dkmt_object_fd )
    {
        req->handle = wine_server_obj_handle( handle );
        req->desc = desc;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (status) ERR("got status %#x\n", status);

    close( fd );
    return image;
}

#else /* No GL */

BOOL egl_import_pixmap(Pixmap pixmap)
{
    return FALSE;
}

#endif
