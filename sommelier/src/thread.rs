use std::fs;
use std::io;
use std::mem;
use std::slice;
use std::sync::{Arc, Mutex};
use std::thread;

use std::collections::HashMap;
use std::io::Read;
use std::io::Write;
use std::os::fd::FromRawFd;
use std::os::unix::net::UnixStream;

use crate::fd;
use crate::ipc;
use crate::mapping::*;
use crate::object::*;

struct Tid(u32);

struct TidPool {
    next: u32,
    used: Vec<u32>,
}

impl TidPool {
    fn alloc(&mut self) -> Tid {
        let (mut pos, idx) = match self.used.binary_search(&self.next) {
            Err(pos) => (pos, self.next),
            _ => panic!(),
        };

        self.used.insert(pos, idx);
        while self.used.get(pos) == Some(&self.next) {
            self.next += 1;
            pos += 1;
        }

        Tid(idx * 4)
    }

    fn free(&mut self, tid: &Tid) {
        match self.used.binary_search(&(tid.0 / 4)) {
            Ok(pos) => self.used.remove(pos),
            _ => panic!(),
        };
    }
}

static TID_POOL: Mutex<TidPool> = Mutex::new(TidPool {
    next: 8,
    used: Vec::new(),
});

struct Thread {
    root: Arc<Mutex<RootDirectory>>,
    process: Arc<Mutex<Process>>,
    request_file: fs::File,
    reply_file: fs::File,
    wait_file: fs::File,
    tid: Tid,
}

impl Drop for Thread {
    fn drop(&mut self) {
        TID_POOL.lock().unwrap().free(&self.tid);
    }
}

const IMAGE_FILE_MACHINE_AMD64: u16 = 0x8664;
const IMAGE_FILE_MACHINE_I386: u16 = 0x014c;

impl Thread {
    fn run(&mut self) {
        loop {
            let (req, data) = self.request().expect("Failed to read request");
            match self.dispatch(req, data) {
                Ok((reply, data)) => self.reply(reply, data).expect("Failed to write reply"),
                Err(err) => self.error(err).expect("Failed to write error"),
            }
        }
    }

    fn request(&mut self) -> io::Result<(ipc::GenericRequest, Vec<u8>)> {
        let mut buffer = [0; mem::size_of::<ipc::GenericRequest>()];
        self.request_file.read_exact(&mut buffer)?;

        let req: ipc::GenericRequest = unsafe { mem::transmute(buffer) };
        let data_size = unsafe { req.header.request_size } as usize;
        if data_size == 0 {
            return Ok((req, Vec::new()));
        }

        let mut data = Vec::with_capacity(data_size);
        let slice = unsafe { slice::from_raw_parts_mut(data.as_mut_ptr(), data_size) };
        self.request_file.read_exact(slice)?;
        unsafe { data.set_len(data_size) };
        Ok((req, data))
    }

    fn error(&mut self, err: u32) -> io::Result<()> {
        let reply = ipc::GenericReply {
            header: ipc::ReplyHeader {
                error: err,
                reply_size: 0,
            },
        };
        let reply_buf: [u8; mem::size_of::<ipc::GenericReply>()] = unsafe { mem::transmute(reply) };
        self.reply_file.write_all(&reply_buf)
    }

    fn reply(&mut self, mut reply: ipc::GenericReply, data: Vec<u8>) -> io::Result<()> {
        reply.header.reply_size = data.len() as u32;
        let reply_buf: [u8; mem::size_of::<ipc::GenericReply>()] = unsafe { mem::transmute(reply) };

        if data.len() == 0 {
            self.reply_file.write_all(&reply_buf)
        } else {
            self.reply_file.write_all(&reply_buf)?;
            self.reply_file.write_all(&data)
        }
    }

