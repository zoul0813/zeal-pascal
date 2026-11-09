"""Canonical public ZOS constants/types and their ordinary ZPU1 metadata."""
import struct
CALLS='READ WRITE OPEN CLOSE DSTAT STAT SEEK IOCTL MKDIR CHDIR CURDIR OPENDIR READDIR RM MOUNT EXIT EXEC DUP MSLEEP SETTIME GETTIME SETDATE GETDATE MAP SWAP PALLOC PFREE'.split()
ERRORS='SUCCESS FAILURE NOT_IMPLEMENTED NOT_SUPPORTED NO_SUCH_ENTRY INVALID_SYSCALL INVALID_PARAMETER INVALID_VIRT_PAGE INVALID_PHYS_ADDRESS INVALID_OFFSET INVALID_NAME INVALID_PATH INVALID_FILESYSTEM INVALID_FILEDEV PATH_TOO_LONG ALREADY_EXIST ALREADY_OPENED ALREADY_MOUNTED READ_ONLY BAD_MODE CANNOT_REGISTER_MORE NO_MORE_ENTRIES NO_MORE_MEMORY NOT_A_DIR NOT_A_FILE ENTRY_CORRUPTED DIR_NOT_EMPTY'.split()
CONSTANTS={**{'ZOS_'+name:i for i,name in enumerate(CALLS)},**{'ZOS_ERR_'+name:i for i,name in enumerate(ERRORS)},
 'ZOS_DEV_STDOUT':0,'ZOS_DEV_STDIN':1,'ZOS_O_RDONLY':0,'ZOS_O_WRONLY':1,'ZOS_O_RDWR':2,'ZOS_O_TRUNC':4,'ZOS_O_APPEND':8,'ZOS_O_CREAT':16,'ZOS_O_NONBLOCK':32,
 'ZOS_SEEK_SET':0,'ZOS_SEEK_CUR':1,'ZOS_SEEK_END':2,'ZOS_FS_RAWTABLE':0,'ZOS_FS_ZEALFS':1,'ZOS_EXEC_OVERRIDE_PROGRAM':0,'ZOS_EXEC_PRESERVE_PROGRAM':1,'ZOS_PATH_MAX':128,'ZOS_FILENAME_LEN_MAX':16,'ZOS_DATE_SIZE':8,'ZOS_STAT_SIZE':29,'ZOS_DIR_ENTRY_SIZE':17,'ZOS_PAGE_SIZE':16384,'ZOS_IOCTL_BUFFER_MAX':128}
# IDs6,7 unsigned words;8 array index;9 fixed NUL-terminated name.
RECORDS={
 'TZOSRegisters':[('A',4),('H',4),('BC',6),('DE',6)],
 'TZOSDate':[(n,4)for n in ('YearHigh','YearLow','Month','Day','WeekDay','Hours','Minutes','Seconds')],
 'TZOSDirectoryEntry':[('Flags',4),('Name',9)],
 'TZOSStat':[('Flags',4),('Size',7),('Date',11),('Name',9)]}
WIDTH={0:2,3:1,4:1,6:2,7:4,8:1,9:16,10:6,11:8,12:17,13:29}
def export(name,role,type,offset=0):
 name=name.lower().encode();return struct.pack('<4B5H',len(name),role,0,0,type,0,0,offset,0)+name

def metadata():
 types=bytearray();exports=bytearray()
 for family,width,lo,hi,capacity,element in ((0x8005,2,0,65535,0,0),(0x8006,4,0,65535,0,0),(0,1,0,15,0,0),(0x8000,16,0,0,8,3)):
  types+=struct.pack('<9H',family,width,lo,hi,capacity,element,0,0,0)
 for i,(name,fields)in enumerate(RECORDS.items(),10):
  types+=struct.pack('<9H',0x8001,WIDTH[i],65535,0,0,0,0,0,len(fields));offset=0
  for field,t in fields:
   b=field.lower().encode();types+=struct.pack('<BB4H',16,len(b),t,offset,65535,0)+b;offset+=WIDTH[t]
  assert offset==WIDTH[i];exports+=export(name,3,i)
 for name,value in CONSTANTS.items():exports+=export(name,2,0,value)
 return exports,types,8

def source():
 text='{ Native Zeal OS ABI. Generated from tools/zos_interface.py. }\nunit ZOS;\ninterface\nconst\n'
 text+=''.join(f'  {name} = {value};\n'for name,value in CONSTANTS.items())
 text+='type\n'
 names={4:'Byte',6:'Word',7:'LongWord',9:'array[0..15] of Char',11:'TZOSDate'}
 for name,fields in RECORDS.items():
  text+=f'  {name} = record\n'+''.join(f'    {field}: {names[t]};\n'for field,t in fields)+'  end;\n'
 return text+'implementation\n{ ZOS, ZOSBuffer and ZOSRegisters are compiler built-ins. }\nend.\n'
