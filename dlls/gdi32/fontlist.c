/*
 * FreeType font engine interface
 *
 * Copyright 2001 Huw D M Davies for CodeWeavers.
 * Copyright 2006 Dmitry Timoshkov for CodeWeavers.
 *
 * This file contains the WineEng* functions.
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
#include "wine/port.h"

#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#include <string.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif
#include <stdio.h>
#include <assert.h>

#ifdef HAVE_CARBON_CARBON_H
#define LoadResource __carbon_LoadResource
#define CompareString __carbon_CompareString
#define GetCurrentThread __carbon_GetCurrentThread
#define GetCurrentProcess __carbon_GetCurrentProcess
#define AnimatePalette __carbon_AnimatePalette
#define EqualRgn __carbon_EqualRgn
#define FillRgn __carbon_FillRgn
#define FrameRgn __carbon_FrameRgn
#define GetPixel __carbon_GetPixel
#define InvertRgn __carbon_InvertRgn
#define LineTo __carbon_LineTo
#define OffsetRgn __carbon_OffsetRgn
#define PaintRgn __carbon_PaintRgn
#define Polygon __carbon_Polygon
#define ResizePalette __carbon_ResizePalette
#define SetRectRgn __carbon_SetRectRgn
#include <Carbon/Carbon.h>
#undef LoadResource
#undef CompareString
#undef GetCurrentThread
#undef _CDECL
#undef GetCurrentProcess
#undef AnimatePalette
#undef EqualRgn
#undef FillRgn
#undef FrameRgn
#undef GetPixel
#undef InvertRgn
#undef LineTo
#undef OffsetRgn
#undef PaintRgn
#undef Polygon
#undef ResizePalette
#undef SetRectRgn
#endif /* HAVE_CARBON_CARBON_H */

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winerror.h"
#include "winreg.h"
#include "wingdi.h"
#include "ntgdi_private.h"
#include "wine/debug.h"
#include "wine/list.h"

#include "resource.h"

WINE_DEFAULT_DEBUG_CHANNEL(font);

#define MAKE_FUNCPTR(f) static typeof(f) * p##f = NULL

#ifdef SONAME_LIBFONTCONFIG
#include <fontconfig/fontconfig.h>
MAKE_FUNCPTR(FcConfigSubstitute);
MAKE_FUNCPTR(FcDefaultSubstitute);
MAKE_FUNCPTR(FcFontList);
MAKE_FUNCPTR(FcFontMatch);
MAKE_FUNCPTR(FcFontSetDestroy);
MAKE_FUNCPTR(FcInit);
MAKE_FUNCPTR(FcPatternAddString);
MAKE_FUNCPTR(FcPatternCreate);
MAKE_FUNCPTR(FcPatternDestroy);
MAKE_FUNCPTR(FcPatternGetBool);
MAKE_FUNCPTR(FcPatternGetInteger);
MAKE_FUNCPTR(FcPatternGetString);
MAKE_FUNCPTR(FcConfigGetFontDirs);
MAKE_FUNCPTR(FcConfigGetCurrent);
MAKE_FUNCPTR(FcCacheCopySet);
MAKE_FUNCPTR(FcCacheNumSubdir);
MAKE_FUNCPTR(FcCacheSubdir);
MAKE_FUNCPTR(FcDirCacheRead);
MAKE_FUNCPTR(FcDirCacheUnload);
MAKE_FUNCPTR(FcStrListCreate);
MAKE_FUNCPTR(FcStrListDone);
MAKE_FUNCPTR(FcStrListNext);
MAKE_FUNCPTR(FcStrSetAdd);
MAKE_FUNCPTR(FcStrSetCreate);
MAKE_FUNCPTR(FcStrSetDestroy);
MAKE_FUNCPTR(FcStrSetMember);
#ifndef FC_NAMELANG
#define FC_NAMELANG "namelang"
#endif
#ifndef FC_PRGNAME
#define FC_PRGNAME "prgname"
#endif
#endif /* SONAME_LIBFONTCONFIG */

#undef MAKE_FUNCPTR

#ifndef FT_MAKE_TAG
#define FT_MAKE_TAG( ch0, ch1, ch2, ch3 ) \
	( ((DWORD)(BYTE)(ch0) << 24) | ((DWORD)(BYTE)(ch1) << 16) | \
	  ((DWORD)(BYTE)(ch2) << 8) | (DWORD)(BYTE)(ch3) )
#endif

#ifndef ft_encoding_none
#define FT_ENCODING_NONE ft_encoding_none
#endif
#ifndef ft_encoding_ms_symbol
#define FT_ENCODING_MS_SYMBOL ft_encoding_symbol
#endif
#ifndef ft_encoding_unicode
#define FT_ENCODING_UNICODE ft_encoding_unicode
#endif
#ifndef ft_encoding_apple_roman
#define FT_ENCODING_APPLE_ROMAN ft_encoding_apple_roman
#endif