    fn dispatch(
        &mut self,
        req: ipc::GenericRequest,
        data: Vec<u8>,
    ) -> Result<(ipc::GenericReply, Vec<u8>), u32> {
        macro_rules! req {
            ($self:ident, $req: ident, $data:ident, $x:ident) => {{
                let req = unsafe { &$req.$x };
                match $self.$x(req, data) {
                    Ok((reply, data)) => Ok((ipc::GenericReply { $x: reply }, data)),
                    Err(err) => Err(err),
                }
            }};
        }

        use ipc::RequestType::*;
        match unsafe { req.header.req } {
            NewProcess => req!(self, req, data, new_process),
            GetNewProcessInfo => req!(self, req, data, get_new_process_info),
            NewThread => req!(self, req, data, new_thread),
            GetStartupInfo => req!(self, req, data, get_startup_info),
            InitProcessDone => req!(self, req, data, init_process_done),
            InitFirstThread => req!(self, req, data, init_first_thread),
            InitThread => req!(self, req, data, init_thread),
            TerminateProcess => req!(self, req, data, terminate_process),
            TerminateThread => req!(self, req, data, terminate_thread),
            GetProcessInfo => req!(self, req, data, get_process_info),
            GetProcessDebugInfo => req!(self, req, data, get_process_debug_info),
            GetProcessImageName => req!(self, req, data, get_process_image_name),
            GetProcessVmCounters => req!(self, req, data, get_process_vm_counters),
            SetProcessInfo => req!(self, req, data, set_process_info),
            GetThreadInfo => req!(self, req, data, get_thread_info),
            GetThreadTimes => req!(self, req, data, get_thread_times),
            SetThreadInfo => req!(self, req, data, set_thread_info),
            SuspendThread => req!(self, req, data, suspend_thread),
            ResumeThread => req!(self, req, data, resume_thread),
            QueueApc => req!(self, req, data, queue_apc),
            GetApcResult => req!(self, req, data, get_apc_result),
            CloseHandle => req!(self, req, data, close_handle),
            SetHandleInfo => req!(self, req, data, set_handle_info),
            DupHandle => req!(self, req, data, dup_handle),
            CompareObjects => req!(self, req, data, compare_objects),
            MakeTemporary => req!(self, req, data, make_temporary),
            OpenProcess => req!(self, req, data, open_process),
            OpenThread => req!(self, req, data, open_thread),
            Select => req!(self, req, data, select),
            CreateEvent => req!(self, req, data, create_event),
            EventOp => req!(self, req, data, event_op),
            QueryEvent => req!(self, req, data, query_event),
            OpenEvent => req!(self, req, data, open_event),
            CreateKeyedEvent => req!(self, req, data, create_keyed_event),
            OpenKeyedEvent => req!(self, req, data, open_keyed_event),
            CreateMutex => req!(self, req, data, create_mutex),
            ReleaseMutex => req!(self, req, data, release_mutex),
            OpenMutex => req!(self, req, data, open_mutex),
            QueryMutex => req!(self, req, data, query_mutex),
            CreateSemaphore => req!(self, req, data, create_semaphore),
            ReleaseSemaphore => req!(self, req, data, release_semaphore),
            QuerySemaphore => req!(self, req, data, query_semaphore),
            OpenSemaphore => req!(self, req, data, open_semaphore),
            CreateFile => req!(self, req, data, create_file),
            OpenFileObject => req!(self, req, data, open_file_object),
            AllocFileHandle => req!(self, req, data, alloc_file_handle),
            GetHandleUnixName => req!(self, req, data, get_handle_unix_name),
            GetHandleFd => req!(self, req, data, get_handle_fd),
            GetDirectoryCacheEntry => req!(self, req, data, get_directory_cache_entry),
            Flush => req!(self, req, data, flush),
            GetFileInfo => req!(self, req, data, get_file_info),
            GetVolumeInfo => req!(self, req, data, get_volume_info),
            LockFile => req!(self, req, data, lock_file),
            UnlockFile => req!(self, req, data, unlock_file),
            RecvSocket => req!(self, req, data, recv_socket),
            SendSocket => req!(self, req, data, send_socket),
            SocketGetEvents => req!(self, req, data, socket_get_events),
            SocketSendIcmpId => req!(self, req, data, socket_send_icmp_id),
            SocketGetIcmpId => req!(self, req, data, socket_get_icmp_id),
            GetNextConsoleRequest => req!(self, req, data, get_next_console_request),
            ReadDirectoryChanges => req!(self, req, data, read_directory_changes),
            ReadChange => req!(self, req, data, read_change),
            CreateMapping => req!(self, req, data, create_mapping),
            OpenMapping => req!(self, req, data, open_mapping),
            GetMappingInfo => req!(self, req, data, get_mapping_info),
            GetImageMapAddress => req!(self, req, data, get_image_map_address),
            MapView => req!(self, req, data, map_view),
            MapImageView => req!(self, req, data, map_image_view),
            MapBuiltinView => req!(self, req, data, map_builtin_view),
            GetImageViewInfo => req!(self, req, data, get_image_view_info),
            UnmapView => req!(self, req, data, unmap_view),
            GetMappingCommittedRange => req!(self, req, data, get_mapping_committed_range),
            AddMappingCommittedRange => req!(self, req, data, add_mapping_committed_range),
            IsSameMapping => req!(self, req, data, is_same_mapping),
            GetMappingFilename => req!(self, req, data, get_mapping_filename),
            ListProcesses => req!(self, req, data, list_processes),
            CreateDebugObj => req!(self, req, data, create_debug_obj),
            WaitDebugEvent => req!(self, req, data, wait_debug_event),
            QueueExceptionEvent => req!(self, req, data, queue_exception_event),
            GetExceptionStatus => req!(self, req, data, get_exception_status),
            ContinueDebugEvent => req!(self, req, data, continue_debug_event),
            DebugProcess => req!(self, req, data, debug_process),
            SetDebugObjInfo => req!(self, req, data, set_debug_obj_info),
            ReadProcessMemory => req!(self, req, data, read_process_memory),
            WriteProcessMemory => req!(self, req, data, write_process_memory),
            CreateKey => req!(self, req, data, create_key),
            OpenKey => req!(self, req, data, open_key),
            DeleteKey => req!(self, req, data, delete_key),
            FlushKey => req!(self, req, data, flush_key),
            EnumKey => req!(self, req, data, enum_key),
            SetKeyValue => req!(self, req, data, set_key_value),
            GetKeyValue => req!(self, req, data, get_key_value),
            EnumKeyValue => req!(self, req, data, enum_key_value),
            DeleteKeyValue => req!(self, req, data, delete_key_value),
            LoadRegistry => req!(self, req, data, load_registry),
            UnloadRegistry => req!(self, req, data, unload_registry),
            SaveRegistry => req!(self, req, data, save_registry),
            SetRegistryNotification => req!(self, req, data, set_registry_notification),
            RenameKey => req!(self, req, data, rename_key),
            CreateTimer => req!(self, req, data, create_timer),
            OpenTimer => req!(self, req, data, open_timer),
            SetTimer => req!(self, req, data, set_timer),
            CancelTimer => req!(self, req, data, cancel_timer),
            GetTimerInfo => req!(self, req, data, get_timer_info),
            GetThreadContext => req!(self, req, data, get_thread_context),
            SetThreadContext => req!(self, req, data, set_thread_context),
            GetSelectorEntry => req!(self, req, data, get_selector_entry),
            AddAtom => req!(self, req, data, add_atom),
            DeleteAtom => req!(self, req, data, delete_atom),
            FindAtom => req!(self, req, data, find_atom),
            GetAtomInformation => req!(self, req, data, get_atom_information),
            GetMsgQueue => req!(self, req, data, get_msg_queue),
            SetQueueFd => req!(self, req, data, set_queue_fd),
            SetQueueMask => req!(self, req, data, set_queue_mask),
            GetQueueStatus => req!(self, req, data, get_queue_status),
            GetProcessIdleEvent => req!(self, req, data, get_process_idle_event),
            SendMessage => req!(self, req, data, send_message),
            PostQuitMessage => req!(self, req, data, post_quit_message),
            SendHardwareMessage => req!(self, req, data, send_hardware_message),
            GetMessage => req!(self, req, data, get_message),
            ReplyMessage => req!(self, req, data, reply_message),
            AcceptHardwareMessage => req!(self, req, data, accept_hardware_message),
            GetMessageReply => req!(self, req, data, get_message_reply),
            SetWinTimer => req!(self, req, data, set_win_timer),
            KillWinTimer => req!(self, req, data, kill_win_timer),
            IsWindowHung => req!(self, req, data, is_window_hung),
            GetSerialInfo => req!(self, req, data, get_serial_info),
            SetSerialInfo => req!(self, req, data, set_serial_info),
            CancelSync => req!(self, req, data, cancel_sync),
            RegisterAsync => req!(self, req, data, register_async),
            CancelAsync => req!(self, req, data, cancel_async),
            GetAsyncResult => req!(self, req, data, get_async_result),
            SetAsyncDirectResult => req!(self, req, data, set_async_direct_result),
            Read => req!(self, req, data, read),
            Write => req!(self, req, data, write),
            Ioctl => req!(self, req, data, ioctl),
            SetIrpResult => req!(self, req, data, set_irp_result),
            CreateNamedPipe => req!(self, req, data, create_named_pipe),
            SetNamedPipeInfo => req!(self, req, data, set_named_pipe_info),
            CreateWindow => req!(self, req, data, create_window),
            DestroyWindow => req!(self, req, data, destroy_window),
            GetDesktopWindow => req!(self, req, data, get_desktop_window),
            SetWindowOwner => req!(self, req, data, set_window_owner),
            GetWindowInfo => req!(self, req, data, get_window_info),
            SetWindowInfo => req!(self, req, data, set_window_info),
            SetParent => req!(self, req, data, set_parent),
            GetWindowParents => req!(self, req, data, get_window_parents),
            GetWindowChildren => req!(self, req, data, get_window_children),
            GetWindowChildrenFromPoint => req!(self, req, data, get_window_children_from_point),
            GetWindowTree => req!(self, req, data, get_window_tree),
            SetWindowPos => req!(self, req, data, set_window_pos),
            GetWindowRectangles => req!(self, req, data, get_window_rectangles),
            GetWindowText => req!(self, req, data, get_window_text),
            SetWindowText => req!(self, req, data, set_window_text),
            GetWindowsOffset => req!(self, req, data, get_windows_offset),
            GetVisibleRegion => req!(self, req, data, get_visible_region),
            GetSurfaceRegion => req!(self, req, data, get_surface_region),
            GetWindowRegion => req!(self, req, data, get_window_region),
            SetWindowRegion => req!(self, req, data, set_window_region),
            GetUpdateRegion => req!(self, req, data, get_update_region),
            UpdateWindowZorder => req!(self, req, data, update_window_zorder),
            RedrawWindow => req!(self, req, data, redraw_window),
            SetWindowProperty => req!(self, req, data, set_window_property),
            RemoveWindowProperty => req!(self, req, data, remove_window_property),
            GetWindowProperty => req!(self, req, data, get_window_property),
            GetWindowProperties => req!(self, req, data, get_window_properties),
            CreateWinstation => req!(self, req, data, create_winstation),
            OpenWinstation => req!(self, req, data, open_winstation),
            CloseWinstation => req!(self, req, data, close_winstation),
            GetProcessWinstation => req!(self, req, data, get_process_winstation),
            SetProcessWinstation => req!(self, req, data, set_process_winstation),
            EnumWinstation => req!(self, req, data, enum_winstation),
            CreateDesktop => req!(self, req, data, create_desktop),
            OpenDesktop => req!(self, req, data, open_desktop),
            OpenInputDesktop => req!(self, req, data, open_input_desktop),
            CloseDesktop => req!(self, req, data, close_desktop),
            GetThreadDesktop => req!(self, req, data, get_thread_desktop),
            SetThreadDesktop => req!(self, req, data, set_thread_desktop),
            EnumDesktop => req!(self, req, data, enum_desktop),
            SetUserObjectInfo => req!(self, req, data, set_user_object_info),
            RegisterHotkey => req!(self, req, data, register_hotkey),
            UnregisterHotkey => req!(self, req, data, unregister_hotkey),
            AttachThreadInput => req!(self, req, data, attach_thread_input),
            GetThreadInput => req!(self, req, data, get_thread_input),
            GetLastInputTime => req!(self, req, data, get_last_input_time),
            GetKeyState => req!(self, req, data, get_key_state),
            SetKeyState => req!(self, req, data, set_key_state),
            SetForegroundWindow => req!(self, req, data, set_foreground_window),
            SetFocusWindow => req!(self, req, data, set_focus_window),
            SetActiveWindow => req!(self, req, data, set_active_window),
            SetCaptureWindow => req!(self, req, data, set_capture_window),
            SetCaretWindow => req!(self, req, data, set_caret_window),
            SetCaretInfo => req!(self, req, data, set_caret_info),
            SetHook => req!(self, req, data, set_hook),
            RemoveHook => req!(self, req, data, remove_hook),
            StartHookChain => req!(self, req, data, start_hook_chain),
            FinishHookChain => req!(self, req, data, finish_hook_chain),
            GetHookInfo => req!(self, req, data, get_hook_info),
            CreateClass => req!(self, req, data, create_class),
            DestroyClass => req!(self, req, data, destroy_class),
            SetClassInfo => req!(self, req, data, set_class_info),
            OpenClipboard => req!(self, req, data, open_clipboard),
            CloseClipboard => req!(self, req, data, close_clipboard),
            EmptyClipboard => req!(self, req, data, empty_clipboard),
            SetClipboardData => req!(self, req, data, set_clipboard_data),
            GetClipboardData => req!(self, req, data, get_clipboard_data),
            GetClipboardFormats => req!(self, req, data, get_clipboard_formats),
            EnumClipboardFormats => req!(self, req, data, enum_clipboard_formats),
            ReleaseClipboard => req!(self, req, data, release_clipboard),
            GetClipboardInfo => req!(self, req, data, get_clipboard_info),
            SetClipboardViewer => req!(self, req, data, set_clipboard_viewer),
            AddClipboardListener => req!(self, req, data, add_clipboard_listener),
            RemoveClipboardListener => req!(self, req, data, remove_clipboard_listener),
            CreateToken => req!(self, req, data, create_token),
            OpenToken => req!(self, req, data, open_token),
            SetGlobalWindows => req!(self, req, data, set_global_windows),
            AdjustTokenPrivileges => req!(self, req, data, adjust_token_privileges),
            GetTokenPrivileges => req!(self, req, data, get_token_privileges),
            CheckTokenPrivileges => req!(self, req, data, check_token_privileges),
            DuplicateToken => req!(self, req, data, duplicate_token),
            FilterToken => req!(self, req, data, filter_token),
            AccessCheck => req!(self, req, data, access_check),
            GetTokenSid => req!(self, req, data, get_token_sid),
            GetTokenGroups => req!(self, req, data, get_token_groups),
            GetTokenDefaultDacl => req!(self, req, data, get_token_default_dacl),
            SetTokenDefaultDacl => req!(self, req, data, set_token_default_dacl),
            SetSecurityObject => req!(self, req, data, set_security_object),
            GetSecurityObject => req!(self, req, data, get_security_object),
            GetSystemHandles => req!(self, req, data, get_system_handles),
            CreateMailslot => req!(self, req, data, create_mailslot),
            SetMailslotInfo => req!(self, req, data, set_mailslot_info),
            CreateDirectory => req!(self, req, data, create_directory),
            OpenDirectory => req!(self, req, data, open_directory),
            GetDirectoryEntry => req!(self, req, data, get_directory_entry),
            CreateSymlink => req!(self, req, data, create_symlink),
            OpenSymlink => req!(self, req, data, open_symlink),
            QuerySymlink => req!(self, req, data, query_symlink),
            GetObjectInfo => req!(self, req, data, get_object_info),
            GetObjectName => req!(self, req, data, get_object_name),
            GetObjectType => req!(self, req, data, get_object_type),
            GetObjectTypes => req!(self, req, data, get_object_types),
            AllocateLocallyUniqueId => req!(self, req, data, allocate_locally_unique_id),
            CreateDeviceManager => req!(self, req, data, create_device_manager),
            CreateDevice => req!(self, req, data, create_device),
            DeleteDevice => req!(self, req, data, delete_device),
            GetNextDeviceRequest => req!(self, req, data, get_next_device_request),
            GetKernelObjectPtr => req!(self, req, data, get_kernel_object_ptr),
            SetKernelObjectPtr => req!(self, req, data, set_kernel_object_ptr),
            GrabKernelObject => req!(self, req, data, grab_kernel_object),
            ReleaseKernelObject => req!(self, req, data, release_kernel_object),
            GetKernelObjectHandle => req!(self, req, data, get_kernel_object_handle),
            MakeProcessSystem => req!(self, req, data, make_process_system),
            GetTokenInfo => req!(self, req, data, get_token_info),
            CreateLinkedToken => req!(self, req, data, create_linked_token),
            CreateCompletion => req!(self, req, data, create_completion),
            OpenCompletion => req!(self, req, data, open_completion),
            AddCompletion => req!(self, req, data, add_completion),
            RemoveCompletion => req!(self, req, data, remove_completion),
            QueryCompletion => req!(self, req, data, query_completion),
            SetCompletionInfo => req!(self, req, data, set_completion_info),
            AddFdCompletion => req!(self, req, data, add_fd_completion),
            SetFdCompletionMode => req!(self, req, data, set_fd_completion_mode),
            SetFdDispInfo => req!(self, req, data, set_fd_disp_info),
            SetFdNameInfo => req!(self, req, data, set_fd_name_info),
            SetFdEofInfo => req!(self, req, data, set_fd_eof_info),
            GetWindowLayeredInfo => req!(self, req, data, get_window_layered_info),
            SetWindowLayeredInfo => req!(self, req, data, set_window_layered_info),
            AllocUserHandle => req!(self, req, data, alloc_user_handle),
            FreeUserHandle => req!(self, req, data, free_user_handle),
            SetCursor => req!(self, req, data, set_cursor),
            GetCursorHistory => req!(self, req, data, get_cursor_history),
            GetRawinputBuffer => req!(self, req, data, get_rawinput_buffer),
            UpdateRawinputDevices => req!(self, req, data, update_rawinput_devices),
            CreateJob => req!(self, req, data, create_job),
            OpenJob => req!(self, req, data, open_job),
            AssignJob => req!(self, req, data, assign_job),
            ProcessInJob => req!(self, req, data, process_in_job),
            SetJobLimits => req!(self, req, data, set_job_limits),
            SetJobCompletionPort => req!(self, req, data, set_job_completion_port),
            GetJobInfo => req!(self, req, data, get_job_info),
            TerminateJob => req!(self, req, data, terminate_job),
            SuspendProcess => req!(self, req, data, suspend_process),
            ResumeProcess => req!(self, req, data, resume_process),
            GetNextThread => req!(self, req, data, get_next_thread),
        }
    }

