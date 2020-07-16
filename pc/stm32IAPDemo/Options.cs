using CommandLine;
using System;

namespace stm32IAPDemo
{
    class Options
    {
        [Option('c', "com", Required = true, HelpText = "com port")]
        public string comPort { get; set; }

        [Option('i', "info", Required = false, HelpText = "get info")]
        public bool getInfo { get; set; }

        [Option('s', "set", Required = false, HelpText = "set time")]
        public bool settime { get; set; }

        [Option('g', "get", Required = false, HelpText = "get time")]
        public bool getTime { get; set; }

        [Option('u', "update", Required = false, HelpText = "update firmware")]
        public string file { get; set; }

        [Option('t', "test", Required = false, HelpText = "test")]
        public bool test { get; set; }
    }
}