#ifdef WORDS_BIGENDIAN
#define GET_BE_WORD(x) (x)
#define GET_BE_DWORD(x) (x)
#else
#define GET_BE_WORD(x) RtlUshortByteSwap(x)
#define GET_BE_DWORD(x) RtlUlongByteSwap(x)
#endif

/* 'gasp' flags */
#define GASP_GRIDFIT 0x01
#define GASP_DOGRAY  0x02

struct font_private_data
{
    struct font_mapping *mapping;
};

static const struct font_callback_funcs *callback_funcs;

struct font_mapping
{
    struct list entry;
    int         refcount;
    dev_t       dev;
    ino_t       ino;
    void       *data;
    size_t      size;
};

static struct list mappings_list = LIST_INIT( mappings_list );

static UINT default_aa_flags;

/****************************************
 *   Notes on .fon files
 *
 * The fonts System, FixedSys and Terminal are special.  There are typically multiple
 * versions installed for different resolutions and codepages.  Windows stores which one to use
 * in HKEY_CURRENT_CONFIG\\Software\\Fonts.
 *    Key            Meaning
 *  FIXEDFON.FON    FixedSys
 *  FONTS.FON       System
 *  OEMFONT.FON     Terminal
 *  LogPixels       Current dpi set by the display control panel applet
 *                  (HKLM\\Software\\Microsoft\\Windows NT\\CurrentVersion\\FontDPI
 *                  also has a LogPixels value that appears to mirror this)
 *
 * On my system these values have data: vgafix.fon, vgasys.fon, vga850.fon and 96 respectively
 * (vgaoem.fon would be your oemfont.fon if you have a US setup).
 * If the resolution is changed to be >= 109dpi then the fonts goto 8514fix, 8514sys and 8514oem
 * (not sure what's happening to the oem codepage here). 109 is nicely halfway between 96 and 120dpi,
 * so that makes sense.
 *
 * Additionally Windows also loads the fonts listed in the [386enh] section of system.ini (this doesn't appear
 * to be mapped into the registry on Windows 2000 at least).
 * I have
 * woafont=app850.fon
 * ega80woa.fon=ega80850.fon
 * ega40woa.fon=ega40850.fon
 * cga80woa.fon=cga80850.fon
 * cga40woa.fon=cga40850.fon
 */

#ifdef HAVE_CARBON_CARBON_H
static char *find_cache_dir(void)
{
    FSRef ref;
    OSErr err;
    static char cached_path[MAX_PATH];
    static const char *wine = "/Wine", *fonts = "/Fonts";

    if(*cached_path) return cached_path;

    err = FSFindFolder(kUserDomain, kCachedDataFolderType, kCreateFolder, &ref);
    if(err != noErr)
    {
        WARN("can't create cached data folder\n");
        return NULL;
    }
    err = FSRefMakePath(&ref, (unsigned char*)cached_path, sizeof(cached_path));
    if(err != noErr)
    {
        WARN("can't create cached data path\n");
        *cached_path = '\0';
        return NULL;
    }
    if(strlen(cached_path) + strlen(wine) + strlen(fonts) + 1 > sizeof(cached_path))
    {
        ERR("Could not create full path\n");
        *cached_path = '\0';
        return NULL;
    }
    strcat(cached_path, wine);

    if(mkdir(cached_path, 0700) == -1 && errno != EEXIST)
    {
        WARN("Couldn't mkdir %s\n", cached_path);
        *cached_path = '\0';
        return NULL;
    }
    strcat(cached_path, fonts);
    if(mkdir(cached_path, 0700) == -1 && errno != EEXIST)
    {
        WARN("Couldn't mkdir %s\n", cached_path);
        *cached_path = '\0';
        return NULL;
    }
    return cached_path;
}

/******************************************************************
 *            expand_mac_font
 *
 * Extracts individual TrueType font files from a Mac suitcase font
 * and saves them into the user's caches directory (see
 * find_cache_dir()).
 * Returns a NULL terminated array of filenames.
 *
 * We do this because they are apps that try to read ttf files
 * themselves and they don't like Mac suitcase files.
 */