    fn new_process(
        &mut self,
        req: &ipc::NewProcessRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::NewProcessReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_new_process_info(
        &mut self,
        req: &ipc::GetNewProcessInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetNewProcessInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn new_thread(
        &mut self,
        req: &ipc::NewThreadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::NewThreadReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_startup_info(
        &mut self,
        req: &ipc::GetStartupInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetStartupInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn init_process_done(
        &mut self,
        req: &ipc::InitProcessDoneRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::InitProcessDoneReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn init_first_thread(
        &mut self,
        req: &ipc::InitFirstThreadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::InitFirstThreadReply, Vec<u8>), u32> {
        println!("{:?}", req);

        let (client, server) = self.receive_fd();
        if client == req.reply_fd {
            self.reply_file = unsafe { fs::File::from_raw_fd(server) };
        }
        if client == req.wait_fd {
            self.wait_file = unsafe { fs::File::from_raw_fd(server) };
        }

        let (client, server) = self.receive_fd();
        if client == req.reply_fd {
            self.reply_file = unsafe { fs::File::from_raw_fd(server) };
        }
        if client == req.wait_fd {
            self.wait_file = unsafe { fs::File::from_raw_fd(server) };
        }

        let supported_machines = [IMAGE_FILE_MACHINE_AMD64, IMAGE_FILE_MACHINE_I386];
        let mut data = Vec::new();
        data.extend_from_slice(unsafe {
            mem::transmute::<&[u16; 2], &[u8; 4]>(&supported_machines)
        });

        Ok((
            ipc::InitFirstThreadReply {
                header: ipc::ReplyHeader::default(),
                pid: self.process.lock().unwrap().pid.0 as ipc::ProcessId,
                tid: self.tid.0 as ipc::ThreadId,
                server_start: 0 as ipc::Timeout,
                session_id: 0,
                info_size: 0,
            },
            data,
        ))
    }

    fn init_thread(
        &mut self,
        req: &ipc::InitThreadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::InitThreadReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn terminate_process(
        &mut self,
        req: &ipc::TerminateProcessRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::TerminateProcessReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn terminate_thread(
        &mut self,
        req: &ipc::TerminateThreadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::TerminateThreadReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_process_info(
        &mut self,
        req: &ipc::GetProcessInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetProcessInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_process_debug_info(
        &mut self,
        req: &ipc::GetProcessDebugInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetProcessDebugInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_process_image_name(
        &mut self,
        req: &ipc::GetProcessImageNameRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetProcessImageNameReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_process_vm_counters(
        &mut self,
        req: &ipc::GetProcessVmCountersRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetProcessVmCountersReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_process_info(
        &mut self,
        req: &ipc::SetProcessInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetProcessInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_thread_info(
        &mut self,
        req: &ipc::GetThreadInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetThreadInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_thread_times(
        &mut self,
        req: &ipc::GetThreadTimesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetThreadTimesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_thread_info(
        &mut self,
        req: &ipc::SetThreadInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetThreadInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn suspend_thread(
        &mut self,
        req: &ipc::SuspendThreadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SuspendThreadReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn resume_thread(
        &mut self,
        req: &ipc::ResumeThreadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ResumeThreadReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn queue_apc(
        &mut self,
        req: &ipc::QueueApcRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::QueueApcReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_apc_result(
        &mut self,
        req: &ipc::GetApcResultRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetApcResultReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn close_handle(
        &mut self,
        req: &ipc::CloseHandleRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CloseHandleReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_handle_info(
        &mut self,
        req: &ipc::SetHandleInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetHandleInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn dup_handle(
        &mut self,
        req: &ipc::DupHandleRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DupHandleReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn compare_objects(
        &mut self,
        req: &ipc::CompareObjectsRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CompareObjectsReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn make_temporary(
        &mut self,
        req: &ipc::MakeTemporaryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::MakeTemporaryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_process(
        &mut self,
        req: &ipc::OpenProcessRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenProcessReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_thread(
        &mut self,
        req: &ipc::OpenThreadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenThreadReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn select(
        &mut self,
        req: &ipc::SelectRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SelectReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_event(
        &mut self,
        req: &ipc::CreateEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn event_op(
        &mut self,
        req: &ipc::EventOpRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::EventOpReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn query_event(
        &mut self,
        req: &ipc::QueryEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::QueryEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_event(
        &mut self,
        req: &ipc::OpenEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_keyed_event(
        &mut self,
        req: &ipc::CreateKeyedEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateKeyedEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_keyed_event(
        &mut self,
        req: &ipc::OpenKeyedEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenKeyedEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_mutex(
        &mut self,
        req: &ipc::CreateMutexRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateMutexReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn release_mutex(
        &mut self,
        req: &ipc::ReleaseMutexRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReleaseMutexReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_mutex(
        &mut self,
        req: &ipc::OpenMutexRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenMutexReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn query_mutex(
        &mut self,
        req: &ipc::QueryMutexRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::QueryMutexReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_semaphore(
        &mut self,
        req: &ipc::CreateSemaphoreRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateSemaphoreReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn release_semaphore(
        &mut self,
        req: &ipc::ReleaseSemaphoreRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReleaseSemaphoreReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn query_semaphore(
        &mut self,
        req: &ipc::QuerySemaphoreRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::QuerySemaphoreReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_semaphore(
        &mut self,
        req: &ipc::OpenSemaphoreRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenSemaphoreReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_file(
        &mut self,
        req: &ipc::CreateFileRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateFileReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_file_object(
        &mut self,
        req: &ipc::OpenFileObjectRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenFileObjectReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn alloc_file_handle(
        &mut self,
        req: &ipc::AllocFileHandleRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AllocFileHandleReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_handle_unix_name(
        &mut self,
        req: &ipc::GetHandleUnixNameRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetHandleUnixNameReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_handle_fd(
        &mut self,
        req: &ipc::GetHandleFdRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetHandleFdReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_directory_cache_entry(
        &mut self,
        req: &ipc::GetDirectoryCacheEntryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetDirectoryCacheEntryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn flush(
        &mut self,
        req: &ipc::FlushRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::FlushReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_file_info(
        &mut self,
        req: &ipc::GetFileInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetFileInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_volume_info(
        &mut self,
        req: &ipc::GetVolumeInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetVolumeInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn lock_file(
        &mut self,
        req: &ipc::LockFileRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::LockFileReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn unlock_file(
        &mut self,
        req: &ipc::UnlockFileRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::UnlockFileReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn recv_socket(
        &mut self,
        req: &ipc::RecvSocketRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RecvSocketReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn send_socket(
        &mut self,
        req: &ipc::SendSocketRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SendSocketReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn socket_get_events(
        &mut self,
        req: &ipc::SocketGetEventsRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SocketGetEventsReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn socket_send_icmp_id(
        &mut self,
        req: &ipc::SocketSendIcmpIdRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SocketSendIcmpIdReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn socket_get_icmp_id(
        &mut self,
        req: &ipc::SocketGetIcmpIdRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SocketGetIcmpIdReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_next_console_request(
        &mut self,
        req: &ipc::GetNextConsoleRequestRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetNextConsoleRequestReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn read_directory_changes(
        &mut self,
        req: &ipc::ReadDirectoryChangesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReadDirectoryChangesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn read_change(
        &mut self,
        req: &ipc::ReadChangeRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReadChangeReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_mapping(
        &mut self,
        req: &ipc::CreateMappingRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateMappingReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_mapping(
        &mut self,
        req: &ipc::OpenMappingRequest,
        data: Vec<u8>,
    ) -> Result<(ipc::OpenMappingReply, Vec<u8>), u32> {
        let utf16 = unsafe { slice::from_raw_parts(data.as_ptr() as *const u16, data.len() / 2) };
        let path = String::from_utf16(&utf16).unwrap();
        println!("{:?}, {:?}", req, path);

        let mut reply = ipc::OpenMappingReply::default();

        use crate::mapping::*;
        if let Some(object) = self.root.lock().unwrap().lookup(&path) {
            use std::ops::DerefMut;
            let mut process = self.process.lock().unwrap();
            let mapping = Mapping::from_object(object.clone());
            reply.handle = object.open(process.deref_mut()).0;
        }

        Ok((reply, Vec::new()))
    }

    fn get_mapping_info(
        &mut self,
        req: &ipc::GetMappingInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetMappingInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_image_map_address(
        &mut self,
        req: &ipc::GetImageMapAddressRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetImageMapAddressReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn map_view(
        &mut self,
        req: &ipc::MapViewRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::MapViewReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn map_image_view(
        &mut self,
        req: &ipc::MapImageViewRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::MapImageViewReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn map_builtin_view(
        &mut self,
        req: &ipc::MapBuiltinViewRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::MapBuiltinViewReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_image_view_info(
        &mut self,
        req: &ipc::GetImageViewInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetImageViewInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn unmap_view(
        &mut self,
        req: &ipc::UnmapViewRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::UnmapViewReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_mapping_committed_range(
        &mut self,
        req: &ipc::GetMappingCommittedRangeRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetMappingCommittedRangeReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn add_mapping_committed_range(
        &mut self,
        req: &ipc::AddMappingCommittedRangeRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AddMappingCommittedRangeReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn is_same_mapping(
        &mut self,
        req: &ipc::IsSameMappingRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::IsSameMappingReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_mapping_filename(
        &mut self,
        req: &ipc::GetMappingFilenameRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetMappingFilenameReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn list_processes(
        &mut self,
        req: &ipc::ListProcessesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ListProcessesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_debug_obj(
        &mut self,
        req: &ipc::CreateDebugObjRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateDebugObjReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn wait_debug_event(
        &mut self,
        req: &ipc::WaitDebugEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::WaitDebugEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn queue_exception_event(
        &mut self,
        req: &ipc::QueueExceptionEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::QueueExceptionEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_exception_status(
        &mut self,
        req: &ipc::GetExceptionStatusRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetExceptionStatusReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn continue_debug_event(
        &mut self,
        req: &ipc::ContinueDebugEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ContinueDebugEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn debug_process(
        &mut self,
        req: &ipc::DebugProcessRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DebugProcessReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_debug_obj_info(
        &mut self,
        req: &ipc::SetDebugObjInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetDebugObjInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn read_process_memory(
        &mut self,
        req: &ipc::ReadProcessMemoryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReadProcessMemoryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn write_process_memory(
        &mut self,
        req: &ipc::WriteProcessMemoryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::WriteProcessMemoryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_key(
        &mut self,
        req: &ipc::CreateKeyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateKeyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_key(
        &mut self,
        req: &ipc::OpenKeyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenKeyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn delete_key(
        &mut self,
        req: &ipc::DeleteKeyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DeleteKeyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn flush_key(
        &mut self,
        req: &ipc::FlushKeyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::FlushKeyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn enum_key(
        &mut self,
        req: &ipc::EnumKeyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::EnumKeyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_key_value(
        &mut self,
        req: &ipc::SetKeyValueRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetKeyValueReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_key_value(
        &mut self,
        req: &ipc::GetKeyValueRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetKeyValueReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn enum_key_value(
        &mut self,
        req: &ipc::EnumKeyValueRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::EnumKeyValueReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn delete_key_value(
        &mut self,
        req: &ipc::DeleteKeyValueRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DeleteKeyValueReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn load_registry(
        &mut self,
        req: &ipc::LoadRegistryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::LoadRegistryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn unload_registry(
        &mut self,
        req: &ipc::UnloadRegistryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::UnloadRegistryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn save_registry(
        &mut self,
        req: &ipc::SaveRegistryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SaveRegistryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_registry_notification(
        &mut self,
        req: &ipc::SetRegistryNotificationRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetRegistryNotificationReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn rename_key(
        &mut self,
        req: &ipc::RenameKeyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RenameKeyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_timer(
        &mut self,
        req: &ipc::CreateTimerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateTimerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_timer(
        &mut self,
        req: &ipc::OpenTimerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenTimerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_timer(
        &mut self,
        req: &ipc::SetTimerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetTimerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn cancel_timer(
        &mut self,
        req: &ipc::CancelTimerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CancelTimerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_timer_info(
        &mut self,
        req: &ipc::GetTimerInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetTimerInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_thread_context(
        &mut self,
        req: &ipc::GetThreadContextRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetThreadContextReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_thread_context(
        &mut self,
        req: &ipc::SetThreadContextRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetThreadContextReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_selector_entry(
        &mut self,
        req: &ipc::GetSelectorEntryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetSelectorEntryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn add_atom(
        &mut self,
        req: &ipc::AddAtomRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AddAtomReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn delete_atom(
        &mut self,
        req: &ipc::DeleteAtomRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DeleteAtomReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn find_atom(
        &mut self,
        req: &ipc::FindAtomRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::FindAtomReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_atom_information(
        &mut self,
        req: &ipc::GetAtomInformationRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetAtomInformationReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_msg_queue(
        &mut self,
        req: &ipc::GetMsgQueueRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetMsgQueueReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_queue_fd(
        &mut self,
        req: &ipc::SetQueueFdRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetQueueFdReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_queue_mask(
        &mut self,
        req: &ipc::SetQueueMaskRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetQueueMaskReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_queue_status(
        &mut self,
        req: &ipc::GetQueueStatusRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetQueueStatusReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_process_idle_event(
        &mut self,
        req: &ipc::GetProcessIdleEventRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetProcessIdleEventReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn send_message(
        &mut self,
        req: &ipc::SendMessageRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SendMessageReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn post_quit_message(
        &mut self,
        req: &ipc::PostQuitMessageRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::PostQuitMessageReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn send_hardware_message(
        &mut self,
        req: &ipc::SendHardwareMessageRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SendHardwareMessageReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_message(
        &mut self,
        req: &ipc::GetMessageRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetMessageReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn reply_message(
        &mut self,
        req: &ipc::ReplyMessageRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReplyMessageReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn accept_hardware_message(
        &mut self,
        req: &ipc::AcceptHardwareMessageRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AcceptHardwareMessageReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_message_reply(
        &mut self,
        req: &ipc::GetMessageReplyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetMessageReplyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_win_timer(
        &mut self,
        req: &ipc::SetWinTimerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetWinTimerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn kill_win_timer(
        &mut self,
        req: &ipc::KillWinTimerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::KillWinTimerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn is_window_hung(
        &mut self,
        req: &ipc::IsWindowHungRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::IsWindowHungReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_serial_info(
        &mut self,
        req: &ipc::GetSerialInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetSerialInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_serial_info(
        &mut self,
        req: &ipc::SetSerialInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetSerialInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn cancel_sync(
        &mut self,
        req: &ipc::CancelSyncRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CancelSyncReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn register_async(
        &mut self,
        req: &ipc::RegisterAsyncRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RegisterAsyncReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn cancel_async(
        &mut self,
        req: &ipc::CancelAsyncRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CancelAsyncReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_async_result(
        &mut self,
        req: &ipc::GetAsyncResultRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetAsyncResultReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_async_direct_result(
        &mut self,
        req: &ipc::SetAsyncDirectResultRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetAsyncDirectResultReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn read(
        &mut self,
        req: &ipc::ReadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReadReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn write(
        &mut self,
        req: &ipc::WriteRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::WriteReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn ioctl(
        &mut self,
        req: &ipc::IoctlRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::IoctlReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_irp_result(
        &mut self,
        req: &ipc::SetIrpResultRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetIrpResultReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_named_pipe(
        &mut self,
        req: &ipc::CreateNamedPipeRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateNamedPipeReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_named_pipe_info(
        &mut self,
        req: &ipc::SetNamedPipeInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetNamedPipeInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_window(
        &mut self,
        req: &ipc::CreateWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn destroy_window(
        &mut self,
        req: &ipc::DestroyWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DestroyWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_desktop_window(
        &mut self,
        req: &ipc::GetDesktopWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetDesktopWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_window_owner(
        &mut self,
        req: &ipc::SetWindowOwnerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetWindowOwnerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_info(
        &mut self,
        req: &ipc::GetWindowInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_window_info(
        &mut self,
        req: &ipc::SetWindowInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetWindowInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_parent(
        &mut self,
        req: &ipc::SetParentRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetParentReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_parents(
        &mut self,
        req: &ipc::GetWindowParentsRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowParentsReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_children(
        &mut self,
        req: &ipc::GetWindowChildrenRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowChildrenReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_children_from_point(
        &mut self,
        req: &ipc::GetWindowChildrenFromPointRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowChildrenFromPointReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_tree(
        &mut self,
        req: &ipc::GetWindowTreeRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowTreeReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_window_pos(
        &mut self,
        req: &ipc::SetWindowPosRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetWindowPosReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_rectangles(
        &mut self,
        req: &ipc::GetWindowRectanglesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowRectanglesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_text(
        &mut self,
        req: &ipc::GetWindowTextRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowTextReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_window_text(
        &mut self,
        req: &ipc::SetWindowTextRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetWindowTextReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_windows_offset(
        &mut self,
        req: &ipc::GetWindowsOffsetRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowsOffsetReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_visible_region(
        &mut self,
        req: &ipc::GetVisibleRegionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetVisibleRegionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_surface_region(
        &mut self,
        req: &ipc::GetSurfaceRegionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetSurfaceRegionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_region(
        &mut self,
        req: &ipc::GetWindowRegionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowRegionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_window_region(
        &mut self,
        req: &ipc::SetWindowRegionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetWindowRegionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_update_region(
        &mut self,
        req: &ipc::GetUpdateRegionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetUpdateRegionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn update_window_zorder(
        &mut self,
        req: &ipc::UpdateWindowZorderRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::UpdateWindowZorderReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn redraw_window(
        &mut self,
        req: &ipc::RedrawWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RedrawWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_window_property(
        &mut self,
        req: &ipc::SetWindowPropertyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetWindowPropertyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn remove_window_property(
        &mut self,
        req: &ipc::RemoveWindowPropertyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RemoveWindowPropertyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_property(
        &mut self,
        req: &ipc::GetWindowPropertyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowPropertyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_properties(
        &mut self,
        req: &ipc::GetWindowPropertiesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowPropertiesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_winstation(
        &mut self,
        req: &ipc::CreateWinstationRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateWinstationReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_winstation(
        &mut self,
        req: &ipc::OpenWinstationRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenWinstationReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn close_winstation(
        &mut self,
        req: &ipc::CloseWinstationRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CloseWinstationReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_process_winstation(
        &mut self,
        req: &ipc::GetProcessWinstationRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetProcessWinstationReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_process_winstation(
        &mut self,
        req: &ipc::SetProcessWinstationRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetProcessWinstationReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn enum_winstation(
        &mut self,
        req: &ipc::EnumWinstationRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::EnumWinstationReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_desktop(
        &mut self,
        req: &ipc::CreateDesktopRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateDesktopReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_desktop(
        &mut self,
        req: &ipc::OpenDesktopRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenDesktopReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_input_desktop(
        &mut self,
        req: &ipc::OpenInputDesktopRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenInputDesktopReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn close_desktop(
        &mut self,
        req: &ipc::CloseDesktopRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CloseDesktopReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_thread_desktop(
        &mut self,
        req: &ipc::GetThreadDesktopRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetThreadDesktopReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_thread_desktop(
        &mut self,
        req: &ipc::SetThreadDesktopRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetThreadDesktopReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn enum_desktop(
        &mut self,
        req: &ipc::EnumDesktopRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::EnumDesktopReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_user_object_info(
        &mut self,
        req: &ipc::SetUserObjectInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetUserObjectInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn register_hotkey(
        &mut self,
        req: &ipc::RegisterHotkeyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RegisterHotkeyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn unregister_hotkey(
        &mut self,
        req: &ipc::UnregisterHotkeyRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::UnregisterHotkeyReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn attach_thread_input(
        &mut self,
        req: &ipc::AttachThreadInputRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AttachThreadInputReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_thread_input(
        &mut self,
        req: &ipc::GetThreadInputRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetThreadInputReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_last_input_time(
        &mut self,
        req: &ipc::GetLastInputTimeRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetLastInputTimeReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_key_state(
        &mut self,
        req: &ipc::GetKeyStateRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetKeyStateReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_key_state(
        &mut self,
        req: &ipc::SetKeyStateRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetKeyStateReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_foreground_window(
        &mut self,
        req: &ipc::SetForegroundWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetForegroundWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_focus_window(
        &mut self,
        req: &ipc::SetFocusWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetFocusWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_active_window(
        &mut self,
        req: &ipc::SetActiveWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetActiveWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_capture_window(
        &mut self,
        req: &ipc::SetCaptureWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetCaptureWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_caret_window(
        &mut self,
        req: &ipc::SetCaretWindowRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetCaretWindowReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_caret_info(
        &mut self,
        req: &ipc::SetCaretInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetCaretInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_hook(
        &mut self,
        req: &ipc::SetHookRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetHookReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn remove_hook(
        &mut self,
        req: &ipc::RemoveHookRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RemoveHookReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn start_hook_chain(
        &mut self,
        req: &ipc::StartHookChainRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::StartHookChainReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn finish_hook_chain(
        &mut self,
        req: &ipc::FinishHookChainRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::FinishHookChainReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_hook_info(
        &mut self,
        req: &ipc::GetHookInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetHookInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_class(
        &mut self,
        req: &ipc::CreateClassRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateClassReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn destroy_class(
        &mut self,
        req: &ipc::DestroyClassRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DestroyClassReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_class_info(
        &mut self,
        req: &ipc::SetClassInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetClassInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_clipboard(
        &mut self,
        req: &ipc::OpenClipboardRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenClipboardReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn close_clipboard(
        &mut self,
        req: &ipc::CloseClipboardRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CloseClipboardReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn empty_clipboard(
        &mut self,
        req: &ipc::EmptyClipboardRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::EmptyClipboardReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_clipboard_data(
        &mut self,
        req: &ipc::SetClipboardDataRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetClipboardDataReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_clipboard_data(
        &mut self,
        req: &ipc::GetClipboardDataRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetClipboardDataReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_clipboard_formats(
        &mut self,
        req: &ipc::GetClipboardFormatsRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetClipboardFormatsReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn enum_clipboard_formats(
        &mut self,
        req: &ipc::EnumClipboardFormatsRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::EnumClipboardFormatsReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn release_clipboard(
        &mut self,
        req: &ipc::ReleaseClipboardRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReleaseClipboardReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_clipboard_info(
        &mut self,
        req: &ipc::GetClipboardInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetClipboardInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_clipboard_viewer(
        &mut self,
        req: &ipc::SetClipboardViewerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetClipboardViewerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn add_clipboard_listener(
        &mut self,
        req: &ipc::AddClipboardListenerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AddClipboardListenerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn remove_clipboard_listener(
        &mut self,
        req: &ipc::RemoveClipboardListenerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RemoveClipboardListenerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_token(
        &mut self,
        req: &ipc::CreateTokenRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateTokenReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_token(
        &mut self,
        req: &ipc::OpenTokenRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenTokenReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_global_windows(
        &mut self,
        req: &ipc::SetGlobalWindowsRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetGlobalWindowsReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn adjust_token_privileges(
        &mut self,
        req: &ipc::AdjustTokenPrivilegesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AdjustTokenPrivilegesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_token_privileges(
        &mut self,
        req: &ipc::GetTokenPrivilegesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetTokenPrivilegesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn check_token_privileges(
        &mut self,
        req: &ipc::CheckTokenPrivilegesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CheckTokenPrivilegesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn duplicate_token(
        &mut self,
        req: &ipc::DuplicateTokenRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DuplicateTokenReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn filter_token(
        &mut self,
        req: &ipc::FilterTokenRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::FilterTokenReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn access_check(
        &mut self,
        req: &ipc::AccessCheckRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AccessCheckReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_token_sid(
        &mut self,
        req: &ipc::GetTokenSidRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetTokenSidReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_token_groups(
        &mut self,
        req: &ipc::GetTokenGroupsRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetTokenGroupsReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_token_default_dacl(
        &mut self,
        req: &ipc::GetTokenDefaultDaclRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetTokenDefaultDaclReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_token_default_dacl(
        &mut self,
        req: &ipc::SetTokenDefaultDaclRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetTokenDefaultDaclReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_security_object(
        &mut self,
        req: &ipc::SetSecurityObjectRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetSecurityObjectReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_security_object(
        &mut self,
        req: &ipc::GetSecurityObjectRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetSecurityObjectReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_system_handles(
        &mut self,
        req: &ipc::GetSystemHandlesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetSystemHandlesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_mailslot(
        &mut self,
        req: &ipc::CreateMailslotRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateMailslotReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_mailslot_info(
        &mut self,
        req: &ipc::SetMailslotInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetMailslotInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_directory(
        &mut self,
        req: &ipc::CreateDirectoryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateDirectoryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_directory(
        &mut self,
        req: &ipc::OpenDirectoryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenDirectoryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_directory_entry(
        &mut self,
        req: &ipc::GetDirectoryEntryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetDirectoryEntryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_symlink(
        &mut self,
        req: &ipc::CreateSymlinkRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateSymlinkReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_symlink(
        &mut self,
        req: &ipc::OpenSymlinkRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenSymlinkReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn query_symlink(
        &mut self,
        req: &ipc::QuerySymlinkRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::QuerySymlinkReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_object_info(
        &mut self,
        req: &ipc::GetObjectInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetObjectInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_object_name(
        &mut self,
        req: &ipc::GetObjectNameRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetObjectNameReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_object_type(
        &mut self,
        req: &ipc::GetObjectTypeRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetObjectTypeReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_object_types(
        &mut self,
        req: &ipc::GetObjectTypesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetObjectTypesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn allocate_locally_unique_id(
        &mut self,
        req: &ipc::AllocateLocallyUniqueIdRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AllocateLocallyUniqueIdReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_device_manager(
        &mut self,
        req: &ipc::CreateDeviceManagerRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateDeviceManagerReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_device(
        &mut self,
        req: &ipc::CreateDeviceRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateDeviceReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn delete_device(
        &mut self,
        req: &ipc::DeleteDeviceRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::DeleteDeviceReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_next_device_request(
        &mut self,
        req: &ipc::GetNextDeviceRequestRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetNextDeviceRequestReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_kernel_object_ptr(
        &mut self,
        req: &ipc::GetKernelObjectPtrRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetKernelObjectPtrReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_kernel_object_ptr(
        &mut self,
        req: &ipc::SetKernelObjectPtrRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetKernelObjectPtrReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn grab_kernel_object(
        &mut self,
        req: &ipc::GrabKernelObjectRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GrabKernelObjectReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn release_kernel_object(
        &mut self,
        req: &ipc::ReleaseKernelObjectRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ReleaseKernelObjectReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_kernel_object_handle(
        &mut self,
        req: &ipc::GetKernelObjectHandleRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetKernelObjectHandleReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn make_process_system(
        &mut self,
        req: &ipc::MakeProcessSystemRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::MakeProcessSystemReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_token_info(
        &mut self,
        req: &ipc::GetTokenInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetTokenInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_linked_token(
        &mut self,
        req: &ipc::CreateLinkedTokenRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateLinkedTokenReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_completion(
        &mut self,
        req: &ipc::CreateCompletionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateCompletionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_completion(
        &mut self,
        req: &ipc::OpenCompletionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenCompletionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn add_completion(
        &mut self,
        req: &ipc::AddCompletionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AddCompletionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn remove_completion(
        &mut self,
        req: &ipc::RemoveCompletionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::RemoveCompletionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn query_completion(
        &mut self,
        req: &ipc::QueryCompletionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::QueryCompletionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_completion_info(
        &mut self,
        req: &ipc::SetCompletionInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetCompletionInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn add_fd_completion(
        &mut self,
        req: &ipc::AddFdCompletionRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AddFdCompletionReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_fd_completion_mode(
        &mut self,
        req: &ipc::SetFdCompletionModeRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetFdCompletionModeReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_fd_disp_info(
        &mut self,
        req: &ipc::SetFdDispInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetFdDispInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_fd_name_info(
        &mut self,
        req: &ipc::SetFdNameInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetFdNameInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_fd_eof_info(
        &mut self,
        req: &ipc::SetFdEofInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetFdEofInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_window_layered_info(
        &mut self,
        req: &ipc::GetWindowLayeredInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetWindowLayeredInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_window_layered_info(
        &mut self,
        req: &ipc::SetWindowLayeredInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetWindowLayeredInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn alloc_user_handle(
        &mut self,
        req: &ipc::AllocUserHandleRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AllocUserHandleReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn free_user_handle(
        &mut self,
        req: &ipc::FreeUserHandleRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::FreeUserHandleReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_cursor(
        &mut self,
        req: &ipc::SetCursorRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetCursorReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_cursor_history(
        &mut self,
        req: &ipc::GetCursorHistoryRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetCursorHistoryReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_rawinput_buffer(
        &mut self,
        req: &ipc::GetRawinputBufferRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetRawinputBufferReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn update_rawinput_devices(
        &mut self,
        req: &ipc::UpdateRawinputDevicesRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::UpdateRawinputDevicesReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn create_job(
        &mut self,
        req: &ipc::CreateJobRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::CreateJobReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn open_job(
        &mut self,
        req: &ipc::OpenJobRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::OpenJobReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn assign_job(
        &mut self,
        req: &ipc::AssignJobRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::AssignJobReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn process_in_job(
        &mut self,
        req: &ipc::ProcessInJobRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ProcessInJobReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_job_limits(
        &mut self,
        req: &ipc::SetJobLimitsRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetJobLimitsReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn set_job_completion_port(
        &mut self,
        req: &ipc::SetJobCompletionPortRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SetJobCompletionPortReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_job_info(
        &mut self,
        req: &ipc::GetJobInfoRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetJobInfoReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn terminate_job(
        &mut self,
        req: &ipc::TerminateJobRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::TerminateJobReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn suspend_process(
        &mut self,
        req: &ipc::SuspendProcessRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::SuspendProcessReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn resume_process(
        &mut self,
        req: &ipc::ResumeProcessRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::ResumeProcessReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn get_next_thread(
        &mut self,
        req: &ipc::GetNextThreadRequest,
        _data: Vec<u8>,
    ) -> Result<(ipc::GetNextThreadReply, Vec<u8>), u32> {
        println!("{:?}", req);
        Err(0xdeadbeef)
    }

    fn receive_fd(&mut self) -> (fd::ClientFd, fd::ServerFd) {
        let mut process = self.process.lock().unwrap();
        let (client, server) = process.receive_fd(self.tid.0);
        (client, server)
    }
}

pub struct Process {
    stream: UnixStream,
    pid: Tid,

    fds: HashMap<u32, Vec<(fd::ClientFd, fd::ServerFd)>>,
    pub handles: HandleTable,
}

impl Drop for Process {
    fn drop(&mut self) {
        TID_POOL.lock().unwrap().free(&self.pid);
    }
}

impl Process {
    pub fn spawn(root: Arc<Mutex<RootDirectory>>, stream: UnixStream) {
        thread::spawn(move || {
            let request_file = fd::send_process_pipe(&stream, ipc::SERVER_PROTOCOL_VERSION);

            let mut tids = TID_POOL.lock().unwrap();
            let (pid, tid) = (tids.alloc(), tids.alloc());
            mem::drop(tids);

            let process = Arc::new(Mutex::new(Process {
                stream,
                pid,

                fds: HashMap::new(),
                handles: HandleTable::new(),
            }));

            let mut thread = Thread {
                root,
                process,
                request_file,
                reply_file: fs::File::open("/dev/null").unwrap(),
                wait_file: fs::File::open("/dev/null").unwrap(),
                tid,
            };

            thread.run()
        });
    }

    fn receive_fd(&mut self, tid: u32) -> (fd::ClientFd, fd::ServerFd) {
        if !self.fds.contains_key(&tid) {
            self.fds.insert(tid, Vec::new());
        }

        loop {
            if let Some((client, server)) = self.fds.get_mut(&tid).unwrap().pop() {
                return (client, server);
            }

            let (dst, client, server) = fd::recv_client_fd(&self.stream);
            if dst == 0 {
                return (client, server);
            }

            self.fds.get_mut(&dst).unwrap().push((client, server));
        }
    }
}
