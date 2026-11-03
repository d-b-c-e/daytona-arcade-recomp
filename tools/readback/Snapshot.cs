using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

// Qualified native whole-object rename contract, shared-delete readers required.
// Private supervisor snapshots only; caller owns and has closed the complete temporary file.
public static class DaytonaSnapshot {
    [DllImport("user32", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder name,int size);
    [DllImport("user32")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
    [DllImport("user32")] static extern bool IsWindowVisible(IntPtr h);
    public static bool GameWindow(IntPtr h,int pid) {
        uint actual;GetWindowThreadProcessId(h,out actual);
        var name=new StringBuilder(256);GetClassName(h,name,name.Capacity);
        return actual==(uint)pid && IsWindowVisible(h) && name.ToString()=="SDL_app";
    }
    [StructLayout(LayoutKind.Sequential)]
    struct Rename { public uint Flags; public IntPtr RootDirectory; public uint FileNameLength; public ushort FileName; }
    [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern SafeFileHandle CreateFileW(string path,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
    [DllImport("kernel32", SetLastError=true)]
    static extern bool SetFileInformationByHandle(SafeFileHandle file,int kind,IntPtr data,uint bytes);
    public static void Publish(string temporary,string destination) {
        byte[] name=Encoding.Unicode.GetBytes(System.IO.Path.GetFullPath(destination));
        int size=Marshal.SizeOf(typeof(Rename))+name.Length;
        IntPtr data=Marshal.AllocHGlobal(size);
        try {
            for(int i=0;i<size;i++)Marshal.WriteByte(data,i,0);
            Marshal.WriteInt32(data,3); // FILE_RENAME_FLAG_REPLACE_IF_EXISTS | POSIX_SEMANTICS
            Marshal.WriteInt32(data,(int)Marshal.OffsetOf(typeof(Rename),"FileNameLength"),name.Length);
            Marshal.Copy(name,0,IntPtr.Add(data,(int)Marshal.OffsetOf(typeof(Rename),"FileName")),name.Length);
            using(var file=CreateFileW(temporary,0x00110000,7,IntPtr.Zero,3,0x00200000,IntPtr.Zero)) {
                if(file.IsInvalid)throw new Win32Exception(Marshal.GetLastWin32Error(),"Snapshot rename handle refused");
                if(!SetFileInformationByHandle(file,22,data,(uint)size))
                    throw new Win32Exception(Marshal.GetLastWin32Error(),"Snapshot FileRenameInfoEx refused");
            }
        } finally { Marshal.FreeHGlobal(data); }
    }
}