static char **expand_mac_font(const char *path)
{
    FSRef ref;
    ResFileRefNum res_ref;
    OSStatus s;
    unsigned int idx;
    const char *out_dir;
    const char *filename;
    int output_len;
    struct {
        char **array;
        unsigned int size, max_size;
    } ret;

    TRACE("path %s\n", path);

    s = FSPathMakeRef((unsigned char*)path, &ref, FALSE);
    if(s != noErr)
    {
        WARN("failed to get ref\n");
        return NULL;
    }

    s = FSOpenResourceFile(&ref, 0, NULL, fsRdPerm, &res_ref);
    if(s != noErr)
    {
        TRACE("no data fork, so trying resource fork\n");
        res_ref = FSOpenResFile(&ref, fsRdPerm);
        if(res_ref == -1)
        {
            TRACE("unable to open resource fork\n");
            return NULL;
        }
    }

    ret.size = 0;
    ret.max_size = 10;
    ret.array = RtlAllocateHeap(GetProcessHeap(), HEAP_ZERO_MEMORY, ret.max_size * sizeof(*ret.array));
    if(!ret.array)
    {
        CloseResFile(res_ref);
        return NULL;
    }

    out_dir = find_cache_dir();

    filename = strrchr(path, '/');
    if(!filename) filename = path;
    else filename++;

    /* output filename has the form out_dir/filename_%04x.ttf */
    output_len = strlen(out_dir) + 1 + strlen(filename) + 5 + 5;

    UseResFile(res_ref);
    idx = 1;
    while(1)
    {
        FamRec *fam_rec;
        unsigned short *num_faces_ptr, num_faces, face;
        AsscEntry *assoc;
        Handle fond;
        ResType fond_res = FT_MAKE_TAG('F','O','N','D');

        fond = Get1IndResource(fond_res, idx);
        if(!fond) break;
        TRACE("got fond resource %d\n", idx);
        HLock(fond);

        fam_rec = *(FamRec**)fond;
        num_faces_ptr = (unsigned short *)(fam_rec + 1);
        num_faces = GET_BE_WORD(*num_faces_ptr);
        num_faces++;
        assoc = (AsscEntry*)(num_faces_ptr + 1);
        TRACE("num faces %04x\n", num_faces);
        for(face = 0; face < num_faces; face++, assoc++)
        {
            Handle sfnt;
            ResType sfnt_res = FT_MAKE_TAG('s','f','n','t');
            unsigned short size, font_id;
            char *output;

            size = GET_BE_WORD(assoc->fontSize);
            font_id = GET_BE_WORD(assoc->fontID);
            if(size != 0)
            {
                TRACE("skipping id %04x because it's not scalable (fixed size %d)\n", font_id, size);
                continue;
            }

            TRACE("trying to load sfnt id %04x\n", font_id);
            sfnt = GetResource(sfnt_res, font_id);
            if(!sfnt)
            {
                TRACE("can't get sfnt resource %04x\n", font_id);
                continue;
            }

            output = RtlAllocateHeap(GetProcessHeap(), 0, output_len);
            if(output)
            {
                int fd;

                sprintf(output, "%s/%s_%04x.ttf", out_dir, filename, font_id);

                fd = open(output, O_CREAT | O_EXCL | O_WRONLY, 0600);
                if(fd != -1 || errno == EEXIST)
                {
                    if(fd != -1)
                    {
                        unsigned char *sfnt_data;

                        HLock(sfnt);
                        sfnt_data = *(unsigned char**)sfnt;
                        write(fd, sfnt_data, GetHandleSize(sfnt));
                        HUnlock(sfnt);
                        close(fd);
                    }
                    if(ret.size >= ret.max_size - 1) /* Always want the last element to be NULL */
                    {
                        ret.max_size *= 2;
                        ret.array = RtlReAllocateHeap(GetProcessHeap(), HEAP_ZERO_MEMORY, ret.array, ret.max_size * sizeof(*ret.array));
                    }
                    ret.array[ret.size++] = output;
                }
                else
                {
                    WARN("unable to create %s\n", output);
                    RtlFreeHeap(GetProcessHeap(), 0, output);
                }
            }
            ReleaseResource(sfnt);
        }
        HUnlock(fond);
        ReleaseResource(fond);
        idx++;
    }
    CloseResFile(res_ref);

    return ret.array;
}

#endif /* HAVE_CARBON_CARBON_H */

static int add_unix_face( const char *unix_name, const WCHAR *file, void *data_ptr, DWORD data_size,
                          DWORD face_index, DWORD flags, DWORD *num_faces )
{
    struct stat st;
    int fd, ret;

    if (num_faces) *num_faces = 0;

    TRACE( "unix_name %s, face_index %u, data_ptr %p, data_size %u, flags %#x\n",
           unix_name, face_index, data_ptr, data_size, flags );

    if (unix_name)
    {
        if ((fd = open( unix_name, O_RDONLY )) == -1) return 0;
        if (fstat( fd, &st ) == -1)
        {
            close( fd );
            return 0;
        }
        data_size = st.st_size;
        data_ptr = mmap( 0, data_size, PROT_READ, MAP_PRIVATE, fd, 0 );
        close( fd );
        if (data_ptr == MAP_FAILED) return 0;
    }

    if (!HIWORD( flags )) flags |= ADDFONT_AA_FLAGS( default_aa_flags );

    ret = callback_funcs->parse_gdi_face( file, data_ptr, data_size, face_index, flags, num_faces );

    if (unix_name) munmap( data_ptr, data_size );
    return ret;
}

