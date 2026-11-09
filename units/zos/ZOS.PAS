{ Native Zeal OS ABI. Generated from tools/zos_interface.py. }
unit ZOS;
interface
const
  ZOS_READ = 0;
  ZOS_WRITE = 1;
  ZOS_OPEN = 2;
  ZOS_CLOSE = 3;
  ZOS_DSTAT = 4;
  ZOS_STAT = 5;
  ZOS_SEEK = 6;
  ZOS_IOCTL = 7;
  ZOS_MKDIR = 8;
  ZOS_CHDIR = 9;
  ZOS_CURDIR = 10;
  ZOS_OPENDIR = 11;
  ZOS_READDIR = 12;
  ZOS_RM = 13;
  ZOS_MOUNT = 14;
  ZOS_EXIT = 15;
  ZOS_EXEC = 16;
  ZOS_DUP = 17;
  ZOS_MSLEEP = 18;
  ZOS_SETTIME = 19;
  ZOS_GETTIME = 20;
  ZOS_SETDATE = 21;
  ZOS_GETDATE = 22;
  ZOS_MAP = 23;
  ZOS_SWAP = 24;
  ZOS_PALLOC = 25;
  ZOS_PFREE = 26;
  ZOS_ERR_SUCCESS = 0;
  ZOS_ERR_FAILURE = 1;
  ZOS_ERR_NOT_IMPLEMENTED = 2;
  ZOS_ERR_NOT_SUPPORTED = 3;
  ZOS_ERR_NO_SUCH_ENTRY = 4;
  ZOS_ERR_INVALID_SYSCALL = 5;
  ZOS_ERR_INVALID_PARAMETER = 6;
  ZOS_ERR_INVALID_VIRT_PAGE = 7;
  ZOS_ERR_INVALID_PHYS_ADDRESS = 8;
  ZOS_ERR_INVALID_OFFSET = 9;
  ZOS_ERR_INVALID_NAME = 10;
  ZOS_ERR_INVALID_PATH = 11;
  ZOS_ERR_INVALID_FILESYSTEM = 12;
  ZOS_ERR_INVALID_FILEDEV = 13;
  ZOS_ERR_PATH_TOO_LONG = 14;
  ZOS_ERR_ALREADY_EXIST = 15;
  ZOS_ERR_ALREADY_OPENED = 16;
  ZOS_ERR_ALREADY_MOUNTED = 17;
  ZOS_ERR_READ_ONLY = 18;
  ZOS_ERR_BAD_MODE = 19;
  ZOS_ERR_CANNOT_REGISTER_MORE = 20;
  ZOS_ERR_NO_MORE_ENTRIES = 21;
  ZOS_ERR_NO_MORE_MEMORY = 22;
  ZOS_ERR_NOT_A_DIR = 23;
  ZOS_ERR_NOT_A_FILE = 24;
  ZOS_ERR_ENTRY_CORRUPTED = 25;
  ZOS_ERR_DIR_NOT_EMPTY = 26;
  ZOS_DEV_STDOUT = 0;
  ZOS_DEV_STDIN = 1;
  ZOS_O_RDONLY = 0;
  ZOS_O_WRONLY = 1;
  ZOS_O_RDWR = 2;
  ZOS_O_TRUNC = 4;
  ZOS_O_APPEND = 8;
  ZOS_O_CREAT = 16;
  ZOS_O_NONBLOCK = 32;
  ZOS_SEEK_SET = 0;
  ZOS_SEEK_CUR = 1;
  ZOS_SEEK_END = 2;
  ZOS_FS_RAWTABLE = 0;
  ZOS_FS_ZEALFS = 1;
  ZOS_EXEC_OVERRIDE_PROGRAM = 0;
  ZOS_EXEC_PRESERVE_PROGRAM = 1;
  ZOS_PATH_MAX = 128;
  ZOS_FILENAME_LEN_MAX = 16;
  ZOS_DATE_SIZE = 8;
  ZOS_STAT_SIZE = 29;
  ZOS_DIR_ENTRY_SIZE = 17;
  ZOS_PAGE_SIZE = 16384;
  ZOS_IOCTL_BUFFER_MAX = 128;
type
  TZOSRegisters = record
    A: Byte;
    H: Byte;
    BC: Word;
    DE: Word;
  end;
  TZOSDate = record
    YearHigh: Byte;
    YearLow: Byte;
    Month: Byte;
    Day: Byte;
    WeekDay: Byte;
    Hours: Byte;
    Minutes: Byte;
    Seconds: Byte;
  end;
  TZOSDirectoryEntry = record
    Flags: Byte;
    Name: array[0..15] of Char;
  end;
  TZOSStat = record
    Flags: Byte;
    Size: LongWord;
    Date: TZOSDate;
    Name: array[0..15] of Char;
  end;
implementation
{ ZOS, ZOSBuffer and ZOSRegisters are compiler built-ins. }
end.
