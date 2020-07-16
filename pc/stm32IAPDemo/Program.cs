using CommandLine;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;

namespace stm32IAPDemo
{
    class Program
    {
        static void Main(string[] args)
        {
            try
            {
                Parser.Default.ParseArguments<Options>(args)
                  .WithParsed(RunOptions)
                  .WithNotParsed(HandleParseError);
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
            }
            Console.WriteLine("press any key to exit...");
            Console.ReadKey();
        }
        static void RunOptions(Options opts)
        {
            var cmd = new Command(opts.comPort, 115200);
            if (opts.test)
            {
                Console.WriteLine("get info");
                Console.WriteLine(cmd.GetInfo());
                Console.WriteLine("set time to now");
                cmd.SetTimeToNow();
                Console.WriteLine("read time");
                for (int i = 0; i < 3; i++)
                {
                    Thread.Sleep(TimeSpan.FromSeconds(1));
                    Console.WriteLine(cmd.ReadTime());
                }
                Console.WriteLine("get temperature adc");
                Console.WriteLine(cmd.GetTemperatureADC());
            }
            if (opts.getInfo)
            {
                Console.WriteLine("get info");
                Console.WriteLine(cmd.GetInfo());
            }
            if (opts.settime)
            {
                Console.WriteLine("set time to now");
                cmd.SetTimeToNow();
            }
            if (opts.getTime)
            {
                Console.WriteLine("read time");
                Console.WriteLine(cmd.ReadTime());
            }
            if (!string.IsNullOrEmpty(opts.file) && File.Exists(opts.file))
            {
                Console.WriteLine("update firmware...");
                cmd.UpdateFirmware(opts.file);
                Console.WriteLine("update firmware done");
            }
        }
        static void HandleParseError(IEnumerable<Error> errs)
        {
            //handle errors
        }
    }
}