static WCHAR *get_dos_file_name( LPCSTR str )
{
    WCHAR *buffer;
    ULONG len = strlen(str) + 1;

    len += 8;  /* \??\unix prefix */
    if (!(buffer = RtlAllocateHeap( GetProcessHeap(), 0, len * sizeof(WCHAR) ))) return NULL;
    if (wine_unix_to_nt_file_name( str, buffer, &len ))
    {
        RtlFreeHeap( GetProcessHeap(), 0, buffer );
        return NULL;
    }
    if (buffer[5] == ':')
    {
        /* get rid of the \??\ prefix */
        /* FIXME: should implement RtlNtPathNameToDosPathName and use that instead */
        memmove( buffer, buffer + 4, (len - 4) * sizeof(WCHAR) );
    }
    else buffer[1] = '\\';
    return buffer;
}

static char *get_unix_file_name( LPCWSTR dosW )
{
    UNICODE_STRING nt_name;
    OBJECT_ATTRIBUTES attr;
    NTSTATUS status;
    ULONG size = 256;
    char *buffer;

    if (!RtlDosPathNameToNtPathName_U( dosW, &nt_name, NULL, NULL )) return NULL;
    InitializeObjectAttributes( &attr, &nt_name, 0, 0, NULL );
    for (;;)
    {
        if (!(buffer = RtlAllocateHeap( GetProcessHeap(), 0, size )))
        {
            RtlFreeUnicodeString( &nt_name );
            return NULL;
        }
        status = wine_nt_to_unix_file_name( &attr, buffer, &size, FILE_OPEN_IF );
        if (status != STATUS_BUFFER_TOO_SMALL) break;
        RtlFreeHeap( GetProcessHeap(), 0, buffer );
    }
    RtlFreeUnicodeString( &nt_name );
    if (status && status != STATUS_NO_SUCH_FILE)
    {
        RtlFreeHeap( GetProcessHeap(), 0, buffer );
        RtlSetLastWin32ErrorAndNtStatusFromNtStatus( status );
        return NULL;
    }
    return buffer;
}

static INT AddFontToList(const WCHAR *dos_name, const char *unix_name, void *font_data_ptr,
                         DWORD font_data_size, DWORD flags)
{
    DWORD face_index = 0, num_faces;
    INT ret = 0;
    WCHAR *filename = NULL;

    /* we always load external fonts from files - otherwise we would get a crash in update_reg_entries */
    assert(unix_name || !(flags & ADDFONT_EXTERNAL_FONT));

#ifdef HAVE_CARBON_CARBON_H
    if(unix_name)
    {
        char **mac_list = expand_mac_font(unix_name);
        if(mac_list)
        {
            BOOL had_one = FALSE;
            char **cursor;
            for(cursor = mac_list; *cursor; cursor++)
            {
                had_one = TRUE;
                AddFontToList(NULL, *cursor, NULL, 0, flags);
                RtlFreeHeap(GetProcessHeap(), 0, *cursor);
            }
            RtlFreeHeap(GetProcessHeap(), 0, mac_list);
            if(had_one)
                return 1;
        }
    }
#endif /* HAVE_CARBON_CARBON_H */

    if (!dos_name && unix_name) dos_name = filename = get_dos_file_name( unix_name );

    do
        ret += add_unix_face( unix_name, dos_name, font_data_ptr, font_data_size, face_index, flags, &num_faces );
    while (num_faces > ++face_index);

    RtlFreeHeap( GetProcessHeap(), 0, filename );
    return ret;
}

/*************************************************************
 * freetype_add_font
 */
static INT CDECL freetype_add_font( const WCHAR *file, DWORD flags )
{
    int ret = 0;
    char *unixname = get_unix_file_name( file );

    if (unixname)
    {
        ret = AddFontToList( file, unixname, NULL, 0, flags );
        RtlFreeHeap( GetProcessHeap(), 0, unixname );
    }
    return ret;
}

/*************************************************************
 * freetype_add_mem_font
 */
static INT CDECL freetype_add_mem_font( void *ptr, SIZE_T size, DWORD flags )
{
    return AddFontToList( NULL, NULL, ptr, size, flags );
}

