using System;
using Example.Cpp.Managed.Library;

namespace Example.Cpp.Managed.Executable
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine(ManagedClass.ManagedFunction("P1", 2));
        }
    }
}
