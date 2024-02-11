pub type Affinity = u64;
pub type FilePos = u64;
pub type IoctlCode = u32;
pub type DataSize = u32;
pub type Atom = u32;
pub type LParam = u64;
pub type MemSize = u64;
pub type ApcParam = u64;
#[derive(Clone, Copy, Debug, Default)]
pub struct ApcResult {}
pub type AbsoluteTime = i64;
#[derive(Clone, Copy, Debug, Default)]
pub struct AsyncData {}
#[derive(Clone, Copy, Debug, Default)]
pub struct GenericMap {}
#[derive(Clone, Copy, Debug, Default)]
pub struct HardwareInput {}
#[derive(Clone, Copy, Debug, Default)]
pub struct IrpParam {}
#[derive(Clone, Copy, Debug, Default)]
pub struct LUID {}
pub type Timeout = i64;
#[derive(Clone, Copy, Debug, Default)]
pub struct Rect {}
pub type ThreadId = u32;
pub type ProcessId = u32;
pub type ClientPtr = u64;
pub type ModHandle = u64;
pub type ObjHandle = u32;
pub type UserHandle = u32;
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RequestHeader {
    pub req: RequestType,
    pub request_size: DataSize,
    pub reply_size: DataSize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReplyHeader {
    pub error: u32,
    pub reply_size: DataSize,
}
/*#include <stdarg.h>*/
/*#include <stdlib.h>*/
/*#include <time.h>*/
/*#include <windef.h>*/
/*#include <winbase.h>*/
/*typedef unsigned int obj_handle_t;*/
/*typedef unsigned int user_handle_t;*/
/*typedef unsigned int atom_t;*/
/*typedef unsigned int process_id_t;*/
/*typedef unsigned int thread_id_t;*/
/*typedef unsigned int data_size_t;*/
/*typedef unsigned int ioctl_code_t;*/
/*typedef unsigned __int64 lparam_t;*/
/*typedef unsigned __int64 apc_param_t;*/
/*typedef unsigned __int64 mem_size_t;*/
/*typedef unsigned __int64 file_pos_t;*/
/*typedef unsigned __int64 client_ptr_t;*/
/*typedef unsigned __int64 affinity_t;*/
/*typedef client_ptr_t mod_handle_t;*/
/*struct request_header*/
/*{*/
/*    int          req;*/
/*    data_size_t  request_size;*/
/*    data_size_t  reply_size;*/
/*};*/
/*struct reply_header*/
/*{*/
/*    unsigned int error;*/
/*    data_size_t  reply_size;*/
/*};*/
/*struct request_max_size*/
/*{*/
/*    int pad[16];*/
/*};*/
/*#define FIRST_USER_HANDLE 0x0020*/
/*#define LAST_USER_HANDLE  0xffef*/
/*typedef union*/
/*{*/
/*    int code;*/
/*    struct*/
/*    {*/
/*        int              code;*/
/*        int              first;*/
/*        unsigned int     exc_code;*/
/*        unsigned int     flags;*/
/*        client_ptr_t     record;*/
/*        client_ptr_t     address;*/
/*        int              nb_params;*/
/*        int              __pad;*/
/*        client_ptr_t     params[15];*/
/*    } exception;*/
/*    struct*/
/*    {*/
/*        int          code;*/
/*        obj_handle_t handle;*/
/*        client_ptr_t start;*/
/*    } create_thread;*/
/*    struct*/
/*    {*/
/*        int          code;*/
/*        obj_handle_t file;*/
/*        obj_handle_t process;*/
/*        obj_handle_t thread;*/
/*        mod_handle_t base;*/
/*        int          dbg_offset;*/
/*        int          dbg_size;*/
/*        client_ptr_t start;*/
/*    } create_process;*/
/*    struct*/
/*    {*/
/*        int          code;*/
/*        int          exit_code;*/
/*    } exit;*/
/*    struct*/
/*    {*/
/*        int          code;*/
/*        obj_handle_t handle;*/
/*        mod_handle_t base;*/
/*        int          dbg_offset;*/
/*        int          dbg_size;*/
/*        client_ptr_t name;*/
/*    } load_dll;*/
/*    struct*/
/*    {*/
/*        int          code;*/
/*        int          __pad;*/
/*        mod_handle_t base;*/
/*    } unload_dll;*/
/*} debug_event_t;*/
/*typedef struct*/
/*{*/
/*    unsigned int     machine;*/
/*    unsigned int     flags;*/
/*    union*/
/*    {*/
/*        struct { unsigned int eip, ebp, esp, eflags, cs, ss; } i386_regs;*/
/*        struct { unsigned __int64 rip, rbp, rsp;*/
/*                 unsigned int cs, ss, flags, __pad; } x86_64_regs;*/
/*        struct { unsigned int sp, lr, pc, cpsr; } arm_regs;*/
/*        struct { unsigned __int64 sp, pc, pstate; } arm64_regs;*/
/*    } ctl;*/
/*    union*/
/*    {*/
/*        struct { unsigned int eax, ebx, ecx, edx, esi, edi; } i386_regs;*/
/*        struct { unsigned __int64 rax,rbx, rcx, rdx, rsi, rdi,*/
/*                                  r8, r9, r10, r11, r12, r13, r14, r15; } x86_64_regs;*/
/*        struct { unsigned int r[13]; } arm_regs;*/
/*        struct { unsigned __int64 x[31]; } arm64_regs;*/
/*    } integer;*/
/*    union*/
/*    {*/
/*        struct { unsigned int ds, es, fs, gs; } i386_regs;*/
/*        struct { unsigned int ds, es, fs, gs; } x86_64_regs;*/
/*    } seg;*/
/*    union*/
/*    {*/
/*        struct { unsigned int ctrl, status, tag, err_off, err_sel, data_off, data_sel, cr0npx;*/
/*                 unsigned char regs[80]; } i386_regs;*/
/*        struct { struct { unsigned __int64 low, high; } fpregs[32]; } x86_64_regs;*/
/*        struct { unsigned __int64 d[32]; unsigned int fpscr; } arm_regs;*/
/*        struct { struct { unsigned __int64 low, high; } q[32]; unsigned int fpcr, fpsr; } arm64_regs;*/
/*    } fp;*/
/*    union*/
/*    {*/
/*        struct { unsigned int dr0, dr1, dr2, dr3, dr6, dr7; } i386_regs;*/
/*        struct { unsigned __int64 dr0, dr1, dr2, dr3, dr6, dr7; } x86_64_regs;*/
/*        struct { unsigned int bvr[8], bcr[8], wvr[1], wcr[1]; } arm_regs;*/
/*        struct { unsigned __int64 bvr[8], wvr[2]; unsigned int bcr[8], wcr[2]; } arm64_regs;*/
/*    } debug;*/
/*    union*/
/*    {*/
/*        unsigned char i386_regs[512];*/
/*    } ext;*/
/*    union*/
/*    {*/
/*        struct { struct { unsigned __int64 low, high; } ymm_high[16]; } regs;*/
/*    } ymm;*/
/*} context_t;*/
/*#define SERVER_CTX_CONTROL            0x01*/
/*#define SERVER_CTX_INTEGER            0x02*/
/*#define SERVER_CTX_SEGMENTS           0x04*/
/*#define SERVER_CTX_FLOATING_POINT     0x08*/
/*#define SERVER_CTX_DEBUG_REGISTERS    0x10*/
/*#define SERVER_CTX_EXTENDED_REGISTERS 0x20*/
/*#define SERVER_CTX_YMM_REGISTERS      0x40*/
/*struct send_fd*/
/*{*/
/*    thread_id_t tid;*/
/*    int         fd;*/
/*};*/
/*struct wake_up_reply*/
/*{*/
/*    client_ptr_t cookie;*/
/*    int          signaled;*/
/*    int          __pad;*/
/*};*/
/*typedef __int64 timeout_t;*/
/*#define TIMEOUT_INFINITE (((timeout_t)0x7fffffff) << 32 | 0xffffffff)*/
/*typedef __int64 abstime_t;*/
/*typedef struct*/
/*{*/
/*    unsigned int debug_flags;*/
/*    unsigned int console_flags;*/
/*    obj_handle_t console;*/
/*    obj_handle_t hstdin;*/
/*    obj_handle_t hstdout;*/
/*    obj_handle_t hstderr;*/
/*    unsigned int x;*/
/*    unsigned int y;*/
/*    unsigned int xsize;*/
/*    unsigned int ysize;*/
/*    unsigned int xchars;*/
/*    unsigned int ychars;*/
/*    unsigned int attribute;*/
/*    unsigned int flags;*/
/*    unsigned int show;*/
/*    process_id_t process_group_id;*/
/*    data_size_t  curdir_len;*/
/*    data_size_t  dllpath_len;*/
/*    data_size_t  imagepath_len;*/
/*    data_size_t  cmdline_len;*/
/*    data_size_t  title_len;*/
/*    data_size_t  desktop_len;*/
/*    data_size_t  shellinfo_len;*/
/*    data_size_t  runtime_len;*/
/*} startup_info_t;*/
/*typedef struct*/
/*{*/
/*    atom_t         atom;*/
/*    int            string;*/
/*    lparam_t       data;*/
/*} property_data_t;*/
/*typedef struct*/
/*{*/
/*    int  left;*/
/*    int  top;*/
/*    int  right;*/
/*    int  bottom;*/
/*} rectangle_t;*/
/*typedef struct*/
/*{*/
/*    obj_handle_t    handle;*/
/*    obj_handle_t    event;*/
/*    client_ptr_t    iosb;*/
/*    client_ptr_t    user;*/
/*    client_ptr_t    apc;*/
/*    apc_param_t     apc_context;*/
/*} async_data_t;*/
/*struct hw_msg_source*/
/*{*/
/*    unsigned int    device;*/
/*    unsigned int    origin;*/
/*};*/
/*struct rawinput*/
/*{*/
/*    int                  type;*/
/*    unsigned int         device;*/
/*    unsigned int         wparam;*/
/*    unsigned int         usage;*/
/*};*/
/*struct hardware_msg_data*/
/*{*/
/*    lparam_t             info;*/
/*    data_size_t          size;*/
/*    int                  __pad;*/
/*    unsigned int         hw_id;*/
/*    unsigned int         flags;*/
/*    struct hw_msg_source source;*/
/*    struct rawinput      rawinput;*/
/*};*/
/*struct callback_msg_data*/
/*{*/
/*    client_ptr_t    callback;*/
/*    lparam_t        data;*/
/*    lparam_t        result;*/
/*};*/
/*struct winevent_msg_data*/
/*{*/
/*    user_handle_t   hook;*/
/*    thread_id_t     tid;*/
/*    client_ptr_t    hook_proc;*/
/*};*/
/*typedef union*/
/*{*/
/*    int type;*/
/*    struct*/
/*    {*/
/*        int            type;*/
/*        unsigned short vkey;*/
/*        unsigned short scan;*/
/*        unsigned int   flags;*/
/*        unsigned int   time;*/
/*        lparam_t       info;*/
/*    } kbd;*/
/*    struct*/
/*    {*/
/*        int            type;*/
/*        int            x;*/
/*        int            y;*/
/*        unsigned int   data;*/
/*        unsigned int   flags;*/
/*        unsigned int   time;*/
/*        lparam_t       info;*/
/*    } mouse;*/
/*    struct*/
/*    {*/
/*        int            type;*/
/*        unsigned int   msg;*/
/*        lparam_t       wparam;*/
/*        lparam_t       lparam;*/
/*        struct*/
/*        {*/
/*            unsigned int device;*/
/*            unsigned int usage;*/
/*            unsigned int count;*/
/*            unsigned int length;*/
/*        } hid;*/
/*    } hw;*/
/*} hw_input_t;*/
/*typedef union*/
/*{*/
/*    unsigned char            bytes[1];*/
/*    struct hardware_msg_data hardware;*/
/*    struct callback_msg_data callback;*/
/*    struct winevent_msg_data winevent;*/
/*} message_data_t;*/
/*struct filesystem_event*/
/*{*/
/*    int         action;*/
/*    data_size_t len;*/
/*    char        name[1];*/
/*};*/
/*struct luid*/
/*{*/
/*    unsigned int low_part;*/
/*    int          high_part;*/
/*};*/
/*struct luid_attr*/
/*{*/
/*    struct luid  luid;*/
/*    unsigned int attrs;*/
/*};*/
/*struct acl*/
/*{*/
/*    unsigned char  revision;*/
/*    unsigned char  pad1;*/
/*    unsigned short size;*/
/*    unsigned short count;*/
/*    unsigned short pad2;*/
/*};*/
/*struct sid*/
/*{*/
/*    unsigned char revision;*/
/*    unsigned char sub_count;*/
/*    unsigned char id_auth[6];*/
/*    unsigned int  sub_auth[15];*/
/*};*/
/*typedef struct*/
/*{*/
/*    unsigned int read;*/
/*    unsigned int write;*/
/*    unsigned int exec;*/
/*    unsigned int all;*/
/*} generic_map_t;*/
/*#define MAX_ACL_LEN 65535*/
/*struct security_descriptor*/
/*{*/
/*    unsigned int control;*/
/*    data_size_t  owner_len;*/
/*    data_size_t  group_len;*/
/*    data_size_t  sacl_len;*/
/*    data_size_t  dacl_len;*/
/*};*/
/*struct object_attributes*/
/*{*/
/*    obj_handle_t rootdir;*/
/*    unsigned int attributes;*/
/*    data_size_t  sd_len;*/
/*    data_size_t  name_len;*/
/*};*/
/*struct object_type_info*/
/*{*/
/*    data_size_t   name_len;*/
/*    unsigned int  index;*/
/*    unsigned int  obj_count;*/
/*    unsigned int  handle_count;*/
/*    unsigned int  obj_max;*/
/*    unsigned int  handle_max;*/
/*    unsigned int  valid_access;*/
/*    generic_map_t mapping;*/
/*};*/
/*enum select_op*/
/*{*/
/*    SELECT_NONE,*/
/*    SELECT_WAIT,*/
/*    SELECT_WAIT_ALL,*/
/*    SELECT_SIGNAL_AND_WAIT,*/
/*    SELECT_KEYED_EVENT_WAIT,*/
/*    SELECT_KEYED_EVENT_RELEASE*/
/*};*/
/*typedef union*/
/*{*/
/*    enum select_op op;*/
/*    struct*/
/*    {*/
/*        enum select_op  op;*/
/*        obj_handle_t    handles[MAXIMUM_WAIT_OBJECTS];*/
/*        int             __pad;*/
/*    } wait;*/
/*    struct*/
/*    {*/
/*        enum select_op  op;*/
/*        obj_handle_t    wait;*/
/*        obj_handle_t    signal;*/
/*    } signal_and_wait;*/
/*    struct*/
/*    {*/
/*        enum select_op  op;*/
/*        obj_handle_t    handle;*/
/*        client_ptr_t    key;*/
/*    } keyed_event;*/
/*} select_op_t;*/
/*enum apc_type*/
/*{*/
/*    APC_NONE,*/
/*    APC_USER,*/
/*    APC_ASYNC_IO,*/
/*    APC_VIRTUAL_ALLOC,*/
/*    APC_VIRTUAL_ALLOC_EX,*/
/*    APC_VIRTUAL_FREE,*/
/*    APC_VIRTUAL_QUERY,*/
/*    APC_VIRTUAL_PROTECT,*/
/*    APC_VIRTUAL_FLUSH,*/
/*    APC_VIRTUAL_LOCK,*/
/*    APC_VIRTUAL_UNLOCK,*/
/*    APC_MAP_VIEW,*/
/*    APC_MAP_VIEW_EX,*/
/*    APC_UNMAP_VIEW,*/
/*    APC_CREATE_THREAD,*/
/*    APC_DUP_HANDLE*/
/*};*/
/*typedef struct*/
/*{*/
/*    enum apc_type    type;*/
/*    int              __pad;*/
/*    client_ptr_t     func;*/
/*    apc_param_t      args[3];*/
/*} user_apc_t;*/
/*typedef union*/
/*{*/
/*    enum apc_type type;*/
/*    user_apc_t    user;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     user;*/
/*        client_ptr_t     sb;*/
/*        data_size_t      result;*/
/*    } async_io;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     op_type;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*        mem_size_t       zero_bits;*/
/*        unsigned int     prot;*/
/*    } virtual_alloc;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     op_type;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*        mem_size_t       limit_low;*/
/*        mem_size_t       limit_high;*/
/*        mem_size_t       align;*/
/*        unsigned int     prot;*/
/*        unsigned int     attributes;*/
/*    } virtual_alloc_ex;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     op_type;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_free;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        int              __pad;*/
/*        client_ptr_t     addr;*/
/*    } virtual_query;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     prot;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_protect;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        int              __pad;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_flush;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        int              __pad;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_lock;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        int              __pad;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_unlock;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        obj_handle_t     handle;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*        file_pos_t       offset;*/
/*        mem_size_t       zero_bits;*/
/*        unsigned int     alloc_type;*/
/*        unsigned int     prot;*/
/*    } map_view;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        obj_handle_t     handle;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*        file_pos_t       offset;*/
/*        mem_size_t       limit_low;*/
/*        mem_size_t       limit_high;*/
/*        unsigned int     alloc_type;*/
/*        unsigned int     prot;*/
/*        unsigned short   machine;*/
/*        unsigned short   __pad[3];*/
/*    } map_view_ex;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     flags;*/
/*        client_ptr_t     addr;*/
/*    } unmap_view;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     flags;*/
/*        client_ptr_t     func;*/
/*        client_ptr_t     arg;*/
/*        mem_size_t       zero_bits;*/
/*        mem_size_t       reserve;*/
/*        mem_size_t       commit;*/
/*    } create_thread;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        obj_handle_t     src_handle;*/
/*        obj_handle_t     dst_process;*/
/*        unsigned int     access;*/
/*        unsigned int     attributes;*/
/*        unsigned int     options;*/
/*    } dup_handle;*/
/*} apc_call_t;*/
/*typedef union*/
/*{*/
/*    enum apc_type type;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        unsigned int     total;*/
/*    } async_io;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_alloc;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_alloc_ex;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_free;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     base;*/
/*        client_ptr_t     alloc_base;*/
/*        mem_size_t       size;*/
/*        unsigned short   state;*/
/*        unsigned short   prot;*/
/*        unsigned short   alloc_prot;*/
/*        unsigned short   alloc_type;*/
/*    } virtual_query;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*        unsigned int     prot;*/
/*    } virtual_protect;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_flush;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_lock;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } virtual_unlock;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } map_view;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        client_ptr_t     addr;*/
/*        mem_size_t       size;*/
/*    } map_view_ex;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*    } unmap_view;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        process_id_t     pid;*/
/*        thread_id_t      tid;*/
/*        client_ptr_t     teb;*/
/*        obj_handle_t     handle;*/
/*    } create_thread;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*        obj_handle_t     handle;*/
/*    } dup_handle;*/
/*    struct*/
/*    {*/
/*        enum apc_type    type;*/
/*        unsigned int     status;*/
/*    } break_process;*/
/*} apc_result_t;*/
/*enum irp_type*/
/*{*/
/*    IRP_CALL_NONE,*/
/*    IRP_CALL_CREATE,*/
/*    IRP_CALL_CLOSE,*/
/*    IRP_CALL_READ,*/
/*    IRP_CALL_WRITE,*/
/*    IRP_CALL_FLUSH,*/
/*    IRP_CALL_IOCTL,*/
/*    IRP_CALL_VOLUME,*/
/*    IRP_CALL_FREE,*/
/*    IRP_CALL_CANCEL*/
/*};*/
/*typedef union*/
/*{*/
/*    enum irp_type        type;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        unsigned int     access;*/
/*        unsigned int     sharing;*/
/*        unsigned int     options;*/
/*        client_ptr_t     device;*/
/*        obj_handle_t     file;*/
/*    } create;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        int              __pad;*/
/*        client_ptr_t     file;*/
/*    } close;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        unsigned int     key;*/
/*        data_size_t      out_size;*/
/*        int              __pad;*/
/*        client_ptr_t     file;*/
/*        file_pos_t       pos;*/
/*    } read;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        unsigned int     key;*/
/*        client_ptr_t     file;*/
/*        file_pos_t       pos;*/
/*    } write;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        int              __pad;*/
/*        client_ptr_t     file;*/
/*    } flush;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        ioctl_code_t     code;*/
/*        data_size_t      out_size;*/
/*        int              __pad;*/
/*        client_ptr_t     file;*/
/*    } ioctl;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        unsigned int     info_class;*/
/*        data_size_t      out_size;*/
/*        int              __pad;*/
/*        client_ptr_t     file;*/
/*    } volume;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        int              __pad;*/
/*        client_ptr_t     obj;*/
/*    } free;*/
/*    struct*/
/*    {*/
/*        enum irp_type    type;*/
/*        int              __pad;*/
/*        client_ptr_t     irp;*/
/*    } cancel;*/
/*} irp_params_t;*/
/*typedef struct*/
/*{*/
/*    client_ptr_t   base;*/
/*    client_ptr_t   map_addr;*/
/*    mem_size_t     stack_size;*/
/*    mem_size_t     stack_commit;*/
/*    unsigned int   entry_point;*/
/*    unsigned int   map_size;*/
/*    unsigned int   zerobits;*/
/*    unsigned int   subsystem;*/
/*    unsigned short subsystem_minor;*/
/*    unsigned short subsystem_major;*/
/*    unsigned short osversion_major;*/
/*    unsigned short osversion_minor;*/
/*    unsigned short image_charact;*/
/*    unsigned short dll_charact;*/
/*    unsigned short machine;*/
/*    unsigned char  contains_code : 1;*/
/*    unsigned char  wine_builtin : 1;*/
/*    unsigned char  wine_fakedll : 1;*/
/*    unsigned char  is_hybrid : 1;*/
/*    unsigned char  padding : 4;*/
/*    unsigned char  image_flags;*/
/*    unsigned int   loader_flags;*/
/*    unsigned int   header_size;*/
/*    unsigned int   file_size;*/
/*    unsigned int   checksum;*/
/*    unsigned int   dbg_offset;*/
/*    unsigned int   dbg_size;*/
/*} pe_image_info_t;*/
/*#define IMAGE_FLAGS_ComPlusNativeReady        0x01*/
/*#define IMAGE_FLAGS_ComPlusILOnly             0x02*/
/*#define IMAGE_FLAGS_ImageDynamicallyRelocated 0x04*/
/*#define IMAGE_FLAGS_ImageMappedFlat           0x08*/
/*#define IMAGE_FLAGS_BaseBelow4gb              0x10*/
/*#define IMAGE_FLAGS_ComPlusPrefer32bit        0x20*/
/*struct rawinput_device*/
/*{*/
/*    unsigned int   usage;*/
/*    unsigned int   flags;*/
/*    user_handle_t  target;*/
/*};*/
/*typedef struct*/
/*{*/
/*    int x;*/
/*    int y;*/
/*    unsigned int time;*/
/*    int __pad;*/
/*    lparam_t info;*/
/*} cursor_pos_t;*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct NewProcessRequest {
    pub header: RequestHeader,
    pub token: ObjHandle,
    pub debug: ObjHandle,
    pub parent_process: ObjHandle,
    pub flags: u32,
    pub socket_fd: i32,
    pub access: u32,
    pub machine: u16,
    __pad_38: [u8; 2],
    pub info_size: DataSize,
    pub handles_size: DataSize,
    pub jobs_size: DataSize,
    /* VARARG(objattr,object_attributes); */
    /* VARARG(handles,uints,handles_size); */
    /* VARARG(jobs,uints,jobs_size); */
    /* VARARG(info,startup_info,info_size); */
    /* VARARG(env,unicode_str); */
    __pad_52: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct NewProcessReply {
    pub header: ReplyHeader,
    pub info: ObjHandle,
    pub pid: ProcessId,
    pub handle: ObjHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetNewProcessInfoRequest {
    pub header: RequestHeader,
    pub info: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetNewProcessInfoReply {
    pub header: ReplyHeader,
    pub success: i32,
    pub exit_code: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct NewThreadRequest {
    pub header: RequestHeader,
    pub process: ObjHandle,
    pub access: u32,
    pub flags: u32,
    pub request_fd: i32,
    /* VARARG(objattr,object_attributes); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct NewThreadReply {
    pub header: ReplyHeader,
    pub tid: ThreadId,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetStartupInfoRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetStartupInfoReply {
    pub header: ReplyHeader,
    pub info_size: DataSize,
    pub machine: u16,
    /* VARARG(info,startup_info,info_size); */
    /* VARARG(env,unicode_str); */
    __pad_14: [u8; 2],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct InitProcessDoneRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub teb: ClientPtr,
    pub peb: ClientPtr,
    pub ldt_copy: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct InitProcessDoneReply {
    pub header: ReplyHeader,
    pub suspend: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct InitFirstThreadRequest {
    pub header: RequestHeader,
    pub unix_pid: i32,
    pub unix_tid: i32,
    pub debug_level: i32,
    pub reply_fd: i32,
    pub wait_fd: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct InitFirstThreadReply {
    pub header: ReplyHeader,
    pub pid: ProcessId,
    pub tid: ThreadId,
    pub server_start: Timeout,
    pub session_id: u32,
    pub info_size: DataSize,
    /* VARARG(machines,ushorts); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct InitThreadRequest {
    pub header: RequestHeader,
    pub unix_tid: i32,
    pub reply_fd: i32,
    pub wait_fd: i32,
    pub teb: ClientPtr,
    pub entry: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct InitThreadReply {
    pub header: ReplyHeader,
    pub suspend: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct TerminateProcessRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub exit_code: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct TerminateProcessReply {
    pub header: ReplyHeader,
    pub _self: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct TerminateThreadRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub exit_code: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct TerminateThreadReply {
    pub header: ReplyHeader,
    pub _self: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessInfoReply {
    pub header: ReplyHeader,
    pub pid: ProcessId,
    pub ppid: ProcessId,
    pub affinity: Affinity,
    pub peb: ClientPtr,
    pub start_time: Timeout,
    pub end_time: Timeout,
    pub session_id: u32,
    pub exit_code: i32,
    pub priority: i32,
    pub machine: u16,
    /* VARARG(image,pe_image_info); */
    __pad_62: [u8; 2],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessDebugInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessDebugInfoReply {
    pub header: ReplyHeader,
    pub debug: ObjHandle,
    pub debug_children: i32,
    /* VARARG(image,pe_image_info); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessImageNameRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub win32: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessImageNameReply {
    pub header: ReplyHeader,
    pub len: DataSize,
    /* VARARG(name,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessVmCountersRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessVmCountersReply {
    pub header: ReplyHeader,
    pub peak_virtual_size: MemSize,
    pub virtual_size: MemSize,
    pub peak_working_set_size: MemSize,
    pub working_set_size: MemSize,
    pub pagefile_usage: MemSize,
    pub peak_pagefile_usage: MemSize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetProcessInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub mask: i32,
    pub priority: i32,
    pub affinity: Affinity,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetProcessInfoReply {
    pub header: ReplyHeader,
}
/*#define SET_PROCESS_INFO_PRIORITY 0x01*/
/*#define SET_PROCESS_INFO_AFFINITY 0x02*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub access: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadInfoReply {
    pub header: ReplyHeader,
    pub pid: ProcessId,
    pub tid: ThreadId,
    pub teb: ClientPtr,
    pub entry_point: ClientPtr,
    pub affinity: Affinity,
    pub exit_code: i32,
    pub priority: i32,
    pub last: i32,
    pub suspend_count: i32,
    pub flags: u32,
    pub desc_len: DataSize,
    /* VARARG(desc,unicode_str); */
}
/*#define GET_THREAD_INFO_FLAG_DBG_HIDDEN 0x01*/
/*#define GET_THREAD_INFO_FLAG_TERMINATED 0x02*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadTimesRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadTimesReply {
    pub header: ReplyHeader,
    pub creation_time: Timeout,
    pub exit_time: Timeout,
    pub unix_pid: i32,
    pub unix_tid: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetThreadInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub mask: i32,
    pub priority: i32,
    pub affinity: Affinity,
    pub entry_point: ClientPtr,
    pub token: ObjHandle,
    /* VARARG(desc,unicode_str); */
    __pad_44: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetThreadInfoReply {
    pub header: ReplyHeader,
}
/*#define SET_THREAD_INFO_PRIORITY    0x01*/
/*#define SET_THREAD_INFO_AFFINITY    0x02*/
/*#define SET_THREAD_INFO_TOKEN       0x04*/
/*#define SET_THREAD_INFO_ENTRYPOINT  0x08*/
/*#define SET_THREAD_INFO_DESCRIPTION 0x10*/
/*#define SET_THREAD_INFO_DBG_HIDDEN  0x20*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SuspendThreadRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SuspendThreadReply {
    pub header: ReplyHeader,
    pub count: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ResumeThreadRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ResumeThreadReply {
    pub header: ReplyHeader,
    pub count: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueueApcRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    /* VARARG(call,apc_call); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueueApcReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    pub _self: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetApcResultRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetApcResultReply {
    pub header: ReplyHeader,
    pub result: ApcResult,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CloseHandleRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CloseHandleReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetHandleInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub flags: i32,
    pub mask: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetHandleInfoReply {
    pub header: ReplyHeader,
    pub old_flags: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DupHandleRequest {
    pub header: RequestHeader,
    pub src_process: ObjHandle,
    pub src_handle: ObjHandle,
    pub dst_process: ObjHandle,
    pub access: u32,
    pub attributes: u32,
    pub options: u32,
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DupHandleReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CompareObjectsRequest {
    pub header: RequestHeader,
    pub first: ObjHandle,
    pub second: ObjHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CompareObjectsReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MakeTemporaryRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MakeTemporaryReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenProcessRequest {
    pub header: RequestHeader,
    pub pid: ProcessId,
    pub access: u32,
    pub attributes: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenProcessReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenThreadRequest {
    pub header: RequestHeader,
    pub tid: ThreadId,
    pub access: u32,
    pub attributes: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenThreadReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SelectRequest {
    pub header: RequestHeader,
    pub flags: i32,
    pub cookie: ClientPtr,
    pub timeout: AbsoluteTime,
    pub size: DataSize,
    pub prev_apc: ObjHandle,
    /* VARARG(result,apc_result); */
    /* VARARG(data,select_op,size); */
    /* VARARG(contexts,contexts); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SelectReply {
    pub header: ReplyHeader,
    pub apc_handle: ObjHandle,
    pub signaled: i32,
    /* VARARG(call,apc_call); */
    /* VARARG(contexts,contexts); */
}
/*#define SELECT_ALERTABLE     1*/
/*#define SELECT_INTERRUPTIBLE 2*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateEventRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub manual_reset: i32,
    pub initial_state: i32,
    /* VARARG(objattr,object_attributes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateEventReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EventOpRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub op: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EventOpReply {
    pub header: ReplyHeader,
    pub state: i32,
    __pad_12: [u8; 4],
}
/*enum event_op { PULSE_EVENT, SET_EVENT, RESET_EVENT };*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueryEventRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueryEventReply {
    pub header: ReplyHeader,
    pub manual_reset: i32,
    pub state: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenEventRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenEventReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateKeyedEventRequest {
    pub header: RequestHeader,
    pub access: u32,
    /* VARARG(objattr,object_attributes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateKeyedEventReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenKeyedEventRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenKeyedEventReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateMutexRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub owned: i32,
    /* VARARG(objattr,object_attributes); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateMutexReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReleaseMutexRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReleaseMutexReply {
    pub header: ReplyHeader,
    pub prev_count: u32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenMutexRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenMutexReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueryMutexRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueryMutexReply {
    pub header: ReplyHeader,
    pub count: u32,
    pub owned: i32,
    pub abandoned: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateSemaphoreRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub initial: u32,
    pub max: u32,
    /* VARARG(objattr,object_attributes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateSemaphoreReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReleaseSemaphoreRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub count: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReleaseSemaphoreReply {
    pub header: ReplyHeader,
    pub prev_count: u32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QuerySemaphoreRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QuerySemaphoreReply {
    pub header: ReplyHeader,
    pub current: u32,
    pub max: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenSemaphoreRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenSemaphoreReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateFileRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub sharing: u32,
    pub create: i32,
    pub options: u32,
    pub attrs: u32,
    /* VARARG(objattr,object_attributes); */
    /* VARARG(filename,string); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateFileReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenFileObjectRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    pub sharing: u32,
    pub options: u32,
    /* VARARG(filename,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenFileObjectReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AllocFileHandleRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub fd: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AllocFileHandleReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetHandleUnixNameRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetHandleUnixNameReply {
    pub header: ReplyHeader,
    pub name_len: DataSize,
    /* VARARG(name,string); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetHandleFdRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetHandleFdReply {
    pub header: ReplyHeader,
    pub _type: i32,
    pub cacheable: i32,
    pub access: u32,
    pub options: u32,
}
/*enum server_fd_type*/
/*{*/
/*    FD_TYPE_INVALID,*/
/*    FD_TYPE_FILE,*/
/*    FD_TYPE_DIR,*/
/*    FD_TYPE_SOCKET,*/
/*    FD_TYPE_SERIAL,*/
/*    FD_TYPE_PIPE,*/
/*    FD_TYPE_MAILSLOT,*/
/*    FD_TYPE_CHAR,*/
/*    FD_TYPE_DEVICE,*/
/*    FD_TYPE_NB_TYPES*/
/*};*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetDirectoryCacheEntryRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetDirectoryCacheEntryReply {
    pub header: ReplyHeader,
    pub entry: i32,
    /* VARARG(free,ints); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FlushRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub _async: AsyncData,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FlushReply {
    pub header: ReplyHeader,
    pub event: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetFileInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub info_class: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetFileInfoReply {
    pub header: ReplyHeader,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetVolumeInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub _async: AsyncData,
    pub info_class: u32,
    __pad_60: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetVolumeInfoReply {
    pub header: ReplyHeader,
    pub wait: ObjHandle,
    /* VARARG(data,bytes); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct LockFileRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub offset: FilePos,
    pub count: FilePos,
    pub shared: i32,
    pub wait: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct LockFileReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    pub overlapped: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnlockFileRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub offset: FilePos,
    pub count: FilePos,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnlockFileReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RecvSocketRequest {
    pub header: RequestHeader,
    pub oob: i32,
    pub _async: AsyncData,
    pub force_async: i32,
    __pad_60: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RecvSocketReply {
    pub header: ReplyHeader,
    pub wait: ObjHandle,
    pub options: u32,
    pub nonblocking: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SendSocketRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub _async: AsyncData,
    pub force_async: i32,
    __pad_60: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SendSocketReply {
    pub header: ReplyHeader,
    pub wait: ObjHandle,
    pub options: u32,
    pub nonblocking: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SocketGetEventsRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub event: ObjHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SocketGetEventsReply {
    pub header: ReplyHeader,
    pub flags: u32,
    /* VARARG(status,uints); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SocketSendIcmpIdRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub icmp_id: u16,
    pub icmp_seq: u16,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SocketSendIcmpIdReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SocketGetIcmpIdRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub icmp_seq: u16,
    __pad_18: [u8; 6],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SocketGetIcmpIdReply {
    pub header: ReplyHeader,
    pub icmp_id: u16,
    __pad_10: [u8; 6],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetNextConsoleRequestRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub signal: i32,
    pub read: i32,
    pub status: u32,
    /* VARARG(out_data,bytes); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetNextConsoleRequestReply {
    pub header: ReplyHeader,
    pub code: u32,
    pub output: u32,
    pub out_size: DataSize,
    /* VARARG(in_data,bytes); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReadDirectoryChangesRequest {
    pub header: RequestHeader,
    pub filter: u32,
    pub subtree: i32,
    pub want_data: i32,
    pub _async: AsyncData,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReadDirectoryChangesReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReadChangeRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReadChangeReply {
    pub header: ReplyHeader,
    /* VARARG(events,filesystem_event); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateMappingRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub flags: u32,
    pub file_access: u32,
    pub size: MemSize,
    pub file_handle: ObjHandle,
    /* VARARG(objattr,object_attributes); */
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateMappingReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenMappingRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenMappingReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMappingInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub access: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMappingInfoReply {
    pub header: ReplyHeader,
    pub size: MemSize,
    pub flags: u32,
    pub shared_file: ObjHandle,
    pub total: DataSize,
    /* VARARG(image,pe_image_info); */
    /* VARARG(name,unicode_str); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetImageMapAddressRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetImageMapAddressReply {
    pub header: ReplyHeader,
    pub addr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MapViewRequest {
    pub header: RequestHeader,
    pub mapping: ObjHandle,
    pub access: u32,
    __pad_20: [u8; 4],
    pub base: ClientPtr,
    pub size: MemSize,
    pub start: FilePos,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MapViewReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MapImageViewRequest {
    pub header: RequestHeader,
    pub mapping: ObjHandle,
    pub base: ClientPtr,
    pub size: MemSize,
    pub entry: u32,
    pub machine: u16,
    __pad_38: [u8; 2],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MapImageViewReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MapBuiltinViewRequest {
    pub header: RequestHeader,
    /* VARARG(image,pe_image_info); */
    /* VARARG(name,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MapBuiltinViewReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetImageViewInfoRequest {
    pub header: RequestHeader,
    pub process: ObjHandle,
    pub addr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetImageViewInfoReply {
    pub header: ReplyHeader,
    pub base: ClientPtr,
    pub size: MemSize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnmapViewRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub base: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnmapViewReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMappingCommittedRangeRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub base: ClientPtr,
    pub offset: FilePos,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMappingCommittedRangeReply {
    pub header: ReplyHeader,
    pub size: MemSize,
    pub committed: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddMappingCommittedRangeRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub base: ClientPtr,
    pub offset: FilePos,
    pub size: MemSize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddMappingCommittedRangeReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct IsSameMappingRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub base1: ClientPtr,
    pub base2: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct IsSameMappingReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMappingFilenameRequest {
    pub header: RequestHeader,
    pub process: ObjHandle,
    pub addr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMappingFilenameReply {
    pub header: ReplyHeader,
    pub len: DataSize,
    /* VARARG(filename,unicode_str); */
    __pad_12: [u8; 4],
}
/*struct thread_info*/
/*{*/
/*    timeout_t       start_time;*/
/*    thread_id_t     tid;*/
/*    int             base_priority;*/
/*    int             current_priority;*/
/*    int             unix_tid;*/
/*    client_ptr_t    teb;*/
/*    client_ptr_t    entry_point;*/
/*};*/
/*struct process_info*/
/*{*/
/*    timeout_t       start_time;*/
/*    data_size_t     name_len;*/
/*    int             thread_count;*/
/*    int             priority;*/
/*    process_id_t    pid;*/
/*    process_id_t    parent_pid;*/
/*    unsigned int    session_id;*/
/*    int             handle_count;*/
/*    int             unix_pid;*/
/*};*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ListProcessesRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ListProcessesReply {
    pub header: ReplyHeader,
    pub info_size: DataSize,
    pub process_count: i32,
    pub total_thread_count: i32,
    pub total_name_len: DataSize,
    /* VARARG(data,process_info,info_size); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDebugObjRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub flags: u32,
    /* VARARG(objattr,object_attributes); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDebugObjReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WaitDebugEventRequest {
    pub header: RequestHeader,
    pub debug: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WaitDebugEventReply {
    pub header: ReplyHeader,
    pub pid: ProcessId,
    pub tid: ThreadId,
    /* VARARG(event,debug_event); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueueExceptionEventRequest {
    pub header: RequestHeader,
    pub first: i32,
    pub code: u32,
    pub flags: u32,
    pub record: ClientPtr,
    pub address: ClientPtr,
    pub len: DataSize,
    /* VARARG(params,uints64,len); */
    __pad_44: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueueExceptionEventReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetExceptionStatusRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetExceptionStatusReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ContinueDebugEventRequest {
    pub header: RequestHeader,
    pub debug: ObjHandle,
    pub pid: ProcessId,
    pub tid: ThreadId,
    pub status: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ContinueDebugEventReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DebugProcessRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub debug: ObjHandle,
    pub attach: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DebugProcessReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetDebugObjInfoRequest {
    pub header: RequestHeader,
    pub debug: ObjHandle,
    pub flags: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetDebugObjInfoReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReadProcessMemoryRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub addr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReadProcessMemoryReply {
    pub header: ReplyHeader,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WriteProcessMemoryRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub addr: ClientPtr,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WriteProcessMemoryReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateKeyRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub options: u32,
    /* VARARG(objattr,object_attributes); */
    /* VARARG(class,unicode_str); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateKeyReply {
    pub header: ReplyHeader,
    pub hkey: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenKeyRequest {
    pub header: RequestHeader,
    pub parent: ObjHandle,
    pub access: u32,
    pub attributes: u32,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenKeyReply {
    pub header: ReplyHeader,
    pub hkey: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DeleteKeyRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DeleteKeyReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FlushKeyRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FlushKeyReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumKeyRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
    pub index: i32,
    pub info_class: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumKeyReply {
    pub header: ReplyHeader,
    pub subkeys: i32,
    pub max_subkey: i32,
    pub max_class: i32,
    pub values: i32,
    pub max_value: i32,
    pub max_data: i32,
    pub modif: Timeout,
    pub total: DataSize,
    pub namelen: DataSize,
    /* VARARG(name,unicode_str,namelen); */
    /* VARARG(class,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetKeyValueRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
    pub _type: i32,
    pub namelen: DataSize,
    /* VARARG(name,unicode_str,namelen); */
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetKeyValueReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetKeyValueRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetKeyValueReply {
    pub header: ReplyHeader,
    pub _type: i32,
    pub total: DataSize,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumKeyValueRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
    pub index: i32,
    pub info_class: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumKeyValueReply {
    pub header: ReplyHeader,
    pub _type: i32,
    pub total: DataSize,
    pub namelen: DataSize,
    /* VARARG(name,unicode_str,namelen); */
    /* VARARG(data,bytes); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DeleteKeyValueRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DeleteKeyValueReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct LoadRegistryRequest {
    pub header: RequestHeader,
    pub file: ObjHandle,
    /* VARARG(objattr,object_attributes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct LoadRegistryReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnloadRegistryRequest {
    pub header: RequestHeader,
    pub parent: ObjHandle,
    pub attributes: u32,
    /* VARARG(name,unicode_str); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnloadRegistryReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SaveRegistryRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
    pub file: ObjHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SaveRegistryReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetRegistryNotificationRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
    pub event: ObjHandle,
    pub subtree: i32,
    pub filter: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetRegistryNotificationReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RenameKeyRequest {
    pub header: RequestHeader,
    pub hkey: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RenameKeyReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateTimerRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub manual: i32,
    /* VARARG(objattr,object_attributes); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateTimerReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenTimerRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenTimerReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetTimerRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub expire: Timeout,
    pub callback: ClientPtr,
    pub arg: ClientPtr,
    pub period: i32,
    __pad_44: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetTimerReply {
    pub header: ReplyHeader,
    pub signaled: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CancelTimerRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CancelTimerReply {
    pub header: ReplyHeader,
    pub signaled: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTimerInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTimerInfoReply {
    pub header: ReplyHeader,
    pub when: Timeout,
    pub signaled: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadContextRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub context: ObjHandle,
    pub flags: u32,
    pub native_flags: u32,
    pub machine: u16,
    __pad_30: [u8; 2],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadContextReply {
    pub header: ReplyHeader,
    pub _self: i32,
    pub handle: ObjHandle,
    /* VARARG(contexts,contexts); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetThreadContextRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub native_flags: u32,
    /* VARARG(contexts,contexts); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetThreadContextReply {
    pub header: ReplyHeader,
    pub _self: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSelectorEntryRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub entry: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSelectorEntryReply {
    pub header: ReplyHeader,
    pub base: u32,
    pub limit: u32,
    pub flags: u8,
    __pad_17: [u8; 7],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddAtomRequest {
    pub header: RequestHeader,
    /* VARARG(name,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddAtomReply {
    pub header: ReplyHeader,
    pub atom: Atom,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DeleteAtomRequest {
    pub header: RequestHeader,
    pub atom: Atom,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DeleteAtomReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FindAtomRequest {
    pub header: RequestHeader,
    /* VARARG(name,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FindAtomReply {
    pub header: ReplyHeader,
    pub atom: Atom,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetAtomInformationRequest {
    pub header: RequestHeader,
    pub atom: Atom,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetAtomInformationReply {
    pub header: ReplyHeader,
    pub count: i32,
    pub pinned: i32,
    pub total: DataSize,
    /* VARARG(name,unicode_str); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMsgQueueRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMsgQueueReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetQueueFdRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetQueueFdReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetQueueMaskRequest {
    pub header: RequestHeader,
    pub wake_mask: u32,
    pub changed_mask: u32,
    pub skip_wait: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetQueueMaskReply {
    pub header: ReplyHeader,
    pub wake_bits: u32,
    pub changed_bits: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetQueueStatusRequest {
    pub header: RequestHeader,
    pub clear_bits: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetQueueStatusReply {
    pub header: ReplyHeader,
    pub wake_bits: u32,
    pub changed_bits: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessIdleEventRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessIdleEventReply {
    pub header: ReplyHeader,
    pub event: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SendMessageRequest {
    pub header: RequestHeader,
    pub id: ThreadId,
    pub _type: i32,
    pub flags: i32,
    pub win: UserHandle,
    pub msg: u32,
    pub wparam: LParam,
    pub lparam: LParam,
    pub timeout: Timeout,
    /* VARARG(data,message_data); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SendMessageReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct PostQuitMessageRequest {
    pub header: RequestHeader,
    pub exit_code: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct PostQuitMessageReply {
    pub header: ReplyHeader,
}
/*enum message_type*/
/*{*/
/*    MSG_ASCII,*/
/*    MSG_UNICODE,*/
/*    MSG_NOTIFY,*/
/*    MSG_CALLBACK,*/
/*    MSG_CALLBACK_RESULT,*/
/*    MSG_OTHER_PROCESS,*/
/*    MSG_POSTED,*/
/*    MSG_HARDWARE,*/
/*    MSG_WINEVENT,*/
/*    MSG_HOOK_LL*/
/*};*/
/*#define SEND_MSG_ABORT_IF_HUNG  0x01*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SendHardwareMessageRequest {
    pub header: RequestHeader,
    pub win: UserHandle,
    pub input: HardwareInput,
    pub flags: u32,
    /* VARARG(report,bytes); */
    __pad_60: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SendHardwareMessageReply {
    pub header: ReplyHeader,
    pub wait: i32,
    pub prev_x: i32,
    pub prev_y: i32,
    pub new_x: i32,
    pub new_y: i32,
    __pad_28: [u8; 4],
}
/*#define SEND_HWMSG_INJECTED    0x01*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMessageRequest {
    pub header: RequestHeader,
    pub flags: u32,
    pub get_win: UserHandle,
    pub get_first: u32,
    pub get_last: u32,
    pub hw_id: u32,
    pub wake_mask: u32,
    pub changed_mask: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMessageReply {
    pub header: ReplyHeader,
    pub win: UserHandle,
    pub msg: u32,
    pub wparam: LParam,
    pub lparam: LParam,
    pub _type: i32,
    pub x: i32,
    pub y: i32,
    pub time: u32,
    pub active_hooks: u32,
    pub total: DataSize,
    /* VARARG(data,message_data); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReplyMessageRequest {
    pub header: RequestHeader,
    pub remove: i32,
    pub result: LParam,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReplyMessageReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AcceptHardwareMessageRequest {
    pub header: RequestHeader,
    pub hw_id: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AcceptHardwareMessageReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMessageReplyRequest {
    pub header: RequestHeader,
    pub cancel: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetMessageReplyReply {
    pub header: ReplyHeader,
    pub result: LParam,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWinTimerRequest {
    pub header: RequestHeader,
    pub win: UserHandle,
    pub msg: u32,
    pub rate: u32,
    pub id: LParam,
    pub lparam: LParam,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWinTimerReply {
    pub header: ReplyHeader,
    pub id: LParam,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct KillWinTimerRequest {
    pub header: RequestHeader,
    pub win: UserHandle,
    pub id: LParam,
    pub msg: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct KillWinTimerReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct IsWindowHungRequest {
    pub header: RequestHeader,
    pub win: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct IsWindowHungReply {
    pub header: ReplyHeader,
    pub is_hung: i32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSerialInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub flags: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSerialInfoReply {
    pub header: ReplyHeader,
    pub eventmask: u32,
    pub cookie: u32,
    pub pending_write: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetSerialInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub flags: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetSerialInfoReply {
    pub header: ReplyHeader,
}
/*#define SERIALINFO_PENDING_WRITE 0x04*/
/*#define SERIALINFO_PENDING_WAIT  0x08*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CancelSyncRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub iosb: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CancelSyncReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RegisterAsyncRequest {
    pub header: RequestHeader,
    pub _type: i32,
    pub _async: AsyncData,
    pub count: i32,
    __pad_60: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RegisterAsyncReply {
    pub header: ReplyHeader,
}
/*#define ASYNC_TYPE_READ  0x01*/
/*#define ASYNC_TYPE_WRITE 0x02*/
/*#define ASYNC_TYPE_WAIT  0x03*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CancelAsyncRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub iosb: ClientPtr,
    pub only_thread: i32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CancelAsyncReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetAsyncResultRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub user_arg: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetAsyncResultReply {
    pub header: ReplyHeader,
    /* VARARG(out_data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetAsyncDirectResultRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub information: ApcParam,
    pub status: u32,
    pub mark_pending: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetAsyncDirectResultReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReadRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub _async: AsyncData,
    pub pos: FilePos,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReadReply {
    pub header: ReplyHeader,
    pub wait: ObjHandle,
    pub options: u32,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WriteRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
    pub _async: AsyncData,
    pub pos: FilePos,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WriteReply {
    pub header: ReplyHeader,
    pub wait: ObjHandle,
    pub options: u32,
    pub size: DataSize,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct IoctlRequest {
    pub header: RequestHeader,
    pub code: IoctlCode,
    pub _async: AsyncData,
    /* VARARG(in_data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct IoctlReply {
    pub header: ReplyHeader,
    pub wait: ObjHandle,
    pub options: u32,
    /* VARARG(out_data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetIrpResultRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub status: u32,
    pub size: DataSize,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetIrpResultReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateNamedPipeRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub options: u32,
    pub sharing: u32,
    pub disposition: u32,
    pub maxinstances: u32,
    pub outsize: u32,
    pub insize: u32,
    pub timeout: Timeout,
    pub flags: u32,
    /* VARARG(objattr,object_attributes); */
    __pad_52: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateNamedPipeReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    pub created: i32,
}
/*#define NAMED_PIPE_MESSAGE_STREAM_WRITE 0x0001*/
/*#define NAMED_PIPE_MESSAGE_STREAM_READ  0x0002*/
/*#define NAMED_PIPE_NONBLOCKING_MODE     0x0004*/
/*#define NAMED_PIPE_SERVER_END           0x8000*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetNamedPipeInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub flags: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetNamedPipeInfoReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateWindowRequest {
    pub header: RequestHeader,
    pub parent: UserHandle,
    pub owner: UserHandle,
    pub atom: Atom,
    pub instance: ModHandle,
    pub dpi: i32,
    pub awareness: i32,
    pub style: u32,
    pub ex_style: u32,
    /* VARARG(class,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateWindowReply {
    pub header: ReplyHeader,
    pub handle: UserHandle,
    pub parent: UserHandle,
    pub owner: UserHandle,
    pub extra: i32,
    pub class_ptr: ClientPtr,
    pub dpi: i32,
    pub awareness: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DestroyWindowRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DestroyWindowReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetDesktopWindowRequest {
    pub header: RequestHeader,
    pub force: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetDesktopWindowReply {
    pub header: ReplyHeader,
    pub top_window: UserHandle,
    pub msg_window: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowOwnerRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    pub owner: UserHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowOwnerReply {
    pub header: ReplyHeader,
    pub full_owner: UserHandle,
    pub prev_owner: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowInfoRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowInfoReply {
    pub header: ReplyHeader,
    pub full_handle: UserHandle,
    pub last_active: UserHandle,
    pub pid: ProcessId,
    pub tid: ThreadId,
    pub atom: Atom,
    pub is_unicode: i32,
    pub dpi: i32,
    pub awareness: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowInfoRequest {
    pub header: RequestHeader,
    pub flags: u16,
    pub is_unicode: i16,
    pub handle: UserHandle,
    pub style: u32,
    pub ex_style: u32,
    pub extra_size: DataSize,
    pub instance: ModHandle,
    pub user_data: LParam,
    pub extra_value: LParam,
    pub extra_offset: i32,
    __pad_60: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowInfoReply {
    pub header: ReplyHeader,
    pub old_style: u32,
    pub old_ex_style: u32,
    pub old_instance: ModHandle,
    pub old_user_data: LParam,
    pub old_extra_value: LParam,
    pub old_id: LParam,
}
/*#define SET_WIN_STYLE     0x01*/
/*#define SET_WIN_EXSTYLE   0x02*/
/*#define SET_WIN_ID        0x04*/
/*#define SET_WIN_INSTANCE  0x08*/
/*#define SET_WIN_USERDATA  0x10*/
/*#define SET_WIN_EXTRA     0x20*/
/*#define SET_WIN_UNICODE   0x40*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetParentRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    pub parent: UserHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetParentReply {
    pub header: ReplyHeader,
    pub old_parent: UserHandle,
    pub full_parent: UserHandle,
    pub dpi: i32,
    pub awareness: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowParentsRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowParentsReply {
    pub header: ReplyHeader,
    pub count: i32,
    /* VARARG(parents,user_handles); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowChildrenRequest {
    pub header: RequestHeader,
    pub desktop: ObjHandle,
    pub parent: UserHandle,
    pub atom: Atom,
    pub tid: ThreadId,
    /* VARARG(class,unicode_str); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowChildrenReply {
    pub header: ReplyHeader,
    pub count: i32,
    /* VARARG(children,user_handles); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowChildrenFromPointRequest {
    pub header: RequestHeader,
    pub parent: UserHandle,
    pub x: i32,
    pub y: i32,
    pub dpi: i32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowChildrenFromPointReply {
    pub header: ReplyHeader,
    pub count: i32,
    /* VARARG(children,user_handles); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowTreeRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowTreeReply {
    pub header: ReplyHeader,
    pub parent: UserHandle,
    pub owner: UserHandle,
    pub next_sibling: UserHandle,
    pub prev_sibling: UserHandle,
    pub first_sibling: UserHandle,
    pub last_sibling: UserHandle,
    pub first_child: UserHandle,
    pub last_child: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowPosRequest {
    pub header: RequestHeader,
    pub swp_flags: u16,
    pub paint_flags: u16,
    pub handle: UserHandle,
    pub previous: UserHandle,
    pub window: Rect,
    pub client: Rect,
    /* VARARG(valid,rectangles); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowPosReply {
    pub header: ReplyHeader,
    pub new_style: u32,
    pub new_ex_style: u32,
    pub surface_win: UserHandle,
    pub needs_update: i32,
}
/*#define SET_WINPOS_PAINT_SURFACE 0x01*/
/*#define SET_WINPOS_PIXEL_FORMAT  0x02*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowRectanglesRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    pub relative: i32,
    pub dpi: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowRectanglesReply {
    pub header: ReplyHeader,
    pub window: Rect,
    pub client: Rect,
}
/*enum coords_relative*/
/*{*/
/*    COORDS_CLIENT,*/
/*    COORDS_WINDOW,*/
/*    COORDS_PARENT,*/
/*    COORDS_SCREEN*/
/*};*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowTextRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowTextReply {
    pub header: ReplyHeader,
    pub length: DataSize,
    /* VARARG(text,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowTextRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    /* VARARG(text,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowTextReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowsOffsetRequest {
    pub header: RequestHeader,
    pub from: UserHandle,
    pub to: UserHandle,
    pub dpi: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowsOffsetReply {
    pub header: ReplyHeader,
    pub x: i32,
    pub y: i32,
    pub mirror: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetVisibleRegionRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub flags: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetVisibleRegionReply {
    pub header: ReplyHeader,
    pub top_win: UserHandle,
    pub top_rect: Rect,
    pub win_rect: Rect,
    pub paint_flags: u32,
    pub total_size: DataSize,
    /* VARARG(region,rectangles); */
    __pad_52: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSurfaceRegionRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSurfaceRegionReply {
    pub header: ReplyHeader,
    pub visible_rect: Rect,
    pub total_size: DataSize,
    /* VARARG(region,rectangles); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowRegionRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowRegionReply {
    pub header: ReplyHeader,
    pub total_size: DataSize,
    /* VARARG(region,rectangles); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowRegionRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub redraw: i32,
    /* VARARG(region,rectangles); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowRegionReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetUpdateRegionRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub from_child: UserHandle,
    pub flags: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetUpdateRegionReply {
    pub header: ReplyHeader,
    pub child: UserHandle,
    pub flags: u32,
    pub total_size: DataSize,
    /* VARARG(region,rectangles); */
    __pad_20: [u8; 4],
}
/*#define UPDATE_NONCLIENT       0x001*/
/*#define UPDATE_ERASE           0x002*/
/*#define UPDATE_PAINT           0x004*/
/*#define UPDATE_INTERNALPAINT   0x008*/
/*#define UPDATE_ALLCHILDREN     0x010*/
/*#define UPDATE_NOCHILDREN      0x020*/
/*#define UPDATE_NOREGION        0x040*/
/*#define UPDATE_DELAYED_ERASE   0x080*/
/*#define UPDATE_CLIPCHILDREN    0x100*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UpdateWindowZorderRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub rect: Rect,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UpdateWindowZorderReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RedrawWindowRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub flags: u32,
    /* VARARG(region,rectangles); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RedrawWindowReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowPropertyRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub data: LParam,
    pub atom: Atom,
    /* VARARG(name,unicode_str); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowPropertyReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RemoveWindowPropertyRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub atom: Atom,
    /* VARARG(name,unicode_str); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RemoveWindowPropertyReply {
    pub header: ReplyHeader,
    pub data: LParam,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowPropertyRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub atom: Atom,
    /* VARARG(name,unicode_str); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowPropertyReply {
    pub header: ReplyHeader,
    pub data: LParam,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowPropertiesRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowPropertiesReply {
    pub header: ReplyHeader,
    pub total: i32,
    /* VARARG(props,properties); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateWinstationRequest {
    pub header: RequestHeader,
    pub flags: u32,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateWinstationReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenWinstationRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenWinstationReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CloseWinstationRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CloseWinstationReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessWinstationRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetProcessWinstationReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetProcessWinstationRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetProcessWinstationReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumWinstationRequest {
    pub header: RequestHeader,
    pub index: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumWinstationReply {
    pub header: ReplyHeader,
    pub next: u32,
    /* VARARG(name,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDesktopRequest {
    pub header: RequestHeader,
    pub flags: u32,
    pub access: u32,
    pub attributes: u32,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDesktopReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenDesktopRequest {
    pub header: RequestHeader,
    pub winsta: ObjHandle,
    pub flags: u32,
    pub access: u32,
    pub attributes: u32,
    /* VARARG(name,unicode_str); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenDesktopReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenInputDesktopRequest {
    pub header: RequestHeader,
    pub flags: u32,
    pub access: u32,
    pub attributes: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenInputDesktopReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CloseDesktopRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CloseDesktopReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadDesktopRequest {
    pub header: RequestHeader,
    pub tid: ThreadId,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadDesktopReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetThreadDesktopRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetThreadDesktopReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumDesktopRequest {
    pub header: RequestHeader,
    pub winstation: ObjHandle,
    pub index: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumDesktopReply {
    pub header: ReplyHeader,
    pub next: u32,
    /* VARARG(name,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetUserObjectInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub flags: u32,
    pub obj_flags: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetUserObjectInfoReply {
    pub header: ReplyHeader,
    pub is_desktop: i32,
    pub old_obj_flags: u32,
    /* VARARG(name,unicode_str); */
}
/*#define SET_USER_OBJECT_SET_FLAGS       1*/
/*#define SET_USER_OBJECT_GET_FULL_NAME   2*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RegisterHotkeyRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub id: i32,
    pub flags: u32,
    pub vkey: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RegisterHotkeyReply {
    pub header: ReplyHeader,
    pub replaced: i32,
    pub flags: u32,
    pub vkey: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnregisterHotkeyRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub id: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnregisterHotkeyReply {
    pub header: ReplyHeader,
    pub flags: u32,
    pub vkey: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AttachThreadInputRequest {
    pub header: RequestHeader,
    pub tid_from: ThreadId,
    pub tid_to: ThreadId,
    pub attach: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AttachThreadInputReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadInputRequest {
    pub header: RequestHeader,
    pub tid: ThreadId,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetThreadInputReply {
    pub header: ReplyHeader,
    pub focus: UserHandle,
    pub capture: UserHandle,
    pub active: UserHandle,
    pub foreground: UserHandle,
    pub menu_owner: UserHandle,
    pub move_size: UserHandle,
    pub caret: UserHandle,
    pub cursor: UserHandle,
    pub show_count: i32,
    pub rect: Rect,
    __pad_60: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetLastInputTimeRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetLastInputTimeReply {
    pub header: ReplyHeader,
    pub time: u32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetKeyStateRequest {
    pub header: RequestHeader,
    pub _async: i32,
    pub key: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetKeyStateReply {
    pub header: ReplyHeader,
    pub state: u8,
    /* VARARG(keystate,bytes); */
    __pad_9: [u8; 7],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetKeyStateRequest {
    pub header: RequestHeader,
    pub _async: i32,
    /* VARARG(keystate,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetKeyStateReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetForegroundWindowRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetForegroundWindowReply {
    pub header: ReplyHeader,
    pub previous: UserHandle,
    pub send_msg_old: i32,
    pub send_msg_new: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFocusWindowRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFocusWindowReply {
    pub header: ReplyHeader,
    pub previous: UserHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetActiveWindowRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetActiveWindowReply {
    pub header: ReplyHeader,
    pub previous: UserHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCaptureWindowRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    pub flags: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCaptureWindowReply {
    pub header: ReplyHeader,
    pub previous: UserHandle,
    pub full_handle: UserHandle,
}
/*#define CAPTURE_MENU     0x01*/
/*#define CAPTURE_MOVESIZE 0x02*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCaretWindowRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    pub width: i32,
    pub height: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCaretWindowReply {
    pub header: ReplyHeader,
    pub previous: UserHandle,
    pub old_rect: Rect,
    pub old_hide: i32,
    pub old_state: i32,
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCaretInfoRequest {
    pub header: RequestHeader,
    pub flags: u32,
    pub handle: UserHandle,
    pub x: i32,
    pub y: i32,
    pub hide: i32,
    pub state: i32,
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCaretInfoReply {
    pub header: ReplyHeader,
    pub full_handle: UserHandle,
    pub old_rect: Rect,
    pub old_hide: i32,
    pub old_state: i32,
    __pad_36: [u8; 4],
}
/*#define SET_CARET_POS        0x01*/
/*#define SET_CARET_HIDE       0x02*/
/*#define SET_CARET_STATE      0x04*/
/*enum caret_state*/
/*{*/
/*    CARET_STATE_OFF,*/
/*    CARET_STATE_ON,*/
/*    CARET_STATE_TOGGLE,*/
/*    CARET_STATE_ON_IF_MOVED*/
/*};*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetHookRequest {
    pub header: RequestHeader,
    pub id: i32,
    pub pid: ProcessId,
    pub tid: ThreadId,
    pub event_min: i32,
    pub event_max: i32,
    pub proc: ClientPtr,
    pub flags: i32,
    pub unicode: i32,
    /* VARARG(module,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetHookReply {
    pub header: ReplyHeader,
    pub handle: UserHandle,
    pub active_hooks: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RemoveHookRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    pub proc: ClientPtr,
    pub id: i32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RemoveHookReply {
    pub header: ReplyHeader,
    pub active_hooks: u32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct StartHookChainRequest {
    pub header: RequestHeader,
    pub id: i32,
    pub event: i32,
    pub window: UserHandle,
    pub object_id: i32,
    pub child_id: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct StartHookChainReply {
    pub header: ReplyHeader,
    pub handle: UserHandle,
    pub pid: ProcessId,
    pub tid: ThreadId,
    pub unicode: i32,
    pub proc: ClientPtr,
    pub active_hooks: u32,
    /* VARARG(module,unicode_str); */
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FinishHookChainRequest {
    pub header: RequestHeader,
    pub id: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FinishHookChainReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetHookInfoRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    pub get_next: i32,
    pub event: i32,
    pub window: UserHandle,
    pub object_id: i32,
    pub child_id: i32,
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetHookInfoReply {
    pub header: ReplyHeader,
    pub handle: UserHandle,
    pub id: i32,
    pub pid: ProcessId,
    pub tid: ThreadId,
    pub proc: ClientPtr,
    pub unicode: i32,
    /* VARARG(module,unicode_str); */
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateClassRequest {
    pub header: RequestHeader,
    pub local: i32,
    pub atom: Atom,
    pub style: u32,
    pub instance: ModHandle,
    pub extra: i32,
    pub win_extra: i32,
    pub client_ptr: ClientPtr,
    pub name_offset: DataSize,
    /* VARARG(name,unicode_str); */
    __pad_52: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateClassReply {
    pub header: ReplyHeader,
    pub atom: Atom,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DestroyClassRequest {
    pub header: RequestHeader,
    pub atom: Atom,
    pub instance: ModHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DestroyClassReply {
    pub header: ReplyHeader,
    pub client_ptr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetClassInfoRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
    pub flags: u32,
    pub atom: Atom,
    pub style: u32,
    pub win_extra: i32,
    pub instance: ModHandle,
    pub extra_offset: i32,
    pub extra_size: DataSize,
    pub extra_value: LParam,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetClassInfoReply {
    pub header: ReplyHeader,
    pub old_atom: Atom,
    pub base_atom: Atom,
    pub old_instance: ModHandle,
    pub old_extra_value: LParam,
    pub old_style: u32,
    pub old_extra: i32,
    pub old_win_extra: i32,
    __pad_44: [u8; 4],
}
/*#define SET_CLASS_ATOM      0x0001*/
/*#define SET_CLASS_STYLE     0x0002*/
/*#define SET_CLASS_WINEXTRA  0x0004*/
/*#define SET_CLASS_INSTANCE  0x0008*/
/*#define SET_CLASS_EXTRA     0x0010*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenClipboardRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenClipboardReply {
    pub header: ReplyHeader,
    pub owner: UserHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CloseClipboardRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CloseClipboardReply {
    pub header: ReplyHeader,
    pub viewer: UserHandle,
    pub owner: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EmptyClipboardRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EmptyClipboardReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetClipboardDataRequest {
    pub header: RequestHeader,
    pub format: u32,
    pub lcid: u32,
    /* VARARG(data,bytes); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetClipboardDataReply {
    pub header: ReplyHeader,
    pub seqno: u32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetClipboardDataRequest {
    pub header: RequestHeader,
    pub format: u32,
    pub render: i32,
    pub cached: i32,
    pub seqno: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetClipboardDataReply {
    pub header: ReplyHeader,
    pub from: u32,
    pub owner: UserHandle,
    pub seqno: u32,
    pub total: DataSize,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetClipboardFormatsRequest {
    pub header: RequestHeader,
    pub format: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetClipboardFormatsReply {
    pub header: ReplyHeader,
    pub count: u32,
    /* VARARG(formats,uints); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumClipboardFormatsRequest {
    pub header: RequestHeader,
    pub previous: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnumClipboardFormatsReply {
    pub header: ReplyHeader,
    pub format: u32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReleaseClipboardRequest {
    pub header: RequestHeader,
    pub owner: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReleaseClipboardReply {
    pub header: ReplyHeader,
    pub viewer: UserHandle,
    pub owner: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetClipboardInfoRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetClipboardInfoReply {
    pub header: ReplyHeader,
    pub window: UserHandle,
    pub owner: UserHandle,
    pub viewer: UserHandle,
    pub seqno: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetClipboardViewerRequest {
    pub header: RequestHeader,
    pub viewer: UserHandle,
    pub previous: UserHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetClipboardViewerReply {
    pub header: ReplyHeader,
    pub old_viewer: UserHandle,
    pub owner: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddClipboardListenerRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddClipboardListenerReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RemoveClipboardListenerRequest {
    pub header: RequestHeader,
    pub window: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RemoveClipboardListenerReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateTokenRequest {
    pub header: RequestHeader,
    pub token_id: LUID,
    pub access: u32,
    pub primary: i32,
    pub impersonation_level: i32,
    pub expire: AbsoluteTime,
    pub group_count: i32,
    pub primary_group: i32,
    pub priv_count: i32,
    __pad_52: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateTokenReply {
    pub header: ReplyHeader,
    pub token: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenTokenRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub access: u32,
    pub attributes: u32,
    pub flags: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenTokenReply {
    pub header: ReplyHeader,
    pub token: ObjHandle,
    __pad_12: [u8; 4],
}
/*#define OPEN_TOKEN_THREAD   1*/
/*#define OPEN_TOKEN_AS_SELF  2*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetGlobalWindowsRequest {
    pub header: RequestHeader,
    pub flags: u32,
    pub shell_window: UserHandle,
    pub shell_listview: UserHandle,
    pub progman_window: UserHandle,
    pub taskman_window: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetGlobalWindowsReply {
    pub header: ReplyHeader,
    pub old_shell_window: UserHandle,
    pub old_shell_listview: UserHandle,
    pub old_progman_window: UserHandle,
    pub old_taskman_window: UserHandle,
}
/*#define SET_GLOBAL_SHELL_WINDOWS   0x01*/
/*#define SET_GLOBAL_PROGMAN_WINDOW  0x02*/
/*#define SET_GLOBAL_TASKMAN_WINDOW  0x04*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AdjustTokenPrivilegesRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub disable_all: i32,
    pub get_modified_state: i32,
    /* VARARG(privileges,luid_attr); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AdjustTokenPrivilegesReply {
    pub header: ReplyHeader,
    pub len: u32,
    /* VARARG(privileges,luid_attr); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenPrivilegesRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenPrivilegesReply {
    pub header: ReplyHeader,
    pub len: u32,
    /* VARARG(privileges,luid_attr); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CheckTokenPrivilegesRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub all_required: i32,
    /* VARARG(privileges,luid_attr); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CheckTokenPrivilegesReply {
    pub header: ReplyHeader,
    pub has_privileges: i32,
    /* VARARG(privileges,luid_attr); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DuplicateTokenRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub access: u32,
    pub primary: i32,
    pub impersonation_level: i32,
    /* VARARG(objattr,object_attributes); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DuplicateTokenReply {
    pub header: ReplyHeader,
    pub new_handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FilterTokenRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub flags: u32,
    pub privileges_size: DataSize,
    /* VARARG(privileges,luid_attr,privileges_size); */
    /* VARARG(disable_sids,sid); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FilterTokenReply {
    pub header: ReplyHeader,
    pub new_handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AccessCheckRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub desired_access: u32,
    pub mapping: GenericMap,
    /* VARARG(sd,security_descriptor); */
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AccessCheckReply {
    pub header: ReplyHeader,
    pub access_granted: u32,
    pub access_status: u32,
    pub privileges_len: u32,
    /* VARARG(privileges,luid_attr); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenSidRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub which_sid: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenSidReply {
    pub header: ReplyHeader,
    pub sid_len: DataSize,
    /* VARARG(sid,sid); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenGroupsRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub attr_mask: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenGroupsReply {
    pub header: ReplyHeader,
    pub attr_len: DataSize,
    pub sid_len: DataSize,
    /* VARARG(attrs,uints,attr_len); */
    /* VARARG(sids,sids); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenDefaultDaclRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenDefaultDaclReply {
    pub header: ReplyHeader,
    pub acl_len: DataSize,
    /* VARARG(acl,acl); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetTokenDefaultDaclRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    /* VARARG(acl,acl); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetTokenDefaultDaclReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetSecurityObjectRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub security_info: u32,
    /* VARARG(sd,security_descriptor); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetSecurityObjectReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSecurityObjectRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub security_info: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSecurityObjectReply {
    pub header: ReplyHeader,
    pub sd_len: u32,
    /* VARARG(sd,security_descriptor); */
    __pad_12: [u8; 4],
}
/*struct handle_info*/
/*{*/
/*    process_id_t owner;*/
/*    obj_handle_t handle;*/
/*    unsigned int access;*/
/*    unsigned int attributes;*/
/*    unsigned int type;*/
/*};*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSystemHandlesRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetSystemHandlesReply {
    pub header: ReplyHeader,
    pub count: u32,
    /* VARARG(data,handle_infos); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateMailslotRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub read_timeout: Timeout,
    pub max_msgsize: u32,
    /* VARARG(objattr,object_attributes); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateMailslotReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetMailslotInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub read_timeout: Timeout,
    pub flags: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetMailslotInfoReply {
    pub header: ReplyHeader,
    pub read_timeout: Timeout,
    pub max_msgsize: u32,
    __pad_20: [u8; 4],
}
/*#define MAILSLOT_SET_READ_TIMEOUT  1*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDirectoryRequest {
    pub header: RequestHeader,
    pub access: u32,
    /* VARARG(objattr,object_attributes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDirectoryReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenDirectoryRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(directory_name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenDirectoryReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetDirectoryEntryRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub index: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetDirectoryEntryReply {
    pub header: ReplyHeader,
    pub total_len: DataSize,
    pub name_len: DataSize,
    /* VARARG(name,unicode_str,name_len); */
    /* VARARG(type,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateSymlinkRequest {
    pub header: RequestHeader,
    pub access: u32,
    /* VARARG(objattr,object_attributes); */
    /* VARARG(target_name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateSymlinkReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenSymlinkRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenSymlinkReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QuerySymlinkRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QuerySymlinkReply {
    pub header: ReplyHeader,
    pub total: DataSize,
    /* VARARG(target_name,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetObjectInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetObjectInfoReply {
    pub header: ReplyHeader,
    pub access: u32,
    pub ref_count: u32,
    pub handle_count: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetObjectNameRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetObjectNameReply {
    pub header: ReplyHeader,
    pub total: DataSize,
    /* VARARG(name,unicode_str); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetObjectTypeRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetObjectTypeReply {
    pub header: ReplyHeader,
    /* VARARG(info,object_type_info); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetObjectTypesRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetObjectTypesReply {
    pub header: ReplyHeader,
    pub count: i32,
    /* VARARG(info,object_types_info); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AllocateLocallyUniqueIdRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AllocateLocallyUniqueIdReply {
    pub header: ReplyHeader,
    pub luid: LUID,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDeviceManagerRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDeviceManagerReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDeviceRequest {
    pub header: RequestHeader,
    pub rootdir: ObjHandle,
    pub user_ptr: ClientPtr,
    pub manager: ObjHandle,
    /* VARARG(name,unicode_str); */
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateDeviceReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DeleteDeviceRequest {
    pub header: RequestHeader,
    pub manager: ObjHandle,
    pub device: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DeleteDeviceReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetNextDeviceRequestRequest {
    pub header: RequestHeader,
    pub manager: ObjHandle,
    pub prev: ObjHandle,
    pub status: u32,
    pub user_ptr: ClientPtr,
    pub pending: i32,
    pub iosb_status: u32,
    pub result: DataSize,
    /* VARARG(data,bytes); */
    __pad_44: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetNextDeviceRequestReply {
    pub header: ReplyHeader,
    pub params: IrpParam,
    pub next: ObjHandle,
    pub client_tid: ThreadId,
    pub client_thread: ClientPtr,
    pub in_size: DataSize,
    /* VARARG(next_data,bytes); */
    __pad_60: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetKernelObjectPtrRequest {
    pub header: RequestHeader,
    pub manager: ObjHandle,
    pub handle: ObjHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetKernelObjectPtrReply {
    pub header: ReplyHeader,
    pub user_ptr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetKernelObjectPtrRequest {
    pub header: RequestHeader,
    pub manager: ObjHandle,
    pub handle: ObjHandle,
    __pad_20: [u8; 4],
    pub user_ptr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetKernelObjectPtrReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GrabKernelObjectRequest {
    pub header: RequestHeader,
    pub manager: ObjHandle,
    pub user_ptr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GrabKernelObjectReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReleaseKernelObjectRequest {
    pub header: RequestHeader,
    pub manager: ObjHandle,
    pub user_ptr: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ReleaseKernelObjectReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetKernelObjectHandleRequest {
    pub header: RequestHeader,
    pub manager: ObjHandle,
    pub user_ptr: ClientPtr,
    pub access: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetKernelObjectHandleReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MakeProcessSystemRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MakeProcessSystemReply {
    pub header: ReplyHeader,
    pub event: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetTokenInfoReply {
    pub header: ReplyHeader,
    pub token_id: LUID,
    pub modified_id: LUID,
    pub session_id: u32,
    pub primary: i32,
    pub impersonation_level: i32,
    pub elevation: i32,
    pub group_count: i32,
    pub privilege_count: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateLinkedTokenRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateLinkedTokenReply {
    pub header: ReplyHeader,
    pub linked: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateCompletionRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub concurrent: u32,
    /* VARARG(objattr,object_attributes); */
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateCompletionReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenCompletionRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(filename,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenCompletionReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddCompletionRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub ckey: ApcParam,
    pub cvalue: ApcParam,
    pub information: ApcParam,
    pub status: u32,
    __pad_44: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddCompletionReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RemoveCompletionRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct RemoveCompletionReply {
    pub header: ReplyHeader,
    pub ckey: ApcParam,
    pub cvalue: ApcParam,
    pub information: ApcParam,
    pub status: u32,
    __pad_36: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueryCompletionRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct QueryCompletionReply {
    pub header: ReplyHeader,
    pub depth: u32,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCompletionInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub ckey: ApcParam,
    pub chandle: ObjHandle,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCompletionInfoReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddFdCompletionRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub cvalue: ApcParam,
    pub information: ApcParam,
    pub status: u32,
    pub _async: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AddFdCompletionReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFdCompletionModeRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub flags: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFdCompletionModeReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFdDispInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub flags: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFdDispInfoReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFdNameInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub rootdir: ObjHandle,
    pub namelen: DataSize,
    pub link: i32,
    pub flags: u32,
    /* VARARG(name,unicode_str,namelen); */
    /* VARARG(filename,string); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFdNameInfoReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFdEofInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub eof: FilePos,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetFdEofInfoReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowLayeredInfoRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetWindowLayeredInfoReply {
    pub header: ReplyHeader,
    pub color_key: u32,
    pub alpha: u32,
    pub flags: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowLayeredInfoRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
    pub color_key: u32,
    pub alpha: u32,
    pub flags: u32,
    __pad_28: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetWindowLayeredInfoReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AllocUserHandleRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AllocUserHandleReply {
    pub header: ReplyHeader,
    pub handle: UserHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FreeUserHandleRequest {
    pub header: RequestHeader,
    pub handle: UserHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FreeUserHandleReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCursorRequest {
    pub header: RequestHeader,
    pub flags: u32,
    pub handle: UserHandle,
    pub show_count: i32,
    pub x: i32,
    pub y: i32,
    pub clip: Rect,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetCursorReply {
    pub header: ReplyHeader,
    pub prev_handle: UserHandle,
    pub prev_count: i32,
    pub prev_x: i32,
    pub prev_y: i32,
    pub new_x: i32,
    pub new_y: i32,
    pub new_clip: Rect,
    pub last_change: u32,
    __pad_52: [u8; 4],
}
/*#define SET_CURSOR_HANDLE 0x01*/
/*#define SET_CURSOR_COUNT  0x02*/
/*#define SET_CURSOR_POS    0x04*/
/*#define SET_CURSOR_CLIP   0x08*/
/*#define SET_CURSOR_NOCLIP 0x10*/
/*#define SET_CURSOR_FSCLIP 0x20*/
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetCursorHistoryRequest {
    pub header: RequestHeader,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetCursorHistoryReply {
    pub header: ReplyHeader,
    /* VARARG(history,cursor_positions); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetRawinputBufferRequest {
    pub header: RequestHeader,
    pub header_size: DataSize,
    pub read_data: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetRawinputBufferReply {
    pub header: ReplyHeader,
    pub next_size: DataSize,
    pub count: u32,
    /* VARARG(data,bytes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UpdateRawinputDevicesRequest {
    pub header: RequestHeader,
    /* VARARG(devices,rawinput_devices); */
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UpdateRawinputDevicesReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateJobRequest {
    pub header: RequestHeader,
    pub access: u32,
    /* VARARG(objattr,object_attributes); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CreateJobReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenJobRequest {
    pub header: RequestHeader,
    pub access: u32,
    pub attributes: u32,
    pub rootdir: ObjHandle,
    /* VARARG(name,unicode_str); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenJobReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AssignJobRequest {
    pub header: RequestHeader,
    pub job: ObjHandle,
    pub process: ObjHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AssignJobReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ProcessInJobRequest {
    pub header: RequestHeader,
    pub job: ObjHandle,
    pub process: ObjHandle,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ProcessInJobReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetJobLimitsRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub limit_flags: u32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetJobLimitsReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetJobCompletionPortRequest {
    pub header: RequestHeader,
    pub job: ObjHandle,
    pub port: ObjHandle,
    __pad_20: [u8; 4],
    pub key: ClientPtr,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetJobCompletionPortReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetJobInfoRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetJobInfoReply {
    pub header: ReplyHeader,
    pub total_processes: i32,
    pub active_processes: i32,
    /* VARARG(pids,uints); */
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct TerminateJobRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
    pub status: i32,
    __pad_20: [u8; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct TerminateJobReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SuspendProcessRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SuspendProcessReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ResumeProcessRequest {
    pub header: RequestHeader,
    pub handle: ObjHandle,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ResumeProcessReply {
    pub header: ReplyHeader,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetNextThreadRequest {
    pub header: RequestHeader,
    pub process: ObjHandle,
    pub last: ObjHandle,
    pub access: u32,
    pub attributes: u32,
    pub flags: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GetNextThreadReply {
    pub header: ReplyHeader,
    pub handle: ObjHandle,
    __pad_12: [u8; 4],
}
#[repr(C)]
#[allow(dead_code)]
#[derive(Clone, Copy, Debug, Default)]
pub enum RequestType {
    #[default]
    NewProcess = 0,
    GetNewProcessInfo = 1,
    NewThread = 2,
    GetStartupInfo = 3,
    InitProcessDone = 4,
    InitFirstThread = 5,
    InitThread = 6,
    TerminateProcess = 7,
    TerminateThread = 8,
    GetProcessInfo = 9,
    GetProcessDebugInfo = 10,
    GetProcessImageName = 11,
    GetProcessVmCounters = 12,
    SetProcessInfo = 13,
    GetThreadInfo = 14,
    GetThreadTimes = 15,
    SetThreadInfo = 16,
    SuspendThread = 17,
    ResumeThread = 18,
    QueueApc = 19,
    GetApcResult = 20,
    CloseHandle = 21,
    SetHandleInfo = 22,
    DupHandle = 23,
    CompareObjects = 24,
    MakeTemporary = 25,
    OpenProcess = 26,
    OpenThread = 27,
    Select = 28,
    CreateEvent = 29,
    EventOp = 30,
    QueryEvent = 31,
    OpenEvent = 32,
    CreateKeyedEvent = 33,
    OpenKeyedEvent = 34,
    CreateMutex = 35,
    ReleaseMutex = 36,
    OpenMutex = 37,
    QueryMutex = 38,
    CreateSemaphore = 39,
    ReleaseSemaphore = 40,
    QuerySemaphore = 41,
    OpenSemaphore = 42,
    CreateFile = 43,
    OpenFileObject = 44,
    AllocFileHandle = 45,
    GetHandleUnixName = 46,
    GetHandleFd = 47,
    GetDirectoryCacheEntry = 48,
    Flush = 49,
    GetFileInfo = 50,
    GetVolumeInfo = 51,
    LockFile = 52,
    UnlockFile = 53,
    RecvSocket = 54,
    SendSocket = 55,
    SocketGetEvents = 56,
    SocketSendIcmpId = 57,
    SocketGetIcmpId = 58,
    GetNextConsoleRequest = 59,
    ReadDirectoryChanges = 60,
    ReadChange = 61,
    CreateMapping = 62,
    OpenMapping = 63,
    GetMappingInfo = 64,
    GetImageMapAddress = 65,
    MapView = 66,
    MapImageView = 67,
    MapBuiltinView = 68,
    GetImageViewInfo = 69,
    UnmapView = 70,
    GetMappingCommittedRange = 71,
    AddMappingCommittedRange = 72,
    IsSameMapping = 73,
    GetMappingFilename = 74,
    ListProcesses = 75,
    CreateDebugObj = 76,
    WaitDebugEvent = 77,
    QueueExceptionEvent = 78,
    GetExceptionStatus = 79,
    ContinueDebugEvent = 80,
    DebugProcess = 81,
    SetDebugObjInfo = 82,
    ReadProcessMemory = 83,
    WriteProcessMemory = 84,
    CreateKey = 85,
    OpenKey = 86,
    DeleteKey = 87,
    FlushKey = 88,
    EnumKey = 89,
    SetKeyValue = 90,
    GetKeyValue = 91,
    EnumKeyValue = 92,
    DeleteKeyValue = 93,
    LoadRegistry = 94,
    UnloadRegistry = 95,
    SaveRegistry = 96,
    SetRegistryNotification = 97,
    RenameKey = 98,
    CreateTimer = 99,
    OpenTimer = 100,
    SetTimer = 101,
    CancelTimer = 102,
    GetTimerInfo = 103,
    GetThreadContext = 104,
    SetThreadContext = 105,
    GetSelectorEntry = 106,
    AddAtom = 107,
    DeleteAtom = 108,
    FindAtom = 109,
    GetAtomInformation = 110,
    GetMsgQueue = 111,
    SetQueueFd = 112,
    SetQueueMask = 113,
    GetQueueStatus = 114,
    GetProcessIdleEvent = 115,
    SendMessage = 116,
    PostQuitMessage = 117,
    SendHardwareMessage = 118,
    GetMessage = 119,
    ReplyMessage = 120,
    AcceptHardwareMessage = 121,
    GetMessageReply = 122,
    SetWinTimer = 123,
    KillWinTimer = 124,
    IsWindowHung = 125,
    GetSerialInfo = 126,
    SetSerialInfo = 127,
    CancelSync = 128,
    RegisterAsync = 129,
    CancelAsync = 130,
    GetAsyncResult = 131,
    SetAsyncDirectResult = 132,
    Read = 133,
    Write = 134,
    Ioctl = 135,
    SetIrpResult = 136,
    CreateNamedPipe = 137,
    SetNamedPipeInfo = 138,
    CreateWindow = 139,
    DestroyWindow = 140,
    GetDesktopWindow = 141,
    SetWindowOwner = 142,
    GetWindowInfo = 143,
    SetWindowInfo = 144,
    SetParent = 145,
    GetWindowParents = 146,
    GetWindowChildren = 147,
    GetWindowChildrenFromPoint = 148,
    GetWindowTree = 149,
    SetWindowPos = 150,
    GetWindowRectangles = 151,
    GetWindowText = 152,
    SetWindowText = 153,
    GetWindowsOffset = 154,
    GetVisibleRegion = 155,
    GetSurfaceRegion = 156,
    GetWindowRegion = 157,
    SetWindowRegion = 158,
    GetUpdateRegion = 159,
    UpdateWindowZorder = 160,
    RedrawWindow = 161,
    SetWindowProperty = 162,
    RemoveWindowProperty = 163,
    GetWindowProperty = 164,
    GetWindowProperties = 165,
    CreateWinstation = 166,
    OpenWinstation = 167,
    CloseWinstation = 168,
    GetProcessWinstation = 169,
    SetProcessWinstation = 170,
    EnumWinstation = 171,
    CreateDesktop = 172,
    OpenDesktop = 173,
    OpenInputDesktop = 174,
    CloseDesktop = 175,
    GetThreadDesktop = 176,
    SetThreadDesktop = 177,
    EnumDesktop = 178,
    SetUserObjectInfo = 179,
    RegisterHotkey = 180,
    UnregisterHotkey = 181,
    AttachThreadInput = 182,
    GetThreadInput = 183,
    GetLastInputTime = 184,
    GetKeyState = 185,
    SetKeyState = 186,
    SetForegroundWindow = 187,
    SetFocusWindow = 188,
    SetActiveWindow = 189,
    SetCaptureWindow = 190,
    SetCaretWindow = 191,
    SetCaretInfo = 192,
    SetHook = 193,
    RemoveHook = 194,
    StartHookChain = 195,
    FinishHookChain = 196,
    GetHookInfo = 197,
    CreateClass = 198,
    DestroyClass = 199,
    SetClassInfo = 200,
    OpenClipboard = 201,
    CloseClipboard = 202,
    EmptyClipboard = 203,
    SetClipboardData = 204,
    GetClipboardData = 205,
    GetClipboardFormats = 206,
    EnumClipboardFormats = 207,
    ReleaseClipboard = 208,
    GetClipboardInfo = 209,
    SetClipboardViewer = 210,
    AddClipboardListener = 211,
    RemoveClipboardListener = 212,
    CreateToken = 213,
    OpenToken = 214,
    SetGlobalWindows = 215,
    AdjustTokenPrivileges = 216,
    GetTokenPrivileges = 217,
    CheckTokenPrivileges = 218,
    DuplicateToken = 219,
    FilterToken = 220,
    AccessCheck = 221,
    GetTokenSid = 222,
    GetTokenGroups = 223,
    GetTokenDefaultDacl = 224,
    SetTokenDefaultDacl = 225,
    SetSecurityObject = 226,
    GetSecurityObject = 227,
    GetSystemHandles = 228,
    CreateMailslot = 229,
    SetMailslotInfo = 230,
    CreateDirectory = 231,
    OpenDirectory = 232,
    GetDirectoryEntry = 233,
    CreateSymlink = 234,
    OpenSymlink = 235,
    QuerySymlink = 236,
    GetObjectInfo = 237,
    GetObjectName = 238,
    GetObjectType = 239,
    GetObjectTypes = 240,
    AllocateLocallyUniqueId = 241,
    CreateDeviceManager = 242,
    CreateDevice = 243,
    DeleteDevice = 244,
    GetNextDeviceRequest = 245,
    GetKernelObjectPtr = 246,
    SetKernelObjectPtr = 247,
    GrabKernelObject = 248,
    ReleaseKernelObject = 249,
    GetKernelObjectHandle = 250,
    MakeProcessSystem = 251,
    GetTokenInfo = 252,
    CreateLinkedToken = 253,
    CreateCompletion = 254,
    OpenCompletion = 255,
    AddCompletion = 256,
    RemoveCompletion = 257,
    QueryCompletion = 258,
    SetCompletionInfo = 259,
    AddFdCompletion = 260,
    SetFdCompletionMode = 261,
    SetFdDispInfo = 262,
    SetFdNameInfo = 263,
    SetFdEofInfo = 264,
    GetWindowLayeredInfo = 265,
    SetWindowLayeredInfo = 266,
    AllocUserHandle = 267,
    FreeUserHandle = 268,
    SetCursor = 269,
    GetCursorHistory = 270,
    GetRawinputBuffer = 271,
    UpdateRawinputDevices = 272,
    CreateJob = 273,
    OpenJob = 274,
    AssignJob = 275,
    ProcessInJob = 276,
    SetJobLimits = 277,
    SetJobCompletionPort = 278,
    GetJobInfo = 279,
    TerminateJob = 280,
    SuspendProcess = 281,
    ResumeProcess = 282,
    GetNextThread = 283,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub union GenericRequest {
    pub header: RequestHeader,
    pub new_process: NewProcessRequest,
    pub get_new_process_info: GetNewProcessInfoRequest,
    pub new_thread: NewThreadRequest,
    pub get_startup_info: GetStartupInfoRequest,
    pub init_process_done: InitProcessDoneRequest,
    pub init_first_thread: InitFirstThreadRequest,
    pub init_thread: InitThreadRequest,
    pub terminate_process: TerminateProcessRequest,
    pub terminate_thread: TerminateThreadRequest,
    pub get_process_info: GetProcessInfoRequest,
    pub get_process_debug_info: GetProcessDebugInfoRequest,
    pub get_process_image_name: GetProcessImageNameRequest,
    pub get_process_vm_counters: GetProcessVmCountersRequest,
    pub set_process_info: SetProcessInfoRequest,
    pub get_thread_info: GetThreadInfoRequest,
    pub get_thread_times: GetThreadTimesRequest,
    pub set_thread_info: SetThreadInfoRequest,
    pub suspend_thread: SuspendThreadRequest,
    pub resume_thread: ResumeThreadRequest,
    pub queue_apc: QueueApcRequest,
    pub get_apc_result: GetApcResultRequest,
    pub close_handle: CloseHandleRequest,
    pub set_handle_info: SetHandleInfoRequest,
    pub dup_handle: DupHandleRequest,
    pub compare_objects: CompareObjectsRequest,
    pub make_temporary: MakeTemporaryRequest,
    pub open_process: OpenProcessRequest,
    pub open_thread: OpenThreadRequest,
    pub select: SelectRequest,
    pub create_event: CreateEventRequest,
    pub event_op: EventOpRequest,
    pub query_event: QueryEventRequest,
    pub open_event: OpenEventRequest,
    pub create_keyed_event: CreateKeyedEventRequest,
    pub open_keyed_event: OpenKeyedEventRequest,
    pub create_mutex: CreateMutexRequest,
    pub release_mutex: ReleaseMutexRequest,
    pub open_mutex: OpenMutexRequest,
    pub query_mutex: QueryMutexRequest,
    pub create_semaphore: CreateSemaphoreRequest,
    pub release_semaphore: ReleaseSemaphoreRequest,
    pub query_semaphore: QuerySemaphoreRequest,
    pub open_semaphore: OpenSemaphoreRequest,
    pub create_file: CreateFileRequest,
    pub open_file_object: OpenFileObjectRequest,
    pub alloc_file_handle: AllocFileHandleRequest,
    pub get_handle_unix_name: GetHandleUnixNameRequest,
    pub get_handle_fd: GetHandleFdRequest,
    pub get_directory_cache_entry: GetDirectoryCacheEntryRequest,
    pub flush: FlushRequest,
    pub get_file_info: GetFileInfoRequest,
    pub get_volume_info: GetVolumeInfoRequest,
    pub lock_file: LockFileRequest,
    pub unlock_file: UnlockFileRequest,
    pub recv_socket: RecvSocketRequest,
    pub send_socket: SendSocketRequest,
    pub socket_get_events: SocketGetEventsRequest,
    pub socket_send_icmp_id: SocketSendIcmpIdRequest,
    pub socket_get_icmp_id: SocketGetIcmpIdRequest,
    pub get_next_console_request: GetNextConsoleRequestRequest,
    pub read_directory_changes: ReadDirectoryChangesRequest,
    pub read_change: ReadChangeRequest,
    pub create_mapping: CreateMappingRequest,
    pub open_mapping: OpenMappingRequest,
    pub get_mapping_info: GetMappingInfoRequest,
    pub get_image_map_address: GetImageMapAddressRequest,
    pub map_view: MapViewRequest,
    pub map_image_view: MapImageViewRequest,
    pub map_builtin_view: MapBuiltinViewRequest,
    pub get_image_view_info: GetImageViewInfoRequest,
    pub unmap_view: UnmapViewRequest,
    pub get_mapping_committed_range: GetMappingCommittedRangeRequest,
    pub add_mapping_committed_range: AddMappingCommittedRangeRequest,
    pub is_same_mapping: IsSameMappingRequest,
    pub get_mapping_filename: GetMappingFilenameRequest,
    pub list_processes: ListProcessesRequest,
    pub create_debug_obj: CreateDebugObjRequest,
    pub wait_debug_event: WaitDebugEventRequest,
    pub queue_exception_event: QueueExceptionEventRequest,
    pub get_exception_status: GetExceptionStatusRequest,
    pub continue_debug_event: ContinueDebugEventRequest,
    pub debug_process: DebugProcessRequest,
    pub set_debug_obj_info: SetDebugObjInfoRequest,
    pub read_process_memory: ReadProcessMemoryRequest,
    pub write_process_memory: WriteProcessMemoryRequest,
    pub create_key: CreateKeyRequest,
    pub open_key: OpenKeyRequest,
    pub delete_key: DeleteKeyRequest,
    pub flush_key: FlushKeyRequest,
    pub enum_key: EnumKeyRequest,
    pub set_key_value: SetKeyValueRequest,
    pub get_key_value: GetKeyValueRequest,
    pub enum_key_value: EnumKeyValueRequest,
    pub delete_key_value: DeleteKeyValueRequest,
    pub load_registry: LoadRegistryRequest,
    pub unload_registry: UnloadRegistryRequest,
    pub save_registry: SaveRegistryRequest,
    pub set_registry_notification: SetRegistryNotificationRequest,
    pub rename_key: RenameKeyRequest,
    pub create_timer: CreateTimerRequest,
    pub open_timer: OpenTimerRequest,
    pub set_timer: SetTimerRequest,
    pub cancel_timer: CancelTimerRequest,
    pub get_timer_info: GetTimerInfoRequest,
    pub get_thread_context: GetThreadContextRequest,
    pub set_thread_context: SetThreadContextRequest,
    pub get_selector_entry: GetSelectorEntryRequest,
    pub add_atom: AddAtomRequest,
    pub delete_atom: DeleteAtomRequest,
    pub find_atom: FindAtomRequest,
    pub get_atom_information: GetAtomInformationRequest,
    pub get_msg_queue: GetMsgQueueRequest,
    pub set_queue_fd: SetQueueFdRequest,
    pub set_queue_mask: SetQueueMaskRequest,
    pub get_queue_status: GetQueueStatusRequest,
    pub get_process_idle_event: GetProcessIdleEventRequest,
    pub send_message: SendMessageRequest,
    pub post_quit_message: PostQuitMessageRequest,
    pub send_hardware_message: SendHardwareMessageRequest,
    pub get_message: GetMessageRequest,
    pub reply_message: ReplyMessageRequest,
    pub accept_hardware_message: AcceptHardwareMessageRequest,
    pub get_message_reply: GetMessageReplyRequest,
    pub set_win_timer: SetWinTimerRequest,
    pub kill_win_timer: KillWinTimerRequest,
    pub is_window_hung: IsWindowHungRequest,
    pub get_serial_info: GetSerialInfoRequest,
    pub set_serial_info: SetSerialInfoRequest,
    pub cancel_sync: CancelSyncRequest,
    pub register_async: RegisterAsyncRequest,
    pub cancel_async: CancelAsyncRequest,
    pub get_async_result: GetAsyncResultRequest,
    pub set_async_direct_result: SetAsyncDirectResultRequest,
    pub read: ReadRequest,
    pub write: WriteRequest,
    pub ioctl: IoctlRequest,
    pub set_irp_result: SetIrpResultRequest,
    pub create_named_pipe: CreateNamedPipeRequest,
    pub set_named_pipe_info: SetNamedPipeInfoRequest,
    pub create_window: CreateWindowRequest,
    pub destroy_window: DestroyWindowRequest,
    pub get_desktop_window: GetDesktopWindowRequest,
    pub set_window_owner: SetWindowOwnerRequest,
    pub get_window_info: GetWindowInfoRequest,
    pub set_window_info: SetWindowInfoRequest,
    pub set_parent: SetParentRequest,
    pub get_window_parents: GetWindowParentsRequest,
    pub get_window_children: GetWindowChildrenRequest,
    pub get_window_children_from_point: GetWindowChildrenFromPointRequest,
    pub get_window_tree: GetWindowTreeRequest,
    pub set_window_pos: SetWindowPosRequest,
    pub get_window_rectangles: GetWindowRectanglesRequest,
    pub get_window_text: GetWindowTextRequest,
    pub set_window_text: SetWindowTextRequest,
    pub get_windows_offset: GetWindowsOffsetRequest,
    pub get_visible_region: GetVisibleRegionRequest,
    pub get_surface_region: GetSurfaceRegionRequest,
    pub get_window_region: GetWindowRegionRequest,
    pub set_window_region: SetWindowRegionRequest,
    pub get_update_region: GetUpdateRegionRequest,
    pub update_window_zorder: UpdateWindowZorderRequest,
    pub redraw_window: RedrawWindowRequest,
    pub set_window_property: SetWindowPropertyRequest,
    pub remove_window_property: RemoveWindowPropertyRequest,
    pub get_window_property: GetWindowPropertyRequest,
    pub get_window_properties: GetWindowPropertiesRequest,
    pub create_winstation: CreateWinstationRequest,
    pub open_winstation: OpenWinstationRequest,
    pub close_winstation: CloseWinstationRequest,
    pub get_process_winstation: GetProcessWinstationRequest,
    pub set_process_winstation: SetProcessWinstationRequest,
    pub enum_winstation: EnumWinstationRequest,
    pub create_desktop: CreateDesktopRequest,
    pub open_desktop: OpenDesktopRequest,
    pub open_input_desktop: OpenInputDesktopRequest,
    pub close_desktop: CloseDesktopRequest,
    pub get_thread_desktop: GetThreadDesktopRequest,
    pub set_thread_desktop: SetThreadDesktopRequest,
    pub enum_desktop: EnumDesktopRequest,
    pub set_user_object_info: SetUserObjectInfoRequest,
    pub register_hotkey: RegisterHotkeyRequest,
    pub unregister_hotkey: UnregisterHotkeyRequest,
    pub attach_thread_input: AttachThreadInputRequest,
    pub get_thread_input: GetThreadInputRequest,
    pub get_last_input_time: GetLastInputTimeRequest,
    pub get_key_state: GetKeyStateRequest,
    pub set_key_state: SetKeyStateRequest,
    pub set_foreground_window: SetForegroundWindowRequest,
    pub set_focus_window: SetFocusWindowRequest,
    pub set_active_window: SetActiveWindowRequest,
    pub set_capture_window: SetCaptureWindowRequest,
    pub set_caret_window: SetCaretWindowRequest,
    pub set_caret_info: SetCaretInfoRequest,
    pub set_hook: SetHookRequest,
    pub remove_hook: RemoveHookRequest,
    pub start_hook_chain: StartHookChainRequest,
    pub finish_hook_chain: FinishHookChainRequest,
    pub get_hook_info: GetHookInfoRequest,
    pub create_class: CreateClassRequest,
    pub destroy_class: DestroyClassRequest,
    pub set_class_info: SetClassInfoRequest,
    pub open_clipboard: OpenClipboardRequest,
    pub close_clipboard: CloseClipboardRequest,
    pub empty_clipboard: EmptyClipboardRequest,
    pub set_clipboard_data: SetClipboardDataRequest,
    pub get_clipboard_data: GetClipboardDataRequest,
    pub get_clipboard_formats: GetClipboardFormatsRequest,
    pub enum_clipboard_formats: EnumClipboardFormatsRequest,
    pub release_clipboard: ReleaseClipboardRequest,
    pub get_clipboard_info: GetClipboardInfoRequest,
    pub set_clipboard_viewer: SetClipboardViewerRequest,
    pub add_clipboard_listener: AddClipboardListenerRequest,
    pub remove_clipboard_listener: RemoveClipboardListenerRequest,
    pub create_token: CreateTokenRequest,
    pub open_token: OpenTokenRequest,
    pub set_global_windows: SetGlobalWindowsRequest,
    pub adjust_token_privileges: AdjustTokenPrivilegesRequest,
    pub get_token_privileges: GetTokenPrivilegesRequest,
    pub check_token_privileges: CheckTokenPrivilegesRequest,
    pub duplicate_token: DuplicateTokenRequest,
    pub filter_token: FilterTokenRequest,
    pub access_check: AccessCheckRequest,
    pub get_token_sid: GetTokenSidRequest,
    pub get_token_groups: GetTokenGroupsRequest,
    pub get_token_default_dacl: GetTokenDefaultDaclRequest,
    pub set_token_default_dacl: SetTokenDefaultDaclRequest,
    pub set_security_object: SetSecurityObjectRequest,
    pub get_security_object: GetSecurityObjectRequest,
    pub get_system_handles: GetSystemHandlesRequest,
    pub create_mailslot: CreateMailslotRequest,
    pub set_mailslot_info: SetMailslotInfoRequest,
    pub create_directory: CreateDirectoryRequest,
    pub open_directory: OpenDirectoryRequest,
    pub get_directory_entry: GetDirectoryEntryRequest,
    pub create_symlink: CreateSymlinkRequest,
    pub open_symlink: OpenSymlinkRequest,
    pub query_symlink: QuerySymlinkRequest,
    pub get_object_info: GetObjectInfoRequest,
    pub get_object_name: GetObjectNameRequest,
    pub get_object_type: GetObjectTypeRequest,
    pub get_object_types: GetObjectTypesRequest,
    pub allocate_locally_unique_id: AllocateLocallyUniqueIdRequest,
    pub create_device_manager: CreateDeviceManagerRequest,
    pub create_device: CreateDeviceRequest,
    pub delete_device: DeleteDeviceRequest,
    pub get_next_device_request: GetNextDeviceRequestRequest,
    pub get_kernel_object_ptr: GetKernelObjectPtrRequest,
    pub set_kernel_object_ptr: SetKernelObjectPtrRequest,
    pub grab_kernel_object: GrabKernelObjectRequest,
    pub release_kernel_object: ReleaseKernelObjectRequest,
    pub get_kernel_object_handle: GetKernelObjectHandleRequest,
    pub make_process_system: MakeProcessSystemRequest,
    pub get_token_info: GetTokenInfoRequest,
    pub create_linked_token: CreateLinkedTokenRequest,
    pub create_completion: CreateCompletionRequest,
    pub open_completion: OpenCompletionRequest,
    pub add_completion: AddCompletionRequest,
    pub remove_completion: RemoveCompletionRequest,
    pub query_completion: QueryCompletionRequest,
    pub set_completion_info: SetCompletionInfoRequest,
    pub add_fd_completion: AddFdCompletionRequest,
    pub set_fd_completion_mode: SetFdCompletionModeRequest,
    pub set_fd_disp_info: SetFdDispInfoRequest,
    pub set_fd_name_info: SetFdNameInfoRequest,
    pub set_fd_eof_info: SetFdEofInfoRequest,
    pub get_window_layered_info: GetWindowLayeredInfoRequest,
    pub set_window_layered_info: SetWindowLayeredInfoRequest,
    pub alloc_user_handle: AllocUserHandleRequest,
    pub free_user_handle: FreeUserHandleRequest,
    pub set_cursor: SetCursorRequest,
    pub get_cursor_history: GetCursorHistoryRequest,
    pub get_rawinput_buffer: GetRawinputBufferRequest,
    pub update_rawinput_devices: UpdateRawinputDevicesRequest,
    pub create_job: CreateJobRequest,
    pub open_job: OpenJobRequest,
    pub assign_job: AssignJobRequest,
    pub process_in_job: ProcessInJobRequest,
    pub set_job_limits: SetJobLimitsRequest,
    pub set_job_completion_port: SetJobCompletionPortRequest,
    pub get_job_info: GetJobInfoRequest,
    pub terminate_job: TerminateJobRequest,
    pub suspend_process: SuspendProcessRequest,
    pub resume_process: ResumeProcessRequest,
    pub get_next_thread: GetNextThreadRequest,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub union GenericReply {
    pub header: ReplyHeader,
    pub new_process: NewProcessReply,
    pub get_new_process_info: GetNewProcessInfoReply,
    pub new_thread: NewThreadReply,
    pub get_startup_info: GetStartupInfoReply,
    pub init_process_done: InitProcessDoneReply,
    pub init_first_thread: InitFirstThreadReply,
    pub init_thread: InitThreadReply,
    pub terminate_process: TerminateProcessReply,
    pub terminate_thread: TerminateThreadReply,
    pub get_process_info: GetProcessInfoReply,
    pub get_process_debug_info: GetProcessDebugInfoReply,
    pub get_process_image_name: GetProcessImageNameReply,
    pub get_process_vm_counters: GetProcessVmCountersReply,
    pub set_process_info: SetProcessInfoReply,
    pub get_thread_info: GetThreadInfoReply,
    pub get_thread_times: GetThreadTimesReply,
    pub set_thread_info: SetThreadInfoReply,
    pub suspend_thread: SuspendThreadReply,
    pub resume_thread: ResumeThreadReply,
    pub queue_apc: QueueApcReply,
    pub get_apc_result: GetApcResultReply,
    pub close_handle: CloseHandleReply,
    pub set_handle_info: SetHandleInfoReply,
    pub dup_handle: DupHandleReply,
    pub compare_objects: CompareObjectsReply,
    pub make_temporary: MakeTemporaryReply,
    pub open_process: OpenProcessReply,
    pub open_thread: OpenThreadReply,
    pub select: SelectReply,
    pub create_event: CreateEventReply,
    pub event_op: EventOpReply,
    pub query_event: QueryEventReply,
    pub open_event: OpenEventReply,
    pub create_keyed_event: CreateKeyedEventReply,
    pub open_keyed_event: OpenKeyedEventReply,
    pub create_mutex: CreateMutexReply,
    pub release_mutex: ReleaseMutexReply,
    pub open_mutex: OpenMutexReply,
    pub query_mutex: QueryMutexReply,
    pub create_semaphore: CreateSemaphoreReply,
    pub release_semaphore: ReleaseSemaphoreReply,
    pub query_semaphore: QuerySemaphoreReply,
    pub open_semaphore: OpenSemaphoreReply,
    pub create_file: CreateFileReply,
    pub open_file_object: OpenFileObjectReply,
    pub alloc_file_handle: AllocFileHandleReply,
    pub get_handle_unix_name: GetHandleUnixNameReply,
    pub get_handle_fd: GetHandleFdReply,
    pub get_directory_cache_entry: GetDirectoryCacheEntryReply,
    pub flush: FlushReply,
    pub get_file_info: GetFileInfoReply,
    pub get_volume_info: GetVolumeInfoReply,
    pub lock_file: LockFileReply,
    pub unlock_file: UnlockFileReply,
    pub recv_socket: RecvSocketReply,
    pub send_socket: SendSocketReply,
    pub socket_get_events: SocketGetEventsReply,
    pub socket_send_icmp_id: SocketSendIcmpIdReply,
    pub socket_get_icmp_id: SocketGetIcmpIdReply,
    pub get_next_console_request: GetNextConsoleRequestReply,
    pub read_directory_changes: ReadDirectoryChangesReply,
    pub read_change: ReadChangeReply,
    pub create_mapping: CreateMappingReply,
    pub open_mapping: OpenMappingReply,
    pub get_mapping_info: GetMappingInfoReply,
    pub get_image_map_address: GetImageMapAddressReply,
    pub map_view: MapViewReply,
    pub map_image_view: MapImageViewReply,
    pub map_builtin_view: MapBuiltinViewReply,
    pub get_image_view_info: GetImageViewInfoReply,
    pub unmap_view: UnmapViewReply,
    pub get_mapping_committed_range: GetMappingCommittedRangeReply,
    pub add_mapping_committed_range: AddMappingCommittedRangeReply,
    pub is_same_mapping: IsSameMappingReply,
    pub get_mapping_filename: GetMappingFilenameReply,
    pub list_processes: ListProcessesReply,
    pub create_debug_obj: CreateDebugObjReply,
    pub wait_debug_event: WaitDebugEventReply,
    pub queue_exception_event: QueueExceptionEventReply,
    pub get_exception_status: GetExceptionStatusReply,
    pub continue_debug_event: ContinueDebugEventReply,
    pub debug_process: DebugProcessReply,
    pub set_debug_obj_info: SetDebugObjInfoReply,
    pub read_process_memory: ReadProcessMemoryReply,
    pub write_process_memory: WriteProcessMemoryReply,
    pub create_key: CreateKeyReply,
    pub open_key: OpenKeyReply,
    pub delete_key: DeleteKeyReply,
    pub flush_key: FlushKeyReply,
    pub enum_key: EnumKeyReply,
    pub set_key_value: SetKeyValueReply,
    pub get_key_value: GetKeyValueReply,
    pub enum_key_value: EnumKeyValueReply,
    pub delete_key_value: DeleteKeyValueReply,
    pub load_registry: LoadRegistryReply,
    pub unload_registry: UnloadRegistryReply,
    pub save_registry: SaveRegistryReply,
    pub set_registry_notification: SetRegistryNotificationReply,
    pub rename_key: RenameKeyReply,
    pub create_timer: CreateTimerReply,
    pub open_timer: OpenTimerReply,
    pub set_timer: SetTimerReply,
    pub cancel_timer: CancelTimerReply,
    pub get_timer_info: GetTimerInfoReply,
    pub get_thread_context: GetThreadContextReply,
    pub set_thread_context: SetThreadContextReply,
    pub get_selector_entry: GetSelectorEntryReply,
    pub add_atom: AddAtomReply,
    pub delete_atom: DeleteAtomReply,
    pub find_atom: FindAtomReply,
    pub get_atom_information: GetAtomInformationReply,
    pub get_msg_queue: GetMsgQueueReply,
    pub set_queue_fd: SetQueueFdReply,
    pub set_queue_mask: SetQueueMaskReply,
    pub get_queue_status: GetQueueStatusReply,
    pub get_process_idle_event: GetProcessIdleEventReply,
    pub send_message: SendMessageReply,
    pub post_quit_message: PostQuitMessageReply,
    pub send_hardware_message: SendHardwareMessageReply,
    pub get_message: GetMessageReply,
    pub reply_message: ReplyMessageReply,
    pub accept_hardware_message: AcceptHardwareMessageReply,
    pub get_message_reply: GetMessageReplyReply,
    pub set_win_timer: SetWinTimerReply,
    pub kill_win_timer: KillWinTimerReply,
    pub is_window_hung: IsWindowHungReply,
    pub get_serial_info: GetSerialInfoReply,
    pub set_serial_info: SetSerialInfoReply,
    pub cancel_sync: CancelSyncReply,
    pub register_async: RegisterAsyncReply,
    pub cancel_async: CancelAsyncReply,
    pub get_async_result: GetAsyncResultReply,
    pub set_async_direct_result: SetAsyncDirectResultReply,
    pub read: ReadReply,
    pub write: WriteReply,
    pub ioctl: IoctlReply,
    pub set_irp_result: SetIrpResultReply,
    pub create_named_pipe: CreateNamedPipeReply,
    pub set_named_pipe_info: SetNamedPipeInfoReply,
    pub create_window: CreateWindowReply,
    pub destroy_window: DestroyWindowReply,
    pub get_desktop_window: GetDesktopWindowReply,
    pub set_window_owner: SetWindowOwnerReply,
    pub get_window_info: GetWindowInfoReply,
    pub set_window_info: SetWindowInfoReply,
    pub set_parent: SetParentReply,
    pub get_window_parents: GetWindowParentsReply,
    pub get_window_children: GetWindowChildrenReply,
    pub get_window_children_from_point: GetWindowChildrenFromPointReply,
    pub get_window_tree: GetWindowTreeReply,
    pub set_window_pos: SetWindowPosReply,
    pub get_window_rectangles: GetWindowRectanglesReply,
    pub get_window_text: GetWindowTextReply,
    pub set_window_text: SetWindowTextReply,
    pub get_windows_offset: GetWindowsOffsetReply,
    pub get_visible_region: GetVisibleRegionReply,
    pub get_surface_region: GetSurfaceRegionReply,
    pub get_window_region: GetWindowRegionReply,
    pub set_window_region: SetWindowRegionReply,
    pub get_update_region: GetUpdateRegionReply,
    pub update_window_zorder: UpdateWindowZorderReply,
    pub redraw_window: RedrawWindowReply,
    pub set_window_property: SetWindowPropertyReply,
    pub remove_window_property: RemoveWindowPropertyReply,
    pub get_window_property: GetWindowPropertyReply,
    pub get_window_properties: GetWindowPropertiesReply,
    pub create_winstation: CreateWinstationReply,
    pub open_winstation: OpenWinstationReply,
    pub close_winstation: CloseWinstationReply,
    pub get_process_winstation: GetProcessWinstationReply,
    pub set_process_winstation: SetProcessWinstationReply,
    pub enum_winstation: EnumWinstationReply,
    pub create_desktop: CreateDesktopReply,
    pub open_desktop: OpenDesktopReply,
    pub open_input_desktop: OpenInputDesktopReply,
    pub close_desktop: CloseDesktopReply,
    pub get_thread_desktop: GetThreadDesktopReply,
    pub set_thread_desktop: SetThreadDesktopReply,
    pub enum_desktop: EnumDesktopReply,
    pub set_user_object_info: SetUserObjectInfoReply,
    pub register_hotkey: RegisterHotkeyReply,
    pub unregister_hotkey: UnregisterHotkeyReply,
    pub attach_thread_input: AttachThreadInputReply,
    pub get_thread_input: GetThreadInputReply,
    pub get_last_input_time: GetLastInputTimeReply,
    pub get_key_state: GetKeyStateReply,
    pub set_key_state: SetKeyStateReply,
    pub set_foreground_window: SetForegroundWindowReply,
    pub set_focus_window: SetFocusWindowReply,
    pub set_active_window: SetActiveWindowReply,
    pub set_capture_window: SetCaptureWindowReply,
    pub set_caret_window: SetCaretWindowReply,
    pub set_caret_info: SetCaretInfoReply,
    pub set_hook: SetHookReply,
    pub remove_hook: RemoveHookReply,
    pub start_hook_chain: StartHookChainReply,
    pub finish_hook_chain: FinishHookChainReply,
    pub get_hook_info: GetHookInfoReply,
    pub create_class: CreateClassReply,
    pub destroy_class: DestroyClassReply,
    pub set_class_info: SetClassInfoReply,
    pub open_clipboard: OpenClipboardReply,
    pub close_clipboard: CloseClipboardReply,
    pub empty_clipboard: EmptyClipboardReply,
    pub set_clipboard_data: SetClipboardDataReply,
    pub get_clipboard_data: GetClipboardDataReply,
    pub get_clipboard_formats: GetClipboardFormatsReply,
    pub enum_clipboard_formats: EnumClipboardFormatsReply,
    pub release_clipboard: ReleaseClipboardReply,
    pub get_clipboard_info: GetClipboardInfoReply,
    pub set_clipboard_viewer: SetClipboardViewerReply,
    pub add_clipboard_listener: AddClipboardListenerReply,
    pub remove_clipboard_listener: RemoveClipboardListenerReply,
    pub create_token: CreateTokenReply,
    pub open_token: OpenTokenReply,
    pub set_global_windows: SetGlobalWindowsReply,
    pub adjust_token_privileges: AdjustTokenPrivilegesReply,
    pub get_token_privileges: GetTokenPrivilegesReply,
    pub check_token_privileges: CheckTokenPrivilegesReply,
    pub duplicate_token: DuplicateTokenReply,
    pub filter_token: FilterTokenReply,
    pub access_check: AccessCheckReply,
    pub get_token_sid: GetTokenSidReply,
    pub get_token_groups: GetTokenGroupsReply,
    pub get_token_default_dacl: GetTokenDefaultDaclReply,
    pub set_token_default_dacl: SetTokenDefaultDaclReply,
    pub set_security_object: SetSecurityObjectReply,
    pub get_security_object: GetSecurityObjectReply,
    pub get_system_handles: GetSystemHandlesReply,
    pub create_mailslot: CreateMailslotReply,
    pub set_mailslot_info: SetMailslotInfoReply,
    pub create_directory: CreateDirectoryReply,
    pub open_directory: OpenDirectoryReply,
    pub get_directory_entry: GetDirectoryEntryReply,
    pub create_symlink: CreateSymlinkReply,
    pub open_symlink: OpenSymlinkReply,
    pub query_symlink: QuerySymlinkReply,
    pub get_object_info: GetObjectInfoReply,
    pub get_object_name: GetObjectNameReply,
    pub get_object_type: GetObjectTypeReply,
    pub get_object_types: GetObjectTypesReply,
    pub allocate_locally_unique_id: AllocateLocallyUniqueIdReply,
    pub create_device_manager: CreateDeviceManagerReply,
    pub create_device: CreateDeviceReply,
    pub delete_device: DeleteDeviceReply,
    pub get_next_device_request: GetNextDeviceRequestReply,
    pub get_kernel_object_ptr: GetKernelObjectPtrReply,
    pub set_kernel_object_ptr: SetKernelObjectPtrReply,
    pub grab_kernel_object: GrabKernelObjectReply,
    pub release_kernel_object: ReleaseKernelObjectReply,
    pub get_kernel_object_handle: GetKernelObjectHandleReply,
    pub make_process_system: MakeProcessSystemReply,
    pub get_token_info: GetTokenInfoReply,
    pub create_linked_token: CreateLinkedTokenReply,
    pub create_completion: CreateCompletionReply,
    pub open_completion: OpenCompletionReply,
    pub add_completion: AddCompletionReply,
    pub remove_completion: RemoveCompletionReply,
    pub query_completion: QueryCompletionReply,
    pub set_completion_info: SetCompletionInfoReply,
    pub add_fd_completion: AddFdCompletionReply,
    pub set_fd_completion_mode: SetFdCompletionModeReply,
    pub set_fd_disp_info: SetFdDispInfoReply,
    pub set_fd_name_info: SetFdNameInfoReply,
    pub set_fd_eof_info: SetFdEofInfoReply,
    pub get_window_layered_info: GetWindowLayeredInfoReply,
    pub set_window_layered_info: SetWindowLayeredInfoReply,
    pub alloc_user_handle: AllocUserHandleReply,
    pub free_user_handle: FreeUserHandleReply,
    pub set_cursor: SetCursorReply,
    pub get_cursor_history: GetCursorHistoryReply,
    pub get_rawinput_buffer: GetRawinputBufferReply,
    pub update_rawinput_devices: UpdateRawinputDevicesReply,
    pub create_job: CreateJobReply,
    pub open_job: OpenJobReply,
    pub assign_job: AssignJobReply,
    pub process_in_job: ProcessInJobReply,
    pub set_job_limits: SetJobLimitsReply,
    pub set_job_completion_port: SetJobCompletionPortReply,
    pub get_job_info: GetJobInfoReply,
    pub terminate_job: TerminateJobReply,
    pub suspend_process: SuspendProcessReply,
    pub resume_process: ResumeProcessReply,
    pub get_next_thread: GetNextThreadReply,
}
pub const SERVER_PROTOCOL_VERSION: u32 = 793;