#ifdef __ANDROID__
static BOOL ReadFontDir(const char *dirname, BOOL external_fonts)
{
    DIR *dir;
    struct dirent *dent;
    char path[MAX_PATH];

    TRACE("Loading fonts from %s\n", debugstr_a(dirname));

    dir = opendir(dirname);
    if(!dir) {
        WARN("Can't open directory %s\n", debugstr_a(dirname));
	return FALSE;
    }
    while((dent = readdir(dir)) != NULL) {
	struct stat statbuf;

        if(!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
	    continue;

	TRACE("Found %s in %s\n", debugstr_a(dent->d_name), debugstr_a(dirname));

	sprintf(path, "%s/%s", dirname, dent->d_name);

	if(stat(path, &statbuf) == -1)
	{
	    WARN("Can't stat %s\n", debugstr_a(path));
	    continue;
	}
	if(S_ISDIR(statbuf.st_mode))
	    ReadFontDir(path, external_fonts);
	else
        {
            DWORD addfont_flags = 0;
            if(external_fonts) addfont_flags |= ADDFONT_EXTERNAL_FONT;
            AddFontToList(NULL, path, NULL, 0, addfont_flags);
        }
    }
    closedir(dir);
    return TRUE;
}
#endif

#ifdef SONAME_LIBFONTCONFIG

static BOOL fontconfig_enabled;
static FcPattern *pattern_serif;
static FcPattern *pattern_fixed;
static FcPattern *pattern_sans;

static UINT parse_aa_pattern( FcPattern *pattern )
{
    FcBool antialias;
    int rgba;
    UINT aa_flags = 0;

    if (pFcPatternGetBool( pattern, FC_ANTIALIAS, 0, &antialias ) == FcResultMatch)
        aa_flags = antialias ? GGO_GRAY4_BITMAP : GGO_BITMAP;

    if (pFcPatternGetInteger( pattern, FC_RGBA, 0, &rgba ) == FcResultMatch)
    {
        switch (rgba)
        {
        case FC_RGBA_RGB:  aa_flags = WINE_GGO_HRGB_BITMAP; break;
        case FC_RGBA_BGR:  aa_flags = WINE_GGO_HBGR_BITMAP; break;
        case FC_RGBA_VRGB: aa_flags = WINE_GGO_VRGB_BITMAP; break;
        case FC_RGBA_VBGR: aa_flags = WINE_GGO_VBGR_BITMAP; break;
        case FC_RGBA_NONE: aa_flags = aa_flags ? aa_flags : GGO_GRAY4_BITMAP; break;
        }
    }
    return aa_flags;
}

static FcPattern *create_family_pattern( const char *name, FcPattern **cached )
{
    FcPattern *ret = NULL, *tmp, *pattern;
    FcResult result;
    if (*cached) return *cached;
    pattern = pFcPatternCreate();
    pFcPatternAddString( pattern, FC_FAMILY, (const FcChar8 *)name );
    pFcPatternAddString( pattern, FC_NAMELANG, (const FcChar8 *)"en-us" );
    pFcPatternAddString( pattern, FC_PRGNAME, (const FcChar8 *)"wine" );
    pFcConfigSubstitute( NULL, pattern, FcMatchPattern );
    pFcDefaultSubstitute( pattern );
    tmp = pFcFontMatch( NULL, pattern, &result );
    pFcPatternDestroy( pattern );
    if (result != FcResultMatch) pFcPatternDestroy( tmp );
    else if ((ret = InterlockedCompareExchangePointer( (void **)cached, tmp, NULL ))) pFcPatternDestroy( tmp );
    else ret = tmp;
    return ret;
}

static void fontconfig_add_font( FcPattern *pattern, DWORD flags )
{
    const char *unix_name, *format;
    WCHAR *dos_name;
    FcBool scalable;
    DWORD aa_flags;
    int face_index;

    TRACE( "(%p %#x)\n", pattern, flags );

    if (pFcPatternGetString( pattern, FC_FILE, 0, (FcChar8 **)&unix_name ) != FcResultMatch)
        return;

    if (pFcPatternGetBool( pattern, FC_SCALABLE, 0, &scalable ) != FcResultMatch)
        scalable = FALSE;

    if (pFcPatternGetString( pattern, FC_FONTFORMAT, 0, (FcChar8 **)&format ) != FcResultMatch)
    {
        TRACE( "ignoring unknown font format %s\n", debugstr_a(unix_name) );
        return;
    }

    if (!strcmp( format, "Type 1" ))
    {
        TRACE( "ignoring Type 1 font %s\n", debugstr_a(unix_name) );
        return;
    }

    if (!scalable && !(flags & ADDFONT_ALLOW_BITMAP))
    {
        TRACE( "ignoring non-scalable font %s\n", debugstr_a(unix_name) );
        return;
    }

    if (!(aa_flags = parse_aa_pattern( pattern ))) aa_flags = default_aa_flags;
    flags |= ADDFONT_AA_FLAGS(aa_flags);

    if (pFcPatternGetInteger( pattern, FC_INDEX, 0, &face_index ) != FcResultMatch)
        face_index = 0;

    dos_name = get_dos_file_name( unix_name );
    add_unix_face( unix_name, dos_name, NULL, 0, face_index, flags, NULL );
    RtlFreeHeap( GetProcessHeap(), 0, dos_name );
}

static void init_fontconfig(void)
{
    void *fc_handle = dlopen(SONAME_LIBFONTCONFIG, RTLD_NOW);

    if (!fc_handle)
    {
        TRACE("Wine cannot find the fontconfig library (%s).\n", SONAME_LIBFONTCONFIG);
        return;
    }

#define LOAD_FUNCPTR(f) if((p##f = dlsym(fc_handle, #f)) == NULL){WARN("Can't find symbol %s\n", #f); return;}
    LOAD_FUNCPTR(FcConfigSubstitute);
    LOAD_FUNCPTR(FcDefaultSubstitute);
    LOAD_FUNCPTR(FcFontList);
    LOAD_FUNCPTR(FcFontMatch);
    LOAD_FUNCPTR(FcFontSetDestroy);
    LOAD_FUNCPTR(FcInit);
    LOAD_FUNCPTR(FcPatternAddString);
    LOAD_FUNCPTR(FcPatternCreate);
    LOAD_FUNCPTR(FcPatternDestroy);
    LOAD_FUNCPTR(FcPatternGetBool);
    LOAD_FUNCPTR(FcPatternGetInteger);
    LOAD_FUNCPTR(FcPatternGetString);
    LOAD_FUNCPTR(FcConfigGetFontDirs);
    LOAD_FUNCPTR(FcConfigGetCurrent);
    LOAD_FUNCPTR(FcCacheCopySet);
    LOAD_FUNCPTR(FcCacheNumSubdir);
    LOAD_FUNCPTR(FcCacheSubdir);
    LOAD_FUNCPTR(FcDirCacheRead);
    LOAD_FUNCPTR(FcDirCacheUnload);
    LOAD_FUNCPTR(FcStrListCreate);
    LOAD_FUNCPTR(FcStrListDone);
    LOAD_FUNCPTR(FcStrListNext);
    LOAD_FUNCPTR(FcStrSetAdd);
    LOAD_FUNCPTR(FcStrSetCreate);
    LOAD_FUNCPTR(FcStrSetDestroy);
    LOAD_FUNCPTR(FcStrSetMember);
#undef LOAD_FUNCPTR

    if (pFcInit())
    {
        FcPattern *pattern = pFcPatternCreate();
        pFcConfigSubstitute( NULL, pattern, FcMatchFont );
        default_aa_flags = parse_aa_pattern( pattern );
        pFcPatternDestroy( pattern );

        if (!default_aa_flags)
        {
            FcPattern *pattern = pFcPatternCreate();
            pFcConfigSubstitute( NULL, pattern, FcMatchPattern );
            default_aa_flags = parse_aa_pattern( pattern );
            pFcPatternDestroy( pattern );
        }

        TRACE( "enabled, default flags = %x\n", default_aa_flags );
        fontconfig_enabled = TRUE;
    }
}

static void fontconfig_add_fonts_from_dir_list( FcConfig *config, FcStrList *dir_list, FcStrSet *done_set, DWORD flags )
{
    const FcChar8 *dir;
    FcFontSet *font_set = NULL;
    FcStrList *subdir_list = NULL;
    FcStrSet *subdir_set = NULL;
    FcCache *cache = NULL;
    int i;

    TRACE( "(%p %p %p %#x)\n", config, dir_list, done_set, flags );

    while ((dir = pFcStrListNext( dir_list )))
    {
        if (pFcStrSetMember( done_set, dir )) continue;

        TRACE( "adding fonts from %s\n", dir );
        if (!(cache = pFcDirCacheRead( dir, FcFalse, config ))) continue;

        if (!(font_set = pFcCacheCopySet( cache ))) goto done;
        for (i = 0; i < font_set->nfont; i++)
            fontconfig_add_font( font_set->fonts[i], flags );
        pFcFontSetDestroy( font_set );
        font_set = NULL;

        if (!(subdir_set = pFcStrSetCreate())) goto done;
        for (i = 0; i < pFcCacheNumSubdir( cache ); i++)
            pFcStrSetAdd( subdir_set, pFcCacheSubdir( cache, i ) );
        pFcDirCacheUnload( cache );
        cache = NULL;

        if (!(subdir_list = pFcStrListCreate( subdir_set ))) goto done;
        pFcStrSetDestroy( subdir_set );
        subdir_set = NULL;

        pFcStrSetAdd( done_set, dir );
        fontconfig_add_fonts_from_dir_list( config, subdir_list, done_set, flags );
        pFcStrListDone( subdir_list );
        subdir_list = NULL;
    }

done:
    if (subdir_set) pFcStrSetDestroy( subdir_set );
    if (cache) pFcDirCacheUnload( cache );
}

static void load_fontconfig_fonts( void )
{
    FcStrList *dir_list = NULL;
    FcStrSet *done_set = NULL;
    FcConfig *config;

    if (!fontconfig_enabled) return;
    if (!(config = pFcConfigGetCurrent())) goto done;
    if (!(done_set = pFcStrSetCreate())) goto done;
    if (!(dir_list = pFcConfigGetFontDirs( config ))) goto done;

    fontconfig_add_fonts_from_dir_list( config, dir_list, done_set, ADDFONT_EXTERNAL_FONT );

done:
    if (dir_list) pFcStrListDone( dir_list );
    if (done_set) pFcStrSetDestroy( done_set );
}

#elif defined(HAVE_CARBON_CARBON_H)

static void load_mac_font_callback(const void *value, void *context)
{
    CFStringRef pathStr = value;
    CFIndex len;
    char* path;

    len = CFStringGetMaximumSizeOfFileSystemRepresentation(pathStr);
    path = RtlAllocateHeap(GetProcessHeap(), 0, len);
    if (path && CFStringGetFileSystemRepresentation(pathStr, path, len))
    {
        TRACE("font file %s\n", path);
        AddFontToList(NULL, path, NULL, 0, ADDFONT_EXTERNAL_FONT);
    }
    RtlFreeHeap(GetProcessHeap(), 0, path);
}

static void load_mac_fonts(void)
{
    CFStringRef removeDupesKey;
    CFBooleanRef removeDupesValue;
    CFDictionaryRef options;
    CTFontCollectionRef col;
    CFArrayRef descs;
    CFMutableSetRef paths;
    CFIndex i;

    removeDupesKey = kCTFontCollectionRemoveDuplicatesOption;
    removeDupesValue = kCFBooleanTrue;
    options = CFDictionaryCreate(NULL, (const void**)&removeDupesKey, (const void**)&removeDupesValue, 1,
                                 &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    col = CTFontCollectionCreateFromAvailableFonts(options);
    if (options) CFRelease(options);
    if (!col)
    {
        WARN("CTFontCollectionCreateFromAvailableFonts failed\n");
        return;
    }

    descs = CTFontCollectionCreateMatchingFontDescriptors(col);
    CFRelease(col);
    if (!descs)
    {
        WARN("CTFontCollectionCreateMatchingFontDescriptors failed\n");
        return;
    }

    paths = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    if (!paths)
    {
        WARN("CFSetCreateMutable failed\n");
        CFRelease(descs);
        return;
    }

    for (i = 0; i < CFArrayGetCount(descs); i++)
    {
        CTFontDescriptorRef desc;
        CFURLRef url;
        CFStringRef ext;
        CFStringRef path;

        desc = CFArrayGetValueAtIndex(descs, i);

#if defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
        url = CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute);
#else
        /* CTFontDescriptor doesn't support kCTFontURLAttribute prior to 10.6, so
           we have to go CFFontDescriptor -> CTFont -> ATSFont -> FSRef -> CFURL. */
        {
            CTFontRef font;
            ATSFontRef atsFont;
            OSStatus status;
            FSRef fsref;

            font = CTFontCreateWithFontDescriptor(desc, 0, NULL);
            if (!font) continue;

            atsFont = CTFontGetPlatformFont(font, NULL);
            if (!atsFont)
            {
                CFRelease(font);
                continue;
            }

            status = ATSFontGetFileReference(atsFont, &fsref);
            CFRelease(font);
            if (status != noErr) continue;

            url = CFURLCreateFromFSRef(NULL, &fsref);
        }
#endif
        if (!url) continue;

        ext = CFURLCopyPathExtension(url);
        if (ext)
        {
            BOOL skip = (CFStringCompare(ext, CFSTR("pfa"), kCFCompareCaseInsensitive) == kCFCompareEqualTo ||
                         CFStringCompare(ext, CFSTR("pfb"), kCFCompareCaseInsensitive) == kCFCompareEqualTo);
            CFRelease(ext);
            if (skip)
            {
                CFRelease(url);
                continue;
            }
        }

        path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
        CFRelease(url);
        if (!path) continue;

        CFSetAddValue(paths, path);
        CFRelease(path);
    }

    CFRelease(descs);

    CFSetApplyFunction(paths, load_mac_font_callback, NULL);
    CFRelease(paths);
}

#endif

/*************************************************************
 * freetype_load_fonts
 */
static void CDECL freetype_load_fonts(void)
{
#ifdef SONAME_LIBFONTCONFIG
    load_fontconfig_fonts();
#elif defined(HAVE_CARBON_CARBON_H)
    load_mac_fonts();
#elif defined(__ANDROID__)
    ReadFontDir("/system/fonts", TRUE);
#endif
}

static struct font_mapping *map_font_file( const char *name )
{
    struct font_mapping *mapping;
    struct stat st;
    int fd;

    if ((fd = open( name, O_RDONLY )) == -1) return NULL;
    if (fstat( fd, &st ) == -1) goto error;

    LIST_FOR_EACH_ENTRY( mapping, &mappings_list, struct font_mapping, entry )
    {
        if (mapping->dev == st.st_dev && mapping->ino == st.st_ino)
        {
            mapping->refcount++;
            close( fd );
            return mapping;
        }
    }
    if (!(mapping = RtlAllocateHeap( GetProcessHeap(), 0, sizeof(*mapping) )))
        goto error;

    mapping->data = mmap( NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );

    if (mapping->data == MAP_FAILED)
    {
        RtlFreeHeap( GetProcessHeap(), 0, mapping );
        return NULL;
    }
    mapping->refcount = 1;
    mapping->dev = st.st_dev;
    mapping->ino = st.st_ino;
    mapping->size = st.st_size;
    list_add_tail( &mappings_list, &mapping->entry );
    return mapping;

error:
    close( fd );
    return NULL;
}

static void unmap_font_file( struct font_mapping *mapping )
{
    if (!--mapping->refcount)
    {
        list_remove( &mapping->entry );
        munmap( mapping->data, mapping->size );
        RtlFreeHeap( GetProcessHeap(), 0, mapping );
    }
}

/*************************************************************
 * freetype_destroy_font
 */
static void CDECL freetype_unmap_font( struct gdi_font *font )
{
    struct font_private_data *data = font->private;

    if (data->mapping) unmap_font_file( data->mapping );
    RtlFreeHeap( GetProcessHeap(), 0, data );
}

/*************************************************************
 * fontconfig_enum_family_fallbacks
 */
static BOOL CDECL fontconfig_enum_family_fallbacks( DWORD pitch_and_family, int index,
                                                    WCHAR buffer[LF_FACESIZE] )
{
#ifdef SONAME_LIBFONTCONFIG
    FcPattern *pat;
    char *str;
    DWORD len;

    if ((pitch_and_family & FIXED_PITCH) || (pitch_and_family & 0xf0) == FF_MODERN) pat = create_family_pattern( "monospace", &pattern_fixed );
    else if ((pitch_and_family & 0xf0) == FF_ROMAN) pat = create_family_pattern( "serif", &pattern_serif );
    else pat = create_family_pattern( "sans", &pattern_sans );

    if (!pat) return FALSE;
    if (pFcPatternGetString( pat, FC_FAMILY, index, (FcChar8 **)&str ) != FcResultMatch) return FALSE;
    RtlUTF8ToUnicodeN( buffer, (LF_FACESIZE - 1) * sizeof(WCHAR), &len, str, strlen(str) );
    buffer[len / sizeof(WCHAR)] = 0;
    return TRUE;
#endif
    return FALSE;
}


/*************************************************************
 * freetype_map_font
 */
static BOOL CDECL freetype_map_font( struct gdi_font *font, void **data_ptr, SIZE_T *data_size )
{
    struct font_private_data *data;

    if (!(data = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*data) ))) return FALSE;
    font->private = data;

    if (font->file[0])
    {
        char *filename = get_unix_file_name( font->file );
        data->mapping = map_font_file( filename );
        RtlFreeHeap( GetProcessHeap(), 0, filename );
        if (!data->mapping)
        {
            WARN("failed to map %s\n", debugstr_w(font->file));
            return FALSE;
        }
        *data_ptr = data->mapping->data;
        *data_size = data->mapping->size;
    }
    else
    {
        *data_ptr = font->data_ptr;
        *data_size = font->data_size;
    }

    return TRUE;
}


