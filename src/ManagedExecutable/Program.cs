using System;
using Example.Cpp.Managed.Library;

namespace Example.Cpp.Managed.Executable
{
    class Program
    {
        static void Main(string[] args)
        {
            ManagedLibrary managedLibrary = new ManagedLibrary();
            int number = managedLibrary.GetNumber(10);
            Console.WriteLine($"Managed Library - GetNumber(10) = {number}");
        }
    }
}
