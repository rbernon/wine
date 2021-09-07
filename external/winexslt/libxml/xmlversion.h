/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

#ifndef __XML_VERSION_H__
#define __XML_VERSION_H__

#include <libxml/xmlexports.h>

#ifdef __cplusplus
extern "C" {
#endif

XMLPUBFUN void XMLCALL xmlCheckVersion(int version);

#define LIBXML_DOTTED_VERSION "2.9.12"
#define LIBXML_VERSION 20912
#define LIBXML_VERSION_STRING "20912"
#define LIBXML_VERSION_EXTRA "wine"
#define LIBXML_TEST_VERSION xmlCheckVersion(LIBXML_VERSION);

#define LIBXML_THREAD_ENABLED
#define LIBXML_THREAD_ALLOC_ENABLED
#define LIBXML_TREE_ENABLED
#define LIBXML_OUTPUT_ENABLED
#define LIBXML_PUSH_ENABLED
#define LIBXML_READER_ENABLED
#define LIBXML_PATTERN_ENABLED
#define LIBXML_WRITER_ENABLED
#define LIBXML_SAX1_ENABLED
/* #define LIBXML_FTP_ENABLED */
/* #define LIBXML_HTTP_ENABLED */
#define LIBXML_VALID_ENABLED
#define LIBXML_HTML_ENABLED
#define LIBXML_LEGACY_ENABLED
#define LIBXML_C14N_ENABLED
#define LIBXML_CATALOG_ENABLED
#define LIBXML_DOCB_ENABLED
#define LIBXML_XPATH_ENABLED
#define LIBXML_XPTR_ENABLED
#define LIBXML_XINCLUDE_ENABLED
/* #define LIBXML_ICONV_ENABLED */
/* #define LIBXML_ICU_ENABLED */
#define LIBXML_ISO8859X_ENABLED
/* #define LIBXML_DEBUG_ENABLED */
#define LIBXML_UNICODE_ENABLED
#define LIBXML_REGEXP_ENABLED
#define LIBXML_AUTOMATA_ENABLED
#define LIBXML_SCHEMAS_ENABLED
#define LIBXML_SCHEMATRON_ENABLED
/* #define LIBXML_ZLIB_ENABLED */
/* #define LIBXML_LZMA_ENABLED */

#define ATTRIBUTE_UNUSED
#define LIBXML_ATTR_ALLOC_SIZE(x)
#define LIBXML_ATTR_FORMAT(fmt,args)

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