/*************************************************************************
 * Kerning support for TrueType fonts
 */

struct TT_kern_table
{
    USHORT version;
    USHORT nTables;
};

struct TT_kern_subtable
{
    USHORT version;
    USHORT length;
    union
    {
        USHORT word;
        struct
        {
            USHORT horizontal : 1;
            USHORT minimum : 1;
            USHORT cross_stream: 1;
            USHORT override : 1;
            USHORT reserved1 : 4;
            USHORT format : 8;
        } bits;
    } coverage;
};

struct TT_format0_kern_subtable
{
    USHORT nPairs;
    USHORT searchRange;
    USHORT entrySelector;
    USHORT rangeShift;
};

struct TT_kern_pair
{
    USHORT left;
    USHORT right;
    short  value;
};

static const struct font_backend_funcs font_funcs =
{
    freetype_load_fonts,
    fontconfig_enum_family_fallbacks,
    freetype_add_font,
    freetype_add_mem_font,
    freetype_map_font,
    freetype_unmap_font,
};

static NTSTATUS init_freetype_lib( HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out )
{
    callback_funcs = ptr_in;
#ifdef SONAME_LIBFONTCONFIG
    init_fontconfig();
#endif
    *(const struct font_backend_funcs **)ptr_out = &font_funcs;
    return STATUS_SUCCESS;
}

NTSTATUS CDECL __wine_init_unix_lib( HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out )
{
    if (reason != DLL_PROCESS_ATTACH) return STATUS_SUCCESS;

    if (ptr_in) return init_freetype_lib( module, reason, ptr_in, ptr_out );
    else return init_opengl_lib( module, reason, ptr_in, ptr_out );
}
